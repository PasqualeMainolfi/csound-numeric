#include "csnlinalg.h"
#include "csnregistry.h"
#include "csnum.h"
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

static int32_t linalg_allocate(CSOUND *csound, OPDS *perf_h, CSN_SCRATCH *a, CSN_SCRATCH *b, CSN_LU_INFO *info, size_t size_a, size_t size_b, size_t n, ITEM_TYPE itype) {
    double *buffer_a = NULL;
    double *buffer_b = NULL;
    size_t *pivots = NULL;
    size_t ba_cap = size_a * 2;
    size_t bb_cap = size_b * 2;
    if (perf_h == NULL) {
        buffer_a = csound->Calloc(csound, sizeof(double) * ba_cap * itype);
        if (buffer_a == NULL) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Internal error: memory allocation failed");
        }
        if (b != NULL) {
            buffer_b = csound->Calloc(csound, sizeof(double) * bb_cap * itype);
            if (buffer_b == NULL) {
                return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Internal error: memory allocation failed");
            }
            b->scratch = buffer_b;
            b->scratch_capacity = bb_cap;
        }
        a->scratch = buffer_a;
        a->scratch_capacity = ba_cap;
        pivots = csound->Calloc(csound, sizeof(size_t) * ba_cap);
        if (pivots == NULL) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Internal error: memory allocation failed");
        }
        info->pivots = pivots;
    } else {
        if (size_a > a->scratch_capacity) {
            buffer_a = csound->ReAlloc(csound, a->scratch, sizeof(double) * ba_cap * itype);
            if (buffer_a == NULL) {
                return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Internal error: memory allocation failed");
            }

            a->scratch = buffer_a;
            a->scratch_capacity = ba_cap;

            pivots = csound->ReAlloc(csound, info->pivots, sizeof(size_t) * ba_cap);
            if (pivots == NULL) {
                return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Internal error: memory allocation failed");
            }
            info->pivots = pivots;
        }

        if (b != NULL) {
            if (size_b > b->scratch_capacity) {
                buffer_b = csound->ReAlloc(csound, b->scratch, sizeof(double) * bb_cap * itype);
                if (buffer_b == NULL) {
                    return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Internal error: memory allocation failed");
                }

                b->scratch = buffer_b;
                b->scratch_capacity = bb_cap;
            }
        }
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
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

int32_t csnarray_solve(CSOUND *csound, CSN_LINALG_SOLVE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle_a = p->source_handle_a->id;
    uint32_t source_handle_b = p->source_handle_b->id;

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
    uint32_t source_ndim_a = source_arr_a->ndim;
    uint32_t *source_shape_a = source_arr_a->shape;
    uint32_t source_ndim_b = source_arr_b->ndim;
    uint32_t *source_shape_b = source_arr_b->shape;
    ITEM_TYPE itype_a = source_arr_a->itype;
    ITEM_TYPE itype_b = source_arr_b->itype;

    if (source_ndim_a != 2 || source_shape_a[0] != source_shape_a[1]) {
        res = csound->InitError(csound, "[csnarray] Matrix must be 2-D N x N");
        goto done;
    }

    if (source_shape_b[0] != source_shape_a[0]) {
        res = csound->InitError(csound, "[csnarray] Incompatible dimensions: shape[0] of B must be equal to nrows/ncols of A");
        goto done;
    }

    ITEM_TYPE itype = (itype_a == CSN_COMPLEX || itype_b == CSN_COMPLEX) ? CSN_COMPLEX : CSN_REAL;

    res = linalg_allocate(csound, NULL, &p->buffer_a, &p->buffer_b, &p->lu_info, source_arr_a->size, source_arr_b->size, (size_t) source_shape_a[0], itype);
    if (res != OK) goto done;

    double *buffer_a = (double *) p->buffer_a.scratch;
    double *buffer_b = (double *) p->buffer_b.scratch;

    if (itype == CSN_COMPLEX && source_arr_a->itype == CSN_REAL) {
        for (size_t i = 0; i < source_arr_a->size; i++) {
            buffer_a[i * 2] = source_arr_a->data[i];
        }
    } else {
        memcpy(buffer_a, source_arr_a->data, sizeof(double) * source_arr_a->size * itype);
    }

    if (itype == CSN_COMPLEX && source_arr_b->itype == CSN_REAL) {
        for (size_t i = 0; i < source_arr_b->size; i++) {
            buffer_b[i * 2] = source_arr_b->data[i];
        }
    } else {
        memcpy(buffer_b, source_arr_b->data, sizeof(double) * source_arr_b->size * itype);
    }

    uint32_t new_dim = source_arr_b->ndim;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    memcpy(new_shape, source_shape_b, sizeof(uint32_t) * CSN_MAX_DIMS);

    uint32_t protect[2] = { source_handle_a, source_handle_b };
    if (create_csnarray_locked(csound, reg, &p->h, new_dim, new_shape, &p->array, p->handle, protect, 2U, &err, itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    size_t nrhs = source_ndim_b == 1 ? 1 : (size_t) source_shape_b[source_ndim_b - 1];
    res = solve_helper(csound, NULL, &p->lu_info, buffer_a, buffer_b, source_shape_a[0], nrhs, itype, NULL, NULL);
    if (res != OK) goto done;

    memcpy(p->array->data, buffer_b, sizeof(double) * source_arr_b->size * itype);

    SET_KDATA_BEGIN(p, reg);
    set_array_version(&p->k_data.prev_output_version, &p->array->version);
    set_array_version(&p->k_data.prev_source_version, &source_arr_a->version);
    set_array_version(&p->k_data.prev_source_version_b, &source_arr_b->version);
    p->is_published = false;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_solve_k(CSOUND *csound, CSN_LINALG_SOLVE *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    uint32_t owned_handle = p->k_data.owned_handle;
    CHECK_REG_HANDLE(csound, &p->h, reg, owned_handle);

    uint32_t source_handle_a = p->source_handle_a->id;
    uint32_t source_handle_b = p->source_handle_b->id;

    int32_t res = OK;
    const char *err = NULL;

    CHECK_KTRIG(p->trig);

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot_a = get_slot(reg, source_handle_a);
    if (slot_a == NULL) {
        res = csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle_a);
        goto done;
    }

    CSN_SLOT *slot_b = get_slot(reg, source_handle_b);
    if (slot_b == NULL) {
        res = csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle_b);
        goto done;
    }

    CSN_ARRAY *source_arr_a = slot_a->array;
    CSN_ARRAY *source_arr_b = slot_b->array;

    if (p->is_published) {
        bool is_same_a = is_same_array_version(&p->k_data.prev_source_version, &source_arr_a->version);
        bool is_same_b = is_same_array_version(&p->k_data.prev_source_version_b, &source_arr_b->version);
        bool is_same_result = false;
        CSN_SLOT *slot = get_slot(reg, owned_handle);
        if (slot != NULL) {
            is_same_result = is_same_array_version(&p->k_data.prev_output_version, &slot->array->version);
        }

        if (is_same_a && is_same_b && is_same_result) {
            p->handle->id = owned_handle;
            goto done;
        }
    }

    uint32_t source_ndim_a = source_arr_a->ndim;
    uint32_t *source_shape_a = source_arr_a->shape;
    uint32_t source_ndim_b = source_arr_b->ndim;
    uint32_t *source_shape_b = source_arr_b->shape;
    ITEM_TYPE itype_a = source_arr_a->itype;
    ITEM_TYPE itype_b = source_arr_b->itype;

    if (source_ndim_a != 2 || source_shape_a[0] != source_shape_a[1]) {
        res = csound->PerfError(csound, &p->h, "[csnarray] Matrix must be 2-D N x N");
        goto done;
    }

    if (source_shape_b[0] != source_shape_a[0]) {
        res = csound->PerfError(csound, &p->h, "[csnarray] Incompatible dimensions: shape[0] of B must be equal to nrows/ncols of A");
        goto done;
    }

    ITEM_TYPE itype = (itype_a == CSN_COMPLEX || itype_b == CSN_COMPLEX) ? CSN_COMPLEX : CSN_REAL;
    res = linalg_allocate(csound, &p->h, &p->buffer_a, &p->buffer_b, &p->lu_info, source_arr_a->size, source_arr_b->size, source_shape_a[0], itype);
    if (res != OK) goto done;

    double *ba = (double *) p->buffer_a.scratch;
    double *bb = (double *) p->buffer_b.scratch;

    if (itype == CSN_COMPLEX && source_arr_a->itype == CSN_REAL) {
        for (size_t i = 0; i < source_arr_a->size; i++) {
            ba[i * 2] = source_arr_a->data[i];
        }
    } else {
        memcpy(ba, source_arr_a->data, sizeof(double) * source_arr_a->size * itype);
    }

    if (itype == CSN_COMPLEX && source_arr_b->itype == CSN_REAL) {
        for (size_t i = 0; i < source_arr_b->size; i++) {
            bb[i * 2] = source_arr_b->data[i];
        }
    } else {
        memcpy(bb, source_arr_b->data, sizeof(double) * source_arr_b->size * itype);
    }

    uint32_t new_dim = source_arr_b->ndim;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    memcpy(new_shape, source_shape_b, sizeof(uint32_t) * CSN_MAX_DIMS);

    size_t req_size = 0;
    if (get_array_size_from_shape(&req_size, new_dim, new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSN_ARRAY *arr = NULL;
    size_t logical_size = req_size;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, new_dim, new_shape, logical_size, itype, err);
    if (res != OK) goto done;
    p->array = arr;

    size_t nrhs = source_ndim_b == 1 ? 1 : (size_t) source_shape_b[source_ndim_b - 1];
    res = solve_helper(csound, &p->h, &p->lu_info, ba, bb, source_shape_a[0], nrhs, itype, NULL, NULL);
    if (res != OK) goto done;

    memcpy(p->array->data, bb, sizeof(double) * source_arr_b->size * itype);

    SET_KDATA_END(p, new_shape, new_dim, itype);
    set_array_version(&p->k_data.prev_output_version, &p->array->version);
    set_array_version(&p->k_data.prev_source_version, &source_arr_a->version);
    set_array_version(&p->k_data.prev_source_version_b, &source_arr_b->version);
    p->is_published = true;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_inverse_deinit(CSOUND *csound, CSN_LINALG_INVERSE *p) {
    deinit_scratch(csound, &p->buffer_a);
    deinit_scratch(csound, &p->buffer_b);
    if (p->lu_info.pivots != NULL) {
        csound->Free(csound, p->lu_info.pivots);
        p->lu_info.pivots = NULL;
    }
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

int32_t csnarray_inverse(CSOUND *csound, CSN_LINALG_INVERSE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle_a->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
        goto done;
    }

    CSN_ARRAY *source_arr_a = slot->array;
    uint32_t source_ndim_a = source_arr_a->ndim;
    uint32_t *source_shape_a = source_arr_a->shape;
    uint32_t n = source_shape_a[0];
    ITEM_TYPE itype = source_arr_a->itype;

    if (source_ndim_a != 2 || source_shape_a[0] != source_shape_a[1]) {
        res = csound->InitError(csound, "[csnarray] Matrix must be 2-D N x N");
        goto done;
    }

    res = linalg_allocate(csound, NULL, &p->buffer_a, &p->buffer_b, &p->lu_info, source_arr_a->size, source_arr_a->size, (size_t) n, itype);
    if (res != OK) goto done;

    double *buffer_a = (double *) p->buffer_a.scratch;
    double *buffer_b = (double *) p->buffer_b.scratch;

    memcpy(buffer_a, source_arr_a->data, sizeof(double) * source_arr_a->size * itype);
    linalg_diag(buffer_b, n, itype);

    uint32_t new_dim = source_ndim_a;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    memcpy(new_shape, source_shape_a, sizeof(uint32_t) * CSN_MAX_DIMS);

    if (create_csnarray_locked(csound, reg, &p->h, new_dim, new_shape, &p->array, p->handle, &source_handle, 1U, &err, itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    size_t nrhs = (size_t) source_shape_a[source_ndim_a - 1];
    res = solve_helper(csound, NULL, &p->lu_info, buffer_a, buffer_b, source_shape_a[0], nrhs, itype, NULL, NULL);

    memcpy(p->array->data, buffer_b, sizeof(double) * source_arr_a->size * itype);

    SET_KDATA_BEGIN(p, reg);
    set_array_version(&p->k_data.prev_output_version, &p->array->version);
    set_array_version(&p->k_data.prev_source_version, &source_arr_a->version);
    p->is_published = false;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_inverse_k(CSOUND *csound, CSN_LINALG_INVERSE *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    uint32_t owned_handle = p->k_data.owned_handle;
    CHECK_REG_HANDLE(csound, &p->h, reg, owned_handle);

    uint32_t source_handle = p->source_handle_a->id;

    int32_t res = OK;
    const char *err = NULL;

    CHECK_KTRIG(p->trig);

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
        goto done;
    }

    CSN_ARRAY *source_arr_a = slot->array;
    uint32_t source_ndim_a = source_arr_a->ndim;
    uint32_t *source_shape_a = source_arr_a->shape;
    uint32_t n = source_shape_a[0];
    ITEM_TYPE itype = source_arr_a->itype;

    if (source_ndim_a != 2 || source_shape_a[0] != source_shape_a[1]) {
        res = csound->PerfError(csound, &p->h, "[csnarray] Matrix must be 2-D N x N");
        goto done;
    }

    if (p->is_published) {
        bool is_same_a = is_same_array_version(&p->k_data.prev_source_version, &source_arr_a->version);
        bool is_same_result = false;
        CSN_SLOT *slot = get_slot(reg, owned_handle);
        if (slot != NULL) {
            is_same_result = is_same_array_version(&p->k_data.prev_output_version, &slot->array->version);
        }

        if (is_same_a && is_same_result) {
            p->handle->id = owned_handle;
            goto done;
        }
    }

    res = linalg_allocate(csound, &p->h, &p->buffer_a, &p->buffer_b, &p->lu_info, source_arr_a->size, source_arr_a->size, (size_t) n, itype);
    if (res != OK) goto done;

    double *buffer_a = (double *) p->buffer_a.scratch;
    double *buffer_b = (double *) p->buffer_b.scratch;

    memcpy(buffer_a, source_arr_a->data, sizeof(double) * source_arr_a->size * itype);
    linalg_diag(buffer_b, n, itype);

    uint32_t new_dim = source_ndim_a;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    memcpy(new_shape, source_shape_a, sizeof(uint32_t) * CSN_MAX_DIMS);

    size_t req_size = 0;
    if (get_array_size_from_shape(&req_size, new_dim, new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSN_ARRAY *arr = NULL;
    size_t logical_size = req_size;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, new_dim, new_shape, logical_size, itype, err);
    if (res != OK) goto done;
    p->array = arr;

    size_t nrhs = (size_t) source_shape_a[source_ndim_a - 1];
    res = solve_helper(csound, NULL, &p->lu_info, buffer_a, buffer_b, source_shape_a[0], nrhs, itype, NULL, NULL);

    memcpy(p->array->data, buffer_b, sizeof(double) * source_arr_a->size * itype);

    SET_KDATA_BEGIN(p, reg);
    set_array_version(&p->k_data.prev_output_version, &p->array->version);
    set_array_version(&p->k_data.prev_source_version, &source_arr_a->version);
    p->is_published = true;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_determinant_helper(CSOUND *csound, MYFLT *det_real, COMPLEXDAT *det_complex, CSNREF *source_handle_a, K_DATA *k_data, bool *is_published, CSN_SCRATCH *buffer, double *prev_real, CSN_COMPLEXDAT *prev_complex, CSN_LU_INFO *lu_info) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = source_handle_a->id;

    int32_t res = OK;

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
        goto done;
    }

    CSN_ARRAY *source_arr_a = slot->array;
    uint32_t source_ndim_a = source_arr_a->ndim;
    uint32_t *source_shape_a = source_arr_a->shape;
    uint32_t n = source_shape_a[0];
    ITEM_TYPE itype = source_arr_a->itype;

    if (source_ndim_a != 2 || n != source_shape_a[1]) {
        res = csound->InitError(csound, "[csnarray] Matrix must be 2-D N x N");
        goto done;
    }

    if (det_real != NULL && itype != CSN_REAL) {
        res = csound->InitError(csound, "[csnarray] Input array must be real");
        goto done;
    }

    if (det_complex != NULL && itype != CSN_COMPLEX) {
        res = csound->InitError(csound, "[csnarray] Input array must be complex");
        goto done;
    }

    res = linalg_allocate(csound, NULL, buffer, NULL, lu_info, source_arr_a->size, 0, (size_t) n, itype);
    if (res != OK) goto done;

    double *lu_buffer = (double *) buffer->scratch;
    memcpy(lu_buffer, source_arr_a->data, sizeof(double) * source_arr_a->size * itype);

    double parity = 1.0;
    bool is_singular = false;
    size_t nrhs = (size_t) source_shape_a[source_ndim_a - 1];
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
        *det_real = (MYFLT) det_d;
        *prev_real = det_d;
    } else {
        det_complex->real = (MYFLT) det_c.re;
        det_complex->imag = (MYFLT) det_c.im;
        det_complex->isPolar = 0;

        prev_complex->re = (MYFLT) det_c.re;
        prev_complex->im = (MYFLT) det_c.im;
    }

    set_array_version(&k_data->prev_source_version, &source_arr_a->version);
    k_data->registry = reg;
    *is_published = false;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_determinant_k_helper(CSOUND *csound, OPDS *h, MYFLT *det_real, COMPLEXDAT *det_complex, CSNREF *source_handle_a, K_DATA *k_data, bool *is_published, CSN_SCRATCH *buffer, double *prev_real, CSN_COMPLEXDAT *prev_complex, CSN_LU_INFO *lu_info) {
    CSN_REGISTRY *reg = k_data->registry;
    CHECK_REGISTRY(csound, h, reg);

    uint32_t source_handle = source_handle_a->id;

    int32_t res = OK;

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->PerfError(csound, h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
        goto done;
    }

    CSN_ARRAY *source_arr_a = slot->array;
    uint32_t source_ndim_a = source_arr_a->ndim;
    uint32_t *source_shape_a = source_arr_a->shape;
    uint32_t n = source_shape_a[0];
    ITEM_TYPE itype = source_arr_a->itype;

    if (source_ndim_a != 2 || n != source_shape_a[1]) {
        res = csound->PerfError(csound, h, "[csnarray] Matrix must be 2-D N x N");
        goto done;
    }

    if (*is_published) {
        if (is_same_array_version(&k_data->prev_source_version, &source_arr_a->version)) {
            if (det_real != NULL) {
                *det_real = (MYFLT) *prev_real;
            } else {
                det_complex->real = (MYFLT) prev_complex->re;
                det_complex->imag = (MYFLT) prev_complex->im;
                det_complex->isPolar = 0;
            }
            goto done;
        }
    }

    res = linalg_allocate(csound, h, buffer, NULL, lu_info, source_arr_a->size, 0, (size_t) n, itype);
    if (res != OK) goto done;

    double *lu_buffer = (double *) buffer->scratch;
    memcpy(lu_buffer, source_arr_a->data, sizeof(double) * source_arr_a->size * itype);

    double parity = 1.0;
    bool is_singular = false;
    size_t nrhs = (size_t) source_shape_a[source_ndim_a - 1];
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
        *det_real = (MYFLT) det_d;
        *prev_real = det_d;
    } else {
        det_complex->real = (MYFLT) det_c.re;
        det_complex->imag = (MYFLT) det_c.im;
        det_complex->isPolar = 0;

        prev_complex->re = (MYFLT) det_c.re;
        prev_complex->im = (MYFLT) det_c.im;
    }

    set_array_version(&k_data->prev_source_version, &source_arr_a->version);
    *is_published = true;

done:
    csound->UnlockMutex(reg->mutex);
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
    return csnarray_determinant_helper(csound, p->det, NULL, p->source_handle, &p->k_data, &p->is_published, &p->buffer, &p->prev_det_real, NULL, &p->lu_info);
}

int32_t csnarray_determinant_real_k(CSOUND *csound, CSN_LINALG_DET_REAL *p) {
    return csnarray_determinant_k_helper(csound, &p->h, p->det, NULL, p->source_handle, &p->k_data, &p->is_published, &p->buffer, &p->prev_det_real, NULL, &p->lu_info);
}

int32_t csnarray_determinant_complex(CSOUND *csound, CSN_LINALG_DET_COMPLEX *p) {
    return csnarray_determinant_helper(csound, NULL, p->det, p->source_handle, &p->k_data, &p->is_published, &p->buffer, NULL, &p->prev_det_complex, &p->lu_info);
}

int32_t csnarray_determinant_complex_k(CSOUND *csound, CSN_LINALG_DET_COMPLEX *p) {
    return csnarray_determinant_k_helper(csound, &p->h, NULL, p->det, p->source_handle, &p->k_data, &p->is_published, &p->buffer, NULL, &p->prev_det_complex, &p->lu_info);
}
