/* Opcode implementations for the vector family.
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

static int32_t vec_body(CSOUND *csound, OPDS *perf_h, CSN_ARRAY *source_arr_a, CSN_ARRAY *source_arr_b) {
    if (source_arr_a->itype != source_arr_b->itype) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Element type mismatch: one array is real and the other complex");
    }

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

    int32_t res = OK;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle_a, p->source_handle_b }, 3);

    CSN_ARRAY *source_arr_a = p->source_handle_a;
    CSN_ARRAY *source_arr_b = p->source_handle_b;
    res = vec_body(csound, NULL, source_arr_a, source_arr_b);
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

    ITEM_TYPE out_itype = (mode == CSN_PAIR_DISTANCE) ? CSN_REAL : source_arr_a->itype;
    const CSN_ARRAY *ops[2] = { source_arr_a, source_arr_b };
    CSNARR_RES out = prepare_output(csound, &p->h, NULL, reg, p->handle, new_ndim, new_shape, out_itype, ops, 2, CSN_ALIAS_REFUSE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    CSN_ARRAY *arr = p->handle;
    res = vec_assign_value(csound, NULL, source_arr_a, source_arr_b, arr, mode);
    if (res != OK) goto done;

    memset(&p->k_data, 0, sizeof(p->k_data));
    SET_KDATA_BEGIN(p->k_data, reg, new_shape, new_ndim, out_itype);
    PUBLISH_ELEMENTWISE(&p->k_data, source_arr_a, source_arr_b, arr, 0.0, 0.0);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

static int32_t csnarray_vec_k_init_helper(CSOUND *csound, CSN_BINOP_HH *p, CSN_VECOP_MODE mode) {
    return csnarray_vec_helper(csound, p, mode);
}

static int32_t csnarray_vec_k_helper(CSOUND *csound, CSN_BINOP_HH *p, CSN_VECOP_MODE mode) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    CHECK_KTRIG(p->trig);
    int32_t res = OK;

    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle_a, p->source_handle_b }, 3);

    CSN_ARRAY *source_arr_a = p->source_handle_a;
    CSN_ARRAY *source_arr_b = p->source_handle_b;
    res = vec_body(csound, &p->h, source_arr_a, source_arr_b);
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

    if (CAN_REUSE_ELEMENTWISE(&p->k_data, source_arr_a, source_arr_b, p->handle, 0.0, 0.0)) goto done;

    ITEM_TYPE out_itype = (mode == CSN_PAIR_DISTANCE) ? CSN_REAL : source_arr_a->itype;
    const CSN_ARRAY *ops[2] = { source_arr_a, source_arr_b };
    CSNARR_RES out = prepare_output(csound, &p->h, &p->h, reg, p->handle, new_ndim, new_shape, out_itype, ops, 2, CSN_ALIAS_REFUSE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    res = vec_assign_value(csound, &p->h, source_arr_a, source_arr_b, p->handle, mode);
    if (res != OK) goto done;

    SET_KDATA_END(p->k_data, new_shape, new_ndim, out_itype);
    PUBLISH_ELEMENTWISE(&p->k_data, source_arr_a, source_arr_b, p->handle, 0.0, 0.0);

done:
    if (pres.locked) csn_unlock(csound, reg);
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

static int32_t scalar_helper_impl_check(CSOUND *csound, OPDS *perf_h, CSN_ARRAY *source_arr_a, CSN_ARRAY *source_arr_b, double dist_order, const cs_float *out_value, const COMPLEXDAT *out_complex, CSN_VECOP_MODE mode) {
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

static void scalar_helper_assign_value(CSN_ARRAY *source_arr_a, CSN_ARRAY *source_arr_b, double dist_order, cs_float *out_value, COMPLEXDAT *out_complex, CSN_VECOP_MODE mode) {
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
        *out_value = (cs_float) pow(value, 1.0 / dist_order);
    } else if (wants_complex) {
        out_complex->real = (cs_float) acc.re;
        out_complex->imag = (cs_float) acc.im;
        out_complex->isPolar = 0;
    } else {
        *out_value = (cs_float) acc.re;
    }
}

static int32_t csnarray_scalar_helper_impl(CSOUND *csound, OPDS *h, CSN_ARRAY *source_arr_a, CSN_ARRAY *source_arr_b, cs_float *out_value, COMPLEXDAT *out_complex, CSN_VECOP_MODE mode, double dist_order, CSN_REGISTRY **registry) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    stamp_globals(csound, h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { source_arr_a, source_arr_b }, 2);

    res = scalar_helper_impl_check(csound, NULL, source_arr_a, source_arr_b, dist_order, out_value, out_complex, mode);
    if (res != OK) goto done;

    scalar_helper_assign_value(source_arr_a, source_arr_b, dist_order, out_value, out_complex, mode);
    *registry = reg;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

static int32_t csnarray_scalar_helper_k_impl(CSOUND *csound, CSN_ARRAY *source_arr_a, CSN_ARRAY *source_arr_b, cs_float *out_value, COMPLEXDAT *out_complex, CSN_VECOP_MODE mode, double dist_order, OPDS *perf_h, const cs_float *trig, K_DATA *k_data) {
    CSN_REGISTRY *reg = k_data->registry;
    CHECK_REGISTRY(csound, perf_h, reg);

    CHECK_KTRIG(trig);
    int32_t res = OK;

    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { source_arr_a, source_arr_b }, 2);

    res = scalar_helper_impl_check(csound, perf_h, source_arr_a, source_arr_b, dist_order, out_value, out_complex, mode);
    if (res != OK) goto done;

    if (CAN_REUSE_ELEMENTWISE(k_data, source_arr_a, source_arr_b, NULL, dist_order, 0.0)) {
        goto done;
    }

    scalar_helper_assign_value(source_arr_a, source_arr_b, dist_order, out_value, out_complex, mode);
    PUBLISH_ELEMENTWISE(k_data, source_arr_a, source_arr_b, NULL, dist_order, 0.0);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

static int32_t csnarray_scalar_helper(CSOUND *csound, CSN_BINOP_HH_SCALAR *p, CSN_VECOP_MODE mode, double dist_order) {
    return csnarray_scalar_helper_impl(csound, &p->h, p->source_handle_a, p->source_handle_b, p->value, NULL, mode, dist_order, &p->k_data.registry);
}

static int32_t csnarray_scalar_helper_k(CSOUND *csound, CSN_BINOP_HH_SCALAR *p, CSN_VECOP_MODE mode, double dist_order, const cs_float *trig) {
    return csnarray_scalar_helper_k_impl(csound, p->source_handle_a, p->source_handle_b, p->value, NULL, mode, dist_order, &p->h, trig, &p->k_data);
}

int32_t csnarray_dotcomp_scalar(CSOUND *csound, CSN_BINOPCOMPLEX_HH_SCALAR *p) {
    return csnarray_scalar_helper_impl(csound, &p->h, p->source_handle_a, p->source_handle_b, NULL, p->value, CSN_DOT_SCALAR, 0.0, &p->k_data.registry);
}

int32_t csnarray_dotcomp_scalar_k(CSOUND *csound, CSN_BINOPCOMPLEX_HH_SCALAR *p) {
    return csnarray_scalar_helper_k_impl(csound, p->source_handle_a, p->source_handle_b, NULL, p->value, CSN_DOT_SCALAR, 0.0, &p->h, p->trig, &p->k_data);
}

int32_t csnarray_innercomp_scalar(CSOUND *csound, CSN_BINOPCOMPLEX_HH_SCALAR *p) {
    return csnarray_scalar_helper_impl(csound, &p->h, p->source_handle_a, p->source_handle_b, NULL, p->value, CSN_INNER_SCALAR, 0.0, &p->k_data.registry);
}

int32_t csnarray_innercomp_scalar_k(CSOUND *csound, CSN_BINOPCOMPLEX_HH_SCALAR *p) {
    return csnarray_scalar_helper_k_impl(csound, p->source_handle_a, p->source_handle_b, NULL, p->value, CSN_INNER_SCALAR, 0.0, &p->h, p->trig, &p->k_data);
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

/* Serves csnnorm and the k form's init. The k form binds its inputs as
   (order, trigger, axis) into the slots the i form names (axis, order, trig),
   so the caller reads them. Its order is a k-argument that may still read 0 at
   init: then the output only takes its layout and the first pass computes. */
static int32_t norm_helper(CSOUND *csound, CSN_NORM_REDUCTION *p, double axis_value, double order, bool is_k_init) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    /* k form: a k order or axis is not known yet. The axis also fixes the
       output's shape, so without it the output is laid out empty. */
    bool order_known = !is_k_init || csn_known_at_init(csound, p->axis);
    bool axis_known = !is_k_init || csn_known_at_init(csound, p->trig);
    bool has_order = order_known && order >= 1.0;
    if (!has_order && !is_k_init) {
        return csound->InitError(csound, "[csnarray] Norm order must be >= 1, got %g", order);
    }

    int32_t res = OK;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle }, 2);

    // a reinit reuses the struct
    deinit_scratch(csound, &p->scratch);
    memset(&p->k_data, 0, sizeof(p->k_data));
    p->k_data.registry = reg;

    CSN_ARRAY *source_arr = p->source_handle;
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    /* Every run along any axis fits in the source's capacity, so the perf
       pass never grows this on a marked output. */
    ITEM_TYPE itype = source_arr->itype;
    size_t scratch_cap = (source_arr->capacity > 0 ? source_arr->capacity : 1) * itype;
    res = csn_scratch_reserve(csound, NULL, false, &p->scratch, scratch_cap, sizeof(double));
    if (res != OK) goto done;

    if (!axis_known) {
        uint32_t empty_shape[CSN_MAX_DIMS] = {0};
        const CSN_ARRAY *ops[1] = { source_arr };
        CSNARR_RES out = prepare_output(csound, &p->h, NULL, reg, p->handle, 1U, empty_shape, CSN_REAL, ops, 1, CSN_ALIAS_REFUSE);
        res = out.err;
        pres.locked |= out.locked;
        goto done;
    }

    CSN_AXIS_SPEC axis_spec = csn_normalize_axis_value(axis_value, source_ndim);
    if (axis_spec.kind != CSN_AXIS_INDEX) {
        res = csound->InitError(csound, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: finite integers %d..%u)", axis_value, source_ndim, -(int32_t) source_ndim, source_ndim - 1);
        goto done;
    }
    uint32_t axis = axis_spec.index;
    size_t run = source_shape[axis];

    /* Reducing along an axis drops it, as every other reduction does and as
       np.linalg.norm(a, axis=k) reports. The norm is real for complex input. */
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    uint32_t new_ndim = 0;
    for (uint32_t i = 0; i < source_ndim; ++i) {
        if (i != (uint32_t) axis) new_shape[new_ndim++] = source_shape[i];
    }

    const CSN_ARRAY *ops[1] = { source_arr };
    CSNARR_RES out = prepare_output(csound, &p->h, NULL, reg, p->handle, new_ndim, new_shape, CSN_REAL, ops, 1, CSN_ALIAS_REFUSE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    SET_KDATA_BEGIN(p->k_data, reg, new_shape, new_ndim, CSN_REAL);
    if (!has_order) goto done;

    CSN_ARRAY *arr = p->handle;
    if (norm_assign_value(source_arr, arr, p->scratch.scratch, axis, run, itype, order) != OK) {
        res = csound->InitError(csound, "[csnarray] Invalid norm iteration shape");
        goto done;
    }
    PUBLISH_ELEMENTWISE(&p->k_data, source_arr, NULL, arr, (double) axis, order);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_norm(CSOUND *csound, CSN_NORM_REDUCTION *p) {
    return norm_helper(csound, p, (double) *p->axis, (double) *p->order, false);
}

int32_t csnarray_norm_k_init(CSOUND *csound, CSN_NORM_REDUCTION *p) {
    return norm_helper(csound, p, (double) *p->trig, (double) *p->axis, true);
}

int32_t csnarray_norm_k(CSOUND *csound, CSN_NORM_REDUCTION *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    double axis_value = (double) *p->trig;
    double order = (double) *p->axis;

    if (order < 1.0) {
        return csound->PerfError(csound, &p->h, "[csnarray] Norm order must be >= 1, got %g", order);
    }

    CHECK_KTRIG(p->order);
    int32_t res = OK;

    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle }, 2);

    CSN_ARRAY *source_arr = p->source_handle;
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    CSN_AXIS_SPEC axis_spec = csn_normalize_axis_value(axis_value, source_ndim);
    if (axis_spec.kind != CSN_AXIS_INDEX) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: finite integers %d..%u)", axis_value, source_ndim, -(int32_t) source_ndim, source_ndim - 1);
        goto done;
    }
    uint32_t axis = axis_spec.index;

    ITEM_TYPE itype = source_arr->itype;
    size_t run = source_shape[axis];
    size_t scratch_doubles = (run > 0 ? run : 1) * itype;

    res = csn_scratch_reserve(csound, &p->h, p->handle->rt_locked, &p->scratch, scratch_doubles, sizeof(double));
    if (res != OK) goto done;

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    uint32_t new_ndim = 0;
    for (uint32_t i = 0; i < source_ndim; ++i) {
        if (i != (uint32_t) axis) new_shape[new_ndim++] = source_shape[i];
    }

    if (CAN_REUSE_ELEMENTWISE(&p->k_data, source_arr, NULL, p->handle, (double) axis, order)) goto done;

    const CSN_ARRAY *ops[1] = { source_arr };
    CSNARR_RES out = prepare_output(csound, &p->h, &p->h, reg, p->handle, new_ndim, new_shape, CSN_REAL, ops, 1, CSN_ALIAS_REFUSE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    if (norm_assign_value(source_arr, p->handle, p->scratch.scratch, axis, run, itype, order) != OK) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Invalid norm iteration shape");
        goto done;
    }

    SET_KDATA_END(p->k_data, new_shape, new_ndim, CSN_REAL);
    PUBLISH_ELEMENTWISE(&p->k_data, source_arr, NULL, p->handle, (double) axis, order);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

static int32_t norm_scalar_helper(CSOUND *csound, CSN_NORM_REDUCTION_SCALAR *p, bool is_k_init) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    /* The k form's order may still read 0 at init: the first pass computes. */
    double order = (double) *p->order;
    bool has_order = (!is_k_init || csn_known_at_init(csound, p->order)) && order >= 1.0;
    if (!has_order && !is_k_init) {
        return csound->InitError(csound, "[csnarray] Norm order must be >= 1, got %g", order);
    }

    memset(&p->k_data, 0, sizeof(p->k_data));
    p->k_data.registry = reg;
    if (!has_order) return OK;

    stamp_globals(csound, &p->h);
    bool locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);
    CSN_ARRAY *source_arr = p->source_handle;
    p->prev_result = norm_from_scratch(source_arr->data, source_arr->size, order, source_arr->itype);
    *p->value = (cs_float) p->prev_result;
    PUBLISH_ELEMENTWISE(&p->k_data, source_arr, NULL, NULL, order, 0.0);
    if (locked) csn_unlock(csound, reg);
    return OK;
}

int32_t csnarray_norm_scalar(CSOUND *csound, CSN_NORM_REDUCTION_SCALAR *p) {
    return norm_scalar_helper(csound, p, false);
}

/* Same as the axis form: the order arrives at k-rate and may read 0 at init. */
int32_t csnarray_norm_scalar_k_init(CSOUND *csound, CSN_NORM_REDUCTION_SCALAR *p) {
    return norm_scalar_helper(csound, p, true);
}

int32_t csnarray_norm_scalar_k(CSOUND *csound, CSN_NORM_REDUCTION_SCALAR *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    double order = (double) *p->order;
    if (order < 1.0) {
        return csound->PerfError(csound, &p->h, "[csnarray] Norm order must be >= 1, got %g", order);
    }

    CHECK_KTRIG(p->trig);

    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);

    CSN_ARRAY *source_arr = p->source_handle;
    if (!CAN_REUSE_ELEMENTWISE(&p->k_data, source_arr, NULL, NULL, order, 0.0)) {
        p->prev_result = norm_from_scratch(source_arr->data, source_arr->size, order, source_arr->itype);
        *p->value = (cs_float) p->prev_result;
        PUBLISH_ELEMENTWISE(&p->k_data, source_arr, NULL, NULL, order, 0.0);
    } else {
        *p->value = (cs_float) p->prev_result;
    }

    p->k_data.registry = reg;
    if (pres.locked) csn_unlock(csound, reg);
    return OK;
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

/* sort and argsort stage each slice through the scratch, normalize reads the
   whole slice for its norm and cumsum / cumprod carry the running value, so
   each reads a cell before the same cell is written and may run in place.
   diff changes the extent; gradient reads the previous cell after writing it. */
static CSN_ALIAS_POLICY unary_ax_alias_policy(CSN_UNARYOP_AX_MODE mode) {
    switch (mode) {
        case CSN_SORT:
        case CSN_ARGSORT:
        case CSN_NORMALIZE:
        case CSN_CUMSUM:
        case CSN_CUMPROD:
            return CSN_ALIAS_ELEMENTWISE;
        default:
            return CSN_ALIAS_REFUSE;
    }
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

static int32_t csnarray_unary_ax_helper(CSOUND *csound, OPDS *h, CSN_ARRAY *source_arr, const cs_float *axis_in, double order, CSN_ARRAY *out_arr, CSN_UNARYOP_AX_MODE mode, CSN_SCRATCH *scratch) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    if (mode == CSN_NORMALIZE && order < 1.0) {
        return csound->InitError(csound, "[csnarray] Norm order must be >= 1, got %g", order);
    }

    int32_t res = OK;
    stamp_globals(csound, h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { source_arr, out_arr }, 2);

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
    bool aliased = out_arr == NULL;
    if (out_arr != NULL) {
        const CSN_ARRAY *ops[1] = { source_arr };
        CSNARR_RES out = prepare_output(csound, h, NULL, reg, out_arr, new_dim, new_shape, source_arr->itype, ops, 1, unary_ax_alias_policy(mode));
        res = out.err;
        if (res != OK) goto done;
        pres.locked |= out.locked;
        arr = out_arr;
        aliased = out.aliased;
    }

    res = unaryop_allocate_scratch(csound, NULL, false, scratch, source_arr->size, itype, mode);
    if (res != OK) goto done;

    res = unary_ax_assign_value(scratch, source_arr, arr, axis, itype, order, mode);
    /* The in-place forms, and an output that is its own source, rewrite an
       array a consumer may be holding: that write takes a new version. */
    if (res == OK && aliased) update_array_data_version(&arr->version);

done:
    // the in-place i-rate forms have no deinit to free it later
    if (out_arr == NULL) deinit_scratch(csound, scratch);
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t opunary_ax_in_k_deinit(CSOUND *csound, CSN_UNARYOP_AX_IN *p) {
    deinit_scratch(csound, &p->scratch);
    return OK;
}

/* Computes at init only what is known there: a k axis leaves even the output's
   shape open (diff and the flattening modes change it), so the output is laid
   out empty; a k order (csnnormalize) leaves only the values to the first
   pass. order_arg is NULL for every mode but csnnormalize. */
static int32_t csnarray_unary_ax_k_init_helper(CSOUND *csound, OPDS *h, CSN_ARRAY *source_arr, const cs_float *axis_in, const cs_float *order_arg, CSN_ARRAY *out_arr, CSN_UNARYOP_AX_MODE mode, CSN_SCRATCH *scratch, K_DATA *k_data) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    bool axis_known = axis_in == NULL || csn_known_at_init(csound, (void *) axis_in);
    bool order_known = order_arg == NULL || csn_known_at_init(csound, (void *) order_arg);
    double order = order_arg != NULL && order_known ? (double) *order_arg : 0.0;
    if (mode == CSN_NORMALIZE && order_known && order_arg != NULL && order < 1.0) {
        return csound->InitError(csound, "[csnarray] Norm order must be >= 1, got %g", order);
    }

    int32_t res = OK;
    stamp_globals(csound, h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { source_arr, out_arr }, 2);
    if (out_arr == NULL) csn_mark_krate(source_arr);
    memset(k_data, 0, sizeof(*k_data));
    SET_KDATA_BEGIN(*k_data, reg, source_arr->shape, source_arr->ndim, source_arr->itype);

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

    /* Reserved for everything the source can hold without reallocating: the
       in-place form has no output shape to hold the source still, so the perf
       pass must never need more than this on a marked source. */
    res = unaryop_allocate_scratch(csound, NULL, false, scratch, source_arr->capacity, itype, mode);
    if (res != OK) goto done;

    if (!axis_known) {
        if (out_arr != NULL) {
            uint32_t empty_shape[CSN_MAX_DIMS] = {0};
            const CSN_ARRAY *ops[1] = { source_arr };
            CSNARR_RES out = prepare_output(csound, h, NULL, reg, out_arr, 1U, empty_shape, itype, ops, 1, CSN_ALIAS_REFUSE);
            res = out.err;
            pres.locked |= out.locked;
        }
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
    bool aliased = out_arr == NULL;
    if (out_arr != NULL) {
        const CSN_ARRAY *ops[1] = { source_arr };
        CSNARR_RES out = prepare_output(csound, h, NULL, reg, out_arr, new_dim, new_shape, source_arr->itype, ops, 1, unary_ax_alias_policy(mode));
        res = out.err;
        if (res != OK) goto done;
        pres.locked |= out.locked;
        arr = out_arr;
        aliased = out.aliased;
    }

    SET_KDATA_BEGIN(*k_data, reg, new_shape, new_dim, itype);
    /* An output that is its own source is computed by every perf pass;
       computing here as well would apply it twice in the first cycle. */
    bool skip = out_arr != NULL && aliased;
    if (!skip && order_known && (mode != CSN_NORMALIZE || order >= 1.0)) {
        res = unary_ax_assign_value(scratch, source_arr, arr, axis, itype, order, mode);
        if (res != OK) goto done;
        if (aliased) update_array_data_version(&arr->version);
        PUBLISH_ELEMENTWISE(k_data, source_arr, NULL, out_arr, (double) axis, order);
    }

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

/* In the k-rate unary axis family the public order is source, trigger, axis.
   The existing generic struct supplies two numeric slots named axis and order;
   the wrapper is the only place where that storage detail is interpreted. */
static inline const cs_float *unary_ax_k_axis(const OPDS *h, cs_float *order) {
    return h->optext->t.inArgCount > 2 ? order : NULL;
}

static inline cs_float *unary_ax_k_trigger(cs_float *axis_slot) {
    return axis_slot;
}

static int32_t csnarray_unary_ax_k_helper(CSOUND *csound, OPDS *h, CSN_ARRAY *source_arr, const cs_float *axis_in, double order, CSN_ARRAY *out_arr, CSN_UNARYOP_AX_MODE mode, CSN_SCRATCH *scratch, K_DATA *k_data, const cs_float *trig) {
    CSN_REGISTRY *reg = k_data->registry;
    CHECK_REGISTRY(csound, h, reg);

    if (mode == CSN_NORMALIZE && order < 1.0) {
        return csound->PerfError(csound, h, "[csnarray] Norm order must be >= 1, got %g", order);
    }

    CHECK_KTRIG(trig);
    int32_t res = OK;

    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { source_arr, out_arr }, 2);

    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;
    ITEM_TYPE itype = source_arr->itype;

    if ((mode == CSN_SORT || mode == CSN_ARGSORT) && itype == CSN_COMPLEX) {
        res = csn_locked_perf_error(csound, h, "[csnarray] Sort operation is for real array only");
        goto done;
    }

    if (mode == CSN_GRADIENT && itype == CSN_COMPLEX) {
        res = csn_locked_perf_error(csound, h, "[csnarray] This operation is not implemented for complex arrays");
        goto done;
    }

    CSN_AXIS_SPEC axis_spec = csn_normalize_axis(axis_in, source_ndim, unary_ax_default(mode));
    if (axis_spec.kind == CSN_AXIS_INVALID) {
        double value = axis_in == NULL ? 0.0 : (double) *axis_in;
        res = csn_locked_perf_error(csound, h, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: finite integers %d..%u)", value, source_ndim, -(int32_t) source_ndim, source_ndim - 1);
        goto done;
    }
    int32_t axis = axis_spec.kind == CSN_AXIS_FLATTEN ? -1 : (int32_t) axis_spec.index;
    double axis_value = (double) axis;

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    uint32_t new_dim = source_ndim;
    res = unary_ax_assign_shape_and_dim(csound, h, source_arr->size, source_shape, &new_dim, new_shape, axis, mode);
    if (res != OK) goto done;

    if (CAN_REUSE_ELEMENTWISE(k_data, source_arr, NULL, out_arr, axis_value, order)) goto done;

    CSN_ARRAY *arr = source_arr;
    bool aliased = out_arr == NULL;
    if (out_arr != NULL) {
        const CSN_ARRAY *ops[1] = { source_arr };
        CSNARR_RES out = prepare_output(csound, h, h, reg, out_arr, new_dim, new_shape, itype, ops, 1, unary_ax_alias_policy(mode));
        res = out.err;
        if (res != OK) goto done;
        pres.locked |= out.locked;
        arr = out_arr;
        aliased = out.aliased;
    }

    /* The scratch serves the output, or the source the in-place form rewrites. */
    bool rt_locked = out_arr == NULL ? source_arr->rt_locked : out_arr->rt_locked;
    res = unaryop_allocate_scratch(csound, h, rt_locked, scratch, source_arr->size, itype, mode);
    if (res != OK) goto done;

    res = unary_ax_assign_value(scratch, source_arr, arr, axis, itype, order, mode);
    if (res != OK) goto done;
    /* prepare_output gave a fresh output its new version; an array rewritten
       in place takes one here. */
    if (aliased) update_array_data_version(&arr->version);

    SET_KDATA_END(*k_data, new_shape, new_dim, itype);
    PUBLISH_ELEMENTWISE(k_data, source_arr, NULL, out_arr, axis_value, order);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_normalize(CSOUND *csound, CSN_UNARYOP_AX *p) {
    const cs_float *axis_in = p->INOCOUNT > 2 ? p->order : NULL;
    double order = p->INOCOUNT > 1 ? (double) *p->axis : 1.0;
    return csnarray_unary_ax_helper(csound, &p->h, p->source_handle, axis_in, order, p->handle, CSN_NORMALIZE, &p->scratch);
}

int32_t csnarray_normalize_k_init(CSOUND *csound, CSN_UNARYOP_AX *p) {
    const cs_float *axis_in = p->INOCOUNT > 3 ? p->trig : NULL;
    return csnarray_unary_ax_k_init_helper(csound, &p->h, p->source_handle, axis_in, p->axis, p->handle, CSN_NORMALIZE, &p->scratch, &p->k_data);
}

int32_t csnarray_normalize_k(CSOUND *csound, CSN_UNARYOP_AX *p) {
    const cs_float *axis_in = p->INOCOUNT > 3 ? p->trig : NULL;
    return csnarray_unary_ax_k_helper(csound, &p->h, p->source_handle, axis_in, (double) *p->axis, p->handle, CSN_NORMALIZE, &p->scratch, &p->k_data, p->order);
}

int32_t csnarray_normalize_in(CSOUND *csound, CSN_UNARYOP_AX_IN *p) {
    const cs_float *axis_in = p->INOCOUNT > 2 ? p->order : NULL;
    double order = p->INOCOUNT > 1 ? (double) *p->axis : 1.0;
    return csnarray_unary_ax_helper(csound, &p->h, p->source_handle, axis_in, order, NULL, CSN_NORMALIZE, &p->scratch);
}

int32_t csnarray_normalize_in_k_init(CSOUND *csound, CSN_UNARYOP_AX_IN *p) {
    const cs_float *axis_in = p->INOCOUNT > 3 ? p->trig : NULL;
    return csnarray_unary_ax_k_init_helper(csound, &p->h, p->source_handle, axis_in, p->axis, NULL, CSN_NORMALIZE, &p->scratch, &p->k_data);
}

int32_t csnarray_normalize_in_k(CSOUND *csound, CSN_UNARYOP_AX_IN *p) {
    const cs_float *axis_in = p->INOCOUNT > 3 ? p->trig : NULL;
    return csnarray_unary_ax_k_helper(csound, &p->h, p->source_handle, axis_in, (double) *p->axis, NULL, CSN_NORMALIZE, &p->scratch, &p->k_data, p->order);
}

int32_t csnarray_sort_in(CSOUND *csound, CSN_UNARYOP_AX_IN *p) {
    const cs_float *axis_in = p->INOCOUNT > 1 ? p->axis : NULL;
    return csnarray_unary_ax_helper(csound, &p->h, p->source_handle, axis_in, 0.0, NULL, CSN_SORT, &p->scratch);
}

int32_t csnarray_sort_in_k_init(CSOUND *csound, CSN_UNARYOP_AX_IN *p) {
    return csnarray_unary_ax_k_init_helper(csound, &p->h, p->source_handle, unary_ax_k_axis(&p->h, p->order), NULL, NULL, CSN_SORT, &p->scratch, &p->k_data);
}

int32_t csnarray_sort_in_k(CSOUND *csound, CSN_UNARYOP_AX_IN *p) {
    return csnarray_unary_ax_k_helper(csound, &p->h, p->source_handle, unary_ax_k_axis(&p->h, p->order), 0.0, NULL, CSN_SORT, &p->scratch, &p->k_data, unary_ax_k_trigger(p->axis));
}

int32_t csnarray_distance(CSOUND *csound, CSN_BINOP_HH_SCALAR *p) {
    double dist_order = (double) *p->arg_a;
    return csnarray_scalar_helper(csound, p, CSN_DISTANCE, dist_order);
}

/* The Minkowski order is a k-argument here, so during the init pass it normally
   still reads 0, which no validation can accept. Stash the registry the perf
   pass needs, leave the scalar at 0, and let the first pass compute it. */
int32_t csnarray_distance_k_init(CSOUND *csound, CSN_BINOP_HH_SCALAR *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    memset(&p->k_data, 0, sizeof(p->k_data));
    p->k_data.registry = reg;

    double dist_order = (double) *p->arg_a;
    if (csn_known_at_init(csound, p->arg_a) && dist_order >= 1.0) return csnarray_distance(csound, p);

    stamp_globals(csound, &p->h);
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
    const cs_float *axis_in = p->INOCOUNT > 1 ? p->axis : NULL;
    return csnarray_unary_ax_helper(csound, &p->h, p->source_handle, axis_in, 0.0, p->handle, CSN_DIFF, &p->scratch);
}

int32_t csnarray_diff_k_init(CSOUND *csound, CSN_UNARYOP_AX *p) {
    return csnarray_unary_ax_k_init_helper(csound, &p->h, p->source_handle, unary_ax_k_axis(&p->h, p->order), NULL, p->handle, CSN_DIFF, &p->scratch, &p->k_data);
}

int32_t csnarray_diff_k(CSOUND *csound, CSN_UNARYOP_AX *p) {
    return csnarray_unary_ax_k_helper(csound, &p->h, p->source_handle, unary_ax_k_axis(&p->h, p->order), 0.0, p->handle, CSN_DIFF, &p->scratch, &p->k_data, unary_ax_k_trigger(p->axis));
}

int32_t csnarray_gradient(CSOUND *csound, CSN_UNARYOP_AX *p) {
    const cs_float *axis_in = p->INOCOUNT > 1 ? p->axis : NULL;
    return csnarray_unary_ax_helper(csound, &p->h, p->source_handle, axis_in, 0.0, p->handle, CSN_GRADIENT, &p->scratch);
}

int32_t csnarray_gradient_k_init(CSOUND *csound, CSN_UNARYOP_AX *p) {
    return csnarray_unary_ax_k_init_helper(csound, &p->h, p->source_handle, unary_ax_k_axis(&p->h, p->order), NULL, p->handle, CSN_GRADIENT, &p->scratch, &p->k_data);
}

int32_t csnarray_gradient_k(CSOUND *csound, CSN_UNARYOP_AX *p) {
    return csnarray_unary_ax_k_helper(csound, &p->h, p->source_handle, unary_ax_k_axis(&p->h, p->order), 0.0, p->handle, CSN_GRADIENT, &p->scratch, &p->k_data, unary_ax_k_trigger(p->axis));
}

int32_t csnarray_cumsum(CSOUND *csound, CSN_UNARYOP_AX *p) {
    const cs_float *axis_in = p->INOCOUNT > 1 ? p->axis : NULL;
    return csnarray_unary_ax_helper(csound, &p->h, p->source_handle, axis_in, 0.0, p->handle, CSN_CUMSUM, &p->scratch);
}

int32_t csnarray_cumsum_k_init(CSOUND *csound, CSN_UNARYOP_AX *p) {
    return csnarray_unary_ax_k_init_helper(csound, &p->h, p->source_handle, unary_ax_k_axis(&p->h, p->order), NULL, p->handle, CSN_CUMSUM, &p->scratch, &p->k_data);
}

int32_t csnarray_cumsum_k(CSOUND *csound, CSN_UNARYOP_AX *p) {
    return csnarray_unary_ax_k_helper(csound, &p->h, p->source_handle, unary_ax_k_axis(&p->h, p->order), 0.0, p->handle, CSN_CUMSUM, &p->scratch, &p->k_data, unary_ax_k_trigger(p->axis));
}

int32_t csnarray_cumprod(CSOUND *csound, CSN_UNARYOP_AX *p) {
    const cs_float *axis_in = p->INOCOUNT > 1 ? p->axis : NULL;
    return csnarray_unary_ax_helper(csound, &p->h, p->source_handle, axis_in, 0.0, p->handle, CSN_CUMPROD, &p->scratch);
}

int32_t csnarray_cumprod_k_init(CSOUND *csound, CSN_UNARYOP_AX *p) {
    return csnarray_unary_ax_k_init_helper(csound, &p->h, p->source_handle, unary_ax_k_axis(&p->h, p->order), NULL, p->handle, CSN_CUMPROD, &p->scratch, &p->k_data);
}

int32_t csnarray_cumprod_k(CSOUND *csound, CSN_UNARYOP_AX *p) {
    return csnarray_unary_ax_k_helper(csound, &p->h, p->source_handle, unary_ax_k_axis(&p->h, p->order), 0.0, p->handle, CSN_CUMPROD, &p->scratch, &p->k_data, unary_ax_k_trigger(p->axis));
}

int32_t csnarray_sort(CSOUND *csound, CSN_UNARYOP_AX *p) {
    const cs_float *axis_in = p->INOCOUNT > 1 ? p->axis : NULL;
    return csnarray_unary_ax_helper(csound, &p->h, p->source_handle, axis_in, 0.0, p->handle, CSN_SORT, &p->scratch);
}

int32_t csnarray_sort_k_init(CSOUND *csound, CSN_UNARYOP_AX *p) {
    return csnarray_unary_ax_k_init_helper(csound, &p->h, p->source_handle, unary_ax_k_axis(&p->h, p->order), NULL, p->handle, CSN_SORT, &p->scratch, &p->k_data);
}

int32_t csnarray_sort_k(CSOUND *csound, CSN_UNARYOP_AX *p) {
    return csnarray_unary_ax_k_helper(csound, &p->h, p->source_handle, unary_ax_k_axis(&p->h, p->order), 0.0, p->handle, CSN_SORT, &p->scratch, &p->k_data, unary_ax_k_trigger(p->axis));
}

int32_t csnarray_argsort(CSOUND *csound, CSN_UNARYOP_AX *p) {
    const cs_float *axis_in = p->INOCOUNT > 1 ? p->axis : NULL;
    return csnarray_unary_ax_helper(csound, &p->h, p->source_handle, axis_in, 0.0, p->handle, CSN_ARGSORT, &p->scratch);
}

int32_t csnarray_argsort_k_init(CSOUND *csound, CSN_UNARYOP_AX *p) {
    return csnarray_unary_ax_k_init_helper(csound, &p->h, p->source_handle, unary_ax_k_axis(&p->h, p->order), NULL, p->handle, CSN_ARGSORT, &p->scratch, &p->k_data);
}

int32_t csnarray_argsort_k(CSOUND *csound, CSN_UNARYOP_AX *p) {
    return csnarray_unary_ax_k_helper(csound, &p->h, p->source_handle, unary_ax_k_axis(&p->h, p->order), 0.0, p->handle, CSN_ARGSORT, &p->scratch, &p->k_data, unary_ax_k_trigger(p->axis));
}

int32_t csnarray_matmul_scalar(CSOUND *csound, CSN_BINOP_HH_SCALAR *p) {
    return csnarray_scalar_helper(csound, p, CSN_DOT_SCALAR, 0.0);
}

int32_t csnarray_matmul_scalar_k(CSOUND *csound, CSN_BINOP_HH_SCALAR *p) {
    return csnarray_scalar_helper_k(csound, p, CSN_DOT_SCALAR, 0.0, p->arg_a);
}

static int32_t matmul_body(CSOUND *csound, OPDS *perf_h, CSN_ARRAY *source_arr_a,CSN_ARRAY *source_arr_b) {
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

    int32_t res = OK;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle_a, p->source_handle_b }, 3);

    CSN_ARRAY *source_arr_a = p->source_handle_a;
    CSN_ARRAY *source_arr_b = p->source_handle_b;
    res = matmul_body(csound, NULL, source_arr_a, source_arr_b);
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

    const CSN_ARRAY *ops[2] = { source_arr_a, source_arr_b };
    CSNARR_RES out = prepare_output(csound, &p->h, NULL, reg, p->handle, box.new_dim, box.new_shape, itype, ops, 2, CSN_ALIAS_REFUSE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    CSN_ARRAY *arr = p->handle;
    if (matmul_assign_value(source_arr_a, source_arr_b, arr, &box) != OK) {
        res = csound->InitError(csound, "[csnarray] Invalid matmul batch iteration shape");
        goto done;
    }

    memset(&p->k_data, 0, sizeof(p->k_data));
    SET_KDATA_BEGIN(p->k_data, reg, arr->shape, arr->ndim, itype);
    PUBLISH_ELEMENTWISE(&p->k_data, source_arr_a, source_arr_b, arr, 0.0, 0.0);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_matmul_k(CSOUND *csound, CSN_BINOP_HH *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    CHECK_KTRIG(p->trig);
    int32_t res = OK;
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle_a, p->source_handle_b }, 3);

    CSN_ARRAY *source_arr_a = p->source_handle_a;
    CSN_ARRAY *source_arr_b = p->source_handle_b;
    res = matmul_body(csound, &p->h, source_arr_a, source_arr_b);
    if (res != OK) goto done;

    uint32_t source_a_dim = source_arr_a->ndim;
    uint32_t source_b_dim = source_arr_b->ndim;
    ITEM_TYPE itype = source_arr_a->itype;

    if (source_a_dim == 1 && source_b_dim == 1) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] The matrix product of two 1-D arrays is a scalar; declare the output as i, not as a handle");
        goto done;
    }

    CSN_MATMUL_LAYOUT box = {0};
    res = matmul_assign_shape_and_dim(csound, &p->h, source_arr_a, source_arr_b, &box);
    if (res != OK) goto done;

    size_t requested_size = 0;
    if (get_array_size_from_shape(&requested_size, box.new_dim, box.new_shape) != OK) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
        goto done;
    }

    if (CAN_REUSE_ELEMENTWISE(&p->k_data, source_arr_a, source_arr_b, p->handle, 0.0, 0.0)) goto done;

    const CSN_ARRAY *ops[2] = { source_arr_a, source_arr_b };
    CSNARR_RES out = prepare_output(csound, &p->h, &p->h, reg, p->handle, box.new_dim, box.new_shape, itype, ops, 2, CSN_ALIAS_REFUSE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    if (matmul_assign_value(source_arr_a, source_arr_b, p->handle, &box) != OK) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Invalid matmul batch iteration shape");
        goto done;
    }

    SET_KDATA_END(p->k_data, box.new_shape, box.new_dim, itype);
    PUBLISH_ELEMENTWISE(&p->k_data, source_arr_a, source_arr_b, p->handle, 0.0, 0.0);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_angle_distance(CSOUND *csound, CSN_BINOP_HH_SCALAR *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    p->k_data.registry = reg;

    int32_t res = OK;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle_a, p->source_handle_b }, 2);

    CSN_ARRAY *source_arr_a = p->source_handle_a;
    CSN_ARRAY *source_arr_b = p->source_handle_b;
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
        *p->value = (cs_float) NAN;
        goto done;
    }

    norm_a = sqrt(norm_a);
    norm_b = sqrt(norm_b);

    double dot = source_arr_a->itype == CSN_COMPLEX ? hypot(dot_ab.re, dot_ab.im) : dot_ab.re;
    double c = dot / (norm_a * norm_b);
    c = c > 1.0 ? 1.0 : c;
    c = c < -1.0 ? -1.0 : c;
    double theta = acos(c);

    *p->value = (cs_float) theta;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_angle_distance_k(CSOUND *csound, CSN_BINOP_HH_SCALAR *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    CHECK_KTRIG(p->arg_a);
    int32_t res = OK;

    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle_a, p->source_handle_b }, 2);

    CSN_ARRAY *source_arr_a = p->source_handle_a;
    CSN_ARRAY *source_arr_b = p->source_handle_b;

    if (CAN_REUSE_ELEMENTWISE(&p->k_data, source_arr_a, source_arr_b, NULL, 0.0, 0.0)) {
        goto done;
    }

    uint32_t *source_shape_a = source_arr_a->shape;
    uint32_t *source_shape_b = source_arr_b->shape;
    size_t source_dim_a = source_arr_a->ndim;
    size_t source_dim_b = source_arr_b->ndim;

    if (source_dim_a != source_dim_b || memcmp(source_shape_a, source_shape_b, sizeof(uint32_t) * CSN_MAX_DIMS) != 0) {
        char abuf[CSN_SHAPE_STR_MAX], bbuf[CSN_SHAPE_STR_MAX];
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Both arrays must have the same shape, got %s and %s", shape_str(abuf, sizeof(abuf), source_shape_a, (uint32_t) source_dim_a), shape_str(bbuf, sizeof(bbuf), source_shape_b, (uint32_t) source_dim_b));
        goto done;
    }

    if (source_arr_a->itype != source_arr_b->itype) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Element type mismatch: one array is real and the other complex");
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
        *p->value = (cs_float) NAN;
        goto done;
    }

    norm_a = sqrt(norm_a);
    norm_b = sqrt(norm_b);

    double dot = source_arr_a->itype == CSN_COMPLEX ? hypot(dot_ab.re, dot_ab.im) : dot_ab.re;
    double c = dot / (norm_a * norm_b);
    c = c > 1.0 ? 1.0 : c;
    c = c < -1.0 ? -1.0 : c;
    double theta = acos(c);

    *p->value = (cs_float) theta;
    PUBLISH_ELEMENTWISE(&p->k_data, source_arr_a, source_arr_b, NULL, 0.0, 0.0);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

static int32_t csnarray_trace_impl(CSOUND *csound, OPDS *h, CSN_ARRAY *source_arr, cs_float *out_value, COMPLEXDAT *out_complex, CSN_REGISTRY **registry) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    *registry = reg;

    int32_t res = OK;
    stamp_globals(csound, h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { source_arr }, 1);

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
        out_complex->real = (cs_float) sum.re;
        out_complex->imag = (cs_float) sum.im;
        out_complex->isPolar = 0;
    } else {
        if (out_value == NULL) {
            res = csound->InitError(csound, "[csnarray] Handle holds a real array; declare the result as i");
            goto done;
        }
        *out_value = (cs_float) sum.re;
    }

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

static int32_t csnarray_trace_impl_k(CSOUND *csound, OPDS *perf_h, CSN_ARRAY *source_arr, cs_float *out_value, COMPLEXDAT *out_complex, CSN_REGISTRY **registry, cs_float *trig, K_DATA *k_data) {
    CSN_REGISTRY *reg = *registry;
    CHECK_REGISTRY(csound, perf_h, reg);

    CHECK_KTRIG(trig);
    int32_t res = OK;

    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { source_arr }, 1);

    uint32_t dim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    if (dim != 2) {
        res = csn_locked_perf_error(csound, perf_h, "[csnarray] Trace needs a 2-D matrix, got %u-D", dim);
        goto done;
    }

    if (CAN_REUSE_ELEMENTWISE(k_data, source_arr, NULL, NULL, 0.0, 0.0)) goto done;

    uint32_t n = source_shape[0] < source_shape[1] ? source_shape[0] : source_shape[1];
    CSN_COMPLEXDAT sum = { 0.0, 0.0 };
    for (uint32_t i = 0; i < n; i++) {
        size_t off = (size_t) i * (source_arr->strides[0] + source_arr->strides[1]);
        complex_add(&sum, sum, item_at(source_arr, off));
    }

    if (source_arr->itype == CSN_COMPLEX) {
        if (out_complex == NULL) {
            res = csn_locked_perf_error(csound, perf_h, "[csnarray] Handle holds a complex array; declare the result as :Complex;");
            goto done;
        }
        out_complex->real = (cs_float) sum.re;
        out_complex->imag = (cs_float) sum.im;
        out_complex->isPolar = 0;
    } else {
        if (out_value == NULL) {
            res = csn_locked_perf_error(csound, perf_h, "[csnarray] Handle holds a real array; declare the result as i");
            goto done;
        }
        *out_value = (cs_float) sum.re;
    }
    PUBLISH_ELEMENTWISE(k_data, source_arr, NULL, NULL, 0.0, 0.0);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_trace(CSOUND *csound, CSN_UNARYOP_SCALAR *p) {
    return csnarray_trace_impl(csound, &p->h, p->source_handle, p->value, NULL, &p->k_data.registry);
}

int32_t csnarray_trace_k(CSOUND *csound, CSN_UNARYOP_SCALAR *p) {
    return csnarray_trace_impl_k(csound, &p->h, p->source_handle, p->value, NULL, &p->k_data.registry, p->trig, &p->k_data);
}

int32_t csnarray_tracecomp(CSOUND *csound, CSN_UNARYOPCOMPLEX_SCALAR *p) {
    return csnarray_trace_impl(csound, &p->h, p->source_handle, NULL, p->value, &p->k_data.registry);
}

int32_t csnarray_tracecomp_k(CSOUND *csound, CSN_UNARYOPCOMPLEX_SCALAR *p) {
    return csnarray_trace_impl_k(csound, &p->h, p->source_handle, NULL, p->value, &p->k_data.registry, p->trig, &p->k_data);
}

static int32_t diag_body(CSOUND *csound, OPDS *perf_h, CSN_ARRAY *source_arr) {
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

    int32_t res = OK;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle }, 2);

    CSN_ARRAY *source_arr = p->source_handle;
    res = diag_body(csound, NULL, source_arr);
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

    const CSN_ARRAY *ops[1] = { source_arr };
    CSNARR_RES out = prepare_output(csound, &p->h, NULL, reg, p->handle, new_dim, new_shape, source_arr->itype, ops, 1, CSN_ALIAS_REFUSE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    CSN_ARRAY *arr = p->handle;
    diag_assign_value(source_arr, arr, new_dim, new_shape);

    memset(&p->k_data, 0, sizeof(p->k_data));
    SET_KDATA_BEGIN(p->k_data, reg, new_shape, new_dim, source_arr->itype);
    PUBLISH_ELEMENTWISE(&p->k_data, source_arr, NULL, arr, 0.0, 0.0);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_diag_k(CSOUND *csound, CSN_UNARYOP *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    CHECK_KTRIG(p->trig);
    int32_t res = OK;
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle }, 2);

    CSN_ARRAY *source_arr = p->source_handle;
    res = diag_body(csound, &p->h, source_arr);
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

    if (CAN_REUSE_ELEMENTWISE(&p->k_data, source_arr, NULL, p->handle, 0.0, 0.0)) goto done;

    const CSN_ARRAY *ops[1] = { source_arr };
    CSNARR_RES out = prepare_output(csound, &p->h, &p->h, reg, p->handle, new_dim, new_shape, source_arr->itype, ops, 1, CSN_ALIAS_REFUSE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    diag_assign_value(source_arr, p->handle, new_dim, new_shape);

    SET_KDATA_END(p->k_data, new_shape, new_dim, source_arr->itype);
    PUBLISH_ELEMENTWISE(&p->k_data, source_arr, NULL, p->handle, 0.0, 0.0);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}
