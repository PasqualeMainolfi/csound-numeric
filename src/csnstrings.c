#include "csnstrings.h"
#include "csnregistry.h"
#include "csnum.h"
#include "csnum_internal.h"
#include "csnfile.h"
#include <stdint.h>
#include <string.h>
#include "arrays.h"


static bool IS_VALID_STRINGDAT(const STRINGDAT *value) {
    return value != NULL && value->data != NULL;
}

/* Every slot owns its string, so one already there is released first: an
   aliased output reaches here with its strings still in place. */
static void fill_csnstrings(CSOUND *csound, CSN_ARRAY *array, const char *value) {
    for (size_t i = 0; i < array->size; i++) {
        if (array->sdata[i] != NULL) csound->Free(csound, array->sdata[i]);
        array->sdata[i] = csound->Strdup(csound, value);
    }
}

/* A string costs an allocation, so a k pass that writes strings cannot run on
   a real-time path. The mark is decided at init, before the first pass. */
int32_t check_strings_not_rt(CSOUND *csound, OPDS *h, const CSN_ARRAY *out) {
    if (!out->rt_locked) return OK;
    /* an in-place form has no output: name the array it writes */
    const char *name = get_out_name(h);
    if (strcmp(name, "?") == 0 && h->optext != NULL && h->optext->t.inlist != NULL && h->optext->t.inlist->count > 0) {
        name = h->optext->t.inlist->arg[0];
    }
    return csn_perf_error(csound, h, "[csnarray] '%s' allocates its strings at perf time and the array is on a real-time path; build it at i-rate, clear the mark with csnrtunlock, or pass irt=0 at the audio source it descends from", name);
}

// EMPTY

int32_t create_empty_csnstrings(CSOUND *csound, CSN_STR_EMPTY *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    p->registry = reg;

    uint32_t ndim = 0;
    uint32_t shape[CSN_MAX_DIMS] = {0};
    int32_t res = parse_shape_array(csound, p->shape, &ndim, shape);
    if (res != OK) return res;

    CSNARR_RES pres = prepare_output(csound, &p->h, NULL, reg, p->handle, ndim, shape, CSN_STRING, NULL, 0, CSN_ALIAS_REFUSE);
    res = pres.err;
    if (res != OK) goto done;
    reset_empty_csnarray(p->handle, ndim, shape, CSN_STRING);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t create_empty_csnstrings_k_init(CSOUND *csound, CSN_STR_EMPTY *p) {
    return create_empty_csnstrings(csound, p);
}

/* Publishes no string, so a pass allocates nothing once the slots exist:
   prepare_output refuses growth on a marked array by itself. */
int32_t create_empty_csnstrings_k(CSOUND *csound, CSN_STR_EMPTY *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    uint32_t ndim = 0;
    uint32_t shape[CSN_MAX_DIMS] = {0};
    int32_t res = parse_shape_array_k(csound, &p->h, p->shape, &ndim, shape);
    if (res != OK) return res;

    size_t requested_size = 0;
    if (get_array_size_from_shape(&requested_size, ndim, shape) != OK) {
        return csn_perf_error(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSNARR_RES pres = prepare_output(csound, &p->h, &p->h, reg, p->handle, ndim, shape, CSN_STRING, NULL, 0, CSN_ALIAS_REFUSE);
    res = pres.err;
    if (res != OK) goto done;
    reset_empty_csnarray(p->handle, ndim, shape, CSN_STRING);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

// FULL

int32_t create_fullstr_csnarray(CSOUND *csound, CSN_FULLSTR *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    p->registry = reg;

    if (!IS_VALID_STRINGDAT(p->value)) {
        return csn_init_error(csound, "[csnarray] Invalid/NULL string");
    }

    CSNARR_RES pres = create_csnarray_from_shape(csound, &p->h, reg, p->shape, p->handle, CSN_STRING);
    if (pres.err != OK) return pres.err;

    fill_csnstrings(csound, p->handle, p->value->data);

    if (pres.locked) csn_unlock(csound, reg);
    return OK;
}

int32_t create_fullstr_csnarray_k_init(CSOUND *csound, CSN_FULLSTR *p) {
    return create_fullstr_csnarray(csound, p);
}

int32_t create_fullstr_csnarray_k(CSOUND *csound, CSN_FULLSTR *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    if (!IS_VALID_STRINGDAT(p->value)) {
        return csn_perf_error(csound, &p->h, "[csnarray] Invalid/NULL string");
    }

    int32_t res = check_strings_not_rt(csound, &p->h, p->handle);
    if (res != OK) return res;

    uint32_t ndim = 0;
    uint32_t shape[CSN_MAX_DIMS] = {0};
    res = parse_shape_array_k(csound, &p->h, p->shape, &ndim, shape);
    if (res != OK) return res;

    size_t requested_size = 0;
    if (get_array_size_from_shape(&requested_size, ndim, shape) != OK) {
        return csn_perf_error(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSNARR_RES pres = prepare_output(csound, &p->h, &p->h, reg, p->handle, ndim, shape, CSN_STRING, NULL, 0, CSN_ALIAS_REFUSE);
    res = pres.err;
    if (res != OK) goto done;

    fill_csnstrings(csound, p->handle, p->value->data);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

// LIKE

int32_t create_like_csnstrings(CSOUND *csound, CSN_STR_INIT_LIKE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    p->registry = reg;

    if (!IS_VALID_STRINGDAT(p->value)) {
        return csn_init_error(csound, "[csnarray] Invalid/NULL string");
    }

    CSN_ARRAY *source_arr = p->handle_from;
    if (source_arr->sdata == NULL) {
        return csn_init_error(csound, "[csnarray] Source array is not initialized");
    }

    int32_t res = OK;
    uint32_t ndim = source_arr->ndim;
    uint32_t shape[CSN_MAX_DIMS] = {0};
    memcpy(shape, source_arr->shape, sizeof(uint32_t) * CSN_MAX_DIMS);

    const CSN_ARRAY *operands[1] = { source_arr };
    CSNARR_RES pres = prepare_output(csound, &p->h, NULL, reg, p->handle, ndim, shape, CSN_STRING, operands, 1, CSN_ALIAS_ELEMENTWISE);
    res = pres.err;
    if (res != OK) goto done;

    fill_csnstrings(csound, p->handle, p->value->data);
    /* An output that is its own source (ELEMENTWISE) reaches here untouched by
       prepare_output, which then moved no version: the fill has to. */
    if (pres.aliased) update_array_data_version(&p->handle->version);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t create_like_csnstrings_k_init(CSOUND *csound, CSN_STR_INIT_LIKE *p) {
    return create_like_csnstrings(csound, p);
}

int32_t create_like_csnstrings_k(CSOUND *csound, CSN_STR_INIT_LIKE *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    CHECK_KTRIG(p->trig);

    if (!IS_VALID_STRINGDAT(p->value)) {
        return csn_perf_error(csound, &p->h, "[csnarray] Invalid/NULL string");
    }

    int32_t res = check_strings_not_rt(csound, &p->h, p->handle);
    if (res != OK) return res;

    CSN_ARRAY *source_arr = p->handle_from;
    uint32_t ndim = source_arr->ndim;
    uint32_t shape[CSN_MAX_DIMS] = {0};
    memcpy(shape, source_arr->shape, sizeof(uint32_t) * ndim);

    size_t requested_size = 0;
    if (get_array_size_from_shape(&requested_size, ndim, shape) != OK) {
        return csn_perf_error(csound, &p->h, "[csnarray] Source array has an invalid shape");
    }

    const CSN_ARRAY *operands[1] = { source_arr };
    CSNARR_RES pres = prepare_output(csound, &p->h, &p->h, reg, p->handle, ndim, shape, CSN_STRING, operands, 1, CSN_ALIAS_ELEMENTWISE);
    res = pres.err;
    if (res != OK) goto done;

    /* ELEMENTWISE has already refused an aliased output whose layout would
       change, so the output has the requested layout either way */
    fill_csnstrings(csound, p->handle, p->value->data);
    if (pres.aliased) update_array_data_version(&p->handle->version);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

// GET AND SET

static void COPY_STRING_TO_STRINGDAT(CSOUND *csound, OPDS *h, STRINGDAT *dest, char *source) {
    char empty[1] = "";
    char *item = source != NULL ? source : empty;
    STRINGDAT view = { item, strlen(item) + 1, -1 };
    const CS_TYPE *string_type = csound->GetType(csound, "S");
    string_type->copyValue(csound, string_type, dest, &view, h->insdshead);
}

/* The item is handed to the S output through the S type's own copy, which
   knows how the engine owns string buffers (refcount -1 marks an alias that
   must not be reallocated in place). A slot never written reads as "". */
static int32_t csnstrings_get_locked(CSOUND *csound, OPDS *h, OPDS *perf_h, CSN_ARRAY *source_arr, CSN_INDEXES indexes, STRINGDAT *out) {
    if (out == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Internal error: null string pointer passed to the element accessor");
    }

    size_t offset = 0;
    int32_t res = csnarray_resolve_item(csound, perf_h, source_arr, indexes, CSN_STRING, &offset);
    if (res != OK) return res;

    COPY_STRING_TO_STRINGDAT(csound, h, out, source_arr->sdata[offset]);
    return OK;
}

static int32_t csnstrings_set_locked(CSOUND *csound, OPDS *perf_h, CSN_ARRAY *source_arr, CSN_INDEXES indexes, const char *value) {
    if (value == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Internal error: null string pointer passed to the element accessor");
    }

    size_t offset = 0;
    int32_t res = csnarray_resolve_item(csound, perf_h, source_arr, indexes, CSN_STRING, &offset);
    if (res != OK) return res;

    /* Compared first, as the numeric csnset does: a k-rate set that stores the
       same string every pass is not a write, and it allocates nothing. */
    char *stored = source_arr->sdata[offset];
    if (stored != NULL && strcmp(stored, value) == 0) return OK;

    /* Only a real change allocates, so only then is the rt mark in the way. */
    if (perf_h != NULL && source_arr->rt_locked) {
        return csn_locked_perf_error(csound, perf_h, "[csnarray] Writing a string allocates at perf time and the array is on a real-time path; set it at i-rate, clear the mark with csnrtunlock, or pass irt=0 at the audio source it descends from");
    }

    if (stored != NULL) csound->Free(csound, stored);
    source_arr->sdata[offset] = csound->Strdup(csound, value);
    update_array_data_version(&source_arr->version);
    return OK;
}

int32_t csnstrings_get(CSOUND *csound, CSN_GETSTRING *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    p->registry = reg;

    stamp_globals(csound, &p->h);
    bool locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);
    int32_t res = csnstrings_get_locked(csound, &p->h, NULL, p->source_handle, indexes_of_arraydat(p->indexes), p->value);
    if (locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnstrings_get_k(CSOUND *csound, CSN_GETSTRING *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    bool locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);
    int32_t res = csnstrings_get_locked(csound, &p->h, &p->h, p->source_handle, indexes_of_arraydat(p->indexes), p->value);
    if (locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnstrings_set(CSOUND *csound, CSN_SETSTRING *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    if (csn_is_k_form(&p->h)) csn_mark_krate(p->source_handle);
    p->registry = reg;

    if (!IS_VALID_STRINGDAT(p->value)) {
        return csn_init_error(csound, "[csnarray] Invalid/NULL string");
    }

    stamp_globals(csound, &p->h);
    bool locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);
    int32_t res = csnstrings_set_locked(csound, NULL, p->source_handle, indexes_of_arraydat(p->indexes), p->value->data);
    if (locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnstrings_set_k(CSOUND *csound, CSN_SETSTRING *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    if (!IS_VALID_STRINGDAT(p->value)) {
        return csn_perf_error(csound, &p->h, "[csnarray] Invalid/NULL string");
    }

    bool locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);
    int32_t res = csnstrings_set_locked(csound, &p->h, p->source_handle, indexes_of_arraydat(p->indexes), p->value->data);
    if (locked) csn_unlock(csound, reg);
    return res;
}

static int32_t csnidxstrings_get_run(CSOUND *csound, OPDS *perf_h, CSN_IDX_GETSTRING *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, perf_h, reg);

    cs_float buffer[CSN_MAX_DIMS];
    CSN_INDEXES indexes;
    int32_t res = gather_indexes(csound, perf_h, p->idx, (size_t) p->INOCOUNT - 1, buffer, &indexes);
    if (res != OK) return res;

    bool locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);
    res = csnstrings_get_locked(csound, &p->h, perf_h, p->source_handle, indexes, p->value);
    if (locked) csn_unlock(csound, reg);
    return res;
}

static int32_t csnidxstrings_set_run(CSOUND *csound, OPDS *perf_h, CSN_IDX_SETSTRING *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, perf_h, reg);

    if (!IS_VALID_STRINGDAT(p->value)) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] Invalid/NULL string");
    }

    // the array, the value, then the indexes
    cs_float buffer[CSN_MAX_DIMS];
    CSN_INDEXES indexes;
    int32_t res = gather_indexes(csound, perf_h, p->idx, (size_t) p->INOCOUNT - 2, buffer, &indexes);
    if (res != OK) return res;

    bool locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);
    res = csnstrings_set_locked(csound, perf_h, p->source_handle, indexes, p->value->data);
    if (locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnidxstrings_get(CSOUND *csound, CSN_IDX_GETSTRING *p) {
    int32_t res = csnidx_bind(csound, &p->h, &p->registry);
    return res != OK ? res : csnidxstrings_get_run(csound, NULL, p);
}

int32_t csnidxstrings_get_k_init(CSOUND *csound, CSN_IDX_GETSTRING *p) {
    return csnidx_bind(csound, &p->h, &p->registry);
}

int32_t csnidxstrings_get_k(CSOUND *csound, CSN_IDX_GETSTRING *p) {
    return csnidxstrings_get_run(csound, &p->h, p);
}

int32_t csnidxstrings_set(CSOUND *csound, CSN_IDX_SETSTRING *p) {
    int32_t res = csnidx_bind(csound, &p->h, &p->registry);
    return res != OK ? res : csnidxstrings_set_run(csound, NULL, p);
}

int32_t csnidxstrings_set_k_init(CSOUND *csound, CSN_IDX_SETSTRING *p) {
    csn_mark_krate(p->source_handle);
    return csnidx_bind(csound, &p->h, &p->registry);
}

int32_t csnidxstrings_set_k(CSOUND *csound, CSN_IDX_SETSTRING *p) {
    return csnidxstrings_set_run(csound, &p->h, p);
}

int32_t from_array_to_csnstrings(CSOUND *csound, CSN_FROM_ARRAY *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    if (p->source == NULL
        || p->source->data == NULL
        || p->source->sizes == NULL
        || p->source->dimensions <= 0
        || p->source->dimensions > CSN_MAX_DIMS) {
        return csn_init_error(csound, "[csnarray] Source must be an i-array with 1 to %d dimensions and allocated data", CSN_MAX_DIMS);
    }

    uint32_t ndim = (uint32_t) p->source->dimensions;
    uint32_t shape[CSN_MAX_DIMS] = {0};

    for (uint32_t i = 0; i < ndim; i++) {
        int32_t size = p->source->sizes[i];
        if (size <= 0) {
            return csn_init_error(csound, "[csnarray] Source extent %u must be >= 1", i);
        }
        shape[i] = (uint32_t) size;
    }

    size_t total_size = 0;
    if (get_array_size_from_shape(&total_size, ndim, shape) != OK) {
        return csn_init_error(csound, "[csnarray] Source shape is invalid or its element count exceeds the configured limit");
    }

    CSNARR_RES pres = prepare_output(csound, &p->h, NULL, reg, p->handle, ndim, shape, CSN_STRING, NULL, 0, CSN_ALIAS_REFUSE);
    int32_t res = pres.err;
    if (res != OK) goto done;

    const STRINGDAT *items = (const STRINGDAT *) p->source->data;
    for (size_t i = 0; i < total_size; i++) {
        p->handle->sdata[i] = csound->Strdup(csound, items[i].data != NULL ? items[i].data : "");
    }

    p->registry = reg;
done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t from_array_to_csnstrings_k_init(CSOUND *csound, CSN_FROM_ARRAY *p) {
    return from_array_to_csnstrings(csound, p);
}

int32_t from_array_to_csnstrings_k(CSOUND *csound, CSN_FROM_ARRAY *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    if (p->source == NULL
        || p->source->data == NULL
        || p->source->sizes == NULL
        || p->source->dimensions <= 0
        || p->source->dimensions > CSN_MAX_DIMS) {
        return csn_perf_error(csound, &p->h, "[csnarray] Source must be a k-array with 1 to %d dimensions and allocated data", CSN_MAX_DIMS);
    }

    CHECK_KTRIG(p->trig);

    uint32_t ndim = (uint32_t) p->source->dimensions;
    uint32_t shape[CSN_MAX_DIMS] = {0};

    for (uint32_t i = 0; i < ndim; i++) {
        int32_t size = p->source->sizes[i];
        if (size <= 0) {
            return csn_perf_error(csound, &p->h, "[csnarray] Source extent %u must be >= 1", i);
        }
        shape[i] = (uint32_t) size;
    }

    size_t requested_size = 0;
    if (get_array_size_from_shape(&requested_size, ndim, shape) != OK) {
        return csn_perf_error(csound, &p->h, "[csnarray] Source shape is invalid or its element count exceeds the configured limit");
    }

    int32_t res = check_strings_not_rt(csound, &p->h, p->handle);
    if (res != OK) return res;

    CSNARR_RES pres = prepare_output(csound, &p->h, &p->h, reg, p->handle, ndim, shape, CSN_STRING, NULL, 0, CSN_ALIAS_REFUSE);
    res = pres.err;
    if (res != OK) goto done;

    const STRINGDAT *items = (const STRINGDAT *) p->source->data;
    for (size_t i = 0; i < requested_size; i++) {
        p->handle->sdata[i] = csound->Strdup(csound, items[i].data != NULL ? items[i].data : "");
    }

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t from_csnstrings_to_array(CSOUND *csound, CSN_TO_ARRAY *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    stamp_globals(csound, &p->h);
    p->registry = reg;

    CSN_ARRAY *src = p->source_handle;
    int32_t res = OK;
    bool locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { src }, 1);
    uint32_t ndim = src->ndim;

    if (src->size == 0 && csn_is_k_form(&p->h)) {
        if (reserve_krate_array_output(csound, &p->h, p->array, src->capacity) != OK) {
            res = csn_init_error(csound, "[csnarray] Could not reserve the output array");
        }
        goto done;
    }

    if (src->size == 0) {
        res = csn_init_error(csound, "[csnarray] Cannot convert an empty CsnStr array to ARRAYDAT");
        goto done;
    }

    if (src->itype != CSN_STRING) {
        res = csn_init_error(csound, "[csnarray] Internal error: the handle does not hold strings");
        goto done;
    }

    int32_t declared = p->array->dimensions > 0 ? p->array->dimensions : 1;
    if ((uint32_t) declared != ndim) {
        res = csn_init_error(csound, "[csnarray] Handle holds a %u-D array but the output is declared %d-D; declare it with %u bracket pairs, or give it %u dimensions with init first", ndim, declared, ndim, ndim);
        goto done;
    }

    size_t total_size = src->size;
    if (total_size > (size_t) INT32_MAX) {
        res = csn_init_error(csound, "[csnarray] Array holds %zu elements, too many for an S-array output (limit %d)", total_size, INT32_MAX);
        goto done;
    }

    res = tabinit(csound, p->array, (int32_t) total_size, p->h.insdshead);
    if (res!= OK || p->array->data == NULL || p->array->sizes == NULL || reserve_krate_array_output(csound, &p->h, p->array, src->capacity) != OK) {
        res = csn_init_error(csound, "[csnarray] Could not allocate the %u-D output S-array of %zu elements", ndim, total_size);
        goto done;
    }

    p->array->dimensions = (int32_t) ndim;
    for (uint32_t i = 0; i < ndim; i++) {
        p->array->sizes[i] = (int32_t) src->shape[i];
    }

    STRINGDAT *items = (STRINGDAT *) p->array->data;
    for (size_t i = 0; i < total_size; i++) {
        COPY_STRING_TO_STRINGDAT(csound, &p->h, &items[i], src->sdata[i]);
    }

done:
    if (locked) csn_unlock(csound, reg);
    return res;
}

int32_t from_csnstrings_to_array_k(CSOUND *csound, CSN_TO_ARRAY *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    int32_t res = OK;
    CHECK_KTRIG(p->trig);

    CSN_ARRAY *src = p->source_handle;
    bool locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { src }, 1);
    uint32_t ndim = src->ndim;

    if (src->itype != CSN_STRING) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Internal error: the handle does not hold strings");
        goto done;
    }

    if (src->size == 0) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Cannot convert an empty CsnStr array to ARRAYDAT");
        goto done;
    }

    int32_t declared = p->array->dimensions > 0 ? p->array->dimensions : 1;
    if ((uint32_t) declared != ndim) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Handle holds a %u-D array but the output is declared %d-D; declare it with %u bracket pairs, or give it %u dimensions with init first", ndim, declared, ndim, ndim);
        goto done;
    }

    size_t total_size = src->size;
    if (total_size > (size_t) INT32_MAX) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Array holds %zu elements, too many for an k-array output (limit %d)", total_size, INT32_MAX);
        goto done;
    }

    if (src->rt_locked && !krate_array_output_fits(p->array, total_size)) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Array is on a real-time path and has grown to %zu elements, past what '%s' reserved at init; clear the mark with csnrtunlock, or pass irt=0 at the audio source it descends from", total_size, get_out_name(&p->h));
        goto done;
    }

    // use int32_t as return value on tabinit in Csound recent version
    res = tabinit(csound, p->array, (int32_t) total_size, p->h.insdshead);
    if (p->array->data == NULL || p->array->sizes == NULL || res != OK) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Could not allocate the %u-D output k-array of %zu elements", ndim, total_size);
        goto done;
    }

    p->array->dimensions = (int32_t) ndim;
    for (uint32_t i = 0; i < ndim; i++) {
        p->array->sizes[i] = (int32_t) src->shape[i];
    }

    STRINGDAT *items = (STRINGDAT *) p->array->data;
    for (size_t i = 0; i < total_size; i++) {
        COPY_STRING_TO_STRINGDAT(csound, &p->h, &items[i], src->sdata[i]);
    }

done:
    if (locked) csn_unlock(csound, reg);
    return res;
}

// LOAD

static int32_t check_load_path(CSOUND *csound, OPDS *perf_h, const STRINGDAT *path, const char **dot) {
    if (path == NULL || path->data == NULL || path->size <= 0 || path->data[0] == '\0') {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] File path cannot be empty");
    }
    *dot = strrchr(path->data, '.');
    if (*dot == NULL || (strcmp(*dot, CSN_FILE_EXT) != 0 && strcmp(*dot, CSN_FILE_NPY_EXT) != 0)) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] Invalid file extension: should be [%s] or [%s]", CSN_FILE_EXT, CSN_FILE_NPY_EXT);
    }
    return OK;
}

/* The file is read before the lock is taken; the loaded strings then move
   into the output prepare_output has just emptied, each slot taking
   ownership of its string, so nothing is copied twice. */
static int32_t load_strings_into(CSOUND *csound, OPDS *h, OPDS *perf_h, CSN_REGISTRY *reg, CSN_ARRAY *out, const char *path, const char *dot) {
    uint32_t ndim = 0;
    uint32_t shape[CSN_MAX_DIMS] = {0};
    char **items = NULL;
    size_t size = 0;
    CSN_FILE_ERROR_CODE err_code = csnfile_load_strings(csound, path, dot, &ndim, shape, &items, &size);
    if (err_code != CSN_FILE_NO_ERROR) {
        const char *err_message = NULL;
        csnfile_dispatch_error(&err_message, err_code);
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] %s", err_message);
    }

    bool locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { out }, 1);
    CSNARR_RES pres = prepare_output(csound, h, perf_h, reg, out, ndim, shape, CSN_STRING, NULL, 0, CSN_ALIAS_REFUSE);
    int32_t res = pres.err;
    if (res == OK) {
        for (size_t i = 0; i < size; i++) {
            out->sdata[i] = items[i];
            items[i] = NULL;
        }
    }
    if (locked || pres.locked) csn_unlock(csound, reg);

    for (size_t i = 0; i < size; i++) {
        if (items[i] != NULL) csound->Free(csound, items[i]);
    }
    csound->Free(csound, items);
    return res;
}

int32_t csnload_strings(CSOUND *csound, CSN_LOAD *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    p->registry = reg;

    const char *dot = NULL;
    int32_t res = check_load_path(csound, NULL, p->path, &dot);
    if (res != OK) return res;

    stamp_globals(csound, &p->h);
    return load_strings_into(csound, &p->h, NULL, reg, p->handle, p->path->data, dot);
}

/* As the numeric csnload.k: the output is published reserved but empty, and
   the file is read when the trigger fires. */
int32_t csnload_strings_k_init(CSOUND *csound, CSN_LOAD *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    p->registry = reg;

    stamp_globals(csound, &p->h);
    bool locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle }, 1);

    uint32_t shape[CSN_MAX_DIMS] = { 1U };
    CSNARR_RES out = prepare_output(csound, &p->h, NULL, reg, p->handle, 1U, shape, CSN_STRING, NULL, 0, CSN_ALIAS_REFUSE);
    int32_t res = out.err;
    if (res == OK) reset_empty_csnarray(p->handle, 1U, shape, CSN_STRING);

    if (locked || out.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnload_strings_k(CSOUND *csound, CSN_LOAD *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    const char *dot = NULL;
    int32_t res = check_load_path(csound, &p->h, p->path, &dot);
    if (res != OK) return res;

    /* no reload cache, as csnload.k: the trigger fires, the file is read */
    CHECK_KTRIG(p->trig);

    res = check_strings_not_rt(csound, &p->h, p->handle);
    if (res != OK) return res;

    return load_strings_into(csound, &p->h, &p->h, reg, p->handle, p->path->data, dot);
}

/* Writes source's strings into destination in transposed order. A copy
   duplicates every string, releasing what destination held; in place, source
   and destination hold the same strings, so the pointers are only permuted
   (into a scratch buffer the caller copies back) and nothing is allocated. */
int32_t transpose_stringdata_assign(CSOUND *csound, char **source, char **destination, size_t size, uint32_t ndim, const uint32_t *shape, const size_t *strides, const uint32_t *axes, bool in_place) {
    CSN_BROADCAST_ITER it;
    if (ND_ITER_INIT(&it, ndim, shape, size, NULL) != OK) return NOTOK;
    while (BROADCAST_ITER_NEXT(&it)) {
        size_t linear = it.linear_index;
        size_t src_index = 0;
        for (uint32_t i = 0; i < ndim; ++i) {
            src_index += (size_t) it.coords[i] * strides[axes[i]];
        }

        if (in_place) {
            destination[linear] = source[src_index];
        } else {
            if (destination[linear] != NULL) csound->Free(csound, destination[linear]);
            destination[linear] = csound->Strdup(csound, source[src_index]);
        }
    }

    return OK;
}

/* Moving strings around inside one array only swaps the slots' pointers: each
   string keeps its single owner and nothing is allocated, so the in-place
   flip, roll and reverse are safe on a real-time path. */
void swap_strings(char **data, size_t a, size_t b) {
    char *tmp = data[a];
    data[a] = data[b];
    data[b] = tmp;
}

static void reverse_string_run(char **data, size_t base, size_t count, size_t stride) {
    if (count < 2) return;
    size_t lo = 0;
    size_t hi = count - 1;
    while (lo < hi) {
        swap_strings(data, base + lo * stride, base + hi * stride);
        lo++;
        hi--;
    }
}

/* item i of the run moves to (i + shift) mod count, as numpy.roll does */
void rotate_string_run(char **data, size_t base, size_t count, size_t stride, int32_t shift) {
    if (count < 2) return;
    int64_t k = (int64_t) shift % (int64_t) count;
    if (k < 0) k += (int64_t) count;
    if (k == 0) return;
    reverse_string_run(data, base, count, stride);
    reverse_string_run(data, base, (size_t) k, stride);
    reverse_string_run(data, base + (size_t) k * stride, count - (size_t) k, stride);
}

void reverse_string_in_place(CSN_ARRAY *arr) {
    for (size_t i = 0; i < arr->size / 2U; i++) {
        swap_strings(arr->sdata, i, arr->size - 1U - i);
    }
}

int32_t concat_block_string_assign_value(CSOUND *csound, const CSN_ARRAY *source_arr, const CSN_ARRAY *data_arr, CSN_ARRAY *destination, uint32_t axis) {
    uint32_t source_axis_extent = source_arr->size == 0 ? 0U : source_arr->shape[axis];
    CSN_BROADCAST_ITER it;
    if (ND_ITER_INIT(&it, destination->ndim, destination->shape, destination->size, NULL) != OK) return NOTOK;
    while (BROADCAST_ITER_NEXT(&it)) {
        size_t linear = it.linear_index;
        const char *from;
        if (it.coords[axis] < source_axis_extent) {
            size_t source_off = 0;
            for (uint32_t d = 0; d < source_arr->ndim; ++d) {
                source_off += (size_t) it.coords[d] * source_arr->strides[d];
            }
            from = source_arr->sdata[source_off];
        } else {
            size_t block_off = 0;
            for (uint32_t d = 0; d < data_arr->ndim; ++d) {
                uint32_t coord = it.coords[d] - (d == axis ? source_axis_extent : 0U);
                block_off += (size_t) coord * data_arr->strides[d];
            }
            from = data_arr->sdata[block_off];
        }
        if (destination->sdata[linear] != NULL) csound->Free(csound, destination->sdata[linear]);
        destination->sdata[linear] = csound->Strdup(csound, from);
    }

    return OK;
}

int32_t pad_assign_string_value(CSOUND *csound, const CSN_ARRAY *source_arr, CSN_ARRAY *destination, STRINGDAT *value, int32_t axis, uint32_t before) {
    bool source_is_empty = source_arr->size == 0;

    CSN_BROADCAST_ITER it;
    if (ND_ITER_INIT(&it, destination->ndim, destination->shape, destination->size, NULL) != OK) return NOTOK;
    while (BROADCAST_ITER_NEXT(&it)) {
        size_t linear = it.linear_index;
        bool is_inside = !source_is_empty;
        size_t source_off = 0;
        for (uint32_t i = 0; is_inside && i < source_arr->ndim; i++) {
            bool is_padded_axis = axis == -1 || (uint32_t) axis == i;
            if (is_padded_axis) {
                if (it.coords[i] < before || it.coords[i] >= before + source_arr->shape[i]) {
                    is_inside = false;
                    break;
                }
                source_off += (size_t) (it.coords[i] - before) * source_arr->strides[i];
            } else {
                source_off += (size_t) it.coords[i] * source_arr->strides[i];
            }
        }

        if (destination->sdata[linear] != NULL) csound->Free(csound, destination->sdata[linear]);
        destination->sdata[linear] = csound->Strdup(csound, is_inside ? source_arr->sdata[source_off] : value->data);
    }

    return OK;
}

/* The k forms keep a copy of the last fill value in k_data, released here
   and before a reinit takes the struct again. */
int32_t csnstrings_pad_k_deinit(CSOUND *csound, CSN_PADSTRING *p) {
    csn_kdata_release_strings(csound, &p->k_data);
    return OK;
}

int32_t csnstrings_pad_in_k_deinit(CSOUND *csound, CSN_PADSTRING_IN *p) {
    csn_kdata_release_strings(csound, &p->k_data);
    free_private_array(csound, &p->scratch);
    return OK;
}

int32_t csnstrings_pad(CSOUND *csound, CSN_PADSTRING *p) {
    const cs_float *axis = p->INOCOUNT > 4 ? p->arg_a : NULL;
    if (!IS_VALID_STRINGDAT(p->value)) {
        return csn_init_error(csound, "[csnarray] Invalid/NULL string");
    }
    return csnarray_pad_helper(csound, &p->h, p->handle, p->source_handle, p->before, p->after, 0.0, NULL, p->value, CSN_STRING, axis);
}

int32_t csnstrings_pad_k_init(CSOUND *csound, CSN_PADSTRING *p) {
    const cs_float *axis_in = p->INOCOUNT > 5 ? p->arg_b : NULL;
    if (!IS_VALID_STRINGDAT(p->value)) {
        return csn_init_error(csound, "[csnarray] Invalid/NULL string");
    }
    csn_kdata_release_strings(csound, &p->k_data);
    return csnarray_pad_k_init_helper(csound, &p->h, &p->k_data, p->handle, p->source_handle, p->before, p->after, NULL, 0.0, NULL, p->value, CSN_STRING, axis_in);
}

int32_t csnstrings_pad_k(CSOUND *csound, CSN_PADSTRING *p) {
    const cs_float *axis_in = p->INOCOUNT > 5 ? p->arg_b : NULL;
    if (!IS_VALID_STRINGDAT(p->value)) {
        return csn_perf_error(csound, &p->h, "[csnarray] Invalid/NULL string");
    }
    int32_t res = check_strings_not_rt(csound, &p->h, p->handle);
    if (res != OK) return res;
    return csnarray_pad_k_helper(csound, &p->h, &p->k_data, p->handle, p->source_handle, p->before, p->after, 0.0, NULL, p->value, CSN_STRING, axis_in, p->arg_a);
}

int32_t csnstrings_pad_in(CSOUND *csound, CSN_PADSTRING_IN *p) {
    const cs_float *axis = p->INOCOUNT > 4 ? p->arg_a : NULL;
    if (!IS_VALID_STRINGDAT(p->value)) {
        return csn_init_error(csound, "[csnarray] Invalid/NULL string");
    }
    return csnarray_pad_in_helper(csound, &p->h, p->source_handle, p->before, p->after, 0.0, NULL, p->value, CSN_STRING, axis);
}

int32_t csnstrings_pad_in_k_init(CSOUND *csound, CSN_PADSTRING_IN *p) {
    csn_kdata_release_strings(csound, &p->k_data);
    memset(&p->k_data, 0, sizeof(p->k_data));
    return csnarray_pad_in_k_scratch_init(csound, &p->h, p->source_handle, CSN_STRING, &p->registry, &p->scratch);
}

/* The fill strings are allocated, so a source on a real-time path is refused
   even in place. */
int32_t csnstrings_pad_in_k(CSOUND *csound, CSN_PADSTRING_IN *p) {
    const cs_float *axis_in = p->INOCOUNT > 5 ? p->arg_b : NULL;
    if (!IS_VALID_STRINGDAT(p->value)) {
        return csn_perf_error(csound, &p->h, "[csnarray] Invalid/NULL string");
    }
    int32_t res = check_strings_not_rt(csound, &p->h, p->source_handle);
    if (res != OK) return res;
    return csnarray_pad_in_k_helper(csound, &p->h, p->registry, p->scratch, &p->k_data, p->source_handle, p->before, p->after, 0.0, NULL, p->value, CSN_STRING, axis_in, p->arg_a);
}

/* The flat take of a string: the S output gets its own copy, through the S
   type as csnget hands one over. */
static int32_t take_flat_string_locked(CSOUND *csound, OPDS *perf_h, CSN_TAKESTRING_FLAT *p) {
    CSN_ARRAY *arr = p->source_handle;
    int32_t res = check_take_flat_body(csound, perf_h, arr, (double) *p->index, false);
    if (res != OK) return res;

    COPY_STRING_TO_STRINGDAT(csound, &p->h, p->value, arr->sdata[(size_t) *p->index]);
    return OK;
}

int32_t csnstrings_take_flat(CSOUND *csound, CSN_TAKESTRING_FLAT *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    p->registry = reg;

    stamp_globals(csound, &p->h);
    bool locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);
    int32_t res = take_flat_string_locked(csound, NULL, p);
    if (locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnstrings_take_flat_k(CSOUND *csound, CSN_TAKESTRING_FLAT *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    bool locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);
    int32_t res = take_flat_string_locked(csound, &p->h, p);
    if (locked) csn_unlock(csound, reg);
    return res;
}

/* PUSH, POP, INSERT, REMOVE

   A slot past size may keep a string (free_csnstrings walks capacity), so
   the ops that shrink the array leave the string they drop there, owned
   and unaliased: pop and remove move pointers only and allocate nothing,
   which keeps them usable on a real-time path. The ops that grow take that
   slot's string back and release it after the copy they store. */

static int32_t check_pushstring_body(CSOUND *csound, OPDS *perf_h, CSN_ARRAY *arr) {
    if (arr->ndim != 1) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Push needs a 1-D array, got %u-D", arr->ndim);
    }

    if (arr->size >= CSN_MAX_ELEMS) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Push would exceed the maximum element count: array already holds %zu of %zu elements", arr->size, (size_t) CSN_MAX_ELEMS);
    }

    return OK;
}

static int32_t check_popstring_body(CSOUND *csound, OPDS *perf_h, CSN_ARRAY *arr) {
    if (arr->ndim != 1) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Pop needs a 1-D array, got %u-D", arr->ndim);
    }

    if (arr->size == 0) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Cannot pop from an empty array");
    }

    return OK;
}

/* Stores a copy of value at index, shifting the slots from index up by one.
   The slot at size, past the end, is the one the shift overwrites: its
   string, if any, is released once the copy is in place. */
static int32_t insertstring_at(CSOUND *csound, OPDS *perf_h, CSN_ARRAY *arr, size_t index, const char *value) {
    size_t new_size = arr->size + 1;
    int32_t res = ensure_mutation_capacity(csound, perf_h, arr, new_size, arr->rt_locked);
    if (res != OK) return res;

    char *dropped = arr->sdata[arr->size];
    memmove(arr->sdata + index + 1, arr->sdata + index, sizeof(char *) * (arr->size - index));
    arr->sdata[index] = csound->Strdup(csound, value);
    if (dropped != NULL) csound->Free(csound, dropped);

    arr->size = new_size;
    arr->shape[0] = (uint32_t) new_size;
    update_array_data_version(&arr->version);
    update_array_layout_version(&arr->version, true, false, false);
    return OK;
}

/* Hands the string at index to out and closes the gap; the string itself
   moves to the slot just past the new end. */
static void removestring_at(CSOUND *csound, OPDS *h, CSN_ARRAY *arr, size_t index, STRINGDAT *out) {
    COPY_STRING_TO_STRINGDAT(csound, h, out, arr->sdata[index]);

    char *removed = arr->sdata[index];
    size_t new_size = arr->size - 1;
    memmove(arr->sdata + index, arr->sdata + index + 1, sizeof(char *) * (new_size - index));
    arr->sdata[new_size] = removed;

    arr->size = new_size;
    arr->shape[0] = (uint32_t) new_size;
    update_array_data_version(&arr->version);
    update_array_layout_version(&arr->version, true, false, false);
}

int32_t csnstrings_push(CSOUND *csound, CSN_PUSHSTRING *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    if (!IS_VALID_STRINGDAT(p->in_value)) {
        return csn_init_error(csound, "[csnarray] Invalid/NULL string");
    }

    stamp_globals(csound, &p->h);
    bool locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);

    CSN_ARRAY *arr = p->source_handle;
    int32_t res = check_pushstring_body(csound, NULL, arr);
    if (res == OK) res = insertstring_at(csound, NULL, arr, arr->size, p->in_value->data);

    if (locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnstrings_push_k_init(CSOUND *csound, CSN_PUSHSTRING_K *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    /* the target changes at perf: whatever reads it follows it there */
    csn_mark_krate(p->source_handle);
    p->registry = reg;
    stamp_globals(csound, &p->h);
    return OK;
}

int32_t csnstrings_push_k(CSOUND *csound, CSN_PUSHSTRING_K *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_KTRIG(p->arg_a);

    if (!IS_VALID_STRINGDAT(p->in_value)) {
        return csn_perf_error(csound, &p->h, "[csnarray] Invalid/NULL string");
    }

    /* the stored copy is an allocation */
    int32_t res = check_strings_not_rt(csound, &p->h, p->source_handle);
    if (res != OK) return res;

    bool locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);

    CSN_ARRAY *arr = p->source_handle;
    res = check_pushstring_body(csound, &p->h, arr);
    if (res == OK) res = insertstring_at(csound, &p->h, arr, arr->size, p->in_value->data);

    if (locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnstrings_pop(CSOUND *csound, CSN_POPSTRING *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    stamp_globals(csound, &p->h);
    bool locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);

    CSN_ARRAY *arr = p->source_handle;
    int32_t res = check_popstring_body(csound, NULL, arr);
    if (res == OK) removestring_at(csound, &p->h, arr, arr->size - 1, p->out_value);

    if (locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnstrings_pop_k_init(CSOUND *csound, CSN_POPSTRING_K *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    csn_mark_krate(p->source_handle);
    stamp_globals(csound, &p->h);
    p->registry = reg;
    return OK;
}

int32_t csnstrings_pop_k(CSOUND *csound, CSN_POPSTRING_K *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_KTRIG(p->arg_a);

    bool locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);

    CSN_ARRAY *arr = p->source_handle;
    int32_t res = check_popstring_body(csound, &p->h, arr);
    if (res == OK) removestring_at(csound, &p->h, arr, arr->size - 1, p->out_value);

    if (locked) csn_unlock(csound, reg);
    return res;
}

static int32_t insertstring_value_locked(CSOUND *csound, OPDS *perf_h, CSN_ARRAY *arr, double index_value, const char *value) {
    if (!IS_VALID_INDEX(index_value)) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Insert index %g is invalid; indexes must be finite non-negative integers", index_value);
    }

    if (arr->ndim != 1) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Insert needs a 1-D array, got %u-D", arr->ndim);
    }

    if (arr->size >= CSN_MAX_ELEMS) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Insert would exceed the maximum element count: array already holds %zu of %zu elements", arr->size, (size_t) CSN_MAX_ELEMS);
    }

    size_t index = (size_t) index_value;
    /* Inclusive upper bound: index == size appends. */
    if (index > arr->size) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Insert index %zu is out of range for an array of %zu elements (valid: 0..%zu, end included)", index, arr->size, arr->size);
    }

    return insertstring_at(csound, perf_h, arr, index, value);
}

int32_t csnstrings_insert(CSOUND *csound, CSN_PUSHSTRING *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    if (!IS_VALID_STRINGDAT(p->in_value)) {
        return csn_init_error(csound, "[csnarray] Invalid/NULL string");
    }

    stamp_globals(csound, &p->h);
    bool locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);
    int32_t res = insertstring_value_locked(csound, NULL, p->source_handle, (double) *p->index, p->in_value->data);
    if (locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnstrings_insert_k(CSOUND *csound, CSN_PUSHSTRING_K *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_KTRIG(p->arg_b);

    if (!IS_VALID_STRINGDAT(p->in_value)) {
        return csn_perf_error(csound, &p->h, "[csnarray] Invalid/NULL string");
    }

    /* the stored copy is an allocation */
    int32_t res = check_strings_not_rt(csound, &p->h, p->source_handle);
    if (res != OK) return res;

    bool locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);
    res = insertstring_value_locked(csound, &p->h, p->source_handle, (double) *p->arg_a, p->in_value->data);
    if (locked) csn_unlock(csound, reg);
    return res;
}

static int32_t removestring_value_locked(CSOUND *csound, OPDS *h, OPDS *perf_h, CSN_ARRAY *arr, double index_value, STRINGDAT *out_value) {
    if (!IS_VALID_INDEX(index_value)) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Remove index %g is invalid; indexes must be finite non-negative integers", index_value);
    }

    if (arr->ndim != 1) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Remove needs a 1-D array, got %u-D", arr->ndim);
    }

    if (arr->size == 0) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Cannot remove from an empty array");
    }

    size_t index = (size_t) index_value;
    if (index >= arr->size) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Remove index %zu is out of range for an array of %zu elements (valid: 0..%zu)", index, arr->size, arr->size - 1);
    }

    removestring_at(csound, h, arr, index, out_value);
    return OK;
}

int32_t csnstrings_remove(CSOUND *csound, CSN_POPSTRING *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    stamp_globals(csound, &p->h);
    bool locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);
    int32_t res = removestring_value_locked(csound, &p->h, NULL, p->source_handle, (double) *p->index, p->out_value);
    if (locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnstrings_remove_k(CSOUND *csound, CSN_POPSTRING_K *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_KTRIG(p->arg_b);

    bool locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);
    int32_t res = removestring_value_locked(csound, &p->h, &p->h, p->source_handle, (double) *p->arg_a, p->out_value);
    if (locked) csn_unlock(csound, reg);
    return res;
}

// WHERE, PUTMASK

/* The mask is a CsnArr read for zero / non-zero, the values are strings: an
   array of the mask's shape or, for the false branch of csnwhere and the
   values of csnputmask, one S. No broadcasting, as in the numeric forms. */
static int32_t check_string_mask(CSOUND *csound, OPDS *perf_h, const CSN_ARRAY *mask_arr, const CSN_ARRAY *arr, const char *what) {
    if (mask_arr->itype != CSN_REAL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] The mask must be a real array");
    }

    if (arr->ndim != mask_arr->ndim || memcmp(arr->shape, mask_arr->shape, sizeof(uint32_t) * mask_arr->ndim) != 0) {
        char mbuf[CSN_SHAPE_STR_MAX], abuf[CSN_SHAPE_STR_MAX];
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] The %s has shape %s but the mask has shape %s; they must match", what, shape_str(abuf, sizeof(abuf), arr->shape, arr->ndim), shape_str(mbuf, sizeof(mbuf), mask_arr->shape, mask_arr->ndim));
    }

    return OK;
}

static int32_t where_body_string(CSOUND *csound, OPDS *perf_h, const CSN_ARRAY *mask_arr, const CSN_ARRAY *true_arr, const CSN_ARRAY *false_arr, const STRINGDAT *false_string) {
    int32_t res = check_string_mask(csound, perf_h, mask_arr, true_arr, "true branch");
    if (res != OK) return res;

    if (false_arr != NULL) return check_string_mask(csound, perf_h, mask_arr, false_arr, "false branch");
    if (!IS_VALID_STRINGDAT(false_string)) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Invalid/NULL string");
    }

    return OK;
}

static void where_string_assign(CSOUND *csound, CSN_ARRAY *out_arr, const CSN_ARRAY *mask_arr, const CSN_ARRAY *true_arr, const CSN_ARRAY *false_arr, const STRINGDAT *false_string) {
    for (size_t i = 0; i < out_arr->size; i++) {
        const char *item = mask_arr->data[i] != 0.0 ? true_arr->sdata[i] : false_arr != NULL ? false_arr->sdata[i] : false_string->data;
        if (out_arr->sdata[i] != NULL) csound->Free(csound, out_arr->sdata[i]);
        out_arr->sdata[i] = csound->Strdup(csound, item);
    }
}

static bool same_string(const char *a, const char *b) {
    return a != NULL && b != NULL ? strcmp(a, b) == 0 : a == b;
}

static int32_t where_string_helper(CSOUND *csound, OPDS *h, CSN_ARRAY *mask_arr, CSN_ARRAY *true_arr, CSN_ARRAY *false_arr, CSN_ARRAY *out_arr, STRINGDAT *false_string) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    stamp_globals(csound, h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { out_arr, mask_arr, true_arr, false_arr }, 4);

    res = where_body_string(csound, NULL, mask_arr, true_arr, false_arr, false_string);
    if (res != OK) goto done;

    size_t n_ops = false_arr == NULL ? 2 : 3;
    const CSN_ARRAY *ops[3] = { mask_arr, true_arr, false_arr };
    CSNARR_RES out = prepare_output(csound, h, NULL, reg, out_arr, mask_arr->ndim, mask_arr->shape, CSN_STRING, ops, n_ops, CSN_ALIAS_REFUSE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    where_string_assign(csound, out_arr, mask_arr, true_arr, false_arr, false_string);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnstrings_where_hh(CSOUND *csound, CSN_WHERE_HH *p) {
    return where_string_helper(csound, &p->h, p->source_handle, p->source_handle_true, p->source_handle_false, p->handle, NULL);
}

int32_t csnstrings_where_hs(CSOUND *csound, CSN_WHERESTRING_HS *p) {
    return where_string_helper(csound, &p->h, p->source_handle, p->source_handle_true, NULL, p->handle, p->source_string_false);
}

/* The k forms keep a copy of the last false string in k_data, released here
   and before a reinit takes the struct again. */
int32_t csnstrings_where_deinit(CSOUND *csound, void *p) {
    CSN_WHERE_COMMON *ptr = (CSN_WHERE_COMMON *) p;
    csn_kdata_release_strings(csound, &ptr->k_data);
    deinit_scratch(csound, &ptr->scratch);
    return OK;
}

/* The operands may be k results their producers could not compute at init:
   the output is laid out, reserved but empty, at the mask's shape. */
static int32_t where_string_k_init_helper(CSOUND *csound, OPDS *h, CSN_ARRAY *mask_arr, CSN_ARRAY *true_arr, CSN_ARRAY *false_arr, CSN_ARRAY *out_arr, STRINGDAT *false_string, K_DATA *k_data, bool *is_published) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    csn_kdata_release_strings(csound, k_data);

    int32_t res = OK;
    stamp_globals(csound, h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { out_arr, mask_arr, true_arr, false_arr }, 4);

    res = where_body_string(csound, NULL, mask_arr, true_arr, false_arr, false_string);
    if (res != OK) goto done;

    size_t n_ops = false_arr == NULL ? 2 : 3;
    const CSN_ARRAY *ops[3] = { mask_arr, true_arr, false_arr };
    CSNARR_RES out = prepare_output(csound, h, NULL, reg, out_arr, mask_arr->ndim, mask_arr->shape, CSN_STRING, ops, n_ops, CSN_ALIAS_REFUSE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    reset_empty_csnarray(out_arr, mask_arr->ndim, mask_arr->shape, CSN_STRING);
    SET_KDATA_BEGIN(*k_data, reg, mask_arr->shape, mask_arr->ndim, CSN_STRING);
    set_array_version(&k_data->prev_output_version, &out_arr->version);
    *is_published = false;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

static int32_t where_string_k_helper(CSOUND *csound, OPDS *h, CSN_ARRAY *mask_arr, CSN_ARRAY *true_arr, CSN_ARRAY *false_arr, CSN_ARRAY *out_arr, STRINGDAT *false_string, K_DATA *k_data, bool *is_published, CSN_THREE_VERSION_K_STATE *versions, const cs_float *trig) {
    CSN_REGISTRY *reg = k_data->registry;
    CHECK_REGISTRY(csound, h, reg);

    CHECK_KTRIG(trig);

    /* a pass that selects strings copies them */
    int32_t res = check_strings_not_rt(csound, h, out_arr);
    if (res != OK) return res;

    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { out_arr, mask_arr, true_arr, false_arr }, 4);

    res = where_body_string(csound, h, mask_arr, true_arr, false_arr, false_string);
    if (res != OK) goto done;

    if (*is_published) {
        bool is_same_mask = is_same_array_version(&versions->prev_a_version, &mask_arr->version);
        bool is_same_true = is_same_array_version(&versions->prev_b_version, &true_arr->version);
        bool is_same_false = false_arr != NULL
            ? is_same_array_version(&versions->prev_c_version, &false_arr->version)
            : same_string(k_data->prev_string, false_string->data);
        bool is_same_result = is_same_array_version(&k_data->prev_output_version, &out_arr->version);
        if (is_same_mask && is_same_true && is_same_false && is_same_result) goto done;
    }

    size_t n_ops = false_arr == NULL ? 2 : 3;
    const CSN_ARRAY *ops[3] = { mask_arr, true_arr, false_arr };
    CSNARR_RES out = prepare_output(csound, h, h, reg, out_arr, mask_arr->ndim, mask_arr->shape, CSN_STRING, ops, n_ops, CSN_ALIAS_REFUSE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    where_string_assign(csound, out_arr, mask_arr, true_arr, false_arr, false_string);

    SET_KDATA_END(*k_data, out_arr->shape, out_arr->ndim, CSN_STRING);
    set_array_version(&k_data->prev_output_version, &out_arr->version);
    set_array_version(&versions->prev_a_version, &mask_arr->version);
    set_array_version(&versions->prev_b_version, &true_arr->version);
    if (false_arr != NULL) {
        set_array_version(&versions->prev_c_version, &false_arr->version);
    } else if (!same_string(k_data->prev_string, false_string->data)) {
        if (k_data->prev_string != NULL) csound->Free(csound, k_data->prev_string);
        k_data->prev_string = csound->Strdup(csound, false_string->data);
    }
    *is_published = true;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnstrings_where_hh_k_init(CSOUND *csound, CSN_WHERE_HH *p) {
    return where_string_k_init_helper(csound, &p->h, p->source_handle, p->source_handle_true, p->source_handle_false, p->handle, NULL, &p->k_data, &p->is_published);
}

int32_t csnstrings_where_hs_k_init(CSOUND *csound, CSN_WHERESTRING_HS *p) {
    return where_string_k_init_helper(csound, &p->h, p->source_handle, p->source_handle_true, NULL, p->handle, p->source_string_false, &p->k_data, &p->is_published);
}

int32_t csnstrings_where_hh_k(CSOUND *csound, CSN_WHERE_HH *p) {
    return where_string_k_helper(csound, &p->h, p->source_handle, p->source_handle_true, p->source_handle_false, p->handle, NULL, &p->k_data, &p->is_published, &p->versions, p->trig);
}

int32_t csnstrings_where_hs_k(CSOUND *csound, CSN_WHERESTRING_HS *p) {
    return where_string_k_helper(csound, &p->h, p->source_handle, p->source_handle_true, NULL, p->handle, p->source_string_false, &p->k_data, &p->is_published, &p->versions, p->trig);
}

/* csnputmask(dest, mask, values), as np.putmask: where the mask is non-zero
   dest takes the value, elsewhere it keeps its own. A slot that already holds
   the value is left alone, as csnset leaves it, so a pass that changes
   nothing allocates nothing. On a marked array at perf time the first slot
   that would change stops the walk before anything is written, since the
   slots before it were all unchanged. The copy is made before the old string
   is released: values may be dest itself. */
static int32_t putmask_string_locked(CSOUND *csound, OPDS *perf_h, CSN_ARRAY *dest, const CSN_ARRAY *mask_arr, const CSN_ARRAY *values_arr, const STRINGDAT *value) {
    int32_t res = check_string_mask(csound, perf_h, mask_arr, dest, "destination");
    if (res != OK) return res;

    if (values_arr != NULL) {
        res = check_string_mask(csound, perf_h, mask_arr, values_arr, "values array");
        if (res != OK) return res;
    } else if (!IS_VALID_STRINGDAT(value)) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Invalid/NULL string");
    }

    bool refuse_alloc = perf_h != NULL && dest->rt_locked;
    bool written = false;
    for (size_t i = 0; i < dest->size; i++) {
        if (mask_arr->data[i] == 0.0) continue;
        const char *item = values_arr != NULL ? values_arr->sdata[i] : value->data;
        if (item == NULL) item = "";
        char *stored = dest->sdata[i];
        if (stored != NULL && strcmp(stored, item) == 0) continue;
        if (refuse_alloc) {
            res = csn_locked_perf_error(csound, perf_h, "[csnarray] Writing a string allocates at perf time and the array is on a real-time path; set it at i-rate, clear the mark with csnrtunlock, or pass irt=0 at the audio source it descends from");
            break;
        }
        dest->sdata[i] = csound->Strdup(csound, item);
        if (stored != NULL) csound->Free(csound, stored);
        written = true;
    }

    if (written) update_array_data_version(&dest->version);
    return res;
}

static int32_t putmask_string_helper(CSOUND *csound, CSN_PUTMASKSTRING *p, OPDS *perf_h, const CSN_ARRAY *values_arr, const STRINGDAT *value) {
    CSN_REGISTRY *reg = p->registry;
    bool locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->dest_handle, p->mask_handle, values_arr }, 3);
    int32_t res = putmask_string_locked(csound, perf_h, p->dest_handle, p->mask_handle, values_arr, value);
    if (locked) csn_unlock(csound, reg);
    return res;
}

static int32_t putmask_string_init(CSOUND *csound, CSN_PUTMASKSTRING *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    p->registry = reg;
    stamp_globals(csound, &p->h);
    return OK;
}

int32_t csnstrings_putmask_hh(CSOUND *csound, CSN_PUTMASKSTRING *p) {
    int32_t res = putmask_string_init(csound, p);
    if (res != OK) return res;
    return putmask_string_helper(csound, p, NULL, (const CSN_ARRAY *) p->values, NULL);
}

int32_t csnstrings_putmask_hs(CSOUND *csound, CSN_PUTMASKSTRING *p) {
    int32_t res = putmask_string_init(csound, p);
    if (res != OK) return res;
    return putmask_string_helper(csound, p, NULL, NULL, (const STRINGDAT *) p->values);
}

int32_t csnstrings_putmask_k_init(CSOUND *csound, CSN_PUTMASKSTRING *p) {
    int32_t res = putmask_string_init(csound, p);
    /* the target changes at perf: whatever reads it follows it there */
    if (res == OK) csn_mark_krate(p->dest_handle);
    return res;
}

int32_t csnstrings_putmask_hh_k(CSOUND *csound, CSN_PUTMASKSTRING *p) {
    CHECK_REGISTRY(csound, &p->h, p->registry);
    CHECK_KTRIG(p->trig);
    return putmask_string_helper(csound, p, &p->h, (const CSN_ARRAY *) p->values, NULL);
}

int32_t csnstrings_putmask_hs_k(CSOUND *csound, CSN_PUTMASKSTRING *p) {
    CHECK_REGISTRY(csound, &p->h, p->registry);
    CHECK_KTRIG(p->trig);
    return putmask_string_helper(csound, p, &p->h, NULL, (const STRINGDAT *) p->values);
}
