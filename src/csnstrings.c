#include "csnstrings.h"
#include "csnregistry.h"
#include "csnum.h"
#include "csnum_internal.h"
#include "csnfile.h"
#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "arrays.h"


/* Grows a slot to hold at least bytes bytes, terminator included, keeping
   what it holds. A string that fits costs nothing; growth on a real-time path
   is refused at perf, so an rt-locked output runs while its strings fit the
   buffers reserved at init. */
static int32_t refuse_string_growth(CSOUND *csound, OPDS *perf_h, size_t bytes, size_t capacity) {
    return csn_locked_perf_error(csound, perf_h, "[csnarray] A string needs %zu bytes, its slot holds %zu, and growing it allocates at perf time on a real-time path; build it at i-rate so the slots reserve their room, clear the mark with csnrtunlock, or pass irt=0 at the audio source it descends from", bytes, capacity);
}

/* Grows a slot to hold at least bytes bytes, terminator included, keeping
   what it holds. A string that fits costs nothing; growth on a real-time path
   is refused at perf, so an rt-locked output runs while its strings fit the
   buffers reserved at init. */
int32_t ensure_string_capacity(CSOUND *csound, OPDS *perf_h, CSN_STRDATA *slot, size_t bytes, bool rt_locked) {
    if (bytes <= slot->capacity) return OK;
    if (perf_h != NULL && rt_locked) return refuse_string_growth(csound, perf_h, bytes, slot->capacity);
    if (csn_str_reserve(csound, slot, bytes) != OK) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Out of memory: allocation of %zu bytes failed", bytes);
    }
    return OK;
}

/* Copies value into the slot through w; value may be the slot's own string,
   and NULL stores "". */
int32_t csn_str_write(CSN_STR_WRITER *w, CSN_STRDATA *slot, const char *value, size_t len) {
    if (value == NULL) {
        value = "";
        len = 0;
    }
    if (value == slot->data) return OK;
    size_t bytes = len + 1;
    int32_t res = ensure_string_capacity(w->csound, w->perf_h, slot, bytes, w->no_growth);
    if (res != OK) return w->err = res;
    memmove(slot->data, value, bytes);
    return OK;
}

/* The error csn_str_write would raise for value, without writing anything.
   An in-place pass asks this of every slot before it writes one, so a pass
   that stops leaves the array as it was. */
int32_t csn_str_check(CSN_STR_WRITER *w, const CSN_STRDATA *slot, const char *value) {
    if (!w->no_growth || value == NULL || value == slot->data) return OK;
    size_t bytes = strlen(value) + 1;
    if (bytes <= slot->capacity) return OK;
    return w->err = refuse_string_growth(w->csound, w->perf_h, bytes, slot->capacity);
}

static bool IS_VALID_STRINGDAT(const STRINGDAT *value) {
    return value != NULL && value->data != NULL;
}

/* Every slot owns its string, so the value is written into the slot's
   buffer: an aliased output reaches here with its strings still in place. At
   init a marked array gives the fill room in every slot it can grow into, so
   a k pass that reshapes it within its capacity still writes in place. */
static int32_t fill_csnstrings(CSN_STR_WRITER *w, CSN_ARRAY *array, const char *value) {
    if (w->perf_h == NULL && array->rt_locked) {
        if (csn_str_reserve_slots(w->csound, array, strlen(value) + 1) != OK) {
            return w->err = csn_init_error(w->csound, "[csnarray] Out of memory: reserving the string slots failed");
        }
    }
    for (size_t i = 0; i < array->size; i++) {
        int32_t res = csn_str_write(w, &array->sdata[i], value, GET_STRINGDAT_LEN(value));
        if (res != OK) return res;
    }
    return OK;
}

/* For the k forms whose pass cannot write into reserved buffers: csnload
   reads its strings fresh from the file, and the in-place forms that rebuild
   the array in a working copy hand their buffers back and forth every pass.
   Every other string writer goes through a CSN_STR_WRITER, which refuses only
   the string that would grow its slot. The mark is decided at init. */
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

    CSN_STR_WRITER w = csn_str_writer(csound, NULL, p->handle);
    int32_t res = fill_csnstrings(&w, p->handle, p->value->data);

    if (pres.locked) csn_unlock(csound, reg);
    return res;
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

    CSN_STR_WRITER w = csn_str_writer(csound, &p->h, p->handle);
    res = fill_csnstrings(&w, p->handle, p->value->data);

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

    CSN_STR_WRITER w = csn_str_writer(csound, NULL, p->handle);
    res = fill_csnstrings(&w, p->handle, p->value->data);
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

    int32_t res = OK;
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
       change, so the output has the requested layout either way. In place the
       slots are checked first, so a pass that stops leaves them as they were. */
    CSN_STR_WRITER w = csn_str_writer(csound, &p->h, p->handle);
    if (pres.aliased) {
        for (size_t i = 0; i < p->handle->size && res == OK; i++) res = csn_str_check(&w, &p->handle->sdata[i], p->value->data);
    }
    if (res == OK) res = fill_csnstrings(&w, p->handle, p->value->data);
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

    COPY_STRING_TO_STRINGDAT(csound, h, out, source_arr->sdata[offset].data);
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
    const char *stored = source_arr->sdata[offset].data;
    if (stored != NULL && strcmp(stored, value) == 0) return OK;

    /* The string goes into the slot's buffer: on a real-time path one that
       does not fit is refused, before anything changes. */
    CSN_STR_WRITER w = csn_str_writer(csound, perf_h, source_arr);
    res = csn_str_write(&w, &source_arr->sdata[offset], value, GET_STRINGDAT_LEN(value));
    if (res != OK) return res;
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
        csn_str_set(csound, &p->handle->sdata[i], items[i].data);
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

    CSNARR_RES pres = prepare_output(csound, &p->h, &p->h, reg, p->handle, ndim, shape, CSN_STRING, NULL, 0, CSN_ALIAS_REFUSE);
    int32_t res = pres.err;
    if (res != OK) goto done;

    const STRINGDAT *items = (const STRINGDAT *) p->source->data;
    CSN_STR_WRITER w = csn_str_writer(csound, &p->h, p->handle);
    for (size_t i = 0; i < requested_size && res == OK; i++) {
        res = csn_str_write(&w, &p->handle->sdata[i], items[i].data, GET_STRINGDAT_LEN(items[i].data));
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
        COPY_STRING_TO_STRINGDAT(csound, &p->h, &items[i], src->sdata[i].data);
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
        COPY_STRING_TO_STRINGDAT(csound, &p->h, &items[i], src->sdata[i].data);
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
            csn_str_release(csound, &out->sdata[i]);
            out->sdata[i].data = items[i];
            out->sdata[i].capacity = strlen(items[i]) + 1;
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
   writes every string into destination's slots; in place, source and
   destination hold the same strings, so the slots are only permuted (into a
   scratch buffer the caller copies back) and nothing is allocated. */
int32_t transpose_stringdata_assign(CSN_STR_WRITER *w, CSN_STRDATA *source, CSN_STRDATA *destination, size_t size, uint32_t ndim, const uint32_t *shape, const size_t *strides, const uint32_t *axes, bool in_place) {
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
            int32_t res = csn_str_write(w, &destination[linear], source[src_index].data, GET_STRINGDAT_LEN(source[src_index].data));
            if (res != OK) return res;
        }
    }

    return OK;
}

/* Moving strings around inside one array only swaps the slots, buffer and
   capacity together: each string keeps its single owner and nothing is
   allocated, so the in-place flip, roll and reverse are safe on a real-time
   path. */
void swap_strings(CSN_STRDATA *data, size_t a, size_t b) {
    CSN_STRDATA tmp = data[a];
    data[a] = data[b];
    data[b] = tmp;
}

static void reverse_string_run(CSN_STRDATA *data, size_t base, size_t count, size_t stride) {
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
void rotate_string_run(CSN_STRDATA *data, size_t base, size_t count, size_t stride, int32_t shift) {
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

int32_t concat_block_string_assign_value(CSN_STR_WRITER *w, const CSN_ARRAY *source_arr, const CSN_ARRAY *data_arr, CSN_ARRAY *destination, uint32_t axis) {
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
            from = source_arr->sdata[source_off].data;
        } else {
            size_t block_off = 0;
            for (uint32_t d = 0; d < data_arr->ndim; ++d) {
                uint32_t coord = it.coords[d] - (d == axis ? source_axis_extent : 0U);
                block_off += (size_t) coord * data_arr->strides[d];
            }
            from = data_arr->sdata[block_off].data;
        }
        int32_t res = csn_str_write(w, &destination->sdata[linear], from, GET_STRINGDAT_LEN(from));
        if (res != OK) return res;
    }

    return OK;
}

int32_t pad_assign_string_value(CSN_STR_WRITER *w, const CSN_ARRAY *source_arr, CSN_ARRAY *destination, STRINGDAT *value, int32_t axis, uint32_t before) {
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

        const char *s = is_inside ? source_arr->sdata[source_off].data : value->data;
        int32_t res = csn_str_write(w, &destination->sdata[linear], s, GET_STRINGDAT_LEN(s));
        if (res != OK) return res;
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
    int32_t res = csnarray_pad_k_init_helper(csound, &p->h, &p->k_data, p->handle, p->source_handle, p->before, p->after, NULL, 0.0, NULL, p->value, CSN_STRING, axis_in);
    /* reserve the copy the perf pass compares against, and room in every
       slot for the fill, so a pass on a real-time path allocates nothing */
    if (res == OK) csn_kdata_keep_strings(csound, &p->k_data, p->value->data, NULL, true);
    if (res == OK && p->handle->rt_locked) {
        size_t bytes = strlen(p->value->data) + 1;
        size_t longest = csn_str_longest(p->source_handle);
        if (csn_str_reserve_slots(csound, p->handle, longest > bytes ? longest : bytes) != OK) {
            res = csn_init_error(csound, "[csnarray] Out of memory: reserving the string slots of '%s' failed", get_out_name(&p->h));
        }
    }
    return res;
}

int32_t csnstrings_pad_k(CSOUND *csound, CSN_PADSTRING *p) {
    const cs_float *axis_in = p->INOCOUNT > 5 ? p->arg_b : NULL;
    if (!IS_VALID_STRINGDAT(p->value)) {
        return csn_perf_error(csound, &p->h, "[csnarray] Invalid/NULL string");
    }
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

/* In place the padded image is built in a working copy whose slots then
   trade places with the source's, so the buffers move every pass and none
   stays reserved: a source on a real-time path is refused. */
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

    COPY_STRING_TO_STRINGDAT(csound, &p->h, p->value, arr->sdata[(size_t) *p->index].data);
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
   buffer moves to index and takes the copy, so a value that fits it (the
   string a pop left there, say) allocates nothing. */
static int32_t insertstring_at(CSOUND *csound, OPDS *perf_h, CSN_ARRAY *arr, size_t index, const char *value) {
    size_t new_size = arr->size + 1;
    int32_t res = ensure_mutation_capacity(csound, perf_h, arr, new_size, arr->rt_locked);
    if (res != OK) return res;

    /* on a real-time path the spare buffer must already hold the value */
    CSN_STR_WRITER w = csn_str_writer(csound, perf_h, arr);
    res = csn_str_check(&w, &arr->sdata[arr->size], value);
    if (res != OK) return res;

    CSN_STRDATA spare = arr->sdata[arr->size];
    memmove(arr->sdata + index + 1, arr->sdata + index, sizeof(*arr->sdata) * (arr->size - index));
    arr->sdata[index] = spare;
    res = csn_str_write(&w, &arr->sdata[index], value, GET_STRINGDAT_LEN(value));
    if (res != OK) return res;

    arr->size = new_size;
    arr->shape[0] = (uint32_t) new_size;
    update_array_data_version(&arr->version);
    update_array_layout_version(&arr->version, true, false, false);
    return OK;
}

/* Hands the string at index to out and closes the gap; the string itself
   moves to the slot just past the new end. */
static void removestring_at(CSOUND *csound, OPDS *h, CSN_ARRAY *arr, size_t index, STRINGDAT *out) {
    COPY_STRING_TO_STRINGDAT(csound, h, out, arr->sdata[index].data);

    CSN_STRDATA removed = arr->sdata[index];
    size_t new_size = arr->size - 1;
    memmove(arr->sdata + index, arr->sdata + index + 1, sizeof(*arr->sdata) * (new_size - index));
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

    bool locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);

    CSN_ARRAY *arr = p->source_handle;
    int32_t res = check_pushstring_body(csound, &p->h, arr);
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

    bool locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);
    int32_t res = insertstring_value_locked(csound, &p->h, p->source_handle, (double) *p->arg_a, p->in_value->data);
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

static int32_t where_string_assign(CSN_STR_WRITER *w, CSN_ARRAY *out_arr, const CSN_ARRAY *mask_arr, const CSN_ARRAY *true_arr, const CSN_ARRAY *false_arr, const STRINGDAT *false_string) {
    for (size_t i = 0; i < out_arr->size; i++) {
        const char *item = mask_arr->data[i] != 0.0 ? true_arr->sdata[i].data : false_arr != NULL ? false_arr->sdata[i].data : false_string->data;
        int32_t res = csn_str_write(w, &out_arr->sdata[i], item, GET_STRINGDAT_LEN(item));
        if (res != OK) return res;
    }
    return OK;
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

    CSN_STR_WRITER w = csn_str_writer(csound, NULL, out_arr);
    res = where_string_assign(&w, out_arr, mask_arr, true_arr, false_arr, false_string);

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
   the output is then laid out, reserved but empty, at the mask's shape. When
   they all hold their elements the selection is made here, which also gives
   the slots (and the copy of the false string) the room a pass on a real-time
   path writes into; the first pass still makes it again. */
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

    bool filled = mask_arr->size > 0 && true_arr->size == mask_arr->size && (false_arr == NULL || false_arr->size == mask_arr->size);
    if (filled) {
        CSN_STR_WRITER w = csn_str_writer(csound, NULL, out_arr);
        res = where_string_assign(&w, out_arr, mask_arr, true_arr, false_arr, false_string);
        if (res != OK) goto done;
        if (false_arr == NULL) csn_kdata_keep_strings(csound, k_data, false_string->data, NULL, true);
    } else {
        reset_empty_csnarray(out_arr, mask_arr->ndim, mask_arr->shape, CSN_STRING);
    }
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

    int32_t res = OK;
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { out_arr, mask_arr, true_arr, false_arr }, 4);

    res = where_body_string(csound, h, mask_arr, true_arr, false_arr, false_string);
    if (res != OK) goto done;

    if (*is_published) {
        bool is_same_mask = is_same_array_version(&versions->prev_a_version, &mask_arr->version);
        bool is_same_true = is_same_array_version(&versions->prev_b_version, &true_arr->version);
        bool is_same_false = false_arr != NULL
            ? is_same_array_version(&versions->prev_c_version, &false_arr->version)
            : !k_data->prev_strings_stale && same_string(k_data->prev_string.data, false_string->data);
        bool is_same_result = is_same_array_version(&k_data->prev_output_version, &out_arr->version);
        if (is_same_mask && is_same_true && is_same_false && is_same_result) goto done;
    }

    size_t n_ops = false_arr == NULL ? 2 : 3;
    const CSN_ARRAY *ops[3] = { mask_arr, true_arr, false_arr };
    CSNARR_RES out = prepare_output(csound, h, h, reg, out_arr, mask_arr->ndim, mask_arr->shape, CSN_STRING, ops, n_ops, CSN_ALIAS_REFUSE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    CSN_STR_WRITER w = csn_str_writer(csound, h, out_arr);
    res = where_string_assign(&w, out_arr, mask_arr, true_arr, false_arr, false_string);
    if (res != OK) goto done;

    SET_KDATA_END(*k_data, out_arr->shape, out_arr->ndim, CSN_STRING);
    set_array_version(&k_data->prev_output_version, &out_arr->version);
    set_array_version(&versions->prev_a_version, &mask_arr->version);
    set_array_version(&versions->prev_b_version, &true_arr->version);
    if (false_arr != NULL) {
        set_array_version(&versions->prev_c_version, &false_arr->version);
    } else {
        csn_kdata_keep_strings(csound, k_data, false_string->data, NULL, !out_arr->rt_locked);
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
   the value is left alone, as csnset leaves it, and a value is written into
   the slot's buffer. On a marked array at perf time a value that does not fit
   its slot stops the pass before anything is written. values may be dest itself: the copy
   goes into the slot's own buffer, which csn_str_set allows. */
static int32_t putmask_string_locked(CSOUND *csound, OPDS *perf_h, CSN_ARRAY *dest, const CSN_ARRAY *mask_arr, const CSN_ARRAY *values_arr, const STRINGDAT *value) {
    int32_t res = check_string_mask(csound, perf_h, mask_arr, dest, "destination");
    if (res != OK) return res;

    if (values_arr != NULL) {
        res = check_string_mask(csound, perf_h, mask_arr, values_arr, "values array");
        if (res != OK) return res;
    } else if (!IS_VALID_STRINGDAT(value)) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Invalid/NULL string");
    }

    /* on a real-time path every slot is checked before one is written */
    CSN_STR_WRITER w = csn_str_writer(csound, perf_h, dest);
    bool written = false;
    for (int32_t pass = w.no_growth ? 0 : 1; pass < 2 && res == OK; pass++) {
        for (size_t i = 0; i < dest->size; i++) {
            if (mask_arr->data[i] == 0.0) continue;
            const char *item = values_arr != NULL ? values_arr->sdata[i].data : value->data;
            if (item == NULL) item = "";
            const char *stored = dest->sdata[i].data;
            if (stored != NULL && strcmp(stored, item) == 0) continue;
            res = pass == 0 ? csn_str_check(&w, &dest->sdata[i], item) : csn_str_write(&w, &dest->sdata[i], item, GET_STRINGDAT_LEN(item));
            if (res != OK) break;
            written |= pass == 1;
        }
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

/* ---------- csnstradd and csnstrmul ----------

   Each result slot is written into its own buffer, so a pass allocates only
   for a result longer than the slot held. On a real-time path a slot may not
   grow: every slot is measured first and a pass that would grow one stops
   before anything is written (ensure_string_capacity refuses it), leaving the
   output as the previous pass left it. The output may be one of the operands
   (X = csnstradd(X, Y)); its offset is then the linear index itself, so each
   slot is read before it is written. */

/* Measures the slot (probe) or writes a + b into it. */
static int32_t concat_into_slot(CSOUND *csound, OPDS *perf_h, CSN_ARRAY *arr, size_t i, const CSN_STRDATA *slot_a, const CSN_STRDATA *slot_b, const char *left, const char *right, bool probe) {
    CSN_STRDATA *slot = &arr->sdata[i];
    const char *a = slot_a != NULL ? slot_a->data : left;
    const char *b = slot_b != NULL ? slot_b->data : right;
    size_t la = a != NULL ? strlen(a) : 0;
    size_t lb = b != NULL ? strlen(b) : 0;
    int32_t res = ensure_string_capacity(csound, perf_h, slot, la + lb + 1, arr->rt_locked);
    if (res != OK || probe) return res;

    /* the slot may be an operand, and growing it may have moved it */
    if (slot_a != NULL) a = slot_a->data;
    if (slot_b != NULL) b = slot_b->data;
    if (b == slot->data) {
        /* b first: it sits at the start of the slot */
        if (lb > 0) memmove(slot->data + la, b, lb);
        if (la > 0) memmove(slot->data, a, la);
    } else {
        /* a first: it may sit at the start of the slot */
        if (la > 0) memmove(slot->data, a, la);
        if (lb > 0) memmove(slot->data + la, b, lb);
    }
    slot->data[la + lb] = '\0';
    return OK;
}

static int32_t addstring_hh_assign_value(CSOUND *csound, OPDS *perf_h, const CSN_BROADCAST_ITER *layout, const CSN_ARRAY *source_arr_a, const CSN_ARRAY *source_arr_b, CSN_ARRAY *arr) {
    bool probe_first = perf_h != NULL && arr->rt_locked;
    for (int32_t pass = probe_first ? 0 : 1; pass < 2; pass++) {
        CSN_BROADCAST_ITER it = *layout;
        while (BROADCAST_ITER_NEXT(&it)) {
            int32_t res = concat_into_slot(csound, perf_h, arr, it.linear_index, &source_arr_a->sdata[it.offsets[0]], &source_arr_b->sdata[it.offsets[1]], NULL, NULL, pass == 0);
            if (res != OK) return res;
        }
    }

    return OK;
}

/* is_left puts the string before each element (csnstradd(S, X)). */
static int32_t addstring_hs_sh_assign_value(CSOUND *csound, OPDS *perf_h, const CSN_ARRAY *source_arr, const STRINGDAT *string, CSN_ARRAY *arr, bool is_left) {
    bool probe_first = perf_h != NULL && arr->rt_locked;
    for (int32_t pass = probe_first ? 0 : 1; pass < 2; pass++) {
        for (size_t i = 0; i < source_arr->size; i++) {
            const CSN_STRDATA *item = &source_arr->sdata[i];
            int32_t res = is_left
                ? concat_into_slot(csound, perf_h, arr, i, NULL, item, string->data, NULL, pass == 0)
                : concat_into_slot(csound, perf_h, arr, i, item, NULL, NULL, string->data, pass == 0);
            if (res != OK) return res;
        }
    }

    return OK;
}

static int32_t string_broadcast_layout(CSOUND *csound, OPDS *perf_h, const CSN_ARRAY *a, const CSN_ARRAY *b, uint32_t *ndim, uint32_t *shape, CSN_BROADCAST_ITER *it) {
    size_t size = 0;
    size_t broad_strides[CSN_MAX_BROADCAST_INPUTS][CSN_MAX_DIMS] = {0};
    const CSN_ARRAY *sources[2] = { a, b };

    char abuf[CSN_SHAPE_STR_MAX], bbuf[CSN_SHAPE_STR_MAX];
    if (BROADCAST_INIT(ndim, shape, &size, broad_strides, sources, 2) != OK || BROADCAST_ITER_INIT(it, *ndim, shape, size, broad_strides, 2) != OK) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Shapes %s and %s cannot be broadcast together: aligned from the last axis, each pair must match or be 1", shape_str(abuf, sizeof(abuf), a->shape, a->ndim), shape_str(bbuf, sizeof(bbuf), b->shape, b->ndim));
    }

    return OK;
}

/* ---------- csnstrmul ---------- */

/* np.strings.multiply: a count below 0 repeats 0 times. A count must be a
   whole number; it is checked before anything is written. */
static bool is_repeat_count(double count) {
    return isfinite(count) && trunc(count) == count;
}

static int32_t check_repeat_count(CSOUND *csound, OPDS *perf_h, double count) {
    if (!is_repeat_count(count)) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] A repeat count must be a whole number, got %g", count);
    }

    return OK;
}

static int32_t strmul_counts_body(CSOUND *csound, OPDS *perf_h, const CSN_ARRAY *counts) {
    if (counts->itype != CSN_REAL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Repeat counts must be a real array");
    }
    for (size_t i = 0; i < counts->size; i++) {
        int32_t res = check_repeat_count(csound, perf_h, counts->data[i]);
        if (res != OK) return res;
    }

    return OK;
}

/* Measures the slot (probe) or writes count copies of the item into it. */
static int32_t repeat_into_slot(CSOUND *csound, OPDS *perf_h, CSN_ARRAY *arr, size_t i, const CSN_STRDATA *item, double count, bool probe) {
    CSN_STRDATA *slot = &arr->sdata[i];
    size_t la = item->data != NULL ? strlen(item->data) : 0;
    /* past 2^63 any non-empty string overflows below; the cap keeps the
       conversion defined */
    size_t reps = count > 0.0 && la > 0 ? (size_t) fmin(count, 0x1p63) : 0;
    if (reps > 0 && la > (SIZE_MAX - 1) / reps) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Repeating a %zu-byte string %g times overflows the string size", la, count);
    }
    size_t total = la * reps;
    int32_t res = ensure_string_capacity(csound, perf_h, slot, total + 1, arr->rt_locked);
    if (res != OK || probe) return res;

    /* the item may be the slot itself, and growing it may have moved it; the
       first copy goes in, then each step doubles what is already there */
    if (total > 0 && item->data != slot->data) memmove(slot->data, item->data, la);
    for (size_t done = la; done < total;) {
        size_t chunk = done < total - done ? done : total - done;
        memcpy(slot->data + done, slot->data, chunk);
        done += chunk;
    }
    slot->data[total] = '\0';
    return OK;
}

static int32_t strmul_hh_assign_value(CSOUND *csound, OPDS *perf_h, const CSN_BROADCAST_ITER *layout, const CSN_ARRAY *source_arr, const CSN_ARRAY *counts, CSN_ARRAY *arr) {
    bool probe_first = perf_h != NULL && arr->rt_locked;
    for (int32_t pass = probe_first ? 0 : 1; pass < 2; pass++) {
        CSN_BROADCAST_ITER it = *layout;
        while (BROADCAST_ITER_NEXT(&it)) {
            int32_t res = repeat_into_slot(csound, perf_h, arr, it.linear_index, &source_arr->sdata[it.offsets[0]], counts->data[it.offsets[1]], pass == 0);
            if (res != OK) return res;
        }
    }

    return OK;
}

static int32_t strmul_hs_assign_value(CSOUND *csound, OPDS *perf_h, const CSN_ARRAY *source_arr, double count, CSN_ARRAY *arr) {
    bool probe_first = perf_h != NULL && arr->rt_locked;
    for (int32_t pass = probe_first ? 0 : 1; pass < 2; pass++) {
        for (size_t i = 0; i < source_arr->size; i++) {
            int32_t res = repeat_into_slot(csound, perf_h, arr, i, &source_arr->sdata[i], count, pass == 0);
            if (res != OK) return res;
        }
    }

    return OK;
}

/* ---------- array-array forms (csnstradd hh, csnstrmul hh) ---------- */

static int32_t string_binop_hh_body(CSOUND *csound, OPDS *perf_h, const CSN_ARRAY *b, CSN_STR_BINOP op) {
    return op == CSN_STR_MUL ? strmul_counts_body(csound, perf_h, b) : OK;
}

static int32_t string_binop_hh_assign(CSOUND *csound, OPDS *perf_h, const CSN_BROADCAST_ITER *it, const CSN_ARRAY *a, const CSN_ARRAY *b, CSN_ARRAY *out, CSN_STR_BINOP op) {
    return op == CSN_STR_MUL ? strmul_hh_assign_value(csound, perf_h, it, a, b, out) : addstring_hh_assign_value(csound, perf_h, it, a, b, out);
}

/* As csn_binop_hh_init: an output that is its own operand is left to the
   perf pass of a k form, or X = csnstradd(X, Y) would apply twice in the
   first cycle. Computing here also reserves the slots' buffers, which is what
   lets a k pass on a real-time path run without allocating. */
int32_t string_binop_hh_init(CSOUND *csound, OPDS *h, CSN_ARRAY *out, CSN_ARRAY *a, CSN_ARRAY *b, K_DATA *k_data, CSN_STR_BINOP op, bool perf_redoes) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    stamp_globals(csound, h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { out, a, b }, 3);

    res = string_binop_hh_body(csound, NULL, b, op);
    if (res != OK) goto done;

    uint32_t new_ndim = 0;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    CSN_BROADCAST_ITER it = {0};
    res = string_broadcast_layout(csound, NULL, a, b, &new_ndim, new_shape, &it);
    if (res != OK) goto done;

    const CSN_ARRAY *sources[2] = { a, b };
    CSNARR_RES r = prepare_output(csound, h, NULL, reg, out, new_ndim, new_shape, CSN_STRING, sources, 2, CSN_ALIAS_ELEMENTWISE);
    res = r.err;
    if (res != OK) goto done;
    pres.locked |= r.locked;

    bool compute = !perf_redoes || !r.aliased;
    if (compute) {
        res = string_binop_hh_assign(csound, NULL, &it, a, b, out, op);
        if (res != OK) goto done;
        if (r.aliased) update_array_data_version(&out->version);
    }

    SET_KDATA_BEGIN(*k_data, reg, new_shape, new_ndim, CSN_STRING);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t string_binop_hh_perf(CSOUND *csound, OPDS *h, CSN_ARRAY *out, CSN_ARRAY *a, CSN_ARRAY *b, K_DATA *k_data, CSN_STR_BINOP op) {
    CSN_REGISTRY *reg = k_data->registry;
    CHECK_REGISTRY(csound, h, reg);

    int32_t res = OK;
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { out, a, b }, 3);

    /* never true for an output that is one of its operands */
    if (CAN_REUSE_ELEMENTWISE(k_data, a, b, out, 0.0, 0.0)) goto done;

    res = string_binop_hh_body(csound, h, b, op);
    if (res != OK) goto done;

    uint32_t new_ndim = 0;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    CSN_BROADCAST_ITER it = {0};
    res = string_broadcast_layout(csound, h, a, b, &new_ndim, new_shape, &it);
    if (res != OK) goto done;

    const CSN_ARRAY *sources[2] = { a, b };
    CSNARR_RES r = prepare_output(csound, h, h, reg, out, new_ndim, new_shape, CSN_STRING, sources, 2, CSN_ALIAS_ELEMENTWISE);
    res = r.err;
    if (res != OK) goto done;
    pres.locked |= r.locked;

    res = string_binop_hh_assign(csound, h, &it, a, b, out, op);
    if (res != OK) goto done;
    if (r.aliased) update_array_data_version(&out->version);

    SET_KDATA_END(*k_data, new_shape, new_ndim, CSN_STRING);
    PUBLISH_ELEMENTWISE(k_data, a, b, out, 0.0, 0.0);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnstrings_add_hh(CSOUND *csound, CSN_BINOP_HH *p) {
    return string_binop_hh_init(csound, &p->h, p->handle, p->source_handle_a, p->source_handle_b, &p->k_data, CSN_STR_ADD, false);
}

int32_t csnstrings_add_hh_k_init(CSOUND *csound, CSN_BINOP_HH *p) {
    return string_binop_hh_init(csound, &p->h, p->handle, p->source_handle_a, p->source_handle_b, &p->k_data, CSN_STR_ADD, true);
}

int32_t csnstrings_add_hh_k(CSOUND *csound, CSN_BINOP_HH *p) {
    CHECK_KTRIG(p->trig);
    return string_binop_hh_perf(csound, &p->h, p->handle, p->source_handle_a, p->source_handle_b, &p->k_data, CSN_STR_ADD);
}

int32_t csnstrings_mul_hh(CSOUND *csound, CSN_BINOP_HH *p) {
    return string_binop_hh_init(csound, &p->h, p->handle, p->source_handle_a, p->source_handle_b, &p->k_data, CSN_STR_MUL, false);
}

int32_t csnstrings_mul_hh_k_init(CSOUND *csound, CSN_BINOP_HH *p) {
    return string_binop_hh_init(csound, &p->h, p->handle, p->source_handle_a, p->source_handle_b, &p->k_data, CSN_STR_MUL, true);
}

int32_t csnstrings_mul_hh_k(CSOUND *csound, CSN_BINOP_HH *p) {
    CHECK_KTRIG(p->trig);
    return string_binop_hh_perf(csound, &p->h, p->handle, p->source_handle_a, p->source_handle_b, &p->k_data, CSN_STR_MUL);
}

/* ---------- csnstradd with one string (hs / sh) ---------- */

/* The k forms keep a copy of the string in k_data, released here and at the
   start of a k init, before a reinit takes the struct again. */
int32_t addstring_hs_sh_init(CSOUND *csound, OPDS *h, CSN_ARRAY *out, CSN_ARRAY *source_arr, STRINGDAT *value, K_DATA *k_data, bool perf_redoes, bool is_left) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    if (perf_redoes) csn_kdata_release_strings(csound, k_data);

    if (!IS_VALID_STRINGDAT(value)) {
        return csn_init_error(csound, "[csnarray] Invalid/NULL string");
    }

    int32_t res = OK;
    stamp_globals(csound, h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { out, source_arr }, 2);

    const CSN_ARRAY *ops[1] = { source_arr };
    CSNARR_RES r = prepare_output(csound, h, NULL, reg, out, source_arr->ndim, source_arr->shape, CSN_STRING, ops, 1, CSN_ALIAS_ELEMENTWISE);
    res = r.err;
    if (res != OK) goto done;
    pres.locked |= r.locked;

    /* An S argument has no rate, so it always holds its value here; only an
       output that is its own operand is left to the perf pass. */
    bool compute = !perf_redoes || !r.aliased;
    if (compute) {
        res = addstring_hs_sh_assign_value(csound, NULL, source_arr, value, out, is_left);
        if (res != OK) goto done;
        if (r.aliased) update_array_data_version(&out->version);
    }

    /* reserve the copy the perf pass compares against, so a pass on a
       real-time path can keep it without allocating */
    if (perf_redoes) csn_kdata_keep_strings(csound, k_data, value->data, NULL, true);

    SET_KDATA_BEGIN(*k_data, reg, source_arr->shape, source_arr->ndim, CSN_STRING);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t addstring_hs_sh_perf(CSOUND *csound, OPDS *h, CSN_ARRAY *out, CSN_ARRAY *source_arr, STRINGDAT *value, K_DATA *k_data, bool is_left) {
    CSN_REGISTRY *reg = k_data->registry;
    CHECK_REGISTRY(csound, h, reg);

    if (!IS_VALID_STRINGDAT(value)) {
        return csn_perf_error(csound, h, "[csnarray] Invalid/NULL string");
    }

    int32_t res = OK;
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { out, source_arr }, 2);

    if (CAN_REUSE_STRING_ELEMENTWISE(k_data, source_arr, NULL, out, value->data, NULL)) goto done;

    const CSN_ARRAY *ops[1] = { source_arr };
    CSNARR_RES r = prepare_output(csound, h, h, reg, out, source_arr->ndim, source_arr->shape, CSN_STRING, ops, 1, CSN_ALIAS_ELEMENTWISE);
    res = r.err;
    if (res != OK) goto done;
    pres.locked |= r.locked;

    res = addstring_hs_sh_assign_value(csound, h, source_arr, value, out, is_left);
    if (res != OK) goto done;
    if (r.aliased) update_array_data_version(&out->version);

    SET_KDATA_END(*k_data, source_arr->shape, source_arr->ndim, CSN_STRING);
    PUBLISH_STRING_ELEMENTWISE(csound, k_data, source_arr, NULL, out, value->data, NULL, !out->rt_locked);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnstrings_add_hs(CSOUND *csound, CSN_BINOPSTRING_HS *p) {
    return addstring_hs_sh_init(csound, &p->h, p->handle, p->source_handle, p->value, &p->k_data, false, false);
}

int32_t csnstrings_add_hs_k_init(CSOUND *csound, CSN_BINOPSTRING_HS *p) {
    return addstring_hs_sh_init(csound, &p->h, p->handle, p->source_handle, p->value, &p->k_data, true, false);
}

int32_t csnstrings_add_hs_k(CSOUND *csound, CSN_BINOPSTRING_HS *p) {
    CHECK_KTRIG(p->trig);
    return addstring_hs_sh_perf(csound, &p->h, p->handle, p->source_handle, p->value, &p->k_data, false);
}

int32_t csnstrings_add_sh(CSOUND *csound, CSN_BINOPSTRING_SH *p) {
    return addstring_hs_sh_init(csound, &p->h, p->handle, p->source_handle, p->value, &p->k_data, false, true);
}

int32_t csnstrings_add_sh_k_init(CSOUND *csound, CSN_BINOPSTRING_SH *p) {
    return addstring_hs_sh_init(csound, &p->h, p->handle, p->source_handle, p->value, &p->k_data, true, true);
}

int32_t csnstrings_add_sh_k(CSOUND *csound, CSN_BINOPSTRING_SH *p) {
    CHECK_KTRIG(p->trig);
    return addstring_hs_sh_perf(csound, &p->h, p->handle, p->source_handle, p->value, &p->k_data, true);
}

int32_t csnstrings_add_hs_deinit(CSOUND *csound, CSN_BINOPSTRING_HS *p) {
    csn_kdata_release_strings(csound, &p->k_data);
    return OK;
}

int32_t csnstrings_add_sh_deinit(CSOUND *csound, CSN_BINOPSTRING_SH *p) {
    csn_kdata_release_strings(csound, &p->k_data);
    return OK;
}

/* ---------- csnstrmul with one count (hs) ---------- */

int32_t strmul_hs_init(CSOUND *csound, OPDS *h, CSN_ARRAY *out, CSN_ARRAY *source_arr, cs_float *count, K_DATA *k_data, bool perf_redoes) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    stamp_globals(csound, h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { out, source_arr }, 2);

    const CSN_ARRAY *ops[1] = { source_arr };
    CSNARR_RES r = prepare_output(csound, h, NULL, reg, out, source_arr->ndim, source_arr->shape, CSN_STRING, ops, 1, CSN_ALIAS_ELEMENTWISE);
    res = r.err;
    if (res != OK) goto done;
    pres.locked |= r.locked;

    /* A k count is not known yet and the first pass computes, but its init
       value still lays the strings out: the buffers it reserves are what a
       pass on a real-time path may fill. An init value that is no count
       reserves nothing and raises no error; the perf pass judges the count. */
    bool known = csn_known_at_init(csound, count);
    bool compute = !perf_redoes || !r.aliased;
    if (compute && !known && !is_repeat_count((double) *count)) compute = false;
    if (compute) {
        res = check_repeat_count(csound, NULL, (double) *count);
        if (res != OK) goto done;
        res = strmul_hs_assign_value(csound, NULL, source_arr, (double) *count, out);
        if (res != OK) goto done;
        if (r.aliased) update_array_data_version(&out->version);
    }

    SET_KDATA_BEGIN(*k_data, reg, source_arr->shape, source_arr->ndim, CSN_STRING);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t strmul_hs_perf(CSOUND *csound, OPDS *h, CSN_ARRAY *out, CSN_ARRAY *source_arr, cs_float *count, K_DATA *k_data) {
    CSN_REGISTRY *reg = k_data->registry;
    CHECK_REGISTRY(csound, h, reg);

    int32_t res = OK;
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { out, source_arr }, 2);

    double n = (double) *count;
    if (CAN_REUSE_ELEMENTWISE(k_data, source_arr, NULL, out, n, 0.0)) goto done;

    res = check_repeat_count(csound, h, n);
    if (res != OK) goto done;

    const CSN_ARRAY *ops[1] = { source_arr };
    CSNARR_RES r = prepare_output(csound, h, h, reg, out, source_arr->ndim, source_arr->shape, CSN_STRING, ops, 1, CSN_ALIAS_ELEMENTWISE);
    res = r.err;
    if (res != OK) goto done;
    pres.locked |= r.locked;

    res = strmul_hs_assign_value(csound, h, source_arr, n, out);
    if (res != OK) goto done;
    if (r.aliased) update_array_data_version(&out->version);

    SET_KDATA_END(*k_data, source_arr->shape, source_arr->ndim, CSN_STRING);
    PUBLISH_ELEMENTWISE(k_data, source_arr, NULL, out, n, 0.0);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnstrings_mul_hs(CSOUND *csound, CSN_BINOP_HS *p) {
    return strmul_hs_init(csound, &p->h, p->handle, p->source_handle, p->scalar, &p->k_data, false);
}

int32_t csnstrings_mul_hs_k_init(CSOUND *csound, CSN_BINOP_HS *p) {
    return strmul_hs_init(csound, &p->h, p->handle, p->source_handle, p->scalar, &p->k_data, true);
}

int32_t csnstrings_mul_hs_k(CSOUND *csound, CSN_BINOP_HS *p) {
    CHECK_KTRIG(p->trig);
    return strmul_hs_perf(csound, &p->h, p->handle, p->source_handle, p->scalar, &p->k_data);
}

/* ---------- string maps: case conversions and strip ---------- */

/* A map rewrites one string in its own buffer and never makes it longer, so
   the copy it starts from is the only write that can need room, and an
   in-place pass needs no probe first. An idempotent map gives the same
   strings when applied again, so a k pass on an unchanged source, even one
   that is its own output, can be skipped. chars is the map's S argument, or
   NULL for a map that takes none. */
typedef void (*CSN_STR_MAP) (char *s, int32_t mode, const char *chars);

typedef struct {
    CSN_STR_MAP fn;
    int32_t mode;
    bool idempotent;
    const STRINGDAT *chars;   /* the S argument, NULL when the opcode has none */
} CSN_STR_OP;

/* What a map reads from its opcode struct: out is NULL in place, trig is read
   by the k passes only. */
typedef struct {
    OPDS *h;
    CSN_ARRAY *out;
    CSN_ARRAY *src;
    cs_float *trig;
    K_DATA *k_data;
} CSN_STR_MAP_ARGS;

typedef enum {
    CSN_CASE_UPPER,
    CSN_CASE_LOWER,
    CSN_CASE_CAPITALIZE,
    CSN_CASE_SWAP,
    CSN_CASE_TITLE
} CSN_CASE;

typedef enum {
    CSN_STRIP,
    CSN_LSTRIP,
    CSN_RSTRIP
} CSN_STRIP_MODE;

static bool ascii_is_letter(unsigned char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
static char ascii_upper(unsigned char c) { return (char) (c >= 'a' && c <= 'z' ? c ^ 0x20 : c); }
static char ascii_lower(unsigned char c) { return (char) (c >= 'A' && c <= 'Z' ? c ^ 0x20 : c); }
static char ascii_swap(unsigned char c) { return (char) (ascii_is_letter(c) ? c ^ 0x20 : c); }

/* Python's bytes whitespace: space, \t, \n, \v, \f, \r. */
static bool ascii_is_space(unsigned char c) { return c == ' ' || (c >= '\t' && c <= '\r'); }

/* ASCII only, whatever the locale: bytes from 0x80 up are left alone, so a
   UTF-8 string keeps its multibyte sequences. Capitalize is
   np.strings.capitalize on bytes: the first byte up, all the others down, so
   a string opening with a digit, a space or a non-ASCII byte comes out all
   lower case. Swap turns each letter into the other case. Title is
   np.strings.title on bytes: a letter goes up after anything that is not a
   letter and down after a letter. Digits and bytes from 0x80 up are not
   letters, so "1st" gives "1St" and "Cafés" gives "CaféS". */
static void map_case(char *s, int32_t mode, const char *chars) {
    (void) chars;
    switch ((CSN_CASE) mode) {
        case CSN_CASE_CAPITALIZE:
            if (*s != '\0') {
                *s = ascii_upper((unsigned char) *s);
                s++;
            }
            for (; *s != '\0'; s++) *s = ascii_lower((unsigned char) *s);
            break;
        case CSN_CASE_UPPER:
            for (; *s != '\0'; s++) *s = ascii_upper((unsigned char) *s);
            break;
        case CSN_CASE_LOWER:
            for (; *s != '\0'; s++) *s = ascii_lower((unsigned char) *s);
            break;
        case CSN_CASE_SWAP:
            for (; *s != '\0'; s++) *s = ascii_swap((unsigned char) *s);
            break;
        case CSN_CASE_TITLE: {
            bool after_letter = false;
            for (; *s != '\0'; s++) {
                unsigned char c = (unsigned char) *s;
                bool is_letter = ascii_is_letter(c);
                if (is_letter) *s = after_letter ? ascii_lower(c) : ascii_upper(c);
                after_letter = is_letter;
            }
            break;
        }
    }
}

/* A byte strip drops. c comes from inside a string, so it is never the NUL
   that strchr would find at the end of chars. */
static bool is_strip_byte(unsigned char c, const char *chars) {
    return chars == NULL ? ascii_is_space(c) : strchr(chars, c) != NULL;
}

/* np.strings.strip on bytes. Without chars it drops ASCII whitespace only, so
   a UTF-8 byte such as the 0xA0 that ends "à" is never taken for a no-break
   space. chars is a set of bytes, as in Python: any of them goes, in any
   order, so "xyhixy" with "xy" gives "hi", "" drops nothing, and a multibyte
   character drops each of its bytes ("é" takes 0xC3 0xA9 off "caffé"). */
static void strip_mode(char *s, int32_t mode, const char *chars) {
    char *start = s;
    char *end = s + strlen(s);
    if (mode != CSN_RSTRIP) {
        while (start < end && is_strip_byte((unsigned char) *start, chars)) start++;
    }
    if (mode != CSN_LSTRIP) {
        while (end > start && is_strip_byte((unsigned char) end[-1], chars)) end--;
    }
    size_t len = (size_t) (end - start);
    if (start != s) memmove(s, start, len);
    s[len] = '\0';
}

/* Swap undoes itself: X = csnstrswapcase(X, k) swaps again on every pass, as
   X = csnmul(X, 0.99) scales again. */
static CSN_STR_OP case_op(CSN_CASE mode) {
    return (CSN_STR_OP) { map_case, (int32_t) mode, mode != CSN_CASE_SWAP, NULL };
}

/* chars is NULL for the forms without the S argument. */
static CSN_STR_OP strip_op(CSN_STRIP_MODE mode, const STRINGDAT *chars) {
    return (CSN_STR_OP) { strip_mode, (int32_t) mode, true, chars };
}

static CSN_STR_MAP_ARGS unary_args(CSN_UNARYOP *p) {
    return (CSN_STR_MAP_ARGS) { &p->h, p->handle, p->source_handle, p->trig, &p->k_data };
}

static CSN_STR_MAP_ARGS unary_in_args(CSN_UNARYOP_IN *p) {
    return (CSN_STR_MAP_ARGS) { &p->h, NULL, p->source_handle, p->trig, &p->k_data };
}

static CSN_STR_MAP_ARGS unary_chars_args(CSN_UNARYOP_STRING *p) {
    return (CSN_STR_MAP_ARGS) { &p->h, p->handle, p->source_handle, p->trig, &p->k_data };
}

static CSN_STR_MAP_ARGS unary_chars_in_args(CSN_UNARYOP_STRING_IN *p) {
    return (CSN_STR_MAP_ARGS) { &p->h, NULL, p->source_handle, p->trig, &p->k_data };
}

static int32_t check_op_chars(CSOUND *csound, OPDS *perf_h, CSN_STR_OP op) {
    if (op.chars == NULL || IS_VALID_STRINGDAT(op.chars)) return OK;
    return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] Invalid/NULL string");
}

static const char *op_chars(CSN_STR_OP op) {
    return op.chars != NULL ? op.chars->data : NULL;
}

/* An S argument carries no version, so a k pass compares it against the copy
   k_data keeps (PUBLISH_STRING_ELEMENTWISE). A map without one keeps none. A
   copy a real-time pass could not keep leaves k_data stale: every pass
   computes until one fits. */
static bool is_same_chars(const K_DATA *k_data, const char *chars) {
    if (k_data->prev_strings_stale) return false;
    if (chars == NULL) return k_data->prev_string.data == NULL;
    return k_data->prev_string.data != NULL && strcmp(k_data->prev_string.data, chars) == 0;
}

/* out may be src: csn_str_write leaves a slot that is its own value alone. */
static int32_t string_map_assign_value(CSN_STR_WRITER *w, CSN_ARRAY *out, const CSN_ARRAY *src, CSN_STR_OP op) {
    const char *chars = op_chars(op);
    for (size_t i = 0; i < src->size; i++) {
        if (csn_str_write(w, &out->sdata[i], src->sdata[i].data, GET_STRINGDAT_LEN(src->sdata[i].data)) != OK) return w->err;
        op.fn(out->sdata[i].data, op.mode, chars);
    }

    return OK;
}

/* With perf_redoes (a k init), an output that is its own source under a map
   that is not idempotent is left to the perf pass, or the first cycle would
   apply it twice. A k init also reserves the copy of chars the perf pass
   compares against, so a pass on a real-time path keeps it without
   allocating; it releases the old one first, before a reinit takes the struct
   again. */
static int32_t string_map_helper(CSOUND *csound, CSN_STR_MAP_ARGS a, CSN_STR_OP op, bool perf_redoes) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    if (perf_redoes) csn_kdata_release_strings(csound, a.k_data);

    int32_t res = check_op_chars(csound, NULL, op);
    if (res != OK) return res;

    stamp_globals(csound, a.h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { a.out, a.src }, 2);

    const CSN_ARRAY *ops[1] = { a.src };
    CSNARR_RES r = prepare_output(csound, a.h, NULL, reg, a.out, a.src->ndim, a.src->shape, CSN_STRING, ops, 1, CSN_ALIAS_ELEMENTWISE);
    res = r.err;
    if (res != OK) goto done;
    pres.locked |= r.locked;

    a.k_data->registry = reg;
    if (perf_redoes) csn_kdata_keep_strings(csound, a.k_data, op_chars(op), NULL, true);
    if (perf_redoes && r.aliased && !op.idempotent) goto done;

    CSN_STR_WRITER w = csn_str_writer(csound, NULL, a.out);
    res = string_map_assign_value(&w, a.out, a.src, op);
    if (r.aliased) update_array_data_version(&a.src->version);
    if (res != OK) goto done;

    PUBLISH_DERIVED_RESULT(a.k_data, a.src, a.out);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

static int32_t string_map_k_helper(CSOUND *csound, CSN_STR_MAP_ARGS a, CSN_STR_OP op) {
    CSN_REGISTRY *reg = a.k_data->registry;
    CHECK_REGISTRY(csound, a.h, reg);

    CHECK_KTRIG(a.trig);

    int32_t res = check_op_chars(csound, a.h, op);
    if (res != OK) return res;
    const char *chars = op_chars(op);

    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { a.out, a.src }, 2);

    bool is_same_source = is_same_array_version(&a.k_data->prev_source_version, &a.src->version);
    bool is_same_result = is_same_array_version(&a.k_data->prev_output_version, &a.out->version);
    bool is_aliased = a.out == a.src;
    if (is_same_source && is_same_result && is_same_chars(a.k_data, chars) && (op.idempotent || !is_aliased)) goto done;

    const CSN_ARRAY *ops[1] = { a.src };
    CSNARR_RES r = prepare_output(csound, a.h, a.h, reg, a.out, a.src->ndim, a.src->shape, CSN_STRING, ops, 1, CSN_ALIAS_ELEMENTWISE);
    res = r.err;
    if (res != OK) goto done;
    pres.locked |= r.locked;

    CSN_STR_WRITER w = csn_str_writer(csound, a.h, a.out);
    res = string_map_assign_value(&w, a.out, a.src, op);
    if (r.aliased) update_array_data_version(&a.src->version);
    if (res != OK) goto done;

    PUBLISH_STRING_ELEMENTWISE(csound, a.k_data, a.src, NULL, a.out, chars, NULL, !a.out->rt_locked);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

static int32_t string_map_in_helper(CSOUND *csound, CSN_STR_MAP_ARGS a, CSN_STR_OP op, bool is_k_init) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    if (is_k_init) csn_kdata_release_strings(csound, a.k_data);

    int32_t res = check_op_chars(csound, NULL, op);
    if (res != OK) return res;

    /* the target changes at perf: whatever reads it follows it there */
    if (is_k_init) {
        csn_mark_krate(a.src);
        csn_kdata_keep_strings(csound, a.k_data, op_chars(op), NULL, true);
    }

    stamp_globals(csound, a.h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { a.src }, 1);

    CSN_STR_WRITER w = csn_str_writer(csound, NULL, a.src);
    res = string_map_assign_value(&w, a.src, a.src, op);
    if (res != OK) goto done;
    update_array_data_version(&a.src->version);

    set_array_version(&a.k_data->prev_source_version, &a.src->version);
    a.k_data->registry = reg;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

/* Runs once for each write by someone else, or each new chars, so a map that
   is not idempotent is applied once per change, as csnreverse.in does. */
static int32_t string_map_in_k_helper(CSOUND *csound, CSN_STR_MAP_ARGS a, CSN_STR_OP op) {
    CSN_REGISTRY *reg = a.k_data->registry;
    CHECK_REGISTRY(csound, a.h, reg);

    CHECK_KTRIG(a.trig);

    int32_t res = check_op_chars(csound, a.h, op);
    if (res != OK) return res;
    const char *chars = op_chars(op);

    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { a.src }, 1);

    bool is_same_source = is_same_array_version(&a.k_data->prev_source_version, &a.src->version);
    if (is_same_source && is_same_chars(a.k_data, chars)) goto done;

    CSN_STR_WRITER w = csn_str_writer(csound, a.h, a.src);
    res = string_map_assign_value(&w, a.src, a.src, op);
    if (res != OK) goto done;
    update_array_data_version(&a.src->version);

    PUBLISH_STRING_ELEMENTWISE(csound, a.k_data, a.src, NULL, NULL, chars, NULL, !a.src->rt_locked);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnstrings_lower(CSOUND *csound, CSN_UNARYOP *p) {
    return string_map_helper(csound, unary_args(p), case_op(CSN_CASE_LOWER), false);
}

int32_t csnstrings_lower_k_init(CSOUND *csound, CSN_UNARYOP *p) {
    return string_map_helper(csound, unary_args(p), case_op(CSN_CASE_LOWER), true);
}

int32_t csnstrings_lower_k(CSOUND *csound, CSN_UNARYOP *p) {
    return string_map_k_helper(csound, unary_args(p), case_op(CSN_CASE_LOWER));
}

int32_t csnstrings_lower_in(CSOUND *csound, CSN_UNARYOP_IN *p) {
    return string_map_in_helper(csound, unary_in_args(p), case_op(CSN_CASE_LOWER), false);
}

int32_t csnstrings_lower_in_k_init(CSOUND *csound, CSN_UNARYOP_IN *p) {
    return string_map_in_helper(csound, unary_in_args(p), case_op(CSN_CASE_LOWER), true);
}

int32_t csnstrings_lower_in_k(CSOUND *csound, CSN_UNARYOP_IN *p) {
    return string_map_in_k_helper(csound, unary_in_args(p), case_op(CSN_CASE_LOWER));
}

int32_t csnstrings_upper(CSOUND *csound, CSN_UNARYOP *p) {
    return string_map_helper(csound, unary_args(p), case_op(CSN_CASE_UPPER), false);
}

int32_t csnstrings_upper_k_init(CSOUND *csound, CSN_UNARYOP *p) {
    return string_map_helper(csound, unary_args(p), case_op(CSN_CASE_UPPER), true);
}

int32_t csnstrings_upper_k(CSOUND *csound, CSN_UNARYOP *p) {
    return string_map_k_helper(csound, unary_args(p), case_op(CSN_CASE_UPPER));
}

int32_t csnstrings_upper_in(CSOUND *csound, CSN_UNARYOP_IN *p) {
    return string_map_in_helper(csound, unary_in_args(p), case_op(CSN_CASE_UPPER), false);
}

int32_t csnstrings_upper_in_k_init(CSOUND *csound, CSN_UNARYOP_IN *p) {
    return string_map_in_helper(csound, unary_in_args(p), case_op(CSN_CASE_UPPER), true);
}

int32_t csnstrings_upper_in_k(CSOUND *csound, CSN_UNARYOP_IN *p) {
    return string_map_in_k_helper(csound, unary_in_args(p), case_op(CSN_CASE_UPPER));
}

int32_t csnstrings_capitalize(CSOUND *csound, CSN_UNARYOP *p) {
    return string_map_helper(csound, unary_args(p), case_op(CSN_CASE_CAPITALIZE), false);
}

int32_t csnstrings_capitalize_k_init(CSOUND *csound, CSN_UNARYOP *p) {
    return string_map_helper(csound, unary_args(p), case_op(CSN_CASE_CAPITALIZE), true);
}

int32_t csnstrings_capitalize_k(CSOUND *csound, CSN_UNARYOP *p) {
    return string_map_k_helper(csound, unary_args(p), case_op(CSN_CASE_CAPITALIZE));
}

int32_t csnstrings_capitalize_in(CSOUND *csound, CSN_UNARYOP_IN *p) {
    return string_map_in_helper(csound, unary_in_args(p), case_op(CSN_CASE_CAPITALIZE), false);
}

int32_t csnstrings_capitalize_in_k_init(CSOUND *csound, CSN_UNARYOP_IN *p) {
    return string_map_in_helper(csound, unary_in_args(p), case_op(CSN_CASE_CAPITALIZE), true);
}

int32_t csnstrings_capitalize_in_k(CSOUND *csound, CSN_UNARYOP_IN *p) {
    return string_map_in_k_helper(csound, unary_in_args(p), case_op(CSN_CASE_CAPITALIZE));
}

int32_t csnstrings_swapcase(CSOUND *csound, CSN_UNARYOP *p) {
    return string_map_helper(csound, unary_args(p), case_op(CSN_CASE_SWAP), false);
}

int32_t csnstrings_swapcase_k_init(CSOUND *csound, CSN_UNARYOP *p) {
    return string_map_helper(csound, unary_args(p), case_op(CSN_CASE_SWAP), true);
}

int32_t csnstrings_swapcase_k(CSOUND *csound, CSN_UNARYOP *p) {
    return string_map_k_helper(csound, unary_args(p), case_op(CSN_CASE_SWAP));
}

int32_t csnstrings_swapcase_in(CSOUND *csound, CSN_UNARYOP_IN *p) {
    return string_map_in_helper(csound, unary_in_args(p), case_op(CSN_CASE_SWAP), false);
}

int32_t csnstrings_swapcase_in_k_init(CSOUND *csound, CSN_UNARYOP_IN *p) {
    return string_map_in_helper(csound, unary_in_args(p), case_op(CSN_CASE_SWAP), true);
}

int32_t csnstrings_swapcase_in_k(CSOUND *csound, CSN_UNARYOP_IN *p) {
    return string_map_in_k_helper(csound, unary_in_args(p), case_op(CSN_CASE_SWAP));
}

int32_t csnstrings_title(CSOUND *csound, CSN_UNARYOP *p) {
    return string_map_helper(csound, unary_args(p), case_op(CSN_CASE_TITLE), false);
}

int32_t csnstrings_title_k_init(CSOUND *csound, CSN_UNARYOP *p) {
    return string_map_helper(csound, unary_args(p), case_op(CSN_CASE_TITLE), true);
}

int32_t csnstrings_title_k(CSOUND *csound, CSN_UNARYOP *p) {
    return string_map_k_helper(csound, unary_args(p), case_op(CSN_CASE_TITLE));
}

int32_t csnstrings_title_in(CSOUND *csound, CSN_UNARYOP_IN *p) {
    return string_map_in_helper(csound, unary_in_args(p), case_op(CSN_CASE_TITLE), false);
}

int32_t csnstrings_title_in_k_init(CSOUND *csound, CSN_UNARYOP_IN *p) {
    return string_map_in_helper(csound, unary_in_args(p), case_op(CSN_CASE_TITLE), true);
}

int32_t csnstrings_title_in_k(CSOUND *csound, CSN_UNARYOP_IN *p) {
    return string_map_in_k_helper(csound, unary_in_args(p), case_op(CSN_CASE_TITLE));
}

int32_t csnstrings_strip(CSOUND *csound, CSN_UNARYOP *p) {
    return string_map_helper(csound, unary_args(p), strip_op(CSN_STRIP, NULL), false);
}

int32_t csnstrings_strip_k_init(CSOUND *csound, CSN_UNARYOP *p) {
    return string_map_helper(csound, unary_args(p), strip_op(CSN_STRIP, NULL), true);
}

int32_t csnstrings_strip_k(CSOUND *csound, CSN_UNARYOP *p) {
    return string_map_k_helper(csound, unary_args(p), strip_op(CSN_STRIP, NULL));
}

int32_t csnstrings_strip_in(CSOUND *csound, CSN_UNARYOP_IN *p) {
    return string_map_in_helper(csound, unary_in_args(p), strip_op(CSN_STRIP, NULL), false);
}

int32_t csnstrings_strip_in_k_init(CSOUND *csound, CSN_UNARYOP_IN *p) {
    return string_map_in_helper(csound, unary_in_args(p), strip_op(CSN_STRIP, NULL), true);
}

int32_t csnstrings_strip_in_k(CSOUND *csound, CSN_UNARYOP_IN *p) {
    return string_map_in_k_helper(csound, unary_in_args(p), strip_op(CSN_STRIP, NULL));
}

int32_t csnstrings_lstrip(CSOUND *csound, CSN_UNARYOP *p) {
    return string_map_helper(csound, unary_args(p), strip_op(CSN_LSTRIP, NULL), false);
}

int32_t csnstrings_lstrip_k_init(CSOUND *csound, CSN_UNARYOP *p) {
    return string_map_helper(csound, unary_args(p), strip_op(CSN_LSTRIP, NULL), true);
}

int32_t csnstrings_lstrip_k(CSOUND *csound, CSN_UNARYOP *p) {
    return string_map_k_helper(csound, unary_args(p), strip_op(CSN_LSTRIP, NULL));
}

int32_t csnstrings_lstrip_in(CSOUND *csound, CSN_UNARYOP_IN *p) {
    return string_map_in_helper(csound, unary_in_args(p), strip_op(CSN_LSTRIP, NULL), false);
}

int32_t csnstrings_lstrip_in_k_init(CSOUND *csound, CSN_UNARYOP_IN *p) {
    return string_map_in_helper(csound, unary_in_args(p), strip_op(CSN_LSTRIP, NULL), true);
}

int32_t csnstrings_lstrip_in_k(CSOUND *csound, CSN_UNARYOP_IN *p) {
    return string_map_in_k_helper(csound, unary_in_args(p), strip_op(CSN_LSTRIP, NULL));
}

int32_t csnstrings_rstrip(CSOUND *csound, CSN_UNARYOP *p) {
    return string_map_helper(csound, unary_args(p), strip_op(CSN_RSTRIP, NULL), false);
}

int32_t csnstrings_rstrip_k_init(CSOUND *csound, CSN_UNARYOP *p) {
    return string_map_helper(csound, unary_args(p), strip_op(CSN_RSTRIP, NULL), true);
}

int32_t csnstrings_rstrip_k(CSOUND *csound, CSN_UNARYOP *p) {
    return string_map_k_helper(csound, unary_args(p), strip_op(CSN_RSTRIP, NULL));
}

int32_t csnstrings_rstrip_in(CSOUND *csound, CSN_UNARYOP_IN *p) {
    return string_map_in_helper(csound, unary_in_args(p), strip_op(CSN_RSTRIP, NULL), false);
}

int32_t csnstrings_rstrip_in_k_init(CSOUND *csound, CSN_UNARYOP_IN *p) {
    return string_map_in_helper(csound, unary_in_args(p), strip_op(CSN_RSTRIP, NULL), true);
}

int32_t csnstrings_rstrip_in_k(CSOUND *csound, CSN_UNARYOP_IN *p) {
    return string_map_in_k_helper(csound, unary_in_args(p), strip_op(CSN_RSTRIP, NULL));
}

/* ---------- strip with a chars argument ---------- */

int32_t csnstrings_stripchar(CSOUND *csound, CSN_UNARYOP_STRING *p) {
    return string_map_helper(csound, unary_chars_args(p), strip_op(CSN_STRIP, p->value), false);
}

int32_t csnstrings_stripchar_k_init(CSOUND *csound, CSN_UNARYOP_STRING *p) {
    return string_map_helper(csound, unary_chars_args(p), strip_op(CSN_STRIP, p->value), true);
}

int32_t csnstrings_stripchar_k(CSOUND *csound, CSN_UNARYOP_STRING *p) {
    return string_map_k_helper(csound, unary_chars_args(p), strip_op(CSN_STRIP, p->value));
}

int32_t csnstrings_stripchar_in(CSOUND *csound, CSN_UNARYOP_STRING_IN *p) {
    return string_map_in_helper(csound, unary_chars_in_args(p), strip_op(CSN_STRIP, p->value), false);
}

int32_t csnstrings_stripchar_in_k_init(CSOUND *csound, CSN_UNARYOP_STRING_IN *p) {
    return string_map_in_helper(csound, unary_chars_in_args(p), strip_op(CSN_STRIP, p->value), true);
}

int32_t csnstrings_stripchar_in_k(CSOUND *csound, CSN_UNARYOP_STRING_IN *p) {
    return string_map_in_k_helper(csound, unary_chars_in_args(p), strip_op(CSN_STRIP, p->value));
}

int32_t csnstrings_lstripchar(CSOUND *csound, CSN_UNARYOP_STRING *p) {
    return string_map_helper(csound, unary_chars_args(p), strip_op(CSN_LSTRIP, p->value), false);
}

int32_t csnstrings_lstripchar_k_init(CSOUND *csound, CSN_UNARYOP_STRING *p) {
    return string_map_helper(csound, unary_chars_args(p), strip_op(CSN_LSTRIP, p->value), true);
}

int32_t csnstrings_lstripchar_k(CSOUND *csound, CSN_UNARYOP_STRING *p) {
    return string_map_k_helper(csound, unary_chars_args(p), strip_op(CSN_LSTRIP, p->value));
}

int32_t csnstrings_lstripchar_in(CSOUND *csound, CSN_UNARYOP_STRING_IN *p) {
    return string_map_in_helper(csound, unary_chars_in_args(p), strip_op(CSN_LSTRIP, p->value), false);
}

int32_t csnstrings_lstripchar_in_k_init(CSOUND *csound, CSN_UNARYOP_STRING_IN *p) {
    return string_map_in_helper(csound, unary_chars_in_args(p), strip_op(CSN_LSTRIP, p->value), true);
}

int32_t csnstrings_lstripchar_in_k(CSOUND *csound, CSN_UNARYOP_STRING_IN *p) {
    return string_map_in_k_helper(csound, unary_chars_in_args(p), strip_op(CSN_LSTRIP, p->value));
}

int32_t csnstrings_rstripchar(CSOUND *csound, CSN_UNARYOP_STRING *p) {
    return string_map_helper(csound, unary_chars_args(p), strip_op(CSN_RSTRIP, p->value), false);
}

int32_t csnstrings_rstripchar_k_init(CSOUND *csound, CSN_UNARYOP_STRING *p) {
    return string_map_helper(csound, unary_chars_args(p), strip_op(CSN_RSTRIP, p->value), true);
}

int32_t csnstrings_rstripchar_k(CSOUND *csound, CSN_UNARYOP_STRING *p) {
    return string_map_k_helper(csound, unary_chars_args(p), strip_op(CSN_RSTRIP, p->value));
}

int32_t csnstrings_rstripchar_in(CSOUND *csound, CSN_UNARYOP_STRING_IN *p) {
    return string_map_in_helper(csound, unary_chars_in_args(p), strip_op(CSN_RSTRIP, p->value), false);
}

int32_t csnstrings_rstripchar_in_k_init(CSOUND *csound, CSN_UNARYOP_STRING_IN *p) {
    return string_map_in_helper(csound, unary_chars_in_args(p), strip_op(CSN_RSTRIP, p->value), true);
}

int32_t csnstrings_rstripchar_in_k(CSOUND *csound, CSN_UNARYOP_STRING_IN *p) {
    return string_map_in_k_helper(csound, unary_chars_in_args(p), strip_op(CSN_RSTRIP, p->value));
}

/* The k forms with chars keep a copy of it in k_data. */
int32_t csnstrings_mapchars_deinit(CSOUND *csound, CSN_UNARYOP_STRING *p) {
    csn_kdata_release_strings(csound, &p->k_data);
    return OK;
}

int32_t csnstrings_mapchars_in_deinit(CSOUND *csound, CSN_UNARYOP_STRING_IN *p) {
    csn_kdata_release_strings(csound, &p->k_data);
    return OK;
}

/* ---------- padding: center, ljust, rjust ---------- */

typedef enum { CSN_STRING_CENTER, CSN_STRING_LJUST, CSN_STRING_RJUST } CSN_JUST_MODE;

/* What a padding reads from its opcode struct: out is NULL in place, fill is
   NULL for the forms without one, trig is read by the k passes only. */
typedef struct {
    OPDS *h;
    CSN_ARRAY *out;
    CSN_ARRAY *src;
    cs_float *width;
    const STRINGDAT *fill;
    cs_float *trig;
    K_DATA *k_data;
} CSN_JUST_ARGS;

static CSN_JUST_ARGS just_args(CSN_BINOP_HS *p) {
    return (CSN_JUST_ARGS) { &p->h, p->handle, p->source_handle, p->scalar, NULL, p->trig, &p->k_data };
}

static CSN_JUST_ARGS just_in_args(CSN_UNARYOP_SCALAR_IN *p) {
    return (CSN_JUST_ARGS) { &p->h, NULL, p->source_handle, p->value, NULL, p->trig, &p->k_data };
}

static CSN_JUST_ARGS just_fill_args(CSN_STRJUST *p) {
    return (CSN_JUST_ARGS) { &p->h, p->handle, p->source_handle, p->width, p->fill, p->trig, &p->k_data };
}

static CSN_JUST_ARGS just_fill_in_args(CSN_STRJUST_IN *p) {
    return (CSN_JUST_ARGS) { &p->h, NULL, p->source_handle, p->width, p->fill, p->trig, &p->k_data };
}

/* np.strings.center / ljust / rjust on bytes, which follow CPython: center
   puts the odd fill on the left when the width is odd, (marg & width & 1),
   so "ab" in 5 gives "  ab " and "a" in 4 gives " a  ". */
static size_t just_left(size_t len, size_t width, CSN_JUST_MODE mode) {
    size_t marg = width - len;
    switch (mode) {
        case CSN_STRING_CENTER: return marg / 2 + (marg & width & 1);
        case CSN_STRING_RJUST: return marg;
        case CSN_STRING_LJUST: break;
    }
    return 0;
}

static int32_t check_width(CSOUND *csound, OPDS *perf_h, double width) {
    if (IS_VALID_LENGTH(width)) return OK;
    return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] A width must be a whole number from 0 up, got %g", width);
}

/* The fill is one byte, as NumPy wants one character in a bytes array: an
   ASCII character, never a multibyte UTF-8 one. Without a fill, a space. */
static int32_t read_fill(CSOUND *csound, OPDS *perf_h, const STRINGDAT *fill, char *c) {
    if (fill == NULL) {
        *c = ' ';
        return OK;
    }
    if (!IS_VALID_STRINGDAT(fill) || strlen(fill->data) != 1) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] A fill character must be exactly one byte, got \"%s\"", IS_VALID_STRINGDAT(fill) ? fill->data : "");
    }
    *c = fill->data[0];
    return OK;
}

static const char *slot_string(const CSN_STRDATA *slot) {
    return slot->data != NULL ? slot->data : "";
}

/* Pads every string shorter than width bytes to width; a longer one is kept
   as it is, as NumPy keeps it. out may be src: in place the slots are probed
   first, so a real-time pass that stops leaves the array as it was, and the
   text moves right inside its own buffer, after any growth. changed says
   whether a string was padded. */
static int32_t string_just_assign_value(CSN_STR_WRITER *w, CSN_ARRAY *out, const CSN_ARRAY *src, size_t width, char fill, CSN_JUST_MODE mode, bool *changed) {
    bool in_place = out == src;
    *changed = false;

    if (in_place && w->no_growth) {
        for (size_t i = 0; i < src->size; i++) {
            const CSN_STRDATA *slot = &src->sdata[i];
            if (strlen(slot_string(slot)) < width && width + 1 > slot->capacity) {
                return w->err = refuse_string_growth(w->csound, w->perf_h, width + 1, slot->capacity);
            }
        }
    }

    for (size_t i = 0; i < src->size; i++) {
        size_t len = strlen(slot_string(&src->sdata[i]));
        if (len >= width) {
            if (!in_place && csn_str_write(w, &out->sdata[i], src->sdata[i].data, GET_STRINGDAT_LEN(src->sdata[i].data)) != OK) return w->err;
            continue;
        }

        int32_t res = ensure_string_capacity(w->csound, w->perf_h, &out->sdata[i], width + 1, w->no_growth);
        if (res != OK) return w->err = res;
        char *dest = out->sdata[i].data;
        size_t left = just_left(len, width, mode);
        if (in_place) {
            memmove(dest + left, dest, len);
        } else {
            memcpy(dest + left, slot_string(&src->sdata[i]), len);
        }
        memset(dest, fill, left);
        memset(dest + left + len, fill, width - len - left);
        dest[width] = '\0';
        *changed = true;
    }

    return OK;
}

/* The fill is one byte, so k_data keeps it as a number, with no copy. */
static void publish_just(K_DATA *k_data, const CSN_ARRAY *src, const CSN_ARRAY *out, size_t width, char fill) {
    PUBLISH_DERIVED_RESULT(k_data, src, out);
    k_data->prev_size = width;
    k_data->prev_scalar_param = (double) (unsigned char) fill;
}

static bool is_same_just(const K_DATA *k_data, size_t width, char fill) {
    return k_data->prev_size == width && k_data->prev_scalar_param == (double) (unsigned char) fill;
}

/* A k width is not known at init, but its init value still lays the strings
   out, as csnstrmul does with a count: the buffers it reserves are what a
   pass on a real-time path may fill, and the perf pass compares the width it
   reads. An init value that is no width reserves nothing and raises no error;
   the perf pass judges the width. Padding again to the same width changes
   nothing, so an output that is its own source is padded here too, unless
   its width is a k one: a padding cannot be undone, and the init value would
   stay in the strings. */
static int32_t string_just_helper(CSOUND *csound, CSN_JUST_ARGS a, CSN_JUST_MODE mode, bool perf_redoes) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    char fill = ' ';
    int32_t res = read_fill(csound, NULL, a.fill, &fill);
    if (res != OK) return res;
    double width_value = (double) *a.width;
    bool known = !perf_redoes || csn_known_at_init(csound, a.width);
    if (known) {
        res = check_width(csound, NULL, width_value);
        if (res != OK) return res;
    }

    stamp_globals(csound, a.h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { a.out, a.src }, 2);

    const CSN_ARRAY *ops[1] = { a.src };
    CSNARR_RES r = prepare_output(csound, a.h, NULL, reg, a.out, a.src->ndim, a.src->shape, CSN_STRING, ops, 1, CSN_ALIAS_ELEMENTWISE);
    res = r.err;
    if (res != OK) goto done;
    pres.locked |= r.locked;

    a.k_data->registry = reg;
    if (!IS_VALID_LENGTH(width_value) || (r.aliased && !known)) goto done;
    size_t width = (size_t) width_value;

    bool changed = false;
    CSN_STR_WRITER w = csn_str_writer(csound, NULL, a.out);
    res = string_just_assign_value(&w, a.out, a.src, width, fill, mode, &changed);
    if (r.aliased && changed) update_array_data_version(&a.src->version);
    if (res != OK) goto done;

    publish_just(a.k_data, a.src, a.out, width, fill);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

static int32_t string_just_k_helper(CSOUND *csound, CSN_JUST_ARGS a, CSN_JUST_MODE mode) {
    CSN_REGISTRY *reg = a.k_data->registry;
    CHECK_REGISTRY(csound, a.h, reg);

    CHECK_KTRIG(a.trig);

    char fill = ' ';
    int32_t res = read_fill(csound, a.h, a.fill, &fill);
    if (res != OK) return res;
    res = check_width(csound, a.h, (double) *a.width);
    if (res != OK) return res;
    size_t width = (size_t) *a.width;

    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { a.out, a.src }, 2);

    bool is_same_source = is_same_array_version(&a.k_data->prev_source_version, &a.src->version);
    bool is_same_result = is_same_array_version(&a.k_data->prev_output_version, &a.out->version);
    if (is_same_source && is_same_result && is_same_just(a.k_data, width, fill)) goto done;

    const CSN_ARRAY *ops[1] = { a.src };
    CSNARR_RES r = prepare_output(csound, a.h, a.h, reg, a.out, a.src->ndim, a.src->shape, CSN_STRING, ops, 1, CSN_ALIAS_ELEMENTWISE);
    res = r.err;
    if (res != OK) goto done;
    pres.locked |= r.locked;

    bool changed = false;
    CSN_STR_WRITER w = csn_str_writer(csound, a.h, a.out);
    res = string_just_assign_value(&w, a.out, a.src, width, fill, mode, &changed);
    if (r.aliased && changed) update_array_data_version(&a.src->version);
    if (res != OK) goto done;

    publish_just(a.k_data, a.src, a.out, width, fill);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

/* In place a padding cannot be undone, so a k width unknown at init is left
   to the first pass rather than applied with its init value. */
static int32_t string_just_in_helper(CSOUND *csound, CSN_JUST_ARGS a, CSN_JUST_MODE mode, bool is_k_init) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    a.k_data->registry = reg;

    /* the target changes at perf: whatever reads it follows it there */
    if (is_k_init) csn_mark_krate(a.src);

    char fill = ' ';
    int32_t res = read_fill(csound, NULL, a.fill, &fill);
    if (res != OK) return res;
    if (is_k_init && !csn_known_at_init(csound, a.width)) return OK;

    res = check_width(csound, NULL, (double) *a.width);
    if (res != OK) return res;
    size_t width = (size_t) *a.width;

    stamp_globals(csound, a.h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { a.src }, 1);

    bool changed = false;
    CSN_STR_WRITER w = csn_str_writer(csound, NULL, a.src);
    res = string_just_assign_value(&w, a.src, a.src, width, fill, mode, &changed);
    if (changed) update_array_data_version(&a.src->version);
    if (res != OK) goto done;

    publish_just(a.k_data, a.src, NULL, width, fill);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

/* Runs once for each write by someone else, each new width or each new fill. */
static int32_t string_just_in_k_helper(CSOUND *csound, CSN_JUST_ARGS a, CSN_JUST_MODE mode) {
    CSN_REGISTRY *reg = a.k_data->registry;
    CHECK_REGISTRY(csound, a.h, reg);

    CHECK_KTRIG(a.trig);

    char fill = ' ';
    int32_t res = read_fill(csound, a.h, a.fill, &fill);
    if (res != OK) return res;
    res = check_width(csound, a.h, (double) *a.width);
    if (res != OK) return res;
    size_t width = (size_t) *a.width;

    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { a.src }, 1);

    bool is_same_source = is_same_array_version(&a.k_data->prev_source_version, &a.src->version);
    if (is_same_source && is_same_just(a.k_data, width, fill)) goto done;

    bool changed = false;
    CSN_STR_WRITER w = csn_str_writer(csound, a.h, a.src);
    res = string_just_assign_value(&w, a.src, a.src, width, fill, mode, &changed);
    if (changed) update_array_data_version(&a.src->version);
    if (res != OK) goto done;

    publish_just(a.k_data, a.src, NULL, width, fill);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnstrings_center(CSOUND *csound, CSN_BINOP_HS *p) {
    return string_just_helper(csound, just_args(p), CSN_STRING_CENTER, false);
}

int32_t csnstrings_center_k_init(CSOUND *csound, CSN_BINOP_HS *p) {
    return string_just_helper(csound, just_args(p), CSN_STRING_CENTER, true);
}

int32_t csnstrings_center_k(CSOUND *csound, CSN_BINOP_HS *p) {
    return string_just_k_helper(csound, just_args(p), CSN_STRING_CENTER);
}

int32_t csnstrings_center_in(CSOUND *csound, CSN_UNARYOP_SCALAR_IN *p) {
    return string_just_in_helper(csound, just_in_args(p), CSN_STRING_CENTER, false);
}

int32_t csnstrings_center_in_k_init(CSOUND *csound, CSN_UNARYOP_SCALAR_IN *p) {
    return string_just_in_helper(csound, just_in_args(p), CSN_STRING_CENTER, true);
}

int32_t csnstrings_center_in_k(CSOUND *csound, CSN_UNARYOP_SCALAR_IN *p) {
    return string_just_in_k_helper(csound, just_in_args(p), CSN_STRING_CENTER);
}

int32_t csnstrings_ljust(CSOUND *csound, CSN_BINOP_HS *p) {
    return string_just_helper(csound, just_args(p), CSN_STRING_LJUST, false);
}

int32_t csnstrings_ljust_k_init(CSOUND *csound, CSN_BINOP_HS *p) {
    return string_just_helper(csound, just_args(p), CSN_STRING_LJUST, true);
}

int32_t csnstrings_ljust_k(CSOUND *csound, CSN_BINOP_HS *p) {
    return string_just_k_helper(csound, just_args(p), CSN_STRING_LJUST);
}

int32_t csnstrings_ljust_in(CSOUND *csound, CSN_UNARYOP_SCALAR_IN *p) {
    return string_just_in_helper(csound, just_in_args(p), CSN_STRING_LJUST, false);
}

int32_t csnstrings_ljust_in_k_init(CSOUND *csound, CSN_UNARYOP_SCALAR_IN *p) {
    return string_just_in_helper(csound, just_in_args(p), CSN_STRING_LJUST, true);
}

int32_t csnstrings_ljust_in_k(CSOUND *csound, CSN_UNARYOP_SCALAR_IN *p) {
    return string_just_in_k_helper(csound, just_in_args(p), CSN_STRING_LJUST);
}

int32_t csnstrings_rjust(CSOUND *csound, CSN_BINOP_HS *p) {
    return string_just_helper(csound, just_args(p), CSN_STRING_RJUST, false);
}

int32_t csnstrings_rjust_k_init(CSOUND *csound, CSN_BINOP_HS *p) {
    return string_just_helper(csound, just_args(p), CSN_STRING_RJUST, true);
}

int32_t csnstrings_rjust_k(CSOUND *csound, CSN_BINOP_HS *p) {
    return string_just_k_helper(csound, just_args(p), CSN_STRING_RJUST);
}

int32_t csnstrings_rjust_in(CSOUND *csound, CSN_UNARYOP_SCALAR_IN *p) {
    return string_just_in_helper(csound, just_in_args(p), CSN_STRING_RJUST, false);
}

int32_t csnstrings_rjust_in_k_init(CSOUND *csound, CSN_UNARYOP_SCALAR_IN *p) {
    return string_just_in_helper(csound, just_in_args(p), CSN_STRING_RJUST, true);
}

int32_t csnstrings_rjust_in_k(CSOUND *csound, CSN_UNARYOP_SCALAR_IN *p) {
    return string_just_in_k_helper(csound, just_in_args(p), CSN_STRING_RJUST);
}

/* ---------- padding with a fill character ---------- */

int32_t csnstrings_center_fill(CSOUND *csound, CSN_STRJUST *p) {
    return string_just_helper(csound, just_fill_args(p), CSN_STRING_CENTER, false);
}

int32_t csnstrings_center_fill_k_init(CSOUND *csound, CSN_STRJUST *p) {
    return string_just_helper(csound, just_fill_args(p), CSN_STRING_CENTER, true);
}

int32_t csnstrings_center_fill_k(CSOUND *csound, CSN_STRJUST *p) {
    return string_just_k_helper(csound, just_fill_args(p), CSN_STRING_CENTER);
}

int32_t csnstrings_center_fill_in(CSOUND *csound, CSN_STRJUST_IN *p) {
    return string_just_in_helper(csound, just_fill_in_args(p), CSN_STRING_CENTER, false);
}

int32_t csnstrings_center_fill_in_k_init(CSOUND *csound, CSN_STRJUST_IN *p) {
    return string_just_in_helper(csound, just_fill_in_args(p), CSN_STRING_CENTER, true);
}

int32_t csnstrings_center_fill_in_k(CSOUND *csound, CSN_STRJUST_IN *p) {
    return string_just_in_k_helper(csound, just_fill_in_args(p), CSN_STRING_CENTER);
}

int32_t csnstrings_ljust_fill(CSOUND *csound, CSN_STRJUST *p) {
    return string_just_helper(csound, just_fill_args(p), CSN_STRING_LJUST, false);
}

int32_t csnstrings_ljust_fill_k_init(CSOUND *csound, CSN_STRJUST *p) {
    return string_just_helper(csound, just_fill_args(p), CSN_STRING_LJUST, true);
}

int32_t csnstrings_ljust_fill_k(CSOUND *csound, CSN_STRJUST *p) {
    return string_just_k_helper(csound, just_fill_args(p), CSN_STRING_LJUST);
}

int32_t csnstrings_ljust_fill_in(CSOUND *csound, CSN_STRJUST_IN *p) {
    return string_just_in_helper(csound, just_fill_in_args(p), CSN_STRING_LJUST, false);
}

int32_t csnstrings_ljust_fill_in_k_init(CSOUND *csound, CSN_STRJUST_IN *p) {
    return string_just_in_helper(csound, just_fill_in_args(p), CSN_STRING_LJUST, true);
}

int32_t csnstrings_ljust_fill_in_k(CSOUND *csound, CSN_STRJUST_IN *p) {
    return string_just_in_k_helper(csound, just_fill_in_args(p), CSN_STRING_LJUST);
}

int32_t csnstrings_rjust_fill(CSOUND *csound, CSN_STRJUST *p) {
    return string_just_helper(csound, just_fill_args(p), CSN_STRING_RJUST, false);
}

int32_t csnstrings_rjust_fill_k_init(CSOUND *csound, CSN_STRJUST *p) {
    return string_just_helper(csound, just_fill_args(p), CSN_STRING_RJUST, true);
}

int32_t csnstrings_rjust_fill_k(CSOUND *csound, CSN_STRJUST *p) {
    return string_just_k_helper(csound, just_fill_args(p), CSN_STRING_RJUST);
}

int32_t csnstrings_rjust_fill_in(CSOUND *csound, CSN_STRJUST_IN *p) {
    return string_just_in_helper(csound, just_fill_in_args(p), CSN_STRING_RJUST, false);
}

int32_t csnstrings_rjust_fill_in_k_init(CSOUND *csound, CSN_STRJUST_IN *p) {
    return string_just_in_helper(csound, just_fill_in_args(p), CSN_STRING_RJUST, true);
}

int32_t csnstrings_rjust_fill_in_k(CSOUND *csound, CSN_STRJUST_IN *p) {
    return string_just_in_k_helper(csound, just_fill_in_args(p), CSN_STRING_RJUST);
}

/* ---------- replace ---------- */

/* What a replace reads from its opcode struct: out is NULL in place, trig is
   read by the k passes only. */
typedef struct {
    OPDS *h;
    CSN_ARRAY *out;
    CSN_ARRAY *src;
    STRINGDAT *old;
    STRINGDAT *new;
    cs_float *count;
    cs_float *trig;
    K_DATA *k_data;
} CSN_REPLACE_ARGS;

static CSN_REPLACE_ARGS replace_args(CSN_STRING_REPLACE *p) {
    return (CSN_REPLACE_ARGS) { &p->h, p->handle, p->source_handle, p->old, p->new, p->count, p->trig, &p->k_data };
}

static CSN_REPLACE_ARGS replace_in_args(CSN_STRING_REPLACE_IN *p) {
    return (CSN_REPLACE_ARGS) { &p->h, NULL, p->source_handle, p->old, p->new, p->count, p->trig, &p->k_data };
}

static int32_t check_replace_strings(CSOUND *csound, OPDS *perf_h, CSN_REPLACE_ARGS a) {
    if (IS_VALID_STRINGDAT(a.old) && IS_VALID_STRINGDAT(a.new)) return OK;
    return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] Invalid/NULL string");
}

/* The count is i-rate, so the init pass judges it once. Any negative count,
   the default -1 included, replaces every occurrence, as in Python. */
static int32_t check_replace_count(CSOUND *csound, double count) {
    if (isfinite(count) && trunc(count) == count) return OK;
    return csn_init_error(csound, "[csnarray] A replace count must be a whole number, got %g", count);
}

static size_t replace_limit(double count) {
    if (count < 0.0 || count >= (double) SIZE_MAX) return SIZE_MAX;
    return (size_t) count;
}

/* The occurrences of old a replace takes in s: from the left, never
   overlapping, at most limit. An empty old matches before every byte and at
   the end, as in Python, so "abc" holds 4. */
static size_t replace_occurrences(const char *s, const char *old, size_t len_old, size_t limit) {
    size_t n = 0;
    if (len_old == 0) {
        n = strlen(s) + 1;
    } else {
        for (const char *at = strstr(s, old); at != NULL && n < limit; at = strstr(at + len_old, old)) n++;
    }
    return n < limit ? n : limit;
}

/* Replaces the first n occurrences inside s, whose buffer already holds the
   result. Each one moves the tail by the difference and the search goes on
   after the text put in, which is never searched again; an empty old also
   steps over one byte of the string after each insertion. */
static void replace_in_buffer(char *s, const char *old, size_t len_old, const char *new, size_t len_new, size_t n) {
    char *at = s;
    for (size_t k = 0; k < n; k++) {
        char *hit = len_old == 0 ? at : strstr(at, old);
        if (hit == NULL) break;
        memmove(hit + len_new, hit + len_old, strlen(hit + len_old) + 1);
        memcpy(hit, new, len_new);
        at = hit + len_new;
        if (len_old == 0 && *at != '\0') at++;
    }
}


static int32_t replace_string_mono(CSN_STR_WRITER *w, CSN_STRDATA *out, CSN_STRDATA *src, const char *old, const char *new, size_t len_new, size_t len_old, size_t limit) {
    bool rewrites = strcmp(old, new) != 0;
    const char *s = slot_string(src);
    size_t n = replace_occurrences(s, old, len_old, limit);
    size_t bytes = strlen(s) - n * len_old + n * len_new + 1;
    if (csn_str_write(w, out, src->data, GET_STRINGDAT_LEN(src->data)) != OK) return w->err;
    if (n == 0 || !rewrites) return OK;

    int32_t res = ensure_string_capacity(w->csound, w->perf_h, out, bytes, w->no_growth);
    if (res != OK) return w->err = res;
    replace_in_buffer(out->data, old, len_old, new, len_new, n);
    return OK;
}


/* np.strings.replace on bytes. out may be src: in place the slots are probed
   first, so a real-time pass that stops leaves the array as it was. changed
   says whether a string was rewritten. */
static int32_t string_replace_assign_value(CSN_STR_WRITER *w, CSN_ARRAY *out, const CSN_ARRAY *src, const char *old, const char *new, size_t limit, bool *changed) {
    bool in_place = out == src;
    size_t len_old = strlen(old);
    size_t len_new = strlen(new);
    if (changed != NULL) *changed = false;

    if (in_place && w->no_growth) {
        for (size_t i = 0; i < src->size; i++) {
            const CSN_STRDATA *slot = &src->sdata[i];
            const char *s = slot_string(slot);
            size_t n = replace_occurrences(s, old, len_old, limit);
            size_t bytes = strlen(s) - n * len_old + n * len_new + 1;
            if (n > 0 && bytes > slot->capacity) {
                return w->err = refuse_string_growth(w->csound, w->perf_h, bytes, slot->capacity);
            }
        }
    }

    for (size_t i = 0; i < src->size; i++) {
        int32_t res = replace_string_mono(w, &out->sdata[i], &src->sdata[i], old, new, len_new, len_old, limit);
        if (res != OK) return res;
        if (changed != NULL) *changed = true;
    }

    return OK;
}

/* A k init releases the copies of old and new a reinit leaves, then reserves
   them, so a pass on a real-time path keeps them without allocating. Replace
   is not idempotent ("a" to "aa" grows again), so an output that is its own
   source is left to the perf pass, or the first cycle would replace twice,
   and X = csnstrreplace(X, ..., k) replaces again on every pass. */
static int32_t string_replace_helper(CSOUND *csound, CSN_REPLACE_ARGS a, bool perf_redoes) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    if (perf_redoes) csn_kdata_release_strings(csound, a.k_data);

    int32_t res = check_replace_strings(csound, NULL, a);
    if (res == OK) res = check_replace_count(csound, (double) *a.count);
    if (res != OK) return res;

    stamp_globals(csound, a.h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { a.out, a.src }, 2);

    const CSN_ARRAY *ops[1] = { a.src };
    CSNARR_RES r = prepare_output(csound, a.h, NULL, reg, a.out, a.src->ndim, a.src->shape, CSN_STRING, ops, 1, CSN_ALIAS_ELEMENTWISE);
    res = r.err;
    if (res != OK) goto done;
    pres.locked |= r.locked;

    a.k_data->registry = reg;
    if (perf_redoes) csn_kdata_keep_strings(csound, a.k_data, a.old->data, a.new->data, true);
    if (perf_redoes && r.aliased) goto done;

    bool changed = false;
    CSN_STR_WRITER w = csn_str_writer(csound, NULL, a.out);
    res = string_replace_assign_value(&w, a.out, a.src, a.old->data, a.new->data, replace_limit((double) *a.count), &changed);
    if (r.aliased && changed) update_array_data_version(&a.src->version);
    if (res != OK) goto done;

    PUBLISH_DERIVED_RESULT(a.k_data, a.src, a.out);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

static int32_t string_replace_k_helper(CSOUND *csound, CSN_REPLACE_ARGS a) {
    CSN_REGISTRY *reg = a.k_data->registry;
    CHECK_REGISTRY(csound, a.h, reg);

    CHECK_KTRIG(a.trig);

    int32_t res = check_replace_strings(csound, a.h, a);
    if (res != OK) return res;

    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { a.out, a.src }, 2);

    if (CAN_REUSE_STRING_ELEMENTWISE(a.k_data, a.src, NULL, a.out, a.old->data, a.new->data)) goto done;

    const CSN_ARRAY *ops[1] = { a.src };
    CSNARR_RES r = prepare_output(csound, a.h, a.h, reg, a.out, a.src->ndim, a.src->shape, CSN_STRING, ops, 1, CSN_ALIAS_ELEMENTWISE);
    res = r.err;
    if (res != OK) goto done;
    pres.locked |= r.locked;

    bool changed = false;
    CSN_STR_WRITER w = csn_str_writer(csound, a.h, a.out);
    res = string_replace_assign_value(&w, a.out, a.src, a.old->data, a.new->data, replace_limit((double) *a.count), &changed);
    if (r.aliased && changed) update_array_data_version(&a.src->version);
    if (res != OK) goto done;

    PUBLISH_STRING_ELEMENTWISE(csound, a.k_data, a.src, NULL, a.out, a.old->data, a.new->data, !a.out->rt_locked);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

static int32_t string_replace_in_helper(CSOUND *csound, CSN_REPLACE_ARGS a, bool is_k_init) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    a.k_data->registry = reg;
    if (is_k_init) csn_kdata_release_strings(csound, a.k_data);

    int32_t res = check_replace_strings(csound, NULL, a);
    if (res == OK) res = check_replace_count(csound, (double) *a.count);
    if (res != OK) return res;

    /* the target changes at perf: whatever reads it follows it there */
    if (is_k_init) {
        csn_mark_krate(a.src);
        csn_kdata_keep_strings(csound, a.k_data, a.old->data, a.new->data, true);
    }

    stamp_globals(csound, a.h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { a.src }, 1);

    bool changed = false;
    CSN_STR_WRITER w = csn_str_writer(csound, NULL, a.src);
    res = string_replace_assign_value(&w, a.src, a.src, a.old->data, a.new->data, replace_limit((double) *a.count), &changed);
    if (changed) update_array_data_version(&a.src->version);
    if (res != OK) goto done;

    PUBLISH_DERIVED_RESULT(a.k_data, a.src, NULL);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

/* Runs once for each write by someone else or each new old or new, so a
   replace is applied once per change, as csnreverse.in does. */
static int32_t string_replace_in_k_helper(CSOUND *csound, CSN_REPLACE_ARGS a) {
    CSN_REGISTRY *reg = a.k_data->registry;
    CHECK_REGISTRY(csound, a.h, reg);

    CHECK_KTRIG(a.trig);

    int32_t res = check_replace_strings(csound, a.h, a);
    if (res != OK) return res;

    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { a.src }, 1);

    if (CAN_REUSE_STRING_ELEMENTWISE(a.k_data, a.src, NULL, NULL, a.old->data, a.new->data)) goto done;

    bool changed = false;
    CSN_STR_WRITER w = csn_str_writer(csound, a.h, a.src);
    res = string_replace_assign_value(&w, a.src, a.src, a.old->data, a.new->data, replace_limit((double) *a.count), &changed);
    if (changed) update_array_data_version(&a.src->version);
    if (res != OK) goto done;

    PUBLISH_STRING_ELEMENTWISE(csound, a.k_data, a.src, NULL, NULL, a.old->data, a.new->data, !a.src->rt_locked);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

/* The k forms keep copies of old and new in k_data. */
int32_t csnstrings_replace_deinit(CSOUND *csound, CSN_STRING_REPLACE *p) {
    csn_kdata_release_strings(csound, &p->k_data);
    return OK;
}

int32_t csnstrings_replace_in_deinit(CSOUND *csound, CSN_STRING_REPLACE_IN *p) {
    csn_kdata_release_strings(csound, &p->k_data);
    return OK;
}

int32_t csnstrings_replace(CSOUND *csound, CSN_STRING_REPLACE *p) {
    return string_replace_helper(csound, replace_args(p), false);
}

int32_t csnstrings_replace_k_init(CSOUND *csound, CSN_STRING_REPLACE *p) {
    return string_replace_helper(csound, replace_args(p), true);
}

int32_t csnstrings_replace_k(CSOUND *csound, CSN_STRING_REPLACE *p) {
    return string_replace_k_helper(csound, replace_args(p));
}

int32_t csnstrings_replace_in(CSOUND *csound, CSN_STRING_REPLACE_IN *p) {
    return string_replace_in_helper(csound, replace_in_args(p), false);
}

int32_t csnstrings_replace_in_k_init(CSOUND *csound, CSN_STRING_REPLACE_IN *p) {
    return string_replace_in_helper(csound, replace_in_args(p), true);
}

int32_t csnstrings_replace_in_k(CSOUND *csound, CSN_STRING_REPLACE_IN *p) {
    return string_replace_in_k_helper(csound, replace_in_args(p));
}

/* ---------- substr ---------- */

/* What a substr reads from its opcode struct: out is NULL in place, trig is
   read by the k passes only, prev is the range the last pass cut. */
typedef struct {
    OPDS *h;
    CSN_ARRAY *out;
    CSN_ARRAY *src;
    cs_float *start;
    cs_float *stop;
    cs_float *step;
    cs_float *trig;
    K_DATA *k_data;
    CSN_SUBSTR_RANGE *prev;
} CSN_SUBSTR_ARGS;

static CSN_SUBSTR_ARGS substr_args(CSN_SUBSTRING *p) {
    return (CSN_SUBSTR_ARGS) { &p->h, p->handle, p->source_handle, p->start, p->stop, p->step, p->trig, &p->k_data, &p->prev };
}

static CSN_SUBSTR_ARGS substr_in_args(CSN_SUBSTRING_IN *p) {
    return (CSN_SUBSTR_ARGS) { &p->h, NULL, p->source_handle, p->start, p->stop, p->step, p->trig, &p->k_data, &p->prev };
}

/* The bytes from start up to stop, stop excluded, every step-th, as
   csngetslice cuts an axis: whole numbers from 0 up, start no greater than
   stop and a step of at least 1. Each string clamps them to its length. */
static bool is_substr_range(double start, double stop, double step) {
    return IS_VALID_INDEX(start) && IS_VALID_INDEX(stop) && IS_VALID_INDEX(step) && step >= 1.0 && start <= stop;
}

static int32_t check_substr_range(CSOUND *csound, OPDS *perf_h, double start, double stop, double step) {
    if (!IS_VALID_INDEX(start) || !IS_VALID_INDEX(stop) || !IS_VALID_INDEX(step)) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] A substring start, stop and step must be whole numbers from 0 up, got %g, %g and %g", start, stop, step);
    }
    if (step < 1.0) return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] A substring step must be at least 1");
    if (start > stop) return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] A substring start (%g) must not be greater than its stop (%g)", start, stop);
    return OK;
}

static CSN_SUBSTR_RANGE substr_range(CSN_SUBSTR_ARGS a) {
    return (CSN_SUBSTR_RANGE) { (uint32_t) *a.start, (uint32_t) *a.stop, (uint32_t) *a.step };
}

static bool is_same_substr_range(const CSN_SUBSTR_RANGE *x, CSN_SUBSTR_RANGE y) {
    return x->start == y.start && x->stop == y.stop && x->step == y.step;
}

static size_t size_from_range(CSN_SUBSTR_RANGE *range, size_t len_s) {
    size_t begin = range->start < len_s ? range->start : len_s;
    size_t end = range->stop < len_s ? range->stop : len_s;
    size_t sub_len = (end - begin + range->step - 1) / range->step;
    return sub_len;
}

/* A substring is never longer than its string, so in place each one moves
   left inside its own buffer, reading ahead of where it writes, and an
   output holds the room prepare_output gave it for the longest source.
   changed says whether a string was cut. */
static int32_t substring_assign_value(CSN_STR_WRITER *w, CSN_ARRAY *out, const CSN_ARRAY *src, CSN_SUBSTR_RANGE range, bool *changed) {
    bool in_place = out == src;
    *changed = false;

    for (size_t i = 0; i < src->size; i++) {
        const char *s = slot_string(&src->sdata[i]);
        size_t len = strlen(s);
        size_t begin = range.start < len ? range.start : len;
        size_t sub_len = size_from_range(&range, len);
        /* the whole string: begin is 0, and the step 1 or the string short */
        if (in_place && sub_len == len) continue;

        if (!in_place) {
            int32_t res = ensure_string_capacity(w->csound, w->perf_h, &out->sdata[i], sub_len + 1, w->no_growth);
            if (res != OK) return w->err = res;
        }
        char *dest = out->sdata[i].data;
        for (size_t j = 0; j < sub_len; j++) dest[j] = s[begin + j * range.step];
        dest[sub_len] = '\0';
        *changed = true;
    }

    return OK;
}

/* A k range is not known at init, but its init values still cut the strings
   when they make a range, as csnstrmul does with a count; when they do not,
   the first pass cuts, and the perf pass judges the range. A substring cut
   again is shorter again, so an output that is its own source is left to the
   perf pass at a k init, or the first cycle would cut twice, and
   X = csnstrsubstr(X, 1, 100, 1, k) cuts again on every pass. */
static int32_t substr_helper(CSOUND *csound, CSN_SUBSTR_ARGS a, bool perf_redoes) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    double start = (double) *a.start, stop = (double) *a.stop, step = (double) *a.step;
    bool known = !perf_redoes || (csn_known_at_init(csound, a.start) && csn_known_at_init(csound, a.stop) && csn_known_at_init(csound, a.step));
    int32_t res = OK;
    if (known) {
        res = check_substr_range(csound, NULL, start, stop, step);
        if (res != OK) return res;
    }

    stamp_globals(csound, a.h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { a.out, a.src }, 2);

    const CSN_ARRAY *ops[1] = { a.src };
    CSNARR_RES r = prepare_output(csound, a.h, NULL, reg, a.out, a.src->ndim, a.src->shape, CSN_STRING, ops, 1, CSN_ALIAS_ELEMENTWISE);
    res = r.err;
    if (res != OK) goto done;
    pres.locked |= r.locked;

    a.k_data->registry = reg;
    if (!is_substr_range(start, stop, step) || (perf_redoes && r.aliased)) goto done;

    bool changed = false;
    CSN_SUBSTR_RANGE range = substr_range(a);
    CSN_STR_WRITER w = csn_str_writer(csound, NULL, a.out);
    res = substring_assign_value(&w, a.out, a.src, range, &changed);
    if (r.aliased && changed) update_array_data_version(&a.src->version);
    if (res != OK) goto done;

    PUBLISH_DERIVED_RESULT(a.k_data, a.src, a.out);
    *a.prev = range;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

static int32_t substr_k_helper(CSOUND *csound, CSN_SUBSTR_ARGS a) {
    CSN_REGISTRY *reg = a.k_data->registry;
    CHECK_REGISTRY(csound, a.h, reg);

    CHECK_KTRIG(a.trig);

    int32_t res = check_substr_range(csound, a.h, (double) *a.start, (double) *a.stop, (double) *a.step);
    if (res != OK) return res;
    CSN_SUBSTR_RANGE range = substr_range(a);

    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { a.out, a.src }, 2);

    if (is_same_substr_range(a.prev, range) && CAN_REUSE_LAST_RESULT(a.k_data, a.src, a.out)) goto done;

    const CSN_ARRAY *ops[1] = { a.src };
    CSNARR_RES r = prepare_output(csound, a.h, a.h, reg, a.out, a.src->ndim, a.src->shape, CSN_STRING, ops, 1, CSN_ALIAS_ELEMENTWISE);
    res = r.err;
    if (res != OK) goto done;
    pres.locked |= r.locked;

    bool changed = false;
    CSN_STR_WRITER w = csn_str_writer(csound, a.h, a.out);
    res = substring_assign_value(&w, a.out, a.src, range, &changed);
    if (r.aliased && changed) update_array_data_version(&a.src->version);
    if (res != OK) goto done;

    PUBLISH_DERIVED_RESULT(a.k_data, a.src, a.out);
    *a.prev = range;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

/* In place a cut cannot be undone, so a k range unknown at init is left to
   the first pass rather than applied with its init values. */
static int32_t substr_in_helper(CSOUND *csound, CSN_SUBSTR_ARGS a, bool is_k_init) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    a.k_data->registry = reg;

    /* the target changes at perf: whatever reads it follows it there */
    if (is_k_init) {
        csn_mark_krate(a.src);
        bool known = csn_known_at_init(csound, a.start) && csn_known_at_init(csound, a.stop) && csn_known_at_init(csound, a.step);
        if (!known) return OK;
    }

    int32_t res = check_substr_range(csound, NULL, (double) *a.start, (double) *a.stop, (double) *a.step);
    if (res != OK) return res;
    CSN_SUBSTR_RANGE range = substr_range(a);

    stamp_globals(csound, a.h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { a.src }, 1);

    bool changed = false;
    CSN_STR_WRITER w = csn_str_writer(csound, NULL, a.src);
    res = substring_assign_value(&w, a.src, a.src, range, &changed);
    if (changed) update_array_data_version(&a.src->version);
    if (res != OK) goto done;

    PUBLISH_DERIVED_RESULT(a.k_data, a.src, NULL);
    *a.prev = range;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

/* Runs once for each write by someone else or each new range, so a cut is
   applied once per change, as csnreverse.in does. */
static int32_t substr_in_k_helper(CSOUND *csound, CSN_SUBSTR_ARGS a) {
    CSN_REGISTRY *reg = a.k_data->registry;
    CHECK_REGISTRY(csound, a.h, reg);

    CHECK_KTRIG(a.trig);

    int32_t res = check_substr_range(csound, a.h, (double) *a.start, (double) *a.stop, (double) *a.step);
    if (res != OK) return res;
    CSN_SUBSTR_RANGE range = substr_range(a);

    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { a.src }, 1);

    if (is_same_substr_range(a.prev, range) && CAN_REUSE_LAST_RESULT(a.k_data, a.src, NULL)) goto done;

    bool changed = false;
    CSN_STR_WRITER w = csn_str_writer(csound, a.h, a.src);
    res = substring_assign_value(&w, a.src, a.src, range, &changed);
    if (changed) update_array_data_version(&a.src->version);
    if (res != OK) goto done;

    PUBLISH_DERIVED_RESULT(a.k_data, a.src, NULL);
    *a.prev = range;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnstrings_substring(CSOUND *csound, CSN_SUBSTRING *p) {
    return substr_helper(csound, substr_args(p), false);
}

int32_t csnstrings_substring_k_init(CSOUND *csound, CSN_SUBSTRING *p) {
    return substr_helper(csound, substr_args(p), true);
}

int32_t csnstrings_substring_k(CSOUND *csound, CSN_SUBSTRING *p) {
    return substr_k_helper(csound, substr_args(p));
}

int32_t csnstrings_substring_in(CSOUND *csound, CSN_SUBSTRING_IN *p) {
    return substr_in_helper(csound, substr_in_args(p), false);
}

int32_t csnstrings_substring_in_k_init(CSOUND *csound, CSN_SUBSTRING_IN *p) {
    return substr_in_helper(csound, substr_in_args(p), true);
}

int32_t csnstrings_substring_in_k(CSOUND *csound, CSN_SUBSTRING_IN *p) {
    return substr_in_k_helper(csound, substr_in_args(p));
}

/* Queries: one number per string (len, find, rfind, startswith, endswith, the
   is* predicates), into a CsnArr shaped as the source. Bytes, ASCII only,
   whatever the locale, as NumPy on S arrays. A slot never written reads as "".
   arg is the S parameter, NULL for the queries without one. The searches look
   at the bytes from start up to end, end excluded, as s[start:end] in Python:
   end is clamped to each string and -1 stands for its end; begin and end
   reach a query clamped that way, and a begin past the end matches nothing,
   not even "". */
typedef double (*CSN_STR_QUERY)(const char *s, size_t begin, size_t end, const char *arg);

typedef struct {
    OPDS *h;
    CSN_ARRAY *out;
    CSN_ARRAY *src;
    const STRINGDAT *value; // NULL when the query takes no S
    cs_float *start;        // NULL when the query takes no range
    cs_float *end;
    cs_float *trig;
    K_DATA *k_data;
    CSN_SUBSTR_RANGE *prev; // the range a k pass last searched
} CSN_STR_QUERY_ARGS;

static CSN_STR_QUERY_ARGS query_args(CSN_UNARYOP *p) {
    return (CSN_STR_QUERY_ARGS) { &p->h, p->handle, p->source_handle, NULL, NULL, NULL, p->trig, &p->k_data, NULL };
}

static CSN_STR_QUERY_ARGS query_range_args(CSN_STRQUERY_STRUCT *p) {
    return (CSN_STR_QUERY_ARGS) { &p->h, p->handle, p->source_handle, p->value, p->start, p->end, p->trig, &p->k_data, &p->prev };
}

static double query_len(const char *s, size_t begin, size_t end, const char *arg) {
    (void) s;
    (void) arg;
    return (double) (end - begin);
}

static double query_find(const char *s, size_t begin, size_t end, const char *arg) {
    size_t len_arg = strlen(arg);
    if (begin > end || end - begin < len_arg) return -1.0;
    for (size_t i = begin; i + len_arg <= end; i++) {
        if (memcmp(s + i, arg, len_arg) == 0) return (double) i;
    }
    return -1.0;
}

/* The last match, overlapping ones included; an empty arg matches at the end. */
static double query_rfind(const char *s, size_t begin, size_t end, const char *arg) {
    size_t len_arg = strlen(arg);
    if (begin > end || end - begin < len_arg) return -1.0;
    for (size_t i = end - len_arg + 1; i-- > begin;) {
        if (memcmp(s + i, arg, len_arg) == 0) return (double) i;
    }
    return -1.0;
}

static double query_startswith(const char *s, size_t begin, size_t end, const char *arg) {
    size_t len_arg = strlen(arg);
    if (begin > end || end - begin < len_arg) return 0.0;
    return memcmp(s + begin, arg, len_arg) == 0 ? 1.0 : 0.0;
}

static double query_endswith(const char *s, size_t begin, size_t end, const char *arg) {
    size_t len_arg = strlen(arg);
    if (begin > end || end - begin < len_arg) return 0.0;
    return memcmp(s + end - len_arg, arg, len_arg) == 0 ? 1.0 : 0.0;
}

static bool ascii_is_digit(unsigned char c) { return c >= '0' && c <= '9'; }
static bool ascii_is_alnum(unsigned char c) { return ascii_is_letter(c) || ascii_is_digit(c); }

/* Python's bytes predicates: true when every byte passes and there is one. */
static double all_bytes(const char *s, bool (*pass)(unsigned char)) {
    if (*s == '\0') return 0.0;
    for (; *s != '\0'; s++) {
        if (!pass((unsigned char) *s)) return 0.0;
    }
    return 1.0;
}

static double query_isalpha(const char *s, size_t begin, size_t end, const char *arg) {
    (void) begin;
    (void) end;
    (void) arg;
    return all_bytes(s, ascii_is_letter);
}

static double query_isdigit(const char *s, size_t begin, size_t end, const char *arg) {
    (void) begin;
    (void) end;
    (void) arg;
    return all_bytes(s, ascii_is_digit);
}

static double query_isalnum(const char *s, size_t begin, size_t end, const char *arg) {
    (void) begin;
    (void) end;
    (void) arg;
    return all_bytes(s, ascii_is_alnum);
}

static double query_isspace(const char *s, size_t begin, size_t end, const char *arg) {
    (void) begin;
    (void) end;
    (void) arg;
    return all_bytes(s, ascii_is_space);
}

static int32_t check_query_value(CSOUND *csound, OPDS *perf_h, const STRINGDAT *value) {
    if (value != NULL && !IS_VALID_STRINGDAT(value)) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] Invalid string");
    }
    return OK;
}

static bool is_query_range(double start, double end) {
    return IS_VALID_INDEX(start) && (end == -1.0 || (IS_VALID_INDEX(end) && start <= end));
}

static int32_t check_query_range(CSOUND *csound, OPDS *perf_h, double start, double end) {
    if (!IS_VALID_INDEX(start) || (end != -1.0 && !IS_VALID_INDEX(end))) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] A search start must be a whole number from 0 up, and its end one too or -1 for the end of the string, got %g and %g", start, end);
    }
    if (end != -1.0 && start > end) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] A search start (%g) must not be greater than its end (%g)", start, end);
    }
    return OK;
}

/* The whole string for the queries without a range; an end of -1 reads as
   UINT32_MAX, which every string is shorter than. Only for a checked range. */
static CSN_SUBSTR_RANGE query_range(const CSN_STR_QUERY_ARGS a) {
    if (a.start == NULL) return (CSN_SUBSTR_RANGE) { 0U, UINT32_MAX, 0U };
    double end = (double) *a.end;
    return (CSN_SUBSTR_RANGE) { (uint32_t) *a.start, end == -1.0 ? UINT32_MAX : (uint32_t) end, 0U };
}

static const char *query_arg(const CSN_STR_QUERY_ARGS a) {
    return a.value != NULL ? a.value->data : NULL;
}

static void string_query_assign_value(CSN_ARRAY *out, const CSN_ARRAY *src, CSN_STR_QUERY fn, const char *arg, CSN_SUBSTR_RANGE range) {
    for (size_t i = 0; i < src->size; i++) {
        const char *s = slot_string(&src->sdata[i]);
        size_t len = strlen(s);
        size_t end = range.stop < len ? range.stop : len;
        out->data[i] = fn(s, range.start, end, arg);
    }
}

/* An S is always known at init, and a k range is searched with its init
   values when they make one, as csnstrsubstr cuts; when they do not, the
   first pass searches, and the perf pass judges the range. A k init also
   keeps the copy of the S the perf pass compares against, so a pass on a
   real-time path keeps it without allocating; it releases the old one first,
   before a reinit takes the struct again. An i form keeps none. */
static int32_t string_query_helper(CSOUND *csound, CSN_STR_QUERY_ARGS a, CSN_STR_QUERY fn, bool is_k_init) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    if (is_k_init) csn_kdata_release_strings(csound, a.k_data);

    int32_t res = check_query_value(csound, NULL, a.value);
    if (res != OK) return res;

    bool has_range = a.start != NULL;
    double start = has_range ? (double) *a.start : 0.0;
    double end = has_range ? (double) *a.end : -1.0;
    bool known = !is_k_init || !has_range || (csn_known_at_init(csound, a.start) && csn_known_at_init(csound, a.end));
    if (known) {
        res = check_query_range(csound, NULL, start, end);
        if (res != OK) return res;
    }

    stamp_globals(csound, a.h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { a.out, a.src }, 2);

    const CSN_ARRAY *ops[1] = { a.src };
    CSNARR_RES r = prepare_output(csound, a.h, NULL, reg, a.out, a.src->ndim, a.src->shape, CSN_REAL, ops, 1, CSN_ALIAS_REFUSE);
    res = r.err;
    if (res != OK) goto done;
    pres.locked |= r.locked;

    a.k_data->registry = reg;
    if (is_k_init) csn_kdata_keep_strings(csound, a.k_data, query_arg(a), NULL, true);
    if (!is_query_range(start, end)) goto done;

    CSN_SUBSTR_RANGE range = query_range(a);
    string_query_assign_value(a.out, a.src, fn, query_arg(a), range);
    if (is_k_init) {
        PUBLISH_STRING_ELEMENTWISE(csound, a.k_data, a.src, NULL, a.out, query_arg(a), NULL, true);
        if (a.prev != NULL) *a.prev = range;
    }

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

static int32_t string_query_k_helper(CSOUND *csound, CSN_STR_QUERY_ARGS a, CSN_STR_QUERY fn) {
    CSN_REGISTRY *reg = a.k_data->registry;
    CHECK_REGISTRY(csound, a.h, reg);

    CHECK_KTRIG(a.trig);

    int32_t res = check_query_value(csound, a.h, a.value);
    if (res != OK) return res;
    if (a.start != NULL) {
        res = check_query_range(csound, a.h, (double) *a.start, (double) *a.end);
        if (res != OK) return res;
    }
    const char *arg = query_arg(a);
    CSN_SUBSTR_RANGE range = query_range(a);

    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { a.out, a.src }, 2);

    bool same_range = a.prev == NULL || is_same_substr_range(a.prev, range);
    if (same_range && CAN_REUSE_STRING_ELEMENTWISE(a.k_data, a.src, NULL, a.out, arg, NULL)) goto done;

    const CSN_ARRAY *ops[1] = { a.src };
    CSNARR_RES r = prepare_output(csound, a.h, a.h, reg, a.out, a.src->ndim, a.src->shape, CSN_REAL, ops, 1, CSN_ALIAS_REFUSE);
    res = r.err;
    if (res != OK) goto done;
    pres.locked |= r.locked;

    string_query_assign_value(a.out, a.src, fn, arg, range);
    PUBLISH_STRING_ELEMENTWISE(csound, a.k_data, a.src, NULL, a.out, arg, NULL, !a.out->rt_locked);
    if (a.prev != NULL) *a.prev = range;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnstrings_query_deinit(CSOUND *csound, CSN_STRQUERY_STRUCT *p) {
    csn_kdata_release_strings(csound, &p->k_data);
    return OK;
}

int32_t csnstrings_len(CSOUND *csound, CSN_UNARYOP *p) {
    return string_query_helper(csound, query_args(p), query_len, false);
}

int32_t csnstrings_len_k_init(CSOUND *csound, CSN_UNARYOP *p) {
    return string_query_helper(csound, query_args(p), query_len, true);
}

int32_t csnstrings_len_k(CSOUND *csound, CSN_UNARYOP *p) {
    return string_query_k_helper(csound, query_args(p), query_len);
}

int32_t csnstrings_find(CSOUND *csound, CSN_STRQUERY_STRUCT *p) {
    return string_query_helper(csound, query_range_args(p), query_find, false);
}

int32_t csnstrings_find_k_init(CSOUND *csound, CSN_STRQUERY_STRUCT *p) {
    return string_query_helper(csound, query_range_args(p), query_find, true);
}

int32_t csnstrings_find_k(CSOUND *csound, CSN_STRQUERY_STRUCT *p) {
    return string_query_k_helper(csound, query_range_args(p), query_find);
}

int32_t csnstrings_rfind(CSOUND *csound, CSN_STRQUERY_STRUCT *p) {
    return string_query_helper(csound, query_range_args(p), query_rfind, false);
}

int32_t csnstrings_rfind_k_init(CSOUND *csound, CSN_STRQUERY_STRUCT *p) {
    return string_query_helper(csound, query_range_args(p), query_rfind, true);
}

int32_t csnstrings_rfind_k(CSOUND *csound, CSN_STRQUERY_STRUCT *p) {
    return string_query_k_helper(csound, query_range_args(p), query_rfind);
}

int32_t csnstrings_startswith(CSOUND *csound, CSN_STRQUERY_STRUCT *p) {
    return string_query_helper(csound, query_range_args(p), query_startswith, false);
}

int32_t csnstrings_startswith_k_init(CSOUND *csound, CSN_STRQUERY_STRUCT *p) {
    return string_query_helper(csound, query_range_args(p), query_startswith, true);
}

int32_t csnstrings_startswith_k(CSOUND *csound, CSN_STRQUERY_STRUCT *p) {
    return string_query_k_helper(csound, query_range_args(p), query_startswith);
}

int32_t csnstrings_endswith(CSOUND *csound, CSN_STRQUERY_STRUCT *p) {
    return string_query_helper(csound, query_range_args(p), query_endswith, false);
}

int32_t csnstrings_endswith_k_init(CSOUND *csound, CSN_STRQUERY_STRUCT *p) {
    return string_query_helper(csound, query_range_args(p), query_endswith, true);
}

int32_t csnstrings_endswith_k(CSOUND *csound, CSN_STRQUERY_STRUCT *p) {
    return string_query_k_helper(csound, query_range_args(p), query_endswith);
}

int32_t csnstrings_isalpha(CSOUND *csound, CSN_UNARYOP *p) {
    return string_query_helper(csound, query_args(p), query_isalpha, false);
}

int32_t csnstrings_isalpha_k_init(CSOUND *csound, CSN_UNARYOP *p) {
    return string_query_helper(csound, query_args(p), query_isalpha, true);
}

int32_t csnstrings_isalpha_k(CSOUND *csound, CSN_UNARYOP *p) {
    return string_query_k_helper(csound, query_args(p), query_isalpha);
}

int32_t csnstrings_isdigit(CSOUND *csound, CSN_UNARYOP *p) {
    return string_query_helper(csound, query_args(p), query_isdigit, false);
}

int32_t csnstrings_isdigit_k_init(CSOUND *csound, CSN_UNARYOP *p) {
    return string_query_helper(csound, query_args(p), query_isdigit, true);
}

int32_t csnstrings_isdigit_k(CSOUND *csound, CSN_UNARYOP *p) {
    return string_query_k_helper(csound, query_args(p), query_isdigit);
}

int32_t csnstrings_isalnum(CSOUND *csound, CSN_UNARYOP *p) {
    return string_query_helper(csound, query_args(p), query_isalnum, false);
}

int32_t csnstrings_isalnum_k_init(CSOUND *csound, CSN_UNARYOP *p) {
    return string_query_helper(csound, query_args(p), query_isalnum, true);
}

int32_t csnstrings_isalnum_k(CSOUND *csound, CSN_UNARYOP *p) {
    return string_query_k_helper(csound, query_args(p), query_isalnum);
}

int32_t csnstrings_isspace(CSOUND *csound, CSN_UNARYOP *p) {
    return string_query_helper(csound, query_args(p), query_isspace, false);
}

int32_t csnstrings_isspace_k_init(CSOUND *csound, CSN_UNARYOP *p) {
    return string_query_helper(csound, query_args(p), query_isspace, true);
}

int32_t csnstrings_isspace_k(CSOUND *csound, CSN_UNARYOP *p) {
    return string_query_k_helper(csound, query_args(p), query_isspace);
}


/* ---------- conversions: strtonum, numtostr ---------- */

/* the longest string a double writes, "-1.2345678901234567e-308", and its NUL */
#define CSN_NUMBER_STRING_BYTES 25

/* the length of word at the start of s in any case, 0 when it is not there */
static size_t match_word(const char *s, const char *word) {
    size_t n = 0;
    for (; word[n] != '\0'; n++) {
        if (((unsigned char) s[n] | 0x20) != (unsigned char) word[n]) return 0;
    }
    return n;
}

/* A string reads as Python's float() reads it: blanks around, a sign, then
   digits with an optional point and exponent, or inf or infinity in any
   case. Anything else, a hex number, a decimal comma, digit underscores or a
   number with more text after it, is not a number and gives NaN, as "nan"
   does. strtod in the C locale makes the value, whatever locale the host
   set. */
static double string_to_number(CSOUND *csound, const char *s) {
    while (ascii_is_space((unsigned char) *s)) s++;
    const char *p = s;
    if (*p == '+' || *p == '-') p++;
    size_t word = match_word(p, "infinity");
    if (word == 0) word = match_word(p, "inf");
    if (word > 0) {
        p += word;
    } else {
        size_t digits = 0;
        for (; ascii_is_digit((unsigned char) *p); p++) digits++;
        if (*p == '.') {
            for (p++; ascii_is_digit((unsigned char) *p); p++) digits++;
        }
        if (digits == 0) return NAN;
        if (*p == 'e' || *p == 'E') {
            p++;
            if (*p == '+' || *p == '-') p++;
            if (!ascii_is_digit((unsigned char) *p)) return NAN;
            while (ascii_is_digit((unsigned char) *p)) p++;
        }
    }
    const char *end = p;
    while (ascii_is_space((unsigned char) *p)) p++;
    if (*p != '\0') return NAN;
    char *parsed = NULL;
    double value = csound->Strtod((char *) s, &parsed);
    return parsed == end ? value : NAN;
}

/* the double that digits d1d2...dn times 10^exponent reads back to, with
   d1 the units digit */
static double read_digits(CSOUND *csound, bool negative, const char *digits, size_t n, int32_t exponent) {
    char str[48];
    csound->Sprintf(str, "%s0.%.*se%d", negative ? "-" : "", (int) n, digits, exponent + 1);
    return csound->Strtod(str, NULL);
}

/* the next n-digit decimal up from digits: 999 + 1 is 100 one place up */
static void increment_digits(char *digits, size_t n, int32_t *exponent) {
    size_t i = n;
    while (i > 0 && digits[i - 1] == '9') digits[--i] = '0';
    if (i == 0) {
        digits[0] = '1';
        (*exponent)++;
        return;
    }
    digits[i - 1]++;
}

/* A number writes as NumPy's astype(str), Python's repr: the fewest digits
   that read back to the same double, the closest to it when several do,
   positional from 1e-4 up to below 1e16 with at least one decimal ("1.0"),
   with an exponent of two digits or more outside it ("1e+16", "1e-05"). out
   holds CSN_NUMBER_STRING_BYTES. */
static void number_to_string(CSOUND *csound, double x, char *out) {
    if (isnan(x)) {
        strcpy(out, "nan");
        return;
    }
    if (isinf(x)) {
        strcpy(out, x < 0.0 ? "-inf" : "inf");
        return;
    }
    bool negative = signbit(x) != 0;
    char digits[17];
    size_t n = 0;
    int32_t exponent = 0;
    for (size_t want = 1; want <= 17; want++) {
        /* the n-digit decimal nearest x, as [-]d[.ddd]e(+|-)xx */
        char sci[32];
        csound->Sprintf(sci, "%.*e", (int) want - 1, x);
        const char *p = sci + (negative ? 1 : 0);
        for (n = 0; *p != 'e'; p++) {
            if (*p != '.') digits[n++] = *p;
        }
        exponent = atoi(p + 1);
        double back = read_digits(csound, negative, digits, n, exponent);
        if (back == x) break;
        /* the nearest misses only at a power of two, where the doubles below
           sit twice as close as those above: it fell below, and the decimal
           one step up may still read back */
        increment_digits(digits, n, &exponent);
        if (read_digits(csound, negative, digits, n, exponent) == x) break;
    }
    while (n > 1 && digits[n - 1] == '0') n--;

    char *o = out;
    if (negative) *o++ = '-';
    if (exponent < -4 || exponent >= 16) {
        *o++ = digits[0];
        if (n > 1) {
            *o++ = '.';
            memcpy(o, digits + 1, n - 1);
            o += n - 1;
        }
        csound->Sprintf(o, "e%c%02d", exponent < 0 ? '-' : '+', abs(exponent));
    } else if (exponent < 0) {
        *o++ = '0';
        *o++ = '.';
        for (int32_t i = -1; i > exponent; i--) *o++ = '0';
        memcpy(o, digits, n);
        o[n] = '\0';
    } else {
        size_t whole = (size_t) exponent + 1;
        for (size_t i = 0; i < whole; i++) *o++ = i < n ? digits[i] : '0';
        *o++ = '.';
        if (n > whole) {
            memcpy(o, digits + whole, n - whole);
            o += n - whole;
        } else {
            *o++ = '0';
        }
        *o = '\0';
    }
}

static int32_t check_conversion_source(CSOUND *csound, OPDS *perf_h, const CSN_ARRAY *src, bool is_to_str) {
    if (!is_to_str || src->itype == CSN_REAL) return OK;
    return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] csnnumtostr takes a real array: write a complex one as two, through csnreal and csnimag");
}

static int32_t string_conversion_assign_value(CSOUND *csound, CSN_STR_WRITER *w, CSN_ARRAY *out_arr, const CSN_ARRAY *source_arr, bool is_to_str) {
    char str[CSN_NUMBER_STRING_BYTES];
    for (size_t i = 0; i < source_arr->size; i++) {
        if (!is_to_str) {
            out_arr->data[i] = string_to_number(csound, slot_string(&source_arr->sdata[i]));
            continue;
        }
        number_to_string(csound, source_arr->data[i], str);
        int32_t res = csn_str_write(w, &out_arr->sdata[i], str, GET_STRINGDAT_LEN(str));
        if (res != OK) return res;
    }

    return OK;
}

static int32_t string_conversion_helper(CSOUND *csound, CSN_UNARYOP *p, bool is_to_str, bool is_k_init) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = check_conversion_source(csound, NULL, p->source_handle, is_to_str);
    if (res != OK) return res;

    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle }, 2);

    const CSN_ARRAY *ops[1] = { p->source_handle };
    ITEM_TYPE itype = is_to_str ? CSN_STRING : CSN_REAL;
    CSNARR_RES r = prepare_output(csound, &p->h, NULL, reg, p->handle, p->source_handle->ndim, p->source_handle->shape, itype, ops, 1, CSN_ALIAS_REFUSE);
    res = r.err;
    if (res != OK) goto done;
    pres.locked |= r.locked;
    p->k_data.registry = reg;

    /* prepare_output sizes string slots from string operands only: a k pass
       on a real-time path writes numbers into them, so they take room for
       the longest one */
    if (is_to_str && is_k_init && p->handle->rt_locked && csn_str_reserve_slots(csound, p->handle, CSN_NUMBER_STRING_BYTES) != OK) {
        res = csn_init_error(csound, "[csnarray] Out of memory: reserving the string slots of '%s' failed", get_out_name(&p->h));
        goto done;
    }

    /* every argument is known at init, so the k init computes too: a result
       published empty would be reused, never recomputed, while the source
       stays */
    CSN_STR_WRITER w = csn_str_writer(csound, NULL, p->handle);
    res = string_conversion_assign_value(csound, &w, p->handle, p->source_handle, is_to_str);
    if (res != OK) goto done;
    if (is_k_init) PUBLISH_DERIVED_RESULT(&p->k_data, p->source_handle, p->handle);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

static int32_t string_conversion_k_helper(CSOUND *csound, CSN_UNARYOP *p, bool is_to_str) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    CHECK_KTRIG(p->trig);

    int32_t res = check_conversion_source(csound, &p->h, p->source_handle, is_to_str);
    if (res != OK) return res;

    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle }, 2);

    if (CAN_REUSE_LAST_RESULT(&p->k_data, p->source_handle, p->handle)) goto done;

    ITEM_TYPE itype = is_to_str ? CSN_STRING : CSN_REAL;
    const CSN_ARRAY *ops[1] = { p->source_handle };
    CSNARR_RES r = prepare_output(csound, &p->h, &p->h, reg, p->handle, p->source_handle->ndim, p->source_handle->shape, itype, ops, 1, CSN_ALIAS_REFUSE);
    res = r.err;
    if (res != OK) goto done;
    pres.locked |= r.locked;

    CSN_STR_WRITER w = csn_str_writer(csound, &p->h, p->handle);
    res = string_conversion_assign_value(csound, &w, p->handle, p->source_handle, is_to_str);
    if (res != OK) goto done;
    PUBLISH_DERIVED_RESULT(&p->k_data, p->source_handle, p->handle);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}


int32_t csnstrings_strtonum(CSOUND *csound, CSN_UNARYOP *p) {
    return string_conversion_helper(csound, p, false, false);
}

int32_t csnstrings_strtonum_k_init(CSOUND *csound, CSN_UNARYOP *p) {
    return string_conversion_helper(csound, p, false, true);
}

int32_t csnstrings_strtonum_k(CSOUND *csound, CSN_UNARYOP *p) {
    return string_conversion_k_helper(csound, p, false);
}

int32_t csnstrings_numtostr(CSOUND *csound, CSN_UNARYOP *p) {
    return string_conversion_helper(csound, p, true, false);
}

int32_t csnstrings_numtostr_k_init(CSOUND *csound, CSN_UNARYOP *p) {
    return string_conversion_helper(csound, p, true, true);
}

int32_t csnstrings_numtostr_k(CSOUND *csound, CSN_UNARYOP *p) {
    return string_conversion_k_helper(csound, p, true);
}


/* ---------- join ---------- */

static int32_t check_join_value(CSOUND *csound, OPDS *perf_h, const STRINGDAT *sep) {
    if (IS_VALID_STRINGDAT(sep)) return OK;
    return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] Invalid/NULL separator");
}

/* Joins every string of the array, in row-major order whatever its rank, with
   sep between two of them, into buffer, as Python's sep.join: no string
   gives "", one gives itself. The buffer grows at init and on a pass off a
   real-time path; on one, a joined string longer than the buffer the init
   pass sized is refused. The S output took its size from the same string at
   init and never shrinks, so a string that fits the buffer fits it too, and
   the S type's copy allocates nothing. */
static int32_t join_strings(CSOUND *csound, OPDS *perf_h, const CSN_ARRAY *source_arr, CSN_STRDATA *buffer, const char *sep, bool rt_locked) {
    size_t sep_len = strlen(sep);
    size_t total = 0;
    for (size_t i = 0; i < source_arr->size; i++) {
        total += strlen(slot_string(&source_arr->sdata[i]));
        if (i > 0) total += sep_len;
    }
    int32_t res = ensure_string_capacity(csound, perf_h, buffer, total + 1, rt_locked);
    if (res != OK) return res;

    char *o = buffer->data;
    for (size_t i = 0; i < source_arr->size; i++) {
        if (i > 0) {
            memcpy(o, sep, sep_len);
            o += sep_len;
        }
        const char *item = slot_string(&source_arr->sdata[i]);
        size_t item_len = strlen(item);
        memcpy(o, item, item_len);
        o += item_len;
    }
    *o = '\0';
    return OK;
}

static int32_t string_join_helper(CSOUND *csound, CSN_REDUCTION_STRING_S *p, bool is_k_init) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    if (is_k_init) {
        csn_kdata_release_strings(csound, &p->k_data);
        csn_str_release(csound, &p->buffer);
    }

    int32_t res = check_join_value(csound, NULL, p->value);
    if (res != OK) return res;

    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);
    p->k_data.registry = reg;

    /* the i form joins through a buffer of its own; the k form keeps its
       buffer, sized here, for the passes that follow */
    CSN_STRDATA local = {0};
    CSN_STRDATA *buffer = is_k_init ? &p->buffer : &local;
    res = join_strings(csound, NULL, p->source_handle, buffer, p->value->data, false);
    if (res == OK) COPY_STRING_TO_STRINGDAT(csound, &p->h, p->string_out, buffer->data);
    if (res == OK && is_k_init) {
        csn_kdata_keep_strings(csound, &p->k_data, p->value->data, NULL, true);
        PUBLISH_STRING_ELEMENTWISE(csound, &p->k_data, p->source_handle, NULL, NULL, p->value->data, NULL, true);
    }
    csn_str_release(csound, &local);

    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

static int32_t string_join_k_helper(CSOUND *csound, CSN_REDUCTION_STRING_S *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    CHECK_KTRIG(p->trig);

    int32_t res = check_join_value(csound, &p->h, p->value);
    if (res != OK) return res;

    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);

    if (CAN_REUSE_STRING_ELEMENTWISE(&p->k_data, p->source_handle, NULL, NULL, p->value->data, NULL)) goto done;

    bool rt_locked = p->source_handle->rt_locked;
    res = join_strings(csound, &p->h, p->source_handle, &p->buffer, p->value->data, rt_locked);
    if (res != OK) goto done;
    COPY_STRING_TO_STRINGDAT(csound, &p->h, p->string_out, p->buffer.data);
    PUBLISH_STRING_ELEMENTWISE(csound, &p->k_data, p->source_handle, NULL, NULL, p->value->data, NULL, !rt_locked);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnstrings_reduction_s_deinit(CSOUND *csound, CSN_REDUCTION_STRING_S *p) {
    csn_str_release(csound, &p->buffer);
    csn_kdata_release_strings(csound, &p->k_data);
    return OK;
}

int32_t csnstrings_strjoin(CSOUND *csound, CSN_REDUCTION_STRING_S *p) {
    return string_join_helper(csound, p, false);
}

int32_t csnstrings_strjoin_k_init(CSOUND *csound, CSN_REDUCTION_STRING_S *p) {
    return string_join_helper(csound, p, true);
}

int32_t csnstrings_strjoin_k(CSOUND *csound, CSN_REDUCTION_STRING_S *p) {
    return string_join_k_helper(csound, p);
}

/* ---------- split, rsplit, partition, rpartition ---------- */

/* Python's str.split, str.rsplit, str.partition and str.rpartition on one S
   string. A cut is a non-overlapping occurrence of the separator, found from
   the left (split, partition) or from the right (rsplit, rpartition), so
   "aaa" splits on "aa" into ["", "a"] and rsplits into ["a", ""]. An empty
   separator is an error, as in Python. */
typedef enum { CSN_STRSPLIT, CSN_STRRSPLIT, CSN_STRPARTITION, CSN_STRRPARTITION } CSN_REDUCTION_H_MODE;

static bool is_partition_mode(CSN_REDUCTION_H_MODE mode) {
    return mode == CSN_STRPARTITION || mode == CSN_STRRPARTITION;
}

static bool is_right_mode(CSN_REDUCTION_H_MODE mode) {
    return mode == CSN_STRRSPLIT || mode == CSN_STRRPARTITION;
}

/* The last occurrence of sep that ends at or before s + end. */
static const char *find_last_occurrence(const char *s, size_t end, const char *sep, size_t sep_len) {
    if (end < sep_len) return NULL;
    for (size_t pos = end - sep_len + 1; pos-- > 0;) {
        if (memcmp(s + pos, sep, sep_len) == 0) return s + pos;
    }
    return NULL;
}

/* The cuts a split makes: every occurrence, or maxsplit at most when it is
   not negative. */
static size_t count_cuts(const char *s, const char *sep, int32_t maxsplit, bool from_right) {
    size_t sep_len = strlen(sep);
    size_t limit = maxsplit < 0 ? SIZE_MAX : (size_t) maxsplit;
    size_t cuts = 0;
    if (from_right) {
        size_t end = strlen(s);
        const char *hit;
        while (cuts < limit && (hit = find_last_occurrence(s, end, sep, sep_len)) != NULL) {
            end = (size_t) (hit - s);
            cuts++;
        }
    } else {
        const char *from = s;
        const char *hit;
        while (cuts < limit && (hit = strstr(from, sep)) != NULL) {
            from = hit + sep_len;
            cuts++;
        }
    }
    return cuts;
}

/* Writes the len bytes at value into the slot as a string of their own:
   csn_str_write takes a whole string, a piece ends inside its source. */
static int32_t write_piece(CSN_STR_WRITER *w, CSN_STRDATA *slot, const char *value, size_t len) {
    int32_t res = ensure_string_capacity(w->csound, w->perf_h, slot, len + 1, w->no_growth);
    if (res != OK) return w->err = res;
    memmove(slot->data, value, len);
    slot->data[len] = '\0';
    return OK;
}

/* The pieces a mode gives: one more than the cuts for a split, three for a
   partition. */
static size_t count_pieces(const char *s, const char *sep, int32_t maxsplit, CSN_REDUCTION_H_MODE mode) {
    if (is_partition_mode(mode)) return 3;
    return count_cuts(s, sep, maxsplit, is_right_mode(mode)) + 1;
}

/* Fills out_arr, already shaped by count_pieces with the same arguments. */
static int32_t split_string(CSN_STR_WRITER *w, CSN_ARRAY *out_arr, const char *s, const char *sep, CSN_REDUCTION_H_MODE mode) {
    CSN_STRDATA *out = out_arr->sdata;
    size_t sep_len = strlen(sep);
    size_t len = strlen(s);
    int32_t res = OK;
    if (is_partition_mode(mode)) {
        const char *hit = mode == CSN_STRPARTITION ? strstr(s, sep) : find_last_occurrence(s, len, sep, sep_len);
        if (hit == NULL) {
            size_t whole = mode == CSN_STRPARTITION ? 0 : 2;
            for (size_t i = 0; i < 3 && res == OK; i++) {
                res = write_piece(w, &out[i], s, i == whole ? len : 0);
            }
            return res;
        }
        size_t head = (size_t) (hit - s);
        res = write_piece(w, &out[0], s, head);
        if (res == OK) res = write_piece(w, &out[1], sep, sep_len);
        if (res == OK) res = write_piece(w, &out[2], hit + sep_len, len - head - sep_len);
        return res;
    }

    size_t cuts = out_arr->size - 1;
    if (mode == CSN_STRSPLIT) {
        const char *from = s;
        for (size_t i = 0; i < cuts; i++) {
            const char *hit = strstr(from, sep);
            res = write_piece(w, &out[i], from, (size_t) (hit - from));
            if (res != OK) return res;
            from = hit + sep_len;
        }
        return write_piece(w, &out[cuts], from, len - (size_t) (from - s));
    }

    size_t end = len;
    for (size_t i = cuts; i > 0; i--) {
        const char *hit = find_last_occurrence(s, end, sep, sep_len);
        size_t start = (size_t) (hit - s) + sep_len;
        res = write_piece(w, &out[i], s + start, end - start);
        if (res != OK) return res;
        end = (size_t) (hit - s);
    }
    return write_piece(w, &out[0], s, end);
}

static int32_t check_split_strings(CSOUND *csound, OPDS *perf_h, const STRINGDAT *string_in, const STRINGDAT *sep) {
    if (!IS_VALID_STRINGDAT(string_in) || !IS_VALID_STRINGDAT(sep)) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] Invalid/NULL string");
    }
    if (sep->data[0] == '\0') {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] Empty separator");
    }
    return OK;
}

/* A split derives from its two strings alone: a pass reuses the result while
   both match the copies k_data kept and nothing rewrote the output. */
static bool can_reuse_split(const K_DATA *k_data, const CSN_ARRAY *out_arr, const char *s, const char *sep) {
    if (k_data->prev_strings_stale || out_arr->sdata == NULL) return false;
    if (k_data->prev_string.data == NULL || strcmp(k_data->prev_string.data, s) != 0) return false;
    if (k_data->prev_string_b.data == NULL || strcmp(k_data->prev_string_b.data, sep) != 0) return false;
    return is_same_array_version(&out_arr->version, &k_data->prev_output_version);
}

static void publish_split(CSOUND *csound, K_DATA *k_data, const CSN_ARRAY *out_arr, const char *s, const char *sep, bool may_grow) {
    csn_kdata_keep_strings(csound, k_data, s, sep, may_grow);
    set_array_version(&k_data->prev_output_version, &out_arr->version);
}

static int32_t string_reduction_h_helper(CSOUND *csound, CSN_REDUCTION_STRING_H *p, CSN_REDUCTION_H_MODE mode, bool is_k_init) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    if (is_k_init) csn_kdata_release_strings(csound, &p->k_data);

    int32_t res = check_split_strings(csound, NULL, p->string_in, p->value);
    if (res != OK) return res;

    int32_t maxsplit = 0;
    if (!is_partition_mode(mode)) {
        if (!IS_VALID_VALUE_INT32((double) *p->arg_a)) {
            return csn_init_error(csound, "[csnarray] maxsplit must be a whole number (negative for no limit)");
        }
        maxsplit = (int32_t) *p->arg_a;
    }

    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle }, 1);
    p->k_data.registry = reg;

    const char *s = p->string_in->data;
    const char *sep = p->value->data;
    uint32_t shape[CSN_MAX_DIMS] = { (uint32_t) count_pieces(s, sep, maxsplit, mode) };
    CSNARR_RES r = prepare_output(csound, &p->h, NULL, reg, p->handle, 1U, shape, CSN_STRING, NULL, 0, CSN_ALIAS_REFUSE);
    res = r.err;
    if (res != OK) goto done;
    pres.locked |= r.locked;

    CSN_STR_WRITER w = csn_str_writer(csound, NULL, p->handle);
    res = split_string(&w, p->handle, s, sep, mode);
    if (res == OK && is_k_init) publish_split(csound, &p->k_data, p->handle, s, sep, true);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

static int32_t string_reduction_h_k_helper(CSOUND *csound, CSN_REDUCTION_STRING_H *p, CSN_REDUCTION_H_MODE mode) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);
    CHECK_KTRIG(is_partition_mode(mode) ? p->arg_a : p->arg_b);

    int32_t res = check_split_strings(csound, &p->h, p->string_in, p->value);
    if (res != OK) return res;

    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle }, 1);

    const char *s = p->string_in->data;
    const char *sep = p->value->data;
    if (can_reuse_split(&p->k_data, p->handle, s, sep)) goto done;

    int32_t maxsplit = is_partition_mode(mode) ? 0 : (int32_t) *p->arg_a;
    uint32_t shape[CSN_MAX_DIMS] = { (uint32_t) count_pieces(s, sep, maxsplit, mode) };
    CSNARR_RES r = prepare_output(csound, &p->h, &p->h, reg, p->handle, 1U, shape, CSN_STRING, NULL, 0, CSN_ALIAS_REFUSE);
    res = r.err;
    if (res != OK) goto done;
    pres.locked |= r.locked;

    CSN_STR_WRITER w = csn_str_writer(csound, &p->h, p->handle);
    res = split_string(&w, p->handle, s, sep, mode);
    if (res == OK) publish_split(csound, &p->k_data, p->handle, s, sep, !p->handle->rt_locked);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnstrings_reduction_h_deinit(CSOUND *csound, CSN_REDUCTION_STRING_H *p) {
    csn_kdata_release_strings(csound, &p->k_data);
    return OK;
}

int32_t csnstrings_strsplit(CSOUND *csound, CSN_REDUCTION_STRING_H *p) {
    return string_reduction_h_helper(csound, p, CSN_STRSPLIT, false);
}

int32_t csnstrings_strsplit_k_init(CSOUND *csound, CSN_REDUCTION_STRING_H *p) {
    return string_reduction_h_helper(csound, p, CSN_STRSPLIT, true);
}

int32_t csnstrings_strsplit_k(CSOUND *csound, CSN_REDUCTION_STRING_H *p) {
    return string_reduction_h_k_helper(csound, p, CSN_STRSPLIT);
}

int32_t csnstrings_strrsplit(CSOUND *csound, CSN_REDUCTION_STRING_H *p) {
    return string_reduction_h_helper(csound, p, CSN_STRRSPLIT, false);
}

int32_t csnstrings_strrsplit_k_init(CSOUND *csound, CSN_REDUCTION_STRING_H *p) {
    return string_reduction_h_helper(csound, p, CSN_STRRSPLIT, true);
}

int32_t csnstrings_strrsplit_k(CSOUND *csound, CSN_REDUCTION_STRING_H *p) {
    return string_reduction_h_k_helper(csound, p, CSN_STRRSPLIT);
}

int32_t csnstrings_strpartition(CSOUND *csound, CSN_REDUCTION_STRING_H *p) {
    return string_reduction_h_helper(csound, p, CSN_STRPARTITION, false);
}

int32_t csnstrings_strpartition_k_init(CSOUND *csound, CSN_REDUCTION_STRING_H *p) {
    return string_reduction_h_helper(csound, p, CSN_STRPARTITION, true);
}

int32_t csnstrings_strpartition_k(CSOUND *csound, CSN_REDUCTION_STRING_H *p) {
    return string_reduction_h_k_helper(csound, p, CSN_STRPARTITION);
}

int32_t csnstrings_strrpartition(CSOUND *csound, CSN_REDUCTION_STRING_H *p) {
    return string_reduction_h_helper(csound, p, CSN_STRRPARTITION, false);
}

int32_t csnstrings_strrpartition_k_init(CSOUND *csound, CSN_REDUCTION_STRING_H *p) {
    return string_reduction_h_helper(csound, p, CSN_STRRPARTITION, true);
}

int32_t csnstrings_strrpartition_k(CSOUND *csound, CSN_REDUCTION_STRING_H *p) {
    return string_reduction_h_k_helper(csound, p, CSN_STRRPARTITION);
}

// STRING - STRING

/* One S through the per-string kernel of the array form. The source is
   copied into the buffer the opcode keeps, the kernel works there, and the
   result goes to the S output through its copyValue, so the output may be
   the source (Sx = csnstrreplace(Sx, ...)). There is no in-place form: an S
   argument is an input, a literal among them, and writing it would change
   every use of that literal. Like strcpyk, an S output grows at perf as it
   needs: there is no slot to reserve. room is the most the kernel holds at
   once, terminator included. */
static int32_t load_s_buffer(CSOUND *csound, OPDS *perf_h, CSN_STRDATA *buffer, const char *s, size_t room) {
    int32_t res = ensure_string_capacity(csound, perf_h, buffer, room, false);
    if (res != OK) return res;
    memcpy(buffer->data, s, strlen(s) + 1);
    return OK;
}

/* Python's str.replace: the buffer holds the longer of the source and the
   result, the room replace_in_buffer works in. */
static int32_t replace_ss_helper(CSOUND *csound, CSN_REPLACE_S *p, bool is_perf) {
    OPDS *perf_h = is_perf ? &p->h : NULL;
    if (!IS_VALID_STRINGDAT(p->string_in) || !IS_VALID_STRINGDAT(p->old) || !IS_VALID_STRINGDAT(p->new)) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] Invalid/NULL string");
    }
    /* the count is i-rate: the init pass judges it once */
    if (!is_perf && check_replace_count(csound, (double) *p->limit) != OK) return NOTOK;

    const char *s = p->string_in->data;
    const char *old = p->old->data;
    const char *new = p->new->data;
    size_t len_source = strlen(s);
    size_t len_old = strlen(old);
    size_t len_new = strlen(new);
    size_t n = replace_occurrences(s, old, len_old, replace_limit((double) *p->limit));
    size_t len_out = len_source - n * len_old + n * len_new;

    int32_t res = load_s_buffer(csound, perf_h, &p->buffer, s, (len_out > len_source ? len_out : len_source) + 1);
    if (res != OK) return res;
    replace_in_buffer(p->buffer.data, old, len_old, new, len_new, n);
    COPY_STRING_TO_STRINGDAT(csound, &p->h, p->string_out, p->buffer.data);
    return OK;
}

/* Python's str.strip, lstrip and rstrip: chars is a set of bytes, NULL for
   ASCII whitespace, as in the array forms. A strip never grows the string. */
static int32_t strip_ss_helper(CSOUND *csound, OPDS *h, CSN_STRDATA *buffer, STRINGDAT *out, const STRINGDAT *in, const STRINGDAT *chars, CSN_STRIP_MODE mode, bool is_perf) {
    OPDS *perf_h = is_perf ? h : NULL;
    if (!IS_VALID_STRINGDAT(in)) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] Invalid/NULL string");
    }
    if (chars != NULL && !IS_VALID_STRINGDAT(chars)) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] Invalid/NULL strip chars");
    }
    int32_t res = load_s_buffer(csound, perf_h, buffer, in->data, strlen(in->data) + 1);
    if (res != OK) return res;
    strip_mode(buffer->data, (int32_t) mode, chars != NULL ? chars->data : NULL);
    COPY_STRING_TO_STRINGDAT(csound, h, out, buffer->data);
    return OK;
}

int32_t csnstrings_replace_ss_deinit(CSOUND *csound, CSN_REPLACE_S *p) {
    csn_str_release(csound, &p->buffer);
    return OK;
}

int32_t csnstrings_replace_ss(CSOUND *csound, CSN_REPLACE_S *p) {
    return replace_ss_helper(csound, p, false);
}

int32_t csnstrings_replace_ss_k(CSOUND *csound, CSN_REPLACE_S *p) {
    CHECK_KTRIG(p->trig);
    return replace_ss_helper(csound, p, true);
}

int32_t csnstrings_strip_ss_deinit(CSOUND *csound, CSN_STRIP_S *p) {
    csn_str_release(csound, &p->buffer);
    return OK;
}

int32_t csnstrings_strip_ss_ws_deinit(CSOUND *csound, CSN_STRIP_WS_S *p) {
    csn_str_release(csound, &p->buffer);
    return OK;
}

#define STRIP_SS_WRAPPERS(name, mode)                                                                          \
    int32_t csnstrings_##name##_ss(CSOUND *csound, CSN_STRIP_S *p) {                                           \
        return strip_ss_helper(csound, &p->h, &p->buffer, p->string_out, p->string_in, p->value, mode, false); \
    }                                                                                                          \
    int32_t csnstrings_##name##_ss_k(CSOUND *csound, CSN_STRIP_S *p) {                                         \
        CHECK_KTRIG(p->trig);                                                                                  \
        return strip_ss_helper(csound, &p->h, &p->buffer, p->string_out, p->string_in, p->value, mode, true);  \
    }                                                                                                          \
    int32_t csnstrings_##name##_ss_ws(CSOUND *csound, CSN_STRIP_WS_S *p) {                                     \
        return strip_ss_helper(csound, &p->h, &p->buffer, p->string_out, p->string_in, NULL, mode, false);     \
    }                                                                                                          \
    int32_t csnstrings_##name##_ss_ws_k(CSOUND *csound, CSN_STRIP_WS_S *p) {                                   \
        CHECK_KTRIG(p->trig);                                                                                  \
        return strip_ss_helper(csound, &p->h, &p->buffer, p->string_out, p->string_in, NULL, mode, true);      \
    }

STRIP_SS_WRAPPERS(strip, CSN_STRIP)
STRIP_SS_WRAPPERS(lstrip, CSN_LSTRIP)
STRIP_SS_WRAPPERS(rstrip, CSN_RSTRIP)

/* a + b, or a repeated count times, on one S: csnstradd and csnstrmul, and
   the operators S + S, S += S, S * n and S *= n. The result is built in the
   buffer the opcode keeps and goes to out through its copyValue, so out may be
   a or b (Sx += Sx). A count is a whole number, and one of 0 or less gives "",
   as in Python. */
int32_t string_addmul_ss_helper(CSOUND *csound, OPDS *h, bool is_perf, CSN_STRDATA *buffer, STRINGDAT *out, const STRINGDAT *a, const STRINGDAT *b, const cs_float *count, bool is_add) {
    OPDS *perf_h = is_perf ? h : NULL;
    if (!IS_VALID_STRINGDAT(a) || (is_add && !IS_VALID_STRINGDAT(b))) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] Invalid/NULL string");
    }

    size_t len_a = strlen(a->data);
    size_t len_b = 0;
    size_t reps = 0;
    size_t len = 0;
    if (is_add) {
        len_b = strlen(b->data);
        len = len_a + len_b;
    } else {
        double n = (double) *count;
        int32_t res = check_repeat_count(csound, perf_h, n);
        if (res != OK) return res;
        /* past 2^63 any non-empty string overflows below; the cap keeps the
           conversion defined */
        reps = n > 0.0 && len_a > 0 ? (size_t) fmin(n, 0x1p63) : 0;
        if (reps > 0 && len_a > (SIZE_MAX - 1) / reps) {
            return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] Repeating a %zu-byte string %g times overflows the string size", len_a, n);
        }
        len = len_a * reps;
    }

    int32_t res = ensure_string_capacity(csound, perf_h, buffer, len + 1, false);
    if (res != OK) return res;
    if (is_add) {
        memcpy(buffer->data, a->data, len_a);
        memcpy(buffer->data + len_a, b->data, len_b);
    } else {
        /* the first copy goes in, then each step doubles what is there */
        if (len > 0) memcpy(buffer->data, a->data, len_a);
        for (size_t done = len_a; done < len;) {
            size_t chunk = done < len - done ? done : len - done;
            memcpy(buffer->data + done, buffer->data, chunk);
            done += chunk;
        }
    }
    buffer->data[len] = '\0';

    COPY_STRING_TO_STRINGDAT(csound, h, out, buffer->data);
    return OK;
}

/* The operators on S: an S has no rate, so S1 + S2, Sx += Sy, S * n and
   Sx *= n compute at init and, at perf, again only when an operand changed
   since, written by strcpyk, sprintfk or another perf-time writer; the rule
   the CsnStr operators follow for an S operand. The copies are taken after
   the write, so an output that is also an operand (Sx += "b") matches them
   and is left alone, as Sx = Sx + "b" would be by a plain =. A k count gives
   the operator a rate: every_pass then runs it on every pass, as kx *= 2. */
static bool same_kept(const CSN_STRDATA *kept, const STRINGDAT *value) {
    if (value == NULL) return kept->data == NULL;
    return IS_VALID_STRINGDAT(value) && kept->data != NULL && strcmp(kept->data, value->data) == 0;
}

int32_t string_addmul_ss_op(CSOUND *csound, OPDS *h, bool is_perf, bool every_pass, K_DATA *k_data, CSN_STRDATA *buffer, STRINGDAT *out, const STRINGDAT *a, const STRINGDAT *b, const cs_float *count, bool is_add) {
    if (!is_perf) {
        csn_kdata_release_strings(csound, k_data);
    } else if (!every_pass && !k_data->prev_strings_stale && same_kept(&k_data->prev_string, a) && same_kept(&k_data->prev_string_b, b)) {
        return OK;
    }
    int32_t res = string_addmul_ss_helper(csound, h, is_perf, buffer, out, a, b, count, is_add);
    if (res == OK && !every_pass) csn_kdata_keep_strings(csound, k_data, a->data, b != NULL ? b->data : NULL, true);
    return res;
}

int32_t csnstrings_mul_ss_deinit(CSOUND *csound, CSN_STRMUL_S *p) {
    csn_str_release(csound, &p->buffer);
    return OK;
}

int32_t csnstrings_mul_ss(CSOUND *csound, CSN_STRMUL_S *p) {
    return string_addmul_ss_helper(csound, &p->h, false, &p->buffer, p->string_out, p->string_in, NULL, p->count, false);
}

int32_t csnstrings_mul_ss_k(CSOUND *csound, CSN_STRMUL_S *p) {
    CHECK_KTRIG(p->trig);
    return string_addmul_ss_helper(csound, &p->h, true, &p->buffer, p->string_out, p->string_in, NULL, p->count, false);
}

int32_t csnstrings_add_ss_deinit(CSOUND *csound, CSN_STRADD_S *p) {
    csn_str_release(csound, &p->buffer);
    return OK;
}

int32_t csnstrings_add_ss(CSOUND *csound, CSN_STRADD_S *p) {
    return string_addmul_ss_helper(csound, &p->h, false, &p->buffer, p->string_out, p->a, p->b, NULL, true);
}

int32_t csnstrings_add_ss_k(CSOUND *csound, CSN_STRADD_S *p) {
    CHECK_KTRIG(p->trig);
    return string_addmul_ss_helper(csound, &p->h, true, &p->buffer, p->string_out, p->a, p->b, NULL, true);
}

/* The queries on one S give a number, through the predicates and searches
   of the array forms: ASCII predicates, an empty string giving 0, the length
   in bytes, and the searches over s[start:end] with end -1 for the end of the
   string, clamped to it. An S has no rate, so the k forms take a trigger and
   compute at init too, a k range only when its values are known there. */
typedef enum { CSN_SQ_ISALPHA, CSN_SQ_ISDIGIT, CSN_SQ_ISALNUM, CSN_SQ_ISSPACE, CSN_SQ_LEN } CSN_STRING_QUERY_FS_MODE;

static int32_t string_query_fs(CSOUND *csound, CSN_STRING_QUERY_FS *p, bool is_perf, CSN_STRING_QUERY_FS_MODE mode) {
    OPDS *perf_h = is_perf ? &p->h : NULL;
    if (!IS_VALID_STRINGDAT(p->string_in)) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] Invalid/NULL string");
    }
    const char *s = p->string_in->data;
    double ans = 0.0;
    switch (mode) {
        case CSN_SQ_ISALPHA: ans = all_bytes(s, ascii_is_letter); break;
        case CSN_SQ_ISDIGIT: ans = all_bytes(s, ascii_is_digit); break;
        case CSN_SQ_ISALNUM: ans = all_bytes(s, ascii_is_alnum); break;
        case CSN_SQ_ISSPACE: ans = all_bytes(s, ascii_is_space); break;
        case CSN_SQ_LEN: ans = (double) strlen(s); break;
    }
    *p->value = (cs_float) ans;
    return OK;
}

#define QUERY_FS_WRAPPERS(name, mode)                                         \
    int32_t csnstrings_##name##_ss(CSOUND *csound, CSN_STRING_QUERY_FS *p) {   \
        return string_query_fs(csound, p, false, mode);                       \
    }                                                                         \
    int32_t csnstrings_##name##_ss_k(CSOUND *csound, CSN_STRING_QUERY_FS *p) { \
        CHECK_KTRIG(p->trig);                                                 \
        return string_query_fs(csound, p, true, mode);                        \
    }

QUERY_FS_WRAPPERS(isalpha, CSN_SQ_ISALPHA)
QUERY_FS_WRAPPERS(isdigit, CSN_SQ_ISDIGIT)
QUERY_FS_WRAPPERS(isalnum, CSN_SQ_ISALNUM)
QUERY_FS_WRAPPERS(isspace, CSN_SQ_ISSPACE)
QUERY_FS_WRAPPERS(strlen, CSN_SQ_LEN)

static int32_t string_query_sf(CSOUND *csound, CSN_STRING_QUERY_SF *p, bool is_perf, CSN_STR_QUERY fn) {
    OPDS *perf_h = is_perf ? &p->h : NULL;
    if (!IS_VALID_STRINGDAT(p->string_in) || !IS_VALID_STRINGDAT(p->prefix)) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] Invalid/NULL string");
    }
    double start = (double) *p->start;
    double end = (double) *p->end;
    int32_t res = check_query_range(csound, perf_h, start, end);
    if (res != OK) return res;

    const char *s = p->string_in->data;
    size_t len = strlen(s);
    size_t stop = end == -1.0 || end > (double) len ? len : (size_t) end;
    *p->index = (cs_float) fn(s, (size_t) start, stop, p->prefix->data);
    return OK;
}

#define QUERY_SF_WRAPPERS(name, fn)                                                                    \
    int32_t csnstrings_##name##_ss(CSOUND *csound, CSN_STRING_QUERY_SF *p) {                            \
        return string_query_sf(csound, p, false, fn);                                                  \
    }                                                                                                  \
    int32_t csnstrings_##name##_ss_k_init(CSOUND *csound, CSN_STRING_QUERY_SF *p) {                     \
        if (!csn_known_at_init(csound, p->start) || !csn_known_at_init(csound, p->end)) return OK;      \
        return string_query_sf(csound, p, false, fn);                                                  \
    }                                                                                                  \
    int32_t csnstrings_##name##_ss_k(CSOUND *csound, CSN_STRING_QUERY_SF *p) {                          \
        CHECK_KTRIG(p->trig);                                                                          \
        return string_query_sf(csound, p, true, fn);                                                   \
    }

QUERY_SF_WRAPPERS(startswith, query_startswith)
QUERY_SF_WRAPPERS(endswith, query_endswith)
QUERY_SF_WRAPPERS(find, query_find)
QUERY_SF_WRAPPERS(rfind, query_rfind)

/* csnstrsubstr on one S: the bytes from start up to stop, every step-th,
   by the rules of the array form, written into the buffer the opcode keeps. */
static int32_t string_substr_ss(CSOUND *csound, CSN_SUBSTRING_S *p, bool is_perf) {
    OPDS *perf_h = is_perf ? &p->h : NULL;
    if (!IS_VALID_STRINGDAT(p->string_in)) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] Invalid/NULL string");
    }
    double start = (double) *p->start;
    double stop = (double) *p->stop;
    double step = (double) *p->step;
    int32_t res = check_substr_range(csound, perf_h, start, stop, step);
    if (res != OK) return res;

    CSN_SUBSTR_RANGE range = { (uint32_t) start, (uint32_t) stop, (uint32_t) step };
    const char *s = p->string_in->data;
    size_t sub_len = size_from_range(&range, strlen(s));
    res = ensure_string_capacity(csound, perf_h, &p->buffer, sub_len + 1, false);
    if (res != OK) return res;
    size_t begin = range.start < strlen(s) ? range.start : strlen(s);
    for (size_t j = 0; j < sub_len; j++) p->buffer.data[j] = s[begin + j * range.step];
    p->buffer.data[sub_len] = '\0';
    COPY_STRING_TO_STRINGDAT(csound, &p->h, p->string_out, p->buffer.data);
    return OK;
}

int32_t csnstrings_substr_ss_deinit(CSOUND *csound, CSN_SUBSTRING_S *p) {
    csn_str_release(csound, &p->buffer);
    return OK;
}

int32_t csnstrings_substr_ss(CSOUND *csound, CSN_SUBSTRING_S *p) {
    return string_substr_ss(csound, p, false);
}

int32_t csnstrings_substr_ss_k_init(CSOUND *csound, CSN_SUBSTRING_S *p) {
    if (!csn_known_at_init(csound, p->start) || !csn_known_at_init(csound, p->stop) || !csn_known_at_init(csound, p->step)) return OK;
    return string_substr_ss(csound, p, false);
}

int32_t csnstrings_substr_ss_k(CSOUND *csound, CSN_SUBSTRING_S *p) {
    CHECK_KTRIG(p->trig);
    return string_substr_ss(csound, p, true);
}

/* The case maps of the array forms on one S, ASCII only. Swapping case undoes
   itself, so Sx = csnstrswapcase(Sx, kTrig) swaps again on every pass that
   runs, as the array form does. */
static int32_t string_case_ss(CSOUND *csound, CSN_STRIP_WS_S *p, CSN_CASE mode, bool is_perf) {
    OPDS *perf_h = is_perf ? &p->h : NULL;
    if (!IS_VALID_STRINGDAT(p->string_in)) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] Invalid/NULL string");
    }
    int32_t res = load_s_buffer(csound, perf_h, &p->buffer, p->string_in->data, strlen(p->string_in->data) + 1);
    if (res != OK) return res;
    map_case(p->buffer.data, (int32_t) mode, NULL);
    COPY_STRING_TO_STRINGDAT(csound, &p->h, p->string_out, p->buffer.data);
    return OK;
}

#define CASE_SS_WRAPPERS(name, mode)                                                \
    int32_t csnstrings_##name##_ss(CSOUND *csound, CSN_STRIP_WS_S *p) {             \
        return string_case_ss(csound, p, mode, false);                              \
    }                                                                               \
    int32_t csnstrings_##name##_ss_k_init(CSOUND *csound, CSN_STRIP_WS_S *p) {      \
        return string_case_ss(csound, p, mode, false);                              \
    }                                                                               \
    int32_t csnstrings_##name##_ss_k(CSOUND *csound, CSN_STRIP_WS_S *p) {           \
        CHECK_KTRIG(p->trig);                                                       \
        return string_case_ss(csound, p, mode, true);                               \
    }

CASE_SS_WRAPPERS(upper, CSN_CASE_UPPER)
CASE_SS_WRAPPERS(lower, CSN_CASE_LOWER)
CASE_SS_WRAPPERS(capitalize, CSN_CASE_CAPITALIZE)
CASE_SS_WRAPPERS(swapcase, CSN_CASE_SWAP)
CASE_SS_WRAPPERS(title, CSN_CASE_TITLE)

/* chr and ord. csnstrchr takes an ASCII code, 1 to 127 (0 would end the
   string at once); csnstrord gives the byte of a one-byte string, 0 to 255. */
static bool is_ascii_code(double value) {
    return isfinite(value) && trunc(value) == value && value >= 1.0 && value <= 127.0;
}

static int32_t string_chr_ss(CSOUND *csound, CSN_STRINGCHR *p, bool is_perf) {
    OPDS *perf_h = is_perf ? &p->h : NULL;
    double value = (double) *p->value;
    if (!is_ascii_code(value)) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] csnstrchr takes an ASCII code, a whole number from 1 to 127, got %g", value);
    }
    char dest[2] = { (char) value, '\0' };
    COPY_STRING_TO_STRINGDAT(csound, &p->h, p->string_out, dest);
    return OK;
}

static int32_t string_ord_ss(CSOUND *csound, CSN_STRINGORD *p, bool is_perf) {
    OPDS *perf_h = is_perf ? &p->h : NULL;
    if (!IS_VALID_STRINGDAT(p->string_in)) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] Invalid/NULL string");
    }
    size_t len = strlen(p->string_in->data);
    if (len != 1) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] csnstrord takes a string of one byte, got %zu bytes", len);
    }
    *p->value = (cs_float) (unsigned char) p->string_in->data[0];
    return OK;
}

int32_t csnstrings_chr(CSOUND *csound, CSN_STRINGCHR *p) {
    return string_chr_ss(csound, p, false);
}

int32_t csnstrings_chr_k_init(CSOUND *csound, CSN_STRINGCHR *p) {
    if (!csn_known_at_init(csound, p->value)) return OK;
    return string_chr_ss(csound, p, false);
}

int32_t csnstrings_chr_k(CSOUND *csound, CSN_STRINGCHR *p) {
    CHECK_KTRIG(p->trig);
    return string_chr_ss(csound, p, true);
}

int32_t csnstrings_ord(CSOUND *csound, CSN_STRINGORD *p) {
    return string_ord_ss(csound, p, false);
}

/* An S has no rate, but the one a k opcode writes may still be empty at init
   (sprintfk, a csnstrchr with a k code): a k init reads only a one-byte
   string, and leaves anything else to the first pass to judge. */
int32_t csnstrings_ord_k_init(CSOUND *csound, CSN_STRINGORD *p) {
    if (!IS_VALID_STRINGDAT(p->string_in) || strlen(p->string_in->data) != 1) return OK;
    return string_ord_ss(csound, p, false);
}

int32_t csnstrings_ord_k(CSOUND *csound, CSN_STRINGORD *p) {
    CHECK_KTRIG(p->trig);
    return string_ord_ss(csound, p, true);
}

/* One S and a real 1-D array, both ways: its raw bytes (csnstrtoascii), its
   UTF-8 bytes (csnstrencode, the S checked as UTF-8 first) or its code points
   (csnstrtocodepoints); and back from ASCII codes (csnstrfromascii), UTF-8
   bytes (csnstrdecode) or code points (csnstrfromcodepoints). UTF-8 is read
   as Python's strict decoder reads it: no overlong forms, no surrogates,
   nothing past U+10FFFF. A NUL would end the string at once, so no form
   writes one. */
typedef enum { CSN_SBYTES_RAW, CSN_SBYTES_UTF8, CSN_SBYTES_CODEPOINTS } CSN_SBYTES_MODE;

/* The length of the UTF-8 sequence at s, avail bytes at most, its code point
   in cp; 0 when the bytes there are not one. */
static size_t utf8_sequence(const unsigned char *s, size_t avail, uint32_t *cp) {
    unsigned char b0 = s[0];
    if (b0 < 0x80) {
        *cp = b0;
        return 1;
    }
    size_t n;
    uint32_t lo = 0x80, hi = 0xBF;
    if (b0 >= 0xC2 && b0 <= 0xDF) {
        n = 2;
        *cp = b0 & 0x1F;
    } else if (b0 >= 0xE0 && b0 <= 0xEF) {
        n = 3;
        *cp = b0 & 0x0F;
        if (b0 == 0xE0) lo = 0xA0;      /* no overlong form */
        if (b0 == 0xED) hi = 0x9F;      /* no surrogate */
    } else if (b0 >= 0xF0 && b0 <= 0xF4) {
        n = 4;
        *cp = b0 & 0x07;
        if (b0 == 0xF0) lo = 0x90;      /* no overlong form */
        if (b0 == 0xF4) hi = 0x8F;      /* nothing past U+10FFFF */
    } else {
        return 0;
    }
    if (avail < n) return 0;
    for (size_t k = 1; k < n; k++) {
        unsigned char b = s[k];
        if (b < (k == 1 ? lo : 0x80) || b > (k == 1 ? hi : 0xBF)) return 0;
        *cp = (*cp << 6) | (b & 0x3F);
    }
    return n;
}

static size_t utf8_length(uint32_t cp) {
    return cp < 0x80 ? 1 : cp < 0x800 ? 2 : cp < 0x10000 ? 3 : 4;
}

static size_t utf8_write(char *dest, uint32_t cp) {
    size_t n = utf8_length(cp);
    if (n == 1) {
        dest[0] = (char) cp;
    } else {
        static const unsigned char lead[] = { 0, 0, 0xC0, 0xE0, 0xF0 };
        for (size_t k = n - 1; k > 0; k--) {
            dest[k] = (char) (0x80 | (cp & 0x3F));
            cp >>= 6;
        }
        dest[0] = (char) (lead[n] | cp);
    }
    return n;
}

/* The code points of s, checked as UTF-8: their count, or the byte at which
   s stops being UTF-8 in bad (and SIZE_MAX returned). */
static size_t utf8_count(const char *s, size_t len, size_t *bad) {
    size_t count = 0;
    uint32_t cp;
    for (size_t i = 0; i < len; count++) {
        size_t n = utf8_sequence((const unsigned char *) s + i, len - i, &cp);
        if (n == 0) {
            *bad = i;
            return SIZE_MAX;
        }
        i += n;
    }
    return count;
}

static int32_t sbytes_fill(CSOUND *csound, OPDS *perf_h, CSN_REGISTRY *reg, CSN_TOASCII_SS *p, CSNARR_RES *pres, CSN_SBYTES_MODE mode, const char *name) {
    const char *s = p->string_in->data;
    size_t len = strlen(s);
    size_t out_len = len;
    if (mode != CSN_SBYTES_RAW) {
        size_t bad = 0;
        size_t count = utf8_count(s, len, &bad);
        if (count == SIZE_MAX) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] %s: the string is not UTF-8 at byte %zu (0x%02X)", name, bad, (unsigned) (unsigned char) s[bad]);
        }
        if (mode == CSN_SBYTES_CODEPOINTS) out_len = count;
    }
    if (out_len > UINT32_MAX) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] %s: a string of %zu bytes is longer than an array axis", name, len);
    }
    uint32_t shape[CSN_MAX_DIMS] = { (uint32_t) out_len };
    CSNARR_RES r = prepare_output(csound, &p->h, perf_h, reg, p->handle, 1U, shape, CSN_REAL, NULL, 0, CSN_ALIAS_REFUSE);
    pres->locked |= r.locked;
    if (r.err != OK) return r.err;
    if (mode == CSN_SBYTES_CODEPOINTS) {
        uint32_t cp;
        size_t i = 0;
        for (size_t j = 0; j < out_len; j++) {
            i += utf8_sequence((const unsigned char *) s + i, len - i, &cp);
            p->handle->data[j] = (double) cp;
        }
    } else {
        for (size_t i = 0; i < len; i++) p->handle->data[i] = (double) (unsigned char) s[i];
    }
    return OK;
}

static int32_t sbytes_helper(CSOUND *csound, CSN_TOASCII_SS *p, bool is_perf, CSN_SBYTES_MODE mode, const char *name) {
    OPDS *perf_h = is_perf ? &p->h : NULL;
    CSN_REGISTRY *reg = is_perf ? p->k_data.registry : get_registry(csound);
    CHECK_REGISTRY(csound, perf_h, reg);
    if (is_perf) CHECK_KTRIG(p->trig);
    if (!IS_VALID_STRINGDAT(p->string_in)) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] Invalid/NULL string");
    }
    if (!is_perf) {
        stamp_globals(csound, &p->h);
        p->k_data.registry = reg;
    }
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle }, 1);
    int32_t res = sbytes_fill(csound, perf_h, reg, p, &pres, mode, name);
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnstrings_toascii_deinit(CSOUND *csound, CSN_TOASCII_SS *p) {
    csn_kdata_release_strings(csound, &p->k_data);
    return OK;
}

#define SBYTES_WRAPPERS(name, mode, label)                                        \
    int32_t csnstrings_##name##_ss(CSOUND *csound, CSN_TOASCII_SS *p) {           \
        return sbytes_helper(csound, p, false, mode, label);                      \
    }                                                                             \
    int32_t csnstrings_##name##_ss_k_init(CSOUND *csound, CSN_TOASCII_SS *p) {    \
        return sbytes_helper(csound, p, false, mode, label);                      \
    }                                                                             \
    int32_t csnstrings_##name##_ss_k(CSOUND *csound, CSN_TOASCII_SS *p) {         \
        return sbytes_helper(csound, p, true, mode, label);                       \
    }

SBYTES_WRAPPERS(toascii, CSN_SBYTES_RAW, "csnstrtoascii")
SBYTES_WRAPPERS(encode, CSN_SBYTES_UTF8, "csnstrencode")
SBYTES_WRAPPERS(tocodepoints, CSN_SBYTES_CODEPOINTS, "csnstrtocodepoints")

/* The way back, from a real 1-D array: ASCII codes 1 to 127, UTF-8 bytes 1
   to 255 that make UTF-8, or code points 1 to U+10FFFF but the surrogates.
   An empty array gives "". A bad value is named with its index. */
static bool is_whole_in(double v, double lo, double hi) {
    return isfinite(v) && trunc(v) == v && v >= lo && v <= hi;
}

static int32_t sbytes_into(CSOUND *csound, OPDS *perf_h, const CSN_ARRAY *src, CSN_STRDATA *buffer, CSN_SBYTES_MODE mode, const char *name) {
    static const char *what[] = { "ASCII codes, whole numbers from 1 to 127", "UTF-8 bytes, whole numbers from 1 to 255", "code points, whole numbers from 1 to 1114111 but the surrogates 55296 to 57343" };
    if (src->itype != CSN_REAL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] %s takes a real array of %s", name, what[mode]);
    }
    if (src->ndim > 1U) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] %s takes a 1-D array, got %u dimensions", name, src->ndim);
    }
    size_t total = 0;
    for (size_t i = 0; i < src->size; i++) {
        double v = src->data[i];
        bool ok = mode == CSN_SBYTES_RAW ? is_ascii_code(v)
                : mode == CSN_SBYTES_UTF8 ? is_whole_in(v, 1.0, 255.0)
                : is_whole_in(v, 1.0, 1114111.0) && !(v >= 55296.0 && v <= 57343.0);
        if (!ok) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] %s takes %s, got %.15g at index %zu", name, what[mode], v, i);
        }
        total += mode == CSN_SBYTES_CODEPOINTS ? utf8_length((uint32_t) v) : 1;
    }
    int32_t res = ensure_string_capacity(csound, perf_h, buffer, total + 1, false);
    if (res != OK) return res;
    size_t o = 0;
    for (size_t i = 0; i < src->size; i++) {
        if (mode == CSN_SBYTES_CODEPOINTS) o += utf8_write(buffer->data + o, (uint32_t) src->data[i]);
        else buffer->data[o++] = (char) (unsigned char) src->data[i];
    }
    buffer->data[o] = '\0';
    if (mode == CSN_SBYTES_UTF8) {
        size_t bad = 0;
        if (utf8_count(buffer->data, o, &bad) == SIZE_MAX) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] %s: the bytes are not UTF-8 at index %zu (0x%02X)", name, bad, (unsigned) (unsigned char) buffer->data[bad]);
        }
    }
    return OK;
}

static int32_t sbytes_back_helper(CSOUND *csound, CSN_FROMASCII_SS *p, bool is_perf, bool is_k_init, CSN_SBYTES_MODE mode, const char *name) {
    OPDS *perf_h = is_perf ? &p->h : NULL;
    CSN_REGISTRY *reg = is_perf ? p->k_data.registry : get_registry(csound);
    CHECK_REGISTRY(csound, perf_h, reg);
    if (is_perf) CHECK_KTRIG(p->trig);
    if (!is_perf) {
        stamp_globals(csound, &p->h);
        p->k_data.registry = reg;
    }
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);
    int32_t res = OK;
    if (is_perf && CAN_REUSE_LAST_RESULT(&p->k_data, p->source_handle, NULL)) goto done;
    res = sbytes_into(csound, perf_h, p->source_handle, &p->buffer, mode, name);
    if (res != OK) goto done;
    COPY_STRING_TO_STRINGDAT(csound, &p->h, p->string_out, p->buffer.data);
    if (is_perf || is_k_init) PUBLISH_DERIVED_RESULT(&p->k_data, p->source_handle, NULL);
done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnstrings_fromascii_deinit(CSOUND *csound, CSN_FROMASCII_SS *p) {
    csn_str_release(csound, &p->buffer);
    return OK;
}

#define SBYTES_BACK_WRAPPERS(name, mode, label)                                     \
    int32_t csnstrings_##name##_ss(CSOUND *csound, CSN_FROMASCII_SS *p) {           \
        return sbytes_back_helper(csound, p, false, false, mode, label);            \
    }                                                                               \
    int32_t csnstrings_##name##_ss_k_init(CSOUND *csound, CSN_FROMASCII_SS *p) {    \
        return sbytes_back_helper(csound, p, false, true, mode, label);             \
    }                                                                               \
    int32_t csnstrings_##name##_ss_k(CSOUND *csound, CSN_FROMASCII_SS *p) {         \
        return sbytes_back_helper(csound, p, true, false, mode, label);             \
    }

SBYTES_BACK_WRAPPERS(fromascii, CSN_SBYTES_RAW, "csnstrfromascii")
SBYTES_BACK_WRAPPERS(decode, CSN_SBYTES_UTF8, "csnstrdecode")
SBYTES_BACK_WRAPPERS(fromcodepoints, CSN_SBYTES_CODEPOINTS, "csnstrfromcodepoints")
