#include "csnregistry.h"
#include "csnum.h"
#include "csnum_internal.h"
#include "csnset.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>


void binary_search(size_t *index, bool *founded, size_t low, const double *data, double value, size_t size, bool right_side) {
    size_t left = low;
    size_t right = size;

    while (left < right) {
        size_t center = left + (right - left) / 2;
        int comp = compare_double(&data[center], &value);
        if (comp < 0 || (right_side && comp == 0)) {
            left = center + 1;
        } else {
            right = center;
        }
    }

    if (index != NULL) *index = left;

    if (founded != NULL) {
        if (!right_side) {
            *founded = left < size && compare_double(&data[left], &value) == 0;
        } else {
            *founded = left > low && compare_double(&data[left - 1], &value) == 0;
        }
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
    return OK;
}

/* Writes a set result into its output. The result is always computed into a
   buffer first, from operands left untouched, so an output that is one of its
   own operands (A = csnsetunion(A, B), A = csnlikeset(A)) can take it in place:
   it gets the result's layout, and, since prepare_output left it alone, a new
   data version. The set mark is stamped last, on the final data version, or
   the next set operation would read the array as modified outside the set API. */
static int32_t set_store_result(CSOUND *csound, OPDS *perf_h, CSN_ARRAY *out, bool aliased, const double *values, size_t count) {
    if (aliased) {
        int32_t res = ensure_mutation_capacity(csound, perf_h, out, count > 0 ? count : 1, out->rt_locked);
        if (res != OK) return res;
        uint32_t shape[CSN_MAX_DIMS] = { (uint32_t) count };
        set_csnarray_layout(out, 1U, shape, count, CSN_REAL);
    }
    if (count > 0) memcpy(out->data, values, sizeof(double) * count);
    if (aliased) update_array_data_version(&out->version);
    SET_ARRAY_KIND(out, CSNSET);
    return OK;
}

int32_t csnarray_likeset(CSOUND *csound, CSNSET_UNARYOP *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    double *buffer = NULL;

    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle }, 2);

    CSN_ARRAY *source_arr = p->source_handle;
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

    const CSN_ARRAY *ops[1] = { p->source_handle };
    CSNARR_RES out = prepare_output(csound, &p->h, NULL, reg, p->handle, new_dim, new_shape, CSN_REAL, ops, 1, CSN_ALIAS_CUSTOM);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    res = set_store_result(csound, NULL, p->handle, out.aliased, buffer, cunique);

done:
    if (buffer != NULL) csound->Free(csound, buffer);
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_likeset_k_init(CSOUND *csound, CSNSET_UNARYOP *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle }, 2);

    CSN_ARRAY *source_arr = p->source_handle;
    if (source_arr->itype == CSN_COMPLEX) {
        res = csound->InitError(csound, "[csnarray] Set operation requires real array");
        goto done;
    }

    /* The source can grow to its capacity without reallocating; reserving that
       much keeps a k-rate pass from growing this on a marked path. A reinit
       reuses the struct, so the previous buffer goes first. */
    deinit_scratch(csound, &p->buffer);
    size_t bcap = source_arr->capacity == 0 ? 1 : source_arr->capacity;
    double *buffer = csound->Calloc(csound, sizeof(double) * bcap);
    if (buffer == NULL) {
        res = csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
        goto done;
    }

    memcpy(buffer, source_arr->data, sizeof(double) * source_arr->size);
    size_t cunique = get_and_count_unique_double(buffer, source_arr->size);

    uint32_t new_dim = 1U;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    new_shape[0] = (uint32_t) cunique;

    p->buffer.scratch = buffer;
    p->buffer.scratch_capacity = bcap;

    const CSN_ARRAY *ops[1] = { p->source_handle };
    CSNARR_RES out = prepare_output(csound, &p->h, NULL, reg, p->handle, new_dim, new_shape, CSN_REAL, ops, 1, CSN_ALIAS_CUSTOM);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    /* idempotent, so an output that is its own source may take the set form
       here as well as on every pass */
    res = set_store_result(csound, NULL, p->handle, out.aliased, buffer, cunique);
    if (res != OK) goto done;
    SET_KDATA_BEGIN(p->k_data, reg, new_shape, new_dim, CSN_REAL);
    set_array_version(&p->k_data.prev_output_version, &p->handle->version);
    set_array_version(&p->k_data.prev_source_version, &source_arr->version);
    p->is_published = false;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_likeset_k(CSOUND *csound, CSNSET_UNARYOP *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    CHECK_KTRIG(p->trig);
    int32_t res = OK;

    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle }, 2);

    CSN_ARRAY *source_arr = p->source_handle;
    if (source_arr->itype == CSN_COMPLEX) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Set operation requires real array");
        goto done;
    }

    if (p->is_published) {
        bool is_same_source = is_same_array_version(&p->k_data.prev_source_version, &source_arr->version);
        bool is_same_result = is_same_array_version(&p->k_data.prev_output_version, &p->handle->version);
        if (is_same_source && is_same_result) goto done;
    }

    size_t source_size = source_arr->size;
    res = csn_scratch_reserve(csound, &p->h, p->handle->rt_locked, &p->buffer, source_size, sizeof(double));
    if (res != OK) goto done;

    memcpy(p->buffer.scratch, source_arr->data, sizeof(double) * source_size);
    double *buffer_temp = (double *) p->buffer.scratch;
    size_t cunique = get_and_count_unique_double(buffer_temp, source_size);

    uint32_t new_dim = 1U;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    new_shape[0] = (uint32_t) cunique;

    const CSN_ARRAY *ops[1] = { p->source_handle };
    CSNARR_RES out = prepare_output(csound, &p->h, &p->h, reg, p->handle, new_dim, new_shape, CSN_REAL, ops, 1, CSN_ALIAS_CUSTOM);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    res = set_store_result(csound, &p->h, p->handle, out.aliased, buffer_temp, cunique);
    if (res != OK) goto done;
    SET_KDATA_END(p->k_data, new_shape, new_dim, CSN_REAL);
    set_array_version(&p->k_data.prev_output_version, &p->handle->version);
    set_array_version(&p->k_data.prev_source_version, &source_arr->version);
    p->is_published = true;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_unlikeset(CSOUND *csound, CSNSET_UNARYOP_IN *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    p->k_data.registry = reg;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);
    SET_ARRAY_KIND(p->source_handle, CSNARR);
    if (pres.locked) csn_unlock(csound, reg);
    return OK;
}

int32_t csnarray_unlikeset_k_init(CSOUND *csound, CSNSET_UNARYOP_IN *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);
    csn_mark_krate(p->source_handle);
    p->k_data.registry = reg;
    if (pres.locked) csn_unlock(csound, reg);
    return OK;
}

int32_t csnarray_unlikeset_k(CSOUND *csound, CSNSET_UNARYOP_IN *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);
    CHECK_KTRIG(p->arg_a);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);
    SET_ARRAY_KIND(p->source_handle, CSNARR);
    if (pres.locked) csn_unlock(csound, reg);
    return OK;
}

static int32_t set_check_body(CSOUND *csound, OPDS *perf_h, CSN_ARRAY *source_arr) {
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

    return OK;
}

static int32_t set_insert_remove_helper(CSOUND *csound, CSNSET_UNARYOP_IN *p, OPDS *perf_h, bool is_insert) {
    CSN_REGISTRY *reg = perf_h == NULL ? get_registry(csound) : p->k_data.registry;
    CHECK_REGISTRY(csound, perf_h, reg);

    if (perf_h != NULL) CHECK_KTRIG(p->arg_b);
    int32_t res = OK;
    double value = (double) *p->arg_a;

    if (perf_h == NULL) stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);

    CSN_ARRAY *source_arr = p->source_handle;
    res = set_check_body(csound, perf_h, source_arr);
    if (res != OK) goto done;

    if (perf_h != NULL && p->is_published) {
        bool is_same_source = is_same_array_version(&p->k_data.prev_source_version, &source_arr->version);
        bool is_same_value = compare_double(&p->k_data.prev_scalar_param, &value) == 0;
        if (is_same_source && is_same_value) goto done;
    }

    size_t index = 0;
    bool founded = false;
    binary_search(&index, &founded, 0, source_arr->data, value, source_arr->size, false);

    if (is_insert) {
        if (founded) goto done;
        if (source_arr->size >= CSN_MAX_ELEMS) {
            res = CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Set insertion would exceed the maximum element count: array already holds %zu of %zu elements", source_arr->size, (size_t) CSN_MAX_ELEMS);
            goto done;
        }
        size_t new_size = source_arr->size + 1;
        res = ensure_mutation_capacity(csound, perf_h, source_arr, new_size, source_arr->rt_locked);
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

    /* Both rates changed the data and the extent: a new version either way,
       then the set mark on it. The k form also records its own write. */
    if (perf_h != NULL) {
        PUBLISH_INPLACE_WRITE(&p->k_data, source_arr, true, false, false);
        SET_KDATA_END(p->k_data, source_arr->shape, 1U, CSN_REAL);
        p->k_data.prev_scalar_param = value;
        p->is_published = true;
    } else {
        update_array_data_version(&source_arr->version);
        update_array_layout_version(&source_arr->version, true, false, false);
    }
    SET_ARRAY_KIND(source_arr, CSNSET);

done:
    if (pres.locked) csn_unlock(csound, reg);
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

    int32_t res = OK;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);
    csn_mark_krate(p->source_handle);

    CSN_ARRAY *source_arr = p->source_handle;
    res = set_check_body(csound, NULL, source_arr);
    if (res != OK) goto done;

    SET_KDATA_BEGIN(p->k_data, reg, source_arr->shape, 1U, CSN_REAL);
    set_array_version(&p->k_data.prev_source_version, &source_arr->version);
    p->is_published = false;

done:
    if (pres.locked) csn_unlock(csound, reg);
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

    int32_t res = OK;
    double value = (double) *p->scalar;

    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);

    CSN_ARRAY *source_arr = p->source_handle;
    res = set_check_body(csound, NULL, source_arr);
    if (res != OK) goto done;

    size_t index = 0;
    bool founded = false;
    binary_search(&index, &founded, 0, source_arr->data, value, source_arr->size, false);

    *p->value = founded ? FL(1.0) : FL(0.0);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_setcontains_k_init(CSOUND *csound, CSNSET_BINARYOP_SCALAR *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    double value = (double) *p->scalar;

    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);

    CSN_ARRAY *source_arr = p->source_handle;
    res = set_check_body(csound, NULL, source_arr);
    if (res != OK) goto done;

    SET_KDATA_BEGIN(p->k_data, reg, source_arr->shape, 1U, CSN_REAL);
    p->is_published = false;

    /* a k value is not known at init: the first pass answers */
    if (!csn_known_at_init(csound, p->scalar)) goto done;

    bool founded = false;
    binary_search(NULL, &founded, 0, source_arr->data, value, source_arr->size, false);
    *p->value = founded ? FL(1.0) : FL(0.0);

    set_array_version(&p->k_data.prev_source_version, &source_arr->version);
    p->k_data.prev_scalar_param = value;
    p->is_published = true;
    p->prev_result = founded;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_setcontains_k(CSOUND *csound, CSNSET_BINARYOP_SCALAR *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    CHECK_KTRIG(p->trig);
    int32_t res = OK;
    *p->value = (cs_float) ((double) p->prev_result);
    double value = (double) *p->scalar;

    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);

    CSN_ARRAY *source_arr = p->source_handle;
    res = set_check_body(csound, &p->h, source_arr);
    if (res != OK) goto done;

    if (p->is_published) {
        bool is_same_source = is_same_array_version(&p->k_data.prev_source_version, &source_arr->version);
        bool is_same_scalar = compare_double(&p->k_data.prev_scalar_param, &value) == 0;
        if (is_same_source && is_same_scalar) {
            *p->value = (cs_float) ((double) p->prev_result);
            goto done;
        }
    }

    size_t index = 0;
    bool founded = false;
    binary_search(&index, &founded, 0, source_arr->data, value, source_arr->size, false);

    double result_temp = (double) founded;
    *p->value = (cs_float) result_temp;

    SET_KDATA_END(p->k_data, source_arr->shape, 1U, CSN_REAL);
    set_array_version(&p->k_data.prev_source_version, &source_arr->version);
    p->k_data.prev_scalar_param = value;
    p->prev_result = founded;
    p->is_published = true;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

static int32_t set_binaryop_body(CSOUND *csound, OPDS *perf_h, CSN_ARRAY *source_arr_a, CSN_ARRAY *source_arr_b) {
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

    return OK;
}

static int32_t set_binaryop_helper(CSOUND *csound, CSNSET_BINARYOP *p, CSNSET_OPS_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle_a, p->source_handle_b }, 3);

    CSN_ARRAY *source_arr_a = p->source_handle_a;
    CSN_ARRAY *source_arr_b = p->source_handle_b;
    res = set_binaryop_body(csound, NULL, source_arr_a, source_arr_b);
    if (res != OK) goto done;

    uint32_t new_shape[CSN_MAX_DIMS] = {0};

    size_t source_a_size = source_arr_a->size;
    size_t source_b_size = source_arr_b->size;
    /* Room for both operands at their full capacity, which is as far as either
       can grow without reallocating: a k-rate pass on a marked path then
       never has to grow this. */
    size_t temp_cap = source_arr_a->capacity + source_arr_b->capacity;
    size_t bcap = temp_cap == 0 ? 1 : temp_cap;

    deinit_scratch(csound, &p->buffer);
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
                binary_search(&index, &founded, 0, buffer, value, count, false);
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
                    binary_search(NULL, &founded, 0, source_arr_b->data, value, source_b_size, false);
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
                    binary_search(NULL, &founded, 0, source_arr_b->data, value, source_b_size, false);
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
                    binary_search(NULL, &founded_a, 0, source_arr_b->data, value_a, source_b_size, false);
                    if (!founded_a) {
                        buffer[count] = value_a;
                        count++;
                    }
                }
                for (size_t i = 0; i < source_b_size; i++) {
                    double value_b = source_arr_b->data[i];
                    bool founded_b = false;
                    binary_search(NULL, &founded_b, 0, source_arr_a->data, value_b, source_a_size, false);
                    if (!founded_b) {
                        size_t index = 0;
                        binary_search(&index, &founded_b, 0, buffer, value_b, count, false);
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
    const CSN_ARRAY *ops[2] = { source_arr_a, source_arr_b };
    CSNARR_RES out = prepare_output(csound, &p->h, NULL, reg, p->handle, 1U, new_shape, CSN_REAL, ops, 2, CSN_ALIAS_CUSTOM);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    /* At k-rate an output that is one of its operands is computed by every
       perf pass; writing it here too would apply A = A ^ B twice in the first
       control cycle. */
    if (!(csn_is_k_form(&p->h) && out.aliased)) {
        res = set_store_result(csound, NULL, p->handle, out.aliased, buffer, count_items);
        if (res != OK) goto done;
    } else {
        SET_ARRAY_KIND(p->handle, CSNSET);
    }

    SET_KDATA_BEGIN(p->k_data, reg, new_shape, 1U, CSN_REAL);
    set_array_version(&p->k_data.prev_output_version, &p->handle->version);
    set_array_version(&p->k_data.prev_source_version, &source_arr_a->version);
    set_array_version(&p->k_data.prev_source_version_b, &source_arr_b->version);
    p->is_published = false;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

static int32_t set_binaryop_k_helper(CSOUND *csound, CSNSET_BINARYOP *p, CSNSET_OPS_MODE mode) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    CHECK_KTRIG(p->trig);
    int32_t res = OK;

    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle_a, p->source_handle_b }, 3);

    CSN_ARRAY *source_arr_a = p->source_handle_a;
    CSN_ARRAY *source_arr_b = p->source_handle_b;
    res = set_binaryop_body(csound, &p->h, source_arr_a, source_arr_b);
    if (res != OK) goto done;

    if (p->is_published && p->handle != source_arr_a && p->handle != source_arr_b) {
        bool is_same_version_a = is_same_array_version(&p->k_data.prev_source_version, &source_arr_a->version);
        bool is_same_version_b = is_same_array_version(&p->k_data.prev_source_version_b, &source_arr_b->version);
        bool is_same_result = is_same_array_version(&p->k_data.prev_output_version, &p->handle->version);
        if (is_same_version_a && is_same_version_b && is_same_result) goto done;
    }

    uint32_t new_ndim = 1U;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};

    size_t source_a_size = source_arr_a->size;
    size_t source_b_size = source_arr_b->size;

    res = csn_scratch_reserve(csound, &p->h, p->handle->rt_locked, &p->buffer, source_a_size + source_b_size, sizeof(double));
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
                binary_search(&index, &founded, 0, buffer, value, count, false);
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
                    binary_search(NULL, &founded, 0, source_arr_b->data, value, source_b_size, false);
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
                    binary_search(NULL, &founded, 0, source_arr_b->data, value, source_b_size, false);
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
                    binary_search(NULL, &founded_a, 0, source_arr_b->data, value_a, source_b_size, false);
                    if (!founded_a) {
                        buffer[count] = value_a;
                        count++;
                    }
                }
                for (size_t i = 0; i < source_b_size; i++) {
                    double value_b = source_arr_b->data[i];
                    bool founded_b = false;
                    binary_search(NULL, &founded_b, 0, source_arr_a->data, value_b, source_a_size, false);
                    if (!founded_b) {
                        size_t index = 0;
                        binary_search(&index, &founded_b, 0, buffer, value_b, count, false);
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
    const CSN_ARRAY *ops[2] = { source_arr_a, source_arr_b };
    CSNARR_RES out = prepare_output(csound, &p->h, &p->h, reg, p->handle, 1U, new_shape, CSN_REAL, ops, 2, CSN_ALIAS_CUSTOM);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    res = set_store_result(csound, &p->h, p->handle, out.aliased, buffer, count_items);
    if (res != OK) goto done;

    SET_KDATA_END(p->k_data, new_shape, new_ndim, CSN_REAL);
    set_array_version(&p->k_data.prev_output_version, &p->handle->version);
    set_array_version(&p->k_data.prev_source_version, &source_arr_a->version);
    set_array_version(&p->k_data.prev_source_version_b, &source_arr_b->version);
    p->is_published = true;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_set_binaryop_deinit(CSOUND *csound, CSNSET_BINARYOP *p) {
    deinit_scratch(csound, &p->buffer);
    return OK;
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

    int32_t res = OK;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle_a, p->source_handle_b }, 2);

    CSN_ARRAY *source_arr_a = p->source_handle_a;
    CSN_ARRAY *source_arr_b = p->source_handle_b;
    res = set_binaryop_body(csound, NULL, source_arr_a, source_arr_b);
    if (res != OK) goto done;

    size_t source_a_size = source_arr_a->size;
    size_t source_b_size = source_arr_b->size;

    bool result = true;
    switch (mode) {
        case CSNSET_SUBSET:
            for (size_t i = 0; i < source_a_size; i++) {
                double value = source_arr_a->data[i];
                bool founded = false;
                binary_search(NULL, &founded, 0, source_arr_b->data, value, source_b_size, false);
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
                binary_search(NULL, &founded, 0, source_arr_a->data, value, source_a_size, false);
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
                binary_search(NULL, &founded, 0, source_arr_b->data, value, source_b_size, false);
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
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

static int32_t set_binaryop_predicate_k_helper(CSOUND *csound, CSNSET_BINARYOP_PREDICATE *p, CSNSET_OPS_MODE mode) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    CHECK_KTRIG(p->trig);
    int32_t res = OK;

    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle_a, p->source_handle_b }, 2);

    CSN_ARRAY *source_arr_a = p->source_handle_a;
    CSN_ARRAY *source_arr_b = p->source_handle_b;
    res = set_binaryop_body(csound, &p->h, source_arr_a, source_arr_b);
    if (res != OK) goto done;

    if (p->is_published) {
        bool is_same_version_a = is_same_array_version(&p->prev_source_version_a, &source_arr_a->version);
        bool is_same_version_b = is_same_array_version(&p->prev_source_version_b, &source_arr_b->version);
        if (is_same_version_a && is_same_version_b) {
            *p->result = (cs_float) p->prev_result;
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
                binary_search(NULL, &founded, 0, source_arr_b->data, value, source_b_size, false);
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
                binary_search(NULL, &founded, 0, source_arr_a->data, value, source_a_size, false);
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
                binary_search(NULL, &founded, 0, source_arr_b->data, value, source_b_size, false);
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
    if (pres.locked) csn_unlock(csound, reg);
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
