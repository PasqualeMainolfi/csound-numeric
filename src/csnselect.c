/* Opcode implementations for the select family.
   The public opcode inventory remains centralized in csnum.c. */
#include "csnset.h"
#include "csnstrings.h"
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

/* The single definition of every comparison. The counting pass and the filling
   pass both go through here, so they cannot disagree about what matches.

   NaN follows IEEE, which is also what numpy reports: the ordered comparisons
   and EQUAL are false against a NaN, NOT_EQUAL is true, and NONZERO treats it
   as nonzero. IS_NAN exists because none of those can single a NaN out. */
static inline bool compare_match(double value, double cmp_value, CSN_COMPARE_MODE mode) {
    switch (mode) {
        case GREATER_THAN:  return value > cmp_value;
        case LESS_THAN:     return value < cmp_value;
        case EQUAL:         return value == cmp_value;
        case GREATER_EQUAL: return value >= cmp_value;
        case LESS_EQUAL:    return value <= cmp_value;
        case NOT_EQUAL:     return value != cmp_value;
        case NONZERO:       return value != 0.0;
        case IS_NAN:        return isnan(value) != 0;
        case IS_FIN:        return isfinite(value) != 0;
        case IS_INF:        return isinf(value) != 0;
    }
    return false;
}

/* compare_match over a whole array with the mode picked once, outside the
   loop: out[i] is 1 or 0 (out may be src), or, with out NULL, the matches are
   only counted. Same tests as compare_match, NaN included. */
static size_t compare_fill(double *out, const double *src, size_t n, double cmp, CSN_COMPARE_MODE mode) {
#define CSN_CMP_LOOP(test) \
    if (out != NULL) { for (size_t i = 0; i < n; i++) { double v = src[i]; out[i] = (test) ? 1.0 : 0.0; } return 0; } \
    else { size_t c = 0; for (size_t i = 0; i < n; i++) { double v = src[i]; c += (test) ? 1U : 0U; } return c; }
    switch (mode) {
        case GREATER_THAN:  CSN_CMP_LOOP(v > cmp)
        case LESS_THAN:     CSN_CMP_LOOP(v < cmp)
        case EQUAL:         CSN_CMP_LOOP(v == cmp)
        case GREATER_EQUAL: CSN_CMP_LOOP(v >= cmp)
        case LESS_EQUAL:    CSN_CMP_LOOP(v <= cmp)
        case NOT_EQUAL:     CSN_CMP_LOOP(v != cmp)
        case NONZERO:       CSN_CMP_LOOP(v != 0.0)
        case IS_NAN:        CSN_CMP_LOOP(isnan(v) != 0)
        case IS_FIN:        CSN_CMP_LOOP(isfinite(v) != 0)
        case IS_INF:        CSN_CMP_LOOP(isinf(v) != 0)
    }
#undef CSN_CMP_LOOP
    return 0;
}

static size_t count_elements_from_array(const CSN_ARRAY *source_arr, const CSN_ARRAY *data_arr, CSN_COMPARE_MODE mode) {
    size_t count = 0;
    for (size_t linear = 0; linear < source_arr->size; ++linear) {
        double value = source_arr->data[linear];
        for (size_t i = 0; i < data_arr->size; ++i) {
            if (compare_match(value, data_arr->data[i], mode)) {
                count++;
                break;
            }
        }
    }
    return count;
}

static int32_t get_linear_index_first_occurence(const CSN_ARRAY *source_arr, const double wanted, CSN_COMPARE_MODE mode) {
    int32_t index = -1;
    for (size_t linear = 0; linear < source_arr->size; ++linear) {
        double value = source_arr->data[linear];
        if (compare_match(value, wanted, mode)) {
            index = (int32_t) linear;
            break;
        }
    }
    return index;
}

static size_t count_elements_from_value(const CSN_ARRAY *source_arr, double cmp_value, CSN_COMPARE_MODE mode) {
    return compare_fill(NULL, source_arr->data, source_arr->size, cmp_value, mode);
}

static int32_t argwhere_body(CSOUND *csound, OPDS *perf_h, CSN_ARRAY *source_arr, CSN_ARRAY *data_arr) {
    if (source_arr->itype != CSN_REAL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] This operation is not implemented for complex arrays");
    }

    if (data_arr != NULL) {
        uint32_t data_ndim = data_arr->ndim;
        if (data_ndim != 1U) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Data array must be 1-D, got %u-D", data_ndim);
        }
    }

    return OK;
}

static int32_t argwhere_assign_value(CSN_ARRAY *source_arr, CSN_ARRAY *data_arr, CSN_ARRAY *arr) {
    CSN_BROADCAST_ITER it;
    if (ND_ITER_INIT(&it, source_arr->ndim, source_arr->shape, source_arr->size, NULL) != OK) return NOTOK;
    size_t match = 0;
    while (BROADCAST_ITER_NEXT(&it)) {
        double value = source_arr->data[it.linear_index];
        bool found = false;
        for (size_t i = 0; i < data_arr->size; ++i) {
            if (value == data_arr->data[i]) {
                found = true;
                break;
            }
        }

        if (!found) continue;

        for (size_t j = 0; j < source_arr->ndim; ++j) {
            arr->data[match * source_arr->ndim + j] = (double) it.coords[j];
        }

        match++;
    }
    return OK;
}

static int32_t indexof_assign_value(CSN_ARRAY *source_arr, const double wanted, CSN_ARRAY *arr) {
    CSN_BROADCAST_ITER it;
    if (ND_ITER_INIT(&it, source_arr->ndim, source_arr->shape, source_arr->size, NULL) != OK) return NOTOK;
    while (BROADCAST_ITER_NEXT(&it)) {
        double value = source_arr->data[it.linear_index];
        if (value == wanted) {
            for (size_t j = 0; j < source_arr->ndim; ++j) {
                arr->data[j] = (double) it.coords[j];
            }
            break;
        }
    }
    return OK;
}

int32_t csnarray_argwhere(CSOUND *csound, CSN_ARGWHERE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle, p->data_handle }, 3);

    CSN_ARRAY *source_arr = p->source_handle;
    CSN_ARRAY *data_arr = p->data_handle;
    res = argwhere_body(csound, NULL, source_arr, data_arr);
    if (res != OK) goto done;


    /* No match yields a {0, ndim} array rather than an invalid handle, so the
       result stays usable in a chain. Matches np.argwhere. */
    size_t count = count_elements_from_array(source_arr, data_arr, EQUAL);

    uint32_t new_shape[2] = { (uint32_t) count, source_arr->ndim };
    const CSN_ARRAY *ops[2] = { source_arr, data_arr };
    CSNARR_RES out = prepare_output(csound, &p->h, NULL, reg, p->handle, 2U, new_shape, source_arr->itype, ops, 2, CSN_ALIAS_REFUSE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    CSN_ARRAY *arr = p->handle;
    if (argwhere_assign_value(source_arr, data_arr, arr) != OK) {
        res = csn_init_error(csound, "[csnarray] Invalid selection iterator layout");
        goto done;
    }

    SET_KDATA_BEGIN(p->k_data, reg, new_shape, 2U, source_arr->itype);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_argwhere_k_init(CSOUND *csound, CSN_ARGWHERE *p) {
    return csnarray_argwhere(csound, p);
}

static inline const cs_float *compare_trig(const OPDS *h, cs_float *cmp_value, cs_float *trig) {
    return h->optext->t.inArgCount > 2 ? trig : cmp_value;
}

int32_t csnarray_argwhere_k(CSOUND *csound, CSN_ARGWHERE *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    CHECK_KTRIG(p->trig);
    int32_t res = OK;

    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle, p->data_handle }, 3);

    CSN_ARRAY *source_arr = p->source_handle;
    CSN_ARRAY *data_arr = p->data_handle;
    res = argwhere_body(csound, &p->h, source_arr, data_arr);
    if (res != OK) goto done;

    if (CAN_REUSE_ELEMENTWISE(&p->k_data, source_arr, data_arr, p->handle, 0.0, 0.0)) goto done;

    size_t count = count_elements_from_array(source_arr, data_arr, EQUAL);
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    new_shape[0] = (uint32_t) count;
    new_shape[1] = source_arr->ndim;

    const CSN_ARRAY *ops[2] = { source_arr, data_arr };
    CSNARR_RES out = prepare_output(csound, &p->h, &p->h, reg, p->handle, 2U, new_shape, source_arr->itype, ops, 2, CSN_ALIAS_REFUSE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    if (argwhere_assign_value(source_arr, data_arr, p->handle) != OK) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Invalid selection iterator layout");
        goto done;
    }

    SET_KDATA_END(p->k_data, new_shape, 2U, source_arr->itype);
    PUBLISH_ELEMENTWISE(&p->k_data, source_arr, data_arr, p->handle, 0.0, 0.0);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

static int32_t argselect_assign_value(CSN_ARRAY *source_arr, CSN_ARRAY *arr, CSN_COMPARE_MODE mode) {
    CSN_BROADCAST_ITER it;
    if (ND_ITER_INIT(&it, source_arr->ndim, source_arr->shape, source_arr->size, NULL) != OK) return NOTOK;
    size_t match = 0;
    while (BROADCAST_ITER_NEXT(&it)) {
        if (compare_match(source_arr->data[it.linear_index], 0.0, mode)) {
            for (size_t j = 0; j < source_arr->ndim; ++j) {
                arr->data[match * source_arr->ndim + j] = (double) it.coords[j];
            }
            match++;
        }
    }
    return OK;
}

/* Shared by csnargnonzero and csnargisnan: both select elements by a predicate
   that needs no comparison value, and both report the coordinates. */
static int32_t csnarray_argselect_helper(CSOUND *csound, CSN_UNARYOP *p, CSN_COMPARE_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle }, 2);

    CSN_ARRAY *source_arr = p->source_handle;

    if (source_arr->itype != CSN_REAL) {
        res = csn_init_error(csound, "[csnarray] This operation is not implemented for complex arrays");
        goto done;
    }

    size_t count = count_elements_from_value(source_arr, 0.0, mode);
    uint32_t new_shape[2] = { (uint32_t) count, source_arr->ndim };

    const CSN_ARRAY *ops[1] = { source_arr };
    CSNARR_RES out = prepare_output(csound, &p->h, NULL, reg, p->handle, 2U, new_shape, source_arr->itype, ops, 1, CSN_ALIAS_REFUSE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    if (argselect_assign_value(source_arr, p->handle, mode) != OK) {
        res = csn_init_error(csound, "[csnarray] Invalid selection iterator layout");
        goto done;
    }
    SET_KDATA_BEGIN(p->k_data, reg, new_shape, 2U, source_arr->itype);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_argselect_k_init(CSOUND *csound, CSN_UNARYOP *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle }, 2);

    CSN_ARRAY *source_arr = p->source_handle;

    if (source_arr->itype != CSN_REAL) {
        res = csn_init_error(csound, "[csnarray] This operation is not implemented for complex arrays");
        goto done;
    }
    /* Shared by csnargnonzero.k and csnargisnan.k, so it cannot know the
       predicate: the output only takes the (0, ndim) layout of an empty
       result, and the first pass computes. */
    uint32_t init_shape[CSN_MAX_DIMS] = { 0U, source_arr->ndim };

    const CSN_ARRAY *ops[1] = { source_arr };
    CSNARR_RES out = prepare_output(csound, &p->h, NULL, reg, p->handle, 2U, init_shape, source_arr->itype, ops, 1, CSN_ALIAS_REFUSE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    SET_KDATA_BEGIN(p->k_data, reg, init_shape, 2U, source_arr->itype);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

static int32_t csnarray_argselect_k_helper(CSOUND *csound, CSN_UNARYOP *p, CSN_COMPARE_MODE mode) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    CHECK_KTRIG(p->trig);
    int32_t res = OK;

    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle }, 2);

    CSN_ARRAY *source_arr = p->source_handle;
    if (source_arr->itype != CSN_REAL) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] This operation is not implemented for complex arrays");
        goto done;
    }

    if (CAN_REUSE_ELEMENTWISE(&p->k_data, source_arr, NULL, p->handle, 0.0, 0.0)) goto done;

    size_t count = count_elements_from_value(source_arr, 0.0, mode);
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    new_shape[0] = (uint32_t) count;
    new_shape[1] = source_arr->ndim;

    const CSN_ARRAY *ops[1] = { source_arr };
    CSNARR_RES out = prepare_output(csound, &p->h, &p->h, reg, p->handle, 2U, new_shape, source_arr->itype, ops, 1, CSN_ALIAS_REFUSE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    if (argselect_assign_value(source_arr, p->handle, mode) != OK) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Invalid selection iterator layout");
        goto done;
    }
    SET_KDATA_END(p->k_data, new_shape, 2U, CSN_REAL);
    PUBLISH_ELEMENTWISE(&p->k_data, source_arr, NULL, p->handle, 0.0, 0.0);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_argnonzero(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_argselect_helper(csound, p, NONZERO);
}

int32_t csnarray_argnonzero_k(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_argselect_k_helper(csound, p, NONZERO);
}

int32_t csnarray_argisnan(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_argselect_helper(csound, p, IS_NAN);
}

int32_t csnarray_argisnan_k(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_argselect_k_helper(csound, p, IS_NAN);
}

static void mask_select_assign_value(const CSN_ARRAY *source_arr, CSN_ARRAY *arr, CSN_COMPARE_MODE mode) {
    compare_fill(arr->data, source_arr->data, source_arr->size, 0.0, mode);
}

static int32_t mask_select_helper(CSOUND *csound, CSN_UNARYOP *p, CSN_COMPARE_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle }, 2);

    CSN_ARRAY *source_arr = p->source_handle;

    if (source_arr->itype != CSN_REAL) {
        res = csn_init_error(csound, "[csnarray] This operation is not implemented for complex arrays");
        goto done;
    }

    /* Each cell is classified from itself, so the mask may be its own source. */
    const CSN_ARRAY *ops[1] = { source_arr };
    CSNARR_RES out = prepare_output(csound, &p->h, NULL, reg, p->handle, source_arr->ndim, source_arr->shape, CSN_REAL, ops, 1, CSN_ALIAS_ELEMENTWISE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    CSN_ARRAY *arr = p->handle;
    memset(&p->k_data, 0, sizeof(p->k_data));
    SET_KDATA_BEGIN(p->k_data, reg, arr->shape, arr->ndim, arr->itype);

    /* At k-rate an output that is its own source is classified by every perf
       pass; doing it here as well would apply it twice in the first cycle. */
    if (csn_is_k_form(&p->h) && out.aliased) goto done;

    mask_select_assign_value(source_arr, arr, mode);
    if (out.aliased && source_arr->size > 0) update_array_data_version(&arr->version);
    PUBLISH_ELEMENTWISE(&p->k_data, source_arr, NULL, arr, 0.0, 0.0);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

static int32_t mask_select_k_helper(CSOUND *csound, CSN_UNARYOP *p, CSN_COMPARE_MODE mode) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    CHECK_KTRIG(p->trig);
    int32_t res = OK;

    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle }, 2);

    CSN_ARRAY *source_arr = p->source_handle;
    if (source_arr->itype != CSN_REAL) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] This operation is not implemented for complex arrays");
        goto done;
    }

    if (CAN_REUSE_ELEMENTWISE(&p->k_data, source_arr, NULL, p->handle, 0.0, 0.0)) goto done;

    const CSN_ARRAY *ops[1] = { source_arr };
    CSNARR_RES out = prepare_output(csound, &p->h, &p->h, reg, p->handle, source_arr->ndim, source_arr->shape, CSN_REAL, ops, 1, CSN_ALIAS_ELEMENTWISE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    mask_select_assign_value(source_arr, p->handle, mode);
    if (out.aliased && source_arr->size > 0) update_array_data_version(&p->handle->version);

    SET_KDATA_END(p->k_data, source_arr->shape, source_arr->ndim, CSN_REAL);
    PUBLISH_ELEMENTWISE(&p->k_data, source_arr, NULL, p->handle, 0.0, 0.0);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_isnan(CSOUND *csound, CSN_UNARYOP *p) {
    return mask_select_helper(csound, p, IS_NAN);
}

int32_t csnarray_isnan_k(CSOUND *csound, CSN_UNARYOP *p) {
    return mask_select_k_helper(csound, p, IS_NAN);
}

int32_t csnarray_isinf(CSOUND *csound, CSN_UNARYOP *p) {
    return mask_select_helper(csound, p, IS_INF);
}

int32_t csnarray_isinf_k(CSOUND *csound, CSN_UNARYOP *p) {
    return mask_select_k_helper(csound, p, IS_INF);
}

int32_t csnarray_isfin(CSOUND *csound, CSN_UNARYOP *p) {
    return mask_select_helper(csound, p, IS_FIN);
}

int32_t csnarray_isfin_k(CSOUND *csound, CSN_UNARYOP *p) {
    return mask_select_k_helper(csound, p, IS_FIN);
}

int compare_double_from_array_elem(const void *a, const void *b) {
    ARRAY_ELEMENT x = *(const ARRAY_ELEMENT *) a;
    ARRAY_ELEMENT y = *(const ARRAY_ELEMENT *) b;
    double x_value = x.value;
    double y_value = y.value;
    if (isnan(x_value) && isnan(y_value)) return 0;
    if (isnan(x_value)) return 1;
    if (isnan(y_value)) return -1;
    if (x_value < y_value) return -1;
    if (x_value > y_value) return 1;
    return 0;
}

static size_t count_unique(ARRAY_ELEMENT *temp, size_t size) {
    qsort(temp, size, sizeof(ARRAY_ELEMENT), compare_double_from_array_elem);
    size_t count = 0;
    for (size_t i = 0; i < size; ++i) {
        /* Same equality the sort used, so NaNs collapse to one entry instead
           of surviving as duplicates: `NaN != NaN` would always be true. */
        if (i == 0 || compare_double_from_array_elem(&temp[i], &temp[i - 1]) != 0){
            temp[count++] = temp[i];
        }
    }
    return count;
}

int32_t csnarray_argunique_deinit(CSOUND *csound, CSN_UNARYOP *p) {
    deinit_scratch(csound, &p->scratch);
    return OK;
}

int32_t csnarray_argunique(CSOUND *csound, CSN_UNARYOP *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    /* Declared before any goto: the done label frees it. */
    ARRAY_ELEMENT *temp = NULL;

    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle }, 2);

    CSN_ARRAY *source_arr = p->source_handle;
    if (source_arr->itype != CSN_REAL) {
        res = csn_init_error(csound, "[csnarray] This operation is not implemented for complex arrays");
        goto done;
    }
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    temp = csound->Calloc(csound, sizeof(ARRAY_ELEMENT) * source_arr->size);
    if (temp == NULL) {
        res = csn_init_error(csound, "[csnarray] Out of memory: allocation of %zu bytes failed", (size_t) (sizeof(ARRAY_ELEMENT) * source_arr->size));
        goto done;
    }

    for (size_t i = 0; i < source_arr->size; i++) {
        temp[i].value = source_arr->data[i];
        temp[i].linear_index = (uint32_t) i;
    }

    /* count == 0 needs no special case: the shape below becomes {0, ndim},
       a legal zero-length array, and the fill loop simply does not run. */
    size_t count = count_unique(temp, source_arr->size);
    uint32_t new_shape[2] = { (uint32_t) count, source_arr->ndim };

    const CSN_ARRAY *ops[1] = { source_arr };
    CSNARR_RES out = prepare_output(csound, &p->h, NULL, reg, p->handle, 2U, new_shape, source_arr->itype, ops, 1, CSN_ALIAS_REFUSE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    CSN_ARRAY *arr = p->handle;
    CSN_BROADCAST_ITER it;
    if (ND_ITER_INIT(&it, source_arr->ndim, source_shape, source_arr->size, NULL) != OK) {
        res = csn_init_error(csound, "[csnarray] Invalid unique-index iterator layout");
        goto done;
    }
    for (size_t i = 0; i < count; ++i) {
        ARRAY_ELEMENT *elem = &temp[i];
        if (ND_ITER_SEEK(&it, elem->linear_index) != OK) {
            res = csn_init_error(csound, "[csnarray] Unique index is out of range");
            goto done;
        }

        for (size_t j = 0; j < source_ndim; ++j) {
            arr->data[i * source_ndim + j] = (double) it.coords[j];
        }
    }

done:
    if (temp != NULL) csound->Free(csound, temp);
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_argunique_k_init(CSOUND *csound, CSN_UNARYOP *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    ARRAY_ELEMENT *temp = NULL;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle }, 2);

    CSN_ARRAY *source_arr = p->source_handle;
    if (source_arr->itype != CSN_REAL) {
        res = csn_init_error(csound, "[csnarray] This operation is not implemented for complex arrays");
        goto done;
    }

    /* The source can grow to its capacity without reallocating; reserving that
       much keeps the perf pass from growing this on a marked path. A reinit
       reuses the struct, so the previous scratch goes first. */
    deinit_scratch(csound, &p->scratch);
    size_t capacity = source_arr->capacity > 0 ? source_arr->capacity : 1;
    temp = csound->Calloc(csound, sizeof(ARRAY_ELEMENT) * capacity);
    if (temp == NULL) {
        res = csn_init_error(csound, "[csnarray] Out of memory: allocation of %zu bytes failed", (size_t) (sizeof(ARRAY_ELEMENT) * capacity));
        goto done;
    }

    /* the first pass computes; the output only takes a (0, ndim) layout */
    uint32_t init_shape[CSN_MAX_DIMS] = { 0U, source_arr->ndim };
    const CSN_ARRAY *ops[1] = { source_arr };
    CSNARR_RES out = prepare_output(csound, &p->h, NULL, reg, p->handle, 2U, init_shape, source_arr->itype, ops, 1, CSN_ALIAS_REFUSE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    p->scratch.scratch = temp;
    p->scratch.scratch_capacity = capacity;
    temp = NULL;
    SET_KDATA_BEGIN(p->k_data, reg, init_shape, 2U, source_arr->itype);

done:
    if (temp != NULL) csound->Free(csound, temp);
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_argunique_k(CSOUND *csound, CSN_UNARYOP *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    CHECK_KTRIG(p->trig);
    int32_t res = OK;

    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle }, 2);

    CSN_ARRAY *source_arr = p->source_handle;
    if (source_arr->itype != CSN_REAL) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] This operation is not implemented for complex arrays");
        goto done;
    }
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;
    size_t source_size = source_arr->size;

    if (CAN_REUSE_ELEMENTWISE(&p->k_data, source_arr, NULL, p->handle, 0.0, 0.0)) goto done;

    res = csn_scratch_reserve(csound, &p->h, p->handle->rt_locked, &p->scratch, source_size, sizeof(ARRAY_ELEMENT));
    if (res != OK) goto done;

    ARRAY_ELEMENT *scratch = (ARRAY_ELEMENT *) p->scratch.scratch;
    for (size_t i = 0; i < source_arr->size; i++) {
        scratch[i].value = source_arr->data[i];
        scratch[i].linear_index = (uint32_t) i;
    }

    size_t count = count_unique(p->scratch.scratch, source_arr->size);
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    new_shape[0] = (uint32_t) count;
    new_shape[1] = source_arr->ndim;

    const CSN_ARRAY *ops[1] = { source_arr };
    CSNARR_RES out = prepare_output(csound, &p->h, &p->h, reg, p->handle, 2U, new_shape, source_arr->itype, ops, 1, CSN_ALIAS_REFUSE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    CSN_BROADCAST_ITER it;
    if (ND_ITER_INIT(&it, source_arr->ndim, source_shape, source_arr->size, NULL) != OK) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Invalid unique-index iterator layout");
        goto done;
    }

    for (size_t i = 0; i < count; ++i) {
        ARRAY_ELEMENT *elem = &((ARRAY_ELEMENT *) p->scratch.scratch)[i];
        if (ND_ITER_SEEK(&it, elem->linear_index) != OK) {
            res = csn_locked_perf_error(csound, &p->h, "[csnarray] Unique index is out of range");
            goto done;
        }

        for (size_t j = 0; j < source_ndim; ++j) {
            p->handle->data[i * source_ndim + j] = (double) it.coords[j];
        }
    }

    SET_KDATA_END(p->k_data, new_shape, 2U, source_arr->itype);
    PUBLISH_ELEMENTWISE(&p->k_data, source_arr, NULL, p->handle, 0.0, 0.0);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_unique(CSOUND *csound, CSN_COMPARE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    ARRAY_ELEMENT *temp = NULL;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle }, 2);

    CSN_ARRAY *source_arr = p->source_handle;
    if (source_arr->itype != CSN_REAL) {
        res = csn_init_error(csound, "[csnarray] This operation is not implemented for complex arrays");
        goto done;
    }

    temp = csound->Calloc(csound, sizeof(ARRAY_ELEMENT) * source_arr->size);
    if (temp == NULL) {
        res = csn_init_error(csound, "[csnarray] Out of memory: allocation of %zu bytes failed", (size_t) (sizeof(ARRAY_ELEMENT) * source_arr->size));
        goto done;
    }

    for (size_t i = 0; i < source_arr->size; i++) {
        temp[i].value = source_arr->data[i];
        temp[i].linear_index = (uint32_t) i;
    }

    size_t count = count_unique(temp, source_arr->size);
    uint32_t new_shape[1] = { (uint32_t) count };

    const CSN_ARRAY *ops[1] = { source_arr };
    CSNARR_RES out = prepare_output(csound, &p->h, NULL, reg, p->handle, 1U, new_shape, source_arr->itype, ops, 1, CSN_ALIAS_REFUSE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    CSN_ARRAY *arr = p->handle;
    for (size_t i = 0; i < count; ++i) {
        arr->data[i] = temp[i].value;
    }

done:
    if (temp != NULL) csound->Free(csound, temp);
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_unique_k_init(CSOUND *csound, CSN_COMPARE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    ARRAY_ELEMENT *temp = NULL;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle }, 2);

    CSN_ARRAY *source_arr = p->source_handle;
    if (source_arr->itype != CSN_REAL) {
        res = csn_init_error(csound, "[csnarray] This operation is not implemented for complex arrays");
        goto done;
    }

    /* The source can grow to its capacity without reallocating; reserving that
       much keeps the perf pass from growing this on a marked path. A reinit
       reuses the struct, so the previous scratch goes first. */
    deinit_scratch(csound, &p->scratch);
    size_t capacity = source_arr->capacity > 0 ? source_arr->capacity : 1;
    temp = csound->Calloc(csound, sizeof(ARRAY_ELEMENT) * capacity);
    if (temp == NULL) {
        res = csn_init_error(csound, "[csnarray] Out of memory: allocation of %zu bytes failed", (size_t) (sizeof(ARRAY_ELEMENT) * capacity));
        goto done;
    }

    const CSN_ARRAY *ops[1] = { source_arr };
    CSNARR_RES out = prepare_output(csound, &p->h, NULL, reg, p->handle, source_arr->ndim, source_arr->shape, source_arr->itype, ops, 1, CSN_ALIAS_REFUSE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    if (source_arr->size > 0) {
        memcpy(p->handle->data, source_arr->data, sizeof(double) * source_arr->size);
        p->handle->size = source_arr->size;
    }

    p->scratch.scratch = temp;
    p->scratch.scratch_capacity = capacity;
    temp = NULL;
    SET_KDATA_BEGIN(p->k_data, reg, source_arr->shape, source_arr->ndim, source_arr->itype);

done:
    if (temp != NULL) csound->Free(csound, temp);
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_unique_k(CSOUND *csound, CSN_COMPARE *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    CHECK_KTRIG(compare_trig(&p->h, p->cmp_value, p->trig));
    int32_t res = OK;

    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle }, 2);

    CSN_ARRAY *source_arr = p->source_handle;
    if (source_arr->itype != CSN_REAL) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] This operation is not implemented for complex arrays");
        goto done;
    }

    if (CAN_REUSE_ELEMENTWISE(&p->k_data, source_arr, NULL, p->handle, 0.0, 0.0)) goto done;

    size_t source_size = source_arr->size;
    res = csn_scratch_reserve(csound, &p->h, p->handle->rt_locked, &p->scratch, source_size, sizeof(ARRAY_ELEMENT));
    if (res != OK) goto done;

    ARRAY_ELEMENT *scratch = (ARRAY_ELEMENT *) p->scratch.scratch;
    for (size_t i = 0; i < source_arr->size; i++) {
        scratch[i].value = source_arr->data[i];
        scratch[i].linear_index = (uint32_t) i;
    }

    size_t count = count_unique(p->scratch.scratch, source_arr->size);
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    new_shape[0] = (uint32_t) count;

    const CSN_ARRAY *ops[1] = { source_arr };
    CSNARR_RES out = prepare_output(csound, &p->h, &p->h, reg, p->handle, 1U, new_shape, source_arr->itype, ops, 1, CSN_ALIAS_REFUSE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    for (size_t i = 0; i < count; ++i) {
        p->handle->data[i] = scratch[i].value;
    }

    SET_KDATA_END(p->k_data, new_shape, 1U, source_arr->itype);
    PUBLISH_ELEMENTWISE(&p->k_data, source_arr, NULL, p->handle, 0.0, 0.0);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

static int32_t csnarray_compare_helper(CSOUND *csound, CSN_COMPARE *p, CSN_COMPARE_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle }, 2);

    CSN_ARRAY *source_arr = p->source_handle;
    if (source_arr->itype != CSN_REAL) {
        res = csn_init_error(csound, "[csnarray] This operation is not implemented for complex arrays");
        goto done;
    }
    double cmp_value = (double) *p->cmp_value;

    // size_t count = count_elements_from_value(source_arr, cmp_value, mode);
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    memcpy(new_shape, source_arr->shape, sizeof(uint32_t) * CSN_MAX_DIMS);

    /* Each cell is compared with itself only, so the result may be its own
       source; both branches are written, as an aliased output is not zeroed. */
    const CSN_ARRAY *ops[1] = { source_arr };
    CSNARR_RES out = prepare_output(csound, &p->h, NULL, reg, p->handle, source_arr->ndim, new_shape, source_arr->itype, ops, 1, CSN_ALIAS_ELEMENTWISE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    compare_fill(p->handle->data, source_arr->data, source_arr->size, cmp_value, mode);
    if (out.aliased && source_arr->size > 0) update_array_data_version(&p->handle->version);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

static int32_t compare_k_init_helper(CSOUND *csound, CSN_COMPARE *p, CSN_COMPARE_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle }, 2);

    CSN_ARRAY *source_arr = p->source_handle;
    if (source_arr->itype != CSN_REAL) {
        res = csn_init_error(csound, "[csnarray] This operation is not implemented for complex arrays");
        goto done;
    }

    const CSN_ARRAY *ops[1] = { source_arr };
    CSNARR_RES out = prepare_output(csound, &p->h, NULL, reg, p->handle, source_arr->ndim, source_arr->shape, source_arr->itype, ops, 1, CSN_ALIAS_ELEMENTWISE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    memset(&p->k_data, 0, sizeof(p->k_data));
    SET_KDATA_BEGIN(p->k_data, reg, source_arr->shape, source_arr->ndim, source_arr->itype);

    /* Compared here only against a value known at init; an output that is its
       own source is compared by every perf pass. Otherwise the layout is the
       source's and the values are left to the first pass. */
    if (out.aliased || !csn_known_at_init(csound, p->cmp_value)) goto done;
    double cmp_value = (double) *p->cmp_value;
    compare_fill(p->handle->data, source_arr->data, source_arr->size, cmp_value, mode);
    PUBLISH_ELEMENTWISE(&p->k_data, source_arr, NULL, p->handle, cmp_value, 0.0);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_greater_than_k_init(CSOUND *csound, CSN_COMPARE *p) {
    return compare_k_init_helper(csound, p, GREATER_THAN);
}

int32_t csnarray_less_than_k_init(CSOUND *csound, CSN_COMPARE *p) {
    return compare_k_init_helper(csound, p, LESS_THAN);
}

int32_t csnarray_not_equal_k_init(CSOUND *csound, CSN_COMPARE *p) {
    return compare_k_init_helper(csound, p, NOT_EQUAL);
}

int32_t csnarray_greater_equal_k_init(CSOUND *csound, CSN_COMPARE *p) {
    return compare_k_init_helper(csound, p, GREATER_EQUAL);
}

int32_t csnarray_less_equal_k_init(CSOUND *csound, CSN_COMPARE *p) {
    return compare_k_init_helper(csound, p, LESS_EQUAL);
}

int32_t csnarray_equal_k_init(CSOUND *csound, CSN_COMPARE *p) {
    return compare_k_init_helper(csound, p, EQUAL);
}

static int32_t csnarray_compare_k_helper(CSOUND *csound, CSN_COMPARE *p, CSN_COMPARE_MODE mode) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    CHECK_KTRIG(p->trig);
    int32_t res = OK;

    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle }, 2);

    CSN_ARRAY *source_arr = p->source_handle;
    if (source_arr->itype != CSN_REAL) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] This operation is not implemented for complex arrays");
        goto done;
    }
    double cmp_value = (double) *p->cmp_value;

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    memcpy(new_shape, source_arr->shape, sizeof(uint32_t) * CSN_MAX_DIMS);

    if (CAN_REUSE_ELEMENTWISE(&p->k_data, source_arr, NULL, p->handle, cmp_value, 0.0)) goto done;

    const CSN_ARRAY *ops[1] = { source_arr };
    CSNARR_RES out = prepare_output(csound, &p->h, &p->h, reg, p->handle, source_arr->ndim, new_shape, source_arr->itype, ops, 1, CSN_ALIAS_ELEMENTWISE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    // both branches: an aliased output is not zeroed
    compare_fill(p->handle->data, source_arr->data, source_arr->size, cmp_value, mode);
    if (out.aliased && source_arr->size > 0) update_array_data_version(&p->handle->version);

    SET_KDATA_END(p->k_data, new_shape, source_arr->ndim, source_arr->itype);
    PUBLISH_ELEMENTWISE(&p->k_data, source_arr, NULL, p->handle, cmp_value, 0.0);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_greater_than(CSOUND *csound, CSN_COMPARE *p) {
    return csnarray_compare_helper(csound, p, GREATER_THAN);
}

int32_t csnarray_greater_than_k(CSOUND *csound, CSN_COMPARE *p) {
    return csnarray_compare_k_helper(csound, p, GREATER_THAN);
}

int32_t csnarray_less_than(CSOUND *csound, CSN_COMPARE *p) {
    return csnarray_compare_helper(csound, p, LESS_THAN);
}

int32_t csnarray_less_than_k(CSOUND *csound, CSN_COMPARE *p) {
    return csnarray_compare_k_helper(csound, p, LESS_THAN);
}

int32_t csnarray_not_equal(CSOUND *csound, CSN_COMPARE *p) {
    return csnarray_compare_helper(csound, p, NOT_EQUAL);
}

int32_t csnarray_not_equal_k(CSOUND *csound, CSN_COMPARE *p) {
    return csnarray_compare_k_helper(csound, p, NOT_EQUAL);
}

int32_t csnarray_greater_equal(CSOUND *csound, CSN_COMPARE *p) {
    return csnarray_compare_helper(csound, p, GREATER_EQUAL);
}

int32_t csnarray_greater_equal_k(CSOUND *csound, CSN_COMPARE *p) {
    return csnarray_compare_k_helper(csound, p, GREATER_EQUAL);
}

int32_t csnarray_less_equal(CSOUND *csound, CSN_COMPARE *p) {
    return csnarray_compare_helper(csound, p, LESS_EQUAL);
}

int32_t csnarray_less_equal_k(CSOUND *csound, CSN_COMPARE *p) {
    return csnarray_compare_k_helper(csound, p, LESS_EQUAL);
}

int32_t csnarray_equal(CSOUND *csound, CSN_COMPARE *p) {
    return csnarray_compare_helper(csound, p, EQUAL);
}

int32_t csnarray_equal_k(CSOUND *csound, CSN_COMPARE *p) {
    return csnarray_compare_k_helper(csound, p, EQUAL);
}

static int32_t csnarray_compare_count_helper(CSOUND *csound, CSN_COUNT *p, CSN_COMPARE_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);

    CSN_ARRAY *source_arr = p->source_handle;
    if (source_arr->itype != CSN_REAL) {
        res = csn_init_error(csound, "[csnarray] This operation is not implemented for complex arrays");
        goto done;
    }

    /* NONZERO and IS_NAN take no comparison value, so their opcodes have no
       such argument to read. */
    size_t count = (mode == NONZERO || mode == IS_NAN)
        ? count_elements_from_value(source_arr, 0.0, mode)
        : count_elements_from_value(source_arr, (double) *p->arg_a, mode);

    *p->value = (cs_float) count;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_compare_count_k_init(CSOUND *csound, CSN_COUNT *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    stamp_globals(csound, &p->h);
    p->k_data.registry = reg;
    p->is_published = false;
    return OK;
}

static int32_t csnarray_compare_count_k_helper(CSOUND *csound, CSN_COUNT *p, CSN_COMPARE_MODE mode) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    const cs_float *trig = mode == EQUAL ? p->arg_b : p->arg_a;
    CHECK_KTRIG(trig);

    int32_t res = OK;
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);

    CSN_ARRAY *source_arr = p->source_handle;
    if (source_arr->itype != CSN_REAL) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] This operation is not implemented for complex arrays");
        goto done;
    }

    double cmp_key = mode == EQUAL ? (double) *p->arg_a : 0.0;
    if (p->is_published && CAN_REUSE_ELEMENTWISE(&p->k_data, source_arr, NULL, NULL, cmp_key, p->k_data.prev_scalar_param_b)) {
        *p->value = (cs_float) p->k_data.prev_scalar_param_b;
        goto done;
    }

    size_t count = (mode == NONZERO || mode == IS_NAN)
        ? count_elements_from_value(source_arr, 0.0, mode)
        : count_elements_from_value(source_arr, cmp_key, mode);

    *p->value = (cs_float) count;
    PUBLISH_ELEMENTWISE(&p->k_data, source_arr, NULL, NULL, cmp_key, (double) count);
    p->is_published = true;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_count_equal(CSOUND *csound, CSN_COUNT *p) {
    return csnarray_compare_count_helper(csound, p, EQUAL);
}

int32_t csnarray_count_equal_k(CSOUND *csound, CSN_COUNT *p) {
    return csnarray_compare_count_k_helper(csound, p, EQUAL);
}

int32_t csnarray_count_nonzero(CSOUND *csound, CSN_COUNT *p) {
    return csnarray_compare_count_helper(csound, p, NONZERO);
}

int32_t csnarray_count_nonzero_k(CSOUND *csound, CSN_COUNT *p) {
    return csnarray_compare_count_k_helper(csound, p, NONZERO);
}

int32_t csnarray_count_nan(CSOUND *csound, CSN_COUNT *p) {
    return csnarray_compare_count_helper(csound, p, IS_NAN);
}

int32_t csnarray_count_nan_k(CSOUND *csound, CSN_COUNT *p) {
    return csnarray_compare_count_k_helper(csound, p, IS_NAN);
}

static int32_t where_body(CSOUND *csound, OPDS *perf_h, CSN_ARRAY *source_arr, CSN_ARRAY *true_arr, CSN_ARRAY *false_arr, double *false_scalar) {
    bool is_same_first_shape = memcmp(source_arr->shape, true_arr->shape, sizeof(true_arr->shape)) == 0;
    if (!is_same_first_shape || source_arr->ndim != true_arr->ndim) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] source array and true array replacement must have same shape and dim");
    }

    if (false_arr != NULL) {
        bool is_same_shape = memcmp(true_arr->shape, false_arr->shape, sizeof(true_arr->shape)) == 0;
        if (!is_same_shape || true_arr->ndim != false_arr->ndim) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] true array and false array replacement must have same shape and dim");
        }
    }

    if (source_arr->itype != CSN_REAL || true_arr->itype != CSN_REAL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] This operation is not implemented for complex arrays");
    }

    if (false_arr != NULL && false_arr->itype != CSN_REAL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] This operation is not implemented for complex arrays");
    }

    if (false_scalar != NULL && !IS_VALID_VALUE(*false_scalar)) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Invalid false scalar to put");
    }

    return OK;
}

static int32_t csnarray_where_helper(CSOUND *csound, OPDS *h, CSN_ARRAY *source_arr, CSN_ARRAY *true_arr, CSN_ARRAY *false_arr, CSN_ARRAY *out_arr, double *false_scalar) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    stamp_globals(csound, h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { out_arr, source_arr, true_arr, false_arr }, 4);

    res = where_body(csound, NULL, source_arr, true_arr, false_arr, false_scalar);
    if (res != OK) goto done;

    uint32_t new_dim = source_arr->ndim;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    memcpy(new_shape, source_arr->shape, sizeof(uint32_t) * CSN_MAX_DIMS);

    size_t n_ops = false_arr == NULL ? 2 : 3;
    const CSN_ARRAY *ops[3] = { source_arr, true_arr, false_arr };
    CSNARR_RES out = prepare_output(csound, h, NULL, reg, out_arr, new_dim, new_shape, CSN_REAL, ops, n_ops, CSN_ALIAS_REFUSE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    size_t size = out_arr->size;
    for (size_t i = 0; i < size; i++) {
        double mask_value = source_arr->data[i];
        if (false_scalar != NULL) {
            out_arr->data[i] = mask_value == 0.0 ? *false_scalar : true_arr->data[i];
        } else {
            out_arr->data[i] = mask_value == 0.0 ? false_arr->data[i] : true_arr->data[i];
        }
    }

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_where_hh(CSOUND *csound, CSN_WHERE_HH *p) {
    return csnarray_where_helper(csound, &p->h, p->source_handle, p->source_handle_true, p->source_handle_false, p->handle, NULL);
}

int32_t csnarray_where_hs(CSOUND *csound, CSN_WHERE_HS *p) {
    double scalar_false = (double) *p->source_scalar_false;
    return csnarray_where_helper(csound, &p->h, p->source_handle, p->source_handle_true, NULL, p->handle, &scalar_false);
}

static int32_t csnarray_where_k_init_helper(CSOUND *csound, OPDS *h, CSN_ARRAY *source_arr, CSN_ARRAY *true_arr, CSN_ARRAY *false_arr, CSN_ARRAY *out_arr, double *false_scalar, K_DATA *k_data, bool *is_published) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    stamp_globals(csound, h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { source_arr, true_arr, false_arr, out_arr }, 4);

    res = where_body(csound, NULL, source_arr, true_arr, false_arr, false_scalar);
    if (res != OK) goto done;

    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    size_t n_ops = false_arr == NULL ? 2 : 3;
    const CSN_ARRAY *ops[3] = { source_arr, true_arr, false_arr };
    CSNARR_RES out = prepare_output(csound, h, NULL, reg, out_arr, source_ndim, source_shape, CSN_REAL, ops, n_ops, CSN_ALIAS_REFUSE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    reset_empty_csnarray(out_arr, source_ndim, source_shape, CSN_REAL);

    SET_KDATA_BEGIN(*k_data, reg, source_shape, source_ndim, CSN_REAL);
    set_array_version(&k_data->prev_output_version, &out_arr->version);
    *is_published = false;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

static int32_t csnarray_where_k_helper(CSOUND *csound, OPDS *h, CSN_ARRAY *source_arr, CSN_ARRAY *true_arr, CSN_ARRAY *false_arr, CSN_ARRAY *out_arr, double *false_scalar, K_DATA *k_data, bool *is_published, CSN_THREE_VERSION_K_STATE *versions, const cs_float *trig) {
    CSN_REGISTRY *reg = k_data->registry;
    CHECK_REGISTRY(csound, h, reg);

    CHECK_KTRIG(trig);
    int32_t res = OK;

    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { source_arr, true_arr, false_arr, out_arr }, 4);

    res = where_body(csound, h, source_arr, true_arr, false_arr, false_scalar);
    if (res != OK) goto done;

    if (*is_published) {
        bool is_same_source_version = is_same_array_version(&versions->prev_a_version, &source_arr->version);
        bool is_same_true_version = is_same_array_version(&versions->prev_b_version, &true_arr->version);
        bool is_same_false = false;
        if (false_arr == NULL && false_scalar != NULL) {
            is_same_false = k_data->prev_scalar_param == *false_scalar;
        } else {
            is_same_false = is_same_array_version(&versions->prev_c_version, &false_arr->version);
        }
        bool is_same_result = is_same_array_version(&k_data->prev_output_version, &out_arr->version);
        if (is_same_source_version && is_same_true_version && is_same_false && is_same_result) goto done;
    }

    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    size_t n_ops = false_arr == NULL ? 2 : 3;
    const CSN_ARRAY *ops[3] = { source_arr, true_arr, false_arr };
    CSNARR_RES out = prepare_output(csound, h, h, reg, out_arr, source_ndim, source_shape, CSN_REAL, ops, n_ops, CSN_ALIAS_REFUSE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    size_t size = out_arr->size;
    for (size_t i = 0; i < size; i++) {
        double mask_value = source_arr->data[i];
        if (false_scalar != NULL) {
            out_arr->data[i] = mask_value == 0.0 ? *false_scalar : true_arr->data[i];
        } else {
            out_arr->data[i] = mask_value == 0.0 ? false_arr->data[i] : true_arr->data[i];
        }
    }

    SET_KDATA_END(*k_data, out_arr->shape, out_arr->ndim, out_arr->itype);
    set_array_version(&k_data->prev_output_version, &out_arr->version);
    set_array_version(&versions->prev_a_version, &source_arr->version);
    set_array_version(&versions->prev_b_version, &true_arr->version);
    if (false_arr != NULL) {
        set_array_version(&versions->prev_c_version, &false_arr->version);
    } else {
        k_data->prev_scalar_param = *false_scalar;
    }
    *is_published = true;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_where_hh_k_init(CSOUND *csound, CSN_WHERE_HH *p) {
    return csnarray_where_k_init_helper(csound, &p->h, p->source_handle, p->source_handle_true, p->source_handle_false, p->handle, NULL, &p->k_data, &p->is_published);
}

int32_t csnarray_where_hs_k_init(CSOUND *csound, CSN_WHERE_HS *p) {
    double scalar_false = (double) *p->source_scalar_false;
    return csnarray_where_k_init_helper(csound, &p->h, p->source_handle, p->source_handle_true, NULL, p->handle, &scalar_false, &p->k_data, &p->is_published);
}

int32_t csnarray_where_hh_k(CSOUND *csound, CSN_WHERE_HH *p) {
    return csnarray_where_k_helper(csound, &p->h, p->source_handle, p->source_handle_true, p->source_handle_false, p->handle, NULL, &p->k_data, &p->is_published, &p->versions, p->trig);
}

int32_t csnarray_where_hs_k(CSOUND *csound, CSN_WHERE_HS *p) {
    double scalar_false = (double) *p->source_scalar_false;
    return csnarray_where_k_helper(csound, &p->h, p->source_handle, p->source_handle_true, NULL, p->handle, &scalar_false, &p->k_data, &p->is_published, &p->versions, p->trig);
}

static int32_t csnarray_where_in_helper(CSOUND *csound, OPDS *h, CSN_ARRAY *source_arr, CSN_ARRAY *true_arr, CSN_ARRAY *false_arr, double *false_scalar) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    stamp_globals(csound, h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { source_arr, true_arr, false_arr }, 3);

    res = where_body(csound, NULL, source_arr, true_arr, false_arr, false_scalar);
    if (res != OK) goto done;

    size_t size = source_arr->size;
    for (size_t i = 0; i < size; i++) {
        double mask_value = source_arr->data[i];
        if (false_scalar != NULL) {
            source_arr->data[i] = mask_value == 0.0 ? *false_scalar : true_arr->data[i];
        } else {
            source_arr->data[i] = mask_value == 0.0 ? false_arr->data[i] : true_arr->data[i];
        }
    }
    update_array_data_version(&source_arr->version);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_where_in_hh(CSOUND *csound, CSN_WHERE_HH_IN *p) {
    return csnarray_where_in_helper(csound, &p->h, p->source_handle, p->source_handle_true, p->source_handle_false, NULL);
}

int32_t csnarray_where_in_hs(CSOUND *csound, CSN_WHERE_HS_IN *p) {
    double scalar_false = (double) *p->source_scalar_false;
    return csnarray_where_in_helper(csound, &p->h, p->source_handle, p->source_handle_true, NULL, &scalar_false);
}

static int32_t csnarray_where_k_in_init_helper(CSOUND *csound, OPDS *h, CSN_ARRAY *source_arr, CSN_ARRAY *true_arr, CSN_ARRAY *false_arr, double *false_scalar, CSN_REGISTRY **registry, bool *is_published) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    csn_mark_krate(source_arr);

    int32_t res = OK;
    stamp_globals(csound, h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { source_arr, true_arr, false_arr }, 3);

    res = where_body(csound, NULL, source_arr, true_arr, false_arr, false_scalar);
    if (res != OK) goto done;

    *registry = reg;
    *is_published = false;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

static int32_t csnarray_where_k_in_helper(CSOUND *csound, OPDS *h, CSN_ARRAY *source_arr, CSN_ARRAY *true_arr, CSN_ARRAY *false_arr, double *false_scalar, CSN_REGISTRY *registry, bool *is_published, CSN_THREE_VERSION_K_STATE *versions, const cs_float *trig, double *prev_scalar_false) {
    CSN_REGISTRY *reg = registry;
    CHECK_REGISTRY(csound, h, reg);

    CHECK_KTRIG(trig);
    int32_t res = OK;

    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { source_arr, true_arr, false_arr }, 3);

    res = where_body(csound, h, source_arr, true_arr, false_arr, false_scalar);
    if (res != OK) goto done;

    if (*is_published) {
        bool is_same_source_version = is_same_array_version(&versions->prev_a_version, &source_arr->version);
        bool is_same_true_version = is_same_array_version(&versions->prev_b_version, &true_arr->version);
        bool is_same_false = false;
        if (false_arr == NULL && false_scalar != NULL) {
            is_same_false = *prev_scalar_false == *false_scalar;
        } else {
            is_same_false = is_same_array_version(&versions->prev_c_version, &false_arr->version);
        }
        if (is_same_source_version && is_same_true_version && is_same_false) goto done;
    }

    size_t size = source_arr->size;
    for (size_t i = 0; i < size; i++) {
        double mask_value = source_arr->data[i];
        if (false_scalar != NULL) {
            source_arr->data[i] = mask_value == 0.0 ? *false_scalar : true_arr->data[i];
        } else {
            source_arr->data[i] = mask_value == 0.0 ? false_arr->data[i] : true_arr->data[i];
        }
    }
    update_array_data_version(&source_arr->version);

    set_array_version(&versions->prev_a_version, &source_arr->version);
    set_array_version(&versions->prev_b_version, &true_arr->version);
    if (false_arr != NULL) {
        set_array_version(&versions->prev_c_version, &false_arr->version);
    } else {
        *prev_scalar_false = *false_scalar;
    }
    *is_published = true;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_where_in_hh_k_init(CSOUND *csound, CSN_WHERE_HH_IN *p) {
    return csnarray_where_k_in_init_helper(csound, &p->h, p->source_handle, p->source_handle_true, p->source_handle_false, NULL, &p->registry, &p->is_published);
}

int32_t csnarray_where_in_hs_k_init(CSOUND *csound, CSN_WHERE_HS_IN *p) {
    double scalar_false = (double) *p->source_scalar_false;
    return csnarray_where_k_in_init_helper(csound, &p->h, p->source_handle, p->source_handle_true, NULL, &scalar_false, &p->registry, &p->is_published);
}

int32_t csnarray_where_in_hh_k(CSOUND *csound, CSN_WHERE_HH_IN *p) {
    return csnarray_where_k_in_helper(csound, &p->h, p->source_handle, p->source_handle_true, p->source_handle_false, NULL, p->registry, &p->is_published, &p->versions, p->trig, &p->prev_scalar_false);
}

int32_t csnarray_where_in_hs_k(CSOUND *csound, CSN_WHERE_HS_IN *p) {
    double scalar_false = (double) *p->source_scalar_false;
    return csnarray_where_k_in_helper(csound, &p->h, p->source_handle, p->source_handle_true, NULL, &scalar_false, p->registry, &p->is_published, &p->versions, p->trig, &p->prev_scalar_false);
}

static int32_t compress_body(CSOUND *csound, OPDS *perf_h, CSN_ARRAY *source_arr, CSN_ARRAY *mask_arr, const cs_float *axis_in, int32_t *axis_out) {
    if (mask_arr->itype != CSN_REAL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Mask must be real-array");
    }

    uint32_t source_ndim = source_arr->ndim;
    CSN_AXIS_SPEC axis_spec = csn_normalize_axis(axis_in, source_ndim, CSN_AXIS_DEFAULT_FLATTEN);
    if (axis_spec.kind == CSN_AXIS_INVALID) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Invalid axis value");
    }
    int32_t axis = axis_spec.kind == CSN_AXIS_FLATTEN ? -1 : (int32_t) axis_spec.index;

    uint32_t axis_length = axis == -1 ? (uint32_t) source_arr->size : source_arr->shape[axis];
    if (mask_arr->ndim != 1U || mask_arr->shape[0] > axis_length) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Mask must be 1-D and no longer than the selected axis");
    }
    *axis_out = axis;

    return OK;
}

static int32_t compress_assign_value(CSOUND *csound, CSN_ARRAY *arr, CSN_ARRAY *source_arr, size_t count_true, size_t dst_size, uint32_t *indexes, int32_t axis_out, uint32_t ndim, ITEM_TYPE itype) {
    if (axis_out == -1) {
        for (size_t i = 0; i < count_true; i++) {
            size_t index = indexes[i];
            switch (itype) {
                case CSN_REAL:
                case CSN_COMPLEX:
                    if (itype == CSN_REAL) {
                        arr->data[i] = source_arr->data[index];
                    } else {
                        arr->data[i * 2] = source_arr->data[index * 2];
                        arr->data[i * 2 + 1] = source_arr->data[index * 2 + 1];
                    }
                    break;
                case CSN_STRING:
                    if (arr->sdata[i] != NULL) csound->Free(csound, arr->sdata[i]);
                    arr->sdata[i] = csound->Strdup(csound, source_arr->sdata[index]);
                    break;
            }
        }
    } else {
        CSN_BROADCAST_ITER it;
        if (ND_ITER_INIT(&it, ndim, arr->shape, dst_size, NULL) != OK) return NOTOK;
        while (BROADCAST_ITER_NEXT(&it)) {
            size_t i = it.linear_index;
            size_t src_off = 0;
            for (uint32_t d = 0; d < ndim; ++d) {
                uint32_t coord = d == (uint32_t) axis_out ? indexes[it.coords[d]] : it.coords[d];
                src_off += (size_t) coord * source_arr->strides[d];
            }
            switch (itype) {
                case CSN_REAL:
                case CSN_COMPLEX:
                    if (itype == CSN_REAL) {
                        arr->data[i] = source_arr->data[src_off];
                    } else {
                        arr->data[i * 2] = source_arr->data[src_off * 2];
                        arr->data[i * 2 + 1] = source_arr->data[src_off * 2 + 1];
                    }
                    break;
                case CSN_STRING:
                    if (arr->sdata[i] != NULL) csound->Free(csound, arr->sdata[i]);
                    arr->sdata[i] = csound->Strdup(csound, source_arr->sdata[src_off]);
                    break;
            }
        }
    }

    return OK;
}

int32_t csnarray_compress(CSOUND *csound, CSN_WHERE_HS *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    const cs_float *axis_in = p->INOCOUNT > 2 ? p->source_scalar_false : NULL;

    int32_t res = OK;
    uint32_t *indexes = NULL;

    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle, p->source_handle_true }, 3);

    CSN_ARRAY *source_arr = p->source_handle;
    CSN_ARRAY *mask_arr = p->source_handle_true;
    int32_t axis_out = -1;
    res = compress_body(csound, NULL, source_arr, mask_arr, axis_in, &axis_out);
    if (res != OK) goto done;

    indexes = csound->Calloc(csound, sizeof(uint32_t) * mask_arr->size);
    if (indexes == NULL) {
        res = csn_init_error(csound, "[csnarray] Internal error: memory allocation failed");
        goto done;
    }
    uint32_t count_true = 0;
    for (uint32_t i = 0; i < mask_arr->size; i++) {
        if (mask_arr->data[i] != 0.0) indexes[count_true++] = i;
    }

    ITEM_TYPE itype = source_arr->itype;
    uint32_t new_dim = source_arr->ndim;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};

    if (axis_out == -1) {
        new_dim = 1U;
        new_shape[0] = count_true;
    } else {
        memcpy(new_shape, source_arr->shape, sizeof(uint32_t) * CSN_MAX_DIMS);
        new_shape[axis_out] = count_true;
    }

    const CSN_ARRAY *ops[2] = { source_arr, mask_arr };
    CSNARR_RES out = prepare_output(csound, &p->h, NULL, reg, p->handle, new_dim, new_shape, itype, ops, 2, CSN_ALIAS_REFUSE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    size_t size = p->handle->size;
    if (compress_assign_value(csound, p->handle, source_arr, count_true, size, indexes, axis_out, new_dim, itype) != OK) {
        res = csn_init_error(csound, "[csnarray] Invalid compress iterator layout");
        goto done;
    }

done:
    if (indexes != NULL) csound->Free(csound, indexes);
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_compress_k_init(CSOUND *csound, CSN_WHERE_HS *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    uint32_t *indexes = NULL;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle, p->source_handle_true }, 3);

    CSN_ARRAY *source_arr = p->source_handle;
    CSN_ARRAY *mask_arr = p->source_handle_true;
    int32_t axis_out = -1;
    const cs_float *axis_in = p->INOCOUNT > 3 ? p->trig : NULL;
    res = compress_body(csound, NULL, source_arr, mask_arr, axis_in, &axis_out);
    if (res != OK) goto done;

    /* The mask may itself be a k result its producer could not compute at
       init, so its count is not read here: the output is laid out, reserved
       but empty, for the most a compress can keep, every element of the run,
       which is also all a marked output may grow to at perf time. */
    ITEM_TYPE itype = source_arr->itype;
    uint32_t new_dim = source_arr->ndim;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    if (axis_out == -1) {
        new_dim = 1U;
        new_shape[0] = (uint32_t) source_arr->size;
    } else {
        memcpy(new_shape, source_arr->shape, sizeof(uint32_t) * CSN_MAX_DIMS);
    }

    deinit_scratch(csound, &p->scratch);
    size_t scratch_capacity = source_arr->capacity;
    if (scratch_capacity == 0) scratch_capacity = 1;
    indexes = csound->Calloc(csound, sizeof(uint32_t) * scratch_capacity);
    if (indexes == NULL) {
        res = csn_init_error(csound, "[csnarray] Internal error: memory allocation failed");
        goto done;
    }

    const CSN_ARRAY *ops[2] = { source_arr, mask_arr };
    CSNARR_RES out = prepare_output(csound, &p->h, NULL, reg, p->handle, new_dim, new_shape, itype, ops, 2, CSN_ALIAS_REFUSE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    reset_empty_csnarray(p->handle, new_dim, new_shape, itype);

    p->scratch.scratch = indexes;
    p->scratch.scratch_capacity = scratch_capacity;
    indexes = NULL;
    SET_KDATA_BEGIN(p->k_data, reg, new_shape, new_dim, itype);
    p->is_published = false;

done:
    if (indexes != NULL) csound->Free(csound, indexes);
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_compress_k(CSOUND *csound, CSN_WHERE_HS *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    CHECK_KTRIG(p->source_scalar_false);
    if (p->source_handle->itype == CSN_STRING) {
        int32_t res_rt = check_strings_not_rt(csound, &p->h, p->handle);
        if (res_rt != OK) return res_rt;
    }

    const cs_float *axis_in = p->INOCOUNT > 3 ? p->trig : NULL;
    int32_t res = OK;

    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle, p->source_handle_true }, 3);

    CSN_ARRAY *source_arr = p->source_handle;
    CSN_ARRAY *mask_arr = p->source_handle_true;
    int32_t axis_out = -1;
    res = compress_body(csound, &p->h, source_arr, mask_arr, axis_in, &axis_out);
    if (res != OK) goto done;

    if (p->is_published) {
        bool is_same_source = is_same_array_version(&p->versions.prev_a_version, &source_arr->version);
        bool is_same_mask = is_same_array_version(&p->versions.prev_b_version, &mask_arr->version);
        bool is_same_axis = p->k_data.prev_axis_i == axis_out;
        bool is_same_result = is_same_array_version(&p->k_data.prev_output_version, &p->handle->version);
        if (is_same_source && is_same_mask && is_same_axis && is_same_result) goto done;
    }

    res = csn_scratch_reserve(csound, &p->h, p->handle->rt_locked, &p->scratch, mask_arr->size, sizeof(uint32_t));
    if (res != OK) goto done;

    uint32_t *indexes_temp = (uint32_t *) p->scratch.scratch;
    uint32_t count_true = 0;
    for (uint32_t i = 0; i < mask_arr->size; i++) {
        if (mask_arr->data[i] != 0.0) indexes_temp[count_true++] = i;
    }

    ITEM_TYPE itype = source_arr->itype;
    uint32_t new_dim = source_arr->ndim;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};

    if (axis_out == -1) {
        new_dim = 1U;
        new_shape[0] = count_true;
    } else {
        memcpy(new_shape, source_arr->shape, sizeof(uint32_t) * CSN_MAX_DIMS);
        new_shape[axis_out] = count_true;
    }

    const CSN_ARRAY *ops[2] = { source_arr, mask_arr };
    CSNARR_RES out = prepare_output(csound, &p->h, &p->h, reg, p->handle, new_dim, new_shape, itype, ops, 2, CSN_ALIAS_REFUSE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    if (compress_assign_value(csound, p->handle, source_arr, count_true, p->handle->size, indexes_temp, axis_out, new_dim, itype) != OK) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Invalid compress iterator layout");
        goto done;
    }

    SET_KDATA_END(p->k_data, new_shape, new_dim, itype);
    p->k_data.prev_axis_i = axis_out;
    set_array_version(&p->k_data.prev_output_version, &p->handle->version);
    set_array_version(&p->versions.prev_a_version, &source_arr->version);
    set_array_version(&p->versions.prev_b_version, &mask_arr->version);
    p->is_published = true;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

static int32_t select_body(CSOUND *csound, OPDS *perf_h, CSN_ARRAY *source_arr, CSN_ARRAY *mask_arr) {
    if (mask_arr->ndim != source_arr->ndim || memcmp(mask_arr->shape, source_arr->shape, sizeof(uint32_t) * CSN_MAX_DIMS) != 0) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] select operation requires source and mask array with same dimension and shape");
    }

    if (mask_arr->itype == CSN_COMPLEX) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] select operation requires real-array mask");
    }

    return OK;
}

static void select_assign_value(CSOUND *csound, CSN_ARRAY *out_arr, CSN_ARRAY *source_arr, CSN_ARRAY *mask_arr) {
    for (size_t i = 0, j = 0; i < source_arr->size; i++) {
        if (mask_arr->data[i] != 0.0) {
            switch (source_arr->itype) {
                case CSN_REAL:
                case CSN_COMPLEX:
                    if (source_arr->itype == CSN_REAL) {
                        out_arr->data[j] = source_arr->data[i];
                    } else {
                        out_arr->data[j * 2] = source_arr->data[i * 2];
                        out_arr->data[j * 2 + 1] = source_arr->data[i * 2 + 1];
                    }
                    break;
                case CSN_STRING:
                    if (out_arr->sdata[j] != NULL) csound->Free(csound, out_arr->sdata[j]);
                    out_arr->sdata[j] = csound->Strdup(csound, source_arr->sdata[i]);
                    break;
            }
            j++;
        }
    }
}

int32_t csnarray_select(CSOUND *csound, CSN_ARGWHERE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle, p->data_handle }, 3);

    CSN_ARRAY *source_arr = p->source_handle;
    CSN_ARRAY *mask_arr = p->data_handle;
    res = select_body(csound, NULL, source_arr, mask_arr);
    if (res != OK) goto done;

    uint32_t count_true = 0;
    for (uint32_t i = 0; i < mask_arr->size; i++) {
        if (mask_arr->data[i] != 0.0) count_true++;
    }

    ITEM_TYPE itype = source_arr->itype;
    uint32_t new_dim = 1U;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    new_shape[0] = count_true;

    const CSN_ARRAY *ops[2] = { source_arr, mask_arr };
    CSNARR_RES out = prepare_output(csound, &p->h, NULL, reg, p->handle, new_dim, new_shape, itype, ops, 2, CSN_ALIAS_REFUSE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    select_assign_value(csound, p->handle, source_arr, mask_arr);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_select_k_init(CSOUND *csound, CSN_ARGWHERE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle, p->data_handle }, 3);

    CSN_ARRAY *source_arr = p->source_handle;
    CSN_ARRAY *mask_arr = p->data_handle;
    res = select_body(csound, NULL, source_arr, mask_arr);
    if (res != OK) goto done;

    uint32_t count_true = 0;
    for (uint32_t i = 0; i < mask_arr->size; i++) {
        if (mask_arr->data[i] != 0.0) count_true++;
    }

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    new_shape[0] = count_true;
    ITEM_TYPE itype = source_arr->itype;

    const CSN_ARRAY *ops[2] = { source_arr, mask_arr };
    CSNARR_RES out = prepare_output(csound, &p->h, NULL, reg, p->handle, 1U, new_shape, itype, ops, 1, CSN_ALIAS_REFUSE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    reset_empty_csnarray(p->handle, 1U, new_shape, itype);

    SET_KDATA_BEGIN(p->k_data, reg, new_shape, 1U, itype);
    p->is_published = false;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_select_k(CSOUND *csound, CSN_ARGWHERE *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    CHECK_KTRIG(p->trig);
    int32_t res = OK;

    if (p->source_handle->itype == CSN_STRING) {
        int32_t res_rt = check_strings_not_rt(csound, &p->h, p->handle);
        if (res_rt != OK) return res_rt;
    }

    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle, p->data_handle }, 3);

    CSN_ARRAY *source_arr = p->source_handle;
    CSN_ARRAY *mask_arr = p->data_handle;
    res = select_body(csound, &p->h, source_arr, mask_arr);
    if (res != OK) goto done;

    if (p->is_published) {
        bool is_same_result = is_same_array_data_version(&p->k_data.prev_output_version, &p->handle->version);
        bool is_same_source = is_same_array_version(&p->versions.prev_a_version, &source_arr->version);
        bool is_same_mask = is_same_array_version(&p->versions.prev_b_version, &mask_arr->version);
        if (is_same_source && is_same_mask && is_same_result) goto done;
    }

    uint32_t count_true = 0;
    for (uint32_t i = 0; i < mask_arr->size; i++) {
        if (mask_arr->data[i] != 0.0) count_true++;
    }

    ITEM_TYPE itype = source_arr->itype;
    uint32_t new_dim = 1U;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    new_shape[0] = count_true;

    const CSN_ARRAY *ops[2] = { source_arr, mask_arr };
    CSNARR_RES out = prepare_output(csound, &p->h, &p->h, reg, p->handle, new_dim, new_shape, itype, ops, 2, CSN_ALIAS_REFUSE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    select_assign_value(csound, p->handle, source_arr, mask_arr);

    SET_KDATA_END(p->k_data, new_shape, new_dim, itype);
    set_array_version(&p->versions.prev_a_version, &source_arr->version);
    set_array_version(&p->versions.prev_b_version, &mask_arr->version);
    set_array_version(&p->k_data.prev_output_version, &p->handle->version);
    p->is_published = true;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}


int32_t csnarray_shuffle(CSOUND *csound, CSN_UNARYOP_IN *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);

    CSN_ARRAY *source_arr = p->source_handle;
    if (source_arr->itype == CSN_COMPLEX) {
        res = csn_init_error(csound, "[csnarray] shuffle operation supports real array only");
        goto done;
    }

    if (source_arr->size == 0) goto done;
    /* the generator is registry state shared by every instrument: each draw
       takes the lock, whether the array is global or not */
    if (!pres.locked) { csn_lock(csound, reg); pres.locked = true; }
    if (source_arr->itype == CSN_STRING) {
        fisher_yates_strings(&reg->rng, source_arr->sdata, source_arr->size);
    } else {
        fisher_yates(&reg->rng, source_arr->data, source_arr->size);
    }

    PUBLISH_INPLACE_WRITE(&p->k_data, source_arr, false, false, false);
    p->k_data.registry = reg;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_shuffle_k_init(CSOUND *csound, CSN_UNARYOP_IN *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    csn_mark_krate(p->source_handle);

    int32_t res = OK;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);

    CSN_ARRAY *source_arr = p->source_handle;
    if (source_arr->itype == CSN_COMPLEX) {
        res = csn_init_error(csound, "[csnarray] shuffle operation supports real array only");
        goto done;
    }

    p->k_data.registry = reg;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_shuffle_k(CSOUND *csound, CSN_UNARYOP_IN *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    CHECK_KTRIG(p->trig);
    int32_t res = OK;

    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);

    CSN_ARRAY *source_arr = p->source_handle;
    if (source_arr->itype == CSN_COMPLEX) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] shuffle operation supports real array only");
        goto done;
    }

    if (source_arr->size == 0) goto done;
    if (!pres.locked) { csn_lock(csound, reg); pres.locked = true; }
    if (source_arr->itype == CSN_STRING) {
        fisher_yates_strings(&reg->rng, source_arr->sdata, source_arr->size);
    } else {
        fisher_yates(&reg->rng, source_arr->data, source_arr->size);
    }

    PUBLISH_INPLACE_WRITE(&p->k_data, source_arr, false, false, false);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

/* The variadic inputs arrive as Csound argument pointers, one per array: a
   CSN_ARRAY ** walked by index, never a run of CSN_ARRAY structs. */
static int32_t stack_body(CSOUND *csound, OPDS *perf_h, uint32_t nargs, CSN_ARRAY *const *sources, uint32_t *ref_shape, uint32_t *ref_ndim, ITEM_TYPE *itype) {
    for (uint32_t i = 0; i < nargs; i++) {
        const CSN_ARRAY *source_arr = sources[i];
        if (i == 0) {
            *ref_ndim = source_arr->ndim;
            *itype = source_arr->itype;
            memcpy(ref_shape, source_arr->shape, sizeof(uint32_t) * CSN_MAX_DIMS);
            continue;
        }
        bool is_same_shape = memcmp(ref_shape, source_arr->shape, sizeof(uint32_t) * CSN_MAX_DIMS) == 0;
        if (!is_same_shape || *ref_ndim != source_arr->ndim) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] stack operation requires array with same shape and dimension");
        }
        if (*itype != source_arr->itype) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] stack operation requires array with same dtype");
        }
    }

    return OK;
}

/* Any of the stacked arrays or the output may be a global. */
static bool stack_lock_if_shared(CSOUND *csound, CSN_REGISTRY *reg, CSN_ARRAY *out, CSN_ARRAY *const *sources, uint32_t nargs) {
    bool locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { out }, 1);
    locked |= csn_lock_if_shared(csound, reg, (const CSN_ARRAY *const *) sources, nargs);
    return locked;
}

/* "*" takes any argument, so the arrays are checked here: all of the
   output's type, a CsnArr or a CsnStr. */
static int32_t stack_check_types(CSOUND *csound, const CSN_ARRAY *out, CSN_ARRAY *const *sources, uint32_t nargs, uint32_t first_arg) {
    const CS_TYPE *expected = CS_GET_ARG_TYPE(out);
    const char *name = expected == CS_TYPE_CSNSTR(csound) ? "CsnStr" : "CsnArr";
    for (uint32_t i = 0; i < nargs; i++) {
        const CS_TYPE *actual_type = CS_GET_ARG_TYPE(sources[i]);
        if (actual_type == NULL || actual_type != expected) {
            return csn_init_error(csound, "[csnarray] Every array stacked into a %s must be a %s: argument %u is not", name, name, first_arg + i);
        }
    }

    return OK;
}

static int32_t stack_assign_value(CSOUND *csound, CSN_ARRAY *stacked, uint32_t axis, CSN_ARRAY *const *sources) {
    CSN_BROADCAST_ITER it;
    if (ND_ITER_INIT(&it, stacked->ndim, stacked->shape, stacked->size, NULL) != OK) return NOTOK;
    while (BROADCAST_ITER_NEXT(&it)) {
        size_t linear = it.linear_index;
        const CSN_ARRAY *source = sources[it.coords[axis]];
        size_t src_offset = 0;
        for (uint32_t i = 0, j = 0; i < stacked->ndim; ++i) {
            if (i != axis) src_offset += (size_t) it.coords[i] * source->strides[j++];
        }
        switch (stacked->itype) {
            case CSN_REAL:
            case CSN_COMPLEX:
                if (stacked->itype == CSN_REAL) {
                    stacked->data[linear] = source->data[src_offset];
                } else {
                    stacked->data[linear * 2] = source->data[src_offset * 2];
                    stacked->data[linear * 2 + 1] = source->data[src_offset * 2 + 1];
                }
                break;
            case CSN_STRING:
                if (stacked->sdata[linear] != NULL) csound->Free(csound, stacked->sdata[linear]);
                stacked->sdata[linear] = csound->Strdup(csound, source->sdata[src_offset]);
                break;
        }
    }

    return OK;
}

int32_t csnarray_stack(CSOUND *csound, CSN_STACK *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t nargs = p->INOCOUNT - 1;
    if (nargs < 2) {
        return csn_init_error(csound, "[csnarray] Invalid number of handles: stack operation requires at least two array");
    }

    CSN_ARRAY *const *sources = (CSN_ARRAY *const *) p->source_handles;
    int32_t res = stack_check_types(csound, p->handle, sources, (uint32_t) nargs, 2U);
    if (res != OK) return res;

    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = stack_lock_if_shared(csound, reg, p->handle, sources, (uint32_t) nargs);

    uint32_t ref_shape[CSN_MAX_DIMS] = {0};
    uint32_t ref_ndim = 0U;
    ITEM_TYPE itype = CSN_REAL;
    res = stack_body(csound, NULL, (uint32_t) nargs, sources, ref_shape, &ref_ndim, &itype);
    if (res != OK) goto done;

    if (ref_ndim == CSN_MAX_DIMS) {
        res = csn_init_error(csound, "[csnarray] Source dimension must be less than 8");
        goto done;
    }

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    uint32_t new_ndim = ref_ndim + 1;

    CSN_AXIS_SPEC axis_spec = csn_normalize_axis_value((double) *p->axis, new_ndim);
    if (axis_spec.kind != CSN_AXIS_INDEX) {
        res = csn_init_error(csound, "[csnarray] Axis out of bounds");
        goto done;
    }
    uint32_t axis = axis_spec.index;

    for (uint32_t i = 0, j = 0; i < new_ndim; i++) {
        new_shape[i] = (i == axis) ? (uint32_t) nargs : ref_shape[j++];
    }

    CSNARR_RES out = prepare_output(csound, &p->h, NULL, reg, p->handle, new_ndim, new_shape, itype, (const CSN_ARRAY *const *) sources, (size_t) nargs, CSN_ALIAS_REFUSE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    if (stack_assign_value(csound, p->handle, axis, sources) != OK) {
        res = csn_init_error(csound, "[csnarray] Invalid stack iterator layout");
        goto done;
    }

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_stack_k_init(CSOUND *csound, CSN_STACK_K *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t nargs = p->INOCOUNT - 2;
    if (nargs < 2) {
        return csn_init_error(csound, "[csnarray] Invalid number of handles: stack operation requires at least two array");
    }

    CSN_ARRAY *const *sources = (CSN_ARRAY *const *) p->source_handles;
    int32_t res = stack_check_types(csound, p->handle, sources, (uint32_t) nargs, 3U);
    if (res != OK) return res;

    p->nargs = nargs;

    /* one recorded version per stacked array; a reinit reuses the struct */
    deinit_scratch(csound, &p->versions);
    ARRAY_VERSION *versions = NULL;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = stack_lock_if_shared(csound, reg, p->handle, sources, (uint32_t) nargs);

    versions = csound->Calloc(csound, sizeof(ARRAY_VERSION) * (size_t) nargs);
    if (versions == NULL) {
        res = csn_init_error(csound, "[csnarray] Internal error, memory allocation failed");
        goto done;
    }

    uint32_t ref_shape[CSN_MAX_DIMS] = {0};
    uint32_t ref_ndim = 0U;
    ITEM_TYPE itype = CSN_REAL;
    res = stack_body(csound, NULL, (uint32_t) nargs, sources, ref_shape, &ref_ndim, &itype);
    if (res != OK) goto done;

    if (ref_ndim == CSN_MAX_DIMS) {
        res = csn_init_error(csound, "[csnarray] Source dimension must be less than 8");
        goto done;
    }

    /* the axis is a k-argument: the output only takes a layout here, and the
       first pass stacks */
    CSNARR_RES out = prepare_output(csound, &p->h, NULL, reg, p->handle, ref_ndim, ref_shape, itype, (const CSN_ARRAY *const *) sources, (size_t) nargs, CSN_ALIAS_REFUSE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    p->versions.scratch = versions;
    p->versions.scratch_capacity = (size_t) nargs;
    p->versions.current_size = (size_t) nargs;
    versions = NULL;
    SET_KDATA_BEGIN(p->k_data, reg, ref_shape, ref_ndim, itype);
    p->is_published = false;

done:
    if (versions != NULL) csound->Free(csound, versions);
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_stack_k(CSOUND *csound, CSN_STACK_K *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    int32_t nargs = p->INOCOUNT - 2;
    if (nargs < 2 || nargs != p->nargs) {
        return csn_perf_error(csound, &p->h, "[csnarray] Invalid number of handles: stack operation requires at least two array and it is not allowed changing number of variadic args setted at init");
    }

    CHECK_KTRIG(p->trig);

    /* the mark is decided at init, so it is read here: every pass that stacks
       strings copies them */
    if (p->handle->itype == CSN_STRING) {
        int32_t res_rt = check_strings_not_rt(csound, &p->h, p->handle);
        if (res_rt != OK) return res_rt;
    }
    int32_t res = OK;

    CSN_ARRAY *const *sources = (CSN_ARRAY *const *) p->source_handles;
    ARRAY_VERSION *versions = (ARRAY_VERSION *) p->versions.scratch;

    CSNARR_RES pres = {0};
    pres.locked = stack_lock_if_shared(csound, reg, p->handle, sources, (uint32_t) nargs);

    uint32_t ref_shape[CSN_MAX_DIMS] = {0};
    uint32_t ref_ndim = 0U;
    ITEM_TYPE itype = CSN_REAL;
    res = stack_body(csound, &p->h, (uint32_t) nargs, sources, ref_shape, &ref_ndim, &itype);
    if (res != OK) goto done;

    if (ref_ndim == CSN_MAX_DIMS) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Source dimension must be less than 8");
        goto done;
    }

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    uint32_t new_ndim = ref_ndim + 1;

    CSN_AXIS_SPEC axis_spec = csn_normalize_axis_value((double) *p->axis, new_ndim);
    if (axis_spec.kind != CSN_AXIS_INDEX) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Axis out of bounds");
        goto done;
    }
    uint32_t axis = axis_spec.index;

    if (p->is_published) {
        bool is_same_args_versions = true;
        for (int32_t i = 0; i < nargs; i++) {
            if (!is_same_array_version(&sources[i]->version, &versions[i])) {
                is_same_args_versions = false;
                break;
            }
        }
        bool is_same_result = is_same_array_version(&p->k_data.prev_output_version, &p->handle->version);
        bool is_same_axis = p->k_data.prev_axis_u == axis;
        if (is_same_args_versions && is_same_result && is_same_axis) goto done;
    }

    for (uint32_t i = 0, j = 0; i < new_ndim; i++) {
        new_shape[i] = (i == axis) ? (uint32_t) nargs : ref_shape[j++];
    }

    CSNARR_RES out = prepare_output(csound, &p->h, &p->h, reg, p->handle, new_ndim, new_shape, itype, (const CSN_ARRAY *const *) sources, (size_t) nargs, CSN_ALIAS_REFUSE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    if (stack_assign_value(csound, p->handle, axis, sources) != OK) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Invalid stack iterator layout");
        goto done;
    }

    for (int32_t i = 0; i < nargs; i++) set_array_version(&versions[i], &sources[i]->version);
    set_array_version(&p->k_data.prev_output_version, &p->handle->version);
    SET_KDATA_END(p->k_data, new_shape, new_ndim, itype);
    p->k_data.prev_axis_u = axis;
    p->is_published = true;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_indexof(CSOUND *csound, CSN_ARGWHERE_INDEX *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    double wanted_value = (double) *p->value;
    if (!IS_VALID_VALUE(wanted_value)) {
        return csn_init_error(csound, "[csnarray] Wanted value is not a number");
    }

    int32_t res = OK;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle }, 2);

    CSN_ARRAY *source_arr = p->source_handle;
    res = argwhere_body(csound, NULL, source_arr, NULL);
    if (res != OK) goto done;

    int32_t index = get_linear_index_first_occurence(source_arr, wanted_value, EQUAL);
    uint32_t found = (uint32_t) (index != -1);

    uint32_t new_ndim = 1U;
    uint32_t new_shape[1] = { source_arr->ndim * found };

    const CSN_ARRAY *ops[1] = { source_arr };
    CSNARR_RES out = prepare_output(csound, &p->h, NULL, reg, p->handle, new_ndim, new_shape, CSN_REAL, ops, 1, CSN_ALIAS_REFUSE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    if (found) {
        if (indexof_assign_value(source_arr, wanted_value, p->handle) != OK) {
            res = csn_init_error(csound, "[csnarray] Invalid index iterator layout");
            goto done;
        }
    } else {
        reset_empty_csnarray(p->handle, new_ndim, new_shape, CSN_REAL);
    }

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_indexof_k_init(CSOUND *csound, CSN_ARGWHERE_INDEX *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle }, 2);

    CSN_ARRAY *source_arr = p->source_handle;
    res = argwhere_body(csound, NULL, source_arr, NULL);
    if (res != OK) goto done;

    uint32_t new_ndim = 1U;
    uint32_t new_shape[1] = { 1U };

    const CSN_ARRAY *ops[1] = { source_arr };
    CSNARR_RES out = prepare_output(csound, &p->h, NULL, reg, p->handle, new_ndim, new_shape, CSN_REAL, ops, 1, CSN_ALIAS_REFUSE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    reset_empty_csnarray(p->handle, new_ndim, new_shape, CSN_REAL);

    set_array_version(&p->k_data.prev_output_version, &p->handle->version);
    SET_KDATA_BEGIN(p->k_data, reg, new_shape, new_ndim, CSN_REAL);
    p->is_published = false;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_indexof_k(CSOUND *csound, CSN_ARGWHERE_INDEX *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    double wanted_value = (double) *p->value;
    if (!IS_VALID_VALUE(wanted_value)) {
        return csn_perf_error(csound, &p->h, "[csnarray] Wanted value is not a number");
    }

    CHECK_KTRIG(p->trig);
    int32_t res = OK;

    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle }, 2);

    CSN_ARRAY *source_arr = p->source_handle;
    res = argwhere_body(csound, &p->h, source_arr, NULL);
    if (res != OK) goto done;

    if (p->is_published) {
        if (CAN_REUSE_ELEMENTWISE(&p->k_data, source_arr, NULL, p->handle, wanted_value, 0.0)) goto done;
    }

    int32_t index = get_linear_index_first_occurence(source_arr, wanted_value, EQUAL);
    uint32_t found = (uint32_t) (index != -1);

    uint32_t new_ndim = 1U;
    uint32_t new_shape[CSN_MAX_DIMS] = { source_arr->ndim * found };

    const CSN_ARRAY *ops[1] = { source_arr };
    CSNARR_RES out = prepare_output(csound, &p->h, &p->h, reg, p->handle, new_ndim, new_shape, CSN_REAL, ops, 1, CSN_ALIAS_REFUSE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    if (found) {
        if (indexof_assign_value(source_arr, wanted_value, p->handle) != OK) {
            res = csn_locked_perf_error(csound, &p->h, "[csnarray] Invalid index iterator layout");
            goto done;
        }
    } else {
        reset_empty_csnarray(p->handle, new_ndim, new_shape, CSN_REAL);
    }

    SET_KDATA_END(p->k_data, new_shape, new_ndim, CSN_REAL);
    PUBLISH_ELEMENTWISE(&p->k_data, source_arr, NULL, p->handle, wanted_value, 0.0);
    p->is_published = true;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

static int32_t bincount_validate_and_find_size(CSOUND *csound, OPDS *perf_h, const CSN_ARRAY *source_arr, uint32_t *result_size) {
    if (source_arr->ndim != 1U) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Bincount source must be 1-D, got %u-D", source_arr->ndim);
    }

    uint32_t max_bin = 0;
    for (size_t i = 0; i < source_arr->size; ++i) {
        double value = source_arr->data[i];
        if (!isfinite(value) || trunc(value) != value || value < 0.0 || value >= (double) CSN_MAX_ELEMS) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Bincount source must contain non-negative integer values smaller than %zu", (size_t) CSN_MAX_ELEMS);
        }

        uint32_t bin = (uint32_t) value;
        if (bin > max_bin) max_bin = bin;
    }

    *result_size = source_arr->size == 0 ? 0U : max_bin + 1U;
    return OK;
}

static void bincount_assign_value(const CSN_ARRAY *source_arr, const CSN_ARRAY *weights_arr, CSN_ARRAY *arr) {
    if (arr->size > 0) {
        memset(arr->data, 0, sizeof(double) * arr->size);
    }

    for (size_t i = 0; i < source_arr->size; ++i) {
        uint32_t bin = (uint32_t) source_arr->data[i];
        arr->data[bin] += weights_arr == NULL ? 1.0 : weights_arr->data[i];
    }
}

int32_t csnarray_bincount_w(CSOUND *csound, CSN_ARGWHERE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle, p->data_handle }, 3);

    CSN_ARRAY *source_arr = p->source_handle;
    CSN_ARRAY *weights_arr = p->data_handle;
    res = argwhere_body(csound, NULL, source_arr, weights_arr);
    if (res != OK) goto done;

    if (weights_arr->ndim != 1U || weights_arr->size != source_arr->size) {
        res = csn_init_error(csound, "[csnarray] Weights array must be 1-D and have the same length as the source");
        goto done;
    }

    if (weights_arr->itype != CSN_REAL || source_arr->itype != CSN_REAL) {
        res = csn_init_error(csound, "[csnarray] Bincount source and weights must be real arrays");
        goto done;
    }

    uint32_t result_size = 0;
    res = bincount_validate_and_find_size(csound, NULL, source_arr, &result_size);
    if (res != OK) goto done;

    uint32_t new_ndim = 1U;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    new_shape[0] = result_size;

    const CSN_ARRAY *ops[2] = { source_arr, weights_arr };
    CSNARR_RES out = prepare_output(csound, &p->h, NULL, reg, p->handle, new_ndim, new_shape, CSN_REAL, ops, 2, CSN_ALIAS_REFUSE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    CSN_ARRAY *arr = p->handle;
    bincount_assign_value(source_arr, weights_arr, arr);

    SET_KDATA_BEGIN(p->k_data, reg, new_shape, new_ndim, CSN_REAL);
    PUBLISH_ELEMENTWISE(&p->k_data, source_arr, weights_arr, p->handle, 0.0, 0.0);
    p->is_published = false;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_bincount(CSOUND *csound, CSN_BINCOUNT_NO_WEIGHTS *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle }, 2);

    CSN_ARRAY *source_arr = p->source_handle;
    res = argwhere_body(csound, NULL, source_arr, NULL);
    if (res != OK) goto done;

    if (source_arr->itype != CSN_REAL) {
        res = csn_init_error(csound, "[csnarray] Bincount source must be a real array");
        goto done;
    }

    uint32_t result_size = 0;
    res = bincount_validate_and_find_size(csound, NULL, source_arr, &result_size);
    if (res != OK) goto done;

    uint32_t new_ndim = 1U;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    new_shape[0] = result_size;

    const CSN_ARRAY *ops[1] = { source_arr };
    CSNARR_RES out = prepare_output(csound, &p->h, NULL, reg, p->handle, new_ndim, new_shape, CSN_REAL, ops, 1, CSN_ALIAS_REFUSE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    CSN_ARRAY *arr = p->handle;
    bincount_assign_value(source_arr, NULL, arr);

    SET_KDATA_BEGIN(p->k_data, reg, new_shape, new_ndim, CSN_REAL);
    PUBLISH_ELEMENTWISE(&p->k_data, source_arr, NULL, p->handle, 0.0, 0.0);
    p->is_published = false;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_bincount_k(CSOUND *csound, CSN_BINCOUNT_NO_WEIGHTS *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    CHECK_KTRIG(p->trig);
    int32_t res = OK;

    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle }, 2);

    CSN_ARRAY *source_arr = p->source_handle;
    res = argwhere_body(csound, &p->h, source_arr, NULL);
    if (res != OK) goto done;

    if (p->is_published) {
        if (CAN_REUSE_ELEMENTWISE(&p->k_data, source_arr, NULL, p->handle, 0.0, 0.0)) goto done;
    }

    if (source_arr->itype != CSN_REAL) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Source/Weights array must be real array");
        goto done;
    }

    uint32_t result_size = 0;
    res = bincount_validate_and_find_size(csound, &p->h, source_arr, &result_size);
    if (res != OK) goto done;

    uint32_t new_ndim = 1U;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    new_shape[0] = result_size;

    const CSN_ARRAY *ops[1] = { source_arr };
    CSNARR_RES out = prepare_output(csound, &p->h, &p->h, reg, p->handle, new_ndim, new_shape, CSN_REAL, ops, 1, CSN_ALIAS_REFUSE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    bincount_assign_value(source_arr, NULL, p->handle);

    SET_KDATA_END(p->k_data, new_shape, new_ndim, CSN_REAL);
    PUBLISH_ELEMENTWISE(&p->k_data, source_arr, NULL, p->handle, 0.0, 0.0);
    p->is_published = true;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_bincount_w_k(CSOUND *csound, CSN_ARGWHERE *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    CHECK_KTRIG(p->trig);
    int32_t res = OK;

    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle, p->data_handle }, 3);

    CSN_ARRAY *source_arr = p->source_handle;
    CSN_ARRAY *weights_arr = p->data_handle;
    res = argwhere_body(csound, &p->h, source_arr, weights_arr);
    if (res != OK) goto done;

    if (p->is_published) {
        if (CAN_REUSE_ELEMENTWISE(&p->k_data, source_arr, weights_arr, p->handle, 0.0, 0.0)) goto done;
    }

    if (weights_arr->ndim != 1U || weights_arr->size != source_arr->size) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Weights array must be 1-D and have the same length as the source");
        goto done;
    }

    if (weights_arr->itype != CSN_REAL || source_arr->itype != CSN_REAL) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Bincount source and weights must be real arrays");
        goto done;
    }

    uint32_t result_size = 0;
    res = bincount_validate_and_find_size(csound, &p->h, source_arr, &result_size);
    if (res != OK) goto done;

    uint32_t new_ndim = 1U;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    new_shape[0] = result_size;

    const CSN_ARRAY *ops[2] = { source_arr, weights_arr };
    CSNARR_RES out = prepare_output(csound, &p->h, &p->h, reg, p->handle, new_ndim, new_shape, CSN_REAL, ops, 2, CSN_ALIAS_REFUSE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    bincount_assign_value(source_arr, weights_arr, p->handle);

    PUBLISH_ELEMENTWISE(&p->k_data, source_arr, weights_arr, p->handle, 0.0, 0.0);
    SET_KDATA_END(p->k_data, new_shape, new_ndim, CSN_REAL);
    p->is_published = true;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_searchsorted_arr(CSOUND *csound, CSN_SEARCHSORTED_ARR *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    if (!IS_VALID_ZERO_ONE((double) *p->side)) {
        return csn_init_error(csound, "[csnarray] Side should be 0 for left or 1 for right");
    }
    p->is_right_search_side = (uint32_t) *p->side == 1U;

    int32_t res = OK;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle, p->data_handle }, 3);

    CSN_ARRAY *source_arr = p->source_handle;
    CSN_ARRAY *data_arr = p->data_handle;

    if (source_arr->itype != CSN_REAL || data_arr->itype != CSN_REAL || source_arr->ndim != 1U || data_arr->ndim != 1U) {
        res = csn_init_error(csound, "[csnarray] Searchsorted requires 1-D real arrays");
        goto done;
    }

    uint32_t new_ndim = 1U;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    new_shape[0] = (uint32_t) data_arr->size;

    const CSN_ARRAY *ops[2] = { source_arr, data_arr };
    CSNARR_RES out = prepare_output(csound, &p->h, NULL, reg, p->handle, new_ndim, new_shape, CSN_REAL, ops, 2, CSN_ALIAS_REFUSE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    CSN_ARRAY *arr = p->handle;
    for (size_t i = 0; i < data_arr->size; i++) {
        double wanted_in = data_arr->data[i];
        size_t index = 0;
        binary_search(&index, NULL, 0, source_arr->data, wanted_in, source_arr->size, p->is_right_search_side);
        arr->data[i] = (double) index;
    }

    SET_KDATA_BEGIN(p->k_data, reg, new_shape, new_ndim, CSN_REAL);
    PUBLISH_ELEMENTWISE(&p->k_data, source_arr, data_arr, arr, 0.0, 0.0);
    p->is_published = false;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_searchsorted_scalar(CSOUND *csound, CSN_SEARCHSORTED_SCALAR *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    double wanted_in = (double) *p->value;
    if (!IS_VALID_ZERO_ONE((double) *p->side)) {
        return csn_init_error(csound, "[csnarray] Side should be 0 for left or 1 for right");
    }
    bool is_right_side = (uint32_t) *p->side == 1U;

    int32_t res = OK;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);

    CSN_ARRAY *source_arr = p->source_handle;
    if (source_arr->itype != CSN_REAL || source_arr->ndim != 1U) {
        res = csn_init_error(csound, "[csnarray] Searchsorted requires a 1-D real array");
        goto done;
    }

    size_t index = 0;
    binary_search(&index, NULL, 0, source_arr->data, wanted_in, source_arr->size, is_right_side);
    *p->index = (cs_float) index;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}


int32_t csnarray_searchsorted_scalar_k_init(CSOUND *csound, CSN_SEARCHSORTED_SCALAR *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    if (!IS_VALID_ZERO_ONE((double) *p->side)) {
        return csn_init_error(csound, "[csnarray] Side should be 0 for left or 1 for right");
    }
    p->is_right_search_side = (uint32_t) *p->side == 1U;

    int32_t res = OK;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);

    CSN_ARRAY *source_arr = p->source_handle;
    if (source_arr->itype != CSN_REAL || source_arr->ndim != 1U) {
        res = csn_init_error(csound, "[csnarray] Searchsorted requires a 1-D real array");
        goto done;
    }

    memset(&p->k_data, 0, sizeof(p->k_data));
    p->k_data.registry = reg;
    p->is_published = false;
    // a k value is not known at init: the first pass searches
    if (!csn_known_at_init(csound, p->value)) goto done;

    double wanted_in = (double) *p->value;
    size_t index = 0;
    binary_search(&index, NULL, 0, source_arr->data, wanted_in, source_arr->size, p->is_right_search_side);
    *p->index = (cs_float) index;

    PUBLISH_ELEMENTWISE(&p->k_data, source_arr, NULL, NULL, wanted_in, 0.0);
    p->k_data.registry = reg;
    p->k_data.prev_axis_u = (uint32_t) index;
    p->is_published = true;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}


int32_t csnarray_searchsorted_arr_k(CSOUND *csound, CSN_SEARCHSORTED_ARR *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    CHECK_KTRIG(p->trig);
    int32_t res = OK;

    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle, p->data_handle }, 3);

    CSN_ARRAY *source_arr = p->source_handle;
    CSN_ARRAY *data_arr = p->data_handle;

    if (source_arr->itype != CSN_REAL || data_arr->itype != CSN_REAL || source_arr->ndim != 1U || data_arr->ndim != 1U) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Searchsorted requires 1-D real arrays");
        goto done;
    }

    uint32_t new_ndim = 1U;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    new_shape[0] = (uint32_t) data_arr->size;

    if (p->is_published && CAN_REUSE_ELEMENTWISE(&p->k_data, source_arr, data_arr, p->handle, 0.0, 0.0)) goto done;

    const CSN_ARRAY *ops[2] = { source_arr, data_arr };
    CSNARR_RES out = prepare_output(csound, &p->h, &p->h, reg, p->handle, new_ndim, new_shape, source_arr->itype, ops, 2, CSN_ALIAS_REFUSE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    for (size_t i = 0; i < data_arr->size; i++) {
        double wanted_in = data_arr->data[i];
        size_t index = 0;
        binary_search(&index, NULL, 0, source_arr->data, wanted_in, source_arr->size, p->is_right_search_side);
        p->handle->data[i] = (double) index;
    }

    SET_KDATA_END(p->k_data, new_shape, new_ndim, CSN_REAL);
    PUBLISH_ELEMENTWISE(&p->k_data, source_arr, data_arr, p->handle, 0.0, 0.0);
    p->is_published = true;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_searchsorted_scalar_k(CSOUND *csound, CSN_SEARCHSORTED_SCALAR *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    CHECK_KTRIG(p->trig);
    int32_t res = OK;

    double wanted_in = (double) *p->value;

    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);

    CSN_ARRAY *source_arr = p->source_handle;
    if (source_arr->itype != CSN_REAL || source_arr->ndim != 1U) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Searchsorted requires a 1-D real array");
        goto done;
    }

    if (p->is_published) {
        bool is_same_source = is_same_array_version(&p->k_data.prev_source_version, &source_arr->version);
        bool is_same_wanted = p->k_data.prev_scalar_param == wanted_in;
        if (is_same_source && is_same_wanted) {
            *p->index = (cs_float) p->k_data.prev_axis_u;
            goto done;
        }
    }

    size_t index = 0;
    binary_search(&index, NULL, 0, source_arr->data, wanted_in, source_arr->size, p->is_right_search_side);
    *p->index = (cs_float) index;

    PUBLISH_ELEMENTWISE(&p->k_data, source_arr, NULL, NULL, wanted_in, 0.0);
    p->k_data.prev_axis_u = (uint32_t) index;
    p->is_published = true;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}
