/* Opcode implementations for the shape family.
   The public opcode inventory remains centralized in csnum.c. */
#include "csnum_internal.h"
#include "csnregistry.h"
#include <float.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// return matrix n x n
static int32_t csnarray_identity_helper(CSOUND *csound, CSN_IDENTITY *p, bool is_ktime) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    CHECK_ITYPE(csound, *p->itype);
    ITEM_TYPE itype = CSN_ITYPE_FROM_ARG(*p->itype);
    int32_t num = (int32_t) *p->num;
    if (!is_ktime) {
        if (num <= 0) {
            return csound->InitError(csound, "[csnarray] The num argument must be > 0, got %d", num);
        }
    } else {
        if (num < 0) {
            return csound->InitError(csound, "[csnarray] The num argument must be > 0, got %d", num);
        }
    }

    uint32_t shape[CSN_MAX_DIMS] = {0};
    shape[0] = num;
    shape[1] = num;

    int32_t res_init = create_csnarray_init(csound, &p->h, 2U, shape, &p->array, p->handle, itype);
    if (res_init != OK) {
        return res_init;
    }

    for (int32_t i = 0; i < num; i++) {
        size_t item = (size_t) i * (size_t) num + (size_t) i;
        p->array->data[item * itype] = 1.0;
    }

    p->k_data.prev_ndim = 2U;
    memcpy(p->k_data.prev_shape, p->array->shape, sizeof(uint32_t) * CSN_MAX_DIMS);
    p->k_data.prev_itype = itype;
    p->k_data.owned_handle = p->handle->id;
    p->k_data.registry = reg;

    return OK;
}

int32_t csnarray_identity(CSOUND *csound, CSN_IDENTITY *p) {
    return csnarray_identity_helper(csound, p, false);
}

int32_t csnarray_identity_k_init(CSOUND *csound, CSN_IDENTITY *p) {
    return csnarray_identity_helper(csound, p, true);
}

int32_t csnarray_identity_k(CSOUND *csound, CSN_IDENTITY *p) {
    /* Validated by the init: itype is an i-argument. */
    ITEM_TYPE itype = CSN_ITYPE_FROM_ARG(*p->itype);
    if (!IS_VALID_VALUE((double) *p->num)) {
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid matrix size");
    }

    int32_t num = (int32_t) *p->num;
    if (num <= 0) {
        return csound->PerfError(csound, &p->h, "[csnarray] The num argument must be > 0, got %d", num);
    }

    CHECK_REG_HANDLE(csound, &p->h, p->k_data.registry, p->k_data.owned_handle);

    uint32_t ndim = 2U;
    uint32_t shape[CSN_MAX_DIMS] = {0};
    shape[0] = num;
    shape[1] = num;

    size_t requested_size = 0;
    if (get_array_size_from_shape(&requested_size, ndim, shape) != OK) {
        return csound->PerfError(csound, &p->h, "[csnarray] Shape is invalid or its element count exceeds the configured limit");
    }

    int32_t res = OK;
    csound->LockMutex(p->k_data.registry->mutex);

    const char *err = NULL;
    CSN_ARRAY *array = NULL;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &array, &p->k_data, NULL, ndim, shape, requested_size, itype, err);
    if (res != OK) goto done;
    p->array = array;

    fill_csnarray(array, 0.0);
    for (int32_t i = 0; i < num; i++) {
        size_t item = (size_t) i * (size_t) num + (size_t) i;
        array->data[item * itype] = 1.0;
    }

    SET_KDATA_END(p, shape, ndim, itype);

done:
    csound->UnlockMutex(p->k_data.registry->mutex);
    return res;
}


int32_t csnarray_reshape(CSOUND *csound, CSN_RESHAPE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;

    uint32_t ndim = 0;
    uint32_t shape[CSN_MAX_DIMS] = {0};

    int32_t res_shape = parse_shape_array(csound, p->new_shape, &ndim, shape);
    if (res_shape != OK) {
        return res_shape;
    }

    size_t new_size = 0;
    if (get_array_size_from_shape(&new_size, ndim, shape) != OK) {
        return csound->InitError(csound, "[csnarray] Shape is invalid or its element count exceeds the configured limit");
    }

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
        goto done;
    }

    CSN_ARRAY *arr = slot->array;

    /* Validated before allocating, so a rejected reshape does not publish a
       handle to a destination nobody asked for. */
    if (new_size != arr->size) {
        res = csound->InitError(csound, "[csnarray] Reshape size mismatch: source has %zu elements, new shape requires %zu", arr->size, new_size);
        goto done;
    }

    if (create_csnarray_locked(csound, reg, &p->h, ndim, shape, &p->array, p->handle, &source_handle, 1U, &err, arr->itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    memcpy(p->array->data, arr->data, sizeof(double) * arr->size * arr->itype);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

/* A computed k-shape normally still contains zeros during the init pass.
   Publish a valid output by copying the source's current layout; the first
   performance pass applies the requested reshape. */
int32_t csnarray_reshape_k_init(CSOUND *csound, CSN_RESHAPE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;
    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
        goto done;
    }

    CSN_ARRAY *source = source_slot->array;
    if (create_csnarray_locked(csound, reg, &p->h, source->ndim, source->shape, &p->array, p->handle, &source_handle, 1U, &err, source->itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    p->array->size = source->size;
    memcpy(p->array->data, source->data, sizeof(double) * source->size * source->itype);
    SET_KDATA_BEGIN(p, reg);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_reshape_k(CSOUND *csound, CSN_RESHAPE *p) {
    CHECK_REG_HANDLE(csound, &p->h, p->k_data.registry, p->k_data.owned_handle);

    uint32_t ndim = 0;
    uint32_t shape[CSN_MAX_DIMS] = {0};

    int32_t res_shape = parse_shape_array_k(csound, &p->h, p->new_shape, &ndim, shape);
    if (res_shape != OK) {
        return res_shape;
    }

    size_t requested_size = 0;
    if (get_array_size_from_shape(&requested_size, ndim, shape) != OK) {
        return csound->PerfError(csound, &p->h, "[csnarray] Shape is invalid or its element count exceeds the configured limit");
    }

    int32_t res = OK;
    csound->LockMutex(p->k_data.registry->mutex);

    uint32_t source_handle = p->source_handle->id;
    CSN_SLOT *source_slot = get_slot(p->k_data.registry, source_handle);
    if (source_slot == NULL) {
        csound->UnlockMutex(p->k_data.registry->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown source array handle %u: no array with this id is registered", source_handle);
    }

    CSN_SLOT *output_slot = get_slot(p->k_data.registry, p->k_data.owned_handle);
    if (output_slot == NULL) {
        csound->UnlockMutex(p->k_data.registry->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] k-rate output slot is no longer active");
    }

    CSN_ARRAY *source = source_slot->array;
    CSN_ARRAY *output = output_slot->array;
    ITEM_TYPE itype = source->itype;

    if (requested_size != source->size) {
        csound->UnlockMutex(p->k_data.registry->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Reshape size mismatch: source has %zu elements, new shape requires %zu", source->size, requested_size);
    }

    /* A shape-only change never needs new storage: reshape preserves the
       element count. Allocate only if the source type changed or the output
       buffer itself is unusable. This also preserves data when source and
       output are the same slot. */
    if (output->data == NULL || output->itype != itype || output->capacity < requested_size) {
        const char *err = NULL;
        res = update_slot_array_locked(csound, p->k_data.registry, p->k_data.owned_handle, ndim, shape, itype, &p->array, &err);
        if (res != OK) {
            csound->UnlockMutex(p->k_data.registry->mutex);
            return csound->PerfError(csound, &p->h, "[csnarray] Could not update k-rate output slot: %s", err != NULL ? err : "unknown error");
        }
        output = p->array;
    }

    set_csnarray_layout(output, ndim, shape, requested_size, itype);
    p->array = output;
    if (output != source) {
        memcpy(output->data, source->data, sizeof(double) * requested_size * itype);
        /* This opcode reaches its output slot without NEED_TO_UPDATE_SLOT, so
           the data counter is its own responsibility: the copy above republishes
           the source's current values into a slot other opcodes read. */
        update_array_data_version(&output->version);
    }

    SET_KDATA_END(p, shape, ndim, itype);

    csound->UnlockMutex(p->k_data.registry->mutex);
    return res;
}

int32_t csnarray_reshape_in(CSOUND *csound, CSN_RESHAPE_IN *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;

    uint32_t ndim = 0;
    uint32_t shape[CSN_MAX_DIMS] = {0};

    int32_t res_shape = parse_shape_array(csound, p->new_shape, &ndim, shape);
    if (res_shape != OK) {
        return res_shape;
    }

    size_t new_size = 0;
    if (get_array_size_from_shape(&new_size, ndim, shape) != OK) {
        return csound->InitError(csound, "[csnarray] Shape is invalid or its element count exceeds the configured limit");
    }

    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
        goto done;
    }

    CSN_ARRAY *arr = slot->array;

    if (new_size != arr->size) {
        res = csound->InitError(csound, "[csnarray] Reshape size mismatch: source has %zu elements, new shape requires %zu", arr->size, new_size);
        goto done;
    }

    /* Through set_csnarray_layout rather than by hand, so the array's shape and
       ndim counters move with the relayout. */
    set_csnarray_layout(arr, ndim, shape, arr->size, arr->itype);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

/* In-place k-rate reshape owns no output buffer. At init time only remember
   the current slot; a computed k-shape is applied during performance. */
int32_t csnarray_reshape_in_k_init(CSOUND *csound, CSN_RESHAPE_IN *p) {
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

    CSN_ARRAY *array = slot->array;
    SET_KDATA_WITH_ID_BEGIN(p, reg, array->shape, array->ndim, array->itype, source_handle);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_reshape_in_k(CSOUND *csound, CSN_RESHAPE_IN *p) {
    if (p->k_data.registry == NULL || p->k_data.owned_handle == 0) {
        return csound->PerfError(csound, &p->h, "[csnarray] k-rate in-place slot was not initialized");
    }

    uint32_t ndim = 0;
    uint32_t shape[CSN_MAX_DIMS] = {0};

    int32_t res_shape = parse_shape_array_k(csound, &p->h, p->new_shape, &ndim, shape);
    if (res_shape != OK) {
        return res_shape;
    }

    size_t requested_size = 0;
    if (get_array_size_from_shape(&requested_size, ndim, shape) != OK) {
        return csound->PerfError(csound, &p->h, "[csnarray] Shape is invalid or its element count exceeds the configured limit");
    }

    int32_t res = OK;
    csound->LockMutex(p->k_data.registry->mutex);

    CSN_SLOT *slot = get_slot(p->k_data.registry, p->k_data.owned_handle);
    if (slot == NULL) {
        csound->UnlockMutex(p->k_data.registry->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] k-rate output slot is no longer active");
    }

    CSN_ARRAY *arr = slot->array;

    if (requested_size != arr->size) {
        csound->UnlockMutex(p->k_data.registry->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Reshape size mismatch: source has %zu elements, new shape requires %zu", arr->size, requested_size);
    }

    ITEM_TYPE itype = arr->itype;
    set_csnarray_layout(arr, ndim, shape, requested_size, itype);

    SET_KDATA_NO_ID_END(p, shape, ndim, itype);

    csound->UnlockMutex(p->k_data.registry->mutex);
    return res;
}

int32_t csnarray_flatten(CSOUND *csound, CSN_RESHAPE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
        goto done;
    }

    CSN_ARRAY *arr = slot->array;

    uint32_t shape[CSN_MAX_DIMS] = {0};
    shape[0] = (uint32_t) arr->size;

    /* _locked: the registry mutex is already held and is not recursive. */
    if (create_csnarray_locked(csound, reg, &p->h, 1U, shape, &p->array, p->handle, &source_handle, 1U, &err, arr->itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    memcpy(p->array->data, arr->data, sizeof(double) * arr->size * arr->itype);
    SET_KDATA_BEGIN(p, reg);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_flatten_k(CSOUND *csound, CSN_RESHAPE *p) {
    CHECK_REG_HANDLE(csound, &p->h, p->k_data.registry, p->k_data.owned_handle);

    int32_t res = OK;
    csound->LockMutex(p->k_data.registry->mutex);

    uint32_t source_handle = p->source_handle->id;
    CSN_SLOT *source_slot = get_slot(p->k_data.registry, source_handle);
    if (source_slot == NULL) {
        csound->UnlockMutex(p->k_data.registry->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown source array handle %u: no array with this id is registered", source_handle);
    }

    CSN_ARRAY *source = source_slot->array;
    ITEM_TYPE itype = source->itype;

    uint32_t ndim = 1U;
    uint32_t shape[CSN_MAX_DIMS] = {0};
    shape[0] = (uint32_t) source->size;

    size_t requested_size = 0;
    if (get_array_size_from_shape(&requested_size, ndim, shape) != OK) {
        csound->UnlockMutex(p->k_data.registry->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Shape is invalid or its element count exceeds the configured limit");
    }

    const char *err = NULL;
    CSN_ARRAY *output = NULL;
    CSN_SLOT *reuse_slot = get_slot(p->k_data.registry, p->k_data.owned_handle);
    if (reuse_slot != NULL && CAN_REUSE_ELEMENTWISE(&p->k_data, source_handle, source, 0, NULL, reuse_slot->array, 0.0, 0.0)) {
        p->handle->id = p->k_data.owned_handle;
        goto done;
    }

    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &output, &p->k_data, NULL, ndim, shape, requested_size, itype, err);
    if (res != OK) goto done;
    p->array = output;

    memcpy(output->data, source->data, sizeof(double) * requested_size * itype);

    SET_KDATA_END(p, shape, ndim, itype);
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle, source, 0, NULL, output, 0.0, 0.0);

done:
    csound->UnlockMutex(p->k_data.registry->mutex);
    return res;
}

int32_t csnarray_flatten_in(CSOUND *csound, CSN_RESHAPE_IN *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
        goto done;
    }

    CSN_ARRAY *arr = slot->array;

    uint32_t flat_shape[CSN_MAX_DIMS] = {0};
    flat_shape[0] = (uint32_t) arr->size;
    set_csnarray_layout(arr, 1U, flat_shape, arr->size, arr->itype);

    SET_KDATA_WITH_ID_BEGIN(p, reg, arr->shape, arr->ndim, arr->itype, source_handle);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_flatten_in_k(CSOUND *csound, CSN_RESHAPE_IN *p) {
    if (p->k_data.registry == NULL || p->k_data.owned_handle == 0) {
        return csound->PerfError(csound, &p->h, "[csnarray] k-rate in-place slot was not initialized");
    }

    int32_t res = OK;
    csound->LockMutex(p->k_data.registry->mutex);

    CSN_SLOT *slot = get_slot(p->k_data.registry, p->k_data.owned_handle);
    if (slot == NULL) {
        csound->UnlockMutex(p->k_data.registry->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] k-rate output slot is no longer active");
    }

    CSN_ARRAY *arr = slot->array;
    uint32_t ndim = 1U;
    uint32_t shape[CSN_MAX_DIMS] = {0};
    shape[0] = (uint32_t) arr->size;

    ITEM_TYPE itype = arr->itype;
    set_csnarray_layout(arr, ndim, shape, arr->size, itype);

    SET_KDATA_NO_ID_END(p, arr->shape, ndim, itype);

    csound->UnlockMutex(p->k_data.registry->mutex);
    return res;
}

static int32_t transpose_data_assign(const double *source, double *destination, size_t size, uint32_t ndim, const uint32_t *shape, const size_t *strides, const uint32_t *axes, ITEM_TYPE itype) {
    CSN_BROADCAST_ITER it;
    if (ND_ITER_INIT(&it, ndim, shape, size, NULL) != OK) return NOTOK;
    while (BROADCAST_ITER_NEXT(&it)) {
        size_t linear = it.linear_index;
        size_t src_index = 0;
        for (uint32_t i = 0; i < ndim; ++i) {
            src_index += (size_t) it.coords[i] * strides[axes[i]];
        }
        if (itype == CSN_REAL) {
            destination[linear] = source[src_index];
        } else {
            destination[linear * 2] = source[src_index * 2];
            destination[linear * 2 + 1] = source[src_index * 2 + 1];
        }
    }
    return OK;
}

static int32_t transpose_axes_assign(const ARRAYDAT *shape, uint32_t *axes, uint32_t ndim) {
    bool used[CSN_MAX_DIMS] = {false};
    for (uint32_t i = 0; i < ndim; ++i) {
        double axis_value = (double) shape->data[i];
        CSN_AXIS_SPEC spec = csn_normalize_axis_value(axis_value, ndim);
        if (spec.kind != CSN_AXIS_INDEX) {
            return NOTOK;
        }

        uint32_t axis = spec.index;
        if (used[axis]) {
            return NOTOK;
        }

        used[axis] = true;
        axes[i] = axis;
    }
    return OK;
}

int32_t csnarray_transpose(CSOUND *csound, CSN_RESHAPE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;

    /* INOCOUNT reflects what the orchestra actually passed, so the no-axes
       overload does not depend on new_shape happening to be NULL. */
    bool is_default = p->INOCOUNT < 2;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
        goto done;
    }

    CSN_ARRAY *arr = slot->array;

    uint32_t ndim = arr->ndim;
    uint32_t shape[CSN_MAX_DIMS] = {0};
    uint32_t axes[CSN_MAX_DIMS] = {0};

    if (is_default) {
        for (uint32_t i = 0; i < ndim; ++i)
            axes[i] = ndim - 1 - i;
    }
    else {
        if (p->new_shape->dimensions != 1 || p->new_shape->sizes == NULL || p->new_shape->sizes[0] != (int32_t) ndim) {
            res = csound->InitError(csound, "[csnarray] Axes argument must be a 1-D array of exactly %u elements, one per dimension", ndim);
            goto done;
        }

        if (transpose_axes_assign(p->new_shape, axes, ndim) != OK) {
            res = csound->InitError(csound, "[csnarray] Axes argument is not a valid permutation");
            goto done;
        }

    }

    for (uint32_t i = 0; i < ndim; ++i) {
        shape[i] = arr->shape[axes[i]];
    }

    /* _locked: the registry mutex is already held and is not recursive. */
    if (create_csnarray_locked(csound, reg, &p->h, ndim, shape, &p->array, p->handle, &source_handle, 1U, &err, arr->itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *dst = p->array;
    dst->size = arr->size;
    if (transpose_data_assign(arr->data, dst->data, arr->size, ndim, shape, arr->strides, axes, arr->itype) != OK) {
        res = csound->InitError(csound, "[csnarray] Invalid transpose iterator layout");
        goto done;
    }
    SET_KDATA_BEGIN(p, reg);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_transpose_k(CSOUND *csound, CSN_RESHAPE *p) {
    CHECK_REG_HANDLE(csound, &p->h, p->k_data.registry, p->k_data.owned_handle);

    uint32_t source_handle = p->source_handle->id;

    bool is_default = p->INOCOUNT < 2;

    int32_t res = CHECK_SELF_ALIAS(csound, &p->h, &p->k_data, source_handle, 0);
    if (res != OK) return res;

    const char *err = NULL;

    csound->LockMutex(p->k_data.registry->mutex);

    CSN_SLOT *source_slot = get_slot(p->k_data.registry, source_handle);
    if (source_slot == NULL) {
        csound->UnlockMutex(p->k_data.registry->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
    }

    CSN_ARRAY *source_arr = source_slot->array;

    uint32_t ndim = source_arr->ndim;
    uint32_t shape[CSN_MAX_DIMS] = {0};
    uint32_t axes[CSN_MAX_DIMS] = {0};

    if (is_default) {
        for (uint32_t i = 0; i < ndim; ++i)
            axes[i] = ndim - 1 - i;
    }
    else {
        if (p->new_shape->dimensions != 1 || p->new_shape->sizes == NULL || p->new_shape->sizes[0] != (int32_t) ndim) {
            csound->UnlockMutex(p->k_data.registry->mutex);
            return csound->PerfError(csound, &p->h, "[csnarray] Axes argument must be a 1-D array of exactly %u elements, one per dimension", ndim);
        }

        if (transpose_axes_assign(p->new_shape, axes, ndim) != OK) {
            csound->UnlockMutex(p->k_data.registry->mutex);
            return csound->PerfError(csound, &p->h, "[csnarray] Axes argument is not a valid permutation");
        }
    }

    for (uint32_t i = 0; i < ndim; ++i) {
        shape[i] = source_arr->shape[axes[i]];
    }

    size_t requested_size = 0;
    res = get_array_size_from_shape(&requested_size, ndim, shape);
    if (res != OK) {
        csound->UnlockMutex(p->k_data.registry->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    ITEM_TYPE itype = source_arr->itype;
    CSN_ARRAY *dst = NULL;
    CSN_SLOT *reuse_slot = get_slot(p->k_data.registry, p->k_data.owned_handle);
    if (reuse_slot != NULL
        && CAN_REUSE_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, reuse_slot->array, 0.0, 0.0)
        && memcmp(p->k_data.prev_axes, axes, sizeof(uint32_t) * ndim) == 0) {
        p->handle->id = p->k_data.owned_handle;
        goto done;
    }

    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &dst, &p->k_data, NULL, ndim, shape, source_arr->size, itype, err);
    if (res != OK) goto done;
    p->array = dst;

    if (transpose_data_assign(source_arr->data, dst->data, source_arr->size, ndim, shape, source_arr->strides, axes, source_arr->itype) != OK) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Invalid transpose iterator layout");
        goto done;
    }

    SET_KDATA_END(p, shape, ndim, itype);
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, dst, 0.0, 0.0);
    memset(p->k_data.prev_axes, 0, sizeof(p->k_data.prev_axes));
    memcpy(p->k_data.prev_axes, axes, sizeof(uint32_t) * ndim);

done:
    csound->UnlockMutex(p->k_data.registry->mutex);
    return res;
}

int32_t csnarray_transpose_in(CSOUND *csound, CSN_RESHAPE_IN *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;

    /* INOCOUNT reflects what the orchestra actually passed, so the no-axes
       overload does not depend on new_shape happening to be NULL. */
    bool is_default = p->INOCOUNT < 2;

    int32_t res = OK;
    double *data = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
        goto done;
    }

    CSN_ARRAY *arr = slot->array;

    uint32_t ndim = arr->ndim;
    uint32_t shape[CSN_MAX_DIMS] = {0};
    size_t strides[CSN_MAX_DIMS] = {0};
    uint32_t axes[CSN_MAX_DIMS] = {0};

    if (is_default) {
        for (uint32_t i = 0; i < ndim; ++i)
            axes[i] = ndim - 1 - i;
    }
    else {
        if (p->new_shape->dimensions != 1 || p->new_shape->sizes == NULL || p->new_shape->sizes[0] != (int32_t) ndim) {
            res = csound->InitError(csound, "[csnarray] Axes argument must be a 1-D array of exactly %u elements, one per dimension", ndim);
            goto done;
        }

        if (transpose_axes_assign(p->new_shape, axes, ndim) != OK) {
            res = csound->InitError(csound, "[csnarray] Axes argument is not a valid permutation");
            goto done;
        }
    }

    /* Allocated only once the axes are known good, so the rejection paths
       above have nothing to release. */
    data = csound->Calloc(csound, sizeof(double) * arr->size * arr->itype);
    if (data == NULL) {
        res = csound->InitError(csound, "[csnarray] Out of memory: allocation of %zu bytes failed", (size_t) (sizeof(double) * arr->size * arr->itype));
        goto done;
    }

    for (uint32_t i = 0; i < ndim; ++i) {
        shape[i] = arr->shape[axes[i]];
    }

    compute_strides(shape, strides, ndim);
    if (transpose_data_assign(arr->data, data, arr->size, ndim, shape, arr->strides, axes, arr->itype) != OK) {
        res = csound->InitError(csound, "[csnarray] Invalid transpose iterator layout");
        goto done;
    }

    memcpy(arr->data, data, sizeof(double) * arr->size * arr->itype);
    memset(arr->shape, 0, sizeof(arr->shape));
    memset(arr->strides, 0, sizeof(arr->strides));

    for (uint32_t i = 0; i < ndim; ++i) {
        arr->shape[i] = shape[i];
        arr->strides[i] = strides[i];
    }

    /* The i-rate in-place forms write an array they do not own, exactly as
       their k-rate twins do, so they owe the same counter bump. The axes
       permutation rewrites shape and strides by hand here, bypassing
       set_csnarray_layout, so the shape counter moves from here too. */
    update_array_data_version(&arr->version);
    update_array_layout_version(&arr->version, true, false, false);

done:
    csound->UnlockMutex(reg->mutex);
    if (data != NULL) {
        csound->Free(csound, data);
    }
    return res;
}

int32_t csnarray_transpose_in_k_init(CSOUND *csound, CSN_RESHAPE_IN *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;

    bool is_default = p->INOCOUNT < 2;

    int32_t res = OK;
    double *data = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
        goto done;
    }

    CSN_ARRAY *arr = slot->array;

    uint32_t ndim = arr->ndim;
    uint32_t shape[CSN_MAX_DIMS] = {0};
    size_t strides[CSN_MAX_DIMS] = {0};
    uint32_t axes[CSN_MAX_DIMS] = {0};

    if (is_default) {
        for (uint32_t i = 0; i < ndim; ++i)
            axes[i] = ndim - 1 - i;
    }
    else {
        if (p->new_shape->dimensions != 1 || p->new_shape->sizes == NULL || p->new_shape->sizes[0] != (int32_t) ndim) {
            res = csound->InitError(csound, "[csnarray] Axes argument must be a 1-D array of exactly %u elements, one per dimension", ndim);
            goto done;
        }

        if (transpose_axes_assign(p->new_shape, axes, ndim) != OK) {
            res = csound->InitError(csound, "[csnarray] Axes argument is not a valid permutation");
            goto done;
        }
    }

    /* Sized to the source's capacity, not its current length: the source can
       grow that far without reallocating, and a marked source must find the
       scratch already big enough at perf time. */
    size_t scratch_capacity = arr->capacity * arr->itype;
    data = csound->Calloc(csound, sizeof(double) * scratch_capacity);
    if (data == NULL) {
        res = csound->InitError(csound, "[csnarray] Out of memory: allocation of %zu bytes failed", (size_t) (sizeof(double) * arr->size * arr->itype));
        goto done;
    }
    p->scratch.scratch_capacity = scratch_capacity;
    p->scratch.scratch = data;

    for (uint32_t i = 0; i < ndim; ++i) {
        shape[i] = arr->shape[axes[i]];
    }
    if (transpose_data_assign(arr->data, p->scratch.scratch, arr->size, ndim, shape, arr->strides, axes, arr->itype) != OK) {
        res = csound->InitError(csound, "[csnarray] Invalid transpose iterator layout");
        goto done;
    }
    memcpy(arr->data, p->scratch.scratch, sizeof(double) * arr->size * arr->itype);

    compute_strides(shape, strides, ndim);

    memset(arr->shape, 0, sizeof(arr->shape));
    memset(arr->strides, 0, sizeof(arr->strides));

    for (uint32_t i = 0; i < ndim; ++i) {
        arr->shape[i] = shape[i];
        arr->strides[i] = strides[i];
    }

    SET_KDATA_WITH_ID_BEGIN(p, reg, shape, ndim, arr->itype, source_handle);
    p->k_data.prev_size = arr->size;
    memset(p->k_data.prev_axes, 0, sizeof(p->k_data.prev_axes));
    memcpy(p->k_data.prev_axes, axes, sizeof(uint32_t) * arr->ndim);

    PUBLISH_INPLACE_WRITE(&p->k_data, source_handle, arr, true, false, false);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_transpose_in_k_deinit(CSOUND *csound, CSN_RESHAPE_IN *p) {
    if (p->scratch.scratch != NULL) {
        csound->Free(csound, p->scratch.scratch);
    }

    return OK;
}

int32_t csnarray_transpose_in_k(CSOUND *csound, CSN_RESHAPE_IN *p) {
    CHECK_REG_HANDLE(csound, &p->h, p->k_data.registry, p->k_data.owned_handle);

    uint32_t source_handle = p->source_handle->id;

    bool is_default = p->INOCOUNT < 2;
    int32_t res = OK;

    csound->LockMutex(p->k_data.registry->mutex);

    CSN_SLOT *slot = get_slot(p->k_data.registry, source_handle);
    if (slot == NULL) {
        csound->UnlockMutex(p->k_data.registry->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
    }

    CSN_ARRAY *arr = slot->array;

    ITEM_TYPE itype = arr->itype;
    uint32_t ndim = arr->ndim;
    uint32_t shape[CSN_MAX_DIMS] = {0};
    size_t strides[CSN_MAX_DIMS] = {0};
    uint32_t axes[CSN_MAX_DIMS] = {0};

    if (is_default) {
        for (uint32_t i = 0; i < ndim; ++i)
            axes[i] = ndim - 1 - i;
    }
    else {
        if (p->new_shape->dimensions != 1 || p->new_shape->sizes == NULL || p->new_shape->sizes[0] != (int32_t) ndim) {
            csound->UnlockMutex(p->k_data.registry->mutex);
            return csound->PerfError(csound, &p->h, "[csnarray] Axes argument must be a 1-D array of exactly %u elements, one per dimension", ndim);
        }

        if (transpose_axes_assign(p->new_shape, axes, ndim) != OK) {
            csound->UnlockMutex(p->k_data.registry->mutex);
            return csound->PerfError(csound, &p->h, "[csnarray] Axes argument is not a valid permutation");
        }
    }

    for (uint32_t i = 0; i < ndim; ++i) {
        shape[i] = arr->shape[axes[i]];
    }

    compute_strides(shape, strides, ndim);

    bool axes_changed = memcmp(axes, p->k_data.prev_axes, sizeof(axes)) != 0;
    res = CHECK_IF_REALLOC_IN(csound, &p->h, &p->k_data, arr, source_handle, &p->scratch, ndim, itype, axes_changed, slot->rt_locked);
    if (res != OK) {
        res = res == NOTOK ? OK : res;
        goto done;
    }

    if (transpose_data_assign(arr->data, p->scratch.scratch, arr->size, ndim, shape, arr->strides, axes, arr->itype) != OK) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Invalid transpose iterator layout");
        goto done;
    }
    memcpy(arr->data, p->scratch.scratch, sizeof(double) * arr->size * arr->itype);

    memset(arr->shape, 0, sizeof(arr->shape));
    memset(arr->strides, 0, sizeof(arr->strides));

    for (uint32_t i = 0; i < ndim; ++i) {
        arr->shape[i] = shape[i];
        arr->strides[i] = strides[i];
    }

    SET_KDATA_NO_ID_END(p, arr->shape, ndim, itype);
    p->k_data.prev_size = arr->size;
    memset(p->k_data.prev_axes, 0, sizeof(p->k_data.prev_axes));
    memcpy(p->k_data.prev_axes, axes, sizeof(axes));

    /* The axes permutation rewrote shape and strides straight onto the array,
       bypassing set_csnarray_layout, so the shape counter moves from here. */
    PUBLISH_INPLACE_WRITE(&p->k_data, source_handle, arr, true, false, false);

done:
    csound->UnlockMutex(p->k_data.registry->mutex);
    return res;
}


static int32_t flip_assign_value(CSN_ARRAY *source, CSN_ARRAY *destination, double *buffer, uint32_t *dest_shape, uint32_t ndim, uint32_t axis) {
    CSN_BROADCAST_ITER it;
    if (ND_ITER_INIT(&it, ndim, dest_shape, source->size, NULL) != OK) return NOTOK;
    while (BROADCAST_ITER_NEXT(&it)) {
        size_t linear = it.linear_index;
        size_t src_index = 0;
        for (uint32_t i = 0; i < ndim; ++i) {
            uint32_t coord = axis == UINT32_MAX || axis == i ? source->shape[i] - 1U - it.coords[i] : it.coords[i];
            src_index += (size_t) coord * source->strides[i];
        }
        if (source->itype == CSN_REAL) {
            if (buffer == NULL) {
                destination->data[linear] = source->data[src_index];
            } else {
                buffer[linear] = source->data[src_index];
            }
        } else {
            if (buffer == NULL) {
                destination->data[linear * 2] = source->data[src_index * 2];
                destination->data[linear * 2 + 1] = source->data[src_index * 2 + 1];
            } else {
                buffer[linear * 2] = source->data[src_index * 2];
                buffer[linear * 2 + 1] = source->data[src_index * 2 + 1];
            }
        }
    }
    return OK;
}

static int32_t csnarray_flip_init_helper(CSOUND *csound, CSN_FLIP_ROLL *p, const MYFLT *axis_in) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
        goto done;
    }

    CSN_ARRAY *arr = slot->array;

    uint32_t ndim = arr->ndim;

    CSN_AXIS_SPEC axis_spec = csn_normalize_axis(axis_in, ndim, CSN_AXIS_DEFAULT_ALL);
    if (axis_spec.kind == CSN_AXIS_INVALID) {
        double value = axis_in == NULL ? 0.0 : (double) *axis_in;
        res = csound->InitError(csound, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: finite integers %d..%u)", value, ndim, -(int32_t) ndim, ndim - 1);
        goto done;
    }
    int32_t axis_flip = axis_spec.kind == CSN_AXIS_ALL ? -1 : (int32_t) axis_spec.index;

    /* _locked: the registry mutex is already held and is not recursive. */
    if (create_csnarray_locked(csound, reg, &p->h, ndim, arr->shape, &p->array, p->handle, &source_handle, 1U, &err, arr->itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *dst = p->array;
    dst->size = arr->size;
    if (flip_assign_value(arr, dst, NULL, dst->shape, ndim, axis_flip) != OK) {
        res = csound->InitError(csound, "[csnarray] Invalid flip iterator layout");
        goto done;
    }
    SET_KDATA_BEGIN(p, reg);
    p->k_data.prev_axis_u = axis_flip;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_flip(CSOUND *csound, CSN_FLIP_ROLL *p) {
    const MYFLT *axis_in = p->INOCOUNT > 1 ? p->param_a : NULL;
    return csnarray_flip_init_helper(csound, p, axis_in);
}

int32_t csnarray_flip_k_init(CSOUND *csound, CSN_FLIP_ROLL *p) {
    const MYFLT *axis_in = p->INOCOUNT > 2 ? p->param_b : NULL;
    return csnarray_flip_init_helper(csound, p, axis_in);
}

int32_t csnarray_flip_k(CSOUND *csound, CSN_FLIP_ROLL *p) {
    CHECK_REG_HANDLE(csound, &p->h, p->k_data.registry, p->k_data.owned_handle);
    CHECK_KTRIG(p->param_a);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = CHECK_SELF_ALIAS(csound, &p->h, &p->k_data, source_handle, 0);
    if (res != OK) return res;

    const char *err = NULL;

    csound->LockMutex(p->k_data.registry->mutex);

    CSN_SLOT *slot = get_slot(p->k_data.registry, source_handle);
    if (slot == NULL) {
        csound->UnlockMutex(p->k_data.registry->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
    }

    CSN_ARRAY *arr = slot->array;
    uint32_t ndim = arr->ndim;

    const MYFLT *axis_in = p->INOCOUNT > 2 ? p->param_b : NULL;
    CSN_AXIS_SPEC axis_spec = csn_normalize_axis(axis_in, ndim, CSN_AXIS_DEFAULT_ALL);
    if (axis_spec.kind == CSN_AXIS_INVALID) {
        double value = axis_in == NULL ? 0.0 : (double) *axis_in;
        csound->UnlockMutex(p->k_data.registry->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: finite integers %d..%u)", value, ndim, -(int32_t) ndim, ndim - 1);
    }
    int32_t axis_flip = axis_spec.kind == CSN_AXIS_ALL ? -1 : (int32_t) axis_spec.index;
    double axis_value = (double) axis_flip;

    CSN_ARRAY *dst = p->array;
    CSN_SLOT *reuse_slot = get_slot(p->k_data.registry, p->k_data.owned_handle);
    if (reuse_slot != NULL && CAN_REUSE_ELEMENTWISE(&p->k_data, source_handle, arr, 0, NULL, reuse_slot->array, axis_value, 0.0)) {
        p->handle->id = p->k_data.owned_handle;
        goto done;
    }

    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &dst, &p->k_data, NULL, ndim, arr->shape, arr->size, arr->itype, err);
    if (res != OK) goto done;

    if (flip_assign_value(arr, dst, NULL, dst->shape, ndim, axis_flip) != OK) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Invalid flip iterator layout");
        goto done;
    }
    SET_KDATA_END(p, arr->shape, ndim, arr->itype);
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle, arr, 0, NULL, dst, axis_value, 0.0);
    /* Through the int32_t, because the all-axes marker is -1 and converting
       that from a double straight into an unsigned is undefined. */
    p->k_data.prev_axis_u = (uint32_t) axis_flip;

done:
    csound->UnlockMutex(p->k_data.registry->mutex);
    return res;
}

int32_t csnarray_flip_in(CSOUND *csound, CSN_FLIP_ROLL_IN *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    double *data = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
        goto done;
    }

    CSN_ARRAY *arr = slot->array;

    uint32_t ndim = arr->ndim;

    const MYFLT *axis_in = p->INOCOUNT > 1 ? p->param_a : NULL;
    CSN_AXIS_SPEC axis_spec = csn_normalize_axis(axis_in, ndim, CSN_AXIS_DEFAULT_ALL);
    if (axis_spec.kind == CSN_AXIS_INVALID) {
        double value = axis_in == NULL ? 0.0 : (double) *axis_in;
        res = csound->InitError(csound, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: finite integers %d..%u)", value, ndim, -(int32_t) ndim, ndim - 1);
        goto done;
    }
    int32_t axis_flip = axis_spec.kind == CSN_AXIS_ALL ? -1 : (int32_t) axis_spec.index;

    /* Allocated only once the axes are known good, so the rejection paths
       above have nothing to release. */
    data = csound->Calloc(csound, sizeof(double) * arr->size * arr->itype);
    if (data == NULL) {
        res = csound->InitError(csound, "[csnarray] Out of memory: allocation of %zu bytes failed", (size_t) (sizeof(double) * arr->size * arr->itype));
        goto done;
    }

    if (flip_assign_value(arr, NULL, data, arr->shape, ndim, axis_flip) != OK) {
        res = csound->InitError(csound, "[csnarray] Invalid flip iterator layout");
        goto done;
    }
    memcpy(arr->data, data, sizeof(double) * arr->size * arr->itype);
    update_array_data_version(&arr->version);

done:
    csound->UnlockMutex(reg->mutex);
    if (data != NULL) {
        csound->Free(csound, data);
    }
    return res;
}

int32_t csnarray_flip_in_k_deinit(CSOUND *csound, CSN_FLIP_ROLL_IN *p) {
    if (p->scratch.scratch != NULL) {
        csound->Free(csound, p->scratch.scratch);
    }
    return OK;
}

int32_t csnarray_flip_in_k_init(CSOUND *csound, CSN_FLIP_ROLL_IN *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
        goto done;
    }

    CSN_ARRAY *arr = slot->array;
    uint32_t ndim = arr->ndim;

    const MYFLT *axis_in = p->INOCOUNT > 2 ? p->param_b : NULL;
    CSN_AXIS_SPEC axis_spec = csn_normalize_axis(axis_in, ndim, CSN_AXIS_DEFAULT_ALL);
    if (axis_spec.kind == CSN_AXIS_INVALID) {
        double value = axis_in == NULL ? 0.0 : (double) *axis_in;
        res = csound->InitError(csound, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: finite integers %d..%u)", value, ndim, -(int32_t) ndim, ndim - 1);
        goto done;
    }
    int32_t axis_flip = axis_spec.kind == CSN_AXIS_ALL ? -1 : (int32_t) axis_spec.index;

    /* Sized to the source's capacity, not its current length: the source can
       grow that far without reallocating, and a marked source must find the
       scratch already big enough at perf time. */
    size_t s_capacity = arr->capacity * (size_t) arr->itype;
    double *data = csound->Calloc(csound, sizeof(double) * s_capacity);
    if (data == NULL) {
        res = csound->InitError(csound, "[csnarray] Out of memory: allocation of %zu bytes failed", (size_t) (sizeof(double) * arr->size * arr->itype));
        goto done;
    }

    if (flip_assign_value(arr, NULL, data, arr->shape, ndim, axis_flip) != OK) {
        res = csound->InitError(csound, "[csnarray] Invalid flip iterator layout");
        goto done;
    }
    memcpy(arr->data, data, sizeof(double) * arr->size * arr->itype);
    p->scratch.scratch = data;
    p->scratch.scratch_capacity = s_capacity;

    memset(p->k_data.prev_shape, 0, sizeof(p->k_data.prev_shape));
    SET_KDATA_WITH_ID_BEGIN(p, reg, arr->shape, ndim, arr->itype, source_handle);
    p->k_data.prev_size = arr->size;
    p->k_data.prev_axis_u = axis_flip;

    /* The init already flipped, so it has to publish like any other pass:
       without this the first k-pass sees a cache it has never filled, decides
       the array moved, and flips a second time straight back to the original. */
    PUBLISH_INPLACE_WRITE(&p->k_data, source_handle, arr, false, false, false);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_flip_in_k(CSOUND *csound, CSN_FLIP_ROLL_IN *p) {
    CHECK_REG_HANDLE(csound, &p->h, p->k_data.registry, p->k_data.owned_handle);
    CHECK_KTRIG(p->param_a);

    uint32_t source_handle = p->source_handle->id;
    int32_t res = OK;

    csound->LockMutex(p->k_data.registry->mutex);

    CSN_SLOT *slot = get_slot(p->k_data.registry, source_handle);
    if (slot == NULL) {
        csound->UnlockMutex(p->k_data.registry->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
    }

    CSN_ARRAY *arr = slot->array;
    uint32_t ndim = arr->ndim;
    ITEM_TYPE itype = arr->itype;

    const MYFLT *axis_in = p->INOCOUNT > 2 ? p->param_b : NULL;
    CSN_AXIS_SPEC axis_spec = csn_normalize_axis(axis_in, ndim, CSN_AXIS_DEFAULT_ALL);
    if (axis_spec.kind == CSN_AXIS_INVALID) {
        double value = axis_in == NULL ? 0.0 : (double) *axis_in;
        csound->UnlockMutex(p->k_data.registry->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: finite integers %d..%u)", value, ndim, -(int32_t) ndim, ndim - 1);
    }
    int32_t axis_flip = axis_spec.kind == CSN_AXIS_ALL ? -1 : (int32_t) axis_spec.index;

    bool axis_changed = axis_flip != p->k_data.prev_axis_u;
    res = CHECK_IF_REALLOC_IN(csound, &p->h, &p->k_data, arr, source_handle, &p->scratch, ndim, itype, axis_changed, slot->rt_locked);
    if (res != OK) {
        res = res == NOTOK ? OK : res;
        goto done;
    }

    memset(p->scratch.scratch, 0, sizeof(double) * p->scratch.scratch_capacity);
    if (flip_assign_value(arr, NULL, p->scratch.scratch, arr->shape, ndim, axis_flip) != OK) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Invalid flip iterator layout");
        goto done;
    }

    memcpy(arr->data, p->scratch.scratch, sizeof(double) * arr->size * arr->itype);
    memset(p->k_data.prev_shape, 0, sizeof(p->k_data.prev_shape));
    SET_KDATA_NO_ID_END(p, arr->shape, ndim, itype);
    p->k_data.prev_size = arr->size;
    p->k_data.prev_axis_u = axis_flip;

    PUBLISH_INPLACE_WRITE(&p->k_data, source_handle, arr, false, false, false);

done:
    csound->UnlockMutex(p->k_data.registry->mutex);
    return res;
}

static uint32_t wrap_index(int64_t x, uint32_t size) {
    int64_t index = x % (int64_t) size;
    if (index < 0) index += size;
    return (uint32_t) index;
}

static void roll_assign_value(CSN_ARRAY *source, CSN_ARRAY *destination, double *buffer, int32_t shift) {
    for (size_t linear = 0; linear < source->size; ++linear) {
        uint32_t src_index = wrap_index((int64_t) linear - shift, (uint32_t) source->size);
        if (source->itype == CSN_REAL) {
            if (destination == NULL) {
                buffer[linear] = source->data[src_index];
            } else {
                destination->data[linear] = source->data[src_index];
            }
        } else {
            if (destination == NULL) {
                buffer[linear * 2] = source->data[src_index * 2];
                buffer[linear * 2 + 1] = source->data[src_index * 2 + 1];
            } else {
                destination->data[linear * 2] = source->data[src_index * 2];
                destination->data[linear * 2 + 1] = source->data[src_index * 2 + 1];
            }
        }
    }
}

static int32_t rollaxis_assign_value(CSN_ARRAY *source, CSN_ARRAY *destination, double *buffer, uint32_t *dest_shape, uint32_t ndim, int32_t shift, int32_t axis) {
    CSN_BROADCAST_ITER it;
    if (ND_ITER_INIT(&it, ndim, dest_shape, source->size, NULL) != OK) return NOTOK;
    while (BROADCAST_ITER_NEXT(&it)) {
        size_t linear = it.linear_index;
        size_t src_index = 0;
        for (uint32_t i = 0; i < ndim; ++i) {
            uint32_t coord = axis == -1 || axis == (int32_t) i ? wrap_index((int64_t) it.coords[i] - shift, source->shape[i]) : it.coords[i];
            src_index += (size_t) coord * source->strides[i];
        }
        if (source->itype == CSN_REAL) {
            if (destination == NULL) {
                buffer[linear] = source->data[src_index];
            } else {
                destination->data[linear] = source->data[src_index];
            }
        } else {
            if (destination == NULL) {
                buffer[linear * 2] = source->data[src_index * 2];
                buffer[linear * 2 + 1] = source->data[src_index * 2 + 1];
            } else {
                destination->data[linear * 2] = source->data[src_index * 2];
                destination->data[linear * 2 + 1] = source->data[src_index * 2 + 1];
            }
        }
    }
    return OK;
}

int32_t csnarray_roll(CSOUND *csound, CSN_FLIP_ROLL *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
        goto done;
    }

    CSN_ARRAY *arr = slot->array;
    uint32_t ndim = arr->ndim;
    double shift_value = (double) *p->param_a;
    if (!IS_VALID_SHIFT(shift_value)) {
        res = csound->InitError(csound, "[csnarray] Shift %g is invalid; shift must be a finite integer in %d..%d", shift_value, INT32_MIN, INT32_MAX);
        goto done;
    }
    int32_t shift = (int32_t) shift_value;

    /* _locked: the registry mutex is already held and is not recursive. */
    if (create_csnarray_locked(csound, reg, &p->h, ndim, arr->shape, &p->array, p->handle, &source_handle, 1U, &err, arr->itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *dst = p->array;
    dst->size = arr->size;
    roll_assign_value(arr, dst, NULL, shift);

    memset(p->k_data.prev_shape, 0, sizeof(p->k_data.prev_shape));
    SET_KDATA_BEGIN(p, reg);
    p->k_data.prev_roll_shift = shift;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_roll_k(CSOUND *csound, CSN_FLIP_ROLL *p) {
    CHECK_REG_HANDLE(csound, &p->h, p->k_data.registry, p->k_data.owned_handle);

    CSN_REGISTRY *reg = p->k_data.registry;
    uint32_t source_handle = p->source_handle->id;

    int32_t res = CHECK_SELF_ALIAS(csound, &p->h, &p->k_data, source_handle, 0);
    if (res != OK) return res;

    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
    }

    CSN_ARRAY *arr = slot->array;
    uint32_t ndim = arr->ndim;
    double shift_value = (double) *p->param_a;
    if (!IS_VALID_SHIFT(shift_value)) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Shift %g is invalid; shift must be a finite integer in %d..%d", shift_value, INT32_MIN, INT32_MAX);
    }
    int32_t shift = (int32_t) shift_value;

    ITEM_TYPE itype = arr->itype;
    CSN_ARRAY *dst = p->array;
    CSN_SLOT *reuse_slot = get_slot(p->k_data.registry, p->k_data.owned_handle);
    if (reuse_slot != NULL && CAN_REUSE_ELEMENTWISE(&p->k_data, source_handle, arr, 0, NULL, reuse_slot->array, shift_value, 0.0)) {
        p->handle->id = p->k_data.owned_handle;
        goto done;
    }

    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &dst, &p->k_data, NULL, ndim, arr->shape, arr->size, itype, err);
    if (res != OK) goto done;

    roll_assign_value(arr, dst, NULL, shift);

    SET_KDATA_END(p, arr->shape, ndim, itype);
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle, arr, 0, NULL, dst, shift_value, 0.0);
    p->k_data.prev_roll_shift = shift;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}


int32_t csnarray_roll_in(CSOUND *csound, CSN_FLIP_ROLL_IN *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;
    int32_t res = OK;
    double *data = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
        goto done;
    }

    CSN_ARRAY *arr = slot->array;
    /* roll works on the flattened array, so the rank plays no part here. */
    double shift_value = (double) *p->param_a;
    if (!IS_VALID_SHIFT(shift_value)) {
        res = csound->InitError(csound, "[csnarray] Shift %g is invalid; shift must be a finite integer in %d..%d", shift_value, INT32_MIN, INT32_MAX);
        goto done;
    }
    int32_t shift = (int32_t) shift_value;

    data = csound->Calloc(csound, sizeof(double) * arr->size * arr->itype);
    if (data == NULL) {
        res = csound->InitError(csound, "[csnarray] Out of memory: allocation of %zu bytes failed", (size_t) (sizeof(double) * arr->size * arr->itype));
        goto done;
    }

    roll_assign_value(arr, NULL, data, shift);
    memcpy(arr->data, data, sizeof(double) * arr->size * arr->itype);
    update_array_data_version(&arr->version);

done:
    csound->UnlockMutex(reg->mutex);
    if (data != NULL) {
        csound->Free(csound, data);
    }
    return res;
}

int32_t csnarray_roll_in_k_init(CSOUND *csound, CSN_FLIP_ROLL_IN *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;
    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
        goto done;
    }

    CSN_ARRAY *arr = slot->array;
    double shift_value = (double) *p->param_a;
    if (!IS_VALID_SHIFT(shift_value)) {
        res = csound->InitError(csound, "[csnarray] Shift %g is invalid; shift must be a finite integer in %d..%d", shift_value, INT32_MIN, INT32_MAX);
        goto done;
    }
    int32_t shift = (int32_t) shift_value;

    /* Sized to the source's capacity, not its current length: the source can
       grow that far without reallocating, and a marked source must find the
       scratch already big enough at perf time. */
    size_t capacity = arr->capacity * (size_t) arr->itype;
    double *data = csound->Calloc(csound, sizeof(double) * capacity);
    if (data == NULL) {
        res = csound->InitError(csound, "[csnarray] Out of memory: allocation of %zu bytes failed", (size_t) (sizeof(double) * arr->size * arr->itype));
        goto done;
    }
    p->scratch.scratch = data;
    p->scratch.scratch_capacity = capacity;

    roll_assign_value(arr, NULL, p->scratch.scratch, shift);

    memcpy(arr->data, data, sizeof(double) * arr->size * arr->itype);
    SET_KDATA_WITH_ID_BEGIN(p, reg, arr->shape, arr->ndim, arr->itype, source_handle);
    p->k_data.prev_size = arr->size;
    p->k_data.prev_roll_shift = shift;

    PUBLISH_INPLACE_WRITE(&p->k_data, source_handle, arr, false, false, false);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_roll_in_k(CSOUND *csound, CSN_FLIP_ROLL_IN *p) {
    CHECK_REG_HANDLE(csound, &p->h, p->k_data.registry, p->k_data.owned_handle);

    CSN_REGISTRY *reg = p->k_data.registry;
    uint32_t source_handle = p->source_handle->id;
    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
    }

    CSN_ARRAY *arr = slot->array;
    uint32_t ndim = arr->ndim;
    uint32_t itype = arr->itype;
    double shift_value = (double) *p->param_a;
    if (!IS_VALID_SHIFT(shift_value)) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Shift %g is invalid; shift must be a finite integer in %d..%d", shift_value, INT32_MIN, INT32_MAX);
    }
    int32_t shift = (int32_t) shift_value;

    bool shift_changed = shift != p->k_data.prev_roll_shift;

    res = CHECK_IF_REALLOC_IN(csound, &p->h, &p->k_data, arr, source_handle, &p->scratch, ndim, itype, shift_changed, slot->rt_locked);
    if (res != OK) {
        res = res == NOTOK ? OK : res;
        goto done;
    }

    roll_assign_value(arr, NULL, p->scratch.scratch, shift);

    memcpy(arr->data, p->scratch.scratch, sizeof(double) * arr->size * arr->itype);
    SET_KDATA_WITH_ID_BEGIN(p, reg, arr->shape, arr->ndim, arr->itype, source_handle);
    p->k_data.prev_size = arr->size;
    p->k_data.prev_roll_shift = shift;

    PUBLISH_INPLACE_WRITE(&p->k_data, source_handle, arr, false, false, false);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_rollaxis(CSOUND *csound, CSN_FLIP_ROLL *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
        goto done;
    }

    CSN_ARRAY *arr = slot->array;
    uint32_t ndim = arr->ndim;
    double shift_value = (double) *p->param_a;
    if (!IS_VALID_SHIFT(shift_value)) {
        res = csound->InitError(csound, "[csnarray] Shift %g is invalid; shift must be a finite integer in %d..%d", shift_value, INT32_MIN, INT32_MAX);
        goto done;
    }
    int32_t shift = (int32_t) shift_value;
    double axis_value = (double) *p->param_b;
    CSN_AXIS_SPEC axis_spec = csn_normalize_axis_value(axis_value, ndim);
    if (axis_spec.kind != CSN_AXIS_INDEX) {
        res = csound->InitError(csound, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: finite integers %d..%u)", axis_value, ndim, -(int32_t) ndim, ndim - 1);
        goto done;
    }
    int32_t axis_roll = (int32_t) axis_spec.index;

    /* _locked: the registry mutex is already held and is not recursive. */
    if (create_csnarray_locked(csound, reg, &p->h, ndim, arr->shape, &p->array, p->handle, &source_handle, 1U, &err, arr->itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *dst = p->array;
    dst->size = arr->size;
    if (rollaxis_assign_value(arr, dst, NULL, dst->shape, ndim, shift, axis_roll) != OK) {
        res = csound->InitError(csound, "[csnarray] Invalid roll iterator layout");
        goto done;
    }
    SET_KDATA_BEGIN(p, reg);
    p->k_data.prev_roll_shift = shift;
    p->k_data.prev_axis_u = axis_roll;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_rollaxis_k(CSOUND *csound, CSN_FLIP_ROLL *p) {
    CHECK_REG_HANDLE(csound, &p->h, p->k_data.registry, p->k_data.owned_handle);

    uint32_t source_handle = p->source_handle->id;
    CSN_REGISTRY *reg = p->k_data.registry;

    int32_t res = CHECK_SELF_ALIAS(csound, &p->h, &p->k_data, source_handle, 0);
    if (res != OK) return res;

    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
    }

    CSN_ARRAY *arr = slot->array;
    uint32_t ndim = arr->ndim;
    double shift_value = (double) *p->param_a;
    if (!IS_VALID_SHIFT(shift_value)) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Shift %g is invalid; shift must be a finite integer in %d..%d", shift_value, INT32_MIN, INT32_MAX);
    }
    int32_t shift = (int32_t) shift_value;
    double axis_value = (double) *p->param_b;
    CSN_AXIS_SPEC axis_spec = csn_normalize_axis_value(axis_value, ndim);
    if (axis_spec.kind != CSN_AXIS_INDEX) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: finite integers %d..%u)", axis_value, ndim, -(int32_t) ndim, ndim - 1);
    }
    int32_t axis_roll = (int32_t) axis_spec.index;

    CSN_ARRAY *dst = p->array;
    CSN_SLOT *reuse_slot = get_slot(p->k_data.registry, p->k_data.owned_handle);
    if (reuse_slot != NULL && CAN_REUSE_ELEMENTWISE(&p->k_data, source_handle, arr, 0, NULL, reuse_slot->array, shift_value, axis_value)) {
        p->handle->id = p->k_data.owned_handle;
        goto done;
    }

    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &dst, &p->k_data, NULL, ndim, arr->shape, arr->size, arr->itype, err);
    if (res != OK) goto done;

    if (rollaxis_assign_value(arr, dst, NULL, dst->shape, ndim, shift, axis_roll) != OK) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Invalid roll iterator layout");
        goto done;
    }
    SET_KDATA_END(p, arr->shape, ndim, arr->itype);
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle, arr, 0, NULL, dst, shift_value, axis_value);
    p->k_data.prev_axis_u = axis_roll;
    p->k_data.prev_roll_shift = shift;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_rollaxis_in(CSOUND *csound, CSN_FLIP_ROLL_IN *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    double *data = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
        goto done;
    }

    CSN_ARRAY *arr = slot->array;
    uint32_t ndim = arr->ndim;
    double shift_value = (double) *p->param_a;
    if (!IS_VALID_SHIFT(shift_value)) {
        res = csound->InitError(csound, "[csnarray] Shift %g is invalid; shift must be a finite integer in %d..%d", shift_value, INT32_MIN, INT32_MAX);
        goto done;
    }
    int32_t shift = (int32_t) shift_value;
    double axis_value = (double) *p->param_b;
    CSN_AXIS_SPEC axis_spec = csn_normalize_axis_value(axis_value, ndim);
    if (axis_spec.kind != CSN_AXIS_INDEX) {
        res = csound->InitError(csound, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: finite integers %d..%u)", axis_value, ndim, -(int32_t) ndim, ndim - 1);
        goto done;
    }
    int32_t axis_roll = (int32_t) axis_spec.index;

    /* Allocated only once the axes are known good, so the rejection paths
       above have nothing to release. */
    data = csound->Calloc(csound, sizeof(double) * arr->size * arr->itype);
    if (data == NULL) {
        res = csound->InitError(csound, "[csnarray] Out of memory: allocation of %zu bytes failed", (size_t) (sizeof(double) * arr->size * arr->itype));
        goto done;
    }

    if (rollaxis_assign_value(arr, NULL, data, arr->shape, ndim, shift, axis_roll) != OK) {
        res = csound->InitError(csound, "[csnarray] Invalid roll iterator layout");
        goto done;
    }
    memcpy(arr->data, data, sizeof(double) * arr->size * arr->itype);
    update_array_data_version(&arr->version);

done:
    csound->UnlockMutex(reg->mutex);
    if (data != NULL) {
        csound->Free(csound, data);
    }
    return res;
}

int32_t csnarray_rollaxis_in_k_init(CSOUND *csound, CSN_FLIP_ROLL_IN *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;
    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
        goto done;
    }

    CSN_ARRAY *arr = slot->array;
    uint32_t ndim = arr->ndim;
    double shift_value = (double) *p->param_a;
    if (!IS_VALID_SHIFT(shift_value)) {
        res = csound->InitError(csound, "[csnarray] Shift %g is invalid; shift must be a finite integer in %d..%d", shift_value, INT32_MIN, INT32_MAX);
        goto done;
    }
    int32_t shift = (int32_t) shift_value;
    double axis_value = (double) *p->param_b;
    CSN_AXIS_SPEC axis_spec = csn_normalize_axis_value(axis_value, ndim);
    if (axis_spec.kind != CSN_AXIS_INDEX) {
        res = csound->InitError(csound, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: finite integers %d..%u)", axis_value, ndim, -(int32_t) ndim, ndim - 1);
        goto done;
    }
    int32_t axis_roll = (int32_t) axis_spec.index;

    /* Sized to the source's capacity, not its current length: the source can
       grow that far without reallocating, and a marked source must find the
       scratch already big enough at perf time. */
    size_t capacity = arr->capacity * (size_t) arr->itype;
    double *data = csound->Calloc(csound, sizeof(double) * capacity);
    if (data == NULL) {
        res = csound->InitError(csound, "[csnarray] Out of memory: allocation of %zu bytes failed", (size_t) (sizeof(double) * arr->size * arr->itype));
        goto done;
    }
    p->scratch.scratch = data;
    p->scratch.scratch_capacity = capacity;

    if (rollaxis_assign_value(arr, NULL, p->scratch.scratch, arr->shape, ndim, shift, axis_roll) != OK) {
        res = csound->InitError(csound, "[csnarray] Invalid roll iterator layout");
        goto done;
    }
    memcpy(arr->data, p->scratch.scratch, sizeof(double) * arr->size * arr->itype);
    SET_KDATA_WITH_ID_BEGIN(p, reg, arr->shape, ndim, arr->itype, source_handle);
    p->k_data.prev_size = arr->size;
    p->k_data.prev_axis_u = axis_roll;
    p->k_data.prev_roll_shift = shift;

    PUBLISH_INPLACE_WRITE(&p->k_data, source_handle, arr, false, false, false);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_rollaxis_in_k(CSOUND *csound, CSN_FLIP_ROLL_IN *p) {
    CHECK_REG_HANDLE(csound, &p->h, p->k_data.registry, p->k_data.owned_handle);
    uint32_t source_handle = p->source_handle->id;
    CSN_REGISTRY *reg = p->k_data.registry;

    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
    }

    CSN_ARRAY *arr = slot->array;
    uint32_t ndim = arr->ndim;
    double shift_value = (double) *p->param_a;
    if (!IS_VALID_SHIFT(shift_value)) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Shift %g is invalid; shift must be a finite integer in %d..%d", shift_value, INT32_MIN, INT32_MAX);
    }
    int32_t shift = (int32_t) shift_value;
    double axis_value = (double) *p->param_b;
    CSN_AXIS_SPEC axis_spec = csn_normalize_axis_value(axis_value, ndim);
    if (axis_spec.kind != CSN_AXIS_INDEX) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: finite integers %d..%u)", axis_value, ndim, -(int32_t) ndim, ndim - 1);
    }
    int32_t axis_roll = (int32_t) axis_spec.index;

    bool is_changed = (shift != p->k_data.prev_roll_shift) || (axis_roll != p->k_data.prev_axis_u);
    res = CHECK_IF_REALLOC_IN(csound, &p->h, &p->k_data, arr, source_handle, &p->scratch, ndim, arr->itype, is_changed, slot->rt_locked);
    if (res != OK) {
        res = res == NOTOK ? OK : res;
        goto done;
    }

    if (rollaxis_assign_value(arr, NULL, p->scratch.scratch, arr->shape, ndim, shift, axis_roll) != OK) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Invalid roll iterator layout");
        goto done;
    }
    SET_KDATA_NO_ID_END(p, arr->shape, ndim, arr->itype);
    memcpy(arr->data, p->scratch.scratch, sizeof(double) * arr->size * arr->itype);
    p->k_data.prev_size = arr->size;
    p->k_data.prev_axis_u = axis_roll;
    p->k_data.prev_roll_shift = shift;

    PUBLISH_INPLACE_WRITE(&p->k_data, source_handle, arr, false, false, false);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}
