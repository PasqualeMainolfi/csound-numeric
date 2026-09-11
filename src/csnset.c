#include "csnregistry.h"
#include "csnum.h"
#include "csnset.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>


void binary_search(size_t *index, bool *founded, size_t low, const double *data, double value, size_t size) {
    size_t left = low;
    size_t right = size;

    while (left < right) {
        size_t center = left + (right - left) / 2;
        if (compare_double(&data[center], &value) < 0) {
            left = center + 1;
        } else {
            right = center;
        }
    }

    if (index != NULL) *index = left;
    if (founded != NULL) {
        *founded = left < size && compare_double(&data[left], &value) == 0;
    }
}

size_t get_and_count_unique_double(double *temp, size_t size) {
    qsort(temp, size, sizeof(double), compare_double);
    size_t count = 0;
    for (size_t i = 0; i < size; ++i) {
        if (i == 0 || compare_double(&temp[i], &temp[i - 1]) != 0) {
            temp[count++] = temp[i];
        }
    }
    return count;
}

int32_t csnarray_likeset_deinit(CSOUND *csound, CSNSET_UNARYOP *p) {
    deinit_scratch(csound, &p->buffer);
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

int32_t csnarray_likeset(CSOUND *csound, CSNSET_UNARYOP *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    const char *err = NULL;
    double *buffer = NULL;

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = slot->array;
    if (source_arr->itype == CSN_COMPLEX) {
        res = csound->InitError(csound, "[csnarray] Set operation requires real array");
        goto done;
    }

    size_t buffer_capacity = source_arr->size == 0 ? 1 : source_arr->size;
    buffer = csound->Calloc(csound, sizeof(double) * buffer_capacity);
    if (buffer == NULL) {
        res = csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
        goto done;
    }

    memcpy(buffer, source_arr->data, sizeof(double) * source_arr->size);
    size_t cunique = get_and_count_unique_double(buffer, source_arr->size);

    uint32_t new_dim = 1U;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    new_shape[0] = (uint32_t) cunique;

    if (create_csnarray_locked(csound, reg, &p->h, new_dim, new_shape, &p->array, p->handle, &source_handle, 1U, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    SET_ARRAY_KIND(p->array, CSNSET);
    memcpy(p->array->data, buffer, sizeof(double) * cunique);

done:
    csound->Free(csound, buffer);
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_likeset_k_init(CSOUND *csound, CSNSET_UNARYOP *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = slot->array;
    if (source_arr->itype == CSN_COMPLEX) {
        res = csound->InitError(csound, "[csnarray] Set operation requires real array");
        goto done;
    }

    /* The source can grow to its capacity without reallocating; reserving that
       much keeps a k-rate pass from growing this on a marked path. */
    size_t bcap = source_arr->capacity == 0 ? 1 : source_arr->capacity;
    double *buffer = csound->Calloc(csound, sizeof(double) * bcap);
    if (buffer == NULL) {
        res = csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
        goto done;
    }

    memcpy(buffer, source_arr->data, sizeof(double) * source_arr->size);
    size_t cunique = get_and_count_unique_double(buffer, source_arr->size);
    p->buffer.scratch = buffer;
    p->buffer.scratch_capacity = bcap;

    uint32_t new_dim = 1U;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    new_shape[0] = (uint32_t) cunique;

    if (create_csnarray_locked(csound, reg, &p->h, new_dim, new_shape, &p->array, p->handle, &source_handle, 1U, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    SET_ARRAY_KIND(p->array, CSNSET);
    double *buffer_copy = (double *) p->buffer.scratch;
    memcpy(p->array->data, buffer_copy, sizeof(double) * cunique);
    SET_KDATA_BEGIN(p, reg);
    set_array_version(&p->k_data.prev_output_version, &p->array->version);
    set_array_version(&p->k_data.prev_source_version, &source_arr->version);
    p->is_published = false;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_likeset_k(CSOUND *csound, CSNSET_UNARYOP *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    uint32_t owned_handle = p->k_data.owned_handle;
    CHECK_REG_HANDLE(csound, &p->h, reg, owned_handle);

    uint32_t source_handle = p->source_handle->id;
    int32_t res = OK;
    const char *err = NULL;

    res = CHECK_SELF_ALIAS(csound, &p->h, &p->k_data, source_handle, 0);
    if (res != OK) return res;

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
        return csound->PerfError(csound, &p->h, "[csnarray] Set operation requires real array");
    }

    if (p->is_published) {
        bool is_same_source = is_same_array_version(&p->k_data.prev_source_version, &source_arr->version);
        bool is_same_result = false;
        CSN_SLOT *slot_res = get_slot(reg, owned_handle);
        if (slot_res != NULL) {
            is_same_result = is_same_array_version(&p->k_data.prev_output_version, &slot_res->array->version);
        }
        if (is_same_source && is_same_result) {
            p->handle->id = owned_handle;
            goto done;
        }
    }

    size_t source_size = source_arr->size;
    res = csn_scratch_reserve(csound, &p->h, csn_slot_rt_locked(reg, owned_handle), &p->buffer, source_size, sizeof(double));
    if (res != OK) goto done;

    memcpy(p->buffer.scratch, source_arr->data, sizeof(double) * source_size);
    double *buffer_temp = (double *) p->buffer.scratch;
    size_t cunique = get_and_count_unique_double(buffer_temp, source_size);

    uint32_t new_dim = 1U;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    new_shape[0] = (uint32_t) cunique;

    size_t req_size = 0;
    if (get_array_size_from_shape(&req_size, new_dim, new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSN_ARRAY *arr = NULL;
    size_t logical_size = req_size;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, new_dim, new_shape, logical_size, CSN_REAL, err);
    if (res != OK) goto done;
    p->array = arr;

    SET_ARRAY_KIND(p->array, CSNSET);
    double *buffer_copy = (double *) p->buffer.scratch;
    memcpy(p->array->data, buffer_copy, sizeof(double) * cunique);
    SET_KDATA_END(p, new_shape, new_dim, CSN_REAL);
    set_array_version(&p->k_data.prev_output_version, &p->array->version);
    set_array_version(&p->k_data.prev_source_version, &source_arr->version);
    p->is_published = true;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_unlikeset(CSOUND *csound, CSNSET_UNARYOP_IN *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    uint32_t source_handle = p->source_handle->id;
    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
    }
    SET_ARRAY_KIND(slot->array, CSNARR);
    p->k_data.registry = reg;
    csound->UnlockMutex(reg->mutex);
    return OK;
}

int32_t csnarray_unlikeset_k_init(CSOUND *csound, CSNSET_UNARYOP_IN *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    p->k_data.registry = reg;
    return OK;
}

int32_t csnarray_unlikeset_k(CSOUND *csound, CSNSET_UNARYOP_IN *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);
    CHECK_KTRIG(p->arg_a);
    uint32_t source_handle = p->source_handle->id;
    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
    }
    SET_ARRAY_KIND(slot->array, CSNARR);
    csound->UnlockMutex(reg->mutex);
    return OK;
}

static int32_t set_check_body(CSOUND *csound, OPDS *perf_h, CSN_ARRAY **source_array, CSN_REGISTRY *reg, uint32_t source_handle) {
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
    }

    CSN_ARRAY *source_arr = slot->array;
    if (source_arr->itype == CSN_COMPLEX) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Set operation requires real array");
    }

    if (source_arr->ndim != 1U) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Set operation requires 1-D real array");
    }

    if (source_arr->kind != CSNSET) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Set operation requires set-array. Use csnlikeset first to mark array as set csn-array");
    }

    if (source_arr->set_data_version != source_arr->version.data_version) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Set-array was modified outside the set API. Use csnlikeset again to restore sorted unique form");
    }

    *source_array = source_arr;
    return OK;
}

static int32_t set_insert_remove_helper(CSOUND *csound, CSNSET_UNARYOP_IN *p, OPDS *perf_h, bool is_insert) {
    CSN_REGISTRY *reg = perf_h == NULL ? get_registry(csound) : p->k_data.registry;
    CHECK_REGISTRY(csound, perf_h, reg);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    if (perf_h != NULL) CHECK_KTRIG(p->arg_b);
    double value = (double) *p->arg_a;

    csound->LockMutex(reg->mutex);
    CSN_ARRAY *source_arr = NULL;
    res = set_check_body(csound, perf_h, &source_arr, reg, source_handle);
    if (res != OK) goto done;

    if (perf_h != NULL && p->is_published) {
        bool is_same_source = is_same_array_version(&p->k_data.prev_source_version, &source_arr->version);
        bool is_same_value = compare_double(&p->k_data.prev_scalar_param, &value) == 0;
        if (is_same_source && is_same_value) goto done;
    }

    size_t index = 0;
    bool founded = false;
    binary_search(&index, &founded, 0, source_arr->data, value, source_arr->size);

    if (is_insert) {
        if (founded) goto done;
        if (source_arr->size >= CSN_MAX_ELEMS) {
            res = CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Set insertion would exceed the maximum element count: array already holds %zu of %zu elements", source_arr->size, (size_t) CSN_MAX_ELEMS);
            goto done;
        }
        size_t new_size = source_arr->size + 1;
        res = ensure_mutation_capacity(csound, perf_h, source_arr, new_size, csn_slot_rt_locked(reg, source_handle));
        if (res != OK) goto done;
        source_arr->shape[0] = (uint32_t) new_size;
        source_arr->size = new_size;
        memmove(source_arr->data + index + 1, source_arr->data + index, sizeof(double) * ((new_size - 1) - index));
        source_arr->data[index] = value;
    } else {
        if (!founded) goto done;
        size_t new_size = source_arr->size - 1;
        source_arr->shape[0] = (uint32_t) new_size;
        source_arr->size = new_size;
        memmove(source_arr->data + index, source_arr->data + index + 1, sizeof(double) * (new_size - index));
    }

    PUBLISH_INPLACE_WRITE(&p->k_data, source_handle, source_arr, true, false, false);
    SET_ARRAY_KIND(source_arr, CSNSET);
    SET_KDATA_NO_ID_END(p, source_arr->shape, 1U, CSN_REAL);
    p->k_data.prev_scalar_param = value;
    p->is_published = true;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}


int32_t csnarray_setinsert(CSOUND *csound, CSNSET_UNARYOP_IN *p) {
    return set_insert_remove_helper(csound, p, NULL, true);
}

int32_t csnarray_setremove(CSOUND *csound, CSNSET_UNARYOP_IN *p) {
    return set_insert_remove_helper(csound, p, NULL, false);
}

int32_t csnarray_setinsertremove_k_init(CSOUND *csound, CSNSET_UNARYOP_IN *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;
    int32_t res = OK;

    csound->LockMutex(reg->mutex);
    CSN_ARRAY *source_arr = NULL;
    res = set_check_body(csound, NULL, &source_arr, reg, source_handle);
    if (res != OK) goto done;

    SET_KDATA_WITH_ID_BEGIN(p, reg, source_arr->shape, 1U, CSN_REAL, source_handle);
    p->k_data.prev_source_handle = source_handle;
    set_array_version(&p->k_data.prev_source_version, &source_arr->version);
    p->is_published = false;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_setinsert_k(CSOUND *csound, CSNSET_UNARYOP_IN *p) {
    return set_insert_remove_helper(csound, p, &p->h, true);
}

int32_t csnarray_setremove_k(CSOUND *csound, CSNSET_UNARYOP_IN *p) {
    return set_insert_remove_helper(csound, p, &p->h, false);
}

int32_t csnarray_setcontains(CSOUND *csound, CSNSET_BINARYOP_SCALAR *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    double value = (double) *p->scalar;

    csound->LockMutex(reg->mutex);
    CSN_ARRAY *source_arr = NULL;
    res = set_check_body(csound, NULL, &source_arr, reg, source_handle);
    if (res != OK) goto done;

    size_t index = 0;
    bool founded = false;
    binary_search(&index, &founded, 0, source_arr->data, value, source_arr->size);

    *p->value = founded ? FL(1.0) : FL(0.0);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_setcontains_k_init(CSOUND *csound, CSNSET_BINARYOP_SCALAR *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    double value = (double) *p->scalar;

    csound->LockMutex(reg->mutex);
    CSN_ARRAY *source_arr = NULL;
    res = set_check_body(csound, NULL, &source_arr, reg, source_handle);
    if (res != OK) goto done;

    bool founded = false;
    binary_search(NULL, &founded, 0, source_arr->data, value, source_arr->size);
    *p->value = founded ? FL(1.0) : FL(0.0);

    SET_KDATA_WITH_ID_BEGIN(p, reg, source_arr->shape, 1U, CSN_REAL, source_handle);
    set_array_version(&p->k_data.prev_source_version, &source_arr->version);
    p->k_data.prev_scalar_param = value;
    p->is_published = true;
    p->prev_result = founded;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_setcontains_k(CSOUND *csound, CSNSET_BINARYOP_SCALAR *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    *p->value = (MYFLT) ((double) p->prev_result);
    CHECK_KTRIG(p->trig);
    double value = (double) *p->scalar;

    csound->LockMutex(reg->mutex);
    CSN_ARRAY *source_arr = NULL;
    res = set_check_body(csound, &p->h, &source_arr, reg, source_handle);
    if (res != OK) goto done;

    if (p->is_published) {
        bool is_same_source = is_same_array_version(&p->k_data.prev_source_version, &source_arr->version);
        bool is_same_scalar = compare_double(&p->k_data.prev_scalar_param, &value) == 0;
        if (is_same_source && is_same_scalar) {
            *p->value = (MYFLT) ((double) p->prev_result);
            goto done;
        }
    }

    size_t index = 0;
    bool founded = false;
    binary_search(&index, &founded, 0, source_arr->data, value, source_arr->size);

    double result_temp = (double) founded;
    *p->value = (MYFLT) result_temp;

    SET_KDATA_NO_ID_END(p, source_arr->shape, 1U, CSN_REAL);
    set_array_version(&p->k_data.prev_source_version, &source_arr->version);
    p->k_data.prev_scalar_param = value;
    p->prev_result = founded;
    p->is_published = true;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t set_binaryop_body(CSOUND *csound, OPDS *perf_h, CSN_ARRAY **source_array_a, CSN_ARRAY **source_array_b, CSN_REGISTRY *reg, uint32_t source_handle_a, uint32_t source_handle_b) {
    CSN_SLOT *slot_a = get_slot(reg, source_handle_a);
    if (slot_a == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle_a);
    }
    CSN_SLOT *slot_b = get_slot(reg, source_handle_b);
    if (slot_b == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle_b);
    }

    CSN_ARRAY *source_arr_a = slot_a->array;
    CSN_ARRAY *source_arr_b = slot_b->array;
    if (source_arr_a->itype == CSN_COMPLEX || source_arr_b->itype == CSN_COMPLEX) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Set operation requires real array");
    }

    if (source_arr_a->ndim != 1U || source_arr_b->ndim != 1U) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Set operation requires 1-D real array");
    }

    if (source_arr_a->kind != CSNSET || source_arr_b->kind != CSNSET) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Set operation requires set-array. Use csnlikeset first to mark array as set csn-array");
    }

    if (source_arr_a->set_data_version != source_arr_a->version.data_version
        || source_arr_b->set_data_version != source_arr_b->version.data_version) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Set-array was modified outside the set API. Use csnlikeset again to restore sorted unique form");
    }

    *source_array_a = source_arr_a;
    *source_array_b = source_arr_b;
    return OK;
}

static int32_t set_binaryop_helper(CSOUND *csound, CSNSET_BINARYOP *p, CSNSET_OPS_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle_a = p->source_handle_a->id;
    uint32_t source_handle_b = p->source_handle_b->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);
    CSN_ARRAY *source_arr_a = NULL;
    CSN_ARRAY *source_arr_b = NULL;
    res = set_binaryop_body(csound, NULL, &source_arr_a, &source_arr_b, reg, source_handle_a, source_handle_b);
    if (res != OK) goto done;

    uint32_t new_shape[CSN_MAX_DIMS] = {0};

    size_t source_a_size = source_arr_a->size;
    size_t source_b_size = source_arr_b->size;
    /* Room for both operands at their full capacity, which is as far as either
       can grow without reallocating: a k-rate pass on a marked path then
       never has to grow this. */
    size_t temp_cap = source_arr_a->capacity + source_arr_b->capacity;
    size_t bcap = temp_cap == 0 ? 1 : temp_cap;

    double *temp_buffer = csound->Calloc(csound, sizeof(double) * bcap);
    if (temp_buffer == NULL) {
        res = csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
        goto done;
    }

    p->buffer.scratch = temp_buffer;
    p->buffer.scratch_capacity = bcap;
    double *buffer = (double *) p->buffer.scratch;

    size_t count_items = 0;
    switch (mode) {
        case CSNSET_UNION: {
            memcpy(buffer, source_arr_a->data, sizeof(double) * source_a_size);
            size_t count = source_a_size;
            for (size_t i = 0; i < source_b_size; i++) {
                double value = source_arr_b->data[i];
                bool founded = false;
                size_t index = 0;
                binary_search(&index, &founded, 0, buffer, value, count);
                if (!founded) {
                    memmove(buffer + index + 1, buffer + index, sizeof(double) * (count - index));
                    buffer[index] = value;
                    count++;
                }
            }
            count_items = count;
            break;
        }
        case CSNSET_INTERSECT: {
                size_t count = 0;
                for (size_t i = 0; i < source_a_size; i++) {
                    double value = source_arr_a->data[i];
                    bool founded = false;
                    binary_search(NULL, &founded, 0, source_arr_b->data, value, source_b_size);
                    if (founded) {
                        buffer[count] = value;
                        count++;
                    }
                }
                count_items = count;
            }
            break;
        case CSNSET_DIFF: {
                size_t count = 0;
                for (size_t i = 0; i < source_a_size; i++) {
                    double value = source_arr_a->data[i];
                    bool founded = false;
                    binary_search(NULL, &founded, 0, source_arr_b->data, value, source_b_size);
                    if (!founded) {
                        buffer[count] = value;
                        count++;
                    }
                }
                count_items = count;
            }
            break;
        case CSNSET_SYMDIFF: {
                size_t count = 0;
                for (size_t i = 0; i < source_a_size; i++) {
                    double value_a = source_arr_a->data[i];
                    bool founded_a = false;
                    binary_search(NULL, &founded_a, 0, source_arr_b->data, value_a, source_b_size);
                    if (!founded_a) {
                        buffer[count] = value_a;
                        count++;
                    }
                }
                for (size_t i = 0; i < source_b_size; i++) {
                    double value_b = source_arr_b->data[i];
                    bool founded_b = false;
                    binary_search(NULL, &founded_b, 0, source_arr_a->data, value_b, source_a_size);
                    if (!founded_b) {
                        size_t index = 0;
                        binary_search(&index, &founded_b, 0, buffer, value_b, count);
                        memmove(buffer + index + 1, buffer + index, sizeof(double) * (count - index));
                        count++;
                        buffer[index] = value_b;
                    }
                }
                count_items = count;
            }
            break;
        default:
            break;
    }

    new_shape[0] = (uint32_t) count_items;
    uint32_t protect[2] = { source_handle_a, source_handle_b };
    if (create_csnarray_locked(csound, reg, &p->h, 1U, new_shape, &p->array, p->handle, protect, 2U, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }
    SET_ARRAY_KIND(p->array, CSNSET);

    if (count_items > 0) {
        memcpy(p->array->data, buffer, sizeof(double) * count_items);
    }

    SET_KDATA_BEGIN(p, reg);

    set_array_version(&p->k_data.prev_output_version, &p->array->version);
    set_array_version(&p->k_data.prev_source_version, &source_arr_a->version);
    set_array_version(&p->k_data.prev_source_version_b, &source_arr_b->version);
    p->is_published = false;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t set_binaryop_k_helper(CSOUND *csound, CSNSET_BINARYOP *p, CSNSET_OPS_MODE mode) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REG_HANDLE(csound, &p->h, reg, p->k_data.owned_handle);

    uint32_t source_handle_a = p->source_handle_a->id;
    uint32_t source_handle_b = p->source_handle_b->id;

    int32_t res = OK;
    const char *err = NULL;

    CHECK_KTRIG(p->trig);

    csound->LockMutex(reg->mutex);
    CSN_ARRAY *source_arr_a = NULL;
    CSN_ARRAY *source_arr_b = NULL;
    res = set_binaryop_body(csound, &p->h, &source_arr_a, &source_arr_b, reg, source_handle_a, source_handle_b);
    if (res != OK) goto done;

    if (p->is_published) {
        bool is_same_version_a = is_same_array_version(&p->k_data.prev_source_version, &source_arr_a->version);
        bool is_same_version_b = is_same_array_version(&p->k_data.prev_source_version_b, &source_arr_b->version);
        bool is_same_result = false;
        CSN_SLOT *res_slot = get_slot(reg, p->k_data.owned_handle);
        if (res_slot != NULL) {
            is_same_result = is_same_array_version(&p->k_data.prev_output_version, &res_slot->array->version);
        }

        if (is_same_version_a && is_same_version_b && is_same_result) {
            p->handle->id = p->k_data.owned_handle;
            goto done;
        }
    }

    uint32_t new_ndim = 1U;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};

    size_t source_a_size = source_arr_a->size;
    size_t source_b_size = source_arr_b->size;
    /* Checked on every pass, the first one included: the operands may have
       grown since init. */
    res = csn_scratch_reserve(csound, &p->h, csn_slot_rt_locked(reg, p->k_data.owned_handle), &p->buffer, source_a_size + source_b_size, sizeof(double));
    if (res != OK) goto done;

    double *buffer = (double *) p->buffer.scratch;

    size_t count_items = 0;
    switch (mode) {
        case CSNSET_UNION: {
            memcpy(buffer, source_arr_a->data, sizeof(double) * source_a_size);
            size_t count = source_a_size;
            for (size_t i = 0; i < source_b_size; i++) {
                double value = source_arr_b->data[i];
                bool founded = false;
                size_t index = 0;
                binary_search(&index, &founded, 0, buffer, value, count);
                if (!founded) {
                    memmove(buffer + index + 1, buffer + index, sizeof(double) * (count - index));
                    buffer[index] = value;
                    count++;
                }
            }
            count_items = count;
            break;
        }
        case CSNSET_INTERSECT: {
                size_t count = 0;
                for (size_t i = 0; i < source_a_size; i++) {
                    double value = source_arr_a->data[i];
                    bool founded = false;
                    binary_search(NULL, &founded, 0, source_arr_b->data, value, source_b_size);
                    if (founded) {
                        buffer[count] = value;
                        count++;
                    }
                }
                count_items = count;
            }
            break;
        case CSNSET_DIFF: {
                size_t count = 0;
                for (size_t i = 0; i < source_a_size; i++) {
                    double value = source_arr_a->data[i];
                    bool founded = false;
                    binary_search(NULL, &founded, 0, source_arr_b->data, value, source_b_size);
                    if (!founded) {
                        buffer[count] = value;
                        count++;
                    }
                }
                count_items = count;
            }
            break;
        case CSNSET_SYMDIFF: {
                size_t count = 0;
                for (size_t i = 0; i < source_a_size; i++) {
                    double value_a = source_arr_a->data[i];
                    bool founded_a = false;
                    binary_search(NULL, &founded_a, 0, source_arr_b->data, value_a, source_b_size);
                    if (!founded_a) {
                        buffer[count] = value_a;
                        count++;
                    }
                }
                for (size_t i = 0; i < source_b_size; i++) {
                    double value_b = source_arr_b->data[i];
                    bool founded_b = false;
                    binary_search(NULL, &founded_b, 0, source_arr_a->data, value_b, source_a_size);
                    if (!founded_b) {
                        size_t index = 0;
                        binary_search(&index, &founded_b, 0, buffer, value_b, count);
                        memmove(buffer + index + 1, buffer + index, sizeof(double) * (count - index));
                        count++;
                        buffer[index] = value_b;
                    }
                }
                count_items = count;
            }
            break;
        default:
            break;
    }

    new_shape[0] = (uint32_t) count_items;
    size_t req_size = 0;
    if (get_array_size_from_shape(&req_size, 1U, new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSN_ARRAY *arr = NULL;
    size_t logical_size = req_size;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, 1U, new_shape, logical_size, CSN_REAL, err);
    if (res != OK) goto done;
    p->array = arr;

    if (count_items > 0) {
        memcpy(p->array->data, buffer, sizeof(double) * count_items);
    }
    SET_ARRAY_KIND(p->array, CSNSET);

    SET_KDATA_END(p, new_shape, new_ndim, CSN_REAL);
    set_array_version(&p->k_data.prev_output_version, &p->array->version);
    set_array_version(&p->k_data.prev_source_version, &source_arr_a->version);
    set_array_version(&p->k_data.prev_source_version_b, &source_arr_b->version);
    p->is_published = true;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}


int32_t csnarray_set_binaryop_deinit(CSOUND *csound, CSNSET_BINARYOP *p) {
    deinit_scratch(csound, &p->buffer);
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

int32_t csnarray_setunion(CSOUND *csound, CSNSET_BINARYOP *p) {
    return set_binaryop_helper(csound, p, CSNSET_UNION);
}

int32_t csnarray_setintersect(CSOUND *csound, CSNSET_BINARYOP *p) {
    return set_binaryop_helper(csound, p, CSNSET_INTERSECT);
}

int32_t csnarray_setdiff(CSOUND *csound, CSNSET_BINARYOP *p) {
    return set_binaryop_helper(csound, p, CSNSET_DIFF);
}

int32_t csnarray_setsymdiff(CSOUND *csound, CSNSET_BINARYOP *p) {
    return set_binaryop_helper(csound, p, CSNSET_SYMDIFF);
}

int32_t csnarray_setunion_k(CSOUND *csound, CSNSET_BINARYOP *p) {
    return set_binaryop_k_helper(csound, p, CSNSET_UNION);
}

int32_t csnarray_setintersect_k(CSOUND *csound, CSNSET_BINARYOP *p) {
    return set_binaryop_k_helper(csound, p, CSNSET_INTERSECT);
}

int32_t csnarray_setdiff_k(CSOUND *csound, CSNSET_BINARYOP *p) {
    return set_binaryop_k_helper(csound, p, CSNSET_DIFF);
}

int32_t csnarray_setsymdiff_k(CSOUND *csound, CSNSET_BINARYOP *p) {
    return set_binaryop_k_helper(csound, p, CSNSET_SYMDIFF);
}

static int32_t set_binaryop_predicate_helper(CSOUND *csound, CSNSET_BINARYOP_PREDICATE *p, CSNSET_OPS_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle_a = p->source_handle_a->id;
    uint32_t source_handle_b = p->source_handle_b->id;

    int32_t res = OK;

    csound->LockMutex(reg->mutex);
    CSN_ARRAY *source_arr_a = NULL;
    CSN_ARRAY *source_arr_b = NULL;
    res = set_binaryop_body(csound, NULL, &source_arr_a, &source_arr_b, reg, source_handle_a, source_handle_b);
    if (res != OK) goto done;

    size_t source_a_size = source_arr_a->size;
    size_t source_b_size = source_arr_b->size;

    bool result = true;
    switch (mode) {
        case CSNSET_SUBSET:
            for (size_t i = 0; i < source_a_size; i++) {
                double value = source_arr_a->data[i];
                bool founded = false;
                binary_search(NULL, &founded, 0, source_arr_b->data, value, source_b_size);
                if (!founded) {
                    result = false;
                    break;
                }
            }
            break;
        case CSNSET_SUPERSET:
            for (size_t i = 0; i < source_b_size; i++) {
                double value = source_arr_b->data[i];
                bool founded = false;
                binary_search(NULL, &founded, 0, source_arr_a->data, value, source_a_size);
                if (!founded) {
                    result = false;
                    break;
                }
            }
            break;
        case CSNSET_DISJOINT:
            for (size_t i = 0; i < source_a_size; i++) {
                double value = source_arr_a->data[i];
                bool founded = false;
                binary_search(NULL, &founded, 0, source_arr_b->data, value, source_b_size);
                if (founded) {
                    result = false;
                    break;
                }
            }
            break;
        case CSNSET_EQUAL:
            if (source_a_size != source_b_size) {
                result = false;
                break;
            }
            for (size_t i = 0; i < source_a_size; i++) {
                double value_a = source_arr_a->data[i];
                double value_b = source_arr_b->data[i];
                if (compare_double(&value_a, &value_b) != 0) {
                    result = false;
                    break;
                }
            }
            break;
        default:
            break;
    }

    p->registry = reg;
    set_array_version(&p->prev_source_version_a, &source_arr_a->version);
    set_array_version(&p->prev_source_version_b, &source_arr_b->version);
    *p->result = result ? FL(1.0) : FL(0.0);
    p->prev_result = (double) result;
    p->is_published = false;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t set_binaryop_predicate_k_helper(CSOUND *csound, CSNSET_BINARYOP_PREDICATE *p, CSNSET_OPS_MODE mode) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    uint32_t source_handle_a = p->source_handle_a->id;
    uint32_t source_handle_b = p->source_handle_b->id;

    int32_t res = OK;

    CHECK_KTRIG(p->trig);

    csound->LockMutex(reg->mutex);
    CSN_ARRAY *source_arr_a = NULL;
    CSN_ARRAY *source_arr_b = NULL;
    res = set_binaryop_body(csound, &p->h, &source_arr_a, &source_arr_b, reg, source_handle_a, source_handle_b);
    if (res != OK) goto done;

    if (p->is_published) {
        bool is_same_version_a = is_same_array_version(&p->prev_source_version_a, &source_arr_a->version);
        bool is_same_version_b = is_same_array_version(&p->prev_source_version_b, &source_arr_b->version);
        if (is_same_version_a && is_same_version_b) {
            *p->result = (MYFLT) p->prev_result;
            goto done;
        }
    }

    size_t source_a_size = source_arr_a->size;
    size_t source_b_size = source_arr_b->size;
    bool result = true;
    switch (mode) {
        case CSNSET_SUBSET:
            for (size_t i = 0; i < source_a_size; i++) {
                double value = source_arr_a->data[i];
                bool founded = false;
                binary_search(NULL, &founded, 0, source_arr_b->data, value, source_b_size);
                if (!founded) {
                    result = false;
                    break;
                }
            }
            break;
        case CSNSET_SUPERSET:
            for (size_t i = 0; i < source_b_size; i++) {
                double value = source_arr_b->data[i];
                bool founded = false;
                binary_search(NULL, &founded, 0, source_arr_a->data, value, source_a_size);
                if (!founded) {
                    result = false;
                    break;
                }
            }
            break;
        case CSNSET_DISJOINT:
            for (size_t i = 0; i < source_a_size; i++) {
                double value = source_arr_a->data[i];
                bool founded = false;
                binary_search(NULL, &founded, 0, source_arr_b->data, value, source_b_size);
                if (founded) {
                    result = false;
                    break;
                }
            }
            break;
        case CSNSET_EQUAL:
            if (source_a_size != source_b_size) {
                result = false;
                break;
            }
            for (size_t i = 0; i < source_a_size; i++) {
                double value_a = source_arr_a->data[i];
                double value_b = source_arr_b->data[i];
                if (compare_double(&value_a, &value_b) != 0) {
                    result = false;
                    break;
                }
            }
            break;
        default:
            break;
    }

    p->registry = reg;
    set_array_version(&p->prev_source_version_a, &source_arr_a->version);
    set_array_version(&p->prev_source_version_b, &source_arr_b->version);
    *p->result = result ? FL(1.0) : FL(0.0);
    p->prev_result = (double) result;
    p->is_published = true;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_set_binaryop_p_deinit(CSOUND *csound, CSNSET_BINARYOP_PREDICATE *p) {
    deinit_scratch(csound, &p->buffer);
    return OK;
}

int32_t csnarray_setissubset(CSOUND *csound, CSNSET_BINARYOP_PREDICATE *p) {
    return set_binaryop_predicate_helper(csound, p, CSNSET_SUBSET);
}

int32_t csnarray_setissuperset(CSOUND *csound, CSNSET_BINARYOP_PREDICATE *p) {
    return set_binaryop_predicate_helper(csound, p, CSNSET_SUPERSET);
}

int32_t csnarray_setisdisjoint(CSOUND *csound, CSNSET_BINARYOP_PREDICATE *p) {
    return set_binaryop_predicate_helper(csound, p, CSNSET_DISJOINT);
}

int32_t csnarray_setisequal(CSOUND *csound, CSNSET_BINARYOP_PREDICATE *p) {
    return set_binaryop_predicate_helper(csound, p, CSNSET_EQUAL);
}

int32_t csnarray_setissubset_k(CSOUND *csound, CSNSET_BINARYOP_PREDICATE *p) {
    return set_binaryop_predicate_k_helper(csound, p, CSNSET_SUBSET);
}

int32_t csnarray_setissuperset_k(CSOUND *csound, CSNSET_BINARYOP_PREDICATE *p) {
    return set_binaryop_predicate_k_helper(csound, p, CSNSET_SUPERSET);
}

int32_t csnarray_setisdisjoint_k(CSOUND *csound, CSNSET_BINARYOP_PREDICATE *p) {
    return set_binaryop_predicate_k_helper(csound, p, CSNSET_DISJOINT);
}

int32_t csnarray_setisequal_k(CSOUND *csound, CSNSET_BINARYOP_PREDICATE *p) {
    return set_binaryop_predicate_k_helper(csound, p, CSNSET_EQUAL);
}
