/* Opcode implementations for the math family.
   The public opcode inventory remains centralized in csnum.c. */
#include "csnum.h"
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

static int32_t binop_hh_assign_value(CSOUND *csound, OPDS *perf_h, CSN_BROADCAST_ITER *it, const CSN_ARRAY *source_arr_a, const CSN_ARRAY *source_arr_b, CSN_ARRAY *arr, ITEM_TYPE itype, CSN_BINOP_MODE mode) {
    while (BROADCAST_ITER_NEXT(it)) {
        size_t i = it->linear_index;
        size_t off_a = it->offsets[0];
        size_t off_b = it->offsets[1];

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

/* An output that is one of its operands has the broadcast shape and type, or
   CSN_ALIAS_ELEMENTWISE refuses it. That operand's offset is then the linear
   index itself, so binop_hh_assign_value reads each element before it writes
   it, and the other operand, which may be broadcast, is never the output:
   X = csnadd(X, Y) and X = csnmul(X, X) run in place with no working copy. */
static int32_t binop_hh_layout(CSOUND *csound, OPDS *perf_h, const CSN_ARRAY *a, const CSN_ARRAY *b, CSN_BINOP_MODE mode, uint32_t *ndim, uint32_t *shape, CSN_BROADCAST_ITER *it, ITEM_TYPE *itype) {
    size_t size = 0;
    size_t broad_strides[CSN_MAX_BROADCAST_INPUTS][CSN_MAX_DIMS] = {0};
    const CSN_ARRAY *sources[2] = { a, b };

    char abuf[CSN_SHAPE_STR_MAX], bbuf[CSN_SHAPE_STR_MAX];
    if (BROADCAST_INIT(ndim, shape, &size, broad_strides, sources, 2) != OK
        || BROADCAST_ITER_INIT(it, *ndim, shape, size, broad_strides, 2) != OK) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Shapes %s and %s cannot be broadcast together: aligned from the last axis, each pair must match or be 1", shape_str(abuf, sizeof(abuf), a->shape, a->ndim), shape_str(bbuf, sizeof(bbuf), b->shape, b->ndim));
    }

    bool any_complex = a->itype == CSN_COMPLEX || b->itype == CSN_COMPLEX;
    if (any_complex && (mode == CSN_LOGICAL_AND_HH || mode == CSN_LOGICAL_OR_HH)) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Logical and and or supports real array only");
    }
    if (any_complex && mode == CSN_HYPOT_HH) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Hypot supports real array only");
    }
    if (any_complex && (mode == CSN_MINIMUM_HH || mode == CSN_MAXIMUM_HH)) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Minimum/Maximum supports real array only");
    }
    if (any_complex && mode == CSN_ATAN2_HH) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] atan2 supports real array only");
    }

    *itype = any_complex ? CSN_COMPLEX : CSN_REAL;
    return OK;
}

/* The array-array core, shared by the csnadd family and the operators.

   An output that is one of its operands has the broadcast shape and type, or
   CSN_ALIAS_ELEMENTWISE refuses it. That operand's offset is then the linear
   index itself, so binop_hh_assign_value reads each element before it writes
   it, and the other operand, which may be broadcast, is never the output:
   X = csnadd(X, Y) and X = csnmul(X, X) run in place with no working copy.
   compute false only lays the output out; the first perf pass computes. */
int32_t csn_binop_hh_init(CSOUND *csound, OPDS *h, CSN_ARRAY *out, CSN_ARRAY *a, CSN_ARRAY *b, K_DATA *k_data, CSN_BINOP_MODE mode, bool compute) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    stamp_globals(csound, h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { out, a, b }, 3);

    uint32_t new_ndim = 0;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    CSN_BROADCAST_ITER it = {0};
    ITEM_TYPE itype = CSN_REAL;
    res = binop_hh_layout(csound, NULL, a, b, mode, &new_ndim, new_shape, &it, &itype);
    if (res != OK) goto done;

    const CSN_ARRAY *sources[2] = { a, b };
    CSNARR_RES r = prepare_output(csound, h, NULL, reg, out, new_ndim, new_shape, itype, sources, 2, CSN_ALIAS_ELEMENTWISE);
    res = r.err;
    if (res != OK) goto done;
    pres.locked |= r.locked;

    if (compute) {
        res = binop_hh_assign_value(csound, NULL, &it, a, b, out, itype, mode);
        if (res != OK) goto done;
        if (r.aliased) update_array_data_version(&out->version);
    }

    SET_KDATA_BEGIN(*k_data, reg, new_shape, new_ndim, itype);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

static int32_t csnarray_binop_hh_helper(CSOUND *csound, CSN_BINOP_HH *p, CSN_BINOP_MODE mode, bool is_k_init) {
    /* the k form computes on its first pass, where its trigger decides */
    return csn_binop_hh_init(csound, &p->h, p->handle, p->source_handle_a, p->source_handle_b, &p->k_data, mode, !is_k_init);
}

int32_t csn_binop_hh_perf(CSOUND *csound, OPDS *h, CSN_ARRAY *out, CSN_ARRAY *a, CSN_ARRAY *b, K_DATA *k_data, CSN_BINOP_MODE mode) {
    CSN_REGISTRY *reg = k_data->registry;
    CHECK_REGISTRY(csound, h, reg);

    int32_t res = OK;
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { out, a, b }, 3);

    /* never true for an output that is one of its operands: X = csnadd(X, Y)
       is meant to apply again on every pass */
    if (CAN_REUSE_ELEMENTWISE(k_data, a, b, out, 0.0, 0.0)) goto done;

    uint32_t new_ndim = 0;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    CSN_BROADCAST_ITER it = {0};
    ITEM_TYPE itype = CSN_REAL;
    res = binop_hh_layout(csound, h, a, b, mode, &new_ndim, new_shape, &it, &itype);
    if (res != OK) goto done;

    const CSN_ARRAY *sources[2] = { a, b };
    CSNARR_RES r = prepare_output(csound, h, h, reg, out, new_ndim, new_shape, itype, sources, 2, CSN_ALIAS_ELEMENTWISE);
    res = r.err;
    if (res != OK) goto done;
    pres.locked |= r.locked;

    res = binop_hh_assign_value(csound, h, &it, a, b, out, itype, mode);
    if (res != OK) goto done;
    if (r.aliased) update_array_data_version(&out->version);

    SET_KDATA_END(*k_data, new_shape, new_ndim, itype);
    PUBLISH_ELEMENTWISE(k_data, a, b, out, 0.0, 0.0);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

static int32_t csnarray_binop_hh_k_helper(CSOUND *csound, CSN_BINOP_HH *p, CSN_BINOP_MODE mode) {
    CHECK_KTRIG(p->trig);
    return csn_binop_hh_perf(csound, &p->h, p->handle, p->source_handle_a, p->source_handle_b, &p->k_data, mode);
}


static int32_t binop_hs_sh_body(CSOUND *csound, OPDS *perf_h, const CSN_ARRAY *source_arr, const COMPLEXDAT *complex_arg, CSN_BINOP_MODE mode) {
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

static int32_t binop_hs_sh_assign_value(CSOUND *csound, OPDS *perf_h, const cs_float *scalar_arg, const COMPLEXDAT *complex_arg, CSN_ARRAY *source_arr, CSN_ARRAY *arr, CSN_BINOP_MODE mode) {
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
/* The array and the scalar are passed explicitly rather than read off a cast
   struct: CSN_BINOP_SH lists them in the opposite order to CSN_BINOP_HS, so a
   single cast would silently swap them. Each result element comes from the
   element in the same cell, so X = csnmul(X, 0.5) runs in place
   (CSN_ALIAS_ELEMENTWISE). */
/* The array-scalar core, shared by the csnadd.hs family and the operators.
   Each result element comes from the element in the same cell, so
   X = csnmul(X, 0.5) runs in place (CSN_ALIAS_ELEMENTWISE). compute false only
   lays the output out, for the k forms whose scalar is a k-argument still at
   its init value: the first perf pass computes. */
int32_t csn_binop_hs_sh_init(CSOUND *csound, OPDS *h, CSN_ARRAY *out, CSN_ARRAY *source_arr, cs_float *scalar_arg, COMPLEXDAT *complex_arg, K_DATA *k_data, CSN_BINOP_MODE mode, bool compute) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    stamp_globals(csound, h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { out, source_arr }, 2);

    res = binop_hs_sh_body(csound, NULL, source_arr, complex_arg, mode);
    if (res != OK) goto done;

    const CSN_ARRAY *ops[1] = { source_arr };
    CSNARR_RES r = prepare_output(csound, h, NULL, reg, out, source_arr->ndim, source_arr->shape, source_arr->itype, ops, 1, CSN_ALIAS_ELEMENTWISE);
    res = r.err;
    if (res != OK) goto done;
    pres.locked |= r.locked;

    if (compute) {
        res = binop_hs_sh_assign_value(csound, NULL, scalar_arg, complex_arg, source_arr, out, mode);
        if (res != OK) goto done;
        if (r.aliased) update_array_data_version(&out->version);
    }

    SET_KDATA_BEGIN(*k_data, reg, source_arr->shape, source_arr->ndim, source_arr->itype);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

/* The array and the scalar are passed explicitly rather than read off a cast
   struct: CSN_BINOP_SH lists them in the opposite order to CSN_BINOP_HS, so a
   single cast would silently swap them. */
static int32_t csnarray_binop_hs_sh_helper(CSOUND *csound, CSN_BINOP_COMMON *p, CSN_ARRAY *source_arr, cs_float *scalar_arg, COMPLEXDAT *complex_arg, CSN_BINOP_MODE mode) {
    return csn_binop_hs_sh_init(csound, &p->h, p->handle, source_arr, scalar_arg, complex_arg, &p->k_data, mode, true);
}

/* The scalar is a k-argument, still at its init value here: the output only
   takes its layout, and the first pass computes. */
static int32_t csnarray_binop_hs_sh_k_init_helper(CSOUND *csound, CSN_BINOP_COMMON *p, CSN_ARRAY *source_arr, COMPLEXDAT *complex_arg, CSN_BINOP_MODE mode) {
    return csn_binop_hs_sh_init(csound, &p->h, p->handle, source_arr, NULL, complex_arg, &p->k_data, mode, false);
}

int32_t csn_binop_hs_sh_perf(CSOUND *csound, OPDS *h, CSN_ARRAY *out, CSN_ARRAY *source_arr, cs_float *scalar_arg, COMPLEXDAT *complex_arg, K_DATA *k_data, CSN_BINOP_MODE mode) {
    CSN_REGISTRY *reg = k_data->registry;
    CHECK_REGISTRY(csound, h, reg);

    int32_t res = OK;
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { out, source_arr }, 2);

    res = binop_hs_sh_body(csound, h, source_arr, complex_arg, mode);
    if (res != OK) goto done;

    /* The scalar parameter belongs to the reuse key just as much as the array:
       the complex forms carry two components, the real ones only the first. */
    double scalar_key = scalar_arg != NULL ? (double) *scalar_arg : 0.0;
    double scalar_key_im = 0.0;
    if (complex_arg != NULL) {
        complexdat_to_rect(complex_arg, &scalar_key, &scalar_key_im);
    }

    if (CAN_REUSE_ELEMENTWISE(k_data, source_arr, NULL, out, scalar_key, scalar_key_im)) goto done;

    const CSN_ARRAY *ops[1] = { source_arr };
    CSNARR_RES r = prepare_output(csound, h, h, reg, out, source_arr->ndim, source_arr->shape, source_arr->itype, ops, 1, CSN_ALIAS_ELEMENTWISE);
    res = r.err;
    if (res != OK) goto done;
    pres.locked |= r.locked;

    res = binop_hs_sh_assign_value(csound, h, scalar_arg, complex_arg, source_arr, out, mode);
    if (res != OK) goto done;
    if (r.aliased) update_array_data_version(&out->version);

    SET_KDATA_END(*k_data, source_arr->shape, source_arr->ndim, source_arr->itype);
    PUBLISH_ELEMENTWISE(k_data, source_arr, NULL, out, scalar_key, scalar_key_im);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

static int32_t csnarray_binop_hs_sh_k_helper(CSOUND *csound, CSN_BINOP_COMMON *p, CSN_ARRAY *source_arr, cs_float *scalar_arg, COMPLEXDAT *complex_arg, cs_float *trig, CSN_BINOP_MODE mode) {
    CHECK_KTRIG(trig);
    return csn_binop_hs_sh_perf(csound, &p->h, p->handle, source_arr, scalar_arg, complex_arg, &p->k_data, mode);
}

int32_t csnarray_add_hh(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_helper(csound, p, CSN_ADD_HH, false);
}

int32_t csnarray_add_hh_k_init(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_helper(csound, p, CSN_ADD_HH, true);
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
    return csnarray_binop_hh_helper(csound, p, CSN_SUB_HH, false);
}

int32_t csnarray_subtract_hh_k_init(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_helper(csound, p, CSN_SUB_HH, true);
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
    return csnarray_binop_hh_helper(csound, p, CSN_MUL_HH, false);
}

int32_t csnarray_mul_hh_k_init(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_helper(csound, p, CSN_MUL_HH, true);
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
    return csnarray_binop_hh_helper(csound, p, CSN_DIV_HH, false);
}

int32_t csnarray_div_hh_k_init(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_helper(csound, p, CSN_DIV_HH, true);
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
    return csnarray_binop_hh_helper(csound, p, CSN_POW_HH, false);
}

int32_t csnarray_pow_hh_k_init(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_helper(csound, p, CSN_POW_HH, true);
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
    return csnarray_binop_hh_helper(csound, p, CSN_LOG_HH, false);
}

int32_t csnarray_log_hh_k_init(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_helper(csound, p, CSN_LOG_HH, true);
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
    return csnarray_binop_hh_helper(csound, p, CSN_LOGICAL_AND_HH, false);
}

int32_t csnarray_logical_and_hh_k_init(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_helper(csound, p, CSN_LOGICAL_AND_HH, true);
}

int32_t csnarray_logical_and_hh_k(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_k_helper(csound, p, CSN_LOGICAL_AND_HH);
}

int32_t csnarray_logical_or_hh(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_helper(csound, p, CSN_LOGICAL_OR_HH, false);
}

int32_t csnarray_logical_or_hh_k_init(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_helper(csound, p, CSN_LOGICAL_OR_HH, true);
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
    return csnarray_binop_hh_helper(csound, p, CSN_HYPOT_HH, false);
}

int32_t csnarray_hypot_hs(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, CSN_HYPOT_HS);
}

int32_t csnarray_hypot_hh_k_init(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_helper(csound, p, CSN_HYPOT_HH, true);
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
    return csnarray_binop_hh_helper(csound, p, CSN_MINIMUM_HH, false);
}

int32_t csnarray_minimum_hs(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, CSN_MINIMUM_HS);
}

int32_t csnarray_minimum_hh_k_init(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_helper(csound, p, CSN_MINIMUM_HH, true);
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
    return csnarray_binop_hh_helper(csound, p, CSN_MAXIMUM_HH, false);
}

int32_t csnarray_maximum_hs(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, CSN_MAXIMUM_HS);
}

int32_t csnarray_maximum_hh_k_init(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_helper(csound, p, CSN_MAXIMUM_HH, true);
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
    return csnarray_binop_hh_helper(csound, p, CSN_ATAN2_HH, false);
}

int32_t csnarray_atan2_hh_k_init(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_helper(csound, p, CSN_ATAN2_HH, true);
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

static int32_t unaryop_body(CSOUND *csound, OPDS *perf_h, const CSN_ARRAY *source_arr, ITEM_TYPE *new_itype, CSN_UNARY_MODE mode) {
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
                arr->data[i] = mode == CSN_DEG2RAD ? a * CSN_PI / 180.0 : a * 180.0 / CSN_PI;
                break;
            default:
                break;
        }
    }
}

/* Serves the i-rate form and, as its init, the k form. Every function writes
   the cell it reads, so X = csnsqrt(X) runs in place; csnabs on a complex
   array changes the element type, which CSN_ALIAS_ELEMENTWISE refuses. */
static int32_t csnarray_unaryop_helper(CSOUND *csound, CSN_UNARYOP *p, CSN_UNARY_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle }, 2);

    CSN_ARRAY *source_arr = p->source_handle;
    ITEM_TYPE new_itype = CSN_REAL;
    res = unaryop_body(csound, NULL, source_arr, &new_itype, mode);
    if (res != OK) goto done;

    const CSN_ARRAY *ops[1] = { source_arr };
    CSNARR_RES out = prepare_output(csound, &p->h, NULL, reg, p->handle, source_arr->ndim, source_arr->shape, new_itype, ops, 1, CSN_ALIAS_ELEMENTWISE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    /* At k-rate an output that is its own source is computed by every perf
       pass; computing it here as well would apply the function twice in the
       first control cycle. */
    if (!(csn_is_k_form(&p->h) && out.aliased)) {
        unaryop_assign_value(source_arr, p->handle, source_arr->itype, mode);
        if (out.aliased) update_array_data_version(&p->handle->version);
    }
    SET_KDATA_BEGIN(p->k_data, reg, source_arr->shape, source_arr->ndim, new_itype);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

static int32_t csnarray_unaryop_k_helper(CSOUND *csound, CSN_UNARYOP *p, CSN_UNARY_MODE mode) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    CHECK_KTRIG(p->trig);

    int32_t res = OK;
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle }, 2);

    CSN_ARRAY *source_arr = p->source_handle;
    ITEM_TYPE new_itype = CSN_REAL;
    res = unaryop_body(csound, &p->h, source_arr, &new_itype, mode);
    if (res != OK) goto done;

    if (CAN_REUSE_ELEMENTWISE(&p->k_data, source_arr, NULL, p->handle, 0.0, 0.0)) goto done;

    const CSN_ARRAY *ops[1] = { source_arr };
    CSNARR_RES out = prepare_output(csound, &p->h, &p->h, reg, p->handle, source_arr->ndim, source_arr->shape, new_itype, ops, 1, CSN_ALIAS_ELEMENTWISE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    unaryop_assign_value(source_arr, p->handle, source_arr->itype, mode);
    if (out.aliased) update_array_data_version(&p->handle->version);

    SET_KDATA_END(p->k_data, source_arr->shape, source_arr->ndim, new_itype);
    PUBLISH_ELEMENTWISE(&p->k_data, source_arr, NULL, p->handle, 0.0, 0.0);

done:
    if (pres.locked) csn_unlock(csound, reg);
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

static int32_t complop_check(CSOUND *csound, OPDS *perf_h, const CSN_ARRAY *source_arr, CSN_COMPLEXOP_MODE mode) {
    if (mode == CSN_REAL_TO_COMPLEX) {
        if (source_arr->itype != CSN_REAL) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Real-to-complex operation requires real array");
        }
    } else if (source_arr->itype != CSN_COMPLEX) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Operation requires complex array");
    }
    return OK;
}

/* csnconj writes each cell from itself and stays complex, so it may run in
   place. csnreal, csnimag and csntocomplex change the element type, so their
   output can never be the source. */
static inline ITEM_TYPE complop_out_itype(CSN_COMPLEXOP_MODE mode) {
    return (mode == CSN_CONJ_PART || mode == CSN_REAL_TO_COMPLEX) ? CSN_COMPLEX : CSN_REAL;
}

static inline CSN_ALIAS_POLICY complop_alias_policy(CSN_COMPLEXOP_MODE mode) {
    return mode == CSN_CONJ_PART ? CSN_ALIAS_ELEMENTWISE : CSN_ALIAS_REFUSE;
}

/* Serves the i-rate form and the k form's init. */
static int32_t csnarray_complop_unary_helper(CSOUND *csound, CSN_UNARYOP *p, CSN_COMPLEXOP_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle }, 2);

    CSN_ARRAY *source_arr = p->source_handle;
    res = complop_check(csound, NULL, source_arr, mode);
    if (res != OK) goto done;

    ITEM_TYPE out_itype = complop_out_itype(mode);
    const CSN_ARRAY *ops[1] = { source_arr };
    CSNARR_RES out = prepare_output(csound, &p->h, NULL, reg, p->handle, source_arr->ndim, source_arr->shape, out_itype, ops, 1, complop_alias_policy(mode));
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    /* the k form's perf pass conjugates an output that is its own source */
    if (!(csn_is_k_form(&p->h) && out.aliased)) {
        complop_unary_assign_value(source_arr, p->handle, mode);
        if (out.aliased) update_array_data_version(&p->handle->version);
    }
    SET_KDATA_BEGIN(p->k_data, reg, source_arr->shape, source_arr->ndim, out_itype);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

static int32_t csnarray_complop_unary_k_init_helper(CSOUND *csound, CSN_UNARYOP *p, CSN_COMPLEXOP_MODE mode) {
    return csnarray_complop_unary_helper(csound, p, mode);
}

static int32_t csnarray_complop_unary_k_helper(CSOUND *csound, CSN_UNARYOP *p, CSN_COMPLEXOP_MODE mode) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    CHECK_KTRIG(p->trig);

    int32_t res = OK;
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle }, 2);

    CSN_ARRAY *source_arr = p->source_handle;
    res = complop_check(csound, &p->h, source_arr, mode);
    if (res != OK) goto done;

    if (CAN_REUSE_ELEMENTWISE(&p->k_data, source_arr, NULL, p->handle, 0.0, 0.0)) goto done;

    ITEM_TYPE out_itype = complop_out_itype(mode);
    const CSN_ARRAY *ops[1] = { source_arr };
    CSNARR_RES out = prepare_output(csound, &p->h, &p->h, reg, p->handle, source_arr->ndim, source_arr->shape, out_itype, ops, 1, complop_alias_policy(mode));
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    complop_unary_assign_value(source_arr, p->handle, mode);
    if (out.aliased) update_array_data_version(&p->handle->version);

    SET_KDATA_END(p->k_data, source_arr->shape, source_arr->ndim, out_itype);
    PUBLISH_ELEMENTWISE(&p->k_data, source_arr, NULL, p->handle, 0.0, 0.0);

done:
    if (pres.locked) csn_unlock(csound, reg);
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

/* The previous source value is kept aside rather than read back from src, so
   dst may be src: an in-place unwrap would otherwise take each difference
   against the element it has just rewritten. */
static void unwrap_slice(double *dst, const double *src, size_t n, size_t stride, double period, double discont) {
    if (n == 0) return;
    double prev = src[0];
    dst[0] = prev;
    for (size_t i = 1; i < n; ++i) {
        double cur = src[i * stride];
        double delta = cur - prev;
        double wrapped = wrap_angle(delta, period);
        double correction = fabs(delta) < discont ? 0.0 : wrapped - delta;
        dst[i * stride] = dst[(i - 1) * stride] + delta + correction;
        prev = cur;
    }
}

static int32_t angle_body(CSOUND *csound, OPDS *perf_h, const CSN_ARRAY *source_arr, double in_period, double in_discount, const cs_float *axis_in, double *out_period, double *out_discount, int32_t *out_axis, CSN_COMPLEXOP_MODE mode) {
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

/* csnangle turns complex into real, so its output is never the source.
   csnwrap folds each angle in its own cell. csnunwrap walks an axis, but
   unwrap_slice keeps the previous source value aside, so it is safe in place
   too, and its layout never changes. */
static inline CSN_ALIAS_POLICY angle_alias_policy(CSN_COMPLEXOP_MODE mode) {
    switch (mode) {
        case CSN_WRAP: return CSN_ALIAS_ELEMENTWISE;
        case CSN_UNWRAP: return CSN_ALIAS_CUSTOM;
        default: return CSN_ALIAS_REFUSE;
    }
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
            CSN_AXIS_SLICE_ITER it;
            if (AXIS_ITER_SLICE_INIT(&it, source_arr, arr, (uint32_t) axis) != OK) {
                return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Incompatible array shapes/ranks");
            }
            while (AXIS_SLICE_ITER_NEXT(&it)) {
                unwrap_slice(arr->data + it.dst_base, source_arr->data + it.src_base, it.axis_size, it.src_axis_stride, period, discount);
            }
        }
    }

    return OK;
}

static int32_t csnarray_angle_helper(CSOUND *csound, CSN_ANGLE *p, CSN_COMPLEXOP_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle }, 2);

    CSN_ARRAY *source_arr = p->source_handle;
    double in_period = (mode == CSN_WRAP || mode == CSN_UNWRAP) ? (double) *p->arg_a : 0.0;
    double in_discount = mode == CSN_UNWRAP ? (double) *p->arg_b : 0.0;
    const cs_float *axis_in = mode == CSN_UNWRAP && p->INOCOUNT > 3 ? p->arg_c : NULL;
    double period = 0.0;
    double discount = 0.0;
    int32_t axis = -1;
    res = angle_body(csound, NULL, source_arr, in_period, in_discount, axis_in, &period, &discount, &axis, mode);
    if (res != OK) goto done;

    const CSN_ARRAY *ops[1] = { source_arr };
    CSNARR_RES out = prepare_output(csound, &p->h, NULL, reg, p->handle, source_arr->ndim, source_arr->shape, CSN_REAL, ops, 1, angle_alias_policy(mode));
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    res = angle_assign_value(csound, NULL, source_arr, p->handle, period, discount, axis, mode);
    if (res != OK) goto done;
    if (out.aliased) update_array_data_version(&p->handle->version);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

/* The period, the discontinuity and the axis are k-arguments, still at their
   init values here: the output only takes its layout, the first pass computes. */
static int32_t csnarray_angle_k_init_helper(CSOUND *csound, CSN_ANGLE *p, CSN_COMPLEXOP_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle }, 2);

    CSN_ARRAY *source_arr = p->source_handle;
    double in_period = (mode == CSN_WRAP || mode == CSN_UNWRAP) ? (double) *p->arg_a : 0.0;
    double in_discount = mode == CSN_UNWRAP ? (double) *p->arg_b : 0.0;
    const cs_float *axis_in = mode == CSN_UNWRAP && p->INOCOUNT > 4 ? p->arg_d : NULL;
    double period = 0.0;
    double discount = 0.0;
    int32_t axis = -1;
    res = angle_body(csound, NULL, source_arr, in_period, in_discount, axis_in, &period, &discount, &axis, mode);
    if (res != OK) goto done;

    const CSN_ARRAY *ops[1] = { source_arr };
    CSNARR_RES out = prepare_output(csound, &p->h, NULL, reg, p->handle, source_arr->ndim, source_arr->shape, CSN_REAL, ops, 1, angle_alias_policy(mode));
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    SET_KDATA_BEGIN(p->k_data, reg, source_arr->shape, source_arr->ndim, CSN_REAL);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

static int32_t csnarray_angle_k_helper(CSOUND *csound, CSN_ANGLE *p, CSN_COMPLEXOP_MODE mode) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    cs_float *trig = (mode == CSN_WRAP || mode == CSN_UNWRAP) ? ((mode == CSN_WRAP) ? p->arg_b : p->arg_c) : p->arg_a;
    CHECK_KTRIG(trig);

    int32_t res = OK;
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle }, 2);

    CSN_ARRAY *source_arr = p->source_handle;
    double in_period = (mode == CSN_WRAP || mode == CSN_UNWRAP) ? (double) *p->arg_a : 0.0;
    double in_discount = mode == CSN_UNWRAP ? (double) *p->arg_b : 0.0;
    const cs_float *axis_in = mode == CSN_UNWRAP && p->INOCOUNT > 4 ? p->arg_d : NULL;
    double period = 0.0;
    double discount = 0.0;
    int32_t axis = -1;
    res = angle_body(csound, &p->h, source_arr, in_period, in_discount, axis_in, &period, &discount, &axis, mode);
    if (res != OK) goto done;

    if (CAN_REUSE_ELEMENTWISE(&p->k_data, source_arr, NULL, p->handle, period, discount) && p->k_data.prev_axis_i == axis) goto done;

    const CSN_ARRAY *ops[1] = { source_arr };
    CSNARR_RES out = prepare_output(csound, &p->h, &p->h, reg, p->handle, source_arr->ndim, source_arr->shape, CSN_REAL, ops, 1, angle_alias_policy(mode));
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    res = angle_assign_value(csound, &p->h, source_arr, p->handle, period, discount, axis, mode);
    if (res != OK) goto done;
    if (out.aliased) update_array_data_version(&p->handle->version);

    SET_KDATA_END(p->k_data, source_arr->shape, source_arr->ndim, CSN_REAL);
    PUBLISH_ELEMENTWISE(&p->k_data, source_arr, NULL, p->handle, period, discount);
    p->k_data.prev_axis_i = axis;

done:
    if (pres.locked) csn_unlock(csound, reg);
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
            CSN_AXIS_SLICE_ITER it;
            if (AXIS_ITER_SLICE_INIT(&it, source_arr, source_arr, (uint32_t) axis) != OK) {
                return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Incompatible array shapes/ranks");
            }
            while (AXIS_SLICE_ITER_NEXT(&it)) {
                unwrap_slice(source_arr->data + it.dst_base, source_arr->data + it.src_base, it.axis_size, it.src_axis_stride, period, discount);
            }
        }
    }

    return OK;
}

static int32_t csnarray_angle_in_helper(CSOUND *csound, CSN_ANGLE_IN *p, const cs_float *axis_in, CSN_COMPLEXOP_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);

    CSN_ARRAY *source_arr = p->source_handle;
    double in_period = (mode == CSN_WRAP || mode == CSN_UNWRAP) ? (double) *p->arg_a : 0.0;
    double in_discount = mode == CSN_UNWRAP ? (double) *p->arg_b : 0.0;
    double period = 0.0;
    double discount = 0.0;
    int32_t axis = -1;
    res = angle_body(csound, NULL, source_arr, in_period, in_discount, axis_in, &period, &discount, &axis, mode);
    if (res != OK) goto done;

    res = angle_in_assign_value(csound, NULL, source_arr, period, discount, axis, mode);
    if (res != OK) goto done;
    update_array_data_version(&source_arr->version);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

/* The in-place k forms do nothing at init: their arguments are k-rate, and the
   first pass applies the operation once. */
static int32_t csnarray_angle_in_k_init_helper(CSOUND *csound, CSN_ANGLE_IN *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    csn_mark_krate(p->source_handle);
    p->registry = reg;
    stamp_globals(csound, &p->h);
    return OK;
}

static int32_t csnarray_angle_in_k_helper(CSOUND *csound, CSN_ANGLE_IN *p, CSN_COMPLEXOP_MODE mode) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    cs_float *trig = (mode == CSN_WRAP || mode == CSN_UNWRAP) ? ((mode == CSN_WRAP) ? p->arg_b : p->arg_c) : p->arg_a;
    CHECK_KTRIG(trig);

    int32_t res = OK;
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);

    CSN_ARRAY *source_arr = p->source_handle;
    double in_period = (mode == CSN_WRAP || mode == CSN_UNWRAP) ? (double) *p->arg_a : 0.0;
    double in_discount = mode == CSN_UNWRAP ? (double) *p->arg_b : 0.0;
    const cs_float *axis_in = mode == CSN_UNWRAP && p->INOCOUNT > 4 ? p->arg_d : NULL;
    double period = 0.0;
    double discount = 0.0;
    int32_t axis = -1;
    res = angle_body(csound, &p->h, source_arr, in_period, in_discount, axis_in, &period, &discount, &axis, mode);
    if (res != OK) goto done;

    if (CAN_REUSE_ELEMENTWISE(&p->k_data, source_arr, NULL, NULL, period, discount) && p->k_data.prev_axis_i == axis) goto done;

    res = angle_in_assign_value(csound, &p->h, source_arr, period, discount, axis, mode);
    if (res != OK) goto done;
    update_array_data_version(&source_arr->version);
    PUBLISH_ELEMENTWISE(&p->k_data, source_arr, NULL, NULL, period, discount);
    p->k_data.prev_axis_i = axis;

done:
    if (pres.locked) csn_unlock(csound, reg);
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

int32_t csnarray_wrap_angle_in_k_init(CSOUND *csound, CSN_ANGLE_IN *p) {
    return csnarray_angle_in_k_init_helper(csound, p);
}

int32_t csnarray_wrap_angle_in_k(CSOUND *csound, CSN_ANGLE_IN *p) {
    return csnarray_angle_in_k_helper(csound, p, CSN_WRAP);
}

int32_t csnarray_unwrap_angle_in(CSOUND *csound, CSN_ANGLE_IN *p) {
    const cs_float *axis_in = p->INOCOUNT > 3 ? p->arg_c : NULL;
    return csnarray_angle_in_helper(csound, p, axis_in, CSN_UNWRAP);
}

int32_t csnarray_unwrap_angle_in_k_init(CSOUND *csound, CSN_ANGLE_IN *p) {
    return csnarray_angle_in_k_init_helper(csound, p);
}

int32_t csnarray_unwrap_angle_in_k(CSOUND *csound, CSN_ANGLE_IN *p) {
    return csnarray_angle_in_k_helper(csound, p, CSN_UNWRAP);
}

static int32_t csnarray_type_helper(CSOUND *csound, OPDS *perf_h, CSN_UNARYOP_SCALAR *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, perf_h, reg);

    if (perf_h != NULL) {
        CHECK_KTRIG(p->trig);
    } else {
        stamp_globals(csound, &p->h);
    }
    p->registry = reg;

    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);
    *p->value = p->source_handle->itype == CSN_REAL ? FL(0.0) : FL(1.0);
    if (pres.locked) csn_unlock(csound, reg);
    return OK;
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

static void reverse_in_place(CSN_ARRAY *arr) {
    size_t itype = (size_t) arr->itype;
    for (size_t i = 0; i < arr->size / 2U; i++) {
        size_t lo = i * itype;
        size_t hi = (arr->size - 1U - i) * itype;
        double temp_re = arr->data[hi];
        arr->data[hi] = arr->data[lo];
        arr->data[lo] = temp_re;
        if (itype == CSN_COMPLEX) {
            double temp_im = arr->data[hi + 1U];
            arr->data[hi + 1U] = arr->data[lo + 1U];
            arr->data[lo + 1U] = temp_im;
        }
    }
}

/* csncopy onto itself has nothing to move (ELEMENTWISE). csnreverse reads the
   far end of the array, so onto itself it swaps the halves instead (CUSTOM:
   the layout is the source's either way). */
static void copy_into(CSN_ARRAY *arr, const CSN_ARRAY *source_arr, bool aliased, bool reverse) {
    if (!aliased) {
        copy_assign_value(source_arr, arr, reverse, source_arr->itype);
    } else if (reverse) {
        reverse_in_place(arr);
        update_array_data_version(&arr->version);
    }
}

/* Serves the i-rate form and the k form's init. */
static int32_t csnarray_copy_helper(CSOUND *csound, CSN_UNARYOP *p, bool reverse) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle }, 2);

    CSN_ARRAY *source_arr = p->source_handle;
    const CSN_ARRAY *ops[1] = { source_arr };
    CSNARR_RES out = prepare_output(csound, &p->h, NULL, reg, p->handle, source_arr->ndim, source_arr->shape, source_arr->itype, ops, 1, reverse ? CSN_ALIAS_CUSTOM : CSN_ALIAS_ELEMENTWISE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    /* the k form's perf pass reverses an output that is its own source */
    if (!(csn_is_k_form(&p->h) && out.aliased)) {
        copy_into(p->handle, source_arr, out.aliased, reverse);
    }
    SET_KDATA_BEGIN(p->k_data, reg, source_arr->shape, source_arr->ndim, source_arr->itype);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

static int32_t csnarray_copy_k_init_helper(CSOUND *csound, CSN_UNARYOP *p, bool reverse) {
    return csnarray_copy_helper(csound, p, reverse);
}

static int32_t csnarray_copy_k_helper(CSOUND *csound, CSN_UNARYOP *p, bool reverse) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    CHECK_KTRIG(p->trig);

    int32_t res = OK;
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle }, 2);

    CSN_ARRAY *source_arr = p->source_handle;
    if (CAN_REUSE_ELEMENTWISE(&p->k_data, source_arr, NULL, p->handle, 0.0, 0.0)) goto done;

    const CSN_ARRAY *ops[1] = { source_arr };
    CSNARR_RES out = prepare_output(csound, &p->h, &p->h, reg, p->handle, source_arr->ndim, source_arr->shape, source_arr->itype, ops, 1, reverse ? CSN_ALIAS_CUSTOM : CSN_ALIAS_ELEMENTWISE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    copy_into(p->handle, source_arr, out.aliased, reverse);

    SET_KDATA_END(p->k_data, source_arr->shape, source_arr->ndim, source_arr->itype);
    PUBLISH_ELEMENTWISE(&p->k_data, source_arr, NULL, p->handle, 0.0, 0.0);

done:
    if (pres.locked) csn_unlock(csound, reg);
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

static void unaryop_in_apply(CSN_ARRAY *arr, CSN_UNARY_MODE mode) {
    switch (mode) {
        case CSN_REVERSE:
            reverse_in_place(arr);
            break;
        case CSN_RAD2DEG:
        case CSN_DEG2RAD:
            for (size_t i = 0; i < arr->size; i++) {
                double value = arr->data[i];
                arr->data[i] = mode == CSN_DEG2RAD ? value * CSN_PI / 180.0 : value * 180.0 / CSN_PI;
            }
            break;
        default:
            break;
    }
}

static int32_t csnarray_unaryop_in_helper(CSOUND *csound, CSN_UNARYOP_IN *p, CSN_UNARY_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    p->k_data.registry = reg;
    int32_t res = OK;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);

    CSN_ARRAY *source_arr = p->source_handle;
    if (source_arr->itype == CSN_COMPLEX && (mode == CSN_DEG2RAD || mode == CSN_RAD2DEG)) {
        res = csound->InitError(csound, "[csnarray] Angle conversion requires real arrays");
        goto done;
    }

    unaryop_in_apply(source_arr, mode);
    if (source_arr->size >= 1U) update_array_data_version(&source_arr->version);

done:
    if (pres.locked) csn_unlock(csound, reg);
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
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    CHECK_KTRIG(p->trig);

    int32_t res = OK;
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);

    CSN_ARRAY *source_arr = p->source_handle;
    if (source_arr->itype == CSN_COMPLEX && (mode == CSN_DEG2RAD || mode == CSN_RAD2DEG)) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Angle conversion requires real arrays");
        goto done;
    }

    /* Not idempotent: reversing twice undoes it and a second conversion scales
       again, so a pass must run exactly once per write by someone else. */
    if (CAN_REUSE_ELEMENTWISE(&p->k_data, source_arr, NULL, NULL, 0.0, 0.0)) goto done;

    unaryop_in_apply(source_arr, mode);
    if (source_arr->size >= 1U) update_array_data_version(&source_arr->version);
    PUBLISH_ELEMENTWISE(&p->k_data, source_arr, NULL, NULL, 0.0, 0.0);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_unaryop_in_k_init(CSOUND *csound, CSN_UNARYOP_IN *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    csn_mark_krate(p->source_handle);

    p->k_data.registry = reg;
    stamp_globals(csound, &p->h);
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


/* Both outputs are refused as operands (the quotient is written before the
   remainder reads the same cell) and as each other: one variable cannot hold
   two results. */
static int32_t divmod_prepare_outputs(CSOUND *csound, OPDS *h, OPDS *perf_h, CSN_REGISTRY *reg, CSN_ARRAY *q_arr, CSN_ARRAY *r_arr, uint32_t ndim, const uint32_t *shape, const CSN_ARRAY *const *ops, size_t n_ops, bool *locked) {
    CSNARR_RES out_q = prepare_output(csound, h, perf_h, reg, q_arr, ndim, shape, CSN_REAL, ops, n_ops, CSN_ALIAS_REFUSE);
    *locked |= out_q.locked;
    if (out_q.err != OK) return out_q.err;
    CSNARR_RES out_r = prepare_output(csound, h, perf_h, reg, r_arr, ndim, shape, CSN_REAL, ops, n_ops, CSN_ALIAS_REFUSE);
    *locked |= out_r.locked;
    return out_r.err;
}

static int32_t divmod_hh_compute(CSOUND *csound, OPDS *perf_h, CSN_BROADCAST_ITER *it, const CSN_ARRAY *a_arr, const CSN_ARRAY *b_arr, CSN_ARRAY *q_arr, CSN_ARRAY *r_arr) {
    while (BROADCAST_ITER_NEXT(it)) {
        double a = a_arr->data[it->offsets[0]];
        double b = b_arr->data[it->offsets[1]];
        if (b == 0.0) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Division by zero");
        }
        double q = floor(a / b);
        q_arr->data[it->linear_index] = q;
        r_arr->data[it->linear_index] = a - q * b;
    }
    return OK;
}

/* Serves the i-rate form, and with is_k_init the k form's init, which only
   lays the outputs out: the first pass computes. */
static int32_t csnarray_divmod_hh_helper(CSOUND *csound, CSN_DIVMOD_HH *p, bool is_k_init) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle_a, p->handle_b, p->source_handle_a, p->source_handle_b }, 4);

    CSN_ARRAY *source_arr_a = p->source_handle_a;
    CSN_ARRAY *source_arr_b = p->source_handle_b;
    if (source_arr_a->itype == CSN_COMPLEX || source_arr_b->itype == CSN_COMPLEX) {
        res = csound->InitError(csound, "[csnarray] Divmod operation requires real array");
        goto done;
    }

    res = csn_check_outputs_distinct(csound, NULL, (CSN_CONARR) { p->handle_a, p->handle_b }, 2);
    if (res != OK) goto done;

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    uint32_t new_ndim = 0;
    size_t new_size = 0;
    size_t broad_strides[CSN_MAX_BROADCAST_INPUTS][CSN_MAX_DIMS] = {{0}};
    const CSN_ARRAY *sources[2] = { source_arr_a, source_arr_b };

    if (BROADCAST_INIT(&new_ndim, new_shape, &new_size, broad_strides, sources, 2) != OK) {
        char abuf[CSN_SHAPE_STR_MAX], bbuf[CSN_SHAPE_STR_MAX];
        res = csound->InitError(csound, "[csnarray] Shapes %s and %s cannot be broadcast together: aligned from the last axis, each pair must match or be 1", shape_str(abuf, sizeof(abuf), source_arr_a->shape, source_arr_a->ndim), shape_str(bbuf, sizeof(bbuf), source_arr_b->shape, source_arr_b->ndim));
        goto done;
    }
    CSN_BROADCAST_ITER it;
    if (BROADCAST_ITER_INIT(&it, new_ndim, new_shape, new_size, broad_strides, 2) != OK) {
        res = csound->InitError(csound, "[csnarray] Invalid broadcast iterator layout");
        goto done;
    }

    res = divmod_prepare_outputs(csound, &p->h, NULL, reg, p->handle_a, p->handle_b, new_ndim, new_shape, sources, 2, &pres.locked);
    if (res != OK) goto done;

    if (is_k_init) {
        reset_empty_csnarray(p->handle_a, new_ndim, new_shape, CSN_REAL);
        reset_empty_csnarray(p->handle_b, new_ndim, new_shape, CSN_REAL);
    } else {
        res = divmod_hh_compute(csound, NULL, &it, source_arr_a, source_arr_b, p->handle_a, p->handle_b);
        if (res != OK) goto done;
    }

    SET_KDATA_BEGIN(p->k_data, reg, new_shape, new_ndim, CSN_REAL);
    p->is_published = false;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

static int32_t divmod_scalar_compute(CSOUND *csound, OPDS *perf_h, const CSN_ARRAY *source_arr, CSN_ARRAY *q_arr, CSN_ARRAY *r_arr, double scalar_value, bool is_left) {
    for (size_t i = 0; i < source_arr->size; i++) {
        double a = source_arr->data[i];
        double num = is_left ? scalar_value : a;
        double den = is_left ? a : scalar_value;
        if (den == 0.0) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Division by zero");
        }
        double q = floor(num / den);
        q_arr->data[i] = q;
        r_arr->data[i] = num - q * den;
    }
    return OK;
}

/* Serves the i-rate forms, and with is_k_init the k forms' init, where the
   scalar is a k-argument still at its init value: the outputs are only laid
   out, and the first pass computes. */
static int32_t csnarray_divmod_hs_sh_helper(CSOUND *csound, OPDS *h, CSN_ARRAY *q_arr, CSN_ARRAY *r_arr, CSN_ARRAY *source_arr, const cs_float *scalar, bool is_left, K_DATA *k_data, bool *is_published, bool is_k_init) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    double scalar_value = (double) *scalar;
    if (!is_k_init) {
        if (!IS_VALID_VALUE(scalar_value)) {
            return csound->InitError(csound, "[csnarray] Invalid scalar value");
        }
        if (!is_left && scalar_value == 0.0) {
            return csound->InitError(csound, "[csnarray] Division by zero");
        }
    }

    int32_t res = OK;
    stamp_globals(csound, h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { q_arr, r_arr, source_arr }, 3);

    if (source_arr->itype == CSN_COMPLEX) {
        res = csound->InitError(csound, "[csnarray] Divmod operation requires real array");
        goto done;
    }

    res = csn_check_outputs_distinct(csound, NULL, (CSN_CONARR) { q_arr, r_arr }, 2);
    if (res != OK) goto done;

    const CSN_ARRAY *ops[1] = { source_arr };
    res = divmod_prepare_outputs(csound, h, NULL, reg, q_arr, r_arr, source_arr->ndim, source_arr->shape, ops, 1, &pres.locked);
    if (res != OK) goto done;

    if (is_k_init) {
        reset_empty_csnarray(q_arr, source_arr->ndim, source_arr->shape, CSN_REAL);
        reset_empty_csnarray(r_arr, source_arr->ndim, source_arr->shape, CSN_REAL);
    } else {
        res = divmod_scalar_compute(csound, NULL, source_arr, q_arr, r_arr, scalar_value, is_left);
        if (res != OK) goto done;
    }

    SET_KDATA_BEGIN(*k_data, reg, source_arr->shape, source_arr->ndim, CSN_REAL);
    *is_published = false;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_divmod_hh(CSOUND *csound, CSN_DIVMOD_HH *p) {
    return csnarray_divmod_hh_helper(csound, p, false);
}

int32_t csnarray_divmod_hs(CSOUND *csound, CSN_DIVMOD_HS *p) {
    return csnarray_divmod_hs_sh_helper(csound, &p->h, p->handle_a, p->handle_b, p->source_handle, p->scalar, false, &p->k_data, &p->is_published, false);
}

int32_t csnarray_divmod_sh(CSOUND *csound, CSN_DIVMOD_SH *p) {
    return csnarray_divmod_hs_sh_helper(csound, &p->h, p->handle_a, p->handle_b, p->source_handle, p->scalar, true, &p->k_data, &p->is_published, false);
}

int32_t csnarray_divmod_hh_k_init(CSOUND *csound, CSN_DIVMOD_HH *p) {
    return csnarray_divmod_hh_helper(csound, p, true);
}

/* Nothing moved: both sources and both outputs still carry the versions this
   opcode recorded after its last write. */
static bool divmod_can_reuse(const CSN_DIVMOD_K_STATE *st, const CSN_ARRAY *a, const CSN_ARRAY *b, const CSN_ARRAY *q, const CSN_ARRAY *r) {
    return is_same_array_version(&a->version, &st->prev_source_a_version)
        && (b == NULL || is_same_array_version(&b->version, &st->prev_source_b_version))
        && is_same_array_version(&q->version, &st->prev_q_version)
        && is_same_array_version(&r->version, &st->prev_r_version);
}

static void divmod_publish(CSN_DIVMOD_K_STATE *st, const CSN_ARRAY *a, const CSN_ARRAY *b, const CSN_ARRAY *q, const CSN_ARRAY *r) {
    set_array_version(&st->prev_source_a_version, &a->version);
    if (b != NULL) set_array_version(&st->prev_source_b_version, &b->version);
    set_array_version(&st->prev_q_version, &q->version);
    set_array_version(&st->prev_r_version, &r->version);
}

static int32_t csnarray_divmod_hh_k_helper(CSOUND *csound, CSN_DIVMOD_HH *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    CHECK_KTRIG(p->trig);

    int32_t res = OK;
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle_a, p->handle_b, p->source_handle_a, p->source_handle_b }, 4);

    CSN_ARRAY *source_arr_a = p->source_handle_a;
    CSN_ARRAY *source_arr_b = p->source_handle_b;
    if (source_arr_a->itype == CSN_COMPLEX || source_arr_b->itype == CSN_COMPLEX) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Divmod operation requires real array");
        goto done;
    }

    if (p->is_published && divmod_can_reuse(&p->k_data.prev_divmod_state, source_arr_a, source_arr_b, p->handle_a, p->handle_b)) goto done;

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    uint32_t new_ndim = 0;
    size_t new_size = 0;
    size_t broad_strides[CSN_MAX_BROADCAST_INPUTS][CSN_MAX_DIMS] = {{0}};
    const CSN_ARRAY *sources[2] = { source_arr_a, source_arr_b };

    if (BROADCAST_INIT(&new_ndim, new_shape, &new_size, broad_strides, sources, 2) != OK) {
        char abuf[CSN_SHAPE_STR_MAX], bbuf[CSN_SHAPE_STR_MAX];
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Shapes %s and %s cannot be broadcast together: aligned from the last axis, each pair must match or be 1", shape_str(abuf, sizeof(abuf), source_arr_a->shape, source_arr_a->ndim), shape_str(bbuf, sizeof(bbuf), source_arr_b->shape, source_arr_b->ndim));
        goto done;
    }
    CSN_BROADCAST_ITER it;
    if (BROADCAST_ITER_INIT(&it, new_ndim, new_shape, new_size, broad_strides, 2) != OK) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Invalid broadcast iterator layout");
        goto done;
    }

    res = divmod_prepare_outputs(csound, &p->h, &p->h, reg, p->handle_a, p->handle_b, new_ndim, new_shape, sources, 2, &pres.locked);
    if (res != OK) goto done;

    res = divmod_hh_compute(csound, &p->h, &it, source_arr_a, source_arr_b, p->handle_a, p->handle_b);
    if (res != OK) goto done;

    SET_KDATA_END(p->k_data, new_shape, new_ndim, CSN_REAL);
    divmod_publish(&p->k_data.prev_divmod_state, source_arr_a, source_arr_b, p->handle_a, p->handle_b);
    p->is_published = true;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

static int32_t csnarray_divmod_hs_sh_k_helper(CSOUND *csound, OPDS *h, CSN_ARRAY *q_arr, CSN_ARRAY *r_arr, CSN_ARRAY *source_arr, const cs_float *scalar, K_DATA *k_data, bool is_left, bool *is_published) {
    CSN_REGISTRY *reg = k_data->registry;
    CHECK_REGISTRY(csound, h, reg);

    double scalar_value = (double) *scalar;
    if (!IS_VALID_VALUE(scalar_value)) {
        return csound->PerfError(csound, h, "[csnarray] Invalid scalar value");
    }
    if (!is_left && scalar_value == 0.0) {
        return csound->PerfError(csound, h, "[csnarray] Division by zero");
    }

    int32_t res = OK;
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { q_arr, r_arr, source_arr }, 3);

    if (source_arr->itype == CSN_COMPLEX) {
        res = csn_locked_perf_error(csound, h, "[csnarray] Divmod operation requires real array");
        goto done;
    }

    if (*is_published && scalar_value == k_data->prev_scalar_param
        && divmod_can_reuse(&k_data->prev_divmod_state, source_arr, NULL, q_arr, r_arr)) goto done;

    const CSN_ARRAY *ops[1] = { source_arr };
    res = divmod_prepare_outputs(csound, h, h, reg, q_arr, r_arr, source_arr->ndim, source_arr->shape, ops, 1, &pres.locked);
    if (res != OK) goto done;

    res = divmod_scalar_compute(csound, h, source_arr, q_arr, r_arr, scalar_value, is_left);
    if (res != OK) goto done;

    SET_KDATA_END(*k_data, source_arr->shape, source_arr->ndim, CSN_REAL);
    divmod_publish(&k_data->prev_divmod_state, source_arr, NULL, q_arr, r_arr);
    k_data->prev_scalar_param = scalar_value;
    *is_published = true;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_divmod_hh_k(CSOUND *csound, CSN_DIVMOD_HH *p) {
    return csnarray_divmod_hh_k_helper(csound, p);
}

int32_t csnarray_divmod_hs_k_init(CSOUND *csound, CSN_DIVMOD_HS *p) {
    return csnarray_divmod_hs_sh_helper(csound, &p->h, p->handle_a, p->handle_b, p->source_handle, p->scalar, false, &p->k_data, &p->is_published, true);
}

int32_t csnarray_divmod_hs_k(CSOUND *csound, CSN_DIVMOD_HS *p) {
    return csnarray_divmod_hs_sh_k_helper(csound, &p->h, p->handle_a, p->handle_b, p->source_handle, p->scalar, &p->k_data, false, &p->is_published);
}

int32_t csnarray_divmod_sh_k_init(CSOUND *csound, CSN_DIVMOD_SH *p) {
    return csnarray_divmod_hs_sh_helper(csound, &p->h, p->handle_a, p->handle_b, p->source_handle, p->scalar, true, &p->k_data, &p->is_published, true);
}

int32_t csnarray_divmod_sh_k(CSOUND *csound, CSN_DIVMOD_SH *p) {
    return csnarray_divmod_hs_sh_k_helper(csound, &p->h, p->handle_a, p->handle_b, p->source_handle, p->scalar, &p->k_data, true, &p->is_published);
}

/* The scalar complex arithmetic behind the :Complex; overloads of csnadd,
   csnsubtract, csnmul, csndiv, csnpow and csnsqrt. One operand may be a real
   number, in which case it enters as a complex with a zero imaginary part —
   except under multiplication, where scaling is cheaper than a full product.

   perf_h is NULL on the init pass and the opcode's OPDS at perf time, so a
   degenerate operand is reported at the rate it is found at, and reported at
   all: division by zero and a zero base under a power have no value, exactly
   as they have none in the array forms. */
static int32_t nonarr_complex_ops(CSOUND *csound, OPDS *perf_h, const COMPLEXDAT *ca, const COMPLEXDAT *cb, const cs_float *ra, const cs_float *rb, COMPLEXDAT *out, CSNCOMPLEX_OPS_MODE mode) {
    CSN_COMPLEXDAT result = {0};
    CSN_COMPLEXDAT a = {0};
    CSN_COMPLEXDAT b = {0};
    double real_value = 0.0;
    if (ca != NULL && cb != NULL) {
        complexdat_to_rect(ca, &a.re, &a.im);
        complexdat_to_rect(cb, &b.re, &b.im);
    } else {
        /* Exactly one side is complex here, and the other is the real operand. */
        real_value = ca == NULL ? (double) *ra : (double) *rb;
        if (ca != NULL) {
            complexdat_to_rect(ca, &a.re, &a.im);
            b.re = real_value;
        } else {
            complexdat_to_rect(cb, &b.re, &b.im);
            a.re = real_value;
        }
    }
    switch (mode) {
        case CSNCOMP_ADD:
            complex_add(&result, a, b);
            break;
        case CSNCOMP_SUB:
            complex_sub(&result, a, b);
            break;
        case CSNCOMP_PROD:
            if (ca != NULL && cb != NULL) {
                complex_prod(&result, a, b);
            } else {
                if (ca != NULL) complex_scalar_prod(&result, a, real_value);
                else complex_scalar_prod(&result, b, real_value);
            }
            break;
        case CSNCOMP_DIV:
            if (complex_div(&result, a, b) != OK) {
                return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] Division by zero");
            }
            break;
        case CSNCOMP_POW:
            if (complex_pow(&result, a, b) != OK) {
                return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] Zero raised to a complex power is undefined");
            }
            break;
        case CSNCOMP_SQRT:
        default:
            /* Unary: nonarr_complex_sqrt handles it without this helper. */
            break;
    }

    out->real = (cs_float) result.re;
    out->imag = (cs_float) result.im;
    out->isPolar = 0;

    return OK;
}


int32_t nonarr_complex_add_cc(CSOUND *csound, CSN_NONARR_CC_OP *p) {
    return nonarr_complex_ops(csound, NULL, p->arg_a, p->arg_b, NULL, NULL, p->value, CSNCOMP_ADD);
}

int32_t nonarr_complex_add_cr(CSOUND *csound, CSN_NONARR_CR_OP *p) {
    return nonarr_complex_ops(csound, NULL, p->arg_a, NULL, NULL, p->arg_b, p->value, CSNCOMP_ADD);
}

int32_t nonarr_complex_add_rc(CSOUND *csound, CSN_NONARR_RC_OP *p) {
    return nonarr_complex_ops(csound, NULL, NULL, p->arg_b, p->arg_a, NULL, p->value, CSNCOMP_ADD);
}

int32_t nonarr_complex_sub_cc(CSOUND *csound, CSN_NONARR_CC_OP *p) {
    return nonarr_complex_ops(csound, NULL, p->arg_a, p->arg_b, NULL, NULL, p->value, CSNCOMP_SUB);
}

int32_t nonarr_complex_sub_cr(CSOUND *csound, CSN_NONARR_CR_OP *p) {
    return nonarr_complex_ops(csound, NULL, p->arg_a, NULL, NULL, p->arg_b, p->value, CSNCOMP_SUB);
}

int32_t nonarr_complex_sub_rc(CSOUND *csound, CSN_NONARR_RC_OP *p) {
    return nonarr_complex_ops(csound, NULL, NULL, p->arg_b, p->arg_a, NULL, p->value, CSNCOMP_SUB);
}

int32_t nonarr_complex_prod_cc(CSOUND *csound, CSN_NONARR_CC_OP *p) {
    return nonarr_complex_ops(csound, NULL, p->arg_a, p->arg_b, NULL, NULL, p->value, CSNCOMP_PROD);
}

int32_t nonarr_complex_prod_cr(CSOUND *csound, CSN_NONARR_CR_OP *p) {
    return nonarr_complex_ops(csound, NULL, p->arg_a, NULL, NULL, p->arg_b, p->value, CSNCOMP_PROD);
}

int32_t nonarr_complex_prod_rc(CSOUND *csound, CSN_NONARR_RC_OP *p) {
    return nonarr_complex_ops(csound, NULL, NULL, p->arg_b, p->arg_a, NULL, p->value, CSNCOMP_PROD);
}

int32_t nonarr_complex_div_cc(CSOUND *csound, CSN_NONARR_CC_OP *p) {
    return nonarr_complex_ops(csound, NULL, p->arg_a, p->arg_b, NULL, NULL, p->value, CSNCOMP_DIV);
}

int32_t nonarr_complex_div_cc_k(CSOUND *csound, CSN_NONARR_CC_OP *p) {
    return nonarr_complex_ops(csound, &p->h, p->arg_a, p->arg_b, NULL, NULL, p->value, CSNCOMP_DIV);
}

int32_t nonarr_complex_div_cr(CSOUND *csound, CSN_NONARR_CR_OP *p) {
    return nonarr_complex_ops(csound, NULL, p->arg_a, NULL, NULL, p->arg_b, p->value, CSNCOMP_DIV);
}

int32_t nonarr_complex_div_cr_k(CSOUND *csound, CSN_NONARR_CR_OP *p) {
    return nonarr_complex_ops(csound, &p->h, p->arg_a, NULL, NULL, p->arg_b, p->value, CSNCOMP_DIV);
}

int32_t nonarr_complex_div_rc(CSOUND *csound, CSN_NONARR_RC_OP *p) {
    return nonarr_complex_ops(csound, NULL, NULL, p->arg_b, p->arg_a, NULL, p->value, CSNCOMP_DIV);
}

int32_t nonarr_complex_div_rc_k(CSOUND *csound, CSN_NONARR_RC_OP *p) {
    return nonarr_complex_ops(csound, &p->h, NULL, p->arg_b, p->arg_a, NULL, p->value, CSNCOMP_DIV);
}

int32_t nonarr_complex_pow_cc(CSOUND *csound, CSN_NONARR_CC_OP *p) {
    return nonarr_complex_ops(csound, NULL, p->arg_a, p->arg_b, NULL, NULL, p->value, CSNCOMP_POW);
}

int32_t nonarr_complex_pow_cc_k(CSOUND *csound, CSN_NONARR_CC_OP *p) {
    return nonarr_complex_ops(csound, &p->h, p->arg_a, p->arg_b, NULL, NULL, p->value, CSNCOMP_POW);
}

int32_t nonarr_complex_pow_cr(CSOUND *csound, CSN_NONARR_CR_OP *p) {
    return nonarr_complex_ops(csound, NULL, p->arg_a, NULL, NULL, p->arg_b, p->value, CSNCOMP_POW);
}

int32_t nonarr_complex_pow_cr_k(CSOUND *csound, CSN_NONARR_CR_OP *p) {
    return nonarr_complex_ops(csound, &p->h, p->arg_a, NULL, NULL, p->arg_b, p->value, CSNCOMP_POW);
}

int32_t nonarr_complex_pow_rc(CSOUND *csound, CSN_NONARR_RC_OP *p) {
    return nonarr_complex_ops(csound, NULL, NULL, p->arg_b, p->arg_a, NULL, p->value, CSNCOMP_POW);
}

int32_t nonarr_complex_pow_rc_k(CSOUND *csound, CSN_NONARR_RC_OP *p) {
    return nonarr_complex_ops(csound, &p->h, NULL, p->arg_b, p->arg_a, NULL, p->value, CSNCOMP_POW);
}

/* Square root takes one operand: there is no second one to pass and none to
   ignore, so it does not go through the binary helper at all. It cannot fail,
   and one function serves both rates. */
int32_t nonarr_complex_sqrt(CSOUND *csound, CSN_NONARR_CCOUT_OP *p) {
    (void) csound;
    CSN_COMPLEXDAT z = {0};
    CSN_COMPLEXDAT result = {0};
    complexdat_to_rect(p->arg_a, &z.re, &z.im);
    complex_sqrt(&result, z);
    p->value->real = (cs_float) result.re;
    p->value->imag = (cs_float) result.im;
    p->value->isPolar = 0;
    return OK;
}

int32_t nonarr_complex_abs(CSOUND *csound, CSN_NONARR_CROUT_OP *p) {
    (void) csound;
    CSN_COMPLEXDAT c = {0};
    complexdat_to_rect(p->arg_a, &c.re, &c.im);
    /* hypot, as everywhere else: re*re + im*im overflows above 1.3e154 and
       flushes to zero below 1.5e-162, where the magnitude itself does not. */
    *p->value = (cs_float) hypot(c.re, c.im);
    return OK;
}

int32_t nonarr_complex_angle(CSOUND *csound, CSN_NONARR_CROUT_OP *p) {
    (void) csound;
    CSN_COMPLEXDAT c = {0};
    complexdat_to_rect(p->arg_a, &c.re, &c.im);
    *p->value = (cs_float) atan2(c.im, c.re);
    return OK;
}

int32_t nonarr_complex_conj(CSOUND *csound, CSN_NONARR_CCOUT_OP *p) {
    (void) csound;
    CSN_COMPLEXDAT c = {0};
    complexdat_to_rect(p->arg_a, &c.re, &c.im);
    p->value->real = (cs_float) c.re;
    p->value->imag = (cs_float) -c.im;
    p->value->isPolar = 0;
    return OK;
}
