#include "csnum.h"
#include "csnlinalg.h"
#include "csnregistry.h"
#include "csnum.h"
#include "csnum_internal.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <float.h>

/* The threshold a pivot has to clear to count as non-zero.
   An exact zero is not the only way to be singular. A matrix whose rows are
   linearly dependent leaves a pivot that is a rounding error rather than a
   zero, and taken at face value it produces an inverse of astronomical numbers
   instead of a refusal. The scale has to come from the matrix itself, so the
   rule is the usual one: the largest magnitude in the operand as it came in,
   times the dimension, times the machine epsilon. An all-zero matrix gives a
   threshold of zero, which its own zero pivot then fails to clear. */
static double lu_tolerance(const double *a, size_t n, ITEM_TYPE itype) {
    double largest = 0.0;
    for (size_t i = 0; i < n * n; ++i) {
        double magnitude = itype == CSN_COMPLEX ? hypot(a[i * 2], a[i * 2 + 1]) : fabs(a[i]);
        if (magnitude > largest) largest = magnitude;
    }
    return (double) n * DBL_EPSILON * largest;
}

int32_t lu_factor_real(double *a, size_t n, CSN_LU_INFO *info) {
    info->size = n;
    info->parity = 1;
    info->singular = false;
    info->tolerance = lu_tolerance(a, n, CSN_REAL);

    for (size_t k = 0; k < n; ++k) {
        size_t pivot = k;
        double max_value = fabs(a[k * n + k]);
        for (size_t i = k + 1; i < n; ++i) {
            double value = fabs(a[i * n + k]);
            if (value > max_value) {
                max_value = value;
                pivot = i;
            }
        }

        if (max_value <= info->tolerance) {
            info->singular = true;
            return NOTOK;
        }

        info->pivots[k] = pivot;

        if (pivot != k) {
            for (size_t j = 0; j < n; ++j) {
                double temp = a[k * n + j];
                a[k * n + j] = a[pivot * n + j];
                a[pivot * n + j] = temp;
            }
            info->parity = -info->parity;
        }

        double pivot_value = a[k * n + k];
        for (size_t i = k + 1; i < n; ++i) {
            double multiplier = a[i * n + k] / pivot_value;
            a[i * n + k] = multiplier;
            for (size_t j = k + 1; j < n; ++j) {
                a[i * n + j] -= multiplier * a[k * n + j];
            }
        }
    }

    return OK;
}

int32_t lu_solve_real(const double *lu, const CSN_LU_INFO *info, double *b, size_t nrhs) {
    size_t n = info->size;
    for (size_t k = 0; k < n; ++k) {
        size_t pivot = info->pivots[k];
        if (pivot != k) {
            for (size_t r = 0; r < nrhs; ++r) {
                double temp = b[k * nrhs + r];
                b[k * nrhs + r] = b[pivot * nrhs + r];
                b[pivot * nrhs + r] = temp;
            }
        }
    }

    for (size_t i = 0; i < n; ++i) {
        for (size_t r = 0; r < nrhs; ++r) {
            double value = b[i * nrhs + r];
            for (size_t j = 0; j < i; ++j) {
                value -= lu[i * n + j] * b[j * nrhs + r];
            }
            b[i * nrhs + r] = value;
        }
    }

    for (size_t ii = n; ii-- > 0;) {
        size_t i = ii;
        double diagonal = lu[i * n + i];
        if (fabs(diagonal) <= info->tolerance) return NOTOK;
        for (size_t r = 0; r < nrhs; ++r) {
            double value = b[i * nrhs + r];
            for (size_t j = i + 1; j < n; ++j) {
                value -= lu[i * n + j] * b[j * nrhs + r];
            }

            b[i * nrhs + r] = value / diagonal;
        }
    }

    return OK;
}

int32_t lu_factor_complex(double *a, size_t n, CSN_LU_INFO *info) {
    info->size = n;
    info->parity = 1;
    info->singular = false;
    info->tolerance = lu_tolerance(a, n, CSN_COMPLEX);

    for (size_t k = 0; k < n; ++k) {

        size_t pivot = k;
        CSN_COMPLEXDAT c = { .re = a[(k * n + k) * 2], .im = a[(k * n + k) * 2 + 1] };
        double max_value = c.re * c.re + c.im * c.im;

        for (size_t i = k + 1; i < n; ++i) {
            CSN_COMPLEXDAT v = { .re = a[(i * n + k) * 2], .im = a[(i * n + k) * 2 + 1] };
            double value = v.re * v.re + v.im * v.im;
            if (value > max_value) {
                max_value = value;
                pivot = i;
            }
        }

        /* The search compares squared magnitudes, the threshold is a
           magnitude: one square root, on the winner only. */
        if (sqrt(max_value) <= info->tolerance) {
            info->singular = true;
            return NOTOK;
        }

        info->pivots[k] = pivot;

        if (pivot != k) {
            for (size_t j = 0; j < n; ++j) {
                CSN_COMPLEXDAT temp = { .re = a[(k * n + j) * 2], .im = a[(k * n + j) * 2 + 1] };
                a[(k * n + j) * 2] = a[(pivot * n + j) * 2];
                a[(k * n + j) * 2 + 1] = a[(pivot * n + j) * 2 + 1];
                a[(pivot * n + j) * 2] = temp.re;
                a[(pivot * n + j) * 2 + 1] = temp.im;
            }
            info->parity = -info->parity;
        }

        CSN_COMPLEXDAT pivot_value = { .re = a[(k * n + k) * 2], .im = a[(k * n + k) * 2 + 1] };

        for (size_t i = k + 1; i < n; ++i) {
            CSN_COMPLEXDAT ca = { .re = a[(i * n + k) * 2], .im = a[(i * n + k) * 2 + 1] };
            CSN_COMPLEXDAT multiplier;
            complex_div(&multiplier, ca, pivot_value);
            a[(i * n + k) * 2] = multiplier.re;
            a[(i * n + k) * 2 + 1] = multiplier.im;
            for (size_t j = k + 1; j < n; ++j) {
                CSN_COMPLEXDAT target = { .re = a[(i * n + j) * 2], .im = a[(i * n + j) * 2 + 1] };
                CSN_COMPLEXDAT value = { .re = a[(k * n + j) * 2], .im = a[(k * n + j) * 2 + 1] };
                CSN_COMPLEXDAT temp;
                complex_prod(&temp, multiplier, value);
                complex_sub(&value, target, temp);
                a[(i * n + j) * 2] = value.re;
                a[(i * n + j) * 2 + 1] = value.im;
            }
        }
    }

    return OK;
}

int32_t lu_solve_complex(const double *lu, const CSN_LU_INFO *info, double *b, size_t nrhs) {
    size_t n = info->size;
    for (size_t k = 0; k < n; ++k) {
        size_t pivot = info->pivots[k];
        if (pivot != k) {
            for (size_t r = 0; r < nrhs; ++r) {
                CSN_COMPLEXDAT temp = { .re = b[(k * nrhs + r) * 2], .im = b[(k * nrhs + r) * 2 + 1] };
                b[(k * nrhs + r) * 2] = b[(pivot * nrhs + r) * 2];
                b[(k * nrhs + r) * 2 + 1] = b[(pivot * nrhs + r) * 2 + 1];
                b[(pivot * nrhs + r) * 2] = temp.re;
                b[(pivot * nrhs + r) * 2 + 1] = temp.im;
            }
        }
    }

    for (size_t i = 0; i < n; ++i) {
        for (size_t r = 0; r < nrhs; ++r) {
            CSN_COMPLEXDAT value = { .re = b[(i * nrhs + r) * 2], .im = b[(i * nrhs + r) * 2 + 1] };
            for (size_t j = 0; j < i; ++j) {
                CSN_COMPLEXDAT lu_value = { .re = lu[(i * n + j) * 2], .im = lu[(i * n + j) * 2 + 1] };
                CSN_COMPLEXDAT lb_value = { .re = b[(j * nrhs + r) * 2], .im = b[(j * nrhs + r) * 2 + 1] };
                CSN_COMPLEXDAT temp;
                complex_prod(&temp, lu_value, lb_value);
                complex_sub(&value, value, temp);
            }

            b[(i * nrhs + r) * 2] = value.re;
            b[(i * nrhs + r) * 2 + 1] = value.im;
        }
    }

    for (size_t ii = n; ii-- > 0;) {
        size_t i = ii;
        CSN_COMPLEXDAT diagonal = { .re = lu[(i * n + i) * 2], .im = lu[(i * n + i) * 2 + 1] };
        double c_abs = hypot(diagonal.re, diagonal.im);
        if (c_abs <= info->tolerance) return NOTOK;
        for (size_t r = 0; r < nrhs; ++r) {
            CSN_COMPLEXDAT value = { .re = b[(i * nrhs + r) * 2], .im = b[(i * nrhs + r) * 2 + 1] };
            for (size_t j = i + 1; j < n; ++j) {
                CSN_COMPLEXDAT lu_value = { .re = lu[(i * n + j) * 2], .im = lu[(i * n + j) * 2 + 1] };
                CSN_COMPLEXDAT lb_value = { .re = b[(j * nrhs + r) * 2], .im = b[(j * nrhs + r) * 2 + 1] };
                CSN_COMPLEXDAT temp;
                complex_prod(&temp, lu_value, lb_value);
                complex_sub(&value, value, temp);
            }
            CSN_COMPLEXDAT lb_value_new = { .re = b[(i * nrhs + r) * 2], .im = b[(i * nrhs + r) * 2 + 1] };
            complex_div(&lb_value_new, value, diagonal);
            b[(i * nrhs + r) * 2] = lb_value_new.re;
            b[(i * nrhs + r) * 2 + 1] = lb_value_new.im;
        }
    }

    return OK;
}

/* A singular matrix is an error for the operations that would have to divide
   by the missing pivot, and an ordinary answer for the one that does not: the
   determinant of a singular matrix is zero, and asking for it is the classic
   way to find out whether a matrix is singular in the first place. A caller
   that passes `singular` is saying it can handle the answer itself, and gets
   the flag instead of a refusal; NumPy draws the line in the same place. */
static int32_t solve_helper(CSOUND *csound, OPDS *perf_h, CSN_LU_INFO *info, double *buffer_a, double *buffer_b, size_t n, size_t nrhs, ITEM_TYPE itype, double *parity, bool *singular) {
    if (singular != NULL) *singular = false;

    if (itype == CSN_COMPLEX) {
        if (lu_factor_complex(buffer_a, n, info) != OK) {
            if (singular != NULL) {
                *singular = true;
                return OK;
            }
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Matrix is singular and cannot be solved");
        }
        if (buffer_b != NULL) {
            if (lu_solve_complex(buffer_a, info, buffer_b, nrhs) != OK) {
                return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Matrix is singular or numerically singular during back substitution");
            }
        }
    } else {
        if (lu_factor_real(buffer_a, n, info) != OK) {
            if (singular != NULL) {
                *singular = true;
                return OK;
            }
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Matrix is singular and cannot be solved");
        }
        if (buffer_b != NULL) {
            if (lu_solve_real(buffer_a, info, buffer_b, nrhs) != OK) {
                return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Matrix is singular or numerically singular during back substitution");
            }
        }
    }
    if (parity != NULL) *parity = (double) info->parity;
    return OK;
}

/* Capacities count doubles, so a complex operand arriving later is measured
   in the units it needs, and the pivots follow a's buffer, which bounds the row
   count. The init calls pass each operand's capacity, the most it can hold
   without reallocating; at perf time these buffers serve the output, or the
   source for the scalar determinant, and growing them is refused when that
   array is marked. A shared operand has the lock held by the caller. */
static int32_t linalg_allocate(CSOUND *csound, OPDS *perf_h, bool rt_locked, CSN_SCRATCH *a, CSN_SCRATCH *b, CSN_LU_INFO *info, size_t size_a, size_t size_b, ITEM_TYPE itype) {
    size_t capacity_before = a->scratch_capacity;
    int32_t res = csn_scratch_reserve(csound, perf_h, rt_locked, a, (size_a > 0 ? size_a : 1) * (size_t) itype, sizeof(double));
    if (res == OK && b != NULL) {
        res = csn_scratch_reserve(csound, perf_h, rt_locked, b, (size_b > 0 ? size_b : 1) * (size_t) itype, sizeof(double));
    }
    if (res != OK) return res;

    if (info->pivots == NULL || a->scratch_capacity != capacity_before) {
        size_t *pivots = csound->ReAlloc(csound, info->pivots, sizeof(size_t) * a->scratch_capacity);
        if (pivots == NULL) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Internal error: memory allocation failed");
        }
        info->pivots = pivots;
    }

    return OK;
}

static void linalg_diag(double *matrix, size_t n, ITEM_TYPE itype) {
    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j < n; j++) {
            matrix[(i * n + j) * itype] = (i == j) ? 1.0 : 0.0;
            if (itype == CSN_COMPLEX) matrix[(i * n + j) * 2 + 1] = 0.0;
        }
    }
}

// impl

int32_t csnarray_solve_deinit(CSOUND *csound, CSN_LINALG_SOLVE *p) {
    deinit_scratch(csound, &p->buffer_a);
    deinit_scratch(csound, &p->buffer_b);
    if (p->lu_info.pivots != NULL) {
        csound->Free(csound, p->lu_info.pivots);
        p->lu_info.pivots = NULL;
    }
    return OK;
}

int32_t csnarray_solve(CSOUND *csound, CSN_LINALG_SOLVE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle_a, p->source_handle_b }, 3);

    CSN_ARRAY *source_arr_a = p->source_handle_a;
    CSN_ARRAY *source_arr_b = p->source_handle_b;
    uint32_t source_ndim_a = source_arr_a->ndim;
    uint32_t *source_shape_a = source_arr_a->shape;
    uint32_t source_ndim_b = source_arr_b->ndim;
    uint32_t *source_shape_b = source_arr_b->shape;
    ITEM_TYPE itype_a = source_arr_a->itype;
    ITEM_TYPE itype_b = source_arr_b->itype;

    if (source_ndim_a != 2 || source_shape_a[0] != source_shape_a[1]) {
        res = csn_init_error(csound, "[csnarray] Matrix must be 2-D N x N");
        goto done;
    }

    if (source_ndim_b > 2) {
        res = csn_init_error(csound, "[csnarray] B must be 1-D or 2-D, got %u-D", source_ndim_b);
        goto done;
    }

    if (source_shape_b[0] != source_shape_a[0]) {
        res = csn_init_error(csound, "[csnarray] Incompatible dimensions: shape[0] of B must be equal to nrows/ncols of A");
        goto done;
    }

    ITEM_TYPE itype = (itype_a == CSN_COMPLEX || itype_b == CSN_COMPLEX) ? CSN_COMPLEX : CSN_REAL;

    res = linalg_allocate(csound, NULL, false, &p->buffer_a, &p->buffer_b, &p->lu_info, source_arr_a->capacity, source_arr_b->capacity, itype);
    if (res != OK) goto done;

    double *buffer_a = (double *) p->buffer_a.scratch;
    double *buffer_b = (double *) p->buffer_b.scratch;

    if (itype == CSN_COMPLEX && source_arr_a->itype == CSN_REAL) {
        for (size_t i = 0; i < source_arr_a->size; i++) {
            buffer_a[i * 2] = source_arr_a->data[i];
            buffer_a[i * 2 + 1] = 0.0;
        }
    } else {
        memcpy(buffer_a, source_arr_a->data, sizeof(double) * source_arr_a->size * itype);
    }

    if (itype == CSN_COMPLEX && source_arr_b->itype == CSN_REAL) {
        for (size_t i = 0; i < source_arr_b->size; i++) {
            buffer_b[i * 2] = source_arr_b->data[i];
            buffer_b[i * 2 + 1] = 0.0;
        }
    } else {
        memcpy(buffer_b, source_arr_b->data, sizeof(double) * source_arr_b->size * itype);
    }

    uint32_t new_dim = source_arr_b->ndim;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    memcpy(new_shape, source_shape_b, sizeof(uint32_t) * CSN_MAX_DIMS);

    const CSN_ARRAY *ops[2] = { source_arr_a, source_arr_b };
    CSNARR_RES out = prepare_output(csound, &p->h, NULL, reg, p->handle, new_dim, new_shape, itype, ops, 2, CSN_ALIAS_REFUSE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    size_t nrhs = source_ndim_b == 1 ? 1 : (size_t) source_shape_b[source_ndim_b - 1];
    res = solve_helper(csound, NULL, &p->lu_info, buffer_a, buffer_b, source_shape_a[0], nrhs, itype, NULL, NULL);
    if (res != OK) goto done;

    memcpy(p->handle->data, buffer_b, sizeof(double) * source_arr_b->size * itype);

    SET_KDATA_BEGIN(p->k_data, reg, new_shape, new_dim, itype);
    set_array_version(&p->k_data.prev_output_version, &p->handle->version);
    set_array_version(&p->k_data.prev_source_version, &source_arr_a->version);
    set_array_version(&p->k_data.prev_source_version_b, &source_arr_b->version);
    p->is_published = false;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_solve_k(CSOUND *csound, CSN_LINALG_SOLVE *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    int32_t res = OK;
    CHECK_KTRIG(p->trig);

    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle_a, p->source_handle_b }, 3);

    CSN_ARRAY *source_arr_a = p->source_handle_a;
    CSN_ARRAY *source_arr_b = p->source_handle_b;

    if (p->is_published) {
        bool is_same_a = is_same_array_version(&p->k_data.prev_source_version, &source_arr_a->version);
        bool is_same_b = is_same_array_version(&p->k_data.prev_source_version_b, &source_arr_b->version);
        bool is_same_result = is_same_array_version(&p->k_data.prev_output_version, &p->handle->version);
        if (is_same_a && is_same_b && is_same_result) goto done;
    }

    uint32_t source_ndim_a = source_arr_a->ndim;
    uint32_t *source_shape_a = source_arr_a->shape;
    uint32_t source_ndim_b = source_arr_b->ndim;
    uint32_t *source_shape_b = source_arr_b->shape;
    ITEM_TYPE itype_a = source_arr_a->itype;
    ITEM_TYPE itype_b = source_arr_b->itype;

    if (source_ndim_a != 2 || source_shape_a[0] != source_shape_a[1]) {
        res = CSN_ACCESSOR_ERROR_LOCKED(csound, &p->h, "[csnarray] Matrix must be 2-D N x N");
        goto done;
    }

    if (source_ndim_b > 2) {
        res = CSN_ACCESSOR_ERROR_LOCKED(csound, &p->h, "[csnarray] B must be 1-D or 2-D, got %u-D", source_ndim_b);
        goto done;
    }

    if (source_shape_b[0] != source_shape_a[0]) {
        res = CSN_ACCESSOR_ERROR_LOCKED(csound, &p->h, "[csnarray] Incompatible dimensions: shape[0] of B must be equal to nrows/ncols of A");
        goto done;
    }

    ITEM_TYPE itype = (itype_a == CSN_COMPLEX || itype_b == CSN_COMPLEX) ? CSN_COMPLEX : CSN_REAL;
    res = linalg_allocate(csound, &p->h, p->handle->rt_locked, &p->buffer_a, &p->buffer_b, &p->lu_info, source_arr_a->size, source_arr_b->size, itype);
    if (res != OK) goto done;

    double *ba = (double *) p->buffer_a.scratch;
    double *bb = (double *) p->buffer_b.scratch;

    if (itype == CSN_COMPLEX && source_arr_a->itype == CSN_REAL) {
        for (size_t i = 0; i < source_arr_a->size; i++) {
            ba[i * 2] = source_arr_a->data[i];
            ba[i * 2 + 1] = 0.0;
        }
    } else {
        memcpy(ba, source_arr_a->data, sizeof(double) * source_arr_a->size * itype);
    }

    if (itype == CSN_COMPLEX && source_arr_b->itype == CSN_REAL) {
        for (size_t i = 0; i < source_arr_b->size; i++) {
            bb[i * 2] = source_arr_b->data[i];
            bb[i * 2 + 1] = 0.0;
        }
    } else {
        memcpy(bb, source_arr_b->data, sizeof(double) * source_arr_b->size * itype);
    }

    uint32_t new_dim = source_arr_b->ndim;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    memcpy(new_shape, source_shape_b, sizeof(uint32_t) * CSN_MAX_DIMS);

    const CSN_ARRAY *ops[2] = { source_arr_a, source_arr_b };
    CSNARR_RES out = prepare_output(csound, &p->h, &p->h, reg, p->handle, new_dim, new_shape, itype, ops, 2, CSN_ALIAS_REFUSE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    size_t nrhs = source_ndim_b == 1 ? 1 : (size_t) source_shape_b[source_ndim_b - 1];
    res = solve_helper(csound, &p->h, &p->lu_info, ba, bb, source_shape_a[0], nrhs, itype, NULL, NULL);
    if (res != OK) goto done;

    memcpy(p->handle->data, bb, sizeof(double) * source_arr_b->size * itype);

    SET_KDATA_END(p->k_data, new_shape, new_dim, itype);
    set_array_version(&p->k_data.prev_output_version, &p->handle->version);
    set_array_version(&p->k_data.prev_source_version, &source_arr_a->version);
    set_array_version(&p->k_data.prev_source_version_b, &source_arr_b->version);
    p->is_published = true;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_inverse_deinit(CSOUND *csound, CSN_LINALG_INVERSE *p) {
    deinit_scratch(csound, &p->buffer_a);
    deinit_scratch(csound, &p->buffer_b);
    if (p->lu_info.pivots != NULL) {
        csound->Free(csound, p->lu_info.pivots);
        p->lu_info.pivots = NULL;
    }
    return OK;
}

int32_t csnarray_inverse(CSOUND *csound, CSN_LINALG_INVERSE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle }, 2);

    CSN_ARRAY *source_arr = p->source_handle;
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;
    uint32_t n = source_shape[0];
    ITEM_TYPE itype = source_arr->itype;

    if (source_ndim != 2 || source_shape[0] != source_shape[1]) {
        res = csn_init_error(csound, "[csnarray] Matrix must be 2-D N x N");
        goto done;
    }

    res = linalg_allocate(csound, NULL, false, &p->buffer_a, &p->buffer_b, &p->lu_info, source_arr->capacity, source_arr->capacity, itype);
    if (res != OK) goto done;

    double *buffer_a = (double *) p->buffer_a.scratch;
    double *buffer_b = (double *) p->buffer_b.scratch;

    memcpy(buffer_a, source_arr->data, sizeof(double) * source_arr->size * itype);
    linalg_diag(buffer_b, n, itype);

    uint32_t new_dim = source_ndim;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    memcpy(new_shape, source_shape, sizeof(uint32_t) * CSN_MAX_DIMS);

    const CSN_ARRAY *ops[1] = { source_arr };
    CSNARR_RES out = prepare_output(csound, &p->h, NULL, reg, p->handle, new_dim, new_shape, itype, ops, 1, CSN_ALIAS_REFUSE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    size_t nrhs = (size_t) source_shape[source_ndim - 1];
    res = solve_helper(csound, NULL, &p->lu_info, buffer_a, buffer_b, source_shape[0], nrhs, itype, NULL, NULL);
    if (res != OK) goto done;

    memcpy(p->handle->data, buffer_b, sizeof(double) * source_arr->size * itype);

    SET_KDATA_BEGIN(p->k_data, reg, new_shape, new_dim, itype);
    set_array_version(&p->k_data.prev_output_version, &p->handle->version);
    set_array_version(&p->k_data.prev_source_version, &source_arr->version);
    p->is_published = false;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_inverse_k(CSOUND *csound, CSN_LINALG_INVERSE *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    int32_t res = OK;
    CHECK_KTRIG(p->trig);

    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle }, 2);

    CSN_ARRAY *source_arr = p->source_handle;
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;
    uint32_t n = source_shape[0];
    ITEM_TYPE itype = source_arr->itype;

    if (source_ndim != 2 || source_shape[0] != source_shape[1]) {
        res = CSN_ACCESSOR_ERROR_LOCKED(csound, &p->h, "[csnarray] Matrix must be 2-D N x N");
        goto done;
    }

    if (p->is_published) {
        bool is_same_a = is_same_array_version(&p->k_data.prev_source_version, &source_arr->version);
        bool is_same_result = is_same_array_version(&p->k_data.prev_output_version, &p->handle->version);
        if (is_same_a && is_same_result) goto done;
    }

    res = linalg_allocate(csound, &p->h, p->handle->rt_locked, &p->buffer_a, &p->buffer_b, &p->lu_info, source_arr->size, source_arr->size, itype);
    if (res != OK) goto done;

    double *buffer_a = (double *) p->buffer_a.scratch;
    double *buffer_b = (double *) p->buffer_b.scratch;

    memcpy(buffer_a, source_arr->data, sizeof(double) * source_arr->size * itype);
    linalg_diag(buffer_b, n, itype);

    uint32_t new_dim = source_ndim;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    memcpy(new_shape, source_shape, sizeof(uint32_t) * CSN_MAX_DIMS);

    const CSN_ARRAY *ops[1] = { source_arr };
    CSNARR_RES out = prepare_output(csound, &p->h, &p->h, reg, p->handle, new_dim, new_shape, itype, ops, 1, CSN_ALIAS_REFUSE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    size_t nrhs = (size_t) source_shape[source_ndim - 1];
    res = solve_helper(csound, &p->h, &p->lu_info, buffer_a, buffer_b, source_shape[0], nrhs, itype, NULL, NULL);
    if (res != OK) goto done;

    memcpy(p->handle->data, buffer_b, sizeof(double) * source_arr->size * itype);

    SET_KDATA_END(p->k_data, new_shape, new_dim, itype);
    set_array_version(&p->k_data.prev_output_version, &p->handle->version);
    set_array_version(&p->k_data.prev_source_version, &source_arr->version);
    p->is_published = true;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

static int32_t csnarray_determinant_helper(CSOUND *csound, OPDS *h, cs_float *det_real, COMPLEXDAT *det_complex, CSN_ARRAY *source_arr, K_DATA *k_data, bool *is_published, CSN_SCRATCH *buffer, double *prev_real, CSN_COMPLEXDAT *prev_complex, CSN_LU_INFO *lu_info) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    stamp_globals(csound, h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { source_arr }, 1);

    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;
    uint32_t n = source_shape[0];
    ITEM_TYPE itype = source_arr->itype;

    if (source_ndim != 2 || n != source_shape[1]) {
        res = csn_init_error(csound, "[csnarray] Matrix must be 2-D N x N");
        goto done;
    }

    if (det_real != NULL && itype != CSN_REAL) {
        res = csn_init_error(csound, "[csnarray] Input array must be real");
        goto done;
    }

    if (det_complex != NULL && itype != CSN_COMPLEX) {
        res = csn_init_error(csound, "[csnarray] Input array must be complex");
        goto done;
    }

    res = linalg_allocate(csound, NULL, false, buffer, NULL, lu_info, source_arr->capacity, 0, itype);
    if (res != OK) goto done;

    double *lu_buffer = (double *) buffer->scratch;
    memcpy(lu_buffer, source_arr->data, sizeof(double) * source_arr->size * itype);

    double parity = 1.0;
    bool is_singular = false;
    size_t nrhs = (size_t) source_shape[source_ndim - 1];
    res = solve_helper(csound, NULL, lu_info, lu_buffer, NULL, n, nrhs, itype, &parity, &is_singular);
    if (res != OK) goto done;

    /* Singular: the determinant is zero, and the factorization stopped where
       it found that out, so the diagonal below is not a determinant to read. */
    double det_d = is_singular ? 0.0 : parity;
    CSN_COMPLEXDAT det_c = { .re = is_singular ? 0.0 : parity, .im = 0.0 };
    if (is_singular) {
        /* nothing to accumulate */
    } else if (det_real != NULL) {
        for (size_t i = 0; i < n; i++) {
            det_d *= lu_buffer[i * n + i];
        }
    } else {
        for (size_t i = 0; i < n; i++) {
            CSN_COMPLEXDAT a = { .re = lu_buffer[(i * n + i) * 2], .im = lu_buffer[(i * n + i) * 2 + 1] };
            complex_prod(&det_c, det_c, a);
        }
    }

    if (det_real != NULL) {
        *det_real = (cs_float) det_d;
        *prev_real = det_d;
    } else {
        det_complex->real = (cs_float) det_c.re;
        det_complex->imag = (cs_float) det_c.im;
        det_complex->isPolar = 0;

        prev_complex->re = (cs_float) det_c.re;
        prev_complex->im = (cs_float) det_c.im;
    }

    set_array_version(&k_data->prev_source_version, &source_arr->version);
    k_data->registry = reg;
    *is_published = false;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

static int32_t csnarray_determinant_k_helper(CSOUND *csound, OPDS *h, cs_float *det_real, COMPLEXDAT *det_complex, CSN_ARRAY *source_arr, K_DATA *k_data, bool *is_published, CSN_SCRATCH *buffer, double *prev_real, CSN_COMPLEXDAT *prev_complex, CSN_LU_INFO *lu_info) {
    CSN_REGISTRY *reg = k_data->registry;
    CHECK_REGISTRY(csound, h, reg);

    int32_t res = OK;
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { source_arr }, 1);

    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;
    uint32_t n = source_shape[0];
    ITEM_TYPE itype = source_arr->itype;

    if (source_ndim != 2 || n != source_shape[1]) {
        res = CSN_ACCESSOR_ERROR_LOCKED(csound, h, "[csnarray] Matrix must be 2-D N x N");
        goto done;
    }

    if (det_real != NULL && itype != CSN_REAL) {
        res = CSN_ACCESSOR_ERROR_LOCKED(csound, h, "[csnarray] Input array must be real");
        goto done;
    }

    if (det_complex != NULL && itype != CSN_COMPLEX) {
        res = CSN_ACCESSOR_ERROR_LOCKED(csound, h, "[csnarray] Input array must be complex");
        goto done;
    }

    if (*is_published) {
        if (is_same_array_version(&k_data->prev_source_version, &source_arr->version)) {
            if (det_real != NULL) {
                *det_real = (cs_float) *prev_real;
            } else {
                det_complex->real = (cs_float) prev_complex->re;
                det_complex->imag = (cs_float) prev_complex->im;
                det_complex->isPolar = 0;
            }
            goto done;
        }
    }

    res = linalg_allocate(csound, h, source_arr->rt_locked, buffer, NULL, lu_info, source_arr->size, 0, itype);
    if (res != OK) goto done;

    double *lu_buffer = (double *) buffer->scratch;
    memcpy(lu_buffer, source_arr->data, sizeof(double) * source_arr->size * itype);

    double parity = 1.0;
    bool is_singular = false;
    size_t nrhs = (size_t) source_shape[source_ndim - 1];
    res = solve_helper(csound, h, lu_info, lu_buffer, NULL, n, nrhs, itype, &parity, &is_singular);
    if (res != OK) goto done;

    /* Singular: the determinant is zero, and the factorization stopped where
       it found that out, so the diagonal below is not a determinant to read. */
    double det_d = is_singular ? 0.0 : parity;
    CSN_COMPLEXDAT det_c = { .re = is_singular ? 0.0 : parity, .im = 0.0 };
    if (is_singular) {
        /* nothing to accumulate */
    } else if (det_real != NULL) {
        for (size_t i = 0; i < n; i++) {
            det_d *= lu_buffer[i * n + i];
        }
    } else {
        for (size_t i = 0; i < n; i++) {
            CSN_COMPLEXDAT a = { .re = lu_buffer[(i * n + i) * 2], .im = lu_buffer[(i * n + i) * 2 + 1] };
            complex_prod(&det_c, det_c, a);
        }
    }

    if (det_real != NULL) {
        *det_real = (cs_float) det_d;
        *prev_real = det_d;
    } else {
        det_complex->real = (cs_float) det_c.re;
        det_complex->imag = (cs_float) det_c.im;
        det_complex->isPolar = 0;

        prev_complex->re = (cs_float) det_c.re;
        prev_complex->im = (cs_float) det_c.im;
    }

    set_array_version(&k_data->prev_source_version, &source_arr->version);
    *is_published = true;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

static int32_t csnarray_determinant_deinit_helper(CSOUND *csound, void *p) {
    CSN_LINALG_DETERMINANT_COMMON *ptr = (CSN_LINALG_DETERMINANT_COMMON *) p;
    deinit_scratch(csound, &ptr->buffer);
    if (ptr->lu_info.pivots != NULL) {
        csound->Free(csound, ptr->lu_info.pivots);
        ptr->lu_info.pivots = NULL;
    }
    return OK;
}

int32_t csnarray_det_real_deinit(CSOUND *csound, CSN_LINALG_DET_REAL *p) {
    return csnarray_determinant_deinit_helper(csound, (CSN_LINALG_DET_REAL *) p);
}

int32_t csnarray_det_complex_deinit(CSOUND *csound, CSN_LINALG_DET_COMPLEX *p) {
    return csnarray_determinant_deinit_helper(csound, (CSN_LINALG_DET_COMPLEX *) p);
}

int32_t csnarray_determinant_real(CSOUND *csound, CSN_LINALG_DET_REAL *p) {
    return csnarray_determinant_helper(csound, &p->h, p->det, NULL, p->source_handle, &p->k_data, &p->is_published, &p->buffer, &p->prev_det_real, NULL, &p->lu_info);
}

int32_t csnarray_determinant_real_k(CSOUND *csound, CSN_LINALG_DET_REAL *p) {
    return csnarray_determinant_k_helper(csound, &p->h, p->det, NULL, p->source_handle, &p->k_data, &p->is_published, &p->buffer, &p->prev_det_real, NULL, &p->lu_info);
}

int32_t csnarray_determinant_complex(CSOUND *csound, CSN_LINALG_DET_COMPLEX *p) {
    return csnarray_determinant_helper(csound, &p->h, NULL, p->det, p->source_handle, &p->k_data, &p->is_published, &p->buffer, NULL, &p->prev_det_complex, &p->lu_info);
}

int32_t csnarray_determinant_complex_k(CSOUND *csound, CSN_LINALG_DET_COMPLEX *p) {
    return csnarray_determinant_k_helper(csound, &p->h, NULL, p->det, p->source_handle, &p->k_data, &p->is_published, &p->buffer, NULL, &p->prev_det_complex, &p->lu_info);
}


// SAVGOL
static int32_t savgol_validate_params(CSOUND *csound, cs_float winsize_arg, cs_float order_arg, cs_float delta_arg, uint32_t *winsize, uint32_t *ncoef) {
    int32_t w = (int32_t) winsize_arg;
    int32_t o = (int32_t) order_arg;

    if (UNLIKELY(w < 3 || (w & 1) == 0)) {
        return csn_init_error(csound, "[csnarray] Savgol winsize must be odd and at least 3");
    }

    if (UNLIKELY(o < 0 || o >= w)) {
        return csn_init_error(csound, "[csnarray] Savgol order must be >= 0 and less than winsize");
    }

    if (UNLIKELY(delta_arg <= FL(0.0))) {
        return csn_init_error(csound, "[csnarray] Savgol delta must be greater than 0");
    }

    *winsize = (uint32_t) w;
    *ncoef = (uint32_t) o + 1U;
    return OK;
}

static double savgol_dot(const double *a, const double *b, uint32_t n) {
    double sum = 0.0;
    for (uint32_t i = 0; i < n; i++) sum += a[i] * b[i];
    return sum;
}

/* Row deriv of the coefficient matrix, scaled by 1 / delta^deriv so that the
   convolution yields the deriv-th derivative per unit of delta. */
static void get_coeffs(const CSN_SAVGOL_BUFFER *sg, double *coeffs_buffer, uint32_t deriv, double delta) {
    double scale = 1.0 / pow(delta, (double) deriv);
    const double *row = sg->coeffs + (size_t) deriv * sg->ncols;
    for (uint32_t i = 0; i < sg->ncols; i++) {
        coeffs_buffer[i] = row[i] * scale;
    }
}

/* Fills sg->coeffs (ncoef * winsize doubles, allocated by the caller): row d
   holds the weights giving the d-th derivative of the least-squares fit at
   the centre of the window, for a sample spacing of 1. Silent: NOTOK when
   memory runs out or the arithmetic breaks down, which distinct abscissae
   otherwise rule out, and the caller reports it.

   The normal equations (A^T A)^-1 A^T square the exponential conditioning
   of the Vandermonde matrix A[i][j] = (i - centre)^j: they lose digits from
   about order 10 and leave a pivot too small to trust well before the order
   reaches the window, at 41/20 or even 101/8. Instead the abscissae are
   scaled to [-1, 1] and Arnoldi iteration (Gram-Schmidt run twice, as in
   Brubeck, Nakatsukasa and Trefethen, "Vandermonde with Arnoldi", SIAM
   Review 63, 2021) builds polynomials q_0 .. q_o orthonormal over the
   window. The fit is the projection sum_k (q_k . y) q_k, and its derivatives
   at the centre follow from the recurrence that defines q_k, differentiated.
   Every accepted order comes out within a few ulps of the exact
   coefficients. */
static int32_t calculate_savgol_coeffs(CSOUND *csound, CSN_SAVGOL_BUFFER *sg, uint32_t winsize, uint32_t ncoef) {
    double half = (double) (winsize - 1U) * 0.5;
    double *t = (double *) csound->Calloc(csound, sizeof(double) * (size_t) winsize);
    double *q = (double *) csound->Calloc(csound, sizeof(double) * (size_t) ncoef * winsize);
    double *h = (double *) csound->Calloc(csound, sizeof(double) * (size_t) ncoef * ncoef);
    double *deriv = (double *) csound->Calloc(csound, sizeof(double) * (size_t) ncoef * ncoef);
    int32_t res = t != NULL && q != NULL && h != NULL && deriv != NULL ? OK : NOTOK;

    if (res == OK) {
        for (uint32_t i = 0; i < winsize; i++) {
            t[i] = ((double) i - half) / half;
            q[i] = 1.0 / sqrt((double) winsize);
        }
    }

    /* Arnoldi: q_k = (t * q_{k-1} - sum_j h[j][k] q_j) / h[k][k]. */
    for (uint32_t k = 1; k < ncoef && res == OK; k++) {
        double *qk = q + (size_t) k * winsize;
        const double *prev = qk - winsize;

        for (uint32_t i = 0; i < winsize; i++) qk[i] = t[i] * prev[i];

        for (int32_t pass = 0; pass < 2; pass++) {
            for (uint32_t j = 0; j < k; j++) {
                const double *qj = q + (size_t) j * winsize;
                double c = savgol_dot(qj, qk, winsize);
                h[j * ncoef + k] += c;
                for (uint32_t i = 0; i < winsize; i++) qk[i] -= c * qj[i];
            }
        }

        double norm = sqrt(savgol_dot(qk, qk, winsize));
        if (!(norm > 0.0) || !isfinite(norm)) {
            res = NOTOK;
            break;
        }
        h[k * ncoef + k] = norm;
        for (uint32_t i = 0; i < winsize; i++) qk[i] /= norm;
    }

    if (res == OK) {
        /* deriv[d][k] is the d-th derivative of q_k at t = 0; q_k has degree
           k, so the higher derivatives stay zero. */
        deriv[0] = 1.0 / sqrt((double) winsize);
        for (uint32_t k = 1; k < ncoef; k++) {
            double hkk = h[k * ncoef + k];
            for (uint32_t d = 0; d <= k; d++) {
                double sum = d > 0 ? (double) d * deriv[(d - 1) * ncoef + k - 1] : 0.0;
                for (uint32_t j = 0; j < k; j++) sum -= h[j * ncoef + k] * deriv[d * ncoef + j];
                deriv[d * ncoef + k] = sum / hkk;
            }
        }

        /* Sample i weighs sum_k deriv[d][k] q_k[i] in the d-th derivative of
           the fit at the centre; 1 / half^d converts from t to samples. */
        double scale = 1.0;
        for (uint32_t d = 0; d < ncoef && res == OK; d++) {
            double *row = sg->coeffs + (size_t) d * winsize;
            for (uint32_t i = 0; i < winsize; i++) {
                double sum = 0.0;
                for (uint32_t k = d; k < ncoef; k++) sum += deriv[d * ncoef + k] * q[(size_t) k * winsize + i];
                row[i] = sum * scale;
                if (!isfinite(row[i])) res = NOTOK;
            }
            scale /= half;
        }
    }

    if (t != NULL) csound->Free(csound, t);
    if (q != NULL) csound->Free(csound, q);
    if (h != NULL) csound->Free(csound, h);
    if (deriv != NULL) csound->Free(csound, deriv);

    sg->nrows = ncoef;
    sg->ncols = winsize;
    return res;
}

int32_t csnarray_savgol_mat(CSOUND *csound, CSN_SAVGOL_MATRIX *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    stamp_globals(csound, &p->h);

    CSN_SAVGOL_BUFFER sg = { NULL, 0, 0 };
    double *row = NULL;

    uint32_t winsize;
    uint32_t ncoef;
    res = savgol_validate_params(csound, *p->winsize, *p->order, *p->delta, &winsize, &ncoef);
    if (res != OK) return res;

    double *c = csound->Calloc(csound, sizeof(double) * (size_t) ncoef * winsize);
    if (c == NULL) {
        return csn_init_error(csound, "[csnarray] Internal error: memory allocation failed");
    }
    sg.coeffs = c;

    if (calculate_savgol_coeffs(csound, &sg, winsize, ncoef) != OK) {
        csound->Free(csound, sg.coeffs);
        return csn_init_error(csound, "[csnarray] Could not compute savgol coefficients");
    }

    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle }, 1);

    uint32_t ndim = 2U;
    uint32_t shape[CSN_MAX_DIMS] = {0};
    shape[0] = sg.nrows;
    shape[1] = sg.ncols;

    CSNARR_RES out = prepare_output(csound, &p->h, NULL, reg, p->handle, ndim, shape, CSN_REAL, NULL, 0, CSN_ALIAS_REFUSE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    row = (double *) csound->Calloc(csound, sizeof(double) * (size_t) sg.ncols);
    if (row == NULL) {
        res = csn_init_error(csound, "[csnarray] Internal error: memory allocation failed");
        goto done;
    }

    CSN_ARRAY *arr = p->handle;
    for (uint32_t i = 0; i < sg.nrows; i++) {
        get_coeffs(&sg, row, i, (double) *p->delta);
        for (uint32_t j = 0; j < sg.ncols; j++) {
            arr->data[i * sg.ncols + j] = row[j];
        }
    }

done:
    if (sg.coeffs != NULL) csound->Free(csound, sg.coeffs);
    if (row != NULL) csound->Free(csound, row);
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_hilbertmat(CSOUND *csound, CSN_HILBERT_MAT *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    double n_value = (double) *p->n;
    if (!IS_VALID_LENGTH(n_value)) {
        return csn_init_error(csound, "[csnarray] Matrix size must be a valid value");
    }
    size_t n = (size_t) n_value;

    int32_t res = OK;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle }, 1);

    uint32_t new_ndim = 2U;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    new_shape[0] = (uint32_t) n;
    new_shape[1] = (uint32_t) n;
    ITEM_TYPE itype = CSN_REAL;

    CSNARR_RES out = prepare_output(csound, &p->h, NULL, reg, p->handle, new_ndim, new_shape, itype, NULL, 0, CSN_ALIAS_REFUSE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    CSN_ARRAY *matrix = p->handle;
    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j < n; j++) {
            matrix->data[i * n + j] = 1.0 / ((double) (i + j) + 1.0);
        }
    }

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_hilbertmat_k_init(CSOUND *csound, CSN_HILBERT_MAT *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle }, 1);

    uint32_t new_ndim = 2U;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    new_shape[0] = 1U;
    new_shape[1] = 1U;
    ITEM_TYPE itype = CSN_REAL;

    CSNARR_RES out = prepare_output(csound, &p->h, NULL, reg, p->handle, new_ndim, new_shape, itype, NULL, 0, CSN_ALIAS_REFUSE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    reset_empty_csnarray(p->handle, new_ndim, new_shape, itype);

    SET_KDATA_BEGIN(p->k_data, reg, new_shape, new_ndim, itype);
    set_array_version(&p->k_data.prev_output_version, &p->handle->version);
    p->is_published = false;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_hilbertmat_k(CSOUND *csound, CSN_HILBERT_MAT *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    double n_value = (double) *p->n;
    if (!IS_VALID_LENGTH(n_value)) {
        return csn_perf_error(csound, &p->h, "[csnarray] Matrix size must be a valid value");
    }
    size_t n = (size_t) n_value;

    int32_t res = OK;
    CHECK_KTRIG(p->trig);

    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle }, 1);

    if (p->is_published) {
        bool is_same_n = p->k_data.prev_size == n;
        bool is_same_result = is_same_array_version(&p->k_data.prev_output_version, &p->handle->version);
        if (is_same_n && is_same_result) goto done;
    }

    uint32_t new_ndim = 2U;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    new_shape[0] = (uint32_t) n;
    new_shape[1] = (uint32_t) n;
    ITEM_TYPE itype = CSN_REAL;

    CSNARR_RES out = prepare_output(csound, &p->h, &p->h, reg, p->handle, new_ndim, new_shape, itype, NULL, 0, CSN_ALIAS_REFUSE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    CSN_ARRAY *matrix = p->handle;
    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j < n; j++) {
            matrix->data[i * n + j] = 1.0 / ((double) (i + j) + 1.0);
        }
    }

    SET_KDATA_END(p->k_data, new_shape, new_ndim, itype);
    set_array_version(&p->k_data.prev_output_version, &p->handle->version);
    p->k_data.prev_size = n;
    p->is_published = true;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}
