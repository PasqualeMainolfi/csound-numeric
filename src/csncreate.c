/* Opcode implementations for the create family.
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
#include "arrays.h"

static int32_t create_csnarray_from_shape(CSOUND *csound, OPDS *h, const ARRAYDAT *source_shape, CSN_ARRAY **array, CSNREF *handle, ITEM_TYPE itype) {
    uint32_t ndim = 0;
    uint32_t shape[CSN_MAX_DIMS] = {0};

    int32_t res_shape = parse_shape_array(csound, source_shape, &ndim, shape);
    if (res_shape != OK) {
        return res_shape;
    }

    return create_csnarray_init(csound, h, ndim, shape, array, handle, itype);
}

int32_t create_empty_csnarray(CSOUND *csound, CSN_ARR_INIT *p) {
    CHECK_ITYPE(csound, *p->itype);
    ITEM_TYPE itype = CSN_ITYPE_FROM_ARG(*p->itype);
    uint32_t ndim = 0;
    uint32_t shape[CSN_MAX_DIMS] = {0};
    int32_t res = parse_shape_array(csound, p->shape, &ndim, shape);
    if (res != OK) return res;
    res = create_csnarray_init(csound, &p->h, ndim, shape, &p->array, p->handle, itype);
    if (res != OK) return res;
    reset_empty_csnarray(p->array, ndim, shape, itype);
    return OK;
}

static int32_t create_shape_csnarray_k_init(CSOUND *csound, CSN_ARR_INIT *p, CSN_K_SHAPE_INIT_MODE mode) {
    CHECK_ITYPE(csound, *p->itype);
    ITEM_TYPE itype = CSN_ITYPE_FROM_ARG(*p->itype);
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t ndim = 0;
    uint32_t shape[CSN_MAX_DIMS] = {0};
    int32_t res = parse_shape_array(csound, p->shape, &ndim, shape);
    if (res != OK) {
        return res;
    }

    res = create_csnarray_init(csound, &p->h, ndim, shape, &p->array, p->handle, itype);
    if (res != OK) {
        return res;
    }

    csound->LockMutex(reg->mutex);
    /* csnempty publishes the reserved shape with no elements; zeros and ones
       publish a filled array, so that reading the handle before the first
       triggered pass gives the same thing the i-rate form would. */
    if (mode == CSN_K_EMPTY) {
        reset_empty_csnarray(p->array, ndim, shape, itype);
    } else {
        fill_csnarray(p->array, mode == CSN_K_ONES ? 1.0 : 0.0);
    }
    csound->UnlockMutex(reg->mutex);

    SET_KDATA_WITH_ID_BEGIN(p, reg, shape, ndim, itype, p->handle->id);
    return OK;
}

int32_t create_empty_csnarray_k_init(CSOUND *csound, CSN_ARR_INIT *p) {
    return create_shape_csnarray_k_init(csound, p, CSN_K_EMPTY);
}

int32_t create_zeros_csnarray_k_init(CSOUND *csound, CSN_ARR_INIT *p) {
    return create_shape_csnarray_k_init(csound, p, CSN_K_ZEROS);
}

int32_t create_ones_csnarray_k_init(CSOUND *csound, CSN_ARR_INIT *p) {
    return create_shape_csnarray_k_init(csound, p, CSN_K_ONES);
}

static int32_t create_shape_csnarray_k(CSOUND *csound, CSN_ARR_INIT *p, CSN_K_SHAPE_INIT_MODE mode) {
    CHECK_REG_HANDLE(csound, &p->h, p->k_data.registry, p->k_data.owned_handle);

    /* itype is an i-argument, so it cannot change between passes and the init
       has already validated it. */
    ITEM_TYPE itype = CSN_ITYPE_FROM_ARG(*p->itype);

    uint32_t ndim = 0;
    uint32_t shape[CSN_MAX_DIMS] = {0};
    int32_t res = parse_shape_array_k(csound, &p->h, p->shape, &ndim, shape);
    if (res != OK) {
        return res;
    }


    const char *err = NULL;
    size_t requested_size = 0;
    if (get_array_size_from_shape(&requested_size, ndim, shape) != OK) {
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    /* csnempty reserves the requested shape but publishes no element yet. */
    size_t logical_size = mode == CSN_K_EMPTY ? 0 : requested_size;

    csound->LockMutex(p->k_data.registry->mutex);

    CSN_ARRAY *array = NULL;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &array, &p->k_data, NULL, ndim, shape, logical_size, itype, err);
    if (res != OK) {
        csound->UnlockMutex(p->k_data.registry->mutex);
        return res;
    }
    p->array = array;

    if (mode != CSN_K_EMPTY) {
        fill_csnarray(array, mode == CSN_K_ONES ? 1.0 : 0.0);
    }

    SET_KDATA_END(p, shape, ndim, itype);
    csound->UnlockMutex(p->k_data.registry->mutex);
    return OK;
}

int32_t create_empty_csnarray_k(CSOUND *csound, CSN_ARR_INIT *p) {
    return create_shape_csnarray_k(csound, p, CSN_K_EMPTY);
}

int32_t create_zeros_csnarray_k(CSOUND *csound, CSN_ARR_INIT *p) {
    return create_shape_csnarray_k(csound, p, CSN_K_ZEROS);
}

int32_t create_ones_csnarray_k(CSOUND *csound, CSN_ARR_INIT *p) {
    return create_shape_csnarray_k(csound, p, CSN_K_ONES);
}

int32_t create_zeros_csnarray(CSOUND *csound, CSN_ARR_INIT *p) {
    CHECK_ITYPE(csound, *p->itype);
    ITEM_TYPE itype = CSN_ITYPE_FROM_ARG(*p->itype);
    int32_t res_init = create_csnarray_from_shape(csound, &p->h, p->shape, &p->array, p->handle, itype);
    if (res_init != OK) {
        return res_init;
    }

    return OK;
}

int32_t create_ones_csnarray(CSOUND *csound, CSN_ARR_INIT *p) {
    CHECK_ITYPE(csound, *p->itype);
    ITEM_TYPE itype = CSN_ITYPE_FROM_ARG(*p->itype);
    int32_t res_init = create_csnarray_from_shape(csound, &p->h, p->shape, &p->array, p->handle, itype);
    if (res_init != OK) {
        return res_init;
    }

    fill_csnarray(p->array, 1.0);
    return OK;
}

int32_t create_like_csnarray_k_init(CSOUND *csound, CSN_ARR_INIT_LIKE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    csound->LockMutex(reg->mutex);

    int32_t res = OK;
    const char *err = NULL;

    uint32_t handle_from = p->handle_from->id;
    CSN_SLOT *slot = get_slot(reg, handle_from);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) handle_from);
        goto done;
    }

    CSN_ARRAY *source_arr = slot->array;
    uint32_t ndim = source_arr->ndim;

    uint32_t shape[CSN_MAX_DIMS] = {0};
    memcpy(shape, source_arr->shape, sizeof(uint32_t) * CSN_MAX_DIMS);

    uint32_t protect[1] = { handle_from };
    if (create_csnarray_locked(csound, reg, &p->h, ndim, shape, &p->array, p->handle, protect, 1U, &err, source_arr->itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;
    double fill_value = (double) *p->value;
    fill_csnarray(arr, fill_value);

    SET_KDATA_BEGIN(p, reg);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t create_like_csnarray_k(CSOUND *csound, CSN_ARR_INIT_LIKE *p) {
    CHECK_REG_HANDLE(csound, &p->h, p->k_data.registry, p->k_data.owned_handle);

    uint32_t handle_from = p->handle_from->id;
    int32_t res = OK;

    csound->LockMutex(p->k_data.registry->mutex);

    CSN_SLOT *slot = get_slot(p->k_data.registry, handle_from);
    if (slot == NULL) {
        csound->UnlockMutex(p->k_data.registry->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) handle_from);
    }

    CSN_ARRAY *source_arr = slot->array;
    uint32_t ndim = source_arr->ndim;
    ITEM_TYPE itype = source_arr->itype;
    if (itype != CSN_REAL && itype != CSN_COMPLEX) {
        csound->UnlockMutex(p->k_data.registry->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Source array has invalid internal type %u", itype);
    }

    uint32_t shape[CSN_MAX_DIMS] = {0};
    memcpy(shape, source_arr->shape, sizeof(uint32_t) * ndim);

    size_t requested_size = 0;
    if (get_array_size_from_shape(&requested_size, ndim, shape) != OK) {
        csound->UnlockMutex(p->k_data.registry->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Source array has an invalid shape");
    }

    const char *err = NULL;
    CSN_ARRAY *output_arr = NULL;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &output_arr, &p->k_data, NULL, ndim, shape, requested_size, itype, err);
    if (res != OK) goto done;
    p->array = output_arr;

    fill_csnarray(output_arr, (double) *p->value);

    SET_KDATA_END(p, shape, ndim, itype);

done:
    csound->UnlockMutex(p->k_data.registry->mutex);
    return res;
}

int32_t create_like_csnarray(CSOUND *csound, CSN_ARR_INIT_LIKE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    csound->LockMutex(reg->mutex);

    int32_t res = OK;
    const char *err = NULL;

    uint32_t handle_from = p->handle_from->id;
    CSN_SLOT *slot = get_slot(reg, handle_from);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) handle_from);
        goto done;
    }

    CSN_ARRAY *source_arr = slot->array;
    uint32_t ndim = source_arr->ndim;

    uint32_t shape[CSN_MAX_DIMS] = {0};
    memcpy(shape, source_arr->shape, sizeof(uint32_t) * CSN_MAX_DIMS);

    uint32_t protect[1] = { handle_from };
    if (create_csnarray_locked(csound, reg, &p->h, ndim, shape, &p->array, p->handle, protect, 1U, &err, source_arr->itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;
    double fill_value = (double) *p->value;
    fill_csnarray(arr, fill_value);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t create_random_csnarray_helper(CSOUND *csound, CSN_ARR_RND_INIT *p, bool is_int) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t ndim = 0;
    uint32_t shape[CSN_MAX_DIMS] = {0};

    int32_t res_shape = parse_shape_array(csound, p->shape, &ndim, shape);
    if (res_shape != OK) {
        return res_shape;
    }

    int32_t res = create_csnarray_init(csound, &p->h, ndim, shape, &p->array, p->handle, CSN_REAL);
    if (res != OK) {
        return res;
    }

    double min = (double) *p->min;
    double max = (double) *p->max;
    if (min == max || min > max || !IS_VALID_VALUE(min) || !IS_VALID_VALUE(max)) {
        return csound->InitError(csound, "[csnarray] Invalid random range");
    }

    if (is_int && (!IS_VALID_VALUE_INT32(min) || !IS_VALID_VALUE_INT32(max))) {
        return csound->InitError(csound, "[csnarray] Invalid random range");
    }

    /* reg->rng is registry-wide state that every draw mutates, so the fill has
       to run under the same lock as everything else: two threads drawing at
       once would otherwise interleave into the generator and could hand out
       the same stream twice. */
    csound->LockMutex(reg->mutex);

    CSN_ARRAY *arr = p->array;
    for (size_t i = 0; i < arr->size; i++) {
        if (is_int) {
            arr->data[i] = min + (double) pcg32_bounded_u32(&reg->rng, (uint32_t) (max - min));
        } else {
            arr->data[i] = min + pcg32_random(&reg->rng) * (max - min);
        }
    }

    csound->UnlockMutex(reg->mutex);

    return OK;
}

static int32_t create_random_csnarray_k_helper(CSOUND *csound, CSN_ARR_RND_INIT *p, bool is_int) {
    CSN_REGISTRY *reg = p->k_data.registry;
    uint32_t source_handle = p->k_data.owned_handle;
    CHECK_REG_HANDLE(csound, &p->h, reg, source_handle);

    CHECK_KTRIG(p->trig);

    int32_t res = OK;
    const char *err = NULL;

    uint32_t new_ndim = 0;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    res = parse_shape_array_k(csound, &p->h, p->shape, &new_ndim, new_shape);
    if (res != OK) return res;

    double min = (double) *p->min;
    double max = (double) *p->max;
    if (min == max || min > max || !IS_VALID_VALUE(min) || !IS_VALID_VALUE(max)) {
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid random range");
    }

    if (is_int && (!IS_VALID_VALUE_INT32(min) || !IS_VALID_VALUE_INT32(max))) {
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid random range");
    }

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
    }

    size_t req_size = 0;
    if (get_array_size_from_shape(&req_size, new_ndim, new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSN_ARRAY *arr = NULL;
    size_t logical_size = req_size;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, new_ndim, new_shape, logical_size, CSN_REAL, err);
    if (res != OK) goto done;
    p->array = arr;

    for (size_t i = 0; i < arr->size; i++) {
        if (is_int) {
            arr->data[i] = min + (double) pcg32_bounded_u32(&reg->rng, (uint32_t) (max - min));
        } else {
            arr->data[i] = min + pcg32_random(&reg->rng) * (max - min);
        }
    }

    SET_KDATA_END(p, new_shape, new_ndim, CSN_REAL);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t create_random_csnarray(CSOUND *csound, CSN_ARR_RND_INIT *p) {
    return create_random_csnarray_helper(csound, p, false);
}

int32_t create_randomint_csnarray(CSOUND *csound, CSN_ARR_RND_INIT *p) {
    return create_random_csnarray_helper(csound, p, true);
}

int32_t create_random_csnarray_k_init(CSOUND *csound, CSN_ARR_RND_INIT *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t ndim = 1U;
    uint32_t shape[CSN_MAX_DIMS] = {0};
    shape[1] = 1U;

    int32_t res = create_csnarray_init(csound, &p->h, ndim, shape, &p->array, p->handle, CSN_REAL);
    if (res != OK) return res;

    csound->LockMutex(reg->mutex);

    reset_empty_csnarray(p->array, ndim, shape, CSN_REAL);
    SET_KDATA_WITH_ID_BEGIN(p, reg, shape, ndim, CSN_REAL, p->handle->id);

    csound->UnlockMutex(reg->mutex);
    return OK;
}

int32_t create_random_csnarray_k(CSOUND *csound, CSN_ARR_RND_INIT *p) {
    return create_random_csnarray_k_helper(csound, p, false);
}
int32_t create_randomint_csnarray_k(CSOUND *csound, CSN_ARR_RND_INIT *p) {
    return create_random_csnarray_k_helper(csound, p, true);
}

int32_t create_full_csnarray(CSOUND *csound, CSN_FULL *p) {
    CHECK_ITYPE(csound, *p->itype);
    ITEM_TYPE itype = CSN_ITYPE_FROM_ARG(*p->itype);
    int32_t res_init = create_csnarray_from_shape(csound, &p->h, p->shape, &p->array, p->handle, itype);
    if (res_init != OK) {
        return res_init;
    }

    fill_csnarray(p->array, (double) *p->value);
    return OK;
}

int32_t create_full_csnarray_k_init(CSOUND *csound, CSN_FULL *p) {
    int32_t res = create_full_csnarray(csound, p);
    if (res != OK) {
        return res;
    };

    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    SET_KDATA_BEGIN(p, reg);
    return OK;
}

int32_t create_full_csnarray_k(CSOUND *csound, CSN_FULL *p) {
    CHECK_REG_HANDLE(csound, &p->h, p->k_data.registry, p->k_data.owned_handle);

    /* Validated by the init: itype is an i-argument. */
    ITEM_TYPE itype = CSN_ITYPE_FROM_ARG(*p->itype);

    uint32_t ndim = 0;
    uint32_t shape[CSN_MAX_DIMS] = {0};
    int32_t res = parse_shape_array_k(csound, &p->h, p->shape, &ndim, shape);
    if (res != OK) {
        return res;
    }

    size_t requested_size = 0;
    if (get_array_size_from_shape(&requested_size, ndim, shape) != OK) {
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    csound->LockMutex(p->k_data.registry->mutex);

    const char *err = NULL;
    CSN_ARRAY *array = NULL;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &array, &p->k_data, NULL, ndim, shape, requested_size, itype, err);
    if (res != OK) {
        csound->UnlockMutex(p->k_data.registry->mutex);
        return res;
    }
    p->array = array;

    fill_csnarray(array, (double) *p->value);

    SET_KDATA_END(p, shape, ndim, itype);

    csound->UnlockMutex(p->k_data.registry->mutex);
    return OK;
}

int32_t create_fullcomp_csnarray_k_init(CSOUND *csound, CSN_FULLCOMPLEX *p) {
    int32_t res = create_fullcomp_csnarray(csound, p);
    if (res != OK) {
        return res;
    };

    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    SET_KDATA_BEGIN(p, reg);
    return OK;
}

int32_t create_fullcomp_csnarray_k(CSOUND *csound, CSN_FULLCOMPLEX *p) {
    CHECK_REG_HANDLE(csound, &p->h, p->k_data.registry, p->k_data.owned_handle);

    uint32_t ndim = 0;
    uint32_t shape[CSN_MAX_DIMS] = {0};
    int32_t res = parse_shape_array_k(csound, &p->h, p->shape, &ndim, shape);
    if (res != OK) {
        return res;
    }

    size_t requested_size = 0;
    if (get_array_size_from_shape(&requested_size, ndim, shape) != OK) {
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    csound->LockMutex(p->k_data.registry->mutex);

    const char *err = NULL;
    CSN_ARRAY *array = NULL;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &array, &p->k_data, NULL, ndim,  shape, requested_size, CSN_COMPLEX, err);
    if (res != OK) {
        csound->UnlockMutex(p->k_data.registry->mutex);
        return res;
    }
    p->array = array;

    double re, im;
    complexdat_to_rect(p->value, &re, &im);
    fill_csnarray_complex(array, re, im);

    SET_KDATA_END(p, shape, ndim, CSN_COMPLEX);

    csound->UnlockMutex(p->k_data.registry->mutex);
    return OK;
}

int32_t create_fullcomp_csnarray(CSOUND *csound, CSN_FULLCOMPLEX *p) {
    int32_t res_init = create_csnarray_from_shape(csound, &p->h, p->shape, &p->array, p->handle, CSN_COMPLEX);
    if (res_init != OK) {
        return res_init;
    }

    double re, im;
    complexdat_to_rect(p->value, &re, &im);
    fill_csnarray_complex(p->array, re, im);
    return OK;
}

int32_t from_array_to_csnarray(CSOUND *csound, CSN_FROM_ARRAY *p) {
    if (p->source == NULL
        || p->source->data == NULL
        || p->source->sizes == NULL
        || p->source->dimensions <= 0
        || p->source->dimensions > CSN_MAX_DIMS) {
        return csound->InitError(csound, "[csnarray] Source must be an i-array with 1 to %d dimensions and allocated data", CSN_MAX_DIMS);
    }

    uint32_t ndim = (uint32_t) p->source->dimensions;
    uint32_t shape[CSN_MAX_DIMS] = {0};

    for (uint32_t i = 0; i < ndim; i++) {
        int32_t size = p->source->sizes[i];
        if (size <= 0) {
            return csound->InitError(csound, "[csnarray] Source extent %u must be >= 1", i);
        }
        shape[i] = (uint32_t) size;
    }

    size_t total_size = 0;
    if (get_array_size_from_shape(&total_size, ndim, shape) != OK) {
        return csound->InitError(csound, "[csnarray] Source shape is invalid or its element count exceeds the configured limit");
    }

    int32_t res_init = create_csnarray_init(csound, &p->h, ndim, shape, &p->array, p->handle, CSN_REAL);
    if (res_init != OK) {
        return res_init;
    }

    for (size_t i = 0; i < total_size; i++) {
        p->array->data[i] = (double) p->source->data[i];
    }

    return OK;
}

int32_t from_array_to_csnarray_k_init(CSOUND *csound, CSN_FROM_ARRAY *p) {
    int32_t res = from_array_to_csnarray(csound, p);
    if (res != OK) {
        return res;
    };

    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    SET_KDATA_BEGIN(p, reg);
    return OK;
}

int32_t from_array_to_csnarray_k(CSOUND *csound, CSN_FROM_ARRAY *p) {
    if (p->source == NULL
        || p->source->data == NULL
        || p->source->sizes == NULL
        || p->source->dimensions <= 0
        || p->source->dimensions > CSN_MAX_DIMS) {
        return csound->PerfError(csound, &p->h, "[csnarray] Source must be a k-array with 1 to %d dimensions and allocated data", CSN_MAX_DIMS);
    }

    CHECK_REG_HANDLE(csound, &p->h, p->k_data.registry, p->k_data.owned_handle);

    uint32_t ndim = (uint32_t) p->source->dimensions;
    uint32_t shape[CSN_MAX_DIMS] = {0};

    for (uint32_t i = 0; i < ndim; i++) {
        int32_t size = p->source->sizes[i];
        if (size <= 0) {
            return csound->PerfError(csound, &p->h, "[csnarray] Source extent %u must be >= 1", i);
        }
        shape[i] = (uint32_t) size;
    }

    size_t requested_size = 0;
    if (get_array_size_from_shape(&requested_size, ndim, shape) != OK) {
        return csound->PerfError(csound, &p->h, "[csnarray] Source shape is invalid or its element count exceeds the configured limit");
    }

    int32_t res = OK;
    csound->LockMutex(p->k_data.registry->mutex);

    const char *err = NULL;
    CSN_ARRAY *array = NULL;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &array, &p->k_data, NULL, ndim, shape, requested_size, CSN_REAL, err);
    if (res != OK) {
        csound->UnlockMutex(p->k_data.registry->mutex);
        return res;
    }
    p->array = array;

    for (size_t i = 0; i < requested_size; i++) {
        array->data[i] = (double) p->source->data[i];
    }

    SET_KDATA_END(p, shape, ndim, CSN_REAL);

    csound->UnlockMutex(p->k_data.registry->mutex);
    return OK;
}

/* ARRAYDAT complex is a vector of COMPLEXDAT */
int32_t from_complexarray_to_csnarray(CSOUND *csound, CSN_FROM_ARRAY *p) {
    if (p->source == NULL
        || p->source->data == NULL
        || p->source->sizes == NULL
        || p->source->dimensions <= 0
        || p->source->dimensions > CSN_MAX_DIMS) {
        return csound->InitError(csound, "[csnarray] Source must be a complex array with 1 to %d dimensions and allocated data", CSN_MAX_DIMS);
    }

    uint32_t ndim = (uint32_t) p->source->dimensions;
    uint32_t shape[CSN_MAX_DIMS] = {0};

    for (uint32_t i = 0; i < ndim; i++) {
        int32_t size = p->source->sizes[i];
        if (size <= 0) {
            return csound->InitError(csound, "[csnarray] Source extent %u must be >= 1", i);
        }
        shape[i] = (uint32_t) size;
    }

    size_t total_size = 0;
    if (get_array_size_from_shape(&total_size, ndim, shape) != OK) {
        return csound->InitError(csound, "[csnarray] Source shape is invalid or its element count exceeds the configured limit");
    }

    int32_t res_init = create_csnarray_init(csound, &p->h, ndim, shape, &p->array, p->handle, CSN_COMPLEX);
    if (res_init != OK) {
        return res_init;
    }

    const COMPLEXDAT *src = (const COMPLEXDAT *) p->source->data;
    for (size_t i = 0; i < total_size; i++) {
        double re, im;
        complexdat_to_rect(&src[i], &re, &im);
        p->array->data[i * 2] = re;
        p->array->data[i * 2 + 1] = im;
    }

    return OK;
}

int32_t from_complexarray_to_csnarray_k_init(CSOUND *csound, CSN_FROM_ARRAY *p) {
    int32_t res = from_complexarray_to_csnarray(csound, p);
    if (res != OK) {
        return res;
    };

    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    SET_KDATA_BEGIN(p, reg);
    return OK;
}

int32_t from_complexarray_to_csnarray_k(CSOUND *csound, CSN_FROM_ARRAY *p) {
    if (p->source == NULL
        || p->source->data == NULL
        || p->source->sizes == NULL
        || p->source->dimensions <= 0
        || p->source->dimensions > CSN_MAX_DIMS) {
        return csound->PerfError(csound, &p->h, "[csnarray] Source must be a complex array with 1 to %d dimensions and allocated data", CSN_MAX_DIMS);
    }

    CHECK_REG_HANDLE(csound, &p->h, p->k_data.registry, p->k_data.owned_handle);

    uint32_t ndim = (uint32_t) p->source->dimensions;
    uint32_t shape[CSN_MAX_DIMS] = {0};

    for (uint32_t i = 0; i < ndim; i++) {
        int32_t size = p->source->sizes[i];
        if (size <= 0) {
            return csound->PerfError(csound, &p->h, "[csnarray] Source extent %u must be >= 1", i);
        }
        shape[i] = (uint32_t) size;
    }

    size_t requested_size = 0;
    if (get_array_size_from_shape(&requested_size, ndim, shape) != OK) {
        return csound->PerfError(csound, &p->h, "[csnarray] Source shape is invalid or its element count exceeds the configured limit");
    }

    int32_t res = OK;
    csound->LockMutex(p->k_data.registry->mutex);

    const char *err = NULL;
    CSN_ARRAY *array = NULL;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &array, &p->k_data, NULL, ndim, shape, requested_size, CSN_COMPLEX, err);
    if (res != OK) {
        csound->UnlockMutex(p->k_data.registry->mutex);
        return res;
    }
    p->array = array;

    const COMPLEXDAT *src = (const COMPLEXDAT *) p->source->data;
    for (size_t i = 0; i < requested_size; i++) {
        double re, im;
        complexdat_to_rect(&src[i], &re, &im);
        array->data[i * 2] = re;
        array->data[i * 2 + 1] = im;
    }

    SET_KDATA_END(p, shape, ndim, CSN_COMPLEX);

    csound->UnlockMutex(p->k_data.registry->mutex);
    return OK;
}

/* A k-rate Csound array output is filled on the audio thread too. Csound owns
   it, so init reserves room for everything the source can hold without
   reallocating, and a marked source that needs more is refused instead of
   letting tabinit allocate. Csound's own copy-on-write detach of a shared
   output array stays outside this guarantee. */
static bool krate_array_output_fits(const ARRAYDAT *out, size_t items) {
    return out->data != NULL && out->arrayMemberSize > 0
        && out->allocated / (size_t) out->arrayMemberSize >= items;
}

static int32_t reserve_krate_array_output(CSOUND *csound, OPDS *h, ARRAYDAT *out, size_t items) {
    if (h->perf == NULL || items == 0) return OK;
    if (items > (size_t) INT32_MAX) return NOTOK;
    int32_t logical_size = out->dimensions == 1 && out->sizes != NULL ? out->sizes[0] : 0;
    tabinit(csound, out, (int32_t) items, h->insdshead);
    if (out->dimensions == 1 && out->sizes != NULL) out->sizes[0] = logical_size;
    return krate_array_output_fits(out, items) ? OK : NOTOK;
}

int32_t from_csnarray_to_array(CSOUND *csound, CSN_TO_ARRAY *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, p->source_handle->id);
    if (slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) p->source_handle->id);
    }

    CSN_ARRAY *src = slot->array;
    uint32_t ndim = src->ndim;

    if (src->size == 0) {
        csound->UnlockMutex(reg->mutex);
        return csound->InitError(csound, "[csnarray] Cannot convert an empty CsnArr to ARRAYDAT");
    }

    if (src->itype != CSN_REAL) {
        csound->UnlockMutex(reg->mutex);
        return csound->InitError(csound, "[csnarray] Handle holds a complex array; declare the output as :Complex;[]");
    }

    /* dimensions arrives pre-set from the declaration, with sizes[] already
       allocated to match. A 0 means the variable carries no rank yet, which
       tabinit would resolve to 1-D. */
    int32_t declared = p->array->dimensions > 0 ? p->array->dimensions : 1;
    if ((uint32_t) declared != ndim) {
        csound->UnlockMutex(reg->mutex);
        return csound->InitError(csound, "[csnarray] Handle holds a %u-D array but the output is declared %d-D; declare it with %u bracket pairs", ndim, declared, ndim);
    }

    size_t total_size = src->size;
    if (total_size > (size_t) INT32_MAX) {
        csound->UnlockMutex(reg->mutex);
        return csound->InitError(csound, "[csnarray] Array holds %zu elements, too many for an i-array output (limit %d)", total_size, INT32_MAX);
    }

    tabinit(csound, p->array, (int32_t) total_size, p->h.insdshead);
    if (p->array->data == NULL || p->array->sizes == NULL
        || reserve_krate_array_output(csound, &p->h, p->array, src->capacity) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->InitError(csound, "[csnarray] Could not allocate the %u-D output i-array of %zu elements", ndim, total_size);
    }

    p->array->dimensions = (int32_t) ndim;
    for (uint32_t i = 0; i < ndim; i++) {
        p->array->sizes[i] = (int32_t) src->shape[i];
    }

    for (size_t i = 0; i < total_size; i++) {
        p->array->data[i] = (MYFLT) src->data[i];
    }

    csound->UnlockMutex(reg->mutex);

    return OK;
}

int32_t from_csnarray_to_array_k(CSOUND *csound, CSN_TO_ARRAY *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, &p->h, reg);

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, p->source_handle->id);
    if (slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) p->source_handle->id);
    }

    CSN_ARRAY *src = slot->array;
    uint32_t ndim = src->ndim;

    if (src->itype != CSN_REAL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Handle holds a complex array; declare the output as :Complex;[]");
    }

    if (src->size == 0) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Cannot convert an empty CsnArr to ARRAYDAT");
    }

    int32_t declared = p->array->dimensions > 0 ? p->array->dimensions : 1;
    if ((uint32_t) declared != ndim) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Handle holds a %u-D array but the output is declared %d-D; declare it with %u bracket pairs", ndim, declared, ndim);
    }

    size_t total_size = src->size;
    if (total_size > (size_t) INT32_MAX) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Array holds %zu elements, too many for an k-array output (limit %d)", total_size, INT32_MAX);
    }

    if (slot->rt_locked && !krate_array_output_fits(p->array, total_size)) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Array %u is on a real-time path and has grown to %zu elements, past what '%s' reserved at init; clear the mark with csnrtunlock, or pass irt=0 at the audio source it descends from", (uint32_t) p->source_handle->id, total_size, get_out_name(&p->h));
    }

    // use int32_t as return value on tabinit in Csound recent version
    tabinit(csound, p->array, (int32_t) total_size, p->h.insdshead);
    if (p->array->data == NULL || p->array->sizes == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Could not allocate the %u-D output k-array of %zu elements", ndim, total_size);
    }

    p->array->dimensions = (int32_t) ndim;
    for (uint32_t i = 0; i < ndim; i++) {
        p->array->sizes[i] = (int32_t) src->shape[i];
    }

    for (size_t i = 0; i < total_size; i++) {
        p->array->data[i] = (MYFLT) src->data[i];
    }

    csound->UnlockMutex(reg->mutex);

    return OK;
}

int32_t from_csnarray_to_complexarray(CSOUND *csound, CSN_TO_ARRAY *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, p->source_handle->id);
    if (slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) p->source_handle->id);
    }

    CSN_ARRAY *src = slot->array;
    uint32_t ndim = src->ndim;

    if (src->size == 0) {
        csound->UnlockMutex(reg->mutex);
        return csound->InitError(csound, "[csnarray] Cannot convert an empty CsnArr to ARRAYDAT");
    }

    if (src->itype != CSN_COMPLEX) {
        csound->UnlockMutex(reg->mutex);
        return csound->InitError(csound, "[csnarray] Handle holds a real array; use csntoarray for it");
    }

    int32_t declared = p->array->dimensions > 0 ? p->array->dimensions : 1;
    if ((uint32_t) declared != ndim) {
        csound->UnlockMutex(reg->mutex);
        return csound->InitError(csound, "[csnarray] Handle holds a %u-D array but the output is declared %d-D; declare it with %u bracket pairs", ndim, declared, ndim);
    }

    size_t total_size = src->size;
    if (total_size > (size_t) INT32_MAX) {
        csound->UnlockMutex(reg->mutex);
        return csound->InitError(csound, "[csnarray] Array holds %zu elements, too many for a complex-array output (limit %d)", total_size, INT32_MAX);
    }

    tabinit(csound, p->array, (int32_t) total_size, p->h.insdshead);
    if (p->array->data == NULL || p->array->sizes == NULL
        || reserve_krate_array_output(csound, &p->h, p->array, src->capacity) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->InitError(csound, "[csnarray] Could not allocate the %u-D output complex array of %zu elements", ndim, total_size);
    }

    p->array->dimensions = (int32_t) ndim;
    for (uint32_t i = 0; i < ndim; i++) {
        p->array->sizes[i] = (int32_t) src->shape[i];
    }

    COMPLEXDAT *dst = (COMPLEXDAT *) p->array->data;
    for (size_t i = 0; i < total_size; i++) {
        dst[i].real = (MYFLT) src->data[i * 2];
        dst[i].imag = (MYFLT) src->data[i * 2 + 1];
        dst[i].isPolar = 0;
    }

    csound->UnlockMutex(reg->mutex);

    return OK;
}

int32_t from_csnarray_to_complexarray_k(CSOUND *csound, CSN_TO_ARRAY *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, &p->h, reg);

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, p->source_handle->id);
    if (slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) p->source_handle->id);
    }

    CSN_ARRAY *src = slot->array;
    uint32_t ndim = src->ndim;

    if (src->size == 0) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Cannot convert an empty CsnArr to ARRAYDAT");
    }

    if (src->itype != CSN_COMPLEX) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Handle holds a real array; use csntoarray for it");
    }

    int32_t declared = p->array->dimensions > 0 ? p->array->dimensions : 1;
    if ((uint32_t) declared != ndim) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Handle holds a %u-D array but the output is declared %d-D; declare it with %u bracket pairs", ndim, declared, ndim);
    }

    size_t total_size = src->size;
    if (total_size > (size_t) INT32_MAX) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Array holds %zu elements, too many for a complex-array output (limit %d)", total_size, INT32_MAX);
    }

    if (slot->rt_locked && !krate_array_output_fits(p->array, total_size)) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Array %u is on a real-time path and has grown to %zu elements, past what '%s' reserved at init; clear the mark with csnrtunlock, or pass irt=0 at the audio source it descends from", (uint32_t) p->source_handle->id, total_size, get_out_name(&p->h));
    }

    tabinit(csound, p->array, (int32_t) total_size, p->h.insdshead);
    if (p->array->data == NULL || p->array->sizes == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Could not allocate the %u-D output k-array of %zu elements", ndim, total_size);
    }

    p->array->dimensions = (int32_t) ndim;
    for (uint32_t i = 0; i < ndim; i++) {
        p->array->sizes[i] = (int32_t) src->shape[i];
    }

    COMPLEXDAT *dst = (COMPLEXDAT *) p->array->data;
    for (size_t i = 0; i < total_size; i++) {
        dst[i].real = (MYFLT) src->data[i * 2];
        dst[i].imag = (MYFLT) src->data[i * 2 + 1];
        dst[i].isPolar = 0;
    }

    csound->UnlockMutex(reg->mutex);

    return OK;
}

int32_t free_csnarray(CSOUND *csound, CSN_FREE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, p->handle->id);
    if (slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) p->handle->id);
    }

    release_slot(csound, reg, slot);

    csound->UnlockMutex(reg->mutex);

    return OK;
}

// get dims
int32_t csnarray_dims(CSOUND *csound, CSN_SIZE_DIMS *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, p->handle->id);
    if (slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) p->handle->id);
    }

    MYFLT dims = (MYFLT) slot->array->ndim;

    csound->UnlockMutex(reg->mutex);

    *p->value = dims;

    return OK;
}

int32_t csnarray_dims_k(CSOUND *csound, CSN_SIZE_DIMS *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, &p->h, reg);

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, p->handle->id);
    if (slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) p->handle->id);
    }

    MYFLT dims = (MYFLT) slot->array->ndim;

    csound->UnlockMutex(reg->mutex);

    *p->value = dims;

    return OK;
}

// get total size (considering all dims)
int32_t csnarray_size(CSOUND *csound, CSN_SIZE_DIMS *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, p->handle->id);
    if (slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) p->handle->id);
    }

    MYFLT total_size = (MYFLT) slot->array->size;

    csound->UnlockMutex(reg->mutex);

    *p->value = total_size;

    return OK;
}

int32_t csnarray_size_k(CSOUND *csound, CSN_SIZE_DIMS *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, &p->h, reg);

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, p->handle->id);
    if (slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) p->handle->id);
    }

    MYFLT total_size = (MYFLT) slot->array->size;

    csound->UnlockMutex(reg->mutex);

    *p->value = total_size;

    return OK;
}

/* Emptiness is derived from the size rather than tracked, so it cannot
   disagree with what the array actually holds. */
int32_t csnarray_is_empty(CSOUND *csound, CSN_SIZE_DIMS *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, p->handle->id);
    if (slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) p->handle->id);
    }

    MYFLT empty = slot->array->size == 0 ? FL(1.0) : FL(0.0);

    csound->UnlockMutex(reg->mutex);

    *p->value = empty;

    return OK;
}

int32_t csnarray_is_empty_k(CSOUND *csound, CSN_SIZE_DIMS *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, &p->h, reg);

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, p->handle->id);
    if (slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) p->handle->id);
    }

    MYFLT empty = slot->array->size == 0 ? FL(1.0) : FL(0.0);

    csound->UnlockMutex(reg->mutex);

    *p->value = empty;

    return OK;
}

// get shape -> shape[size dim 1, size dim 2, size dim 3, ...]
int32_t csnarray_shape(CSOUND *csound, CSN_SHAPE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, p->handle->id);
    if (slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) p->handle->id);
    }

    uint32_t dims = (MYFLT) slot->array->ndim;
    uint32_t *shape = slot->array->shape;

    /* The k form reserves every dimension an array can have, so a source that
       changes rank at perf time never makes tabinit allocate. */
    tabinit(csound, p->shape, (int32_t) dims, p->h.insdshead);
    if (reserve_krate_array_output(csound, &p->h, p->shape, CSN_MAX_DIMS) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->InitError(csound, "[csnarray] Could not allocate the shape output of %u elements", (uint32_t) CSN_MAX_DIMS);
    }
    for (uint32_t i = 0; i < dims; i++) {
        p->shape->data[i] = (MYFLT) shape[i];
    }

    csound->UnlockMutex(reg->mutex);

    return OK;
}

int32_t csnarray_shape_k(CSOUND *csound, CSN_SHAPE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, &p->h, reg);

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, p->handle->id);
    if (slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) p->handle->id);
    }

    uint32_t dims = (MYFLT) slot->array->ndim;
    uint32_t *shape = slot->array->shape;

    tabinit(csound, p->shape, (int32_t) dims, p->h.insdshead);
    for (uint32_t i = 0; i < dims; i++) {
        p->shape->data[i] = (MYFLT) shape[i];
    }

    csound->UnlockMutex(reg->mutex);

    return OK;
}

static int32_t spaced_space_body(CSOUND *csound, OPDS *perf_h, double step_num, const MYFLT *in_start, const MYFLT *in_stop, const MYFLT *in_base, double *base, double *ratio, uint32_t *usize, CSN_SPACED_SPACE_MODE mode) {
    double start = (double) *in_start;
    double stop = (double) *in_stop;
    switch (mode) {
        case CSN_ARANGE:
            if ((stop > start && step_num < 0) || (stop < start && step_num > 0)) {
                return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Step %g has the wrong sign to go from start %g to stop %g", step_num, start, stop);
            }
            int32_t size = (int32_t) ceil((stop - start) / step_num);
            if (size == 0) {
                return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Start %g, stop %g and step %g produce an empty array", start, stop, step_num);
            }
            *usize = (uint32_t) size;
            break;
        case CSN_LINSPACE:
        case CSN_LOGSPACE:
        case CSN_GEOMSPACE:
            if (mode == CSN_LOGSPACE) {
                if (in_base == NULL) {
                    return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] The base argument is missing");
                }
                *base = (double) *in_base;
                if (*base <= 0) {
                    return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] The base argument must be > 0, got %g", *base);
                }
            }
            if (mode == CSN_GEOMSPACE) {
                *ratio = pow(stop / start, 1.0 / (step_num - 1));
            }
            *usize = (uint32_t) step_num;
            break;
    }

    return OK;
}

/* arange_step stays a double all the way down: a fractional step would floor to
   zero and a negative one would wrap if this took an unsigned integer, while
   the element count above is already computed in double. */
static void spaced_space_assign_value(CSN_ARRAY *array, uint32_t usize, double arange_step, double start, double stop, double base, double ratio, CSN_SPACED_SPACE_MODE mode) {
    switch (mode) {
        case CSN_ARANGE:
            for (uint32_t i = 0; i < usize; i++) {
                array->data[i] =  start + ((double) i * arange_step);
            }
            break;
        case CSN_LINSPACE:
            if (usize == 1) {
                array->data[0] = start;
            } else {
                double step = (stop - start) / (double) (usize - 1);
                for (uint32_t i = 0; i < usize; i++) {
                    array->data[i] =  (double) start + (i * step);
                }
                array->data[usize - 1] = stop;
            }
            break;
        case CSN_LOGSPACE:
            if (usize == 1) {
                array->data[0] = pow(base, start);
            } else {
                double step = (stop - start) / (double) (usize - 1);
                for (int32_t i = 0; i < usize; i++) {
                    double exponent = start + (double) i * step;
                    array->data[i] =  pow(base, exponent);
                }
                array->data[usize - 1] = pow(base, stop);
            }
            break;
        case CSN_GEOMSPACE:
            if (usize == 1) {
                array->data[0] = start;
            } else {
                for (int32_t i = 0; i < usize; i++) {
                    array->data[i] =  start * pow(ratio, i);
                }
                array->data[usize - 1] = stop;
            }
    }
}

static int32_t spaced_space_helper(CSOUND *csound, CSN_SPACED_SPACE *p, CSN_SPACED_SPACE_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    double num = (double) *p->step_num;
    if (mode == CSN_ARANGE) {
        if (num == 0.0) return csound->InitError(csound, "[csnarray] The step argument must not be zero");
    } else {
        if (num <= 0.0) return csound->InitError(csound, "[csnarray] The num argument must be > 0, got %d", (int32_t) num);
    }

    double arange_step = num;
    double base = 1.0;
    const MYFLT *in_base = mode == CSN_LOGSPACE ? p->arg_a : NULL;
    double ratio = 0.0;
    uint32_t usize = 0;
    /* Init pass, and the only caller that reaches spaced_space_body without
       the registry lock: the report has to go through InitError, both because
       that is the phase we are in and because the locked report would release
       a mutex this path never took. */
    int32_t res = spaced_space_body(csound, NULL, arange_step, p->start, p->stop, in_base, &base, &ratio, &usize, mode);
    if (res != OK) return res;

    int32_t res_init = create_csnarray_init(csound, &p->h, 1U, &usize, &p->array, p->handle, CSN_REAL);
    if (res_init != OK) return res_init;

    csound->LockMutex(reg->mutex);
    double start = (double) *p->start;
    double stop = (double) *p->stop;
    spaced_space_assign_value(p->array, usize, arange_step, start, stop, base, ratio, mode);
    csound->UnlockMutex(reg->mutex);
    return OK;
}

static int32_t spaced_space_k_init_helper(CSOUND *csound, CSN_SPACED_SPACE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    uint32_t shape[CSN_MAX_DIMS] = {0};
    shape[0] = 1;
    res = create_csnarray_init(csound, &p->h, 1U, shape, &p->array, p->handle, CSN_REAL);
    if (res != OK) {
        return res;
    }

    csound->LockMutex(reg->mutex);
    reset_empty_csnarray(p->array, 1U, shape, CSN_REAL);
    csound->UnlockMutex(reg->mutex);

    SET_KDATA_WITH_ID_BEGIN(p, reg, shape, 1U, CSN_REAL, p->handle->id);
    return OK;
}

static int32_t spaced_space_k_helper(CSOUND *csound, CSN_SPACED_SPACE *p, CSN_SPACED_SPACE_MODE mode) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REG_HANDLE(csound, &p->h, reg, p->k_data.owned_handle);

    const MYFLT *trig = mode == CSN_LOGSPACE ? p->arg_b : p->arg_a;
    CHECK_KTRIG(trig);

    double num = (double) *p->step_num;
    if (mode == CSN_ARANGE) {
        if (num == 0.0) return csound->PerfError(csound, &p->h, "[csnarray] The step argument must not be zero");
    } else {
        if (num <= 0.0) return csound->PerfError(csound, &p->h, "[csnarray] The num argument must be > 0, got %d", (int32_t) num);
    }

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    double arange_step = num;
    double base = 1.0;
    const MYFLT *in_base = mode == CSN_LOGSPACE ? p->arg_a : NULL;
    double ratio = 0.0;
    uint32_t usize = 0;
    res = spaced_space_body(csound, &p->h, arange_step, p->start, p->stop, in_base, &base, &ratio, &usize, mode);
    if (res != OK) goto done;

    uint32_t shape[CSN_MAX_DIMS] = {0};
    shape[0] = usize;

    size_t req_size = 0;
    if (get_array_size_from_shape(&req_size, 1U, shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSN_ARRAY *arr = NULL;
    size_t logical_size = usize == 0 ? 0 : req_size;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, 1U, shape, logical_size, CSN_REAL, err);
    if (res != OK) goto done;
    p->array = arr;

    double start = (double) *p->start;
    double stop = (double) *p->stop;
    spaced_space_assign_value(p->array, usize, arange_step, start, stop, base, ratio, mode);
    SET_KDATA_NO_ID_END(p, shape, 1U, CSN_REAL);

done:
    csound->UnlockMutex(reg->mutex);
    return OK;
}

int32_t csnarray_spaced_space_k_init(CSOUND *csound, CSN_SPACED_SPACE *p) {
    return spaced_space_k_init_helper(csound, p);
}

int32_t csnarray_arange(CSOUND *csound, CSN_SPACED_SPACE *p) {
    return spaced_space_helper(csound, p, CSN_ARANGE);
}

int32_t csnarray_arange_k(CSOUND *csound, CSN_SPACED_SPACE *p) {
    return spaced_space_k_helper(csound, p, CSN_ARANGE);
}

int32_t csnarray_linspace(CSOUND *csound, CSN_SPACED_SPACE *p) {
    return spaced_space_helper(csound, p, CSN_LINSPACE);
}

int32_t csnarray_linspace_k(CSOUND *csound, CSN_SPACED_SPACE *p) {
    return spaced_space_k_helper(csound, p, CSN_LINSPACE);
}

int32_t csnarray_logspace(CSOUND *csound, CSN_SPACED_SPACE *p) {
    return spaced_space_helper(csound, p, CSN_LOGSPACE);
}

int32_t csnarray_logspace_k(CSOUND *csound, CSN_SPACED_SPACE *p) {
    return spaced_space_k_helper(csound, p, CSN_LOGSPACE);
}

int32_t csnarray_geomspace(CSOUND *csound, CSN_SPACED_SPACE *p) {
    return spaced_space_helper(csound, p, CSN_GEOMSPACE);
}

int32_t csnarray_geomspace_k(CSOUND *csound, CSN_SPACED_SPACE *p) {
    return spaced_space_k_helper(csound, p, CSN_GEOMSPACE);
}
