/* Opcode implementations for the vector family.
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

static int32_t check_dot_shape(const uint32_t *shape_a, const uint32_t *shape_b, size_t dim_a, size_t dim_b) {
    size_t bk = (dim_b >= 2) ? dim_b - 2 : 0;
    return (shape_a[dim_a - 1] == shape_b[bk]) ? OK : NOTOK;
}

static inline CSN_COMPLEXDAT item_at(const CSN_ARRAY *arr, size_t off) {
    CSN_COMPLEXDAT z = { arr->data[off * arr->itype], 0.0 };
    if (arr->itype == CSN_COMPLEX) z.im = arr->data[off * 2 + 1];
    return z;
}

static inline void item_set(CSN_ARRAY *arr, size_t off, CSN_COMPLEXDAT z) {
    arr->data[off * arr->itype] = z.re;
    if (arr->itype == CSN_COMPLEX) arr->data[off * 2 + 1] = z.im;
}

static void get_dot_inner_accum(CSN_COMPLEXDAT *acc, size_t base_a, size_t base_b, size_t stride_a, size_t stride_b, CSN_ARRAY *source_arr_a, CSN_ARRAY *source_arr_b, uint32_t loop_dim) {
    acc->re = 0.0;
    acc->im = 0.0;
    for (uint32_t k = 0; k < loop_dim; ++k) {
        size_t off_a = base_a + (size_t) k * stride_a;
        size_t off_b = base_b + (size_t) k * stride_b;
        CSN_COMPLEXDAT prod = {0};
        complex_prod(&prod, item_at(source_arr_a, off_a), item_at(source_arr_b, off_b));
        complex_add(acc, *acc, prod);
    }
}

static int32_t dot_inner(CSN_ARRAY *out_arr, CSN_ARRAY *source_arr_a, CSN_ARRAY *source_arr_b, CSN_VECOP_MODE mode) {
    uint32_t source_dim_a = source_arr_a->ndim;
    uint32_t source_dim_b = source_arr_b->ndim;
    uint32_t *source_shape_a = source_arr_a->shape;
    uint32_t ka_dim  = source_shape_a[source_dim_a - 1];
    uint32_t bk = mode == CSN_DOT ? (source_dim_b >= 2 ? source_dim_b - 2 : 0) : source_dim_b - 1;
    size_t plan[CSN_MAX_BROADCAST_INPUTS][CSN_MAX_DIMS] = {{0}};
    uint32_t d = 0;
    for (uint32_t i = 0; i + 1 < source_dim_a; ++i) plan[0][d++] = source_arr_a->strides[i];
    for (uint32_t i = 0; i < source_dim_b; ++i) {
        if (i != bk) plan[1][d++] = source_arr_b->strides[i];
    }
    CSN_BROADCAST_ITER it;
    if (BROADCAST_ITER_INIT(&it, out_arr->ndim, out_arr->shape, out_arr->size, plan, 2) != OK) return NOTOK;
    while (BROADCAST_ITER_NEXT(&it)) {
        CSN_COMPLEXDAT acc = { 0.0, 0.0 };
        get_dot_inner_accum(&acc, it.offsets[0], it.offsets[1], source_arr_a->strides[source_dim_a - 1], source_arr_b->strides[bk], source_arr_a, source_arr_b, ka_dim);
        item_set(out_arr, it.linear_index, acc);
    }
    return OK;
}

static int32_t vec_body(CSOUND *csound, OPDS *perf_h, CSN_REGISTRY *reg, uint32_t source_handle_a, uint32_t source_handle_b, CSN_ARRAY **source_array_a, CSN_ARRAY **source_array_b) {
    CSN_SLOT *slot_a = get_slot(reg, source_handle_a);
    if (slot_a == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle_a);
    }

    CSN_SLOT *slot_b = get_slot(reg, source_handle_b);
    if (slot_b == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle_b);
    }

    *source_array_a = slot_a->array;
    CSN_ARRAY *source_arr_a = *source_array_a;

    if (source_arr_a->itype != slot_b->array->itype) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Element type mismatch: one array is real and the other complex");
    }

    *source_array_b = slot_b->array;

    return OK;
}

static int32_t vec_assign_shape_and_dim(CSOUND *csound, OPDS *perf_h, const uint32_t *source_shape_a, const uint32_t *source_shape_b, const uint32_t source_dim_a, const uint32_t source_dim_b, uint32_t *new_shape, uint32_t *new_ndim, CSN_VECOP_MODE mode) {
    uint32_t j = 0;
    size_t bk = (source_dim_b >= 2) ? source_dim_b - 2 : 0;
    switch (mode) {
        case CSN_DOT:
            if (source_dim_a == 1 && source_dim_b == 1) {
                return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] The dot product of two 1-D arrays is a scalar; declare the output as i, not as a handle");
            }

            if (check_dot_shape(source_shape_a, source_shape_b, source_dim_a, source_dim_b) != OK) {
                char abuf[CSN_SHAPE_STR_MAX], bbuf[CSN_SHAPE_STR_MAX];
                return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Shapes %s and %s are not valid for a dot product: the last axis of the first must match the second-to-last axis of the second", shape_str(abuf, sizeof(abuf), source_shape_a, (uint32_t) source_dim_a), shape_str(bbuf, sizeof(bbuf), source_shape_b, (uint32_t) source_dim_b));
            }

            j = 0;
            for (uint32_t i = 0; i + 1 < source_dim_a; ++i){
                new_shape[j++] = source_shape_a[i];
            }
            for (uint32_t i = 0; i < source_dim_b; ++i){
                if (i != bk) new_shape[j++] = source_shape_b[i];
            }

            *new_ndim = j;
            break;
        case CSN_INNER:
            if (source_dim_a == 1 && source_dim_b == 1) {
                return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] The inner product of two 1-D arrays is a scalar; declare the output as i, not as a handle");
            }

            if (source_shape_a[source_dim_a - 1] != source_shape_b[source_dim_b - 1]) {
                char abuf[CSN_SHAPE_STR_MAX], bbuf[CSN_SHAPE_STR_MAX];
                return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Shapes %s and %s are not valid for an inner product: the last axis must match (%u vs %u)", shape_str(abuf, sizeof(abuf), source_shape_a, (uint32_t) source_dim_a), shape_str(bbuf, sizeof(bbuf), source_shape_b, (uint32_t) source_dim_b), source_shape_a[source_dim_a - 1], source_shape_b[source_dim_b - 1]);
            }

            j = 0;
            for (uint32_t i = 0; i < source_dim_a - 1; ++i) {
                new_shape[j++] = source_shape_a[i];
            }

            for (uint32_t i = 0; i < source_dim_b - 1; ++i) {
                new_shape[j++] = source_shape_b[i];
            }

            *new_ndim = j;
            break;
        case CSN_OUTER:
            if (source_dim_a != 1 || source_dim_b != 1) {
                return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Outer product needs two 1-D arrays, got %u-D and %u-D", (uint32_t) source_dim_a, (uint32_t) source_dim_b);
            }

            new_shape[0] = source_shape_a[0];
            new_shape[1] = source_shape_b[0];
            *new_ndim = 2U;
            break;
        case CSN_PAIR_DISTANCE:
        case CSN_PROJECT:
        case CSN_REJECT:
        case CSN_REFLECT:
            if (source_dim_a != source_dim_b || memcmp(source_shape_a, source_shape_b, sizeof(uint32_t) * CSN_MAX_DIMS) != 0) {
                char abuf[CSN_SHAPE_STR_MAX], bbuf[CSN_SHAPE_STR_MAX];
                return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Both arrays must have the same shape, got %s and %s", shape_str(abuf, sizeof(abuf), source_shape_a, (uint32_t) source_dim_a), shape_str(bbuf, sizeof(bbuf), source_shape_b, (uint32_t) source_dim_b));
            }

            memcpy(new_shape, source_shape_a, sizeof(uint32_t) * CSN_MAX_DIMS);
            *new_ndim = source_dim_a;
            break;
        case CSN_CROSS:
            if (source_dim_a != 1 || source_dim_b != 1 || source_shape_a[0] != 3 || source_shape_b[0] != 3) {
                char abuf[CSN_SHAPE_STR_MAX], bbuf[CSN_SHAPE_STR_MAX];
                return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Cross product needs two 1-D arrays of size 3, got %s and %s", shape_str(abuf, sizeof(abuf), source_shape_a, (uint32_t) source_dim_a), shape_str(bbuf, sizeof(bbuf), source_shape_b, (uint32_t) source_dim_b));
            }

            new_shape[0] = 3;
            *new_ndim = 1;
            break;
        default:
            break;
    }

    return OK;
}

static int32_t vec_assign_value(CSOUND *csound, OPDS *perf_h, CSN_ARRAY *source_arr_a, CSN_ARRAY *source_arr_b, CSN_ARRAY *arr, CSN_VECOP_MODE mode) {
    size_t size_a = source_arr_a->size;
    size_t size_b = source_arr_b->size;
    CSN_COMPLEXDAT dot_ab = { 0.0, 0.0 };
    CSN_COMPLEXDAT dot_bb = { 0.0, 0.0 };
    switch(mode) {
        case CSN_DOT:
        case CSN_INNER:
            if (dot_inner(arr, source_arr_a, source_arr_b, mode) != OK) {
                return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Invalid dot/inner iteration shape");
            }
            break;
        case CSN_OUTER:
            for (size_t i = 0; i < size_a; ++i) {
                for (size_t j = 0; j < size_b; ++j) {
                    CSN_COMPLEXDAT prod = {0};
                    complex_prod(&prod, item_at(source_arr_a, i), item_at(source_arr_b, j));
                    item_set(arr, i * size_b + j, prod);
                }
            }
            break;
        case CSN_PAIR_DISTANCE:
            for (size_t i = 0; i < size_a; ++i) {
                CSN_COMPLEXDAT d = {0};
                complex_sub(&d, item_at(source_arr_a, i), item_at(source_arr_b, i));
                arr->data[i] = hypot(d.re, d.im);
            }
            break;
        case CSN_PROJECT:
        case CSN_REJECT:
        case CSN_REFLECT: {
            for (size_t i = 0; i < size_a; ++i) {
                CSN_COMPLEXDAT ai = item_at(source_arr_a, i);
                CSN_COMPLEXDAT bi = item_at(source_arr_b, i);
                CSN_COMPLEXDAT bconj = { bi.re, -bi.im };
                CSN_COMPLEXDAT t = {0};
                complex_prod(&t, ai, bconj);
                complex_add(&dot_ab, dot_ab, t);
                complex_prod(&t, bi, bconj);
                complex_add(&dot_bb, dot_bb, t);
            }

            if (dot_bb.re == 0.0 && dot_bb.im == 0.0) {
                return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] The second vector has zero length, so the projection onto it is undefined");
            }

            CSN_COMPLEXDAT scale = {0};
            if (complex_div(&scale, dot_ab, dot_bb) != OK) {
                return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] The second vector has zero length, so the projection onto it is undefined");
            }

            for (size_t i = 0; i < size_a; ++i) {
                CSN_COMPLEXDAT ai = item_at(source_arr_a, i);
                CSN_COMPLEXDAT proj = {0};
                complex_prod(&proj, scale, item_at(source_arr_b, i));

                CSN_COMPLEXDAT value = {0};
                switch (mode) {
                    case CSN_PROJECT:
                        value = proj;
                        break;
                    case CSN_REJECT:
                        complex_sub(&value, ai, proj);
                        break;
                    case CSN_REFLECT: {
                        CSN_COMPLEXDAT twice = { proj.re * 2.0, proj.im * 2.0 };
                        complex_sub(&value, ai, twice);
                        break;
                    }
                    default:
                        break;
                }

                item_set(arr, i, value);
            }
            break;
        }
        case CSN_CROSS: {
            /* Bilinear */
            CSN_COMPLEXDAT a0 = item_at(source_arr_a, 0), a1 = item_at(source_arr_a, 1), a2 = item_at(source_arr_a, 2);
            CSN_COMPLEXDAT b0 = item_at(source_arr_b, 0), b1 = item_at(source_arr_b, 1), b2 = item_at(source_arr_b, 2);
            CSN_COMPLEXDAT l = {0}, r = {0}, c = {0};

            complex_prod(&l, a1, b2); complex_prod(&r, a2, b1);
            complex_sub(&c, l, r); item_set(arr, 0, c);

            complex_prod(&l, a2, b0); complex_prod(&r, a0, b2);
            complex_sub(&c, l, r); item_set(arr, 1, c);

            complex_prod(&l, a0, b1); complex_prod(&r, a1, b0);
            complex_sub(&c, l, r); item_set(arr, 2, c);
            break;
        }
        default:
            break;
    }

    return OK;
}

/* project, reject and cross are defined here over real vectors only, so a
   complex operand is refused rather than silently treated as two real halves.
   The other vector operations carry complex operands through unchanged. */
static int32_t vec_reject_complex(CSOUND *csound, OPDS *perf_h, CSN_VECOP_MODE mode, const CSN_ARRAY *a, const CSN_ARRAY *b) {
    if (mode != CSN_PROJECT && mode != CSN_REJECT && mode != CSN_CROSS) {
        return OK;
    }

    if (a->itype != CSN_COMPLEX && b->itype != CSN_COMPLEX) {
        return OK;
    }

    return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] This operation is not implemented for complex arrays");
}

static int32_t csnarray_vec_helper(CSOUND *csound, CSN_BINOP_HH *p, CSN_VECOP_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle_a = (uint32_t) p->source_handle_a->id;
    uint32_t source_handle_b = (uint32_t) p->source_handle_b->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr_a = NULL;
    CSN_ARRAY *source_arr_b = NULL;
    res = vec_body(csound, NULL, reg, source_handle_a, source_handle_b, &source_arr_a, &source_arr_b);
    if (res != OK) goto done;

    res = vec_reject_complex(csound, NULL, mode, source_arr_a, source_arr_b);
    if (res != OK) goto done;

    uint32_t *source_shape_a = source_arr_a->shape;
    uint32_t *source_shape_b = source_arr_b->shape;
    size_t source_dim_a = source_arr_a->ndim;
    size_t source_dim_b = source_arr_b->ndim;

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    uint32_t new_ndim = 0;
    res = vec_assign_shape_and_dim(csound, NULL, source_shape_a, source_shape_b, source_dim_a, source_dim_b, new_shape, &new_ndim, mode);
    if (res != OK) goto done;

    const uint32_t protect[2] = { source_handle_a, source_handle_b };
    ITEM_TYPE out_itype = (mode == CSN_PAIR_DISTANCE) ? CSN_REAL : source_arr_a->itype;
    if (create_csnarray_locked(csound, reg, &p->h, new_ndim, new_shape, &p->array, p->handle, protect, 2U, &err, out_itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;
    if (source_arr_a->size == 0 || source_arr_b->size == 0) arr->size = 0;
    res = vec_assign_value(csound, NULL, source_arr_a, source_arr_b, arr, mode);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_vec_k_init_helper(CSOUND *csound, CSN_BINOP_HH *p, CSN_VECOP_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle_a = (uint32_t) p->source_handle_a->id;
    uint32_t source_handle_b = (uint32_t) p->source_handle_b->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr_a = NULL;
    CSN_ARRAY *source_arr_b = NULL;
    res = vec_body(csound, NULL, reg, source_handle_a, source_handle_b, &source_arr_a, &source_arr_b);
    if (res != OK) goto done;

    res = vec_reject_complex(csound, NULL, mode, source_arr_a, source_arr_b);
    if (res != OK) goto done;

    uint32_t *source_shape_a = source_arr_a->shape;
    size_t source_dim_a = source_arr_a->ndim;

    /* The output is seeded from operand a and only takes its real shape on the
       first triggered pass, but the operand pair is checked here too: without
       it an unusable combination (a cross product of non 3-vectors, say) would
       surface at the first trigger, or not at all if the gate never opens. */
    uint32_t check_shape[CSN_MAX_DIMS] = {0};
    uint32_t check_ndim = 0;
    res = vec_assign_shape_and_dim(csound, NULL, source_shape_a, source_arr_b->shape, source_dim_a, source_arr_b->ndim, check_shape, &check_ndim, mode);
    if (res != OK) goto done;

    const uint32_t protect[2] = { source_handle_a, source_handle_b };
    ITEM_TYPE itype = source_arr_a->itype;
    if (create_csnarray_locked(csound, reg, &p->h, source_dim_a, source_shape_a, &p->array, p->handle, protect, 2U, &err, itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    p->array->size = source_arr_a->size;
    if (source_arr_a->size > 0) {
        memcpy(p->array->data, source_arr_a->data, sizeof(double) * source_arr_a->size * itype);
    }

    SET_KDATA_BEGIN(p, reg);
    p->k_data.prev_size = source_arr_a->size;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_vec_k_helper(CSOUND *csound, CSN_BINOP_HH *p, CSN_VECOP_MODE mode) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REG_HANDLE(csound, &p->h, reg, p->k_data.owned_handle);

    uint32_t source_handle_a = (uint32_t) p->source_handle_a->id;
    uint32_t source_handle_b = (uint32_t) p->source_handle_b->id;

    int32_t res = OK;
    const char *err = NULL;

    res = CHECK_SELF_ALIAS(csound, &p->h, &p->k_data, source_handle_a, source_handle_b);
    if (res != OK) return res;

    CHECK_KTRIG(p->trig);

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr_a = NULL;
    CSN_ARRAY *source_arr_b = NULL;
    res = vec_body(csound, &p->h, reg, source_handle_a, source_handle_b, &source_arr_a, &source_arr_b);
    if (res != OK) goto done;

    res = vec_reject_complex(csound, &p->h, mode, source_arr_a, source_arr_b);
    if (res != OK) goto done;

    uint32_t *source_shape_a = source_arr_a->shape;
    uint32_t *source_shape_b = source_arr_b->shape;
    size_t source_dim_a = source_arr_a->ndim;
    size_t source_dim_b = source_arr_b->ndim;

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    uint32_t new_ndim = 0;
    res = vec_assign_shape_and_dim(csound, &p->h, source_shape_a, source_shape_b, source_dim_a, source_dim_b, new_shape, &new_ndim, mode);
    if (res != OK) goto done;

    size_t requested_size = 0;
    if (get_array_size_from_shape(&requested_size, new_ndim, new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSN_SLOT *out_slot = get_slot(reg, p->k_data.owned_handle);
    if (out_slot != NULL && CAN_REUSE_ELEMENTWISE(&p->k_data, source_handle_a, source_arr_a, source_handle_b, source_arr_b, out_slot->array, 0.0, 0.0)) {
        p->handle->id = p->k_data.owned_handle;
        goto done;
    }

    CSN_ARRAY *arr = p->array;
    ITEM_TYPE out_itype = (mode == CSN_PAIR_DISTANCE) ? CSN_REAL : source_arr_a->itype;
    size_t logical_size = (source_arr_a->size == 0 || source_arr_b->size == 0) ? 0 : requested_size;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, new_ndim, new_shape, logical_size, out_itype, err);
    if (res != OK) goto done;

    res = vec_assign_value(csound, &p->h, source_arr_a, source_arr_b, arr, mode);
    if (res != OK) goto done;

    SET_KDATA_END(p, new_shape, new_ndim, out_itype);
    p->k_data.prev_size = arr->size;
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle_a, source_arr_a, source_handle_b, source_arr_b, arr, 0.0, 0.0);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_dot(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_vec_helper(csound, p, CSN_DOT);
}

int32_t csnarray_dot_k_init(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_vec_k_init_helper(csound, p, CSN_DOT);
}

int32_t csnarray_dot_k(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_vec_k_helper(csound, p, CSN_DOT);
}

static int32_t scalar_helper_impl_check(CSOUND *csound, OPDS *perf_h, CSN_ARRAY *source_arr_a, CSN_ARRAY *source_arr_b, double dist_order, const MYFLT *out_value, const COMPLEXDAT *out_complex, CSN_VECOP_MODE mode) {
    uint32_t *source_shape_a = source_arr_a->shape;
    uint32_t *source_shape_b = source_arr_b->shape;
    size_t source_dim_a = source_arr_a->ndim;
    size_t source_dim_b = source_arr_b->ndim;

    if (source_dim_a != 1 || source_dim_b != 1 || source_shape_a[0] != source_shape_b[0]) {
        char abuf[CSN_SHAPE_STR_MAX], bbuf[CSN_SHAPE_STR_MAX];
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Both arrays must be 1-D with the same size, got %s and %s", shape_str(abuf, sizeof(abuf), source_shape_a, (uint32_t) source_dim_a), shape_str(bbuf, sizeof(bbuf), source_shape_b, (uint32_t) source_dim_b));
    }

    if (source_arr_a->itype != source_arr_b->itype) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Element type mismatch: one array is real and the other complex");
    }

    if (mode == CSN_DISTANCE && dist_order <= 0.0) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Distance order must be >= 1, got %g", dist_order);
    }

    bool wants_complex = (mode != CSN_DISTANCE) && source_arr_a->itype == CSN_COMPLEX;
    if (wants_complex && out_complex == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Handle holds a complex array; declare the result as :Complex;");
    }
    if (!wants_complex && out_value == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Handle holds a real array; declare the result as i");
    }

    return OK;
}

static void scalar_helper_assign_value(CSN_ARRAY *source_arr_a, CSN_ARRAY *source_arr_b, double dist_order, MYFLT *out_value, COMPLEXDAT *out_complex, CSN_VECOP_MODE mode) {
    bool wants_complex = (mode != CSN_DISTANCE) && source_arr_a->itype == CSN_COMPLEX;
    double value = 0.0;
    CSN_COMPLEXDAT acc = { 0.0, 0.0 };
    for (size_t i = 0; i < source_arr_a->size; i++) {
        CSN_COMPLEXDAT ai = item_at(source_arr_a, i);
        CSN_COMPLEXDAT bi = item_at(source_arr_b, i);
        switch (mode) {
            case CSN_DOT_SCALAR:
            case CSN_INNER_SCALAR: {
                /* Bilinear */
                CSN_COMPLEXDAT prod = {0};
                complex_prod(&prod, ai, bi);
                complex_add(&acc, acc, prod);
                break;
            }
            case CSN_DISTANCE: {
                CSN_COMPLEXDAT d = {0};
                complex_sub(&d, ai, bi);
                value += pow(hypot(d.re, d.im), dist_order);
                break;
            }
            default:
                break;
        }
    }

    if (mode == CSN_DISTANCE) {
        *out_value = (MYFLT) pow(value, 1.0 / dist_order);
    } else if (wants_complex) {
        out_complex->real = (MYFLT) acc.re;
        out_complex->imag = (MYFLT) acc.im;
        out_complex->isPolar = 0;
    } else {
        *out_value = (MYFLT) acc.re;
    }
}

static int32_t csnarray_scalar_helper_impl(CSOUND *csound, CSNREF *ref_a, CSNREF *ref_b, MYFLT *out_value, COMPLEXDAT *out_complex, CSN_VECOP_MODE mode, double dist_order, CSN_REGISTRY **registry) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle_a = (uint32_t) ref_a->id;
    uint32_t source_handle_b = (uint32_t) ref_b->id;

    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot_a = get_slot(reg, source_handle_a);
    if (slot_a == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle_a);
        goto done;
    }

    CSN_SLOT *slot_b = get_slot(reg, source_handle_b);
    if (slot_b == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle_b);
        goto done;
    }

    CSN_ARRAY *source_arr_a = slot_a->array;
    CSN_ARRAY *source_arr_b = slot_b->array;
    res = scalar_helper_impl_check(csound, NULL, source_arr_a, source_arr_b, dist_order, out_value, out_complex, mode);
    if (res != OK) goto done;

    scalar_helper_assign_value(source_arr_a, source_arr_b, dist_order, out_value, out_complex, mode);
    *registry = reg;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_scalar_helper_k_impl(CSOUND *csound, CSNREF *ref_a, CSNREF *ref_b, MYFLT *out_value, COMPLEXDAT *out_complex, CSN_VECOP_MODE mode, double dist_order, OPDS *perf_h, CSN_REGISTRY *registry, const MYFLT *trig, K_DATA *k_data) {
    CSN_REGISTRY *reg = registry;
    if (registry == NULL) {
        return csound->PerfError(csound, perf_h, "[csnarray] Internal error: the csnum array registry is not available");
    }

    uint32_t source_handle_a = (uint32_t) ref_a->id;
    uint32_t source_handle_b = (uint32_t) ref_b->id;

    CHECK_KTRIG(trig);

    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot_a = get_slot(reg, source_handle_a);
    if (slot_a == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle_a);
    }

    CSN_SLOT *slot_b = get_slot(reg, source_handle_b);
    if (slot_b == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle_b);
    }

    CSN_ARRAY *source_arr_a = slot_a->array;
    CSN_ARRAY *source_arr_b = slot_b->array;
    res = scalar_helper_impl_check(csound, perf_h, source_arr_a, source_arr_b, dist_order, out_value, out_complex, mode);
    if (res != OK) goto done;

    if (CAN_REUSE_ELEMENTWISE(k_data, source_handle_a, source_arr_a, source_handle_b, source_arr_b, NULL, dist_order, 0.0)) {
        goto done;
    }

    scalar_helper_assign_value(source_arr_a, source_arr_b, dist_order, out_value, out_complex, mode);
    PUBLISH_ELEMENTWISE(k_data, source_handle_a, source_arr_a, source_handle_b, source_arr_b, NULL, dist_order, 0.0);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_scalar_helper(CSOUND *csound, CSN_BINOP_HH_SCALAR *p, CSN_VECOP_MODE mode, double dist_order) {
    return csnarray_scalar_helper_impl(csound, p->source_handle_a, p->source_handle_b, p->value, NULL, mode, dist_order, &p->registry);
}

static int32_t csnarray_scalar_helper_k(CSOUND *csound, CSN_BINOP_HH_SCALAR *p, CSN_VECOP_MODE mode, double dist_order, const MYFLT *trig) {
    return csnarray_scalar_helper_k_impl(csound, p->source_handle_a, p->source_handle_b, p->value, NULL, mode, dist_order, &p->h, p->registry, trig, &p->k_data);
}

int32_t csnarray_dotcomp_scalar(CSOUND *csound, CSN_BINOPCOMPLEX_HH_SCALAR *p) {
    return csnarray_scalar_helper_impl(csound, p->source_handle_a, p->source_handle_b, NULL, p->value, CSN_DOT_SCALAR, 0.0, &p->registry);
}

int32_t csnarray_dotcomp_scalar_k(CSOUND *csound, CSN_BINOPCOMPLEX_HH_SCALAR *p) {
    return csnarray_scalar_helper_k_impl(csound, p->source_handle_a, p->source_handle_b, NULL, p->value, CSN_DOT_SCALAR, 0.0, &p->h, p->registry, p->trig, &p->k_data);
}

int32_t csnarray_innercomp_scalar(CSOUND *csound, CSN_BINOPCOMPLEX_HH_SCALAR *p) {
    return csnarray_scalar_helper_impl(csound, p->source_handle_a, p->source_handle_b, NULL, p->value, CSN_INNER_SCALAR, 0.0, &p->registry);
}

int32_t csnarray_innercomp_scalar_k(CSOUND *csound, CSN_BINOPCOMPLEX_HH_SCALAR *p) {
    return csnarray_scalar_helper_k_impl(csound, p->source_handle_a, p->source_handle_b, NULL, p->value, CSN_INNER_SCALAR, 0.0, &p->h, p->registry, p->trig, &p->k_data);
}

int32_t csnarray_dot_scalar(CSOUND *csound, CSN_BINOP_HH_SCALAR *p) {
    return csnarray_scalar_helper(csound, p, CSN_DOT_SCALAR, 0.0);
}

int32_t csnarray_dot_scalar_k(CSOUND *csound, CSN_BINOP_HH_SCALAR *p) {
    return csnarray_scalar_helper_k(csound, p, CSN_DOT_SCALAR, 0.0, p->arg_a);
}

int32_t csnarray_inner(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_vec_helper(csound, p, CSN_INNER);
}

int32_t csnarray_inner_k_init(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_vec_k_init_helper(csound, p, CSN_INNER);
}

int32_t csnarray_inner_k(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_vec_k_helper(csound, p, CSN_INNER);
}

int32_t csnarray_inner_scalar(CSOUND *csound, CSN_BINOP_HH_SCALAR *p) {
    return csnarray_scalar_helper(csound, p, CSN_INNER_SCALAR, 0.0);
}

int32_t csnarray_inner_scalar_k(CSOUND *csound, CSN_BINOP_HH_SCALAR *p) {
    return csnarray_scalar_helper_k(csound, p, CSN_INNER_SCALAR, 0.0, p->arg_a);
}

int32_t csnarray_outer(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_vec_helper(csound, p, CSN_OUTER);
}

int32_t csnarray_outer_k_init(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_vec_k_init_helper(csound, p, CSN_OUTER);
}

int32_t csnarray_outer_k(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_vec_k_helper(csound, p, CSN_OUTER);
}

int32_t csnarray_project(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_vec_helper(csound, p, CSN_PROJECT);
}

int32_t csnarray_project_k_init(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_vec_k_init_helper(csound, p, CSN_PROJECT);
}

int32_t csnarray_project_k(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_vec_k_helper(csound, p, CSN_PROJECT);
}

int32_t csnarray_reject(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_vec_helper(csound, p, CSN_REJECT);
}

int32_t csnarray_reject_k_init(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_vec_k_init_helper(csound, p, CSN_REJECT);
}

int32_t csnarray_reject_k(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_vec_k_helper(csound, p, CSN_REJECT);
}

int32_t csnarray_reflect(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_vec_helper(csound, p, CSN_REFLECT);
}

int32_t csnarray_reflect_k_init(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_vec_k_init_helper(csound, p, CSN_REFLECT);
}

int32_t csnarray_reflect_k(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_vec_k_helper(csound, p, CSN_REFLECT);
}

int32_t csnarray_cross(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_vec_helper(csound, p, CSN_CROSS);
}

int32_t csnarray_cross_k_init(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_vec_k_init_helper(csound, p, CSN_CROSS);
}

int32_t csnarray_cross_k(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_vec_k_helper(csound, p, CSN_CROSS);
}

static double norm_from_scratch(const double *arr, size_t size, double order, ITEM_TYPE itype) {
    double acc = 0.0;
    for (size_t i = 0; i < size; i++) {
        double x = itype == CSN_COMPLEX ? hypot(arr[i * 2], arr[i * 2 + 1]) : fabs(arr[i]);
        acc += pow(x, order);
    }
    return pow(acc, 1.0 / order);
}

static int32_t norm_assign_value(CSN_ARRAY *source_arr, CSN_ARRAY *destination, double *scratch, int32_t axis, uint32_t run_size, ITEM_TYPE itype, double order) {
    CSN_AXIS_SLICE_ITER it;
    if (AXIS_ITER_SLICE_INIT(&it, source_arr, destination, (uint32_t) axis) != OK) return NOTOK;
    while (AXIS_SLICE_ITER_NEXT(&it)) {
        for (uint32_t k = 0; k < run_size; ++k) {
            size_t off = it.src_base + (size_t) k * it.src_axis_stride;
            scratch[k * itype] = source_arr->data[off * itype];
            if (itype == CSN_COMPLEX) scratch[k * 2 + 1] = source_arr->data[off * 2 + 1];
        }
        destination->data[it.dst_base] = norm_from_scratch(scratch, run_size, order, itype);
    }
    return OK;
}

int32_t csnarray_norm(CSOUND *csound, CSN_NORM_REDUCTION *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = (uint32_t) p->source_handle->id;
    double axis_value = (double) *p->axis;
    double order = (double) *p->order;

    if (order < 1.0) {
        return csound->InitError(csound, "[csnarray] Norm order must be >= 1, got %g", order);
    }

    int32_t res = OK;
    const char *err = NULL;
    double *scratch = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = source_slot->array;
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    CSN_AXIS_SPEC axis_spec = csn_normalize_axis_value(axis_value, source_ndim);
    if (axis_spec.kind != CSN_AXIS_INDEX) {
        res = csound->InitError(csound, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: finite integers %d..%u)", axis_value, source_ndim, -(int32_t) source_ndim, source_ndim - 1);
        goto done;
    }
    uint32_t axis = axis_spec.index;

    ITEM_TYPE itype = source_arr->itype;
    size_t run = source_shape[axis];
    size_t scratch_items = run > 0 ? run : 1;
    scratch = csound->Calloc(csound, sizeof(double) * scratch_items * itype);
    if (scratch == NULL) {
        res = csound->InitError(csound, "[csnarray] Out of memory: allocation of %zu bytes failed", (size_t) (sizeof(double) * scratch_items * itype));
        goto done;
    }

    /* Reducing along an axis drops it, as every other reduction does and as
       np.linalg.norm(a, axis=k) reports. */
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    uint32_t new_ndim = 0;
    for (uint32_t i = 0; i < source_ndim; ++i) {
        if (i != (uint32_t) axis) new_shape[new_ndim++] = source_shape[i];
    }

    const uint32_t protect[1] = { source_handle };
    if (create_csnarray_locked(csound, reg, &p->h, new_ndim, new_shape, &p->array, p->handle, protect, 1U, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;
    if (norm_assign_value(source_arr, arr, scratch, axis, run, itype, order) != OK) {
        res = csound->InitError(csound, "[csnarray] Invalid norm iteration shape");
        goto done;
    }

done:
    csound->UnlockMutex(reg->mutex);
    if (scratch != NULL) {
        csound->Free(csound, scratch);
    }
    return res;
}

int32_t csnarray_norm_k_init(CSOUND *csound, CSN_NORM_REDUCTION *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = (uint32_t) p->source_handle->id;
    /* Public k-rate order: source, order, trigger, axis. */
    double axis_value = (double) *p->trig;
    double order = (double) *p->axis;

    /* order is a k-argument and normally still reads 0 during the init pass, so
       only a value the orchestra really set can be rejected here; the perf pass
       checks it again before every use. */
    if (order != 0.0 && order < 1.0) {
        return csound->InitError(csound, "[csnarray] Norm order must be >= 1, got %g", order);
    }

    int32_t res = OK;
    const char *err = NULL;
    double *scratch = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = source_slot->array;
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    CSN_AXIS_SPEC axis_spec = csn_normalize_axis_value(axis_value, source_ndim);
    if (axis_spec.kind != CSN_AXIS_INDEX) {
        res = csound->InitError(csound, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: finite integers %d..%u)", axis_value, source_ndim, -(int32_t) source_ndim, source_ndim - 1);
        goto done;
    }
    uint32_t axis = axis_spec.index;

    ITEM_TYPE itype = source_arr->itype;
    size_t run = source_shape[axis];
    /* Counted in doubles, so a later complex source cannot outgrow it
       unnoticed. Any run along any axis fits in the source's capacity, which
       keeps a marked path from growing this at perf time. */
    size_t scratch_cap = (source_arr->capacity > 0 ? source_arr->capacity : 1) * itype;
    scratch = csound->Calloc(csound, sizeof(double) * scratch_cap);
    if (scratch == NULL) {
        res = csound->InitError(csound, "[csnarray] Out of memory: allocation of %zu bytes failed", (size_t) (sizeof(double) * scratch_cap));
        goto done;
    }

    /* Reducing along an axis drops it, as every other reduction does and as
       np.linalg.norm(a, axis=k) reports. */
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    uint32_t new_ndim = 0;
    for (uint32_t i = 0; i < source_ndim; ++i) {
        if (i != (uint32_t) axis) new_shape[new_ndim++] = source_shape[i];
    }

    const uint32_t protect[1] = { source_handle };
    if (create_csnarray_locked(csound, reg, &p->h, new_ndim, new_shape, &p->array, p->handle, protect, 1U, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;
    /* With the order still unset the output keeps its shape and stays zeroed;
       the first triggered pass fills it. */
    if (order >= 1.0) {
        if (norm_assign_value(source_arr, arr, scratch, axis, run, itype, order) != OK) {
            res = csound->InitError(csound, "[csnarray] Invalid norm iteration shape");
            goto done;
        }
    }
    SET_KDATA_BEGIN(p, reg);
    p->scratch.scratch = scratch;
    p->scratch.scratch_capacity = scratch_cap;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_norm_k(CSOUND *csound, CSN_NORM_REDUCTION *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REG_HANDLE(csound, &p->h, reg, p->k_data.owned_handle);

    uint32_t source_handle = (uint32_t) p->source_handle->id;
    double axis_value = (double) *p->trig;
    double order = (double) *p->axis;

    if (order < 1.0) {
        return csound->PerfError(csound, &p->h, "[csnarray] Norm order must be >= 1, got %g", order);
    }

    CHECK_KTRIG(p->order);

    int32_t res = OK;
    const char *err = NULL;

    res = CHECK_SELF_ALIAS(csound, &p->h, &p->k_data, source_handle, 0);
    if (res != OK) return res;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
    }

    CSN_ARRAY *source_arr = source_slot->array;
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    CSN_AXIS_SPEC axis_spec = csn_normalize_axis_value(axis_value, source_ndim);
    if (axis_spec.kind != CSN_AXIS_INDEX) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: finite integers %d..%u)", axis_value, source_ndim, -(int32_t) source_ndim, source_ndim - 1);
    }
    uint32_t axis = axis_spec.index;

    ITEM_TYPE itype = source_arr->itype;
    size_t run = source_shape[axis];
    size_t scratch_doubles = (run > 0 ? run : 1) * itype;

    res = csn_scratch_reserve(csound, &p->h, csn_slot_rt_locked(reg, p->k_data.owned_handle), &p->scratch, scratch_doubles, sizeof(double));
    if (res != OK) goto done;

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    uint32_t new_ndim = 0;
    for (uint32_t i = 0; i < source_ndim; ++i) {
        if (i != (uint32_t) axis) new_shape[new_ndim++] = source_shape[i];
    }

    size_t requested_size = 0;
    if (get_array_size_from_shape(&requested_size, new_ndim, new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSN_ARRAY *arr = p->array;
    size_t logical_size = source_arr->size == 0 ? 0 : requested_size;
    CSN_SLOT *out_slot = get_slot(reg, p->k_data.owned_handle);
    if (out_slot != NULL && CAN_REUSE_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, out_slot->array, (double) axis, order)) {
        p->handle->id = p->k_data.owned_handle;
        goto done;
    }

    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, new_ndim, new_shape, logical_size, itype, err);
    if (res != OK) goto done;

    if (norm_assign_value(source_arr, arr, p->scratch.scratch, axis, run, itype, order) != OK) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Invalid norm iteration shape");
        goto done;
    }
    SET_KDATA_END(p, new_shape, new_ndim, itype);
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, arr, (double) axis, order);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_norm_scalar(CSOUND *csound, CSN_NORM_REDUCTION_SCALAR *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    double order = (double) *p->order;
    if (order < 1.0) {
        return csound->InitError(csound, "[csnarray] Norm order must be >= 1, got %g", order);
    }

    uint32_t source_handle_a = (uint32_t) p->source_handle->id;
    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, source_handle_a);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle_a);
        goto done;
    }

    CSN_ARRAY *source_arr = slot->array;
    *p->value = (MYFLT) norm_from_scratch(source_arr->data, source_arr->size, order, source_arr->itype);
    p->registry = reg;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

/* Same as the axis form: the order arrives at k-rate and reads 0 at init. */
int32_t csnarray_norm_scalar_k_init(CSOUND *csound, CSN_NORM_REDUCTION_SCALAR *p) {
    if ((double) *p->order >= 1.0) {
        return csnarray_norm_scalar(csound, p);
    }

    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    p->registry = reg;
    *p->value = FL(0.0);
    return OK;
}

int32_t csnarray_norm_scalar_k(CSOUND *csound, CSN_NORM_REDUCTION_SCALAR *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    double order = (double) *p->order;
    if (order < 1.0) {
        return csound->PerfError(csound, &p->h, "[csnarray] Norm order must be >= 1, got %g", order);
    }

    CHECK_KTRIG(p->trig);

    uint32_t source_handle_a = (uint32_t) p->source_handle->id;
    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, source_handle_a);
    if (slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle_a);
    }

    CSN_ARRAY *source_arr = slot->array;
    if (!CAN_REUSE_ELEMENTWISE(&p->k_data, source_handle_a, source_arr, 0, NULL, NULL, order, 0.0)) {
        *p->value = (MYFLT) norm_from_scratch(source_arr->data, source_arr->size, order, source_arr->itype);
        PUBLISH_ELEMENTWISE(&p->k_data, source_handle_a, source_arr, 0, NULL, NULL, order, 0.0);
    }
    p->registry = reg;

    csound->UnlockMutex(reg->mutex);
    return res;
}

CSN_COMPLEXDAT slice_get(const double *src, size_t i, size_t stride, ITEM_TYPE itype) {
    size_t at = i * stride * itype;
    CSN_COMPLEXDAT z = { src[at], itype == CSN_COMPLEX ? src[at + 1] : 0.0 };
    return z;
}

void slice_put(double *dst, size_t i, size_t stride, ITEM_TYPE itype, CSN_COMPLEXDAT z) {
    size_t at = i * stride * itype;
    dst[at] = z.re;
    if (itype == CSN_COMPLEX) dst[at + 1] = z.im;
}

static void normalize_slice(double *dst, const double *src, size_t n, size_t stride, double order, ITEM_TYPE itype) {
    double acc = 0.0;
    for (size_t i = 0; i < n; ++i) {
        CSN_COMPLEXDAT z = slice_get(src, i, stride, itype);
        double mag = itype == CSN_COMPLEX ? hypot(z.re, z.im) : fabs(z.re);
        acc += pow(mag, order);
    }

    double nrm = pow(acc, 1.0 / order);
    for (size_t i = 0; i < n; ++i) {
        CSN_COMPLEXDAT z = slice_get(src, i, stride, itype);
        if (nrm != 0.0) {
            z.re /= nrm;
            z.im /= nrm;
        }
        slice_put(dst, i, stride, itype, z);
    }
}

static void cumsumprod_slice(double *dst, const double *src, size_t n, size_t stride, bool is_cumsum, ITEM_TYPE itype) {
    CSN_COMPLEXDAT acc = { is_cumsum ? 0.0 : 1.0, 0.0 };
    for (size_t i = 0; i < n; ++i) {
        CSN_COMPLEXDAT z = slice_get(src, i, stride, itype);
        if (is_cumsum) complex_add(&acc, acc, z);
        else complex_prod(&acc, acc, z);
        slice_put(dst, i, stride, itype, acc);
    }
}

static void diff_slice(double *dst, const double *src, size_t n, size_t src_stride, size_t dst_stride, ITEM_TYPE itype) {
    for (size_t i = 0; i + 1 < n; ++i) {
        CSN_COMPLEXDAT hi = slice_get(src, i + 1, src_stride, itype);
        CSN_COMPLEXDAT lo = slice_get(src, i, src_stride, itype);
        CSN_COMPLEXDAT d = {0};
        complex_sub(&d, hi, lo);
        slice_put(dst, i, dst_stride, itype, d);
    }
}

static void gradient_slice(double *dst, const double *src, size_t n, size_t stride, ITEM_TYPE itype) {
    if (n == 0)
        return;
    if (n == 1) {
        CSN_COMPLEXDAT zero = { 0.0, 0.0 };
        slice_put(dst, 0, stride, itype, zero);
        return;
    }

    CSN_COMPLEXDAT d = {0};
    complex_sub(&d, slice_get(src, 1, stride, itype), slice_get(src, 0, stride, itype));
    slice_put(dst, 0, stride, itype, d);

    for (size_t i = 1; i < n - 1; ++i) {
        CSN_COMPLEXDAT c = {0};
        complex_sub(&c, slice_get(src, i + 1, stride, itype), slice_get(src, i - 1, stride, itype));
        c.re *= 0.5;
        c.im *= 0.5;
        slice_put(dst, i, stride, itype, c);
    }

    CSN_COMPLEXDAT e = {0};
    complex_sub(&e, slice_get(src, n - 1, stride, itype), slice_get(src, n - 2, stride, itype));
    slice_put(dst, n - 1, stride, itype, e);
}

static void sort_slice(double *buffer, double *dst, const double *src, size_t n, size_t stride) {
    for (size_t i = 0; i < n; ++i) {
        buffer[i] = src[i * stride];
    }

    qsort(buffer, n, sizeof(double), compare_double);

    for (size_t i = 0; i < n; ++i) {
        dst[i * stride] = buffer[i];
    }
}

static void argsort_slice(ARRAY_ELEMENT *buffer, double *dst, const double *src, size_t n, size_t stride) {
    for (size_t i = 0; i < n; ++i) {
        buffer[i].value = src[i * stride];
        /* argsort returns coordinates within the selected axis, not offsets
           into the underlying flat storage. */
        buffer[i].linear_index = (uint32_t) i;
    }

    qsort(buffer, n, sizeof(ARRAY_ELEMENT), compare_double_from_array_elem);

    for (size_t i = 0; i < n; ++i) {
        dst[i * stride] = (double) buffer[i].linear_index;
    }
}

static int32_t unary_ax_assign_shape_and_dim(CSOUND *csound, OPDS *perf_h, const size_t source_size, const uint32_t *source_shape, uint32_t *new_dim, uint32_t *new_shape, int32_t axis, CSN_UNARYOP_AX_MODE mode) {
    memcpy(new_shape, source_shape, sizeof(uint32_t) * CSN_MAX_DIMS);
    if (mode == CSN_DIFF) {
        if (axis == -1) {
            if (source_size < 2) {
                return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Diff needs at least 2 elements, got %zu", source_size);
            }

            memset(new_shape, 0, sizeof(uint32_t) * CSN_MAX_DIMS);
            new_shape[0] = (uint32_t) source_size - 1;
            *new_dim = 1U;
        } else {
            if (source_shape[axis] < 2) {
                return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Diff needs at least 2 elements along axis %d, got %u", axis, source_shape[axis]);
            }
            new_shape[axis]--;
        }
    } else if ((mode == CSN_CUMSUM || mode == CSN_CUMPROD) && axis == -1) {
        memset(new_shape, 0, sizeof(uint32_t) * CSN_MAX_DIMS);
        new_shape[0] = (uint32_t) source_size;
        *new_dim = 1U;
    }

    return OK;
}

static int32_t unary_ax_assign_value(CSN_SCRATCH *scratch, CSN_ARRAY *source_arr, CSN_ARRAY *arr, int32_t axis, ITEM_TYPE itype, double order, CSN_UNARYOP_AX_MODE mode) {
    if (axis == -1) {
        switch (mode) {
            case CSN_NORMALIZE:
                normalize_slice(arr->data, source_arr->data, source_arr->size, 1, order, itype);
                break;
            case CSN_DIFF:
                diff_slice(arr->data, source_arr->data, source_arr->size, 1, 1, itype);
                break;
            case CSN_GRADIENT:
                gradient_slice(arr->data, source_arr->data, source_arr->size, 1, itype);
                break;
            case CSN_CUMSUM:
            case CSN_CUMPROD: {
                bool is_cumsum = mode == CSN_CUMSUM;
                cumsumprod_slice(arr->data, source_arr->data, source_arr->size, 1, is_cumsum, itype);
                break;
            }
            case CSN_SORT:
                /* Skipped when the array is sorting itself: memcpy over itself
                   is undefined, and qsort then works in place. */
                if (arr->data != source_arr->data) {
                    memcpy(arr->data, source_arr->data, sizeof(double) * source_arr->size);
                }
                qsort(arr->data, source_arr->size, sizeof(double), compare_double);
                break;
            case CSN_ARGSORT:{
                if (source_arr->size == 0U)
                    break;
                argsort_slice((ARRAY_ELEMENT *) scratch->scratch, arr->data, source_arr->data, source_arr->size, 1U);
                break;
            }
        }

        return OK;
    }

    CSN_AXIS_SLICE_ITER it;
    if (AXIS_ITER_SLICE_INIT(&it, source_arr, arr, (uint32_t) axis) != OK) return NOTOK;
    if ((mode == CSN_SORT || mode == CSN_ARGSORT) && source_arr->shape[axis] == 0U) return OK;

    while (AXIS_SLICE_ITER_NEXT(&it)) {
        size_t src_base = it.src_base;
        size_t dst_base = it.dst_base;
        size_t src_stride = it.src_axis_stride;
        size_t dst_stride = it.dst_axis_stride;
        switch (mode) {
            case CSN_NORMALIZE:
                normalize_slice(arr->data + dst_base * itype, source_arr->data + src_base * itype, it.axis_size, src_stride, order, itype);
                break;
            case CSN_DIFF:
                diff_slice(arr->data + dst_base * itype, source_arr->data + src_base * itype, it.axis_size, src_stride, dst_stride, itype);
                break;
            case CSN_GRADIENT:
                gradient_slice(arr->data + dst_base * itype, source_arr->data + src_base * itype, it.axis_size, src_stride, itype);
                break;
            case CSN_CUMSUM:
            case CSN_CUMPROD: {
                bool is_cumsum = mode == CSN_CUMSUM;
                cumsumprod_slice(arr->data + dst_base * itype, source_arr->data + src_base * itype, it.axis_size, src_stride, is_cumsum, itype);
                break;
            }
            case CSN_SORT:
                sort_slice((double *) scratch->scratch, arr->data + dst_base, source_arr->data + src_base, it.axis_size, src_stride);
                break;
            case CSN_ARGSORT:
                argsort_slice((ARRAY_ELEMENT *) scratch->scratch, arr->data + dst_base, source_arr->data + src_base, it.axis_size, src_stride);
                break;
        }
    }

    return OK;
}

static int32_t unaryop_allocate_scratch(CSOUND *csound, OPDS *perf_h, bool rt_locked, CSN_SCRATCH *scratch, size_t size, ITEM_TYPE itype, CSN_UNARYOP_AX_MODE mode) {
    /* Only the two sort modes stage a slice through the scratch buffer. The
       capacity counts cells of the mode's element type, one per real item and
       two per complex one, so a source that turns complex is measured in the
       units it needs. */
    if (mode != CSN_SORT && mode != CSN_ARGSORT) {
        return OK;
    }

    size_t elem_size = mode == CSN_ARGSORT ? sizeof(ARRAY_ELEMENT) : sizeof(double);
    return csn_scratch_reserve(csound, perf_h, rt_locked, scratch, size * (size_t) itype, elem_size);
}

static CSN_AXIS_DEFAULT unary_ax_default(CSN_UNARYOP_AX_MODE mode) {
    switch (mode) {
        case CSN_DIFF:
        case CSN_SORT:
        case CSN_ARGSORT:
            return CSN_AXIS_DEFAULT_LAST;
        case CSN_NORMALIZE:
        case CSN_GRADIENT:
        case CSN_CUMSUM:
        case CSN_CUMPROD:
            return CSN_AXIS_DEFAULT_FLATTEN;
    }
    return CSN_AXIS_DEFAULT_REQUIRED;
}

static int32_t csnarray_unary_ax_helper(CSOUND *csound, const OPDS *h, CSNREF *src_ref, const MYFLT *axis_in, double order, CSNREF *out_handle, CSN_ARRAY **out_array, CSN_UNARYOP_AX_MODE mode, CSN_SCRATCH *scratch) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    if (mode == CSN_NORMALIZE && order < 1.0) {
        return csound->InitError(csound, "[csnarray] Norm order must be >= 1, got %g", order);
    }

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, src_ref->id);
    if (source_slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) src_ref->id);
        goto done;
    }

    CSN_ARRAY *source_arr = source_slot->array;
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;
    ITEM_TYPE itype = source_arr->itype;

    if ((mode == CSN_SORT || mode == CSN_ARGSORT) && itype == CSN_COMPLEX) {
        res = csound->InitError(csound, "[csnarray] Sort operation is for real array only");
        goto done;
    }

    if (mode == CSN_GRADIENT && itype == CSN_COMPLEX) {
        res = csound->InitError(csound, "[csnarray] This operation is not implemented for complex arrays");
        goto done;
    }

    CSN_AXIS_SPEC axis_spec = csn_normalize_axis(axis_in, source_ndim, unary_ax_default(mode));
    if (axis_spec.kind == CSN_AXIS_INVALID) {
        double value = axis_in == NULL ? 0.0 : (double) *axis_in;
        res = csound->InitError(csound, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: finite integers %d..%u)", value, source_ndim, -(int32_t) source_ndim, source_ndim - 1);
        goto done;
    }
    int32_t axis = axis_spec.kind == CSN_AXIS_FLATTEN ? -1 : (int32_t) axis_spec.index;

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    uint32_t new_dim = source_ndim;
    res = unary_ax_assign_shape_and_dim(csound, NULL, source_arr->size, source_shape, &new_dim, new_shape, axis, mode);
    if (res != OK) goto done;

    CSN_ARRAY *arr = source_arr;
    if (out_handle != NULL) {
        const uint32_t protect[1] = { src_ref->id };
        if (create_csnarray_locked(csound, reg, h, new_dim, new_shape, out_array, out_handle, protect, 1U, &err, source_arr->itype) != OK) {
            res = csound->InitError(csound, "[csnarray] %s", err);
            goto done;
        }
        arr = *out_array;
    }

    res = unaryop_allocate_scratch(csound, NULL, false, scratch, source_arr->size, itype, mode);
    if (res != OK) goto done;

    res = unary_ax_assign_value(scratch, source_arr, arr, axis, itype, order, mode);
    /* arr is source_arr on the in-place forms, so this is the write that a
       consumer of the source has to be told about. The out-of-place form fills
       an array it just created, which no cache can be holding yet. */
    if (res == OK && out_handle == NULL) update_array_data_version(&arr->version);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t opunary_ax_in_k_deinit(CSOUND *csound, CSN_UNARYOP_AX_IN *p) {
    if (p->scratch.scratch != NULL) {
        csound->Free(csound, p->scratch.scratch);
    }

    return OK;
}

static int32_t csnarray_unary_ax_k_init_helper(CSOUND *csound, const OPDS *h, CSNREF *src_ref, const MYFLT *axis_in, double order, CSNREF *out_handle, CSN_ARRAY **out_array, CSN_UNARYOP_AX_MODE mode, CSN_SCRATCH *scratch, K_DATA *k_data) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    /* Only csnnormalize takes an order, and it is a k-argument that normally
       still reads 0 during the init pass: reject only a value the orchestra
       really set. The perf pass checks it again before every use. */
    if (mode == CSN_NORMALIZE && order != 0.0 && order < 1.0) {
        return csound->InitError(csound, "[csnarray] Norm order must be >= 1, got %g", order);
    }

    uint32_t source_handle = src_ref->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) src_ref->id);
        goto done;
    }

    CSN_ARRAY *source_arr = source_slot->array;
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;
    ITEM_TYPE itype = source_arr->itype;

    if ((mode == CSN_SORT || mode == CSN_ARGSORT) && itype == CSN_COMPLEX) {
        res = csound->InitError(csound, "[csnarray] Sort operation is for real array only");
        goto done;
    }

    if (mode == CSN_GRADIENT && itype == CSN_COMPLEX) {
        res = csound->InitError(csound, "[csnarray] This operation is not implemented for complex arrays");
        goto done;
    }

    CSN_AXIS_SPEC axis_spec = csn_normalize_axis(axis_in, source_ndim, unary_ax_default(mode));
    if (axis_spec.kind == CSN_AXIS_INVALID) {
        double value = axis_in == NULL ? 0.0 : (double) *axis_in;
        res = csound->InitError(csound, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: finite integers %d..%u)", value, source_ndim, -(int32_t) source_ndim, source_ndim - 1);
        goto done;
    }
    int32_t axis = axis_spec.kind == CSN_AXIS_FLATTEN ? -1 : (int32_t) axis_spec.index;

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    uint32_t new_dim = source_ndim;
    res = unary_ax_assign_shape_and_dim(csound, NULL, source_arr->size, source_shape, &new_dim, new_shape, axis, mode);
    if (res != OK) goto done;

    CSN_ARRAY *arr = source_arr;
    if (out_handle != NULL) {
        const uint32_t protect[1] = { src_ref->id };
        if (create_csnarray_locked(csound, reg, h, new_dim, new_shape, out_array, out_handle, protect, 1U, &err, source_arr->itype) != OK) {
            res = csound->InitError(csound, "[csnarray] %s", err);
            goto done;
        }
        arr = *out_array;
    }

    /* Reserved for everything the source can hold without reallocating: the
       in-place form has no output shape to hold the source still, so the perf
       pass must never need more than this on a marked source. */
    res = unaryop_allocate_scratch(csound, NULL, false, scratch, source_arr->capacity, itype, mode);
    if (res != OK) goto done;

    /* csnnormalize with the order still unset leaves the output as created; the
       first triggered pass computes it. Every other mode ignores order. */
    if (mode != CSN_NORMALIZE || order >= 1.0) {
        res = unary_ax_assign_value(scratch, source_arr, arr, axis, itype, order, mode);
        if (res != OK) goto done;
        if (out_handle == NULL) update_array_data_version(&arr->version);
    }

    memset(k_data->prev_shape, 0, sizeof(k_data->prev_shape));
    memcpy(k_data->prev_shape, new_shape, sizeof(k_data->prev_shape));
    k_data->prev_ndim = new_dim;
    k_data->prev_itype = itype;
    k_data->registry = reg;
    k_data->prev_size = source_arr->size;
    if (out_handle != NULL) {
        k_data->owned_handle = out_handle->id;
    }

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

/* csnnormalize is the only mode with an order argument, so on every other form
   the trigger is bound one slot earlier — into the struct's order field. Both
   structs list the inputs in the same sequence, and INOCOUNT is the declared
   arity, so it tells the two apart. */
static inline MYFLT *unary_ax_trig(const OPDS *h, MYFLT *order, MYFLT *trig) {
    return h->optext->t.inArgCount > 3 ? trig : order;
}

/* In the k-rate unary axis family the public order is source, trigger, axis.
   The existing generic struct supplies two numeric slots named axis and order;
   the wrapper is the only place where that storage detail is interpreted. */
static inline const MYFLT *unary_ax_k_axis(const OPDS *h, MYFLT *order) {
    return h->optext->t.inArgCount > 2 ? order : NULL;
}

static inline MYFLT *unary_ax_k_trigger(MYFLT *axis_slot) {
    return axis_slot;
}

static int32_t csnarray_unary_ax_k_helper(CSOUND *csound, OPDS *h, CSNREF *src_ref, const MYFLT *axis_in, double order, CSNREF *out_handle, CSN_ARRAY **out_array, CSN_UNARYOP_AX_MODE mode, CSN_SCRATCH *scratch, K_DATA *k_data, const MYFLT *trig) {
    CSN_REGISTRY *reg = k_data->registry;
    CHECK_REGISTRY(csound, h, reg);

    if (mode == CSN_NORMALIZE && order < 1.0) {
        return csound->PerfError(csound, h, "[csnarray] Norm order must be >= 1, got %g", order);
    }

    uint32_t source_handle = src_ref->id;

    int32_t res = OK;
    const char *err = NULL;

    /* Only csngrad reads a neighbour it has already written; every other mode
       here either finishes reading the whole slice before it writes any of it
       (the norm accumulates first, the two sorts stage through the scratch) or
       writes the cell it just read (the running totals). Those are checked
       against the layout instead, once the source is resolved — which is also
       what rejects csndiff, whose result is one element shorter. */
    if (out_handle != NULL && mode == CSN_GRADIENT) {
        res = CHECK_SELF_ALIAS(csound, h, k_data, source_handle, 0);
        if (res != OK) return res;
    }

    CHECK_KTRIG(trig);

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) src_ref->id);
    }

    CSN_ARRAY *source_arr = source_slot->array;
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;
    ITEM_TYPE itype = source_arr->itype;

    if ((mode == CSN_SORT || mode == CSN_ARGSORT) && itype == CSN_COMPLEX) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, h, "[csnarray] Sort operation is for real array only");
    }

    if (mode == CSN_GRADIENT && itype == CSN_COMPLEX) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, h, "[csnarray] This operation is not implemented for complex arrays");
    }

    CSN_AXIS_SPEC axis_spec = csn_normalize_axis(axis_in, source_ndim, unary_ax_default(mode));
    if (axis_spec.kind == CSN_AXIS_INVALID) {
        double value = axis_in == NULL ? 0.0 : (double) *axis_in;
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, h, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: finite integers %d..%u)", value, source_ndim, -(int32_t) source_ndim, source_ndim - 1);
    }
    int32_t axis = axis_spec.kind == CSN_AXIS_FLATTEN ? -1 : (int32_t) axis_spec.index;
    double axis_value = (double) axis;

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    uint32_t new_dim = source_ndim;
    res = unary_ax_assign_shape_and_dim(csound, h, source_arr->size, source_shape, &new_dim, new_shape, axis, mode);
    if (res != OK) goto done;


    CSN_SLOT *out_slot = out_handle != NULL ? get_slot(reg, k_data->owned_handle) : NULL;
    if ((out_handle == NULL || out_slot != NULL)
        && CAN_REUSE_ELEMENTWISE(k_data, source_handle, source_arr, 0, NULL, out_slot != NULL ? out_slot->array : NULL, axis_value, order)) {
        if (out_handle != NULL) out_handle->id = k_data->owned_handle;
        goto done;
    }

    CSN_ARRAY *arr = source_arr;
    if (out_handle != NULL) {
        size_t requested_size = 0;
        if (get_array_size_from_shape(&requested_size, new_dim, new_shape) != OK) {
            csound->UnlockMutex(reg->mutex);
            return csound->PerfError(csound, h, "[csnarray] Invalid shape or element count exceeds the configured limit");
        }
        if (mode != CSN_GRADIENT) {
            res = CHECK_SELF_ALIAS_CELL_LOCAL(csound, h, k_data, source_handle, source_arr, new_dim, new_shape, itype);
            if (res != OK) goto done;
        }

        size_t logical_size = source_arr->size == 0 ? 0 : requested_size;
        res = NEED_TO_UPDATE_SLOT(csound, h, out_array, k_data, NULL, new_dim, new_shape, logical_size, itype, err);
        if (res != OK) goto done;
        arr = *out_array;
    }

    /* The scratch serves the output, or the source the in-place form rewrites. */
    res = unaryop_allocate_scratch(csound, h, csn_slot_rt_locked(reg, out_handle != NULL ? k_data->owned_handle : source_handle), scratch, source_arr->size, itype, mode);
    if (res != OK) goto done;

    res = unary_ax_assign_value(scratch, source_arr, arr, axis, itype, order, mode);
    if (res != OK) goto done;
    /* The out-of-place form already carries a new generation from
       NEED_TO_UPDATE_SLOT; only the in-place form writes an array nobody else
       has stamped. */
    if (out_handle == NULL) update_array_data_version(&arr->version);

    memset(k_data->prev_shape, 0, sizeof(k_data->prev_shape));
    memcpy(k_data->prev_shape, new_shape, sizeof(k_data->prev_shape));
    k_data->prev_ndim = new_dim;
    k_data->prev_itype = itype;
    k_data->prev_size = source_arr->size;
    if (out_handle != NULL) {
        out_handle->id = k_data->owned_handle;
    }
    PUBLISH_ELEMENTWISE(k_data, source_handle, source_arr, 0, NULL, out_handle != NULL ? arr : NULL, axis_value, order);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_normalize(CSOUND *csound, CSN_UNARYOP_AX *p) {
    const MYFLT *axis_in = p->INOCOUNT > 2 ? p->order : NULL;
    double order = p->INOCOUNT > 1 ? (double) *p->axis : 1.0;
    return csnarray_unary_ax_helper(csound, &p->h, p->source_handle, axis_in, order, p->handle, &p->array, CSN_NORMALIZE, &p->scratch);
}

int32_t csnarray_normalize_k_init(CSOUND *csound, CSN_UNARYOP_AX *p) {
    const MYFLT *axis_in = p->INOCOUNT > 3 ? p->trig : NULL;
    return csnarray_unary_ax_k_init_helper(csound, &p->h, p->source_handle, axis_in, (double) *p->axis, p->handle, &p->array, CSN_NORMALIZE, &p->scratch, &p->k_data);
}

int32_t csnarray_normalize_k(CSOUND *csound, CSN_UNARYOP_AX *p) {
    const MYFLT *axis_in = p->INOCOUNT > 3 ? p->trig : NULL;
    return csnarray_unary_ax_k_helper(csound, &p->h, p->source_handle, axis_in, (double) *p->axis, p->handle, &p->array, CSN_NORMALIZE, &p->scratch, &p->k_data, p->order);
}

int32_t csnarray_normalize_in(CSOUND *csound, CSN_UNARYOP_AX_IN *p) {
    const MYFLT *axis_in = p->INOCOUNT > 2 ? p->order : NULL;
    double order = p->INOCOUNT > 1 ? (double) *p->axis : 1.0;
    return csnarray_unary_ax_helper(csound, &p->h, p->source_handle, axis_in, order, NULL, NULL, CSN_NORMALIZE, &p->scratch);
}

int32_t csnarray_normalize_in_k_init(CSOUND *csound, CSN_UNARYOP_AX_IN *p) {
    const MYFLT *axis_in = p->INOCOUNT > 3 ? p->trig : NULL;
    return csnarray_unary_ax_k_init_helper(csound, &p->h, p->source_handle, axis_in, (double) *p->axis, NULL, NULL, CSN_NORMALIZE, &p->scratch, &p->k_data);
}

int32_t csnarray_normalize_in_k(CSOUND *csound, CSN_UNARYOP_AX_IN *p) {
    const MYFLT *axis_in = p->INOCOUNT > 3 ? p->trig : NULL;
    return csnarray_unary_ax_k_helper(csound, &p->h, p->source_handle, axis_in, (double) *p->axis, NULL, NULL, CSN_NORMALIZE, &p->scratch, &p->k_data, p->order);
}

int32_t csnarray_sort_in(CSOUND *csound, CSN_UNARYOP_AX_IN *p) {
    const MYFLT *axis_in = p->INOCOUNT > 1 ? p->axis : NULL;
    return csnarray_unary_ax_helper(csound, &p->h, p->source_handle, axis_in, 0.0, NULL, NULL, CSN_SORT, &p->scratch);
}

int32_t csnarray_sort_in_k_init(CSOUND *csound, CSN_UNARYOP_AX_IN *p) {
    return csnarray_unary_ax_k_init_helper(csound, &p->h, p->source_handle, unary_ax_k_axis(&p->h, p->order), 0.0, NULL, NULL, CSN_SORT, &p->scratch, &p->k_data);
}

int32_t csnarray_sort_in_k(CSOUND *csound, CSN_UNARYOP_AX_IN *p) {
    return csnarray_unary_ax_k_helper(csound, &p->h, p->source_handle, unary_ax_k_axis(&p->h, p->order), 0.0, NULL, NULL, CSN_SORT, &p->scratch, &p->k_data, unary_ax_k_trigger(p->axis));
}

int32_t csnarray_distance(CSOUND *csound, CSN_BINOP_HH_SCALAR *p) {
    double dist_order = (double) *p->arg_a;
    return csnarray_scalar_helper(csound, p, CSN_DISTANCE, dist_order);
}

/* The Minkowski order is a k-argument here, so during the init pass it normally
   still reads 0, which no validation can accept. Stash the registry the perf
   pass needs, leave the scalar at 0, and let the first pass compute it. */
int32_t csnarray_distance_k_init(CSOUND *csound, CSN_BINOP_HH_SCALAR *p) {
    double dist_order = (double) *p->arg_a;
    if (dist_order >= 1.0) {
        return csnarray_distance(csound, p);
    }

    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    p->registry = reg;
    *p->value = FL(0.0);
    return OK;
}

int32_t csnarray_distance_k(CSOUND *csound, CSN_BINOP_HH_SCALAR *p) {
    double dist_order = (double) *p->arg_a;
    return csnarray_scalar_helper_k(csound, p, CSN_DISTANCE, dist_order, p->arg_b);
}

int32_t csnarray_pair_distance(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_vec_helper(csound, p, CSN_PAIR_DISTANCE);
}

int32_t csnarray_pair_distance_k_init(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_vec_k_init_helper(csound, p, CSN_PAIR_DISTANCE);
}

int32_t csnarray_pair_distance_k(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_vec_k_helper(csound, p, CSN_PAIR_DISTANCE);
}

int32_t csnarray_diff(CSOUND *csound, CSN_UNARYOP_AX *p) {
    const MYFLT *axis_in = p->INOCOUNT > 1 ? p->axis : NULL;
    return csnarray_unary_ax_helper(csound, &p->h, p->source_handle, axis_in, 0.0, p->handle, &p->array, CSN_DIFF, &p->scratch);
}

int32_t csnarray_diff_k_init(CSOUND *csound, CSN_UNARYOP_AX *p) {
    return csnarray_unary_ax_k_init_helper(csound, &p->h, p->source_handle, unary_ax_k_axis(&p->h, p->order), 0.0, p->handle, &p->array, CSN_DIFF, &p->scratch, &p->k_data);
}

int32_t csnarray_diff_k(CSOUND *csound, CSN_UNARYOP_AX *p) {
    return csnarray_unary_ax_k_helper(csound, &p->h, p->source_handle, unary_ax_k_axis(&p->h, p->order), 0.0, p->handle, &p->array, CSN_DIFF, &p->scratch, &p->k_data, unary_ax_k_trigger(p->axis));
}

int32_t csnarray_gradient(CSOUND *csound, CSN_UNARYOP_AX *p) {
    const MYFLT *axis_in = p->INOCOUNT > 1 ? p->axis : NULL;
    return csnarray_unary_ax_helper(csound, &p->h, p->source_handle, axis_in, 0.0, p->handle, &p->array, CSN_GRADIENT, &p->scratch);
}

int32_t csnarray_gradient_k_init(CSOUND *csound, CSN_UNARYOP_AX *p) {
    return csnarray_unary_ax_k_init_helper(csound, &p->h, p->source_handle, unary_ax_k_axis(&p->h, p->order), 0.0, p->handle, &p->array, CSN_GRADIENT, &p->scratch, &p->k_data);
}

int32_t csnarray_gradient_k(CSOUND *csound, CSN_UNARYOP_AX *p) {
    return csnarray_unary_ax_k_helper(csound, &p->h, p->source_handle, unary_ax_k_axis(&p->h, p->order), 0.0, p->handle, &p->array, CSN_GRADIENT, &p->scratch, &p->k_data, unary_ax_k_trigger(p->axis));
}

int32_t csnarray_cumsum(CSOUND *csound, CSN_UNARYOP_AX *p) {
    const MYFLT *axis_in = p->INOCOUNT > 1 ? p->axis : NULL;
    return csnarray_unary_ax_helper(csound, &p->h, p->source_handle, axis_in, 0.0, p->handle, &p->array, CSN_CUMSUM, &p->scratch);
}

int32_t csnarray_cumsum_k_init(CSOUND *csound, CSN_UNARYOP_AX *p) {
    return csnarray_unary_ax_k_init_helper(csound, &p->h, p->source_handle, unary_ax_k_axis(&p->h, p->order), 0.0, p->handle, &p->array, CSN_CUMSUM, &p->scratch, &p->k_data);
}

int32_t csnarray_cumsum_k(CSOUND *csound, CSN_UNARYOP_AX *p) {
    return csnarray_unary_ax_k_helper(csound, &p->h, p->source_handle, unary_ax_k_axis(&p->h, p->order), 0.0, p->handle, &p->array, CSN_CUMSUM, &p->scratch, &p->k_data, unary_ax_k_trigger(p->axis));
}

int32_t csnarray_cumprod(CSOUND *csound, CSN_UNARYOP_AX *p) {
    const MYFLT *axis_in = p->INOCOUNT > 1 ? p->axis : NULL;
    return csnarray_unary_ax_helper(csound, &p->h, p->source_handle, axis_in, 0.0, p->handle, &p->array, CSN_CUMPROD, &p->scratch);
}

int32_t csnarray_cumprod_k_init(CSOUND *csound, CSN_UNARYOP_AX *p) {
    return csnarray_unary_ax_k_init_helper(csound, &p->h, p->source_handle, unary_ax_k_axis(&p->h, p->order), 0.0, p->handle, &p->array, CSN_CUMPROD, &p->scratch, &p->k_data);
}

int32_t csnarray_cumprod_k(CSOUND *csound, CSN_UNARYOP_AX *p) {
    return csnarray_unary_ax_k_helper(csound, &p->h, p->source_handle, unary_ax_k_axis(&p->h, p->order), 0.0, p->handle, &p->array, CSN_CUMPROD, &p->scratch, &p->k_data, unary_ax_k_trigger(p->axis));
}

int32_t csnarray_sort(CSOUND *csound, CSN_UNARYOP_AX *p) {
    const MYFLT *axis_in = p->INOCOUNT > 1 ? p->axis : NULL;
    return csnarray_unary_ax_helper(csound, &p->h, p->source_handle, axis_in, 0.0, p->handle, &p->array, CSN_SORT, &p->scratch);
}

int32_t csnarray_sort_k_init(CSOUND *csound, CSN_UNARYOP_AX *p) {
    return csnarray_unary_ax_k_init_helper(csound, &p->h, p->source_handle, unary_ax_k_axis(&p->h, p->order), 0.0, p->handle, &p->array, CSN_SORT, &p->scratch, &p->k_data);
}

int32_t csnarray_sort_k(CSOUND *csound, CSN_UNARYOP_AX *p) {
    return csnarray_unary_ax_k_helper(csound, &p->h, p->source_handle, unary_ax_k_axis(&p->h, p->order), 0.0, p->handle, &p->array, CSN_SORT, &p->scratch, &p->k_data, unary_ax_k_trigger(p->axis));
}

int32_t csnarray_argsort(CSOUND *csound, CSN_UNARYOP_AX *p) {
    const MYFLT *axis_in = p->INOCOUNT > 1 ? p->axis : NULL;
    return csnarray_unary_ax_helper(csound, &p->h, p->source_handle, axis_in, 0.0, p->handle, &p->array, CSN_ARGSORT, &p->scratch);
}

int32_t csnarray_argsort_k_init(CSOUND *csound, CSN_UNARYOP_AX *p) {
    return csnarray_unary_ax_k_init_helper(csound, &p->h, p->source_handle, unary_ax_k_axis(&p->h, p->order), 0.0, p->handle, &p->array, CSN_ARGSORT, &p->scratch, &p->k_data);
}

int32_t csnarray_argsort_k(CSOUND *csound, CSN_UNARYOP_AX *p) {
    return csnarray_unary_ax_k_helper(csound, &p->h, p->source_handle, unary_ax_k_axis(&p->h, p->order), 0.0, p->handle, &p->array, CSN_ARGSORT, &p->scratch, &p->k_data, unary_ax_k_trigger(p->axis));
}

int32_t csnarray_matmul_scalar(CSOUND *csound, CSN_BINOP_HH_SCALAR *p) {
    return csnarray_scalar_helper(csound, p, CSN_DOT_SCALAR, 0.0);
}

int32_t csnarray_matmul_scalar_k(CSOUND *csound, CSN_BINOP_HH_SCALAR *p) {
    return csnarray_scalar_helper_k(csound, p, CSN_DOT_SCALAR, 0.0, p->arg_a);
}

static int32_t matmul_body(CSOUND *csound, OPDS *perf_h, CSN_REGISTRY *reg, CSN_ARRAY **source_array_a,CSN_ARRAY **source_array_b, uint32_t source_handle_a, uint32_t source_handle_b) {
    CSN_SLOT *source_slot_a = get_slot(reg, source_handle_a);
    if (source_slot_a == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle_a);
    }

    CSN_SLOT *source_slot_b = get_slot(reg, source_handle_b);
    if (source_slot_b == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle_b);
    }

    *source_array_a = source_slot_a->array;
    *source_array_b = source_slot_b->array;
    CSN_ARRAY *source_arr_a = *source_array_a;
    CSN_ARRAY *source_arr_b = *source_array_b;

    if (source_arr_a->itype != source_arr_b->itype) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Element type mismatch: one array is real and the other complex");
    }

    return OK;
}

typedef struct {
    uint32_t a_shape[CSN_MAX_DIMS];
    uint32_t b_shape[CSN_MAX_DIMS];
    size_t a_strides[CSN_MAX_DIMS];
    size_t b_strides[CSN_MAX_DIMS];
    uint32_t batch_shape[CSN_MAX_DIMS];
    uint32_t new_shape[CSN_MAX_DIMS];
    uint32_t a_dim;
    uint32_t b_dim;
    uint32_t a_batch_ndim;
    uint32_t b_batch_ndim;
    uint32_t batch_ndim;
    uint32_t new_dim;
    uint32_t rows;
    uint32_t inner;
    uint32_t cols;
} CSN_MATMUL_LAYOUT;

static int32_t matmul_assign_shape_and_dim(CSOUND *csound, OPDS *perf_h, CSN_ARRAY *source_arr_a, CSN_ARRAY *source_arr_b, CSN_MATMUL_LAYOUT *box) {
    uint32_t source_a_dim = source_arr_a->ndim;
    uint32_t source_b_dim = source_arr_b->ndim;
    bool a_promoted = source_a_dim == 1;
    bool b_promoted = source_b_dim == 1;

    if (a_promoted) {
        box->a_dim = 2U;
        box->a_shape[0] = 1U;
        box->a_shape[1] = source_arr_a->shape[0];
        box->a_strides[0] = 0;
        box->a_strides[1] = source_arr_a->strides[0];
    } else {
        box->a_dim = source_a_dim;
        memcpy(box->a_shape, source_arr_a->shape, sizeof(uint32_t) * box->a_dim);
        memcpy(box->a_strides, source_arr_a->strides, sizeof(size_t) * box->a_dim);
    }

    if (b_promoted) {
        box->b_dim = 2U;
        box->b_shape[0] = source_arr_b->shape[0];
        box->b_shape[1] = 1U;
        box->b_strides[0] = source_arr_b->strides[0];
        box->b_strides[1] = 0;
    } else {
        box->b_dim = source_b_dim;
        memcpy(box->b_shape, source_arr_b->shape, sizeof(uint32_t) * box->b_dim);
        memcpy(box->b_strides, source_arr_b->strides, sizeof(size_t) * box->b_dim);
    }

    box->rows = box->a_shape[box->a_dim - 2];
    box->inner = box->a_shape[box->a_dim - 1];
    box->cols = box->b_shape[box->b_dim - 1];

    if (box->inner != box->b_shape[box->b_dim - 2]) {
        char abuf[CSN_SHAPE_STR_MAX], bbuf[CSN_SHAPE_STR_MAX];
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Shapes %s and %s are not valid for a matrix product: the last axis of the first (%u) must match the second-to-last of the second (%u)", shape_str(abuf, sizeof(abuf), source_arr_a->shape, source_a_dim), shape_str(bbuf, sizeof(bbuf), source_arr_b->shape, source_b_dim), box->inner, box->b_shape[box->b_dim - 2]);
    }

    box->a_batch_ndim = box->a_dim - 2;
    box->b_batch_ndim = box->b_dim - 2;
    box->batch_ndim = box->a_batch_ndim > box->b_batch_ndim ? box->a_batch_ndim : box->b_batch_ndim;

    for (uint32_t i = 0; i < box->batch_ndim; ++i) {
        uint32_t ea = i < box->a_batch_ndim ? box->a_shape[box->a_batch_ndim - 1 - i] : 1U;
        uint32_t eb = i < box->b_batch_ndim ? box->b_shape[box->b_batch_ndim - 1 - i] : 1U;
        if (ea != eb && ea != 1U && eb != 1U) {
            char abuf[CSN_SHAPE_STR_MAX], bbuf[CSN_SHAPE_STR_MAX];
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Batch shapes %s and %s cannot be broadcast together: outside the last two axes, aligned from the right, each pair must match or be 1", shape_str(abuf, sizeof(abuf), source_arr_a->shape, source_a_dim), shape_str(bbuf, sizeof(bbuf), source_arr_b->shape, source_b_dim));
        }
        box->batch_shape[box->batch_ndim - 1 - i] = ea == 1U ? eb : ea;
    }

    uint32_t out_ndim = 0;
    for (uint32_t i = 0; i < box->batch_ndim; ++i) {
        box->new_shape[out_ndim++] = box->batch_shape[i];
    }
    if (!a_promoted) box->new_shape[out_ndim++] = box->rows;
    if (!b_promoted) box->new_shape[out_ndim++] = box->cols;

    if (out_ndim > CSN_MAX_DIMS) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] The result would have %u dimensions, the maximum is %d", out_ndim, CSN_MAX_DIMS);
    }
    box->new_dim = out_ndim;
    return OK;
}

static int32_t matmul_assign_value(CSN_ARRAY *source_arr_a, CSN_ARRAY *source_arr_b, CSN_ARRAY *arr, CSN_MATMUL_LAYOUT *box) {
    if (arr->size == 0) return OK;
    size_t batch_count = 1;
    for (uint32_t i = 0; i < box->batch_ndim; ++i) {
        batch_count *= box->batch_shape[i];
    }

    size_t a_row_stride = box->a_strides[box->a_dim - 2];
    size_t a_col_stride = box->a_strides[box->a_dim - 1];
    size_t b_row_stride = box->b_strides[box->b_dim - 2];
    size_t b_col_stride = box->b_strides[box->b_dim - 1];

    uint32_t iter_shape[CSN_MAX_DIMS] = {1};
    size_t plan[CSN_MAX_BROADCAST_INPUTS][CSN_MAX_DIMS] = {{0}};
    for (uint32_t d = 0; d < box->batch_ndim; ++d) iter_shape[d] = box->batch_shape[d];
    for (uint32_t i = 0; i < box->a_batch_ndim; ++i) {
        uint32_t d = box->batch_ndim - box->a_batch_ndim + i;
        if (box->a_shape[i] != 1U) plan[0][d] = box->a_strides[i];
    }
    for (uint32_t i = 0; i < box->b_batch_ndim; ++i) {
        uint32_t d = box->batch_ndim - box->b_batch_ndim + i;
        if (box->b_shape[i] != 1U) plan[1][d] = box->b_strides[i];
    }
    CSN_BROADCAST_ITER it;
    if (BROADCAST_ITER_INIT(&it, box->batch_ndim == 0 ? 1 : box->batch_ndim, iter_shape, batch_count, plan, 2) != OK) return NOTOK;
    while (BROADCAST_ITER_NEXT(&it)) {
        size_t batch = it.linear_index;
        size_t a_base = it.offsets[0];
        size_t b_base = it.offsets[1];

        for (uint32_t r = 0; r < box->rows; ++r) {
            for (uint32_t c = 0; c < box->cols; ++c) {
                /* Bilinear, as numpy.matmul */
                CSN_COMPLEXDAT acc = { 0.0, 0.0 };
                for (uint32_t k = 0; k < box->inner; ++k) {
                    size_t off_a = a_base + (size_t) r * a_row_stride + (size_t) k * a_col_stride;
                    size_t off_b = b_base + (size_t) k * b_row_stride + (size_t) c * b_col_stride;
                    CSN_COMPLEXDAT prod = {0};
                    complex_prod(&prod, item_at(source_arr_a, off_a), item_at(source_arr_b, off_b));
                    complex_add(&acc, acc, prod);
                }
                item_set(arr, (batch * box->rows + r) * box->cols + c, acc);
            }
        }
    }
    return OK;
}

int32_t csnarray_matmul(CSOUND *csound, CSN_BINOP_HH *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle_a = p->source_handle_a->id;
    uint32_t source_handle_b = p->source_handle_b->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr_a = NULL;
    CSN_ARRAY *source_arr_b = NULL;
    res = matmul_body(csound, NULL, reg, &source_arr_a, &source_arr_b, source_handle_a, source_handle_b);
    if (res != OK) goto done;

    uint32_t source_a_dim = source_arr_a->ndim;
    uint32_t source_b_dim = source_arr_b->ndim;
    ITEM_TYPE itype = source_arr_a->itype;

    if (source_a_dim == 1 && source_b_dim == 1) {
        res = csound->InitError(csound, "[csnarray] The matrix product of two 1-D arrays is a scalar; declare the output as i, not as a handle");
        goto done;
    }

    CSN_MATMUL_LAYOUT box = {0};
    res = matmul_assign_shape_and_dim(csound, NULL, source_arr_a, source_arr_b, &box);
    if (res != OK) goto done;

    const uint32_t protect[2] = { source_handle_a, source_handle_b };
    if (create_csnarray_locked(csound, reg, &p->h, box.new_dim, box.new_shape, &p->array, p->handle, protect, 2U, &err, itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;
    if (source_arr_a->size == 0 || source_arr_b->size == 0) arr->size = 0;
    if (matmul_assign_value(source_arr_a, source_arr_b, arr, &box) != OK) {
        res = csound->InitError(csound, "[csnarray] Invalid matmul batch iteration shape");
        goto done;
    }
    SET_KDATA_BEGIN(p, reg);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_matmul_k(CSOUND *csound, CSN_BINOP_HH *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REG_HANDLE(csound, &p->h, reg, p->k_data.owned_handle);

    uint32_t source_handle_a = p->source_handle_a->id;
    uint32_t source_handle_b = p->source_handle_b->id;

    CHECK_KTRIG(p->trig);

    int32_t res = OK;
    const char *err = NULL;

    res = CHECK_SELF_ALIAS(csound, &p->h, &p->k_data, source_handle_a, source_handle_b);
    if (res != OK) return res;

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr_a = NULL;
    CSN_ARRAY *source_arr_b = NULL;
    res = matmul_body(csound, &p->h, reg, &source_arr_a, &source_arr_b, source_handle_a, source_handle_b);
    if (res != OK) goto done;

    uint32_t source_a_dim = source_arr_a->ndim;
    uint32_t source_b_dim = source_arr_b->ndim;
    ITEM_TYPE itype = source_arr_a->itype;

    if (source_a_dim == 1 && source_b_dim == 1) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] The matrix product of two 1-D arrays is a scalar; declare the output as i, not as a handle");
    }

    CSN_MATMUL_LAYOUT box = {0};
    res = matmul_assign_shape_and_dim(csound, &p->h, source_arr_a, source_arr_b, &box);
    if (res != OK) goto done;

    size_t requested_size = 0;
    if (get_array_size_from_shape(&requested_size, box.new_dim, box.new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSN_SLOT *out_slot = get_slot(reg, p->k_data.owned_handle);
    if (out_slot != NULL && CAN_REUSE_ELEMENTWISE(&p->k_data, source_handle_a, source_arr_a, source_handle_b, source_arr_b, out_slot->array, 0.0, 0.0)) {
        p->handle->id = p->k_data.owned_handle;
        goto done;
    }

    CSN_ARRAY *arr = p->array;
    size_t logical_size = (source_arr_a->size == 0 && source_arr_b->size == 0) ? 0 : requested_size;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, box.new_dim, box.new_shape, logical_size, itype, err);
    if (res != OK) goto done;

    if (matmul_assign_value(source_arr_a, source_arr_b, arr, &box) != OK) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Invalid matmul batch iteration shape");
        goto done;
    }
    SET_KDATA_END(p, box.new_shape, box.new_dim, itype);
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle_a, source_arr_a, source_handle_b, source_arr_b, arr, 0.0, 0.0);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_angle_distance(CSOUND *csound, CSN_BINOP_HH_SCALAR *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle_a = (uint32_t) p->source_handle_a->id;
    uint32_t source_handle_b = (uint32_t) p->source_handle_b->id;

    /* Stashed before the zero-length shortcut below, which is a warning and not
       an error: leaving it to the tail would let a note that starts with a zero
       vector run on with a NULL registry, and every later k pass would fail. */
    p->registry = reg;

    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot_a = get_slot(reg, source_handle_a);
    if (slot_a == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle_a);
        goto done;
    }

    CSN_SLOT *slot_b = get_slot(reg, source_handle_b);
    if (slot_b == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle_b);
        goto done;
    }

    CSN_ARRAY *source_arr_a = slot_a->array;
    CSN_ARRAY *source_arr_b = slot_b->array;
    uint32_t *source_shape_a = source_arr_a->shape;
    uint32_t *source_shape_b = source_arr_b->shape;
    size_t source_dim_a = source_arr_a->ndim;
    size_t source_dim_b = source_arr_b->ndim;

    if (source_dim_a != source_dim_b || memcmp(source_shape_a, source_shape_b, sizeof(uint32_t) * CSN_MAX_DIMS) != 0) {
        char abuf[CSN_SHAPE_STR_MAX], bbuf[CSN_SHAPE_STR_MAX];
        res = csound->InitError(csound, "[csnarray] Both arrays must have the same shape, got %s and %s", shape_str(abuf, sizeof(abuf), source_shape_a, (uint32_t) source_dim_a), shape_str(bbuf, sizeof(bbuf), source_shape_b, (uint32_t) source_dim_b));
        goto done;
    }

    if (source_arr_a->itype != source_arr_b->itype) {
        res = csound->InitError(csound, "[csnarray] Element type mismatch: one array is real and the other complex");
        goto done;
    }

    CSN_COMPLEXDAT dot_ab = { 0.0, 0.0 };
    double norm_a = 0.0;
    double norm_b = 0.0;
    for (size_t i = 0; i < source_arr_a->size; i++) {
        CSN_COMPLEXDAT a = item_at(source_arr_a, i);
        CSN_COMPLEXDAT b = item_at(source_arr_b, i);
        CSN_COMPLEXDAT bconj = { b.re, -b.im };
        CSN_COMPLEXDAT t = {0};
        complex_prod(&t, a, bconj);
        complex_add(&dot_ab, dot_ab, t);
        norm_a += a.re * a.re + a.im * a.im;
        norm_b += b.re * b.re + b.im * b.im;
    }

    if (norm_a == 0.0 || norm_b == 0.0) {
        csound->Message(csound, "[csnarray] Angle undefined for zero-length vectors");
        *p->value = (MYFLT) NAN;
        goto done;
    }

    norm_a = sqrt(norm_a);
    norm_b = sqrt(norm_b);

    double dot = source_arr_a->itype == CSN_COMPLEX ? hypot(dot_ab.re, dot_ab.im) : dot_ab.re;
    double c = dot / (norm_a * norm_b);
    c = c > 1.0 ? 1.0 : c;
    c = c < -1.0 ? -1.0 : c;
    double theta = acos(c);

    *p->value = (MYFLT) theta;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_angle_distance_k(CSOUND *csound, CSN_BINOP_HH_SCALAR *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    uint32_t source_handle_a = (uint32_t) p->source_handle_a->id;
    uint32_t source_handle_b = (uint32_t) p->source_handle_b->id;

    CHECK_KTRIG(p->arg_a);

    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot_a = get_slot(reg, source_handle_a);
    if (slot_a == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle_a);
    }

    CSN_SLOT *slot_b = get_slot(reg, source_handle_b);
    if (slot_b == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle_b);
    }

    CSN_ARRAY *source_arr_a = slot_a->array;
    CSN_ARRAY *source_arr_b = slot_b->array;

    if (CAN_REUSE_ELEMENTWISE(&p->k_data, source_handle_a, source_arr_a, source_handle_b, source_arr_b, NULL, 0.0, 0.0)) {
        goto done;
    }

    uint32_t *source_shape_a = source_arr_a->shape;
    uint32_t *source_shape_b = source_arr_b->shape;
    size_t source_dim_a = source_arr_a->ndim;
    size_t source_dim_b = source_arr_b->ndim;

    if (source_dim_a != source_dim_b || memcmp(source_shape_a, source_shape_b, sizeof(uint32_t) * CSN_MAX_DIMS) != 0) {
        char abuf[CSN_SHAPE_STR_MAX], bbuf[CSN_SHAPE_STR_MAX];
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Both arrays must have the same shape, got %s and %s", shape_str(abuf, sizeof(abuf), source_shape_a, (uint32_t) source_dim_a), shape_str(bbuf, sizeof(bbuf), source_shape_b, (uint32_t) source_dim_b));
    }

    if (source_arr_a->itype != source_arr_b->itype) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Element type mismatch: one array is real and the other complex");
    }

    CSN_COMPLEXDAT dot_ab = { 0.0, 0.0 };
    double norm_a = 0.0;
    double norm_b = 0.0;
    for (size_t i = 0; i < source_arr_a->size; i++) {
        CSN_COMPLEXDAT a = item_at(source_arr_a, i);
        CSN_COMPLEXDAT b = item_at(source_arr_b, i);
        CSN_COMPLEXDAT bconj = { b.re, -b.im };
        CSN_COMPLEXDAT t = {0};
        complex_prod(&t, a, bconj);
        complex_add(&dot_ab, dot_ab, t);
        norm_a += a.re * a.re + a.im * a.im;
        norm_b += b.re * b.re + b.im * b.im;
    }

    if (norm_a == 0.0 || norm_b == 0.0) {
        csound->Message(csound, "[csnarray] Angle undefined for zero-length vectors");
        *p->value = (MYFLT) NAN;
        goto done;
    }

    norm_a = sqrt(norm_a);
    norm_b = sqrt(norm_b);

    double dot = source_arr_a->itype == CSN_COMPLEX ? hypot(dot_ab.re, dot_ab.im) : dot_ab.re;
    double c = dot / (norm_a * norm_b);
    c = c > 1.0 ? 1.0 : c;
    c = c < -1.0 ? -1.0 : c;
    double theta = acos(c);

    *p->value = (MYFLT) theta;
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle_a, source_arr_a, source_handle_b, source_arr_b, NULL, 0.0, 0.0);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_trace_impl(CSOUND *csound, CSNREF *src_ref, MYFLT *out_value, COMPLEXDAT *out_complex, CSN_REGISTRY **registry) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = (uint32_t) src_ref->id;
    int32_t res = OK;

    *registry = reg;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot_a = get_slot(reg, source_handle);
    if (slot_a == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = slot_a->array;
    uint32_t dim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    if (dim != 2) {
        res = csound->InitError(csound, "[csnarray] Trace needs a 2-D matrix, got %u-D", dim);
        goto done;
    }

    uint32_t n = source_shape[0] < source_shape[1] ? source_shape[0] : source_shape[1];
    CSN_COMPLEXDAT sum = { 0.0, 0.0 };
    for (uint32_t i = 0; i < n; i++) {
        size_t off = (size_t) i * (source_arr->strides[0] + source_arr->strides[1]);
        complex_add(&sum, sum, item_at(source_arr, off));
    }

    if (source_arr->itype == CSN_COMPLEX) {
        if (out_complex == NULL) {
            res = csound->InitError(csound, "[csnarray] Handle holds a complex array; declare the result as :Complex;");
            goto done;
        }
        out_complex->real = (MYFLT) sum.re;
        out_complex->imag = (MYFLT) sum.im;
        out_complex->isPolar = 0;
    } else {
        if (out_value == NULL) {
            res = csound->InitError(csound, "[csnarray] Handle holds a real array; declare the result as i");
            goto done;
        }
        *out_value = (MYFLT) sum.re;
    }

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_trace_impl_k(CSOUND *csound, OPDS *perf_h, CSNREF *src_ref, MYFLT *out_value, COMPLEXDAT *out_complex, CSN_REGISTRY **registry, MYFLT *trig, K_DATA *k_data) {
    CSN_REGISTRY *reg = *registry;
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = (uint32_t) src_ref->id;
    int32_t res = OK;

    CHECK_KTRIG(trig);

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot_a = get_slot(reg, source_handle);
    if (slot_a == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
    }

    CSN_ARRAY *source_arr = slot_a->array;
    uint32_t dim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    if (dim != 2) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, perf_h, "[csnarray] Trace needs a 2-D matrix, got %u-D", dim);
    }

    if (CAN_REUSE_ELEMENTWISE(k_data, source_handle, source_arr, 0, NULL, NULL, 0.0, 0.0)) {
        csound->UnlockMutex(reg->mutex);
        return res;
    }

    uint32_t n = source_shape[0] < source_shape[1] ? source_shape[0] : source_shape[1];
    CSN_COMPLEXDAT sum = { 0.0, 0.0 };
    for (uint32_t i = 0; i < n; i++) {
        size_t off = (size_t) i * (source_arr->strides[0] + source_arr->strides[1]);
        complex_add(&sum, sum, item_at(source_arr, off));
    }

    if (source_arr->itype == CSN_COMPLEX) {
        if (out_complex == NULL) {
            csound->UnlockMutex(reg->mutex);
            return csound->PerfError(csound, perf_h, "[csnarray] Handle holds a complex array; declare the result as :Complex;");
        }
        out_complex->real = (MYFLT) sum.re;
        out_complex->imag = (MYFLT) sum.im;
        out_complex->isPolar = 0;
    } else {
        if (out_value == NULL) {
            csound->UnlockMutex(reg->mutex);
            return csound->PerfError(csound, perf_h, "[csnarray] Handle holds a real array; declare the result as i");
        }
        *out_value = (MYFLT) sum.re;
    }
    PUBLISH_ELEMENTWISE(k_data, source_handle, source_arr, 0, NULL, NULL, 0.0, 0.0);

    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_trace(CSOUND *csound, CSN_UNARYOP_SCALAR *p) {
    return csnarray_trace_impl(csound, p->source_handle, p->value, NULL, &p->registry);
}

int32_t csnarray_trace_k(CSOUND *csound, CSN_UNARYOP_SCALAR *p) {
    return csnarray_trace_impl_k(csound, &p->h, p->source_handle, p->value, NULL, &p->registry, p->trig, &p->k_data);
}

int32_t csnarray_tracecomp(CSOUND *csound, CSN_UNARYOPCOMPLEX_SCALAR *p) {
    return csnarray_trace_impl(csound, p->source_handle, NULL, p->value, &p->registry);
}

int32_t csnarray_tracecomp_k(CSOUND *csound, CSN_UNARYOPCOMPLEX_SCALAR *p) {
    return csnarray_trace_impl_k(csound, &p->h, p->source_handle, NULL, p->value, &p->registry, p->trig, &p->k_data);
}

static int32_t diag_body(CSOUND *csound, OPDS *perf_h, CSN_REGISTRY *reg, CSN_ARRAY **source_array, uint32_t source_handle) {
    CSN_SLOT *slot_a = get_slot(reg, source_handle);
    if (slot_a == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
    }

    *source_array = slot_a->array;
    CSN_ARRAY *source_arr = *source_array;
    uint32_t dim = source_arr->ndim;

    if (dim == 0 || dim > 2) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Diag needs a 1-D or 2-D array, got %u-D", dim);
    }

    return OK;
}

/* new_dim is the rank of the destination, the opposite of the source's: 1 means
   the diagonal of a matrix is being read out, 2 means a vector is being spread
   over the diagonal of a fresh matrix. */
static void diag_assign_value(CSN_ARRAY *source_arr, CSN_ARRAY *arr, uint32_t new_dim, uint32_t *new_shape) {
    for (uint32_t i = 0; i < new_shape[0]; i++) {
        if (new_dim == 1) {
            size_t off = (size_t) i * (source_arr->strides[0] + source_arr->strides[1]);
            item_set(arr, i, item_at(source_arr, off));
        } else {
            size_t off = (size_t) i * (arr->strides[0] + arr->strides[1]);
            item_set(arr, off, item_at(source_arr, i));
        }
    }
}

int32_t csnarray_diag(CSOUND *csound, CSN_UNARYOP *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = (uint32_t) p->source_handle->id;
    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    res = diag_body(csound, NULL, reg, &source_arr, source_handle);
    if (res != OK) goto done;

    uint32_t dim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    uint32_t new_dim = dim == 2U ? 1U : 2U;
    if (dim == 1) {
        new_shape[0] = source_shape[0];
        new_shape[1] = source_shape[0];
    } else {
        new_shape[0] = source_shape[0] < source_shape[1] ? source_shape[0] : source_shape[1];
    }

    const uint32_t protect[1] = { source_handle };
    if (create_csnarray_locked(csound, reg, &p->h, new_dim, new_shape, &p->array, p->handle, protect, 1U, &err, source_arr->itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;
    diag_assign_value(source_arr, arr, new_dim, new_shape);
    SET_KDATA_BEGIN(p, reg);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_diag_k(CSOUND *csound, CSN_UNARYOP *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REG_HANDLE(csound, &p->h, reg, p->k_data.owned_handle);

    uint32_t source_handle = (uint32_t) p->source_handle->id;

    int32_t res = OK;
    const char *err = NULL;
    res = CHECK_SELF_ALIAS(csound, &p->h, &p->k_data, source_handle, 0);
    if (res != OK) return res;

    CHECK_KTRIG(p->trig);

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    res = diag_body(csound, &p->h, reg, &source_arr, source_handle);
    if (res != OK) goto done;

    uint32_t dim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    uint32_t new_dim = dim == 2U ? 1U : 2U;
    if (dim == 1) {
        new_shape[0] = source_shape[0];
        new_shape[1] = source_shape[0];
    } else {
        new_shape[0] = source_shape[0] < source_shape[1] ? source_shape[0] : source_shape[1];
    }

    size_t requested_size = 0;
    if (get_array_size_from_shape(&requested_size, new_dim, new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSN_ARRAY *arr = p->array;
    size_t logical_size = source_arr->size == 0 ? 0 : requested_size;
    CSN_SLOT *reuse_slot = get_slot(p->k_data.registry, p->k_data.owned_handle);
    if (reuse_slot != NULL && CAN_REUSE_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, reuse_slot->array, 0.0, 0.0)) {
        p->handle->id = p->k_data.owned_handle;
        goto done;
    }

    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, new_dim, new_shape, logical_size, source_arr->itype, err);
    if (res != OK) goto done;

    diag_assign_value(source_arr, arr, new_dim, new_shape);
    SET_KDATA_END(p, new_shape, new_dim, source_arr->itype);
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, arr, 0.0, 0.0);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}
