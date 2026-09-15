/* Opcode implementations for the math family.
   The public opcode inventory remains centralized in csnum.c. */
#include "csnum_internal.h"
#include "csnregistry.h"
#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int32_t broadcast_shape(const CSN_ARRAY *a, const CSN_ARRAY *b,  uint32_t *out_shape, uint32_t *out_ndim) {
    uint32_t n = (a->ndim > b->ndim) ? a->ndim : b->ndim;
    if (n > CSN_MAX_DIMS) {
        return NOTOK;
    }

    for (uint32_t i = 0; i < n; i++) {
        uint32_t ea = (i < a->ndim) ? a->shape[a->ndim - 1 - i] : 1;
        uint32_t eb = (i < b->ndim) ? b->shape[b->ndim - 1 - i] : 1;

        if (ea != eb && ea != 1 && eb != 1) {
            return NOTOK;
        }

        out_shape[n - 1 - i] = (ea > eb) ? ea : eb;
    }

    *out_ndim = n;
    return OK;
}

/* Where a destination coordinate reads from inside one operand. An axis of
   extent 1 is stretched, so it always reads index 0; leading axes the operand
   lacks are skipped. */
static size_t broadcast_offset(const CSN_ARRAY *arr, const uint32_t *dst_coords, uint32_t out_ndim) {
    size_t off = 0;
    uint32_t lead = out_ndim - arr->ndim;

    for (uint32_t i = 0; i < arr->ndim; i++) {
        uint32_t c = (arr->shape[i] == 1) ? 0 : dst_coords[lead + i];
        off += (size_t) c * arr->strides[i];
    }

    return off;
}

static int32_t binop_hh_assign_value(CSOUND *csound, OPDS *perf_h, const CSN_ARRAY *source_arr_a, const CSN_ARRAY *source_arr_b, CSN_ARRAY *arr, ITEM_TYPE itype, CSN_BINOP_MODE mode) {
    bool same_shape = source_arr_a->ndim == source_arr_b->ndim
        && memcmp(source_arr_a->shape, source_arr_b->shape, sizeof(uint32_t) * source_arr_a->ndim) == 0;

    for (size_t i = 0; i < arr->size; i++) {
        size_t off_a = i;
        size_t off_b = i;

        if (!same_shape) {
            uint32_t dst_coords[CSN_MAX_DIMS] = {0};
            from_linear_to_coords(dst_coords, arr->shape, i, arr->ndim);
            off_a = broadcast_offset(source_arr_a, dst_coords, arr->ndim);
            off_b = broadcast_offset(source_arr_b, dst_coords, arr->ndim);
        }

        double a = source_arr_a->data[off_a];
        double b = source_arr_b->data[off_b];
        CSN_COMPLEXDAT ca = {0};
        CSN_COMPLEXDAT cb = {0};
        CSN_COMPLEXDAT c = { 0.0, 0.0 };
        if (itype == CSN_COMPLEX) {
            if (source_arr_a->itype == CSN_COMPLEX) {
                ca.re = source_arr_a->data[off_a * 2];
                ca.im = source_arr_a->data[off_a * 2 + 1];
            } else {
                ca.re = a;
                ca.im = 0.0;
            }
            if (source_arr_b->itype == CSN_COMPLEX) {
                cb.re = source_arr_b->data[off_b * 2];
                cb.im = source_arr_b->data[off_b * 2 + 1];
            } else {
                cb.re = b;
                cb.im = 0.0;
            }
        }

        switch (mode) {
            case CSN_ADD_HH:
                if (itype == CSN_REAL) {
                    arr->data[i] = a + b;
                } else {
                    complex_add(&c, ca, cb);
                    arr->data[i * 2] = c.re;
                    arr->data[i * 2 + 1] = c.im;
                }
                break;
            case CSN_SUB_HH:
                if (itype == CSN_REAL) {
                    arr->data[i] = a - b;
                } else {
                    complex_sub(&c, ca, cb);
                    arr->data[i * 2] = c.re;
                    arr->data[i * 2 + 1] = c.im;
                }
                break;
            case CSN_MUL_HH:
                if (itype == CSN_REAL) {
                    arr->data[i] = a * b;
                } else {
                    if (source_arr_b->itype == CSN_REAL) {
                        complex_scalar_prod(&c, ca, b);
                    } else if (source_arr_a->itype == CSN_REAL) {
                        complex_scalar_prod(&c, cb, a);
                    } else {
                        complex_prod(&c, ca, cb);
                    }
                    arr->data[i * 2] = c.re;
                    arr->data[i * 2 + 1] = c.im;
                }
                break;
            /* IEEE, as numpy: x/0 is an infinity and 0/0 is NaN. */
            case CSN_DIV_HH:
                if (itype == CSN_REAL) {
                    if (b == 0.0) {
                        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Division by zero");
                    }
                    arr->data[i] = a / b;
                } else {
                    if (complex_div(&c, ca, cb) != OK) {
                        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Division by zero");
                    };
                    arr->data[i * 2] = c.re;
                    arr->data[i * 2 + 1] = c.im;
                }
                break;
            case CSN_POW_HH:
                if (itype == CSN_REAL) {
                    arr->data[i] = pow(a, b);
                } else {
                    if (complex_pow(&c, ca, cb) != OK) {
                        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Division by zero");
                    };
                    arr->data[i * 2] = c.re;
                    arr->data[i * 2 + 1] = c.im;
                }
                break;
            case CSN_LOG_HH:
                /* Change of base elementwise: log of a in base b. */
                if (itype == CSN_REAL) {
                    arr->data[i] = log(a) / log(b);
                } else {
                    CSN_COMPLEXDAT log_a = {0};
                    CSN_COMPLEXDAT log_b = {0};
                    complex_log(&log_a, ca);
                    complex_log(&log_b, cb);
                    if (complex_div(&c, log_a, log_b) != OK) {
                        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Division by zero");
                    }
                    arr->data[i * 2] = c.re;
                    arr->data[i * 2 + 1] = c.im;
                }
                break;
            case CSN_LOGICAL_AND_HH:
                arr->data[i] = (double) ((a != 0.0) && (b != 0.0));
                break;
            case CSN_LOGICAL_OR_HH:
                arr->data[i] = (double) ((a != 0.0) || (b != 0.0));
                break;
            case CSN_HYPOT_HH:
                arr->data[i] = sqrt(a * a + b * b);
                break;
            case CSN_MINIMUM_HH:
                arr->data[i] = fmin(a, b);
                break;
            case CSN_MAXIMUM_HH:
                arr->data[i] = fmax(a, b);
                break;
            case CSN_ATAN2_HH:
                arr->data[i] = atan2(a, b);
                break;
            default:
                break;
        }
    }

    return OK;
}

static int32_t csnarray_binop_hh_helper(CSOUND *csound, CSN_BINOP_HH *p, CSN_BINOP_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle_a = p->source_handle_a->id;
    uint32_t source_handle_b = p->source_handle_b->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot_a = get_slot(reg, source_handle_a);
    if (source_slot_a == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle_a);
        goto done;
    }

    CSN_SLOT *source_slot_b = get_slot(reg, source_handle_b);
    if (source_slot_b == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle_b);
        goto done;
    }

    CSN_ARRAY *source_arr_a = source_slot_a->array;
    CSN_ARRAY *source_arr_b = source_slot_b->array;

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    uint32_t new_ndim = 0;
    if (broadcast_shape(source_arr_a, source_arr_b, new_shape, &new_ndim) != OK) {
        char abuf[CSN_SHAPE_STR_MAX], bbuf[CSN_SHAPE_STR_MAX];
        res = csound->InitError(csound, "[csnarray] Shapes %s and %s cannot be broadcast together: aligned from the last axis, each pair must match or be 1", shape_str(abuf, sizeof(abuf), source_arr_a->shape, source_arr_a->ndim), shape_str(bbuf, sizeof(bbuf), source_arr_b->shape, source_arr_b->ndim));
        goto done;
    }

    bool is_logic = (mode == CSN_LOGICAL_AND_HH || mode == CSN_LOGICAL_OR_HH);
    if (is_logic) {
        if (source_arr_a->itype == CSN_COMPLEX || source_arr_b->itype == CSN_COMPLEX) {
            res = csound->InitError(csound, "[csnarray] Logical and and or supports real array only");
            goto done;
        }
    }

    if ((source_arr_a->itype == CSN_COMPLEX || source_arr_b->itype == CSN_COMPLEX) && mode == CSN_HYPOT_HH) {
        res = csound->InitError(csound, "[csnarray] Hypot supports real array only");
        goto done;
    }

    if ((source_arr_a->itype == CSN_COMPLEX || source_arr_b->itype == CSN_COMPLEX) && (mode == CSN_MINIMUM_HH || mode == CSN_MAXIMUM_HH)) {
        res = csound->InitError(csound, "[csnarray] Minimum/Maximum supports real array only");
        goto done;
    }

    if ((source_arr_a->itype == CSN_COMPLEX || source_arr_b->itype == CSN_COMPLEX) && mode == CSN_ATAN2_HH) {
        res = csound->InitError(csound, "[csnarray] atan2 supports real array only");
        goto done;
    }

    bool type_mode = source_arr_a->itype == CSN_COMPLEX || source_arr_b->itype == CSN_COMPLEX;
    ITEM_TYPE itype = type_mode ? CSN_COMPLEX : CSN_REAL;

    const uint32_t protect[2] = { source_handle_a, source_handle_b };
    if (create_csnarray_locked(csound, reg, &p->h, new_ndim, new_shape, &p->array, p->handle, protect, 2U, &err, itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;
    res = binop_hh_assign_value(csound, NULL, source_arr_a, source_arr_b, arr, itype, mode);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_binop_hh_k_init_helper(CSOUND *csound, CSN_BINOP_HH *p, CSN_BINOP_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle_a = p->source_handle_a->id;
    uint32_t source_handle_b = p->source_handle_b->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot_a = get_slot(reg, source_handle_a);
    if (source_slot_a == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle_a);
        goto done;
    }

    CSN_SLOT *source_slot_b = get_slot(reg, source_handle_b);
    if (source_slot_b == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle_b);
        goto done;
    }

    CSN_ARRAY *source_arr_a = source_slot_a->array;

    /* Checked here as well as in the perf pass, which cannot be dropped: an
       operand's itype is dynamic at k-rate. Without this the init would publish
       a complex slot for an operation that rejects complex operands, and the
       first error a player sees would come from whatever reads that output
       rather than from here. */
    if (mode == CSN_LOGICAL_AND_HH || mode == CSN_LOGICAL_OR_HH) {
        if (source_arr_a->itype == CSN_COMPLEX || source_slot_b->array->itype == CSN_COMPLEX) {
            res = csound->InitError(csound, "[csnarray] Logical and and or supports real array only");
            goto done;
        }
    }

    if ((source_arr_a->itype == CSN_COMPLEX || source_slot_b->array->itype == CSN_COMPLEX) && mode == CSN_HYPOT_HH) {
        res = csound->InitError(csound, "[csnarray] Hypot supports real array only");
        goto done;
    }

    if ((source_arr_a->itype == CSN_COMPLEX || source_slot_b->array->itype == CSN_COMPLEX) && (mode == CSN_MINIMUM_HH || mode == CSN_MAXIMUM_HH)) {
        res = csound->InitError(csound, "[csnarray] Minimum/Maximum supports real array only");
        goto done;
    }

    if ((source_arr_a->itype == CSN_COMPLEX || source_slot_b->array->itype == CSN_COMPLEX) && mode == CSN_ATAN2_HH) {
        res = csound->InitError(csound, "[csnarray] atan2 supports real array only");
        goto done;
    }

    const uint32_t protect[2] = { source_handle_a, source_handle_b };
    if (create_csnarray_locked(csound, reg, &p->h, source_arr_a->ndim, source_arr_a->shape, &p->array, p->handle, protect, 2U, &err, source_arr_a->itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    if (source_arr_a->size > 0) {
        memcpy(p->array->data, source_arr_a->data, sizeof(double) * source_arr_a->size * source_arr_a->itype);
        p->array->size = source_arr_a->size;
    }

    SET_KDATA_BEGIN(p, reg);
    p->k_data.prev_size = source_arr_a->size;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_binop_hh_k_helper(CSOUND *csound, CSN_BINOP_HH *p, CSN_BINOP_MODE mode) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REG_HANDLE(csound, &p->h, reg, p->k_data.owned_handle);

    CHECK_KTRIG(p->trig);

    uint32_t source_handle_a = p->source_handle_a->id;
    uint32_t source_handle_b = p->source_handle_b->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot_a = get_slot(reg, source_handle_a);
    if (source_slot_a == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle_a);
    }

    CSN_SLOT *source_slot_b = get_slot(reg, source_handle_b);
    if (source_slot_b == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle_b);
    }

    CSN_ARRAY *source_arr_a = source_slot_a->array;
    CSN_ARRAY *source_arr_b = source_slot_b->array;

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    uint32_t new_ndim = 0;
    if (broadcast_shape(source_arr_a, source_arr_b, new_shape, &new_ndim) != OK) {
        char abuf[CSN_SHAPE_STR_MAX], bbuf[CSN_SHAPE_STR_MAX];
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Shapes %s and %s cannot be broadcast together: aligned from the last axis, each pair must match or be 1", shape_str(abuf, sizeof(abuf), source_arr_a->shape, source_arr_a->ndim), shape_str(bbuf, sizeof(bbuf), source_arr_b->shape, source_arr_b->ndim));
    }

    bool is_logic = (mode == CSN_LOGICAL_AND_HH || mode == CSN_LOGICAL_OR_HH);
    if (is_logic) {
        if (source_arr_a->itype == CSN_COMPLEX || source_arr_b->itype == CSN_COMPLEX) {
            csound->UnlockMutex(reg->mutex);
            return csound->PerfError(csound, &p->h, "[csnarray] Logical and and or supports real array only");
        }
    }

    if ((source_arr_a->itype == CSN_COMPLEX || source_arr_b->itype == CSN_COMPLEX) && mode == CSN_HYPOT_HH) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Hypot supports real array only");
    }

    if ((source_arr_a->itype == CSN_COMPLEX || source_arr_b->itype == CSN_COMPLEX) && (mode == CSN_MINIMUM_HH || mode == CSN_MAXIMUM_HH)) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Minimum/Maximum supports real array only");
    }

    if ((source_arr_a->itype == CSN_COMPLEX || source_arr_b->itype == CSN_COMPLEX) && mode == CSN_ATAN2_HH) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] atan2 supports real array only");
    }

    bool type_mode = source_arr_a->itype == CSN_COMPLEX || source_arr_b->itype == CSN_COMPLEX;
    ITEM_TYPE itype = type_mode ? CSN_COMPLEX : CSN_REAL;

    size_t requested_size = 0;
    if (get_array_size_from_shape(&requested_size, new_ndim, new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    size_t logical_size = (source_arr_a->size == 0 && source_arr_b->size == 0) ? 0 : requested_size;

    /* Unlike the permuting opcodes, an elementwise binop may feed on its own
       output (X = csnadd(X, B)): each result cell is written once from the cell
       it reads. That only holds while the result keeps the aliased operand's
       exact layout — a broadcast that grows it makes NEED_TO_UPDATE_SLOT
       reallocate and drop the data the fill is about to read. */
    uint32_t owned = p->k_data.owned_handle;
    if (source_handle_a == owned || source_handle_b == owned) {
        const CSN_ARRAY *aliased = source_handle_a == owned ? source_arr_a : source_arr_b;
        if (aliased->ndim != new_ndim
            || aliased->itype != itype
            || memcmp(aliased->shape, new_shape, sizeof(uint32_t) * new_ndim) != 0) {
            char abuf[CSN_SHAPE_STR_MAX], bbuf[CSN_SHAPE_STR_MAX];
            csound->UnlockMutex(reg->mutex);
            return csound->PerfError(csound, &p->h, "[csnarray] Input array %u is also this opcode's own output, so the result must keep its %s layout, not %s: assign the broadcast result to a different handle", owned, shape_str(abuf, sizeof(abuf), aliased->shape, aliased->ndim), shape_str(bbuf, sizeof(bbuf), new_shape, new_ndim));
        }
    }

    CSN_SLOT *out_slot = get_slot(reg, owned);
    if (out_slot != NULL && CAN_REUSE_ELEMENTWISE(&p->k_data, source_handle_a, source_arr_a, source_handle_b, source_arr_b, out_slot->array, 0.0, 0.0)) {
        p->handle->id = owned;
        goto done;
    }

    /* The slot the i-time pass registered is republished in place: allocating a
       new array here would register a fresh handle on every control period. */
    CSN_ARRAY *arr = NULL;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, new_ndim, new_shape, logical_size, itype, err);
    if (res != OK) goto done;
    p->array = arr;

    res = binop_hh_assign_value(csound, &p->h, source_arr_a, source_arr_b, arr, itype, mode);
    if (res != OK) goto done;
    SET_KDATA_END(p, new_shape, new_ndim, itype);
    p->k_data.prev_size = arr->size;
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle_a, source_arr_a, source_handle_b, source_arr_b, arr, 0.0, 0.0);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}


static int32_t binop_hs_sh_body(CSOUND *csound, OPDS *perf_h, CSN_REGISTRY *reg, CSN_ARRAY **source_array, uint32_t source_handle, COMPLEXDAT *complex_arg, CSN_BINOP_MODE mode) {
    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
    }

    *source_array = source_slot->array;
    CSN_ARRAY *source_arr = *source_array;

    if ((mode == CSN_LOGICAL_AND_HS || mode == CSN_LOGICAL_OR_HS) && source_arr->itype == CSN_COMPLEX) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Logical and/or supports real array only");
    }

    if (source_arr->itype == CSN_COMPLEX && mode == CSN_HYPOT_HS) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Hypot supports real array only");
    }

    if (source_arr->itype == CSN_COMPLEX && (mode == CSN_MINIMUM_HS || mode == CSN_MAXIMUM_HS)) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Minimum/Maximum supports real array only");
    }

    if (source_arr->itype == CSN_COMPLEX && (mode == CSN_ATAN2_HS || mode == CSN_ATAN2_SH)) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] atan2 supports real array only");
    }

    if (complex_arg != NULL && (mode == CSN_LOGICAL_AND_HS || mode == CSN_LOGICAL_OR_HS)) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Logical and/or supports real scalar only");
    }

    if (complex_arg != NULL && source_arr->itype != CSN_COMPLEX) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Handle holds a real array; use a real scalar instead of a :Complex; one");
    }

    return OK;
}

static int32_t binop_hs_sh_assign_value(CSOUND *csound, OPDS *perf_h, const MYFLT *scalar_arg, const COMPLEXDAT *complex_arg, CSN_ARRAY *source_arr, CSN_ARRAY *arr, CSN_BINOP_MODE mode) {
    double real_scalar = 0.0;
    if (scalar_arg != NULL) {
        real_scalar = (double) *scalar_arg;
    }

    CSN_COMPLEXDAT complex_scalar = { 0.0, 0.0 };
    if (complex_arg != NULL) {
        double re, im;
        complexdat_to_rect(complex_arg, &re, &im);
        complex_scalar.re = re;
        complex_scalar.im = im;
    } else {
        complex_scalar.re = real_scalar;
    }
    for (size_t i = 0; i < source_arr->size; i++) {
        double a = source_arr->data[i];
        CSN_COMPLEXDAT ca = { 0.0, 0.0 };
        CSN_COMPLEXDAT c = {0};
        if (source_arr->itype == CSN_COMPLEX) {
            ca.re = source_arr->data[i * 2];
            ca.im = source_arr->data[i * 2 + 1];
        }
        switch (mode) {
            case CSN_ADD_HS:
            case CSN_ADD_SH:
                if (source_arr->itype == CSN_REAL) {
                    arr->data[i] = a + real_scalar;
                } else {
                    complex_add(&c, ca, complex_scalar);
                    arr->data[i * 2] = c.re;
                    arr->data[i * 2 + 1] = c.im;
                }
                break;
            case CSN_SUB_HS:
                if (source_arr->itype == CSN_REAL) {
                    arr->data[i] = a - real_scalar;
                } else {
                    complex_sub(&c, ca, complex_scalar);
                    arr->data[i * 2] = c.re;
                    arr->data[i * 2 + 1] = c.im;
                }
                break;
            case CSN_SUB_SH:
                if (source_arr->itype == CSN_REAL) {
                    arr->data[i] = real_scalar - a;
                } else {
                    complex_sub(&c, complex_scalar, ca);
                    arr->data[i * 2] = c.re;
                    arr->data[i * 2 + 1] = c.im;
                }
                break;
            case CSN_MUL_HS:
            case CSN_MUL_SH:
                if (source_arr->itype == CSN_REAL) {
                    arr->data[i] = a * real_scalar;
                } else {
                    complex_prod(&c, ca, complex_scalar);
                    arr->data[i * 2] = c.re;
                    arr->data[i * 2 + 1] = c.im;
                }
                break;
            /* Division and log follow IEEE, as numpy does: x/0 is an infinity,
               0/0 and log of a negative are NaN. Raising here instead would
               make a note vanish mid-performance depending on its data. */
            case CSN_DIV_HS:
                if (source_arr->itype == CSN_REAL) {
                    arr->data[i] = a / real_scalar;
                } else {
                    if (complex_div(&c, ca, complex_scalar) != OK) {
                        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Division by zero");
                    };
                    arr->data[i * 2] = c.re;
                    arr->data[i * 2 + 1] = c.im;
                }
                break;
            case CSN_DIV_SH:
                if (source_arr->itype == CSN_REAL) {
                    arr->data[i] = real_scalar / a;
                } else {
                    if (complex_div(&c, complex_scalar, ca) != OK) {
                        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Division by zero");
                    };
                    arr->data[i * 2] = c.re;
                    arr->data[i * 2 + 1] = c.im;
                }
                break;
            case CSN_POW_HS:
                if (source_arr->itype == CSN_REAL) {
                    arr->data[i] = pow(a, real_scalar);
                } else {
                    if (complex_pow(&c, ca, complex_scalar) != OK) {
                        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Division by zero");
                    };
                    arr->data[i * 2] = c.re;
                    arr->data[i * 2 + 1] = c.im;
                }
                break;
            case CSN_POW_SH:
                if (source_arr->itype == CSN_REAL) {
                    arr->data[i] = pow(real_scalar, a);
                } else {
                    if (complex_pow(&c, complex_scalar, ca) != OK) {
                        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Division by zero");
                    };
                    arr->data[i * 2] = c.re;
                    arr->data[i * 2 + 1] = c.im;
                }
                break;
            case CSN_LOG_HS:
                /* Change of base. Base 1 divides by log(1) == 0 and yields an
                   infinity, which is also what numpy returns. */
                if (source_arr->itype == CSN_REAL) {
                    arr->data[i] = log(a) / log(real_scalar);
                } else {
                    CSN_COMPLEXDAT log_a = {0};
                    CSN_COMPLEXDAT log_b = {0};
                    complex_log(&log_a, ca);
                    complex_log(&log_b, complex_scalar);
                    if (complex_div(&c, log_a, log_b) != OK) {
                        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Division by zero");
                    }
                    arr->data[i * 2] = c.re;
                    arr->data[i * 2 + 1] = c.im;
                }
                break;
            case CSN_LOG_SH:
                /* The array supplies the base here, the scalar the argument. */
                if (source_arr->itype == CSN_REAL) {
                    arr->data[i] = log(real_scalar) / log(a);
                } else {
                    CSN_COMPLEXDAT log_a = {0};
                    CSN_COMPLEXDAT log_b = {0};
                    complex_log(&log_a, ca);
                    complex_log(&log_b, complex_scalar);
                    if (complex_div(&c, log_b, log_a) != OK) {
                        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Division by zero");
                    }
                    arr->data[i * 2] = c.re;
                    arr->data[i * 2 + 1] = c.im;
                }
                break;
            case CSN_LOGICAL_AND_HS:
                arr->data[i] = (double) ((a != 0.0) && (real_scalar != 0.0));
                break;
            case CSN_LOGICAL_OR_HS:
                arr->data[i] = (double) ((a != 0.0) || (real_scalar != 0.0));
                break;
            case CSN_HYPOT_HS:
                arr->data[i] = (double) sqrt(a * a + real_scalar * real_scalar);
                break;
            case CSN_MINIMUM_HS:
                arr->data[i] = (double) fmin(a, real_scalar);
                break;
            case CSN_MAXIMUM_HS:
                arr->data[i] = (double) fmax(a, real_scalar);
                break;
            case CSN_ATAN2_HS:
                arr->data[i] = atan2(a, real_scalar);
                break;
            case CSN_ATAN2_SH:
                arr->data[i] = atan2(real_scalar, a);
                break;
            default:
                break;
        }
    }

    return OK;
}

/* The handle and the scalar are passed explicitly rather than read off a
   cast struct: CSN_BINOP_SH lists them in the opposite order to CSN_BINOP_HS,
   so a single cast would silently swap them. */
static int32_t csnarray_binop_hs_sh_helper(CSOUND *csound, CSN_BINOP_COMMON *p, CSNREF *handle_arg, MYFLT *scalar_arg, COMPLEXDAT *complex_arg, CSN_BINOP_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = handle_arg->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    res = binop_hs_sh_body(csound, NULL, reg, &source_arr, source_handle, complex_arg, mode);
    if (res != OK) goto done;

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    memcpy(new_shape, source_arr->shape, sizeof(uint32_t) * CSN_MAX_DIMS);

    const uint32_t protect[1] = { source_handle };
    if (create_csnarray_locked(csound, reg, &p->h, source_arr->ndim, new_shape, &p->array, p->handle, protect, 1U, &err, source_arr->itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;

    res = binop_hs_sh_assign_value(csound, NULL, scalar_arg, complex_arg, source_arr, arr, mode);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_binop_hs_sh_k_init_helper(CSOUND *csound, CSN_BINOP_COMMON *p, CSNREF *handle_arg, COMPLEXDAT *complex_arg, CSN_BINOP_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = handle_arg->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    res = binop_hs_sh_body(csound, NULL, reg, &source_arr, source_handle, complex_arg, mode);
    if (res != OK) goto done;

    const uint32_t protect[1] = { source_handle };
    if (create_csnarray_locked(csound, reg, &p->h, source_arr->ndim, source_arr->shape, &p->array, p->handle, protect, 1U, &err, source_arr->itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;
    if (source_arr->size > 0) {
        memcpy(arr->data, source_arr->data, sizeof(double) * source_arr->size * source_arr->itype);
        arr->size = source_arr->size;
    }

    SET_KDATA_BEGIN(p, reg);
    p->k_data.prev_size = source_arr->size;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_binop_hs_sh_k_helper(CSOUND *csound, CSN_BINOP_COMMON *p, CSNREF *handle_arg, MYFLT *scalar_arg, COMPLEXDAT *complex_arg, MYFLT *trig, CSN_BINOP_MODE mode) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REG_HANDLE(csound, &p->h, reg, p->k_data.owned_handle);

    CHECK_KTRIG(trig);

    uint32_t source_handle = handle_arg->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    res = binop_hs_sh_body(csound, &p->h, reg, &source_arr, source_handle, complex_arg, mode);
    if (res != OK) goto done;

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    memcpy(new_shape, source_arr->shape, sizeof(uint32_t) * CSN_MAX_DIMS);

    size_t requested_size = 0;
    if (get_array_size_from_shape(&requested_size, source_arr->ndim, new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    /* The scalar parameter belongs to the reuse key just as much as the array:
       the complex forms carry two components, the real ones only the first. */
    double scalar_key = scalar_arg != NULL ? (double) *scalar_arg : 0.0;
    double scalar_key_im = 0.0;
    if (complex_arg != NULL) {
        complexdat_to_rect(complex_arg, &scalar_key, &scalar_key_im);
    }

    CSN_SLOT *out_slot = get_slot(reg, p->k_data.owned_handle);
    if (out_slot != NULL && CAN_REUSE_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, out_slot->array, scalar_key, scalar_key_im)) {
        p->handle->id = p->k_data.owned_handle;
        goto done;
    }

    /* The scalar forms keep the source layout, so X = csnmul(X, k) needs no
       alias check: the request always matches the destination already. */
    CSN_ARRAY *arr = NULL;
    size_t logical_size = source_arr->size == 0 ? 0 : requested_size;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, source_arr->ndim, new_shape, logical_size, source_arr->itype, err);
    if (res != OK) goto done;
    p->array = arr;

    res = binop_hs_sh_assign_value(csound, &p->h, scalar_arg, complex_arg, source_arr, arr, mode);
    if (res != OK) goto done;

    SET_KDATA_END(p, new_shape, source_arr->ndim, source_arr->itype);
    p->k_data.prev_size = arr->size;
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, arr, scalar_key, scalar_key_im);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_add_hh(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_helper(csound, p, CSN_ADD_HH);
}

int32_t csnarray_add_hh_k_init(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_k_init_helper(csound, p, CSN_ADD_HH);
}

int32_t csnarray_add_hh_k(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_k_helper(csound, p, CSN_ADD_HH);
}

int32_t csnarray_add_hs(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, CSN_ADD_HS);
}

int32_t csnarray_add_hs_k_init(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_k_init_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, CSN_ADD_HS);
}

int32_t csnarray_add_hs_k(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_k_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, p->trig, CSN_ADD_HS);
}

int32_t csnarray_subtract_hh(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_helper(csound, p, CSN_SUB_HH);
}

int32_t csnarray_subtract_hh_k_init(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_k_init_helper(csound, p, CSN_SUB_HH);
}

int32_t csnarray_subtract_hh_k(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_k_helper(csound, p, CSN_SUB_HH);
}

int32_t csnarray_subtract_hs(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, CSN_SUB_HS);
}

int32_t csnarray_subtract_hs_k_init(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_k_init_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, CSN_SUB_HS);
}

int32_t csnarray_subtract_hs_k(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_k_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, p->trig, CSN_SUB_HS);
}

int32_t csnarray_subtract_sh(CSOUND *csound, CSN_BINOP_SH *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, CSN_SUB_SH);
}

int32_t csnarray_subtract_sh_k_init(CSOUND *csound, CSN_BINOP_SH *p) {
    return csnarray_binop_hs_sh_k_init_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, CSN_SUB_SH);
}

int32_t csnarray_subtract_sh_k(CSOUND *csound, CSN_BINOP_SH *p) {
    return csnarray_binop_hs_sh_k_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, p->trig, CSN_SUB_SH);
}

int32_t csnarray_mul_hh(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_helper(csound, p, CSN_MUL_HH);
}

int32_t csnarray_mul_hh_k_init(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_k_init_helper(csound, p, CSN_MUL_HH);
}

int32_t csnarray_mul_hh_k(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_k_helper(csound, p, CSN_MUL_HH);
}

int32_t csnarray_mul_hs(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, CSN_MUL_HS);
}

int32_t csnarray_mul_hs_k_init(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_k_init_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, CSN_MUL_HS);
}

int32_t csnarray_mul_hs_k(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_k_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, p->trig, CSN_MUL_HS);
}

int32_t csnarray_div_hh(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_helper(csound, p, CSN_DIV_HH);
}

int32_t csnarray_div_hh_k_init(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_k_init_helper(csound, p, CSN_DIV_HH);
}

int32_t csnarray_div_hh_k(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_k_helper(csound, p, CSN_DIV_HH);
}

int32_t csnarray_div_hs(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, CSN_DIV_HS);
}

int32_t csnarray_div_hs_k_init(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, CSN_DIV_HS);
}

int32_t csnarray_div_hs_k(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_k_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, p->trig, CSN_DIV_HS);
}

int32_t csnarray_div_sh(CSOUND *csound, CSN_BINOP_SH *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, CSN_DIV_SH);
}

int32_t csnarray_div_sh_k_init(CSOUND *csound, CSN_BINOP_SH *p) {
    return csnarray_binop_hs_sh_k_init_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, CSN_DIV_SH);
}

int32_t csnarray_div_sh_k(CSOUND *csound, CSN_BINOP_SH *p) {
    return csnarray_binop_hs_sh_k_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, p->trig, CSN_DIV_SH);
}

int32_t csnarray_pow_hh(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_helper(csound, p, CSN_POW_HH);
}

int32_t csnarray_pow_hh_k_init(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_k_init_helper(csound, p, CSN_POW_HH);
}

int32_t csnarray_pow_hh_k(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_k_helper(csound, p, CSN_POW_HH);
}

int32_t csnarray_pow_hs(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, CSN_POW_HS);
}

int32_t csnarray_pow_hs_k_init(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_k_init_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, CSN_POW_HS);
}

int32_t csnarray_pow_hs_k(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_k_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, p->trig, CSN_POW_HS);
}

int32_t csnarray_pow_sh(CSOUND *csound, CSN_BINOP_SH *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, CSN_POW_SH);
}

int32_t csnarray_pow_sh_k_init(CSOUND *csound, CSN_BINOP_SH *p) {
    return csnarray_binop_hs_sh_k_init_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, CSN_POW_SH);
}

int32_t csnarray_pow_sh_k(CSOUND *csound, CSN_BINOP_SH *p) {
    return csnarray_binop_hs_sh_k_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, p->trig, CSN_POW_SH);
}

int32_t csnarray_log_hh(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_helper(csound, p, CSN_LOG_HH);
}

int32_t csnarray_log_hh_k_init(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_k_init_helper(csound, p, CSN_LOG_HH);
}

int32_t csnarray_log_hh_k(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_k_helper(csound, p, CSN_LOG_HH);
}

int32_t csnarray_log_hs(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, CSN_LOG_HS);
}

int32_t csnarray_log_hs_k_init(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_k_init_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, CSN_LOG_HS);
}

int32_t csnarray_log_hs_k(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_k_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, p->trig, CSN_LOG_HS);
}

int32_t csnarray_log_sh(CSOUND *csound, CSN_BINOP_SH *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, CSN_LOG_SH);
}

int32_t csnarray_log_sh_k_init(CSOUND *csound, CSN_BINOP_SH *p) {
    return csnarray_binop_hs_sh_k_init_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, CSN_LOG_SH);
}

int32_t csnarray_log_sh_k(CSOUND *csound, CSN_BINOP_SH *p) {
    return csnarray_binop_hs_sh_k_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, p->trig, CSN_LOG_SH);
}

int32_t csnarray_addcomp_hs(CSOUND *csound, CSN_BINOPCOMPLEX_HS *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, p->scalar, CSN_ADD_HS);
}

int32_t csnarray_addcomp_hs_k_init(CSOUND *csound, CSN_BINOPCOMPLEX_HS *p) {
    return csnarray_binop_hs_sh_k_init_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, CSN_ADD_HS);
}

int32_t csnarray_addcomp_hs_k(CSOUND *csound, CSN_BINOPCOMPLEX_HS *p) {
    return csnarray_binop_hs_sh_k_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, p->scalar, p->trig, CSN_ADD_HS);
}

int32_t csnarray_subtractcomp_hs(CSOUND *csound, CSN_BINOPCOMPLEX_HS *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, p->scalar, CSN_SUB_HS);
}

int32_t csnarray_subtractcomp_hs_k_init(CSOUND *csound, CSN_BINOPCOMPLEX_HS *p) {
    return csnarray_binop_hs_sh_k_init_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, CSN_SUB_HS);
}

int32_t csnarray_subtractcomp_hs_k(CSOUND *csound, CSN_BINOPCOMPLEX_HS *p) {
    return csnarray_binop_hs_sh_k_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, p->scalar, p->trig, CSN_SUB_HS);
}

int32_t csnarray_subtractcomp_sh(CSOUND *csound, CSN_BINOPCOMPLEX_SH *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, p->scalar, CSN_SUB_SH);
}

int32_t csnarray_subtractcomp_sh_k_init(CSOUND *csound, CSN_BINOPCOMPLEX_SH *p) {
    return csnarray_binop_hs_sh_k_init_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, CSN_SUB_SH);
}

int32_t csnarray_subtractcomp_sh_k(CSOUND *csound, CSN_BINOPCOMPLEX_SH *p) {
    return csnarray_binop_hs_sh_k_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, p->scalar, p->trig, CSN_SUB_SH);
}

int32_t csnarray_mulcomp_hs(CSOUND *csound, CSN_BINOPCOMPLEX_HS *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, p->scalar, CSN_MUL_HS);
}

int32_t csnarray_mulcomp_hs_k_init(CSOUND *csound, CSN_BINOPCOMPLEX_HS *p) {
    return csnarray_binop_hs_sh_k_init_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, CSN_MUL_HS);
}

int32_t csnarray_mulcomp_hs_k(CSOUND *csound, CSN_BINOPCOMPLEX_HS *p) {
    return csnarray_binop_hs_sh_k_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, p->scalar, p->trig, CSN_MUL_HS);
}

int32_t csnarray_divcomp_hs(CSOUND *csound, CSN_BINOPCOMPLEX_HS *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, p->scalar, CSN_DIV_HS);
}

int32_t csnarray_divcomp_hs_k_init(CSOUND *csound, CSN_BINOPCOMPLEX_HS *p) {
    return csnarray_binop_hs_sh_k_init_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, CSN_DIV_HS);
}

int32_t csnarray_divcomp_hs_k(CSOUND *csound, CSN_BINOPCOMPLEX_HS *p) {
    return csnarray_binop_hs_sh_k_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, p->scalar, p->trig, CSN_DIV_HS);
}

int32_t csnarray_divcomp_sh(CSOUND *csound, CSN_BINOPCOMPLEX_SH *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, p->scalar, CSN_DIV_SH);
}

int32_t csnarray_divcomp_sh_k_init(CSOUND *csound, CSN_BINOPCOMPLEX_SH *p) {
    return csnarray_binop_hs_sh_k_init_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, CSN_DIV_SH);
}

int32_t csnarray_divcomp_sh_k(CSOUND *csound, CSN_BINOPCOMPLEX_SH *p) {
    return csnarray_binop_hs_sh_k_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, p->scalar, p->trig, CSN_DIV_SH);
}

int32_t csnarray_powcomp_hs(CSOUND *csound, CSN_BINOPCOMPLEX_HS *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, p->scalar, CSN_POW_HS);
}

int32_t csnarray_powcomp_hs_k_init(CSOUND *csound, CSN_BINOPCOMPLEX_HS *p) {
    return csnarray_binop_hs_sh_k_init_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, CSN_POW_HS);
}

int32_t csnarray_powcomp_hs_k(CSOUND *csound, CSN_BINOPCOMPLEX_HS *p) {
    return csnarray_binop_hs_sh_k_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, p->scalar, p->trig, CSN_POW_HS);
}

int32_t csnarray_powcomp_sh(CSOUND *csound, CSN_BINOPCOMPLEX_SH *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, p->scalar, CSN_POW_SH);
}

int32_t csnarray_powcomp_sh_k_init(CSOUND *csound, CSN_BINOPCOMPLEX_SH *p) {
    return csnarray_binop_hs_sh_k_init_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, CSN_POW_SH);
}

int32_t csnarray_powcomp_sh_k(CSOUND *csound, CSN_BINOPCOMPLEX_SH *p) {
    return csnarray_binop_hs_sh_k_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, p->scalar, p->trig, CSN_POW_SH);
}

int32_t csnarray_logcomp_sh(CSOUND *csound, CSN_BINOPCOMPLEX_SH *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, p->scalar, CSN_LOG_SH);
}

int32_t csnarray_logcomp_sh_k_init(CSOUND *csound, CSN_BINOPCOMPLEX_SH *p) {
    return csnarray_binop_hs_sh_k_init_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, CSN_LOG_SH);
}

int32_t csnarray_logcomp_sh_k(CSOUND *csound, CSN_BINOPCOMPLEX_SH *p) {
    return csnarray_binop_hs_sh_k_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, p->scalar, p->trig, CSN_LOG_SH);
}

int32_t csnarray_logcomp_hs(CSOUND *csound, CSN_BINOPCOMPLEX_HS *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, p->scalar, CSN_LOG_HS);
}

int32_t csnarray_logcomp_hs_k_init(CSOUND *csound, CSN_BINOPCOMPLEX_HS *p) {
    return csnarray_binop_hs_sh_k_init_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, CSN_LOG_HS);
}

int32_t csnarray_logcomp_hs_k(CSOUND *csound, CSN_BINOPCOMPLEX_HS *p) {
    return csnarray_binop_hs_sh_k_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, p->scalar, p->trig, CSN_LOG_HS);
}

int32_t csnarray_logical_and_hh(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_helper(csound, p, CSN_LOGICAL_AND_HH);
}

int32_t csnarray_logical_and_hh_k_init(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_k_init_helper(csound, p, CSN_LOGICAL_AND_HH);
}

int32_t csnarray_logical_and_hh_k(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_k_helper(csound, p, CSN_LOGICAL_AND_HH);
}

int32_t csnarray_logical_or_hh(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_helper(csound, p, CSN_LOGICAL_OR_HH);
}

int32_t csnarray_logical_or_hh_k_init(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_k_init_helper(csound, p, CSN_LOGICAL_OR_HH);
}

int32_t csnarray_logical_or_hh_k(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_k_helper(csound, p, CSN_LOGICAL_OR_HH);
}

int32_t csnarray_logical_and_hs(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, CSN_LOGICAL_AND_HS);
}

int32_t csnarray_logical_and_hs_k_init(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_k_init_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, CSN_LOGICAL_AND_HS);
}

int32_t csnarray_logical_and_hs_k(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_k_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, p->trig, CSN_LOGICAL_AND_HS);
}

int32_t csnarray_logical_or_hs(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, CSN_LOGICAL_OR_HS);
}

int32_t csnarray_logical_or_hs_k_init(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_k_init_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, CSN_LOGICAL_OR_HS);
}

int32_t csnarray_logical_or_hs_k(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_k_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, p->trig, CSN_LOGICAL_OR_HS);
}

int32_t csnarray_logical_and_sh(CSOUND *csound, CSN_BINOP_SH *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, CSN_LOGICAL_AND_HS);
}

int32_t csnarray_logical_and_sh_k_init(CSOUND *csound, CSN_BINOP_SH *p) {
    return csnarray_binop_hs_sh_k_init_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, CSN_LOGICAL_AND_HS);
}

int32_t csnarray_logical_and_sh_k(CSOUND *csound, CSN_BINOP_SH *p) {
    return csnarray_binop_hs_sh_k_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, p->trig, CSN_LOGICAL_AND_HS);
}

int32_t csnarray_logical_or_sh(CSOUND *csound, CSN_BINOP_SH *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, CSN_LOGICAL_OR_HS);
}

int32_t csnarray_logical_or_sh_k_init(CSOUND *csound, CSN_BINOP_SH *p) {
    return csnarray_binop_hs_sh_k_init_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, CSN_LOGICAL_OR_HS);
}

int32_t csnarray_logical_or_sh_k(CSOUND *csound, CSN_BINOP_SH *p) {
    return csnarray_binop_hs_sh_k_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, p->trig, CSN_LOGICAL_OR_HS);
}

int32_t csnarray_hypot_hh(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_helper(csound, p, CSN_HYPOT_HH);
}

int32_t csnarray_hypot_hs(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, CSN_HYPOT_HS);
}

int32_t csnarray_hypot_hh_k_init(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_k_init_helper(csound, p, CSN_HYPOT_HH);
}

int32_t csnarray_hypot_hh_k(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_k_helper(csound, p, CSN_HYPOT_HH);
}

int32_t csnarray_hypot_hs_k_init(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_k_init_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, CSN_HYPOT_HS);
}

int32_t csnarray_hypot_hs_k(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_k_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, p->trig, CSN_HYPOT_HS);
}

int32_t csnarray_minimum_hh(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_helper(csound, p, CSN_MINIMUM_HH);
}

int32_t csnarray_minimum_hs(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, CSN_MINIMUM_HS);
}

int32_t csnarray_minimum_hh_k_init(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_k_init_helper(csound, p, CSN_MINIMUM_HH);
}

int32_t csnarray_minimum_hh_k(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_k_helper(csound, p, CSN_MINIMUM_HH);
}

int32_t csnarray_minimum_hs_k_init(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_k_init_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, CSN_MINIMUM_HS);
}

int32_t csnarray_minimum_hs_k(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_k_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, p->trig, CSN_MINIMUM_HS);
}

int32_t csnarray_maximum_hh(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_helper(csound, p, CSN_MAXIMUM_HH);
}

int32_t csnarray_maximum_hs(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, CSN_MAXIMUM_HS);
}

int32_t csnarray_maximum_hh_k_init(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_k_init_helper(csound, p, CSN_MAXIMUM_HH);
}

int32_t csnarray_maximum_hh_k(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_k_helper(csound, p, CSN_MAXIMUM_HH);
}

int32_t csnarray_maximum_hs_k_init(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_k_init_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, CSN_MAXIMUM_HS);
}

int32_t csnarray_maximum_hs_k(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_k_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, p->trig, CSN_MAXIMUM_HS);
}

int32_t csnarray_atan2_hh(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_helper(csound, p, CSN_ATAN2_HH);
}

int32_t csnarray_atan2_hh_k_init(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_k_init_helper(csound, p, CSN_ATAN2_HH);
}

int32_t csnarray_atan2_hh_k(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_k_helper(csound, p, CSN_ATAN2_HH);
}

int32_t csnarray_atan2_hs(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, CSN_ATAN2_HS);
}

int32_t csnarray_atan2_hs_k_init(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_k_init_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, CSN_ATAN2_HS);
}

int32_t csnarray_atan2_hs_k(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_k_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, p->trig, CSN_ATAN2_HS);
}

int32_t csnarray_atan2_sh(CSOUND *csound, CSN_BINOP_SH *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, CSN_ATAN2_SH);
}

int32_t csnarray_atan2_sh_k_init(CSOUND *csound, CSN_BINOP_SH *p) {
    return csnarray_binop_hs_sh_k_init_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, CSN_ATAN2_SH);
}

int32_t csnarray_atan2_sh_k(CSOUND *csound, CSN_BINOP_SH *p) {
    return csnarray_binop_hs_sh_k_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, p->trig, CSN_ATAN2_SH);
}

static int32_t unaryop_body(CSOUND *csound, OPDS *perf_h, CSN_ARRAY **source_array, CSN_REGISTRY *reg, uint32_t source_handle, ITEM_TYPE *new_itype, uint32_t *new_shape, CSN_UNARY_MODE mode) {
    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
    }

    *source_array = source_slot->array;
    CSN_ARRAY *source_arr = *source_array;
    ITEM_TYPE itype = source_arr->itype;
    if (itype == CSN_COMPLEX
        && (mode == CSN_FLOOR
            || mode == CSN_CEIL
            || mode == CSN_ROUND
            || mode == CSN_LOGICAL_NOT
            || mode == CSN_DEG2RAD
            || mode == CSN_RAD2DEG)
    ) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] floor, ceil, round, logical not and angle operations not allowed for complex array");
    }

    *new_itype = (itype == CSN_COMPLEX && mode == CSN_ABS) ? CSN_REAL : itype;
    memcpy(new_shape, source_arr->shape, sizeof(uint32_t) * CSN_MAX_DIMS);
    return OK;
}

/* itype is the *source's* element type, because it selects how each element is
   read out of source_arr. It is not always the destination's: csnabs over a
   complex array writes a real result, so passing the output type here would
   walk the interleaved source one double at a time. */
static void unaryop_assign_value(CSN_ARRAY *source_arr, CSN_ARRAY *arr, ITEM_TYPE itype, CSN_UNARY_MODE mode) {
    for (size_t i = 0; i < source_arr->size; i++) {
        double a = 0.0;
        CSN_COMPLEXDAT ca = { 0.0, 0.0 };
        CSN_COMPLEXDAT cout = { 0.0, 0.0 };
        if (itype == CSN_REAL) {
            a = source_arr->data[i];
        } else {
            ca.re = source_arr->data[i * 2];
            ca.im = source_arr->data[i * 2 + 1];
        }
        switch (mode) {
            case CSN_SQRT:
                if (itype == CSN_REAL) {
                    arr->data[i] = sqrt(a);
                } else {
                    complex_sqrt(&cout, ca);
                    arr->data[i * 2] = cout.re;
                    arr->data[i * 2 + 1] = cout.im;
                }
                break;
            case CSN_CBRT:
                if (itype == CSN_REAL) {
                    arr->data[i] = cbrt(a);
                } else {
                    CSN_COMPLEXDAT one_third = { 1.0 / 3.0, 0.0 };
                    complex_pow(&cout, ca, one_third);
                    arr->data[i * 2] = cout.re;
                    arr->data[i * 2 + 1] = cout.im;
                }
                break;
            case CSN_ABS:
                if (itype == CSN_REAL) {
                    arr->data[i] = fabs(a);
                } else {
                    arr->data[i] = hypot(ca.re, ca.im);
                }
                break;
            case CSN_SIGN:
                /* numpy: sign(0) is 0 and sign(NaN) is NaN, so returning a
                   itself covers both without a special case. */
                if (itype == CSN_REAL) {
                    arr->data[i] = (a > 0.0) ? 1.0 : ((a < 0.0) ? -1.0 : a);
                } else {
                    complex_sign(&cout, ca);
                    arr->data[i * 2] = cout.re;
                    arr->data[i * 2 + 1] = cout.im;
                }
                break;
            case CSN_EXP:
                if (itype == CSN_REAL) {
                    arr->data[i] = exp(a);
                } else {
                    complex_exp(&cout, ca);
                    arr->data[i * 2] = cout.re;
                    arr->data[i * 2 + 1] = cout.im;
                }
                break;
            case CSN_SIN:
                if (itype == CSN_REAL) {
                    arr->data[i] = sin(a);
                } else {
                    complex_sin(&cout, ca);
                    arr->data[i * 2] = cout.re;
                    arr->data[i * 2 + 1] = cout.im;
                }
                break;
            case CSN_COS:
                if (itype == CSN_REAL) {
                    arr->data[i] = cos(a);
                } else {
                    complex_cos(&cout, ca);
                    arr->data[i * 2] = cout.re;
                    arr->data[i * 2 + 1] = cout.im;
                }
                break;
            case CSN_TAN:
                if (itype == CSN_REAL) {
                    arr->data[i] = tan(a);
                } else {
                    complex_tan(&cout, ca);
                    arr->data[i * 2] = cout.re;
                    arr->data[i * 2 + 1] = cout.im;
                }
                break;
            case CSN_ASIN:
                if (itype == CSN_REAL) {
                    arr->data[i] = asin(a);
                } else {
                    complex_asin(&cout, ca);
                    arr->data[i * 2] = cout.re;
                    arr->data[i * 2 + 1] = cout.im;
                }
                break;
            case CSN_ACOS:
                if (itype == CSN_REAL) {
                    arr->data[i] = acos(a);
                } else {
                    complex_acos(&cout, ca);
                    arr->data[i * 2] = cout.re;
                    arr->data[i * 2 + 1] = cout.im;
                }
                break;
            case CSN_ATAN:
                if (itype == CSN_REAL) {
                    arr->data[i] = atan(a);
                } else {
                    complex_atan(&cout, ca);
                    arr->data[i * 2] = cout.re;
                    arr->data[i * 2 + 1] = cout.im;
                }
                break;
            case CSN_SINH:
                if (itype == CSN_REAL) {
                    arr->data[i] = sinh(a);
                } else {
                    complex_sinh(&cout, ca);
                    arr->data[i * 2] = cout.re;
                    arr->data[i * 2 + 1] = cout.im;
                }
                break;
            case CSN_COSH:
                if (itype == CSN_REAL) {
                    arr->data[i] = cosh(a);
                } else {
                    complex_cosh(&cout, ca);
                    arr->data[i * 2] = cout.re;
                    arr->data[i * 2 + 1] = cout.im;
                }
                break;
            case CSN_TANH:
                if (itype == CSN_REAL) {
                    arr->data[i] = tanh(a);
                } else {
                    complex_tanh(&cout, ca);
                    arr->data[i * 2] = cout.re;
                    arr->data[i * 2 + 1] = cout.im;
                }
                break;
            case CSN_ASINH:
                if (itype == CSN_REAL) {
                    arr->data[i] = asinh(a);
                } else {
                    complex_asinh(&cout, ca);
                    arr->data[i * 2] = cout.re;
                    arr->data[i * 2 + 1] = cout.im;
                }
                break;
            case CSN_ACOSH:
                if (itype == CSN_REAL) {
                    arr->data[i] = acosh(a);
                } else {
                    complex_acosh(&cout, ca);
                    arr->data[i * 2] = cout.re;
                    arr->data[i * 2 + 1] = cout.im;
                }
                break;
            case CSN_ATANH:
                if (itype == CSN_REAL) {
                    arr->data[i] = atanh(a);
                } else {
                    complex_atanh(&cout, ca);
                    arr->data[i * 2] = cout.re;
                    arr->data[i * 2 + 1] = cout.im;
                }
                break;
            case CSN_FLOOR:
                arr->data[i] = floor(a);
                break;
            case CSN_CEIL:
                arr->data[i] = ceil(a);
                break;
            case CSN_ROUND:
                /* numpy rounds halves to even; C's round() sends them away
                   from zero, so 2.5 would become 3 instead of 2. rint follows
                   the current mode, which is round-to-nearest-even by default. */
                arr->data[i] = rint(a);
                break;
            case CSN_LOGICAL_NOT:
                arr->data[i] = (double) !(a != 0.0);
                break;
            case CSN_DEG2RAD:
            case CSN_RAD2DEG:
                arr->data[i] = mode == CSN_DEG2RAD ? a * M_PI / 180.0 : a * 180.0 / M_PI;
                break;
            default:
                break;
        }
    }
}

static int32_t csnarray_unaryop_helper(CSOUND *csound, CSN_UNARYOP *p, CSN_UNARY_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    ITEM_TYPE new_itype;
    res = unaryop_body(csound, NULL, &source_arr, reg, source_handle, &new_itype, new_shape, mode);
    if (res != OK) goto done;

    const uint32_t protect[1] = { source_handle };
    if (create_csnarray_locked(csound, reg, &p->h, source_arr->ndim, new_shape, &p->array, p->handle, protect, 1U, &err, new_itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;
    unaryop_assign_value(source_arr, arr, source_arr->itype, mode);
    SET_KDATA_BEGIN(p, reg);
    p->k_data.prev_size = arr->size;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_unaryop_k_helper(CSOUND *csound, CSN_UNARYOP *p, CSN_UNARY_MODE mode) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REG_HANDLE(csound, &(p->h), reg, p->k_data.owned_handle);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    const char *err = NULL;

    CHECK_KTRIG(p->trig);

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    ITEM_TYPE new_itype;
    res = unaryop_body(csound, &p->h, &source_arr, reg, source_handle, &new_itype, new_shape, mode);
    if (res != OK) goto done;

    size_t requested_size = 0;
    if (get_array_size_from_shape(&requested_size, source_arr->ndim, new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSN_ARRAY *arr = p->array;
    size_t logical_size = source_arr->size == 0 ? 0 : requested_size;
    /* Every unary function writes the cell it reads and keeps the source's
       shape; the one layout change in the family is csnabs turning a complex
       array real, which the layout test below catches. */
    res = CHECK_SELF_ALIAS_CELL_LOCAL(csound, &p->h, &p->k_data, source_handle, source_arr, source_arr->ndim, new_shape, new_itype);
    if (res != OK) goto done;

    CSN_SLOT *out_slot = get_slot(reg, p->k_data.owned_handle);
    if (out_slot != NULL && CAN_REUSE_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, out_slot->array, 0.0, 0.0)) {
        p->handle->id = p->k_data.owned_handle;
        goto done;
    }

    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, source_arr->ndim, new_shape, logical_size, new_itype, err);
    if (res != OK) goto done;

    unaryop_assign_value(source_arr, arr, source_arr->itype, mode);
    SET_KDATA_END(p, new_shape, source_arr->ndim, new_itype);
    p->k_data.prev_size = arr->size;
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, arr, 0.0, 0.0);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_sqrt(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_helper(csound, p, CSN_SQRT);
}

int32_t csnarray_cbrt(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_helper(csound, p, CSN_CBRT);
}

int32_t csnarray_abs(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_helper(csound, p, CSN_ABS);
}

int32_t csnarray_exp(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_helper(csound, p, CSN_EXP);
}

int32_t csnarray_sin(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_helper(csound, p, CSN_SIN);
}

int32_t csnarray_cos(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_helper(csound, p, CSN_COS);
}

int32_t csnarray_tan(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_helper(csound, p, CSN_TAN);
}

int32_t csnarray_asin(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_helper(csound, p, CSN_ASIN);
}

int32_t csnarray_acos(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_helper(csound, p, CSN_ACOS);
}

int32_t csnarray_atan(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_helper(csound, p, CSN_ATAN);
}

int32_t csnarray_sinh(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_helper(csound, p, CSN_SINH);
}

int32_t csnarray_cosh(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_helper(csound, p, CSN_COSH);
}

int32_t csnarray_tanh(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_helper(csound, p, CSN_TANH);
}

int32_t csnarray_asinh(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_helper(csound, p, CSN_ASINH);
}

int32_t csnarray_acosh(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_helper(csound, p, CSN_ACOSH);
}

int32_t csnarray_atanh(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_helper(csound, p, CSN_ATANH);
}

int32_t csnarray_floor(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_helper(csound, p, CSN_FLOOR);
}

int32_t csnarray_ceil(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_helper(csound, p, CSN_CEIL);
}

int32_t csnarray_round(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_helper(csound, p, CSN_ROUND);
}

int32_t csnarray_sign(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_helper(csound, p, CSN_SIGN);
}

int32_t csnarray_logical_not(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_helper(csound, p, CSN_LOGICAL_NOT);
}

int32_t csnarray_degtorad(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_helper(csound, p, CSN_DEG2RAD);
}

int32_t csnarray_radtodeg(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_helper(csound, p, CSN_RAD2DEG);
}

int32_t csnarray_sqrt_k(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_k_helper(csound, p, CSN_SQRT);
}

int32_t csnarray_cbrt_k(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_k_helper(csound, p, CSN_CBRT);
}

int32_t csnarray_abs_k(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_k_helper(csound, p, CSN_ABS);
}

int32_t csnarray_exp_k(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_k_helper(csound, p, CSN_EXP);
}

int32_t csnarray_sin_k(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_k_helper(csound, p, CSN_SIN);
}

int32_t csnarray_cos_k(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_k_helper(csound, p, CSN_COS);
}

int32_t csnarray_tan_k(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_k_helper(csound, p, CSN_TAN);
}

int32_t csnarray_asin_k(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_k_helper(csound, p, CSN_ASIN);
}

int32_t csnarray_acos_k(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_k_helper(csound, p, CSN_ACOS);
}

int32_t csnarray_atan_k(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_k_helper(csound, p, CSN_ATAN);
}

int32_t csnarray_sinh_k(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_k_helper(csound, p, CSN_SINH);
}

int32_t csnarray_cosh_k(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_k_helper(csound, p, CSN_COSH);
}

int32_t csnarray_tanh_k(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_k_helper(csound, p, CSN_TANH);
}

int32_t csnarray_asinh_k(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_k_helper(csound, p, CSN_ASINH);
}

int32_t csnarray_acosh_k(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_k_helper(csound, p, CSN_ACOSH);
}

int32_t csnarray_atanh_k(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_k_helper(csound, p, CSN_ATANH);
}

int32_t csnarray_floor_k(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_k_helper(csound, p, CSN_FLOOR);
}

int32_t csnarray_ceil_k(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_k_helper(csound, p, CSN_CEIL);
}

int32_t csnarray_round_k(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_k_helper(csound, p, CSN_ROUND);
}

int32_t csnarray_sign_k(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_k_helper(csound, p, CSN_SIGN);
}

int32_t csnarray_logical_not_k(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_k_helper(csound, p, CSN_LOGICAL_NOT);
}

int32_t csnarray_degtorad_k(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_k_helper(csound, p, CSN_DEG2RAD);
}

int32_t csnarray_radtodeg_k(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_k_helper(csound, p, CSN_RAD2DEG);
}

static void complop_unary_assign_value(CSN_ARRAY *source_arr, CSN_ARRAY *arr, CSN_COMPLEXOP_MODE mode) {
    for (size_t i = 0; i < source_arr->size; i ++) {
        switch (mode) {
            case CSN_REAL_PART:
                arr->data[i] = source_arr->data[i * 2];
                break;
            case CSN_IMAG_PART:
                arr->data[i] = source_arr->data[i * 2 + 1];
                break;
            case CSN_REAL_TO_COMPLEX:
                arr->data[i * 2] = source_arr->data[i];
                arr->data[i * 2 + 1] = 0.0;
                break;
            case CSN_CONJ_PART:
                arr->data[i * 2] = source_arr->data[i * 2];
                arr->data[i * 2 + 1] = -source_arr->data[i * 2 + 1];
                break;
            default:
                break;
        }
    }
}

static int32_t csnarray_complop_unary_helper(CSOUND *csound, CSN_UNARYOP *p, CSN_COMPLEXOP_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = source_slot->array;
    ITEM_TYPE itype = source_arr->itype;

    if (mode == CSN_REAL_TO_COMPLEX) {
        if (itype != CSN_REAL) {
            res = csound->InitError(csound, "[csnarray] Real-to-complex operation requires real array");
            goto done;
        }
    } else if (itype != CSN_COMPLEX) {
        res = csound->InitError(csound, "[csnarray] Operation requires complex array");
        goto done;
    }

    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    uint32_t new_dim = source_ndim;
    memcpy(new_shape, source_shape, sizeof(uint32_t) * CSN_MAX_DIMS);

    ITEM_TYPE out_itype = (mode == CSN_CONJ_PART || mode == CSN_REAL_TO_COMPLEX) ? CSN_COMPLEX : CSN_REAL;

    const uint32_t protect[1] = { source_handle };
    if (create_csnarray_locked(csound, reg, &p->h, new_dim, new_shape, &p->array, p->handle, protect, 1U, &err, out_itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;
    complop_unary_assign_value(source_arr, arr, mode);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_complop_unary_k_init_helper(CSOUND *csound, CSN_UNARYOP *p, CSN_COMPLEXOP_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = source_slot->array;
    ITEM_TYPE itype = source_arr->itype;

    if (mode == CSN_REAL_TO_COMPLEX) {
        if (itype != CSN_REAL) {
            res = csound->InitError(csound, "[csnarray] Real-to-complex operation requires real array");
            goto done;
        }
    } else if (itype != CSN_COMPLEX) {
        res = csound->InitError(csound, "[csnarray] Operation requires complex array");
        goto done;
    }

    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    const uint32_t protect[1] = { source_handle };
    if (create_csnarray_locked(csound, reg, &p->h, source_ndim, source_shape, &p->array, p->handle, protect, 1U, &err, source_arr->itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;
    if (source_arr->size > 0) {
        memcpy(arr->data, source_arr->data, sizeof(double) * source_arr->size);
        arr->size = source_arr->size;
    }

    SET_KDATA_BEGIN(p, reg);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_complop_unary_k_helper(CSOUND *csound, CSN_UNARYOP *p, CSN_COMPLEXOP_MODE mode) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REG_HANDLE(csound, &p->h, reg, p->k_data.owned_handle);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    const char *err = NULL;
    /* csnconj writes each cell from itself and stays complex. csnreal, csnimag
       and csntocomplex all change the element type, which reallocates the
       destination and drops the source the fill is about to read. */
    if (mode != CSN_CONJ_PART) {
        res = CHECK_SELF_ALIAS(csound, &p->h, &p->k_data, source_handle, 0);
        if (res != OK) return res;
    }

    CHECK_KTRIG(p->trig);

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
    }

    CSN_ARRAY *source_arr = source_slot->array;
    ITEM_TYPE itype = source_arr->itype;

    if (mode == CSN_REAL_TO_COMPLEX) {
        if (itype != CSN_REAL) {
            csound->UnlockMutex(reg->mutex);
            return csound->PerfError(csound, &p->h, "[csnarray] Real-to-complex operation requires real array");
        }
    } else if (itype != CSN_COMPLEX) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Operation requires complex array");
    }

    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    uint32_t new_dim = source_ndim;
    memcpy(new_shape, source_shape, sizeof(uint32_t) * CSN_MAX_DIMS);

    ITEM_TYPE out_itype = (mode == CSN_CONJ_PART || mode == CSN_REAL_TO_COMPLEX) ? CSN_COMPLEX : CSN_REAL;

    size_t req_size = 0;
    if (get_array_size_from_shape(&req_size, new_dim, new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSN_ARRAY *arr = NULL;
    size_t logical_size = source_arr->size == 0 ? 0 : req_size;
    /* out_itype, not the source's: csnreal and csnimag hand back a real array
       from a complex one, csnconj and csntocomplex the other way round. */
    if (mode == CSN_CONJ_PART) {
        res = CHECK_SELF_ALIAS_CELL_LOCAL(csound, &p->h, &p->k_data, source_handle, source_arr, new_dim, new_shape, out_itype);
        if (res != OK) goto done;
    }

    CSN_SLOT *reuse_slot = get_slot(reg, p->k_data.owned_handle);
    if (reuse_slot != NULL && CAN_REUSE_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, reuse_slot->array, 0.0, 0.0)) {
        p->handle->id = p->k_data.owned_handle;
        goto done;
    }

    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, new_dim, new_shape, logical_size, out_itype, err);
    if (res != OK) goto done;
    p->array = arr;

    complop_unary_assign_value(source_arr, arr, mode);
    SET_KDATA_END(p, new_shape, new_dim, out_itype);
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, arr, 0.0, 0.0);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_real(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_complop_unary_helper(csound, p, CSN_REAL_PART);
}

int32_t csnarray_real_k_init(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_complop_unary_k_init_helper(csound, p, CSN_REAL_PART);
}

int32_t csnarray_real_k(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_complop_unary_k_helper(csound, p, CSN_REAL_PART);
}

int32_t csnarray_imag(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_complop_unary_helper(csound, p, CSN_IMAG_PART);
}

int32_t csnarray_imag_k_init(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_complop_unary_k_init_helper(csound, p, CSN_IMAG_PART);
}

int32_t csnarray_imag_k(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_complop_unary_k_helper(csound, p, CSN_IMAG_PART);
}

int32_t csnarray_complex_to_real(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_complop_unary_helper(csound, p, CSN_REAL_PART);
}

int32_t csnarray_complex_to_real_k_init(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_complop_unary_k_init_helper(csound, p, CSN_REAL_PART);
}

int32_t csnarray_complex_to_real_k(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_complop_unary_k_helper(csound, p, CSN_REAL_PART);
}

int32_t csnarray_real_to_complex(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_complop_unary_helper(csound, p, CSN_REAL_TO_COMPLEX);
}

int32_t csnarray_real_to_complex_k_init(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_complop_unary_k_init_helper(csound, p, CSN_REAL_TO_COMPLEX);
}

int32_t csnarray_real_to_complex_k(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_complop_unary_k_helper(csound, p, CSN_REAL_TO_COMPLEX);
}

int32_t csnarray_conj(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_complop_unary_helper(csound, p, CSN_CONJ_PART);
}

int32_t csnarray_conj_k_init(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_complop_unary_k_init_helper(csound, p, CSN_CONJ_PART);
}

int32_t csnarray_conj_k(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_complop_unary_k_helper(csound, p, CSN_CONJ_PART);
}

static inline double wrap_angle(double angle, double period) {
    double half_period = period * 0.5;
    double x = fmod(half_period + angle, period);
    if (x < 0.0)  x += period;
    return x - half_period;
}

static void unwrap_slice(double *dst, const double *src, size_t n, size_t stride, double period, double discont) {
    if (n == 0) return;
    dst[0] = src[0];
    for (size_t i = 1; i < n; ++i) {
        double delta = src[i * stride] - src[(i - 1) * stride];
        double wrapped = wrap_angle(delta, period);
        double correction = fabs(delta) < discont ? 0.0 : wrapped - delta;
        dst[i * stride] = dst[(i - 1) * stride] + delta + correction;
    }
}

static int32_t angle_body(CSOUND *csound, OPDS *perf_h, CSN_REGISTRY *reg,
                          uint32_t source_handle, CSN_ARRAY **source_array,
                          double in_period, double in_discount,
                          const MYFLT *axis_in, double *out_period,
                          double *out_discount, int32_t *out_axis,
                          CSN_COMPLEXOP_MODE mode) {
    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
    }

    *source_array = source_slot->array;
    CSN_ARRAY *source_arr = *source_array;
    ITEM_TYPE itype = source_arr->itype;

    if (mode == CSN_COMPLEX_TO_ANGLE) {
        if (itype != CSN_COMPLEX) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Complex to angle requires complex array");
        }
    } else if (itype != CSN_REAL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] wrap/unwrap angle operations requires real array");
    }

    switch (mode) {
        case CSN_WRAP:
            *out_period = in_period;
            break;
        case CSN_UNWRAP:
            *out_period = in_period;
            *out_discount = in_discount;
            if (*out_discount <= 0.0) *out_discount = *out_period * 0.5;
            break;
        default:
            break;
    }

    uint32_t source_ndim = source_arr->ndim;

    if (mode != CSN_UNWRAP) {
        *out_axis = -1;
        return OK;
    }

    CSN_AXIS_SPEC axis_spec = csn_normalize_axis(axis_in, source_ndim, CSN_AXIS_DEFAULT_FLATTEN);
    if (axis_spec.kind == CSN_AXIS_INVALID) {
        double value = axis_in == NULL ? 0.0 : (double) *axis_in;
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h,
            "[csnarray] Axis %g is invalid for a %u-D array (valid axes: finite integers %d..%u)",
            value, source_ndim, -(int32_t) source_ndim, source_ndim - 1U);
    }
    *out_axis = axis_spec.kind == CSN_AXIS_FLATTEN ? -1 : (int32_t) axis_spec.index;
    return OK;
}

static int32_t angle_assign_value(CSOUND *csound, OPDS *perf_h, CSN_ARRAY *source_arr, CSN_ARRAY *arr, double period, double discount, int32_t axis, CSN_COMPLEXOP_MODE mode) {
    if (mode == CSN_COMPLEX_TO_ANGLE || mode == CSN_WRAP) {
        for (size_t i = 0; i < source_arr->size; i ++) {
            double angle = 0.0;
            if (mode == CSN_COMPLEX_TO_ANGLE) {
                double re = source_arr->data[i * 2];
                double im = source_arr->data[i * 2 + 1];
                angle = atan2(im, re);
            } else {
                double angle_temp = source_arr->data[i];
                angle = wrap_angle(angle_temp, period);
            }
            arr->data[i] = angle;
        }
    } else if (mode == CSN_UNWRAP) {
        if (source_arr->size < 2) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unwrap needs at least 2 elements, got %zu", source_arr->size);
        }

        if (axis == -1) {
            unwrap_slice(arr->data, source_arr->data, source_arr->size, 1, period, discount);
        } else {
            uint32_t reduced_shape[CSN_MAX_DIMS] = {0};
            uint32_t reduced_ndim = 0;
            size_t slice_count = 1;
            for (uint32_t i = 0; i < source_arr->ndim; ++i) {
                if (i != (uint32_t) axis) {
                    reduced_shape[reduced_ndim++] = source_arr->shape[i];
                    slice_count *= source_arr->shape[i];
                }
            }

            size_t src_stride = source_arr->strides[axis];
            for (size_t linear = 0; linear < slice_count; ++linear) {
                uint32_t dst_coords[CSN_MAX_DIMS] = {0};
                uint32_t src_coords[CSN_MAX_DIMS] = {0};

                from_linear_to_coords(dst_coords, reduced_shape, linear, reduced_ndim);
                for (uint32_t i = 0, j = 0; i < source_arr->ndim; ++i) {
                    src_coords[i] = (i == (uint32_t) axis) ? 0 : dst_coords[j++];
                }

                size_t src_base = from_coords_to_offset(src_coords, source_arr->strides, source_arr->ndim);
                size_t dst_base = from_coords_to_offset(src_coords, arr->strides, source_arr->ndim);
                unwrap_slice(arr->data + dst_base, source_arr->data + src_base, source_arr->shape[axis], src_stride, period, discount);
            }
        }
    }

    return OK;
}

static int32_t csnarray_angle_helper(CSOUND *csound, CSN_ANGLE *p, CSN_COMPLEXOP_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    double in_period = (mode == CSN_WRAP || mode == CSN_UNWRAP) ? (double) *p->arg_a : 0.0;
    double in_discount = mode == CSN_UNWRAP ? (double) *p->arg_b : 0.0;
    const MYFLT *axis_in = mode == CSN_UNWRAP && p->INOCOUNT > 3 ? p->arg_c : NULL;
    double period = 0.0;
    double discount = 0.0;
    int32_t axis = -1;
    res = angle_body(csound, NULL, reg, source_handle, &source_arr, in_period, in_discount, axis_in, &period, &discount, &axis, mode);
    if (res != OK) goto done;

    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    uint32_t new_dim = source_ndim;
    memcpy(new_shape, source_shape, sizeof(uint32_t) * CSN_MAX_DIMS);

    const uint32_t protect[1] = { source_handle };
    if (create_csnarray_locked(csound, reg, &p->h, new_dim, new_shape, &p->array, p->handle, protect, 1U, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;
    res = angle_assign_value(csound, NULL, source_arr, arr, period, discount, axis, mode);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_angle_k_init_helper(CSOUND *csound, CSN_ANGLE *p, CSN_COMPLEXOP_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    double in_period = (mode == CSN_WRAP || mode == CSN_UNWRAP) ? (double) *p->arg_a : 0.0;
    double in_discount = mode == CSN_UNWRAP ? (double) *p->arg_b : 0.0;
    const MYFLT *axis_in = mode == CSN_UNWRAP && p->INOCOUNT > 4 ? p->arg_d : NULL;
    double period = 0.0;
    double discount = 0.0;
    int32_t axis = -1;
    res = angle_body(csound, NULL, reg, source_handle, &source_arr, in_period, in_discount, axis_in, &period, &discount, &axis, mode);
    if (res != OK) goto done;

    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    const uint32_t protect[1] = { source_handle };
    if (create_csnarray_locked(csound, reg, &p->h, source_ndim, source_shape, &p->array, p->handle, protect, 1U, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    if (source_arr->size > 0) {
        memcpy(p->array->data, source_arr->data, sizeof(double) * source_arr->size);
        p->array->size = source_arr->size;
    }

    SET_KDATA_BEGIN(p, reg);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_angle_k_helper(CSOUND *csound, CSN_ANGLE *p, CSN_COMPLEXOP_MODE mode) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REG_HANDLE(csound, &p->h, reg, p->k_data.owned_handle);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    const char *err = NULL;
    /* Wrapping folds each angle on its own; csnunwrap walks an axis and reads
       the neighbour it has just written, and csnangle turns complex into real. */
    if (mode != CSN_WRAP) {
        res = CHECK_SELF_ALIAS(csound, &p->h, &p->k_data, source_handle, 0);
        if (res != OK) return res;
    }

    MYFLT *trig = (mode == CSN_WRAP || mode == CSN_UNWRAP) ? ((mode == CSN_WRAP) ? p->arg_b : p->arg_c) : p->arg_a;
    CHECK_KTRIG(trig);

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    double in_period = (mode == CSN_WRAP || mode == CSN_UNWRAP) ? (double) *p->arg_a : 0.0;
    double in_discount = mode == CSN_UNWRAP ? (double) *p->arg_b : 0.0;
    const MYFLT *axis_in = mode == CSN_UNWRAP && p->INOCOUNT > 4 ? p->arg_d : NULL;
    double period = 0.0;
    double discount = 0.0;
    int32_t axis = -1;
    res = angle_body(csound, &p->h, reg, source_handle, &source_arr, in_period, in_discount, axis_in, &period, &discount, &axis, mode);
    if (res != OK) goto done;

    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;
    ITEM_TYPE itype = source_arr->itype;

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    uint32_t new_dim = source_ndim;
    memcpy(new_shape, source_shape, sizeof(uint32_t) * CSN_MAX_DIMS);

    size_t req_size = 0;
    if (get_array_size_from_shape(&req_size, new_dim, new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSN_ARRAY *arr = p->array;
    size_t logical_size = source_arr->size == 0 ? 0 : req_size;
    if (mode == CSN_WRAP) {
        res = CHECK_SELF_ALIAS_CELL_LOCAL(csound, &p->h, &p->k_data, source_handle, source_arr, new_dim, new_shape, itype);
        if (res != OK) goto done;
    }

    CSN_SLOT *out_slot = get_slot(reg, p->k_data.owned_handle);
    if (out_slot != NULL && CAN_REUSE_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, out_slot->array, period, discount) && (int32_t) p->k_data.prev_axis_u == axis) {
        p->handle->id = p->k_data.owned_handle;
        goto done;
    }

    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, new_dim, new_shape, logical_size, itype, err);
    if (res != OK) goto done;

    res = angle_assign_value(csound, &p->h, source_arr, arr, period, discount, axis, mode);
    if (res != OK) goto done;

    SET_KDATA_END(p, new_shape, new_dim, itype);
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, arr, period, discount);
    p->k_data.prev_axis_u = (uint32_t) axis;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t angle_in_assign_value(CSOUND *csound, OPDS *perf_h, CSN_ARRAY *source_arr, double period, double discount, int32_t axis, CSN_COMPLEXOP_MODE mode) {
    if (mode == CSN_WRAP) {
        for (size_t i = 0; i < source_arr->size; i ++) {
            double angle_temp = source_arr->data[i];
            double angle = wrap_angle(angle_temp, period);
            source_arr->data[i] = angle;
        }
    } else if (mode == CSN_UNWRAP) {
        if (source_arr->size < 2) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unwrap needs at least 2 elements, got %zu", source_arr->size);
        }

        if (axis == -1) {
            unwrap_slice(source_arr->data, source_arr->data, source_arr->size, 1, period, discount);
        } else {
            uint32_t reduced_shape[CSN_MAX_DIMS] = {0};
            uint32_t reduced_ndim = 0;
            size_t slice_count = 1;
            for (uint32_t i = 0; i < source_arr->ndim; ++i) {
                if (i != (uint32_t) axis) {
                    reduced_shape[reduced_ndim++] = source_arr->shape[i];
                    slice_count *= source_arr->shape[i];
                }
            }

            size_t src_stride = source_arr->strides[axis];
            for (size_t linear = 0; linear < slice_count; ++linear) {
                uint32_t dst_coords[CSN_MAX_DIMS] = {0};
                uint32_t src_coords[CSN_MAX_DIMS] = {0};

                from_linear_to_coords(dst_coords, reduced_shape, linear, reduced_ndim);
                for (uint32_t i = 0, j = 0; i < source_arr->ndim; ++i) {
                    src_coords[i] = (i == (uint32_t) axis) ? 0 : dst_coords[j++];
                }

                size_t src_base = from_coords_to_offset(src_coords, source_arr->strides, source_arr->ndim);
                size_t dst_base = from_coords_to_offset(src_coords, source_arr->strides, source_arr->ndim);
                unwrap_slice(source_arr->data + dst_base, source_arr->data + src_base, source_arr->shape[axis], src_stride, period, discount);
            }
        }
    }

    return OK;
}

static int32_t csnarray_angle_in_helper(CSOUND *csound, CSN_ANGLE_IN *p, const MYFLT *axis_in, CSN_COMPLEXOP_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    p->registry = reg;
    uint32_t source_handle = p->source_handle->id;
    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    double in_period = (mode == CSN_WRAP || mode == CSN_UNWRAP) ? (double) *p->arg_a : 0.0;
    double in_discount = mode == CSN_UNWRAP ? (double) *p->arg_b : 0.0;
    double period = 0.0;
    double discount = 0.0;
    int32_t axis = -1;
    res = angle_body(csound, &p->h, reg, source_handle, &source_arr, in_period, in_discount, axis_in, &period, &discount, &axis, mode);
    if (res != OK) goto done;


    res = angle_in_assign_value(csound, &p->h, source_arr, period, discount, axis, mode);
    if (res != OK) goto done;
    update_array_data_version(&source_arr->version);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_angle_in_k_helper(CSOUND *csound, CSN_ANGLE_IN *p, CSN_COMPLEXOP_MODE mode) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    p->registry = reg;
    uint32_t source_handle = p->source_handle->id;
    int32_t res = OK;

    MYFLT *trig = (mode == CSN_WRAP || mode == CSN_UNWRAP) ? ((mode == CSN_WRAP) ? p->arg_b : p->arg_c) : p->arg_a;
    CHECK_KTRIG(trig);

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    double in_period = (mode == CSN_WRAP || mode == CSN_UNWRAP) ? (double) *p->arg_a : 0.0;
    double in_discount = mode == CSN_UNWRAP ? (double) *p->arg_b : 0.0;
    const MYFLT *axis_in = mode == CSN_UNWRAP && p->INOCOUNT > 4 ? p->arg_d : NULL;
    double period = 0.0;
    double discount = 0.0;
    int32_t axis = -1;
    res = angle_body(csound, &p->h, reg, source_handle, &source_arr, in_period, in_discount, axis_in, &period, &discount, &axis, mode);
    if (res != OK) goto done;

    if (CAN_REUSE_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, NULL, period, discount) && (int32_t) p->k_data.prev_axis_u == axis) {
        goto done;
    }

    res = angle_in_assign_value(csound, &p->h, source_arr, period, discount, axis, mode);
    if (res != OK) goto done;
    update_array_data_version(&source_arr->version);
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, NULL, period, discount);
    p->k_data.prev_axis_u = (uint32_t) axis;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}


int32_t csnarray_angle(CSOUND *csound, CSN_ANGLE *p) {
    return csnarray_angle_helper(csound, p, CSN_COMPLEX_TO_ANGLE);
}

int32_t csnarray_angle_k_init(CSOUND *csound, CSN_ANGLE *p) {
    return csnarray_angle_k_init_helper(csound, p, CSN_COMPLEX_TO_ANGLE);
}

int32_t csnarray_angle_k(CSOUND *csound, CSN_ANGLE *p) {
    return csnarray_angle_k_helper(csound, p, CSN_COMPLEX_TO_ANGLE);
}

int32_t csnarray_wrap_angle(CSOUND *csound, CSN_ANGLE *p) {
    return csnarray_angle_helper(csound, p, CSN_WRAP);
}

int32_t csnarray_wrap_angle_k_init(CSOUND *csound, CSN_ANGLE *p) {
    return csnarray_angle_k_init_helper(csound, p, CSN_WRAP);
}

int32_t csnarray_wrap_angle_k(CSOUND *csound, CSN_ANGLE *p) {
    return csnarray_angle_k_helper(csound, p, CSN_WRAP);
}

int32_t csnarray_unwrap_angle(CSOUND *csound, CSN_ANGLE *p) {
    return csnarray_angle_helper(csound, p, CSN_UNWRAP);
}

int32_t csnarray_unwrap_angle_k_init(CSOUND *csound, CSN_ANGLE *p) {
    return csnarray_angle_k_init_helper(csound, p, CSN_UNWRAP);
}

int32_t csnarray_unwrap_angle_k(CSOUND *csound, CSN_ANGLE *p) {
    return csnarray_angle_k_helper(csound, p, CSN_UNWRAP);
}

int32_t csnarray_wrap_angle_in(CSOUND *csound, CSN_ANGLE_IN *p) {
    return csnarray_angle_in_helper(csound, p, NULL, CSN_WRAP);
}

int32_t csnarray_wrap_angle_in_k(CSOUND *csound, CSN_ANGLE_IN *p) {
    return csnarray_angle_in_k_helper(csound, p, CSN_WRAP);
}

int32_t csnarray_unwrap_angle_in(CSOUND *csound, CSN_ANGLE_IN *p) {
    const MYFLT *axis_in = p->INOCOUNT > 3 ? p->arg_c : NULL;
    return csnarray_angle_in_helper(csound, p, axis_in, CSN_UNWRAP);
}

int32_t csnarray_unwrap_angle_in_k_init(CSOUND *csound, CSN_ANGLE_IN *p) {
    const MYFLT *axis_in = p->INOCOUNT > 4 ? p->arg_d : NULL;
    return csnarray_angle_in_helper(csound, p, axis_in, CSN_UNWRAP);
}

int32_t csnarray_unwrap_angle_in_k(CSOUND *csound, CSN_ANGLE_IN *p) {
    return csnarray_angle_in_k_helper(csound, p, CSN_UNWRAP);
}

static int32_t csnarray_type_helper(CSOUND *csound, OPDS *perf_h, CSN_UNARYOP_SCALAR *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, perf_h, reg);

    p->registry = reg;
    uint32_t source_handle = p->source_handle->id;
    int32_t res = OK;

    if (perf_h != NULL) {
        CHECK_KTRIG(p->trig);
    }

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        res = CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = source_slot->array;
    ITEM_TYPE itype = source_arr->itype;
    *p->value = itype == CSN_REAL ? FL(0.0) : FL(1.0);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_type(CSOUND *csound, CSN_UNARYOP_SCALAR *p) {
    return csnarray_type_helper(csound, NULL, p);
}

int32_t csnarray_type_k(CSOUND *csound, CSN_UNARYOP_SCALAR *p) {
    return csnarray_type_helper(csound, &p->h, p);
}

/* Shared by the i-rate form, the k init and the k perf: a straight copy, or the
   same copy read back to front. A complex item moves as a pair. */
static void copy_assign_value(const CSN_ARRAY *source_arr, CSN_ARRAY *arr, bool reverse, ITEM_TYPE itype) {
    if (!reverse) {
        memcpy(arr->data, source_arr->data, sizeof(double) * source_arr->size * (size_t) itype);
        return;
    }

    for (size_t i = 0; i < source_arr->size; i++) {
        size_t src = (source_arr->size - 1U - i) * (size_t) itype;
        size_t dst = i * (size_t) itype;
        arr->data[dst] = source_arr->data[src];
        if (itype == CSN_COMPLEX)
            arr->data[dst + 1U] = source_arr->data[src + 1U];
    }
}

static int32_t csnarray_copy_helper(CSOUND *csound, CSN_UNARYOP *p, bool reverse) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = source_slot->array;
    ITEM_TYPE itype = source_arr->itype;
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    uint32_t new_dim = source_ndim;
    memcpy(new_shape, source_shape, sizeof(uint32_t) * CSN_MAX_DIMS);

    const uint32_t protect[1] = { source_handle };
    if (create_csnarray_locked(csound, reg, &p->h, new_dim, new_shape, &p->array, p->handle, protect, 1U, &err, itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;
    copy_assign_value(source_arr, arr, reverse, itype);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_copy_k_init_helper(CSOUND *csound, CSN_UNARYOP *p, bool reverse) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = source_slot->array;
    ITEM_TYPE itype = source_arr->itype;
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    const uint32_t protect[1] = { source_handle };
    if (create_csnarray_locked(csound, reg, &p->h, source_ndim, source_shape, &p->array, p->handle, protect, 1U, &err, itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    if (source_arr->size > 0) {
        /* reverse and itype matter here too: a gated csnreverse must already
           read back to front, and a complex item is two doubles wide. */
        copy_assign_value(source_arr, p->array, reverse, itype);
        p->array->size = source_arr->size;
    }

    SET_KDATA_BEGIN(p, reg);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_copy_k_helper(CSOUND *csound, CSN_UNARYOP *p, bool reverse) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REG_HANDLE(csound, &p->h, reg, p->k_data.owned_handle);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    const char *err = NULL;

    /* csncopy leaves every value in its own cell, so a handle may copy onto
       itself; csnreverse reads the cell at the far end of the array and must
       still be kept apart from its own output. The layout test is applied once
       the source is resolved, below. */
    if (reverse) {
        res = CHECK_SELF_ALIAS(csound, &p->h, &p->k_data, source_handle, 0);
        if (res != OK) return res;
    }

    CHECK_KTRIG(p->trig);

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
    }

    CSN_ARRAY *source_arr = source_slot->array;
    ITEM_TYPE itype = source_arr->itype;
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    uint32_t new_dim = source_ndim;
    memcpy(new_shape, source_shape, sizeof(uint32_t) * CSN_MAX_DIMS);

    size_t req_size = 0;
    if (get_array_size_from_shape(&req_size, new_dim, new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSN_ARRAY *arr = p->array;
    size_t logical_size = source_arr->size == 0 ? 0 : req_size;
    if (!reverse) {
        res = CHECK_SELF_ALIAS_CELL_LOCAL(csound, &p->h, &p->k_data, source_handle, source_arr, new_dim, new_shape, itype);
        if (res != OK) goto done;
    }

    CSN_SLOT *reuse_slot = get_slot(reg, p->k_data.owned_handle);
    if (reuse_slot != NULL && CAN_REUSE_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, reuse_slot->array, 0.0, 0.0)) {
        p->handle->id = p->k_data.owned_handle;
        goto done;
    }

    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, new_dim, new_shape, logical_size, itype, err);
    if (res != OK) goto done;

    if (arr->data != source_arr->data) {
        copy_assign_value(source_arr, arr, reverse, itype);
    }
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, arr, 0.0, 0.0);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_copy(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_copy_helper(csound, p, false);
}

int32_t csnarray_copy_k_init(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_copy_k_init_helper(csound, p, false);
}

int32_t csnarray_copy_k(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_copy_k_helper(csound, p, false);
}

int32_t csnarray_reverse(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_copy_helper(csound, p, true);
}

int32_t csnarray_reverse_k_init(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_copy_k_init_helper(csound, p, true);
}

int32_t csnarray_reverse_k(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_copy_k_helper(csound, p, true);
}

static int32_t csnarray_unaryop_in_helper(CSOUND *csound, CSN_UNARYOP_IN *p, CSN_UNARY_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    p->registry = reg;
    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = source_slot->array;
    ITEM_TYPE itype = source_arr->itype;

    if (itype == CSN_COMPLEX && (mode == CSN_DEG2RAD || mode == CSN_RAD2DEG)) {
        res = csound->InitError(csound, "[csnarray] Angle conversion requires real arrays");
        goto done;
    }

    switch (mode) {
        case CSN_REVERSE:
            for (size_t i = 0; i < source_arr->size / 2U; i++) {
                size_t lo = i * (size_t) itype;
                size_t hi = (source_arr->size - 1U - i) * (size_t) itype;
                double temp_re = source_arr->data[hi];
                source_arr->data[hi] = source_arr->data[lo];
                source_arr->data[lo] = temp_re;
                if (itype == CSN_COMPLEX) {
                    double temp_im = source_arr->data[hi + 1U];
                    source_arr->data[hi + 1U] = source_arr->data[lo + 1U];
                    source_arr->data[lo + 1U] = temp_im;
                }
            }
            break;
        case CSN_RAD2DEG:
        case CSN_DEG2RAD:
            for (size_t i = 0; i < source_arr->size; i++) {
                double value = source_arr->data[i];
                double angle = mode == CSN_DEG2RAD ? value * M_PI / 180.0 : value * 180.0 / M_PI;
                source_arr->data[i] = angle;
            }
            break;
        default:
            break;
    }
    if (source_arr->size >= 1U) update_array_data_version(&source_arr->version);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_reverse_in(CSOUND *csound, CSN_UNARYOP_IN *p) {
    return csnarray_unaryop_in_helper(csound, p, CSN_REVERSE);
}

int32_t csnarray_degtorad_in(CSOUND *csound, CSN_UNARYOP_IN *p) {
    return csnarray_unaryop_in_helper(csound, p, CSN_DEG2RAD);
}

int32_t csnarray_radtodeg_in(CSOUND *csound, CSN_UNARYOP_IN *p) {
    return csnarray_unaryop_in_helper(csound, p, CSN_RAD2DEG);
}

int32_t csnarray_unaryop_in_k_helper(CSOUND *csound, CSN_UNARYOP_IN *p, CSN_UNARY_MODE mode) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    uint32_t source_handle = p->source_handle->id;
    int32_t res = OK;

    CHECK_KTRIG(p->trig);

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
    }

    CSN_ARRAY *source_arr = source_slot->array;
    ITEM_TYPE itype = source_arr->itype;

    if (itype == CSN_COMPLEX && (mode == CSN_DEG2RAD || mode == CSN_RAD2DEG)) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Angle conversion requires real arrays");
    }

    /* Not idempotent: reversing twice undoes it and a second conversion scales
       again, so a pass must run exactly once per write by someone else. */
    if (CAN_REUSE_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, NULL, 0.0, 0.0)) {
        csound->UnlockMutex(reg->mutex);
        return res;
    }

    switch (mode) {
        case CSN_REVERSE:
            for (size_t i = 0; i < source_arr->size / 2U; i++) {
                size_t lo = i * (size_t) itype;
                size_t hi = (source_arr->size - 1U - i) * (size_t) itype;
                double temp_re = source_arr->data[hi];
                source_arr->data[hi] = source_arr->data[lo];
                source_arr->data[lo] = temp_re;
                if (itype == CSN_COMPLEX) {
                    double temp_im = source_arr->data[hi + 1U];
                    source_arr->data[hi + 1U] = source_arr->data[lo + 1U];
                    source_arr->data[lo + 1U] = temp_im;
                }
            }
            break;
        case CSN_RAD2DEG:
        case CSN_DEG2RAD:
            for (size_t i = 0; i < source_arr->size; i++) {
                double value = source_arr->data[i];
                double angle = mode == CSN_DEG2RAD ? value * M_PI / 180.0 : value * 180.0 / M_PI;
                source_arr->data[i] = angle;
            }
            break;
        default:
            break;
    }
    if (source_arr->size >= 1U) update_array_data_version(&source_arr->version);
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, NULL, 0.0, 0.0);

    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_unaryop_in_k_init(CSOUND *csound, CSN_UNARYOP_IN *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, &p->h, reg);

    p->registry = reg;
    return OK;
}

int32_t csnarray_reverse_in_k(CSOUND *csound, CSN_UNARYOP_IN *p) {
    return csnarray_unaryop_in_k_helper(csound, p, CSN_REVERSE);
}

int32_t csnarray_degtorad_in_k(CSOUND *csound, CSN_UNARYOP_IN *p) {
    return csnarray_unaryop_in_k_helper(csound, p, CSN_DEG2RAD);
}

int32_t csnarray_radtodeg_in_k(CSOUND *csound, CSN_UNARYOP_IN *p) {
    return csnarray_unaryop_in_k_helper(csound, p, CSN_RAD2DEG);
}


static int32_t csnarray_divmod_hh_helper(CSOUND *csound, CSN_DIVMOD_HH *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle_a = (uint32_t) p->source_handle_a->id;
    uint32_t source_handle_b = (uint32_t) p->source_handle_b->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot_a = get_slot(reg, source_handle_a);
    if (slot_a == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle_a);
        goto done;
    }

    CSN_SLOT *slot_b = get_slot(reg, source_handle_b);
    if (slot_b == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle_b);
        goto done;
    }

    CSN_ARRAY *source_arr_a = slot_a->array;
    CSN_ARRAY *source_arr_b = slot_b->array;
    if (source_arr_a->itype == CSN_COMPLEX || source_arr_b->itype == CSN_COMPLEX) {
        res = csound->InitError(csound, "[csnarray] Divmod operation requires real array");
        goto done;
    }

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    uint32_t new_ndim = 0;

    if (broadcast_shape(source_arr_a, source_arr_b, new_shape, &new_ndim) != OK) {
        char abuf[CSN_SHAPE_STR_MAX], bbuf[CSN_SHAPE_STR_MAX];
        res = csound->InitError(csound, "[csnarray] Shapes %s and %s cannot be broadcast together: aligned from the last axis, each pair must match or be 1", shape_str(abuf, sizeof(abuf), source_arr_a->shape, source_arr_a->ndim), shape_str(bbuf, sizeof(bbuf), source_arr_b->shape, source_arr_b->ndim));
        goto done;
    }

    const uint32_t protect[2] = { source_handle_a, source_handle_b };
    if (create_csnarray_locked(csound, reg, &p->h, new_ndim, new_shape, &p->array_a, p->handle_a, protect, 2U, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    if (create_csnarray_locked(csound, reg, &p->h, new_ndim, new_shape, &p->array_b, p->handle_b, protect, 2U, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *q_arr = p->array_a;
    CSN_ARRAY *r_arr = p->array_b;
    size_t size = q_arr->size;

    bool same_shape = source_arr_a->ndim == source_arr_b->ndim
        && memcmp(source_arr_a->shape, source_arr_b->shape, sizeof(uint32_t) * source_arr_a->ndim) == 0;

    for (size_t i = 0; i < size; i++) {
        size_t off_a = i;
        size_t off_b = i;

        if (!same_shape) {
            uint32_t dst_coords[CSN_MAX_DIMS] = {0};
            from_linear_to_coords(dst_coords, new_shape, i, new_ndim);
            off_a = broadcast_offset(source_arr_a, dst_coords, new_ndim);
            off_b = broadcast_offset(source_arr_b, dst_coords, new_ndim);
        }

        double a = source_arr_a->data[off_a];
        double b = source_arr_b->data[off_b];
        if (b == 0.0) {
            res = csound->InitError(csound, "[csnarray] Division by zero");
            goto done;
        }

        double q = floor(a / b);
        double r = a - q * b;

        q_arr->data[i] = q;
        r_arr->data[i] = r;
    }

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_divmod_hs_sh_helper(CSOUND *csound, OPDS *h, CSNREF *handle_a, CSNREF *handle_b, CSNREF *source_handle, CSN_ARRAY **p_array_a, CSN_ARRAY **p_array_b, const MYFLT *scalar, bool is_left) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t shandle = (uint32_t) source_handle->id;

    double scalar_value = (double) *scalar;
    if (!IS_VALID_VALUE(scalar_value)) {
        return csound->InitError(csound, "[csnarray] Invalid scalar value");
    }

    if (scalar_value == 0.0) {
        return csound->InitError(csound, "[csnarray] Division by zero");
    }

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, shandle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", shandle);
        goto done;
    }

    CSN_ARRAY *source_arr = slot->array;
    if (source_arr->itype == CSN_COMPLEX) {
        res = csound->InitError(csound, "[csnarray] Divmod operation requires real array");
        goto done;
    }

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    uint32_t new_ndim = source_arr->ndim;
    memcpy(new_shape, source_arr->shape, sizeof(new_shape));

    const uint32_t protect[1] = { shandle };
    if (create_csnarray_locked(csound, reg, h, new_ndim, new_shape, p_array_a, handle_a, protect, 1U, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    if (create_csnarray_locked(csound, reg, h, new_ndim, new_shape, p_array_b, handle_b, protect, 1U, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *q_arr = *p_array_a;
    CSN_ARRAY *r_arr = *p_array_b;
    size_t size = q_arr->size;

    for (size_t i = 0; i < size; i++) {
        double a = source_arr->data[i];

        double num = is_left ? scalar_value : a;
        double den = is_left ? a : scalar_value;
        if (den == 0.0) {
            return csound->InitError(csound, "[csnarray] Division by zero");
        }
        double q = floor(num / den);
        double r = num - q * den;

        q_arr->data[i] = q;
        r_arr->data[i] = r;
    }

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_divmod_hh(CSOUND *csound, CSN_DIVMOD_HH *p) {
    return csnarray_divmod_hh_helper(csound, p);
}

int32_t csnarray_divmod_hs(CSOUND *csound, CSN_DIVMOD_HS *p) {
    return csnarray_divmod_hs_sh_helper(csound, &p->h, p->handle_a, p->handle_b, p->source_handle, &p->array_a, &p->array_b, p->scalar, false);
}

int32_t csnarray_divmod_sh(CSOUND *csound, CSN_DIVMOD_SH *p) {
    return csnarray_divmod_hs_sh_helper(csound, &p->h, p->handle_a, p->handle_b, p->source_handle, &p->array_a, &p->array_b, p->scalar, true);
}

int32_t csnarray_divmod_hh_k_init(CSOUND *csound, CSN_DIVMOD_HH *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle_a = (uint32_t) p->source_handle_a->id;
    uint32_t source_handle_b = (uint32_t) p->source_handle_b->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot_a = get_slot(reg, source_handle_a);
    if (slot_a == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle_a);
        goto done;
    }

    CSN_SLOT *slot_b = get_slot(reg, source_handle_b);
    if (slot_b == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle_b);
        goto done;
    }

    CSN_ARRAY *source_arr_a = slot_a->array;
    CSN_ARRAY *source_arr_b = slot_b->array;
    if (source_arr_a->itype == CSN_COMPLEX || source_arr_b->itype == CSN_COMPLEX) {
        res = csound->InitError(csound, "[csnarray] Divmod operation requires real array");
        goto done;
    }

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    uint32_t new_ndim = 0;

    if (broadcast_shape(source_arr_a, source_arr_b, new_shape, &new_ndim) != OK) {
        char abuf[CSN_SHAPE_STR_MAX], bbuf[CSN_SHAPE_STR_MAX];
        res = csound->InitError(csound, "[csnarray] Shapes %s and %s cannot be broadcast together: aligned from the last axis, each pair must match or be 1", shape_str(abuf, sizeof(abuf), source_arr_a->shape, source_arr_a->ndim), shape_str(bbuf, sizeof(bbuf), source_arr_b->shape, source_arr_b->ndim));
        goto done;
    }

    const uint32_t protect[2] = { source_handle_a, source_handle_b };
    if (create_csnarray_locked(csound, reg, &p->h, new_ndim, new_shape, &p->array_a, p->handle_a, protect, 2U, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    if (create_csnarray_locked(csound, reg, &p->h, new_ndim, new_shape, &p->array_b, p->handle_b, protect, 2U, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    reset_empty_csnarray(p->array_a, new_ndim, new_shape, CSN_REAL);
    reset_empty_csnarray(p->array_b, new_ndim, new_shape, CSN_REAL);

    memset(p->k_data.prev_shape, 0, sizeof(uint32_t) * CSN_MAX_DIMS);
    memcpy(p->k_data.prev_shape, new_shape, sizeof(uint32_t) * CSN_MAX_DIMS);
    p->k_data.prev_ndim = new_ndim;
    p->k_data.prev_divmod_state.owned_handle_q = (uint32_t) p->handle_a->id;
    p->k_data.prev_divmod_state.owned_handle_r = (uint32_t) p->handle_b->id;
    p->k_data.registry = reg;
    p->is_published = false;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_divmod_hs_sh_k_init_helper(CSOUND *csound, OPDS *h, CSNREF *handle_a, CSNREF *handle_b, CSN_ARRAY **p_array_a, CSN_ARRAY **p_array_b, CSNREF *source_handle, K_DATA *k_data, bool *is_published) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t shandle = (uint32_t) source_handle->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, shandle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", shandle);
        goto done;
    }

    CSN_ARRAY *source_arr = slot->array;
    if (source_arr->itype == CSN_COMPLEX) {
        res = csound->InitError(csound, "[csnarray] Divmod operation requires real array");
        goto done;
    }

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    uint32_t new_ndim = source_arr->ndim;
    memcpy(new_shape, source_arr->shape, sizeof(new_shape));

    const uint32_t protect[1] = { shandle };
    if (create_csnarray_locked(csound, reg, h, new_ndim, new_shape, p_array_a, handle_a, protect, 1U, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    if (create_csnarray_locked(csound, reg, h, new_ndim, new_shape, p_array_b, handle_b, protect, 1U, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    reset_empty_csnarray(*p_array_a, new_ndim, new_shape, CSN_REAL);
    reset_empty_csnarray(*p_array_b, new_ndim, new_shape, CSN_REAL);

    memset(k_data->prev_shape, 0, sizeof(uint32_t) * CSN_MAX_DIMS);
    memcpy(k_data->prev_shape, new_shape, sizeof(uint32_t) * CSN_MAX_DIMS);
    k_data->prev_ndim = new_ndim;
    k_data->prev_divmod_state.owned_handle_q = handle_a->id;
    k_data->prev_divmod_state.owned_handle_r = handle_b->id;
    k_data->registry = reg;
    *is_published = false;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_divmod_hh_k_helper(CSOUND *csound, CSN_DIVMOD_HH *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    uint32_t handle_a = p->k_data.prev_divmod_state.owned_handle_q;
    uint32_t handle_b = p->k_data.prev_divmod_state.owned_handle_r;
    if (reg == NULL || handle_a == INVALID_HANDLE || handle_b == INVALID_HANDLE) {
        return csound->PerfError(csound, &p->h, "[csnarray] k-rate output slot was not initialized");
    }

    uint32_t source_handle_a = (uint32_t) p->source_handle_a->id;
    uint32_t source_handle_b = (uint32_t) p->source_handle_b->id;

    uint32_t owned_handle_a = p->k_data.prev_divmod_state.owned_handle_q;
    uint32_t owned_handle_b = p->k_data.prev_divmod_state.owned_handle_r;

    CHECK_KTRIG(p->trig);

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot_a = get_slot(reg, source_handle_a);
    if (slot_a == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle_a);
    }

    CSN_SLOT *slot_b = get_slot(reg, source_handle_b);
    if (slot_b == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle_b);
    }

    CSN_ARRAY *source_arr_a = slot_a->array;
    CSN_ARRAY *source_arr_b = slot_b->array;
    if (source_arr_a->itype == CSN_COMPLEX || source_arr_b->itype == CSN_COMPLEX) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Divmod operation requires real array");
    }

    bool is_same_a = is_same_array_version(&source_arr_a->version, &p->k_data.prev_divmod_state.prev_source_a_version);
    bool is_same_b = is_same_array_version(&source_arr_b->version, &p->k_data.prev_divmod_state.prev_source_b_version);

    if (is_same_a && is_same_b && p->is_published) goto done;

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    uint32_t new_ndim = 0;

    if (broadcast_shape(source_arr_a, source_arr_b, new_shape, &new_ndim) != OK) {
        char abuf[CSN_SHAPE_STR_MAX], bbuf[CSN_SHAPE_STR_MAX];
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Shapes %s and %s cannot be broadcast together: aligned from the last axis, each pair must match or be 1", shape_str(abuf, sizeof(abuf), source_arr_a->shape, source_arr_a->ndim), shape_str(bbuf, sizeof(bbuf), source_arr_b->shape, source_arr_b->ndim));
    }

    size_t req_size = 0;
    if (get_array_size_from_shape(&req_size, new_ndim, new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSN_ARRAY *q_arr = NULL;
    size_t logical_size = source_arr_a->size == 0 ? 0 : req_size;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &q_arr, &p->k_data, &owned_handle_a, new_ndim, new_shape, logical_size, CSN_REAL, err);
    if (res != OK) goto done;
    p->array_a = q_arr;

    CSN_ARRAY *r_arr = NULL;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &r_arr, &p->k_data, &owned_handle_b, new_ndim, new_shape, logical_size, CSN_REAL, err);
    if (res != OK) goto done;
    p->array_b = r_arr;

    size_t size = q_arr->size;

    bool same_shape = source_arr_a->ndim == source_arr_b->ndim
        && memcmp(source_arr_a->shape, source_arr_b->shape, sizeof(uint32_t) * source_arr_a->ndim) == 0;

    for (size_t i = 0; i < size; i++) {
        size_t off_a = i;
        size_t off_b = i;

        if (!same_shape) {
            uint32_t dst_coords[CSN_MAX_DIMS] = {0};
            from_linear_to_coords(dst_coords, new_shape, i, new_ndim);
            off_a = broadcast_offset(source_arr_a, dst_coords, new_ndim);
            off_b = broadcast_offset(source_arr_b, dst_coords, new_ndim);
        }

        double a = source_arr_a->data[off_a];
        double b = source_arr_b->data[off_b];
        if (b == 0.0) {
            csound->UnlockMutex(reg->mutex);
            return csound->PerfError(csound, &p->h, "[csnarray] Division by zero");
        }

        double q = floor(a / b);
        double r = a - q * b;

        q_arr->data[i] = q;
        r_arr->data[i] = r;
    }

    memset(p->k_data.prev_shape, 0, sizeof(uint32_t) * CSN_MAX_DIMS);
    memcpy(p->k_data.prev_shape, new_shape, sizeof(uint32_t) * CSN_MAX_DIMS);
    p->k_data.prev_ndim = new_ndim;
    p->handle_a->id = p->k_data.prev_divmod_state.owned_handle_q;
    p->handle_b->id = p->k_data.prev_divmod_state.owned_handle_r;
    p->is_published = true;
    set_array_version(&p->k_data.prev_divmod_state.prev_source_a_version, &source_arr_a->version);
    set_array_version(&p->k_data.prev_divmod_state.prev_source_b_version, &source_arr_b->version);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_divmod_hs_sh_k_helper(CSOUND *csound, OPDS *h, CSNREF *handle_a, CSNREF *handle_b, CSN_ARRAY **p_array_a, CSN_ARRAY **p_array_b, CSNREF *source_handle, const MYFLT *scalar, K_DATA *k_data, bool is_left, bool *is_published) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, h, reg);

    uint32_t shandle = (uint32_t) source_handle->id;
    uint32_t owned_handle_a = k_data->prev_divmod_state.owned_handle_q;
    uint32_t owned_handle_b = k_data->prev_divmod_state.owned_handle_r;

    double scalar_value = (double) *scalar;
    if (!IS_VALID_VALUE(scalar_value)) {
        return csound->PerfError(csound, h, "[csnarray] Invalid scalar value");
    }

    if (scalar_value == 0.0) {
        return csound->PerfError(csound, h, "[csnarray] Division by zero");
    }

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, shandle);
    if (slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", shandle);
    }

    CSN_ARRAY *source_arr = slot->array;
    if (source_arr->itype == CSN_COMPLEX) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, h, "[csnarray] Divmod operation requires real array");
    }

    bool is_same_version = is_same_array_version(&source_arr->version, &k_data->prev_divmod_state.prev_source_a_version);
    if (is_same_version && scalar_value == k_data->prev_scalar_param && *is_published) goto done;

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    uint32_t new_ndim = source_arr->ndim;
    memcpy(new_shape, source_arr->shape, sizeof(new_shape));

    size_t req_size = 0;
    if (get_array_size_from_shape(&req_size, new_ndim, new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSN_ARRAY *q_arr = NULL;
    size_t logical_size = source_arr->size == 0 ? 0 : req_size;
    res = NEED_TO_UPDATE_SLOT(csound, h, &q_arr, k_data, &owned_handle_a, new_ndim, new_shape, logical_size, CSN_REAL, err);
    if (res != OK) goto done;
    *p_array_a = q_arr;

    CSN_ARRAY *r_arr = NULL;
    res = NEED_TO_UPDATE_SLOT(csound, h, &r_arr, k_data, &owned_handle_b, new_ndim, new_shape, logical_size, CSN_REAL, err);
    if (res != OK) goto done;
    *p_array_b = r_arr;

    size_t size = q_arr->size;

    for (size_t i = 0; i < size; i++) {
        double a = source_arr->data[i];

        double num = is_left ? scalar_value : a;
        double den = is_left ? a : scalar_value;
        if (den == 0.0) {
            return csound->InitError(csound, "[csnarray] Division by zero");
        }
        double q = floor(num / den);
        double r = num - q * den;

        q_arr->data[i] = q;
        r_arr->data[i] = r;
    }

    memset(k_data->prev_shape, 0, sizeof(uint32_t) * CSN_MAX_DIMS);
    memcpy(k_data->prev_shape, new_shape, sizeof(uint32_t) * CSN_MAX_DIMS);
    k_data->prev_ndim = new_ndim;
    handle_a->id = k_data->prev_divmod_state.owned_handle_q;
    handle_b->id = k_data->prev_divmod_state.owned_handle_r;
    set_array_version(&k_data->prev_divmod_state.prev_source_a_version, &source_arr->version);
    k_data->prev_scalar_param = scalar_value;
    *is_published = true;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_divmod_hh_k(CSOUND *csound, CSN_DIVMOD_HH *p) {
    return csnarray_divmod_hh_k_helper(csound, p);
}

int32_t csnarray_divmod_hs_k_init(CSOUND *csound, CSN_DIVMOD_HS *p) {
    return csnarray_divmod_hs_sh_k_init_helper(csound, &p->h, p->handle_a, p->handle_b, &p->array_a, &p->array_b, p->source_handle, &p->k_data, &p->is_published);
}

int32_t csnarray_divmod_hs_k(CSOUND *csound, CSN_DIVMOD_HS *p) {
    return csnarray_divmod_hs_sh_k_helper(csound, &p->h, p->handle_a, p->handle_b, &p->array_a, &p->array_b, p->source_handle, p->scalar, &p->k_data, false, &p->is_published);
}

int32_t csnarray_divmod_sh_k_init(CSOUND *csound, CSN_DIVMOD_SH *p) {
    return csnarray_divmod_hs_sh_k_init_helper(csound, &p->h, p->handle_a, p->handle_b, &p->array_a, &p->array_b, p->source_handle, &p->k_data, &p->is_published);
}

int32_t csnarray_divmod_sh_k(CSOUND *csound, CSN_DIVMOD_SH *p) {
    return csnarray_divmod_hs_sh_k_helper(csound, &p->h, p->handle_a, p->handle_b, &p->array_a, &p->array_b, p->source_handle, p->scalar, &p->k_data, true, &p->is_published);
}
