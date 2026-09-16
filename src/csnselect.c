/* Opcode implementations for the select family.
   The public opcode inventory remains centralized in csnum.c. */
#include "csnset.h"
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
    size_t count = 0;
    for (size_t linear = 0; linear < source_arr->size; ++linear) {
        if (compare_match(source_arr->data[linear], cmp_value, mode)) count++;
    }
    return count;
}

static int32_t argwhere_body(CSOUND *csound, OPDS *perf_h, CSN_REGISTRY *reg, CSN_ARRAY **source_array, CSN_ARRAY **data_array, uint32_t source_handle, uint32_t data_handle) {
    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
    }

    CSN_ARRAY *source_arr = source_slot->array;

    if (source_arr->itype != CSN_REAL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] This operation is not implemented for complex arrays");
    }
    *source_array = source_slot->array;

    if (data_handle != INVALID_HANDLE) {
        CSN_SLOT *data_slot = get_slot(reg, data_handle);
        if (data_slot == NULL) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) data_handle);
        }
        CSN_ARRAY *data_arr = data_slot->array;
        uint32_t data_ndim = data_arr->ndim;
        if (data_ndim != 1U) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Data array must be 1-D, got %u-D", data_ndim);
        }
        *data_array = data_slot->array;
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

    uint32_t source_handle = p->source_handle->id;
    uint32_t data_handle = p->data_handle->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    CSN_ARRAY *data_arr = NULL;
    res = argwhere_body(csound, NULL, reg, &source_arr, &data_arr, source_handle, data_handle);
    if (res != OK) goto done;


    /* No match yields a {0, ndim} array rather than an invalid handle, so the
       result stays usable in a chain. Matches np.argwhere. */
    size_t count = count_elements_from_array(source_arr, data_arr, EQUAL);

    uint32_t new_shape[2] = { (uint32_t) count, source_arr->ndim };
    const uint32_t protect[2] = { source_handle, data_handle };
    if (create_csnarray_locked(csound, reg, &p->h, 2U, new_shape, &p->array, p->handle, protect, 2U, &err, source_arr->itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;
    if (argwhere_assign_value(source_arr, data_arr, arr) != OK) {
        res = csound->InitError(csound, "[csnarray] Invalid selection iterator layout");
        goto done;
    }

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_argwhere_k_init(CSOUND *csound, CSN_ARGWHERE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;
    uint32_t data_handle = p->data_handle->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    CSN_ARRAY *data_arr = NULL;
    res = argwhere_body(csound, NULL, reg, &source_arr, &data_arr, source_handle, data_handle);
    if (res != OK) goto done;

    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    const uint32_t protect[2] = { source_handle, data_handle };
    if (create_csnarray_locked(csound, reg, &p->h, source_ndim, source_shape, &p->array, p->handle, protect, 2U, &err, source_arr->itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    if (source_arr->size > 0) {
        memcpy(p->array->data, source_arr->data, sizeof(double) * source_arr->size * source_arr->itype);
        p->array->size = source_arr->size;
    }

    SET_KDATA_BEGIN(p, reg);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

/* csnargwhere takes a mask array before the trigger; csnargunique, csnargnonzero
   and csnargisnan do not, so on those the trigger is bound one slot earlier,
   into the data_handle pointer. The declared arity tells them apart. */
static inline const MYFLT *argwhere_trig(const OPDS *h, CSNREF *data_handle, MYFLT *trig) {
    return h->optext->t.inArgCount > 2 ? trig : (const MYFLT *) data_handle;
}

/* Same story for the comparison family: csnunique has no compared value, so its
   trigger lands in cmp_value. */
static inline const MYFLT *compare_trig(const OPDS *h, MYFLT *cmp_value, MYFLT *trig) {
    return h->optext->t.inArgCount > 2 ? trig : cmp_value;
}

int32_t csnarray_argwhere_k(CSOUND *csound, CSN_ARGWHERE *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REG_HANDLE(csound, &p->h, reg, p->k_data.owned_handle);

    uint32_t source_handle = p->source_handle->id;
    uint32_t data_handle = p->data_handle->id;

    int32_t res = OK;
    const char *err = NULL;
    res = CHECK_SELF_ALIAS(csound, &p->h, &p->k_data, source_handle, data_handle);
    if (res != OK) return res;

    CHECK_KTRIG(p->trig);

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    CSN_ARRAY *data_arr = NULL;
    res = argwhere_body(csound, &p->h, reg, &source_arr, &data_arr, source_handle, data_handle);
    if (res != OK) goto done;


    size_t count = count_elements_from_array(source_arr, data_arr, EQUAL);
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    new_shape[0] = (uint32_t) count;
    new_shape[1] = source_arr->ndim;

    size_t req_size = 0;
    if (get_array_size_from_shape(&req_size, 2U, new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSN_ARRAY *arr = NULL;
    size_t logical_size = count == 0 ? 0 : req_size;
    CSN_SLOT *reuse_slot = get_slot(reg, p->k_data.owned_handle);
    if (reuse_slot != NULL && CAN_REUSE_ELEMENTWISE(&p->k_data, source_handle, source_arr, data_handle, data_arr, reuse_slot->array, 0.0, 0.0)) {
        p->handle->id = p->k_data.owned_handle;
        goto done;
    }

    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, 2U, new_shape, logical_size, source_arr->itype, err);
    if (res != OK) goto done;
    if (argwhere_assign_value(source_arr, data_arr, arr) != OK) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Invalid selection iterator layout");
        goto done;
    }
    p->array = arr;

    SET_KDATA_END(p, new_shape, 2U, source_arr->itype);
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle, source_arr, data_handle, data_arr, arr, 0.0, 0.0);

done:
    csound->UnlockMutex(reg->mutex);
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
static int32_t csnarray_argselect_helper(CSOUND *csound, CSN_ARGWHERE *p, CSN_COMPARE_MODE mode) {
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

    if (source_arr->itype != CSN_REAL) {
        res = csound->InitError(csound, "[csnarray] This operation is not implemented for complex arrays");
        goto done;
    }

    size_t count = count_elements_from_value(source_arr, 0.0, mode);
    uint32_t new_shape[2] = { (uint32_t) count, source_arr->ndim };

    const uint32_t protect[1] = { source_handle };
    if (create_csnarray_locked(csound, reg, &p->h, 2U, new_shape, &p->array, p->handle, protect, 1U, &err, source_arr->itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;
    if (argselect_assign_value(source_arr, arr, mode) != OK) {
        res = csound->InitError(csound, "[csnarray] Invalid selection iterator layout");
        goto done;
    }

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_argselect_k_init(CSOUND *csound, CSN_ARGWHERE *p) {
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

    if (source_arr->itype != CSN_REAL) {
        res = csound->InitError(csound, "[csnarray] This operation is not implemented for complex arrays");
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
        memcpy(arr->data, source_arr->data, sizeof(double) * source_arr->size); // REAL
        arr->size = source_arr->size;
    }

    SET_KDATA_BEGIN(p, reg);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_argselect_k_helper(CSOUND *csound, CSN_ARGWHERE *p, CSN_COMPARE_MODE mode) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REG_HANDLE(csound, &p->h, reg, p->source_handle);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    const char *err = NULL;
    res = CHECK_SELF_ALIAS(csound, &p->h, &p->k_data, source_handle, 0);
    if (res != OK) return res;

    CHECK_KTRIG(argwhere_trig(&p->h, p->data_handle, p->trig));

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
    }

    CSN_ARRAY *source_arr = source_slot->array;

    if (source_arr->itype != CSN_REAL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] This operation is not implemented for complex arrays");
    }

    size_t count = count_elements_from_value(source_arr, 0.0, mode);
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    new_shape[0] = (uint32_t) count;
    new_shape[1] = source_arr->ndim;

    size_t req_size = 0;
    if (get_array_size_from_shape(&req_size, 2U, new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSN_ARRAY *arr = NULL;
    size_t logical_size = count == 0 ? 0 : req_size;
    CSN_SLOT *reuse_slot = get_slot(reg, p->k_data.owned_handle);
    if (reuse_slot != NULL && CAN_REUSE_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, reuse_slot->array, 0.0, 0.0)) {
        p->handle->id = p->k_data.owned_handle;
        goto done;
    }

    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, 2U, new_shape, logical_size, source_arr->itype, err);
    if (res != OK) goto done;

    p->array = arr;
    if (argselect_assign_value(source_arr, arr, mode) != OK) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Invalid selection iterator layout");
        goto done;
    }
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, arr, 0.0, 0.0);

    SET_KDATA_END(p, new_shape, 2U, CSN_REAL);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_argnonzero(CSOUND *csound, CSN_ARGWHERE *p) {
    return csnarray_argselect_helper(csound, p, NONZERO);
}

int32_t csnarray_argnonzero_k(CSOUND *csound, CSN_ARGWHERE *p) {
    return csnarray_argselect_k_helper(csound, p, NONZERO);
}

int32_t csnarray_argisnan(CSOUND *csound, CSN_ARGWHERE *p) {
    return csnarray_argselect_helper(csound, p, IS_NAN);
}

int32_t csnarray_argisnan_k(CSOUND *csound, CSN_ARGWHERE *p) {
    return csnarray_argselect_k_helper(csound, p, IS_NAN);
}

static void mask_select_assign_value(const CSN_ARRAY *source_arr, CSN_ARRAY *arr, CSN_COMPARE_MODE mode) {
    for (size_t i = 0; i < source_arr->size; i++) {
        arr->data[i] = compare_match(source_arr->data[i], 0.0, mode) ? 1.0 : 0.0;
    }
}

static int32_t mask_select_helper(CSOUND *csound, CSN_UNARYOP *p, CSN_COMPARE_MODE mode) {
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

    if (source_arr->itype != CSN_REAL) {
        res = csound->InitError(csound, "[csnarray] This operation is not implemented for complex arrays");
        goto done;
    }

    const uint32_t protect[1] = { source_handle };
    if (create_csnarray_locked(csound, reg, &p->h, source_arr->ndim, source_arr->shape, &p->array, p->handle, protect, 1U, &err, source_arr->itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;
    set_csnarray_layout(arr, source_arr->ndim, source_arr->shape, source_arr->size, CSN_REAL);
    mask_select_assign_value(source_arr, arr, mode);
    update_array_data_version(&arr->version);
    SET_KDATA_BEGIN(p, reg);
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, arr, 0.0, 0.0);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t mask_select_k_helper(CSOUND *csound, CSN_UNARYOP *p, CSN_COMPARE_MODE mode) {
    CSN_REGISTRY *reg = p->k_data.registry;
    uint32_t owned_handle = p->k_data.owned_handle;
    CHECK_REG_HANDLE(csound, &p->h, reg, owned_handle);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    const char *err = NULL;

    CHECK_KTRIG(p->trig);

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
        goto done;
    }
    CSN_ARRAY *source_arr = source_slot->array;

    if (source_arr->itype != CSN_REAL) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] This operation is not implemented for complex arrays");
        goto done;
    }

    res = CHECK_SELF_ALIAS_CELL_LOCAL(csound, &p->h, &p->k_data, source_handle, source_arr, source_arr->ndim, source_arr->shape, CSN_REAL);
    if (res != OK) goto done;

    CSN_SLOT *out_slot = get_slot(reg, owned_handle);
    if (out_slot != NULL && CAN_REUSE_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, out_slot->array, 0.0, 0.0)) {
        p->handle->id = owned_handle;
        goto done;
    }

    size_t req_size = 0;
    if (get_array_size_from_shape(&req_size, source_arr->ndim, source_arr->shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSN_ARRAY *arr = NULL;
    size_t logical_size = source_arr->size == 0 ? 0 : req_size;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, source_arr->ndim, source_arr->shape, logical_size, CSN_REAL, err);
    if (res != OK) goto done;
    p->array = arr;

    mask_select_assign_value(source_arr, arr, mode);

    SET_KDATA_END(p, source_arr->shape, source_arr->ndim, CSN_REAL);
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, arr, 0.0, 0.0);

done:
    csound->UnlockMutex(reg->mutex);
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

int32_t csnarray_argunique(CSOUND *csound, CSN_ARGWHERE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    const char *err = NULL;

    /* Declared before any goto: the done label frees it. */
    ARRAY_ELEMENT *temp = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = source_slot->array;

    if (source_arr->itype != CSN_REAL) {
        res = csound->InitError(csound, "[csnarray] This operation is not implemented for complex arrays");
        goto done;
    }
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    temp = csound->Calloc(csound, sizeof(ARRAY_ELEMENT) * source_arr->size);
    if (temp == NULL) {
        res = csound->InitError(csound, "[csnarray] Out of memory: allocation of %zu bytes failed", (size_t) (sizeof(ARRAY_ELEMENT) * source_arr->size));
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

    const uint32_t protect[1] = { source_handle };
    if (create_csnarray_locked(csound, reg, &p->h, 2U, new_shape, &p->array, p->handle, protect, 1U, &err, source_arr->itype) != OK) {
        csound->Free(csound, temp);
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;
    CSN_BROADCAST_ITER it;
    if (ND_ITER_INIT(&it, source_arr->ndim, source_shape, source_arr->size, NULL) != OK) {
        res = csound->InitError(csound, "[csnarray] Invalid unique-index iterator layout");
        goto done;
    }
    for (size_t i = 0; i < count; ++i) {
        ARRAY_ELEMENT *elem = &temp[i];
        if (ND_ITER_SEEK(&it, elem->linear_index) != OK) {
            res = csound->InitError(csound, "[csnarray] Unique index is out of range");
            goto done;
        }

        for (size_t j = 0; j < source_ndim; ++j) {
            arr->data[i * source_ndim + j] = (double) it.coords[j];
        }
    }

done:
    if (temp != NULL) {
        csound->Free(csound, temp);
    }
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_argunique_k_init(CSOUND *csound, CSN_ARGWHERE *p) {
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

    if (source_arr->itype != CSN_REAL) {
        res = csound->InitError(csound, "[csnarray] This operation is not implemented for complex arrays");
        goto done;
    }

    /* The source can grow to its capacity without reallocating; reserving that
       much keeps the perf pass from growing this on a marked path. */
    size_t capacity = source_arr->capacity > 0 ? source_arr->capacity : 1;
    ARRAY_ELEMENT *temp = csound->Calloc(csound, sizeof(ARRAY_ELEMENT) * capacity);
    if (temp == NULL) {
        res = csound->InitError(csound, "[csnarray] Out of memory: allocation of %zu bytes failed", (size_t) (sizeof(ARRAY_ELEMENT) * source_arr->size));
        goto done;
    }

    p->scratch.scratch = temp;
    p->scratch.scratch_capacity = capacity;

    const uint32_t protect[1] = { source_handle };
    if (create_csnarray_locked(csound, reg, &p->h, 2U, source_arr->shape, &p->array, p->handle, protect, 1U, &err, source_arr->itype) != OK) {
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

int32_t csnarray_argunique_k(CSOUND *csound, CSN_ARGWHERE *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REG_HANDLE(csound, &p->h, reg, p->k_data.owned_handle);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    const char *err = NULL;
    res = CHECK_SELF_ALIAS(csound, &p->h, &p->k_data, source_handle, 0);
    if (res != OK) return res;

    CHECK_KTRIG(argwhere_trig(&p->h, p->data_handle, p->trig));

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
    }

    CSN_ARRAY *source_arr = source_slot->array;

    if (source_arr->itype != CSN_REAL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] This operation is not implemented for complex arrays");
    }
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;
    size_t source_size = source_arr->size;

    res = csn_scratch_reserve(csound, &p->h, csn_slot_rt_locked(reg, p->k_data.owned_handle), &p->scratch, source_size, sizeof(ARRAY_ELEMENT));
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

    size_t req_size = 0;
    if (get_array_size_from_shape(&req_size, 2U, new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSN_ARRAY *arr = p->array;
    size_t logical_size = count == 0 ? 0 : req_size;
    CSN_SLOT *out_slot = get_slot(reg, p->k_data.owned_handle);
    if (out_slot != NULL && CAN_REUSE_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, out_slot->array, 0.0, 0.0)) {
        p->handle->id = p->k_data.owned_handle;
        goto done;
    }

    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, 2U, new_shape, logical_size, source_arr->itype, err);
    if (res != OK) goto done;

    CSN_BROADCAST_ITER it;
    if (ND_ITER_INIT(&it, source_arr->ndim, source_shape, source_arr->size, NULL) != OK) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Invalid unique-index iterator layout");
        goto done;
    }
    for (size_t i = 0; i < count; ++i) {
        ARRAY_ELEMENT *elem = &p->scratch.scratch[i];
        if (ND_ITER_SEEK(&it, elem->linear_index) != OK) {
            res = csn_locked_perf_error(csound, &p->h, "[csnarray] Unique index is out of range");
            goto done;
        }

        for (size_t j = 0; j < source_ndim; ++j) {
            arr->data[i * source_ndim + j] = (double) it.coords[j];
        }
    }

    SET_KDATA_END(p, new_shape, 2U, source_arr->itype);
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, arr, 0.0, 0.0);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_unique(CSOUND *csound, CSN_COMPARE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    const char *err = NULL;

    /* Declared before any goto: the done label frees it. */
    ARRAY_ELEMENT *temp = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = source_slot->array;

    if (source_arr->itype != CSN_REAL) {
        res = csound->InitError(csound, "[csnarray] This operation is not implemented for complex arrays");
        goto done;
    }

    temp = csound->Calloc(csound, sizeof(ARRAY_ELEMENT) * source_arr->size);
    if (temp == NULL) {
        res = csound->InitError(csound, "[csnarray] Out of memory: allocation of %zu bytes failed", (size_t) (sizeof(ARRAY_ELEMENT) * source_arr->size));
        goto done;
    }

    for (size_t i = 0; i < source_arr->size; i++) {
        temp[i].value = source_arr->data[i];
        temp[i].linear_index = (uint32_t) i;
    }

    size_t count = count_unique(temp, source_arr->size);
    uint32_t new_shape[1] = { (uint32_t) count };

    const uint32_t protect[1] = { source_handle };
    if (create_csnarray_locked(csound, reg, &p->h, 1U, new_shape, &p->array, p->handle, protect, 1U, &err, source_arr->itype) != OK) {
        csound->Free(csound, temp);
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;
    for (size_t i = 0; i < count; ++i) {
        arr->data[i] = temp[i].value;
    }

done:
    csound->Free(csound, temp);
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_unique_k_init(CSOUND *csound, CSN_COMPARE *p) {
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

    if (source_arr->itype != CSN_REAL) {
        res = csound->InitError(csound, "[csnarray] This operation is not implemented for complex arrays");
        goto done;
    }

    /* The source can grow to its capacity without reallocating; reserving that
       much keeps the perf pass from growing this on a marked path. */
    size_t capacity = source_arr->capacity > 0 ? source_arr->capacity : 1;
    ARRAY_ELEMENT *temp = csound->Calloc(csound, sizeof(ARRAY_ELEMENT) * capacity);
    if (temp == NULL) {
        res = csound->InitError(csound, "[csnarray] Out of memory: allocation of %zu bytes failed", (size_t) (sizeof(ARRAY_ELEMENT) * source_arr->size));
        goto done;
    }
    p->scratch.scratch = temp;
    p->scratch.scratch_capacity = capacity;

    const uint32_t protect[1] = { source_handle };
    if (create_csnarray_locked(csound, reg, &p->h, source_arr->ndim, source_arr->shape, &p->array, p->handle, protect, 1U, &err, source_arr->itype) != OK) {
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

int32_t csnarray_unique_k(CSOUND *csound, CSN_COMPARE *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REG_HANDLE(csound, &p->h, reg, p->k_data.owned_handle);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    const char *err = NULL;
    res = CHECK_SELF_ALIAS(csound, &p->h, &p->k_data, source_handle, 0);
    if (res != OK) return res;

    CHECK_KTRIG(compare_trig(&p->h, p->cmp_value, p->trig));

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
    }

    CSN_ARRAY *source_arr = source_slot->array;

    if (source_arr->itype != CSN_REAL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] This operation is not implemented for complex arrays");
    }

    size_t source_size = source_arr->size;
    res = csn_scratch_reserve(csound, &p->h, csn_slot_rt_locked(reg, p->k_data.owned_handle), &p->scratch, source_size, sizeof(ARRAY_ELEMENT));
    if (res != OK) goto done;

    ARRAY_ELEMENT *scratch = (ARRAY_ELEMENT *) p->scratch.scratch;

    for (size_t i = 0; i < source_arr->size; i++) {
        scratch[i].value = source_arr->data[i];
        scratch[i].linear_index = (uint32_t) i;
    }

    size_t count = count_unique(p->scratch.scratch, source_arr->size);
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    new_shape[0] = (uint32_t) count;

    size_t req_size = 0;
    if (get_array_size_from_shape(&req_size, 1U, new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSN_ARRAY *arr = NULL;
    size_t logical_size = count == 0 ? 0 : req_size;
    CSN_SLOT *out_slot = get_slot(reg, p->k_data.owned_handle);
    if (out_slot != NULL && CAN_REUSE_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, out_slot->array, 0.0, 0.0)) {
        p->handle->id = p->k_data.owned_handle;
        goto done;
    }

    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, 1U, new_shape, logical_size, source_arr->itype, err);
    if (res != OK) goto done;
    p->array = arr;

    for (size_t i = 0; i < count; ++i) {
        arr->data[i] = scratch[i].value;
    }

    SET_KDATA_END(p, new_shape, 1U, source_arr->itype);
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, arr, 0.0, 0.0);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_compare_helper(CSOUND *csound, CSN_COMPARE *p, CSN_COMPARE_MODE mode) {
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

    if (source_arr->itype != CSN_REAL) {
        res = csound->InitError(csound, "[csnarray] This operation is not implemented for complex arrays");
        goto done;
    }
    double cmp_value = (double) *p->cmp_value;

    // size_t count = count_elements_from_value(source_arr, cmp_value, mode);
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    memcpy(new_shape, source_arr->shape, sizeof(uint32_t) * CSN_MAX_DIMS);

    const uint32_t protect[1] = { source_handle };
    if (create_csnarray_locked(csound, reg, &p->h, source_arr->ndim, new_shape, &p->array, p->handle, protect, 1U, &err, source_arr->itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;

    for (size_t i = 0; i < source_arr->size; ++i) {
        double value = source_arr->data[i];
        if (compare_match(value, cmp_value, mode)) arr->data[i] = 1.0;
    }

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_compare_k_init(CSOUND *csound, CSN_COMPARE *p) {
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

    if (source_arr->itype != CSN_REAL) {
        res = csound->InitError(csound, "[csnarray] This operation is not implemented for complex arrays");
        goto done;
    }

    const uint32_t protect[1] = { source_handle };
    if (create_csnarray_locked(csound, reg, &p->h, source_arr->ndim, source_arr->shape, &p->array, p->handle, protect, 1U, &err, source_arr->itype) != OK) {
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

static int32_t csnarray_compare_k_helper(CSOUND *csound, CSN_COMPARE *p, CSN_COMPARE_MODE mode) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REG_HANDLE(csound, &p->h, reg, p->k_data.owned_handle);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    const char *err = NULL;
    CHECK_KTRIG(p->trig);

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
    }

    CSN_ARRAY *source_arr = source_slot->array;

    if (source_arr->itype != CSN_REAL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] This operation is not implemented for complex arrays");
    }
    double cmp_value = (double) *p->cmp_value;

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    memcpy(new_shape, source_arr->shape, sizeof(uint32_t) * CSN_MAX_DIMS);

    size_t req_size = 0;
    if (get_array_size_from_shape(&req_size, source_arr->ndim, new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    /* Comparing an array against a scalar writes each result cell from the cell
       it sits on, so feeding on its own output is legal as long as the layout
       holds still. */
    res = CHECK_SELF_ALIAS_CELL_LOCAL(csound, &p->h, &p->k_data, source_handle, source_arr, source_arr->ndim, new_shape, source_arr->itype);
    if (res != OK) goto done;

    CSN_SLOT *out_slot = get_slot(reg, p->k_data.owned_handle);
    if (out_slot != NULL && CAN_REUSE_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, out_slot->array, cmp_value, 0.0)) {
        p->handle->id = p->k_data.owned_handle;
        goto done;
    }

    CSN_ARRAY *arr = NULL;
    size_t logical_size = source_arr->size == 0 ? 0 : req_size;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, source_arr->ndim, new_shape, logical_size, source_arr->itype, err);
    if (res != OK) goto done;
    p->array = arr;

    /* Both branches are written, never just the matches: the slot is only
       reallocated when the request changes, so on a steady layout an unwritten
       cell would still be holding the 1.0 some earlier pass put there. */
    for (size_t i = 0; i < source_arr->size; ++i) {
        double value = source_arr->data[i];
        arr->data[i] = compare_match(value, cmp_value, mode) ? 1.0 : 0.0;
    }

    SET_KDATA_END(p, new_shape, source_arr->ndim, source_arr->itype);
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, arr, cmp_value, 0.0);

done:
    csound->UnlockMutex(reg->mutex);
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

    uint32_t source_handle = p->source_handle->id;
    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = source_slot->array;

    if (source_arr->itype != CSN_REAL) {
        res = csound->InitError(csound, "[csnarray] This operation is not implemented for complex arrays");
        goto done;
    }

    /* NONZERO and IS_NAN take no comparison value, so their opcodes have no
       such argument to read. */
    size_t count = (mode == NONZERO || mode == IS_NAN)
        ? count_elements_from_value(source_arr, 0.0, mode)
        : count_elements_from_value(source_arr, (double) *p->arg_a, mode);

    *p->value = (MYFLT) count;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_compare_count_k_init(CSOUND *csound, CSN_COUNT *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    p->registry = reg;
    return OK;
}

static int32_t csnarray_compare_count_k_helper(CSOUND *csound, CSN_COUNT *p, CSN_COMPARE_MODE mode) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    uint32_t source_handle = p->source_handle->id;
    int32_t res = OK;

    const MYFLT *trig = mode == EQUAL ? p->arg_b : p->arg_a;
    CHECK_KTRIG(trig);

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
    }

    CSN_ARRAY *source_arr = source_slot->array;

    if (source_arr->itype != CSN_REAL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] This operation is not implemented for complex arrays");
    }

    double cmp_key = mode == EQUAL ? (double) *p->arg_a : 0.0;
    if (CAN_REUSE_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, NULL, cmp_key, 0.0)) {
        csound->UnlockMutex(reg->mutex);
        return res;
    }

    size_t count = (mode == NONZERO || mode == IS_NAN)
        ? count_elements_from_value(source_arr, 0.0, mode)
        : count_elements_from_value(source_arr, cmp_key, mode);

    *p->value = (MYFLT) count;
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, NULL, cmp_key, 0.0);

    csound->UnlockMutex(reg->mutex);
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

static int32_t where_body(CSOUND *csound, OPDS *perf_h, CSN_REGISTRY *reg, CSN_ARRAY **source_array, CSN_ARRAY **true_array, CSN_ARRAY **false_array, double *false_scalar, CSNREF *shandle, CSNREF *thandle, CSNREF *fhandle) {
    uint32_t source_handle = shandle->id;
    uint32_t true_handle = thandle->id;

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
    }
    CSN_ARRAY *source_arr = source_slot->array;

    CSN_SLOT *true_slot = get_slot(reg, true_handle);
    if (true_slot == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) true_handle);
    }
    CSN_ARRAY *true_arr = true_slot->array;
    bool is_same_first_shape = memcmp(source_arr->shape, true_arr->shape, sizeof(true_arr->shape)) == 0;
    if (!is_same_first_shape || source_arr->ndim != true_arr->ndim) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] source array and true array replacement must have same shape and dim");
    }

    CSN_ARRAY *false_arr = NULL;
    if (fhandle != NULL) {
        uint32_t false_handle = fhandle->id;
        CSN_SLOT *false_slot = get_slot(reg, false_handle);
        if (false_slot == NULL) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", false_handle);
        }
        false_arr = false_slot->array;
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

    *source_array = source_slot->array;
    *true_array = true_slot->array;
    if (false_arr != NULL) *false_array = false_arr;
    return OK;
}

static int32_t csnarray_where_helper(CSOUND *csound, OPDS *h, CSNREF *source_handle, CSNREF *true_handle, CSNREF *false_handle, CSN_ARRAY **p_array, CSNREF *handle, double *false_scalar) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);
    CSN_ARRAY *source_arr = NULL;
    CSN_ARRAY *true_arr = NULL;
    CSN_ARRAY *false_arr = NULL;
    res = where_body(csound, NULL, reg, &source_arr, &true_arr, &false_arr, false_scalar, source_handle, true_handle, false_handle);
    if (res != OK) goto done;

    uint32_t new_dim = source_arr->ndim;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    memcpy(new_shape, source_arr->shape, sizeof(uint32_t) * CSN_MAX_DIMS);

    uint32_t fhandle = false_handle == NULL ? 0 : false_handle->id;
    uint32_t n_protect = false_handle == NULL ? 2 : 3;
    uint32_t protect[3] = { source_handle->id, true_handle->id, fhandle };
    if (create_csnarray_locked(csound, reg, h, new_dim, new_shape, p_array, handle, protect, n_protect, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = *p_array;
    size_t size = arr->size;
    for (size_t i = 0; i < size; i++) {
        double mask_value = source_arr->data[i];
        if (false_scalar != NULL) {
            arr->data[i] = mask_value == 0.0 ? *false_scalar : true_arr->data[i];
        } else {
            arr->data[i] = mask_value == 0.0 ? false_arr->data[i] : true_arr->data[i];
        }
    }
    *p_array = arr;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_where_hh(CSOUND *csound, CSN_WHERE_HH *p) {
    return csnarray_where_helper(csound, &p->h, p->source_handle, p->source_handle_true, p->source_handle_false, &p->array, p->handle, NULL);
}

int32_t csnarray_where_hs(CSOUND *csound, CSN_WHERE_HS *p) {
    double scalar_false = (double) *p->source_scalar_false;
    return csnarray_where_helper(csound, &p->h, p->source_handle, p->source_handle_true, NULL, &p->array, p->handle, &scalar_false);
}

static int32_t csnarray_where_k_init_helper(CSOUND *csound, OPDS *h, CSNREF *source_handle, CSNREF *true_handle, CSNREF *false_handle, CSN_ARRAY **p_array, CSNREF *handle, double *false_scalar, K_DATA *k_data, bool *is_published) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);
    CSN_ARRAY *source_arr = NULL;
    CSN_ARRAY *true_arr = NULL;
    CSN_ARRAY *false_arr = NULL;
    res = where_body(csound, NULL, reg, &source_arr, &true_arr, &false_arr, false_scalar, source_handle, true_handle, false_handle);
    if (res != OK) goto done;

    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    uint32_t fhandle = false_handle == NULL ? 0 : false_handle->id;
    uint32_t n_protect = false_handle == NULL ? 2 : 3;
    uint32_t protect[3] = { source_handle->id, true_handle->id, fhandle };
    if (create_csnarray_locked(csound, reg, h, source_ndim, source_shape, p_array, handle, protect, n_protect, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    reset_empty_csnarray(*p_array, source_ndim, source_shape, CSN_REAL);

    memset(k_data->prev_shape, 0, sizeof(k_data->prev_shape));
    memcpy(k_data->prev_shape, source_shape, sizeof(k_data->prev_shape));
    k_data->prev_ndim = source_ndim;
    k_data->prev_itype = CSN_REAL;
    k_data->owned_handle = handle->id;
    k_data->registry = reg;
    set_array_version(&k_data->prev_output_version, &(*p_array)->version);
    *is_published = false;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_where_k_helper(CSOUND *csound, OPDS *h, CSNREF *source_handle, CSNREF *true_handle, CSNREF *false_handle, CSN_ARRAY **p_array, CSNREF *handle, double *false_scalar, K_DATA *k_data, bool *is_published, CSN_WHERE_VERSION_K_STATE *versions, const MYFLT *trig) {
    CSN_REGISTRY *reg = k_data->registry;
    CHECK_REG_HANDLE(csound, h, reg, k_data->owned_handle);

    int32_t res = OK;
    const char *err = NULL;

    res = CHECK_SELF_ALIAS(csound, h, k_data, source_handle->id, true_handle->id);
    if (res != OK) return res;
    if (false_handle != NULL) {
        res = CHECK_SELF_ALIAS(csound, h, k_data, false_handle->id, 0);
        if (res != OK) return res;
    }

    CHECK_KTRIG(trig);

    csound->LockMutex(reg->mutex);
    CSN_ARRAY *source_arr = NULL;
    CSN_ARRAY *true_arr = NULL;
    CSN_ARRAY *false_arr = NULL;
    res = where_body(csound, h, reg, &source_arr, &true_arr, &false_arr, false_scalar, source_handle, true_handle, false_handle);
    if (res != OK) goto done;

    if (*is_published) {
        bool is_same_source_version = is_same_array_version(&versions->prev_a_version, &source_arr->version);
        bool is_same_true_version = is_same_array_version(&versions->prev_b_version, &true_arr->version);
        bool is_same_false = false;
        if (false_handle == NULL && false_scalar != NULL) {
            is_same_false = k_data->prev_scalar_param == *false_scalar;
        } else {
            is_same_false = is_same_array_version(&versions->prev_c_version, &false_arr->version);
        }

        CSN_SLOT *out_slot = get_slot(reg, k_data->owned_handle);
        bool is_same_result = out_slot != NULL && is_same_array_version(&k_data->prev_output_version, &out_slot->array->version);

        if (is_same_source_version && is_same_true_version && is_same_false && is_same_result) {
            handle->id = k_data->owned_handle;
            goto done;
        }
    }

    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    size_t req_size = 0;
    if (get_array_size_from_shape(&req_size, source_ndim, source_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSN_ARRAY *arr = NULL;
    size_t logical_size = source_arr->size == 0 ? 0 : req_size;
    res = NEED_TO_UPDATE_SLOT(csound, h, &arr, k_data, NULL, source_ndim, source_shape, logical_size, CSN_REAL, err);
    if (res != OK) {
        csound->UnlockMutex(reg->mutex);
        return res;
    }

    size_t size = arr->size;
    for (size_t i = 0; i < size; i++) {
        double mask_value = source_arr->data[i];
        if (false_scalar != NULL) {
            arr->data[i] = mask_value == 0.0 ? *false_scalar : true_arr->data[i];
        } else {
            arr->data[i] = mask_value == 0.0 ? false_arr->data[i] : true_arr->data[i];
        }
    }
    update_array_data_version(&arr->version);
    *p_array = arr;

    memset(k_data->prev_shape, 0, sizeof(k_data->prev_shape));
    memcpy(k_data->prev_shape, source_shape, sizeof(k_data->prev_shape));
    k_data->prev_ndim = source_ndim;
    handle->id = k_data->owned_handle;
    set_array_version(&k_data->prev_output_version, &(*p_array)->version);
    set_array_version(&versions->prev_a_version, &source_arr->version);
    set_array_version(&versions->prev_b_version, &true_arr->version);
    if (false_handle != NULL) {
        set_array_version(&versions->prev_c_version, &false_arr->version);
    } else {
        k_data->prev_scalar_param = *false_scalar;
    }
    *is_published = true;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_where_hh_k_init(CSOUND *csound, CSN_WHERE_HH *p) {
    return csnarray_where_k_init_helper(csound, &p->h, p->source_handle, p->source_handle_true, p->source_handle_false, &p->array, p->handle, NULL, &p->k_data, &p->is_published);
}

int32_t csnarray_where_hs_k_init(CSOUND *csound, CSN_WHERE_HS *p) {
    double scalar_false = (double) *p->source_scalar_false;
    return csnarray_where_k_init_helper(csound, &p->h, p->source_handle, p->source_handle_true, NULL, &p->array, p->handle, &scalar_false, &p->k_data, &p->is_published);
}

int32_t csnarray_where_hh_k(CSOUND *csound, CSN_WHERE_HH *p) {
    return csnarray_where_k_helper(csound, &p->h, p->source_handle, p->source_handle_true, p->source_handle_false, &p->array, p->handle, NULL, &p->k_data, &p->is_published, &p->versions, p->trig);
}

int32_t csnarray_where_hs_k(CSOUND *csound, CSN_WHERE_HS *p) {
    double scalar_false = (double) *p->source_scalar_false;
    return csnarray_where_k_helper(csound, &p->h, p->source_handle, p->source_handle_true, NULL, &p->array, p->handle, &scalar_false, &p->k_data, &p->is_published, &p->versions, p->trig);
}

static int32_t csnarray_where_in_helper(CSOUND *csound, CSNREF *source_handle, CSNREF *true_handle, CSNREF *false_handle, double *false_scalar) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    csound->LockMutex(reg->mutex);
    CSN_ARRAY *source_arr = NULL;
    CSN_ARRAY *true_arr = NULL;
    CSN_ARRAY *false_arr = NULL;
    int32_t res = where_body(csound, NULL, reg, &source_arr, &true_arr, &false_arr, false_scalar, source_handle, true_handle, false_handle);
    if (res != OK) {
        csound->UnlockMutex(reg->mutex);
        return res;
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

    csound->UnlockMutex(reg->mutex);
    return OK;
}

int32_t csnarray_where_in_hh(CSOUND *csound, CSN_WHERE_HH_IN *p) {
    return csnarray_where_in_helper(csound, p->source_handle, p->source_handle_true, p->source_handle_false, NULL);
}

int32_t csnarray_where_in_hs(CSOUND *csound, CSN_WHERE_HS_IN *p) {
    double scalar_false = (double) *p->source_scalar_false;
    return csnarray_where_in_helper(csound, p->source_handle, p->source_handle_true, NULL, &scalar_false);
}

static int32_t csnarray_where_k_in_init_helper(CSOUND *csound, CSNREF *source_handle, CSNREF *true_handle, CSNREF *false_handle, double *false_scalar, CSN_REGISTRY **registry, bool *is_published) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    CSN_ARRAY *true_arr = NULL;
    CSN_ARRAY *false_arr = NULL;
    int32_t res = where_body(csound, NULL, reg, &source_arr, &true_arr, &false_arr, false_scalar, source_handle, true_handle, false_handle);
    if (res != OK) {
        csound->UnlockMutex(reg->mutex);
        return res;
    }
    *registry = reg;
    *is_published = false;

    csound->UnlockMutex(reg->mutex);
    return OK;
}

static int32_t csnarray_where_k_in_helper(CSOUND *csound, OPDS *h, CSNREF *source_handle, CSNREF *true_handle, CSNREF *false_handle, double *false_scalar, CSN_REGISTRY *registry, bool *is_published, CSN_WHERE_VERSION_K_STATE *versions, const MYFLT *trig, double *prev_scalar_false) {
    CSN_REGISTRY *reg = registry;
    CHECK_REGISTRY(csound, h, reg);

    int32_t res = OK;
    CHECK_KTRIG(trig);

    csound->LockMutex(reg->mutex);
    CSN_ARRAY *source_arr = NULL;
    CSN_ARRAY *true_arr = NULL;
    CSN_ARRAY *false_arr = NULL;
    res = where_body(csound, h, reg, &source_arr, &true_arr, &false_arr, false_scalar, source_handle, true_handle, false_handle);
    if (res != OK) goto done;

    if (*is_published) {
        bool is_same_source_version = is_same_array_version(&versions->prev_a_version, &source_arr->version);
        bool is_same_true_version = is_same_array_version(&versions->prev_b_version, &true_arr->version);
        bool is_same_false = false;
        if (false_handle == NULL && false_scalar != NULL) {
            is_same_false = *prev_scalar_false == *false_scalar;
        } else {
            is_same_false = is_same_array_version(&versions->prev_c_version, &false_arr->version);
        }

        if (is_same_source_version && is_same_true_version && is_same_false) {
            goto done;
        }
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
    if (false_handle != NULL) {
        set_array_version(&versions->prev_c_version, &false_arr->version);
    } else {
        *prev_scalar_false = *false_scalar;
    }
    *is_published = true;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_where_in_hh_k_init(CSOUND *csound, CSN_WHERE_HH_IN *p) {
    return csnarray_where_k_in_init_helper(csound, p->source_handle, p->source_handle_true, p->source_handle_false, NULL, &p->registry, &p->is_published);
}

int32_t csnarray_where_in_hs_k_init(CSOUND *csound, CSN_WHERE_HS_IN *p) {
    double scalar_false = (double) *p->source_scalar_false;
    return csnarray_where_k_in_init_helper(csound, p->source_handle, p->source_handle_true, NULL, &scalar_false, &p->registry, &p->is_published);
}

int32_t csnarray_where_in_hh_k(CSOUND *csound, CSN_WHERE_HH_IN *p) {
    return csnarray_where_k_in_helper(csound, &p->h, p->source_handle, p->source_handle_true, p->source_handle_false, NULL, p->registry, &p->is_published, &p->versions, p->trig, &p->prev_scalar_false);
}

int32_t csnarray_where_in_hs_k(CSOUND *csound, CSN_WHERE_HS_IN *p) {
    double scalar_false = (double) *p->source_scalar_false;
    return csnarray_where_k_in_helper(csound, &p->h, p->source_handle, p->source_handle_true, NULL, &scalar_false, p->registry, &p->is_published, &p->versions, p->trig, &p->prev_scalar_false);
}

static int32_t compress_body(CSOUND *csound, OPDS *perf_h, CSN_REGISTRY *reg, CSN_ARRAY **source_array, CSN_ARRAY **mask, CSNREF *shandle, CSNREF *mhandle, const MYFLT *axis_in, int32_t *axis_out) {
    uint32_t source_handle = shandle->id;
    uint32_t mask_handle = mhandle->id;

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
    }
    CSN_ARRAY *source_arr = source_slot->array;
    uint32_t source_ndim = source_arr->ndim;

    CSN_SLOT *mask_slot = get_slot(reg, mask_handle);
    if (mask_slot == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", mask_handle);
    }
    CSN_ARRAY *mask_arr = mask_slot->array;
    if (mask_arr->itype != CSN_REAL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Mask must be real-array");
    }

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

    *source_array = source_slot->array;
    *mask = mask_slot->array;
    return OK;
}

static int32_t compress_assign_value(CSN_ARRAY *arr, CSN_ARRAY *source_arr, size_t count_true, size_t dst_size, uint32_t *indexes, int32_t axis_out, uint32_t ndim, ITEM_TYPE itype) {
    if (axis_out == -1) {
        for (size_t i = 0; i < count_true; i++) {
            size_t index = indexes[i];
            if (itype == CSN_REAL) {
                arr->data[i] = source_arr->data[index];
            } else {
                arr->data[i * 2] = source_arr->data[index * 2];
                arr->data[i * 2 + 1] = source_arr->data[index * 2 + 1];
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
            if (itype == CSN_REAL) {
                arr->data[i] = source_arr->data[src_off];
            } else {
                arr->data[i * 2] = source_arr->data[src_off * 2];
                arr->data[i * 2 + 1] = source_arr->data[src_off * 2 + 1];
            }
        }
    }
    return OK;
}

int32_t csnarray_compress(CSOUND *csound, CSN_WHERE_HS *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    CSNREF *source_handle = p->source_handle;
    CSNREF *mask_handle = p->source_handle_true;
    const MYFLT *axis_in = p->INOCOUNT > 2 ? p->source_scalar_false : NULL;

    int32_t res = OK;
    const char *err = NULL;
    uint32_t *indexes = NULL;

    csound->LockMutex(reg->mutex);
    CSN_ARRAY *source_arr = NULL;
    CSN_ARRAY *mask_arr = NULL;
    int32_t axis_out = -1;
    res = compress_body(csound, NULL, reg, &source_arr, &mask_arr, source_handle, mask_handle, axis_in, &axis_out);
    if (res != OK) goto done;

    indexes = csound->Calloc(csound, sizeof(uint32_t) * mask_arr->size);
    if (indexes == NULL) {
        res = csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
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

    uint32_t protect[2] = { source_handle->id, mask_handle->id };
    if (create_csnarray_locked(csound, reg, &p->h, new_dim, new_shape, &p->array, p->handle, protect, 2U, &err, itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    size_t size = p->array->size;
    if (compress_assign_value(p->array, source_arr, count_true, size, indexes, axis_out, new_dim, itype) != OK) {
        res = csound->InitError(csound, "[csnarray] Invalid compress iterator layout");
        goto done;
    }
    update_array_data_version(&p->array->version);

done:
    if (indexes != NULL) csound->Free(csound, indexes);
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_compress_k_init(CSOUND *csound, CSN_WHERE_HS *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);
    CSN_ARRAY *source_arr = NULL;
    CSN_ARRAY *mask_arr = NULL;
    int32_t axis_out = -1;
    const MYFLT *axis_in = p->INOCOUNT > 3 ? p->trig : NULL;
    res = compress_body(csound, NULL, reg, &source_arr, &mask_arr, p->source_handle, p->source_handle_true, axis_in, &axis_out);
    if (res != OK) goto done;

    uint32_t count_true = 0;
    for (uint32_t i = 0; i < mask_arr->size; i++) {
        if (mask_arr->data[i] != 0.0) count_true++;
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

    size_t scratch_capacity = source_arr->capacity;
    if (scratch_capacity == 0) scratch_capacity = 1;
    uint32_t *indexes = csound->Calloc(csound, sizeof(uint32_t) * scratch_capacity);
    if (indexes == NULL) {
        res = csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
        goto done;
    }
    p->scratch.scratch = indexes;
    p->scratch.scratch_capacity = scratch_capacity;

    uint32_t protect[2] = { p->source_handle->id, p->source_handle_true->id };
    if (create_csnarray_locked(csound, reg, &p->h, new_dim, new_shape, &p->array, p->handle, protect, 2U, &err, itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    reset_empty_csnarray(p->array, new_dim, new_shape, itype);
    SET_KDATA_BEGIN(p, reg);
    p->is_published = false;

done:
    csound->UnlockMutex(reg->mutex);
    if (res != OK) deinit_scratch(csound, &p->scratch);
    return res;
}

int32_t csnarray_compress_k(CSOUND *csound, CSN_WHERE_HS *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    uint32_t owned_handle = p->k_data.owned_handle;
    CHECK_REG_HANDLE(csound, &p->h, reg, owned_handle);

    CSNREF *source_handle = p->source_handle;
    CSNREF *mask_handle = p->source_handle_true;
    const MYFLT *axis_in = p->INOCOUNT > 3 ? p->trig : NULL;

    int32_t res = OK;
    const char *err = NULL;

    res = CHECK_SELF_ALIAS(csound, &p->h, &p->k_data, source_handle->id, mask_handle->id);
    if (res != OK) return res;

    CHECK_KTRIG(p->source_scalar_false);

    csound->LockMutex(reg->mutex);
    CSN_ARRAY *source_arr = NULL;
    CSN_ARRAY *mask_arr = NULL;
    int32_t axis_out = -1;
    res = compress_body(csound, &p->h, reg, &source_arr, &mask_arr, source_handle, mask_handle, axis_in, &axis_out);
    if (res != OK) goto done;

    if (p->is_published) {
        bool is_same_source = is_same_array_version(&p->versions.prev_a_version, &source_arr->version);
        bool is_same_mask = is_same_array_version(&p->versions.prev_b_version, &mask_arr->version);
        bool is_same_axis = p->k_data.prev_axis_u == axis_out;

        bool is_same_result = false;
        CSN_SLOT *slot = get_slot(reg, owned_handle);
        if (slot != NULL) {
            is_same_result = is_same_array_version(&p->k_data.prev_output_version, &slot->array->version);
        }

        if (is_same_source && is_same_mask && is_same_axis && is_same_result) {
            p->handle->id = p->k_data.owned_handle;
            goto done;
        }
    }

    res = csn_scratch_reserve(csound, &p->h, csn_slot_rt_locked(reg, p->k_data.owned_handle), &p->scratch, mask_arr->size, sizeof(uint32_t));
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

    size_t req_size = 0;
    if (get_array_size_from_shape(&req_size, new_dim, new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSN_ARRAY *arr = NULL;
    size_t logical_size = (source_arr->size == 0 || mask_arr->size == 0) ? 0 : req_size;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, new_dim, new_shape, logical_size, itype, err);
    if (res != OK) goto done;
    p->array = arr;

    if (compress_assign_value(p->array, source_arr, count_true, p->array->size, indexes_temp, axis_out, new_dim, itype) != OK) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Invalid compress iterator layout");
        goto done;
    }
    SET_KDATA_END(p, new_shape, new_dim, itype);
    p->k_data.prev_axis_u = axis_out;
    set_array_version(&p->k_data.prev_output_version, &p->array->version);
    set_array_version(&p->versions.prev_a_version, &source_arr->version);
    set_array_version(&p->versions.prev_b_version, &mask_arr->version);
    p->scratch.scratch = indexes_temp;
    p->is_published = true;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}


static int32_t select_body(CSOUND *csound, OPDS *perf_h, CSN_ARRAY **source_array, CSN_ARRAY **mask_array, CSN_REGISTRY *reg, uint32_t source_handle, uint32_t mask_handle) {
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
    }
    CSN_ARRAY *source_arr = slot->array;

    CSN_SLOT *mask_slot = get_slot(reg, mask_handle);
    if (mask_slot == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", mask_handle);
    }
    CSN_ARRAY *mask_arr = mask_slot->array;

    if (mask_arr->ndim != source_arr->ndim || memcmp(mask_arr->shape, source_arr->shape, sizeof(uint32_t) * CSN_MAX_DIMS) != 0) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] select operation requires source and mask array with same dimension and shape");
    }

    if (mask_arr->itype == CSN_COMPLEX) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] select operation requires real-array mask");
    }

    *source_array = source_arr;
    *mask_array = mask_arr;
    return OK;
}

static void select_assign_value(CSN_ARRAY *out_arr, CSN_ARRAY *source_arr, CSN_ARRAY *mask_arr) {
    for (size_t i = 0, j = 0; i < source_arr->size; i++) {
        if (mask_arr->data[i] != 0.0) {
            if (source_arr->itype == CSN_REAL) {
                out_arr->data[j] = source_arr->data[i];
            } else {
                out_arr->data[j * 2] = source_arr->data[i * 2];
                out_arr->data[j * 2 + 1] = source_arr->data[i * 2 + 1];
            }
            j++;
        }
    }
}

int32_t csnarray_select(CSOUND *csound, CSN_ARGWHERE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;
    uint32_t mask_handle = p->data_handle->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);
    CSN_ARRAY *source_arr = NULL;
    CSN_ARRAY *mask_arr = NULL;
    res = select_body(csound, NULL, &source_arr, &mask_arr, reg, source_handle, mask_handle);
    if (res != OK) goto done;

    uint32_t count_true = 0;
    for (uint32_t i = 0; i < mask_arr->size; i++) {
        if (mask_arr->data[i] != 0.0) count_true++;
    }

    ITEM_TYPE itype = source_arr->itype;
    uint32_t new_dim = 1U;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    new_shape[0] = count_true;

    uint32_t protect[2] = { source_handle, mask_handle };
    if (create_csnarray_locked(csound, reg, &p->h, new_dim, new_shape, &p->array, p->handle, protect, 2U, &err, itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    select_assign_value(p->array, source_arr, mask_arr);
    update_array_data_version(&p->array->version);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_select_k_init(CSOUND *csound, CSN_ARGWHERE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;
    uint32_t mask_handle = p->data_handle->id;
    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);
    CSN_ARRAY *source_arr = NULL;
    CSN_ARRAY *mask_arr = NULL;
    res = select_body(csound, NULL, &source_arr, &mask_arr, reg, source_handle, mask_handle);
    if (res != OK) goto done;

    uint32_t count_true = 0;
    for (uint32_t i = 0; i < mask_arr->size; i++) {
        if (mask_arr->data[i] != 0.0) count_true++;
    }

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    new_shape[0] = count_true;
    ITEM_TYPE itype = source_arr->itype;
    uint32_t protect[2] = { source_handle, mask_handle };
    if (create_csnarray_locked(csound, reg, &p->h, 1U, new_shape, &p->array, p->handle, protect, 2U, &err, itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    reset_empty_csnarray(p->array, 1U, new_shape, itype);

    SET_KDATA_BEGIN(p, reg);
    p->is_published = false;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_select_k(CSOUND *csound, CSN_ARGWHERE *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    uint32_t owned_handle = p->k_data.owned_handle;
    CHECK_REG_HANDLE(csound, &p->h, reg, owned_handle);

    uint32_t source_handle = p->source_handle->id;
    uint32_t mask_handle = p->data_handle->id;

    int32_t res = OK;
    const char *err = NULL;

    CHECK_KTRIG(p->trig);

    csound->LockMutex(reg->mutex);
    CSN_ARRAY *source_arr = NULL;
    CSN_ARRAY *mask_arr = NULL;
    res = select_body(csound, &p->h, &source_arr, &mask_arr, reg, source_handle, mask_handle);
    if (res != OK) goto done;

    if (p->is_published) {
        bool is_same_result = false;
        CSN_SLOT *res_slot = get_slot(reg, p->k_data.owned_handle);
        if (res_slot != NULL) {
            is_same_result = is_same_array_data_version(&p->k_data.prev_output_version, &res_slot->array->version);
        }
        bool is_same_source = is_same_array_version(&p->versions.prev_a_version, &source_arr->version);
        bool is_same_mask = is_same_array_version(&p->versions.prev_b_version, &mask_arr->version);
        if (is_same_source && is_same_mask && is_same_result) {
            p->handle->id = p->k_data.owned_handle;
            goto done;
        }
    }

    uint32_t count_true = 0;
    for (uint32_t i = 0; i < mask_arr->size; i++) {
        if (mask_arr->data[i] != 0.0) count_true++;
    }

    ITEM_TYPE itype = source_arr->itype;
    uint32_t new_dim = 1U;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    new_shape[0] = count_true;

    size_t req_size = 0;
    if (get_array_size_from_shape(&req_size, new_dim, new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSN_ARRAY *arr = NULL;
    size_t logical_size = (mask_arr->size == 0 || source_arr->size == 0) ? 0 : req_size;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, new_dim, new_shape, logical_size, itype, err);
    if (res != OK) goto done;
    p->array = arr;

    select_assign_value(p->array, source_arr, mask_arr);
    update_array_data_version(&p->array->version);

    SET_KDATA_END(p, new_shape, new_dim, itype);
    p->is_published = true;
    set_array_version(&p->versions.prev_a_version, &source_arr->version);
    set_array_version(&p->versions.prev_b_version, &mask_arr->version);
    set_array_version(&p->k_data.prev_output_version, &p->array->version);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}


int32_t csnarray_shuffle(CSOUND *csound, CSN_UNARYOP_IN *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;
    int32_t res = OK;

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = slot->array;
    if (source_arr->itype == CSN_COMPLEX) {
        res = csound->InitError(csound, "[csnarray] shuffle operation supports real array only");
        goto done;
    }

    if (source_arr->size == 0) goto done;
    fisher_yates(&reg->rng, source_arr->data, source_arr->size);
    PUBLISH_INPLACE_WRITE(&p->k_data, source_handle, source_arr, false, false, false);
    p->k_data.registry = reg;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_shuffle_k_init(CSOUND *csound, CSN_UNARYOP_IN *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;
    int32_t res = OK;

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = slot->array;
    if (source_arr->itype == CSN_COMPLEX) {
        res = csound->InitError(csound, "[csnarray] shuffle operation supports real array only");
        goto done;
    }

    p->k_data.registry = reg;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_shuffle_k(CSOUND *csound, CSN_UNARYOP_IN *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    uint32_t source_handle = p->source_handle->id;
    int32_t res = OK;

    CHECK_KTRIG(p->trig);

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
    }

    CSN_ARRAY *source_arr = slot->array;
    if (source_arr->itype == CSN_COMPLEX) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] shuffle operation supports real array only");
    }

    if (source_arr->size == 0) goto done;
    fisher_yates(&reg->rng, source_arr->data, source_arr->size);
    PUBLISH_INPLACE_WRITE(&p->k_data, source_handle, source_arr, false, false, false);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t stack_body(CSOUND *csound, OPDS *perf_h, CSN_REGISTRY *reg, uint32_t nargs, const uint32_t *shandle, CSN_ARRAY *sources, uint32_t *ref_shape, uint32_t *ref_ndim, ITEM_TYPE *itype) {
    CSN_SLOT *slot;
    for (int32_t i = 0; i < nargs; i++) {
        uint32_t source_handle = shandle[i];
        slot = get_slot(reg, source_handle);
        if (slot == NULL) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
        }
        CSN_ARRAY *source_arr = slot->array;
        if (i > 0) {
            bool is_same_shape = memcmp(ref_shape, source_arr->shape, sizeof(uint32_t) * CSN_MAX_DIMS) == 0;
            if (!is_same_shape || *ref_ndim != source_arr->ndim) {
                return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] stack operation requires array with same shape and dimension");
            }
            if (*itype != source_arr->itype) {
                return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] stack operation requires array with same dtype");
            }
        }
        if (i == 0) {
            *ref_ndim = source_arr->ndim;
            *itype = source_arr->itype;
            memcpy(ref_shape, source_arr->shape, sizeof(uint32_t) * CSN_MAX_DIMS);
        }

        memcpy(sources + i, source_arr, sizeof(CSN_ARRAY));
    }
    return OK;
}

static int32_t stack_assign_value(CSN_ARRAY *stacked, uint32_t axis, CSN_ARRAY *sources) {
    CSN_BROADCAST_ITER it;
    if (ND_ITER_INIT(&it, stacked->ndim, stacked->shape, stacked->size, NULL) != OK) return NOTOK;
    while (BROADCAST_ITER_NEXT(&it)) {
        size_t linear = it.linear_index;
        uint32_t source_index = it.coords[axis];
        const CSN_ARRAY *source = &sources[source_index];
        size_t src_offset = 0;
        for (uint32_t i = 0, j = 0; i < stacked->ndim; ++i) {
            if (i != axis) src_offset += (size_t) it.coords[i] * source->strides[j++];
        }
        if (stacked->itype == CSN_REAL) {
            stacked->data[linear] = source->data[src_offset];
        } else {
            stacked->data[linear * 2] = source->data[src_offset * 2];
            stacked->data[linear * 2 + 1] = source->data[src_offset * 2 + 1];
        }
    }
    return OK;
}

int32_t csnarray_stack(CSOUND *csound, CSN_STACK *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    CSN_ARRAY *sources = NULL;
    uint32_t *source_handles = NULL;

    int32_t nargs = p->INOCOUNT - 1;
    if (nargs < 2) {
        return csound->InitError(csound, "[csnarray] Invalid number of handles: stack operation requires at least two array");
    }

    sources = csound->Calloc(csound, sizeof(CSN_ARRAY) * nargs);
    if (sources == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
    }

    source_handles = csound->Calloc(csound, sizeof(uint32_t) * nargs);
    if (source_handles == NULL) {
        csound->Free(csound, sources);
        return csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
    }

    int32_t res = OK;
    const char *err = NULL;

    const CS_TYPE *expected = CS_TYPE_CSNARR(csound);
    for (uint32_t i = 0; i < (uint32_t) nargs; i++) {
        CSNREF *shandle = (CSNREF *) p->source_handles[i];
        const CS_TYPE *actuale_type = CS_GET_ARG_TYPE(shandle);
        if (actuale_type == NULL || actuale_type != expected) {
            csound->Free(csound, sources);
            csound->Free(csound, source_handles);
            return csound->InitError(csound, "[csnarray] Invalid handle: handles must be valid CsnArr");
        }
        source_handles[i] = shandle->id;
    }

    csound->LockMutex(reg->mutex);
    uint32_t ref_shape[CSN_MAX_DIMS] = {0};
    uint32_t ref_ndim = 0U;
    ITEM_TYPE itype;
    res = stack_body(csound, NULL, reg, nargs, source_handles, sources, ref_shape, &ref_ndim, &itype);
    if (res != OK) goto done;

    if (ref_ndim == CSN_MAX_DIMS) {
        res = csound->InitError(csound, "[csnarray] Source dimension must be less than 8");
        goto done;
    }

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    uint32_t new_ndim = ref_ndim + 1;

    CSN_AXIS_SPEC axis_spec = csn_normalize_axis_value((double) *p->axis, new_ndim);
    if (axis_spec.kind != CSN_AXIS_INDEX) {
        res = csound->InitError(csound, "[csnarray] Axis out of bounds");
        goto done;
    }
    uint32_t axis = axis_spec.index;

    for (uint32_t i = 0, j = 0; i < new_ndim; i++) {
        new_shape[i] = (i == axis) ? (uint32_t) nargs : ref_shape[j++];
    }

    if (create_csnarray_locked(csound, reg, &p->h, new_ndim, new_shape, &p->array, p->handle, source_handles, (uint32_t) nargs, &err, itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    if (stack_assign_value(p->array, axis, sources) != OK) {
        res = csound->InitError(csound, "[csnarray] Invalid stack iterator layout");
        goto done;
    }

done:
    if (sources != NULL) csound->Free(csound, sources);
    if (source_handles != NULL) csound->Free(csound, source_handles);
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_stack_k_init(CSOUND *csound, CSN_STACK_K *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t nargs = p->INOCOUNT - 2;
    if (nargs < 2) {
        return csound->InitError(csound, "[csnarray] Invalid number of handles: stack operation requires at least two array");
    }

    CSN_ARRAY *sources = csound->Calloc(csound, sizeof(CSN_ARRAY) * nargs);
    if (sources == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
    }

    CSN_ARRAY *sources_temp = csound->Calloc(csound, sizeof(CSN_ARRAY) * nargs);
    if (sources_temp == NULL) {
        csound->Free(csound, sources);
        return csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
    }

    uint32_t *source_handles = csound->Calloc(csound, sizeof(uint32_t) * nargs);
    if (source_handles == NULL) {
        csound->Free(csound, sources);
        csound->Free(csound, sources_temp);
        return csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
    }

    int32_t res = OK;
    const char *err = NULL;

    const CS_TYPE *expected = CS_TYPE_CSNARR(csound);
    for (uint32_t i = 0; i < (uint32_t) nargs; i++) {
        CSNREF *shandle = (CSNREF *) p->source_handles[i];
        const CS_TYPE *actuale_type = CS_GET_ARG_TYPE(shandle);
        if (actuale_type == NULL || actuale_type != expected) {
            csound->Free(csound, sources);
            csound->Free(csound, sources_temp);
            csound->Free(csound, source_handles);
            return csound->InitError(csound, "[csnarray] Invalid handle: handle must be valid CsnArr");
        }
        source_handles[i] = shandle->id;
    }

    p->buffer_sources.scratch = sources;
    p->buffer_sources.scratch_capacity = nargs;
    p->buffer_temp_sources.scratch = sources_temp;
    p->buffer_temp_sources.scratch_capacity = nargs;
    p->buffer_handles.scratch = source_handles;
    p->buffer_handles.scratch_capacity = nargs;
    p->nargs = nargs;

    csound->LockMutex(reg->mutex);
    uint32_t ref_shape[CSN_MAX_DIMS] = {0};
    uint32_t ref_ndim = 0U;
    ITEM_TYPE itype;
    CSN_ARRAY *bsources = (CSN_ARRAY *) p->buffer_sources.scratch;
    uint32_t *bhandles = (uint32_t *) p->buffer_handles.scratch;
    res = stack_body(csound, NULL, reg, nargs, bhandles, bsources, ref_shape, &ref_ndim, &itype);
    if (res != OK) goto done;

    if (ref_ndim == CSN_MAX_DIMS) {
        res = csound->InitError(csound, "[csnarray] Source dimension must be less than 8");
        goto done;
    }

    if (create_csnarray_locked(csound, reg, &p->h, ref_ndim, ref_shape, &p->array, p->handle, bhandles, (uint32_t) nargs, &err, itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    SET_KDATA_BEGIN(p, reg);
    p->is_published = false;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_stack_k(CSOUND *csound, CSN_STACK_K *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    uint32_t owned_handle = p->k_data.owned_handle;
    CHECK_REG_HANDLE(csound, &p->h, reg, owned_handle);

    int32_t nargs = p->INOCOUNT - 2;
    if (nargs < 2 || nargs != p->nargs) {
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid number of handles: stack operation requires at least two array and it is not allowed changing number of variadic args setted at init");
    }

    int32_t res = OK;
    const char *err = NULL;

    CHECK_KTRIG(p->trig);

    for (uint32_t i = 0; i < (uint32_t) nargs; i++) {
        CSNREF *shandle = (CSNREF *) p->source_handles[i];
        uint32_t prev_id = ((uint32_t *) p->buffer_handles.scratch)[i];
        if (prev_id != shandle->id) {
            return csound->PerfError(csound, &p->h, "[csnarray] Handles mismatch: it is not allowed to change handle at perf-time");
        }
        res = CHECK_SELF_ALIAS(csound, &p->h, &p->k_data, prev_id, 0);
        if (res != OK) return res;
    }

    csound->LockMutex(reg->mutex);
    CSN_ARRAY *bsources = (CSN_ARRAY *) p->buffer_sources.scratch;
    uint32_t *bhandles = (uint32_t *) p->buffer_handles.scratch;

    CSN_ARRAY *bsources_temp = (CSN_ARRAY *) p->buffer_temp_sources.scratch;
    memcpy(bsources_temp, bsources, sizeof(CSN_ARRAY) * p->buffer_sources.scratch_capacity);

    uint32_t ref_shape[CSN_MAX_DIMS] = {0};
    uint32_t ref_ndim = 0U;
    ITEM_TYPE itype;
    res = stack_body(csound, &p->h, reg, nargs, bhandles, bsources, ref_shape, &ref_ndim, &itype);
    if (res != OK) goto done;

    if (ref_ndim == CSN_MAX_DIMS) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Source dimension must be less than 8");
    }

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    uint32_t new_ndim = ref_ndim + 1;

    CSN_AXIS_SPEC axis_spec = csn_normalize_axis_value((double) *p->axis, new_ndim);
    if (axis_spec.kind != CSN_AXIS_INDEX) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Axis out of bounds");
    }
    uint32_t axis = axis_spec.index;

    if (p->is_published) {
        bool is_same_args_versions = true;
        for (int32_t i = 0; i < p->nargs; i++) {
            CSNREF *shandle = (CSNREF *) p->source_handles[i];
            CSN_SLOT *temp_slot = get_slot(reg, shandle->id);
            if (temp_slot != NULL) {
                CSN_ARRAY *s = &bsources_temp[i];
                if (!is_same_array_version(&s->version, &temp_slot->array->version)) {
                    is_same_args_versions = false;
                    break;
                }
            }
        }

        bool is_same_result = false;
        CSN_SLOT *res_slot = get_slot(reg, owned_handle);
        if (res_slot != NULL){
            is_same_result = is_same_array_version(&p->k_data.prev_output_version, &res_slot->array->version);
        }

        bool is_same_axis = p->k_data.prev_axis_u == axis;
        if (is_same_args_versions && is_same_result && is_same_axis) {
            p->handle->id = owned_handle;
            goto done;
        }
    }

    for (uint32_t i = 0, j = 0; i < new_ndim; i++) {
        new_shape[i] = (i == axis) ? (uint32_t) nargs : ref_shape[j++];
    }

    size_t req_size = 0;
    if (get_array_size_from_shape(&req_size, new_ndim, new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSN_ARRAY *arr = NULL;
    size_t logical_size = req_size;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, new_ndim, new_shape, logical_size, itype, err);
    if (res != OK) goto done;
    p->array = arr;

    if (stack_assign_value(p->array, axis, p->buffer_sources.scratch) != OK) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Invalid stack iterator layout");
        goto done;
    }
    set_array_version(&p->k_data.prev_output_version, &p->array->version);
    SET_KDATA_END(p, new_shape, new_ndim, itype);
    p->k_data.prev_axis_u = axis;
    p->is_published = true;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_indexof_deinit(CSOUND *csound, CSN_ARGWHERE_INDEX *p) {
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

int32_t csnarray_indexof(CSOUND *csound, CSN_ARGWHERE_INDEX *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;
    double wanted_value = (double) *p->value;
    if (!IS_VALID_VALUE(wanted_value)) {
        return csound->InitError(csound, "[csnarray] Wanted value is not a number");
    }

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    res = argwhere_body(csound, NULL, reg, &source_arr, NULL, source_handle, 0);
    if (res != OK) goto done;

    int32_t index = get_linear_index_first_occurence(source_arr, wanted_value, EQUAL);
    uint32_t found = (uint32_t) (index != -1);

    uint32_t new_ndim = 1U;
    uint32_t new_shape[1] = { source_arr->ndim * found };
    if (create_csnarray_locked(csound, reg, &p->h, new_ndim, new_shape, &p->array, p->handle, &source_handle, 1U, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    if (found) {
        if (indexof_assign_value(source_arr, wanted_value, p->array) != OK) {
            res = csound->InitError(csound, "[csnarray] Invalid index iterator layout");
            goto done;
        }
    } else {
        reset_empty_csnarray(p->array, new_ndim, new_shape, CSN_REAL);
    }

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_indexof_k_init(CSOUND *csound, CSN_ARGWHERE_INDEX *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);
    CSN_ARRAY *source_arr = NULL;
    res = argwhere_body(csound, NULL, reg, &source_arr, NULL, source_handle, 0);
    if (res != OK) goto done;

    uint32_t new_ndim = 1U;
    uint32_t new_shape[1] = { 1U };
    if (create_csnarray_locked(csound, reg, &p->h, new_ndim, new_shape, &p->array, p->handle, &source_handle, 1U, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    reset_empty_csnarray(p->array, new_ndim, new_shape, CSN_REAL);

    set_array_version(&p->k_data.prev_output_version, &p->array->version);
    SET_KDATA_BEGIN(p, reg);
    p->is_published = false;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_indexof_k(CSOUND *csound, CSN_ARGWHERE_INDEX *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    uint32_t owned_handle = p->k_data.owned_handle;
    CHECK_REG_HANDLE(csound, &p->h, reg, owned_handle);

    uint32_t source_handle = p->source_handle->id;
    double wanted_value = (double) *p->value;
    if (!IS_VALID_VALUE(wanted_value)) {
        return csound->PerfError(csound, &p->h, "[csnarray] Wanted value is not a number");
    }

    int32_t res = OK;
    const char *err = NULL;

    res = CHECK_SELF_ALIAS(csound, &p->h, &p->k_data, source_handle, 0);
    if (res != OK) return res;

    CHECK_KTRIG(p->trig);

    csound->LockMutex(reg->mutex);
    CSN_ARRAY *source_arr = NULL;
    res = argwhere_body(csound, &p->h, reg, &source_arr, NULL, source_handle, 0);
    if (res != OK) goto done;

    if (p->is_published) {
        CSN_SLOT *slot_res = get_slot(reg, owned_handle);
        if (slot_res != NULL && CAN_REUSE_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, slot_res->array, wanted_value, 0.0)) {
            p->handle->id = owned_handle;
            goto done;
        }
    }

    int32_t index = get_linear_index_first_occurence(source_arr, wanted_value, EQUAL);
    uint32_t found = (uint32_t) (index != -1);

    uint32_t new_ndim = 1U;
    uint32_t new_shape[1] = { source_arr->ndim * found };

    size_t req_size = 0;
    if (get_array_size_from_shape(&req_size, new_ndim, new_shape) != OK) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
        goto done;
    }

    CSN_ARRAY *arr = NULL;
    size_t logical_size = req_size;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, new_ndim, new_shape, logical_size, CSN_REAL, err);
    if (res != OK) goto done;
    p->array = arr;

    if (found) {
        if (indexof_assign_value(source_arr, wanted_value, p->array) != OK) {
            res = csn_locked_perf_error(csound, &p->h, "[csnarray] Invalid index iterator layout");
            goto done;
        }
    } else {
        reset_empty_csnarray(p->array, new_ndim, new_shape, CSN_REAL);
    }

    SET_KDATA_END(p, new_shape, new_ndim, CSN_REAL);
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, p->array, wanted_value, 0.0);
    p->is_published = true;

done:
    csound->UnlockMutex(reg->mutex);
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

int32_t csnarray_bincount_no_w_deinit(CSOUND *csound, CSN_BINCOUNT_NO_WEIGHTS *p) {
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

int32_t csnarray_bincount_w(CSOUND *csound, CSN_ARGWHERE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;
    uint32_t weights_handle = p->data_handle->id;

    int32_t res = OK;
    const char *err = NULL;
    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    CSN_ARRAY *weights_arr = NULL;
    res = argwhere_body(csound, NULL, reg, &source_arr, &weights_arr, source_handle, weights_handle);
    if (res != OK) goto done;

    if (weights_arr->ndim != 1U || weights_arr->size != source_arr->size) {
        res = csound->InitError(csound, "[csnarray] Weights array must be 1-D and have the same length as the source");
        goto done;
    }

    if (weights_arr->itype != CSN_REAL || source_arr->itype != CSN_REAL) {
        res = csound->InitError(csound, "[csnarray] Bincount source and weights must be real arrays");
        goto done;
    }

    uint32_t result_size = 0;
    res = bincount_validate_and_find_size(csound, NULL, source_arr, &result_size);
    if (res != OK) goto done;

    uint32_t new_ndim = 1U;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    new_shape[0] = result_size;
    const uint32_t protect[2] = { source_handle, weights_handle };
    if (create_csnarray_locked(csound, reg, &p->h, new_ndim, new_shape, &p->array, p->handle, protect, 2U, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;
    bincount_assign_value(source_arr, weights_arr, arr);

    SET_KDATA_BEGIN(p, reg);
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle, source_arr, weights_handle, weights_arr, p->array, 0.0, 0.0);
    p->is_published = false;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_bincount(CSOUND *csound, CSN_BINCOUNT_NO_WEIGHTS *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    const char *err = NULL;
    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    res = argwhere_body(csound, NULL, reg, &source_arr, NULL, source_handle, 0);
    if (res != OK) goto done;

    if (source_arr->itype != CSN_REAL) {
        res = csound->InitError(csound, "[csnarray] Bincount source must be a real array");
        goto done;
    }

    uint32_t result_size = 0;
    res = bincount_validate_and_find_size(csound, NULL, source_arr, &result_size);
    if (res != OK) goto done;

    uint32_t new_ndim = 1U;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    new_shape[0] = result_size;
    if (create_csnarray_locked(csound, reg, &p->h, new_ndim, new_shape, &p->array, p->handle, &source_handle, 1U, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;
    bincount_assign_value(source_arr, NULL, arr);

    SET_KDATA_BEGIN(p, reg);
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, p->array, 0.0, 0.0);
    p->is_published = false;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_bincount_k(CSOUND *csound, CSN_BINCOUNT_NO_WEIGHTS *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    uint32_t owned_handle = p->k_data.owned_handle;
    CHECK_REG_HANDLE(csound, &p->h, reg, owned_handle);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    const char *err = NULL;
    res = CHECK_SELF_ALIAS(csound, &p->h, &p->k_data, source_handle, 0);
    if (res != OK) return res;

    CHECK_KTRIG(p->trig);

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    res = argwhere_body(csound, &p->h, reg, &source_arr, NULL, source_handle, 0);
    if (res != OK) goto done;

    if (p->is_published) {
        CSN_SLOT *res_slot = get_slot(reg, owned_handle);
        if (res_slot != NULL && CAN_REUSE_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, res_slot->array, 0.0, 0.0)) {
            p->handle->id = owned_handle;
            goto done;
        }
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

    size_t req_size = 0;
    if (get_array_size_from_shape(&req_size, new_ndim, new_shape) != OK) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
        goto done;
    }

    CSN_ARRAY *arr = NULL;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, new_ndim, new_shape, req_size, CSN_REAL, err);
    if (res != OK) goto done;
    p->array = arr;

    bincount_assign_value(source_arr, NULL, p->array);

    SET_KDATA_END(p, new_shape, new_ndim, CSN_REAL);
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, p->array, 0.0, 0.0);
    p->is_published = true;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_bincount_w_k(CSOUND *csound, CSN_ARGWHERE *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    uint32_t owned_handle = p->k_data.owned_handle;
    CHECK_REG_HANDLE(csound, &p->h, reg, owned_handle);

    uint32_t source_handle = p->source_handle->id;
    uint32_t weights_handle = p->data_handle->id;


    int32_t res = OK;
    const char *err = NULL;
    res = CHECK_SELF_ALIAS(csound, &p->h, &p->k_data, source_handle, weights_handle);
    if (res != OK) return res;

    CHECK_KTRIG(p->trig);

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    CSN_ARRAY *weights_arr = NULL;
    res = argwhere_body(csound, &p->h, reg, &source_arr, &weights_arr, source_handle, weights_handle);
    if (res != OK) goto done;

    if (p->is_published) {
        CSN_SLOT *res_slot = get_slot(reg, owned_handle);
        if (res_slot != NULL && CAN_REUSE_ELEMENTWISE(&p->k_data, source_handle, source_arr, weights_handle, weights_arr, res_slot->array, 0.0, 0.0)) {
            p->handle->id = owned_handle;
            goto done;
        }
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

    size_t req_size = 0;
    if (get_array_size_from_shape(&req_size, new_ndim, new_shape) != OK) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
        goto done;
    }

    CSN_ARRAY *arr = NULL;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, new_ndim, new_shape, req_size, CSN_REAL, err);
    if (res != OK) goto done;
    p->array = arr;

    bincount_assign_value(source_arr, weights_arr, p->array);

    PUBLISH_ELEMENTWISE(&p->k_data, source_handle, source_arr, weights_handle, weights_arr, p->array, 0.0, 0.0);
    SET_KDATA_END(p, new_shape, new_ndim, CSN_REAL);
    p->is_published = true;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}


int32_t csnarray_searchsorted_a_deinit(CSOUND *csound, CSN_SEARCHSORTED_ARR *p) {
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

int32_t csnarray_searchsorted_arr(CSOUND *csound, CSN_SEARCHSORTED_ARR *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;
    uint32_t data_handle = p->data_handle->id;
    if (!IS_VALID_ZERO_ONE((double) *p->side)) {
        return csound->InitError(csound, "[csnarray] Side should be 0 for left or 1 for right");
    }
    p->is_right_search_side = (uint32_t) *p->side == 1U;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);
    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
        goto done;
    }
    CSN_SLOT *data_slot = get_slot(reg, data_handle);
    if (data_slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", data_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = source_slot->array;
    CSN_ARRAY *data_arr = data_slot->array;

    if (source_arr->itype != CSN_REAL || data_arr->itype != CSN_REAL || source_arr->ndim != 1U || data_arr->ndim != 1U) {
        res = csound->InitError(csound, "[csnarray] Searchsorted requires 1-D real arrays");
        goto done;
    }

    uint32_t new_ndim = 1U;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    new_shape[0] = (uint32_t) data_arr->size;
    const uint32_t protect[2] = { source_handle, data_handle };
    if (create_csnarray_locked(csound, reg, &p->h, new_ndim, new_shape, &p->array, p->handle, protect, 2U, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;
    for (size_t i = 0; i < data_arr->size; i++) {
        double wanted_in = data_arr->data[i];
        size_t index = 0;
        binary_search(&index, NULL, 0, source_arr->data, wanted_in, source_arr->size, p->is_right_search_side);
        arr->data[i] = (double) index;
    }

    SET_KDATA_BEGIN(p, reg);
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle, source_arr, data_handle, data_arr, arr, 0.0, 0.0);
    p->is_published = false;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_searchsorted_scalar(CSOUND *csound, CSN_SEARCHSORTED_SCALAR *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;
    double wanted_in = (double) *p->value;
    if (!IS_VALID_ZERO_ONE((double) *p->side)) {
        return csound->InitError(csound, "[csnarray] Side should be 0 for left or 1 for right");
    }
    bool is_right_side = (uint32_t) *p->side == 1U;

    int32_t res = OK;

    csound->LockMutex(reg->mutex);
    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = source_slot->array;

    if (source_arr->itype != CSN_REAL || source_arr->ndim != 1U) {
        res = csound->InitError(csound, "[csnarray] Searchsorted requires a 1-D real array");
        goto done;
    }

    size_t index = 0;
    binary_search(&index, NULL, 0, source_arr->data, wanted_in, source_arr->size, is_right_side);
    *p->index = (MYFLT) index;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}


int32_t csnarray_searchsorted_scalar_k_init(CSOUND *csound, CSN_SEARCHSORTED_SCALAR *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;
    int32_t res = OK;
    if (!IS_VALID_ZERO_ONE((double) *p->side)) {
        return csound->InitError(csound, "[csnarray] Side should be 0 for left or 1 for right");
    }
    p->is_right_search_side = (uint32_t) *p->side == 1U;

    csound->LockMutex(reg->mutex);
    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = source_slot->array;
    if (source_arr->itype != CSN_REAL || source_arr->ndim != 1U) {
        res = csound->InitError(csound, "[csnarray] Searchsorted requires a 1-D real array");
        goto done;
    }

    double wanted_in = (double) *p->value;
    size_t index = 0;
    binary_search(&index, NULL, 0, source_arr->data, wanted_in, source_arr->size, p->is_right_search_side);
    *p->index = (MYFLT) index;

    PUBLISH_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, NULL, wanted_in, 0.0);
    p->k_data.registry = reg;
    p->k_data.prev_axis_u = (uint32_t) index;
    p->is_published = true;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}


int32_t csnarray_searchsorted_arr_k(CSOUND *csound, CSN_SEARCHSORTED_ARR *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    uint32_t owned_handle = p->k_data.owned_handle;
    CHECK_REG_HANDLE(csound, &p->h, reg, owned_handle);

    uint32_t source_handle = p->source_handle->id;
    uint32_t data_handle = p->data_handle->id;

    int32_t res = OK;
    const char *err = NULL;

    res = CHECK_SELF_ALIAS(csound, &p->h, &p->k_data, source_handle, data_handle);
    if (res != OK) return res;

    CHECK_KTRIG(p->trig);

    csound->LockMutex(reg->mutex);
    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
        goto done;
    }
    CSN_SLOT *data_slot = get_slot(reg, data_handle);
    if (data_slot == NULL) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", data_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = source_slot->array;
    CSN_ARRAY *data_arr = data_slot->array;

    if (source_arr->itype != CSN_REAL || data_arr->itype != CSN_REAL || source_arr->ndim != 1U || data_arr->ndim != 1U) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Searchsorted requires 1-D real arrays");
        goto done;
    }

    uint32_t new_ndim = 1U;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    new_shape[0] = (uint32_t) data_arr->size;

    CSN_SLOT *reuse_slot = get_slot(reg, p->k_data.owned_handle);
    if (p->is_published && reuse_slot != NULL && CAN_REUSE_ELEMENTWISE(&p->k_data, source_handle, source_arr, data_handle, data_arr, reuse_slot->array, 0.0, 0.0)) {
        p->handle->id = p->k_data.owned_handle;
        goto done;
    }

    size_t req_size = 0;
    if (get_array_size_from_shape(&req_size, new_ndim, new_shape) != OK) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
        goto done;
    }

    CSN_ARRAY *arr = NULL;
    size_t logical_size = data_arr->size == 0 ? 0 : req_size;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, new_ndim, new_shape, logical_size, source_arr->itype, err);
    if (res != OK) goto done;
    p->array = arr;

    for (size_t i = 0; i < data_arr->size; i++) {
        double wanted_in = data_arr->data[i];
        size_t index = 0;
        binary_search(&index, NULL, 0, source_arr->data, wanted_in, source_arr->size, p->is_right_search_side);
        arr->data[i] = (double) index;
    }

    SET_KDATA_END(p, new_shape, new_ndim, CSN_REAL);
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle, source_arr, data_handle, data_arr, p->array, 0.0, 0.0);
    p->is_published = true;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_searchsorted_scalar_k(CSOUND *csound, CSN_SEARCHSORTED_SCALAR *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    uint32_t source_handle = p->source_handle->id;
    double wanted_in = (double) *p->value;

    CHECK_KTRIG(p->trig);

    int32_t res = OK;
    csound->LockMutex(reg->mutex);
    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = source_slot->array;

    if (source_arr->itype != CSN_REAL || source_arr->ndim != 1U) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Searchsorted requires a 1-D real array");
        goto done;
    }

    if (p->is_published) {
        bool is_same_source = is_same_array_version(&p->k_data.prev_source_version, &source_arr->version);
        bool is_same_wanted = p->k_data.prev_scalar_param == wanted_in;
        if (is_same_source && is_same_wanted) {
            *p->index = (MYFLT) p->k_data.prev_axis_u;
            goto done;
        }
    }

    size_t index = 0;
    binary_search(&index, NULL, 0, source_arr->data, wanted_in, source_arr->size, p->is_right_search_side);
    *p->index = (MYFLT) index;

    PUBLISH_ELEMENTWISE(&p->k_data, source_handle, source_slot->array, 0, NULL, NULL, wanted_in, 0.0);
    p->k_data.prev_axis_u = (uint32_t) index;
    p->is_published = true;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}
