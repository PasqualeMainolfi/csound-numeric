/* Opcode implementations for the index family.
   The public opcode inventory remains centralized in csnum.c. */
#include "csnstrings.h"
#include "csnum.h"
#include "csnum_internal.h"
#include "csnregistry.h"
#include <float.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int32_t get_index_offset(CSOUND *csound, OPDS *perf_h, size_t *offset, uint32_t ndim, const CSN_ARRAY *arr, const cs_float *indexes) {
    size_t temp_offset = 0;
    for (uint32_t i = 0; i < ndim; i++) {
        double index_value = (double) indexes[i];

        if (!IS_VALID_INDEX(index_value)) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Index %g at position %u is invalid; indexes must be finite non-negative integers", index_value, i);
        }

        uint32_t index = (uint32_t) index_value;
        if (index >= arr->shape[i]) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Index %u at position %u is out of range for extent %u (valid: 0..%u)", index, i, arr->shape[i], arr->shape[i] - 1);
        }

        temp_offset += arr->strides[i] * (size_t) index;
    }

    *offset = temp_offset;
    return OK;
}

CSN_INDEXES indexes_of_arraydat(const ARRAYDAT *indexes) {
    if (indexes == NULL || indexes->data == NULL || indexes->sizes == NULL || indexes->dimensions != 1 || indexes->sizes[0] < 0) {
        return (CSN_INDEXES) { NULL, 0 };
    }
    return (CSN_INDEXES) { indexes->data, (size_t) indexes->sizes[0] };
}

/* v[i][j] passes each index as its own argument; Csound writes a pointer for
   each one given, and nothing past them. */
int32_t gather_indexes(CSOUND *csound, OPDS *perf_h, cs_float *const *args, size_t count, cs_float *buffer, CSN_INDEXES *out) {
    if (count == 0 || count > CSN_MAX_DIMS) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] Expected one index per dimension (at most %d), got %zu", CSN_MAX_DIMS, count);
    }
    for (size_t i = 0; i < count; i++) buffer[i] = *args[i];
    *out = (CSN_INDEXES) { buffer, count };
    return OK;
}

int32_t csnarray_resolve_item(CSOUND *csound, OPDS *perf_h, CSN_ARRAY *source_arr, CSN_INDEXES indexes, ITEM_TYPE expected_itype, size_t *out_offset) {
    uint32_t ndim = source_arr->ndim;

    if (source_arr->itype != expected_itype) {
        if (expected_itype == CSN_COMPLEX) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Handle holds a real array; use the real form of the accessor");
        }
        if (expected_itype == CSN_STRING) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Handle holds a string array; use the string form of the accessor");
        }
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Handle holds a complex array; declare the value as :Complex; to read or write it (read an item with csnget)");
    }

    if (indexes.data == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Index argument must be a 1-D array of exactly %u elements, one per dimension", ndim);
    }
    if (indexes.count != (size_t) ndim) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Expected %u indexes, one per dimension, got %zu", ndim, indexes.count);
    }

    int32_t res = get_index_offset(csound, perf_h, out_offset, ndim, source_arr, indexes.data);
    if (res != OK) return res;

    if (*out_offset >= source_arr->size) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Index resolves to offset %zu, outside the logical array size %zu", *out_offset, source_arr->size);
    }

    return OK;
}

static int32_t csnarray_get_set_locked(CSOUND *csound, OPDS *perf_h, CSN_ARRAY *source_arr, CSN_INDEXES indexes, cs_float *value, bool is_get) {
    if (value == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Internal error: null value pointer passed to the element accessor");
    }

    size_t offset = 0;
    int32_t res = csnarray_resolve_item(csound, perf_h, source_arr, indexes, CSN_REAL, &offset);
    if (res != OK) return res;

    if (is_get) {
        *value = (cs_float) source_arr->data[offset];
    } else {
        /* Compared first, because a k-rate csnset that stores the same value
           every pass must not look like a write. Feeding an array to an
           in-place opcode that is not idempotent (csnflip.in, csnroll.in) the
           array would otherwise be flipped back and forth forever. One
           comparison, not a scan: this accessor only ever touches one item. */
        double stored = (double) *value;
        if (source_arr->data[offset] != stored) {
            source_arr->data[offset] = stored;
            update_array_data_version(&source_arr->version);
        }
    }

    return OK;
}

static int32_t csnarray_get_set_complex_locked(CSOUND *csound, OPDS *perf_h, CSN_ARRAY *source_arr, CSN_INDEXES indexes, COMPLEXDAT *value, bool is_get) {
    if (value == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Internal error: null value pointer passed to the element accessor");
    }

    size_t offset = 0;
    int32_t res = csnarray_resolve_item(csound, perf_h, source_arr, indexes, CSN_COMPLEX, &offset);
    if (res != OK) return res;

    size_t at = offset * 2;
    if (is_get) {
        value->real = (cs_float) source_arr->data[at];
        value->imag = (cs_float) source_arr->data[at + 1];
        value->isPolar = 0;
    } else {
        double re, im;
        complexdat_to_rect(value, &re, &im);
        if (source_arr->data[at] != re || source_arr->data[at + 1] != im) {
            source_arr->data[at] = re;
            source_arr->data[at + 1] = im;
            update_array_data_version(&source_arr->version);
        }
    }

    return OK;
}

int32_t csnarray_get(CSOUND *csound, CSN_GET *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    p->registry = reg;

    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);

    int32_t res = csnarray_get_set_locked(csound, NULL, p->source_handle, indexes_of_arraydat(p->indexes), p->value, true);

    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_get_k(CSOUND *csound, CSN_GET *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);

    int32_t res = csnarray_get_set_locked(csound, &p->h, p->source_handle, indexes_of_arraydat(p->indexes), p->value, true);

    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_get_complex(CSOUND *csound, CSN_GETCOMPLEX *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    p->registry = reg;

    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);

    int32_t res = csnarray_get_set_complex_locked(csound, NULL, p->source_handle, indexes_of_arraydat(p->indexes), p->value, true);

    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_get_complex_k(CSOUND *csound, CSN_GETCOMPLEX *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);

    int32_t res = csnarray_get_set_complex_locked(csound, &p->h, p->source_handle, indexes_of_arraydat(p->indexes), p->value, true);

    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_set(CSOUND *csound, CSN_SET *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    if (csn_is_k_form(&p->h)) csn_mark_krate(p->source_handle);
    p->registry = reg;

    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);

    int32_t res = csnarray_get_set_locked(csound, NULL, p->source_handle, indexes_of_arraydat(p->indexes), p->value, false);

    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_set_k(CSOUND *csound, CSN_SET *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);

    int32_t res = csnarray_get_set_locked(csound, &p->h, p->source_handle, indexes_of_arraydat(p->indexes), p->value, false);

    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_set_complex(CSOUND *csound, CSN_SETCOMPLEX *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    if (csn_is_k_form(&p->h)) csn_mark_krate(p->source_handle);
    p->registry = reg;

    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);

    int32_t res = csnarray_get_set_complex_locked(csound, NULL, p->source_handle, indexes_of_arraydat(p->indexes), p->value, false);

    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_set_complex_k(CSOUND *csound, CSN_SETCOMPLEX *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);

    int32_t res = csnarray_get_set_complex_locked(csound, &p->h, p->source_handle, indexes_of_arraydat(p->indexes), p->value, false);

    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

/* v[i][j]: Csound compiles indexing to ##array_get / ##array_set /
   ##array_init, one index per dimension. A k form runs at perf only, as
   kvar = x does: its init just binds the registry, and a k write marks the
   array k-rate so whatever reads it follows it at perf. */
int32_t csnidx_bind(CSOUND *csound, OPDS *h, CSN_REGISTRY **registry) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    *registry = reg;
    stamp_globals(csound, h);
    return OK;
}

static int32_t csnidx_get_run(CSOUND *csound, OPDS *perf_h, CSN_IDX_GET *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, perf_h, reg);

    cs_float buffer[CSN_MAX_DIMS];
    CSN_INDEXES indexes;
    int32_t res = gather_indexes(csound, perf_h, p->idx, (size_t) p->INOCOUNT - 1, buffer, &indexes);
    if (res != OK) return res;

    bool locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);
    res = csnarray_get_set_locked(csound, perf_h, p->source_handle, indexes, p->value, true);
    if (locked) csn_unlock(csound, reg);
    return res;
}

static int32_t csnidx_set_run(CSOUND *csound, OPDS *perf_h, CSN_IDX_SET *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, perf_h, reg);

    // the array, the value, then the indexes
    cs_float buffer[CSN_MAX_DIMS];
    CSN_INDEXES indexes;
    int32_t res = gather_indexes(csound, perf_h, p->idx, (size_t) p->INOCOUNT - 2, buffer, &indexes);
    if (res != OK) return res;

    bool locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);
    res = csnarray_get_set_locked(csound, perf_h, p->source_handle, indexes, p->value, false);
    if (locked) csn_unlock(csound, reg);
    return res;
}

static int32_t csnidx_setc_run(CSOUND *csound, OPDS *perf_h, CSN_IDX_SETC *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, perf_h, reg);

    // the array, the value, then the indexes
    cs_float buffer[CSN_MAX_DIMS];
    CSN_INDEXES indexes;
    int32_t res = gather_indexes(csound, perf_h, p->idx, (size_t) p->INOCOUNT - 2, buffer, &indexes);
    if (res != OK) return res;

    bool locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);
    res = csnarray_get_set_complex_locked(csound, perf_h, p->source_handle, indexes, p->value, false);
    if (locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnidx_get(CSOUND *csound, CSN_IDX_GET *p) {
    int32_t res = csnidx_bind(csound, &p->h, &p->registry);
    return res != OK ? res : csnidx_get_run(csound, NULL, p);
}

int32_t csnidx_get_k_init(CSOUND *csound, CSN_IDX_GET *p) {
    return csnidx_bind(csound, &p->h, &p->registry);
}

int32_t csnidx_get_k(CSOUND *csound, CSN_IDX_GET *p) {
    return csnidx_get_run(csound, &p->h, p);
}

/* "=" with i arguments, and init whatever the rate of its arguments: one
   write, at init time. */
int32_t csnidx_set(CSOUND *csound, CSN_IDX_SET *p) {
    int32_t res = csnidx_bind(csound, &p->h, &p->registry);
    return res != OK ? res : csnidx_set_run(csound, NULL, p);
}

int32_t csnidx_set_k_init(CSOUND *csound, CSN_IDX_SET *p) {
    csn_mark_krate(p->source_handle);
    return csnidx_bind(csound, &p->h, &p->registry);
}

int32_t csnidx_set_k(CSOUND *csound, CSN_IDX_SET *p) {
    return csnidx_set_run(csound, &p->h, p);
}

int32_t csnidx_setc(CSOUND *csound, CSN_IDX_SETC *p) {
    int32_t res = csnidx_bind(csound, &p->h, &p->registry);
    return res != OK ? res : csnidx_setc_run(csound, NULL, p);
}

int32_t csnidx_setc_k_init(CSOUND *csound, CSN_IDX_SETC *p) {
    csn_mark_krate(p->source_handle);
    return csnidx_bind(csound, &p->h, &p->registry);
}

int32_t csnidx_setc_k(CSOUND *csound, CSN_IDX_SETC *p) {
    return csnidx_setc_run(csound, &p->h, p);
}

static int32_t csnarray_get_rowcol_helper(CSOUND *csound, CSN_GET_ROWCOL *p, bool is_row) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle }, 2);

    CSN_ARRAY *source_arr = p->source_handle;
    uint32_t source_dim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;
    size_t *source_strides = source_arr->strides;

    if (source_dim != 2U) {
        res = csn_init_error(csound, "[csnarray] csngetrow/col requires 2D array");
        goto done;
    }

    if (!IS_VALID_INDEX((double) *p->index)) {
        res = csn_init_error(csound, "[csnarray] Not valid index");
        goto done;
    }

    uint32_t n_items = is_row ? source_shape[0] : source_shape[1];
    uint32_t index = (uint32_t) *p->index;
    if (index >= n_items) {
        res = csn_init_error(csound, "[csnarray] Index out of bounds");
        goto done;
    }

    uint32_t new_dim = 1U;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    new_shape[0] = is_row ? source_shape[1] : source_shape[0];
    ITEM_TYPE itype = source_arr->itype;

    const CSN_ARRAY *ops[1] = { source_arr };
    CSNARR_RES out = prepare_output(csound, &p->h, NULL, reg, p->handle, new_dim, new_shape, itype, ops, 1, CSN_ALIAS_REFUSE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    CSN_ARRAY *arr = p->handle;
    for (size_t i = 0; i < p->handle->size; i++) {
        size_t r_index = index * source_strides[0] + i * source_strides[1];
        size_t c_index = i * source_strides[0] + index * source_strides[1];
        size_t ndx = is_row ? r_index : c_index;
        if (itype == CSN_REAL) {
            arr->data[i] = source_arr->data[ndx];
        } else {
            arr->data[i * 2] = source_arr->data[ndx * 2];
            arr->data[i * 2 + 1] = source_arr->data[ndx * 2 + 1];
        }
    }

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_get_row(CSOUND *csound, CSN_GET_ROWCOL *p) {
    return csnarray_get_rowcol_helper(csound, p, true);
}

int32_t csnarray_get_col(CSOUND *csound, CSN_GET_ROWCOL *p) {
    return csnarray_get_rowcol_helper(csound, p, false);
}

static int32_t csnarray_get_rowcol_k_init_helper(CSOUND *csound, CSN_GET_ROWCOL *p, bool is_row) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle }, 2);

    CSN_ARRAY *source_arr = p->source_handle;
    uint32_t source_dim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;
    if (source_dim != 2U) {
        res = csn_init_error(csound, "[csnarray] csngetrow/col requires 2D array");
        goto done;
    }

    uint32_t shape[CSN_MAX_DIMS] = {0};
    shape[0] = is_row ? source_shape[1] : source_shape[0];

    const CSN_ARRAY *ops[1] = { source_arr };
    CSNARR_RES out = prepare_output(csound, &p->h, NULL, reg, p->handle, 1U, shape, source_arr->itype, ops, 1, CSN_ALIAS_REFUSE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    reset_empty_csnarray(p->handle, 1U, shape, source_arr->itype);

    SET_KDATA_BEGIN(p->k_data, reg, shape, 1U, p->handle->itype);
    set_array_version(&p->k_data.prev_output_version, &p->handle->version);
    set_array_version(&p->k_data.prev_source_version, &source_arr->version);
    p->is_published = false;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

static int32_t csnarray_get_rowcol_k_helper(CSOUND *csound, CSN_GET_ROWCOL *p, bool is_row) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    int32_t res = OK;
    CHECK_KTRIG(p->trig);

    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle }, 2);

    CSN_ARRAY *source_arr = p->source_handle;
    uint32_t source_dim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;
    size_t *source_strides = source_arr->strides;
    if (source_dim != 2U) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] get row/col requires 2D array");
        goto done;
    }

    if (!IS_VALID_INDEX((double) *p->index)) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Not valid index");
        goto done;
    }

    uint32_t n_items = is_row ? source_shape[0] : source_shape[1];
    uint32_t index = (uint32_t) *p->index;
    if (index >= n_items) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Index out of bounds");
        goto done;
    }

    if (p->is_published) {
        bool is_same_source = is_same_array_version(&p->k_data.prev_source_version, &source_arr->version);
        bool is_same_index = index == p->k_data.prev_index;
        bool is_same_result = is_same_array_version(&p->k_data.prev_output_version, &p->handle->version);
        if (is_same_index && is_same_source && is_same_result) goto done;
    }

    uint32_t new_dim = 1U;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    new_shape[0] = is_row ? source_shape[1] : source_shape[0];
    ITEM_TYPE itype = source_arr->itype;

    const CSN_ARRAY *ops[1] = { source_arr };
    CSNARR_RES out = prepare_output(csound, &p->h, &p->h, reg, p->handle, new_dim, new_shape, itype, ops, 1, CSN_ALIAS_REFUSE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    CSN_ARRAY *arr = p->handle;
    for (size_t i = 0; i < arr->size; i++) {
        size_t r_index = index * source_strides[0] + i * source_strides[1];
        size_t c_index = i * source_strides[0] + index * source_strides[1];
        size_t ndx = is_row ? r_index : c_index;
        if (itype == CSN_REAL) {
            arr->data[i] = source_arr->data[ndx];
        } else {
            arr->data[i * 2] = source_arr->data[ndx * 2];
            arr->data[i * 2 + 1] = source_arr->data[ndx * 2 + 1];
        }
    }

    SET_KDATA_END(p->k_data, new_shape, 1U, itype);
    set_array_version(&p->k_data.prev_source_version, &source_arr->version);
    set_array_version(&p->k_data.prev_output_version, &p->handle->version);
    p->k_data.prev_index = index;
    p->is_published = true;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_get_row_k_init(CSOUND *csound, CSN_GET_ROWCOL *p) {
    return csnarray_get_rowcol_k_init_helper(csound, p, true);
}

int32_t csnarray_get_col_k_init(CSOUND *csound, CSN_GET_ROWCOL *p) {
    return csnarray_get_rowcol_k_init_helper(csound, p, false);
}

int32_t csnarray_get_row_k(CSOUND *csound, CSN_GET_ROWCOL *p) {
    return csnarray_get_rowcol_k_helper(csound, p, true);
}

int32_t csnarray_get_col_k(CSOUND *csound, CSN_GET_ROWCOL *p) {
    return csnarray_get_rowcol_k_helper(csound, p, false);
}

int32_t check_take_flat_body(CSOUND *csound, OPDS *perf_h, CSN_ARRAY *arr, double index, bool is_complex) {
    if (!is_complex) {
        if (arr->itype == CSN_COMPLEX) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Handle holds a complex array; declare the taken value as :Complex;");
        }
    } else {
        if (arr->itype != CSN_COMPLEX) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Handle holds a real array; declare the taken value as i or k");
        }
    }
    if (!IS_VALID_INDEX(index)) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Index %g is invalid; indexes must be finite non-negative integers", index);
    }

    uint32_t valid_index = (uint32_t) index;

    if (valid_index >= arr->size) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Index %u is out of range", valid_index);
    }

    return OK;
}

static int32_t check_take_body(CSOUND *csound, OPDS *perf_h, CSN_ARRAY *arr, uint32_t *shape, uint32_t *out_ndim, uint32_t *out_axis, uint32_t *out_index, double index, double in_axis) {
    uint32_t ndim = arr->ndim;

    /* Dropping the only axis would leave a rank-0 array, which the registry
       cannot represent. That case is the two-argument form, which yields a
       plain scalar. */
    if (ndim < 2) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Take along an axis needs a 2-D or higher array; use the two-argument form for a scalar");
    }

    CSN_AXIS_SPEC axis_spec = csn_normalize_axis_value(in_axis, ndim);
    if (axis_spec.kind != CSN_AXIS_INDEX) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: finite integers %d..%u)", in_axis, ndim, -(int32_t) ndim, ndim - 1);
    }
    *out_axis = axis_spec.index;
    uint32_t axis = *out_axis;

    if (!IS_VALID_INDEX(index)) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Index %g is invalid; indexes must be finite non-negative integers", index);
    }
    uint32_t valid_index = (uint32_t) index;

    if (valid_index >= arr->shape[axis]) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Index %u is out of range", valid_index);
    }
    *out_index = valid_index;

    // remove axis passed for new shape
    *out_ndim = ndim - 1;
    for (uint32_t i = 0, j = 0; i < ndim; i++) {
        if (i == axis) continue;
        shape[j++] = arr->shape[i];
    }
    return OK;
}

static int32_t take_assign_value(CSOUND *csound, CSN_ARRAY *source, CSN_ARRAY *destination, uint32_t in_ndim, uint32_t out_ndim, uint32_t axis, uint32_t index) {
    CSN_BROADCAST_ITER it;
    if (ND_ITER_INIT(&it, out_ndim, destination->shape, destination->size, NULL) != OK) return NOTOK;
    while (BROADCAST_ITER_NEXT(&it)) {
        size_t linear = it.linear_index;
        size_t src_index = (size_t) index * source->strides[axis];
        for (uint32_t i = 0, k = 0; i < in_ndim; ++i) {
            if (i != axis) src_index += (size_t) it.coords[k++] * source->strides[i];
        }
        switch (source->itype) {
            case CSN_REAL:
            case CSN_COMPLEX:
                if (source->itype == CSN_COMPLEX) {
                    destination->data[linear * 2] = source->data[src_index * 2];
                    destination->data[linear * 2 + 1] = source->data[src_index * 2 + 1];
                } else {
                    destination->data[linear] = source->data[src_index];
                }
                break;
            case CSN_STRING:
                if (destination->sdata[linear] != NULL) csound->Free(csound, destination->sdata[linear]);
                destination->sdata[linear] = csound->Strdup(csound, source->sdata[src_index]);
                break;
        }
    }

    return OK;
}

int32_t csnarray_take(CSOUND *csound, CSN_TAKE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle , p->handle }, 2);

    CSN_ARRAY *arr = p->source_handle;
    uint32_t out_ndim = 0;
    uint32_t axis = 0;
    uint32_t index = 0;
    uint32_t shape[CSN_MAX_DIMS] = {0};
    res = check_take_body(csound, NULL, arr, shape, &out_ndim, &axis, &index, (double) *p->index, (double) *p->axis);
    if (res != OK) goto done;

    const CSN_ARRAY *ops[1] = { arr };
    CSNARR_RES out = prepare_output(csound, &p->h, NULL, reg, p->handle, out_ndim, shape, arr->itype, ops, 1, CSN_ALIAS_REFUSE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    /* Walk the destination, which is smaller than the source by exactly the
       extent of the dropped axis. */
    if (take_assign_value(csound, arr, p->handle, arr->ndim, out_ndim, axis, index) != OK) {
        res = csn_init_error(csound, "[csnarray] Invalid take iterator layout");
        goto done;
    }

    SET_KDATA_BEGIN(p->k_data, reg, shape, out_ndim, p->handle->itype);
    p->k_data.prev_axis_u = axis;
    p->k_data.prev_index = index;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_take_k(CSOUND *csound, CSN_TAKE *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    /* a pass that takes strings copies them */
    if (p->source_handle->itype == CSN_STRING) {
        int32_t res_rt = check_strings_not_rt(csound, &p->h, p->handle);
        if (res_rt != OK) return res_rt;
    }

    int32_t res = OK;
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle, p->handle }, 2);

    CSN_ARRAY *arr = p->source_handle;
    uint32_t out_ndim = 0;
    uint32_t axis = 0;
    uint32_t index = 0;
    uint32_t shape[CSN_MAX_DIMS] = {0};
    res = check_take_body(csound, &p->h, arr, shape, &out_ndim, &axis, &index, (double) *p->index, (double) *p->axis);
    if (res != OK) goto done;

    uint32_t ndim = arr->ndim;
    ITEM_TYPE itype = arr->itype;

    /* The whole source version, not only its data: a source reshaped in place
       holds the same data and still gives another result. */
    if (p->k_data.prev_axis_u == axis && p->k_data.prev_index == index
        && CAN_REUSE_ELEMENTWISE(&p->k_data, arr, NULL, p->handle, 0.0, 0.0)) goto done;

    const CSN_ARRAY *ops[1] = { arr };
    CSNARR_RES out = prepare_output(csound, &p->h, &p->h, reg, p->handle, out_ndim, shape, arr->itype, ops, 1, CSN_ALIAS_REFUSE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    if (take_assign_value(csound, arr, p->handle, ndim, out_ndim, axis, index) != OK) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Invalid take iterator layout");
        goto done;
    }

    SET_KDATA_END(p->k_data, shape, out_ndim, itype);
    p->k_data.prev_axis_u = axis;
    p->k_data.prev_index = index;
    PUBLISH_ELEMENTWISE(&p->k_data, arr, NULL, p->handle, 0.0, 0.0);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

/* Two-argument form: indexes the flattened array and yields a scalar, matching
   np.take(a, i) with axis=None. This is the rank-1 case the axis form cannot
   express, since dropping the only axis would leave nothing behind. */
int32_t csnarray_take_flat(CSOUND *csound, CSN_TAKE_FLAT *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    p->registry = reg;

    int32_t res = OK;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);

    CSN_ARRAY *arr = p->source_handle;
    res = check_take_flat_body(csound, NULL, arr, (double) *p->index, false);
    if (res != OK) goto done;

    *p->value = (cs_float) arr->data[(size_t) *p->index];

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_take_flat_k(CSOUND *csound, CSN_TAKE_FLAT *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    int32_t res = OK;
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);

    CSN_ARRAY *arr = p->source_handle;
    res = check_take_flat_body(csound, &p->h, arr, (double) *p->index, false);
    if (res != OK) goto done;

    *p->value = (cs_float) arr->data[(size_t) *p->index];

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_takecomp_flat(CSOUND *csound, CSN_TAKECOMPLEX_FLAT *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    p->registry = reg;

    int32_t res = OK;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);

    CSN_ARRAY *arr = p->source_handle;
    res = check_take_flat_body(csound, NULL, arr, (double) *p->index, true);
    if (res != OK) goto done;

    size_t index = (size_t) *p->index;
    p->value->real = (cs_float) arr->data[index * 2];
    p->value->imag = (cs_float) arr->data[index * 2 + 1];
    p->value->isPolar = 0;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_takecomp_flat_k(CSOUND *csound, CSN_TAKECOMPLEX_FLAT *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    int32_t res = OK;
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);

    CSN_ARRAY *arr = p->source_handle;
    res = check_take_flat_body(csound, &p->h, arr, (double) *p->index, true);
    if (res != OK) goto done;

    size_t index = (size_t) *p->index;
    p->value->real = (cs_float) arr->data[index * 2];
    p->value->imag = (cs_float) arr->data[index * 2 + 1];
    p->value->isPolar = 0;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

static int32_t validate_slice_spec(CSOUND *csound, OPDS *perf_h, const CSN_ARRAY *array, double axis_value, double start_value, double stop_value, double step_value, uint32_t *out_axis, uint32_t *out_start, uint32_t *out_step, uint32_t *out_shape, size_t *out_size) {
    uint32_t ndim = array->ndim;
    CSN_AXIS_SPEC axis_spec = csn_normalize_axis_value(axis_value, ndim);
    if (axis_spec.kind != CSN_AXIS_INDEX) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: finite integers %d..%u)", axis_value, ndim, -(int32_t) ndim, ndim - 1);
    }

    if (!IS_VALID_INDEX(start_value) || !IS_VALID_INDEX(stop_value) || !IS_VALID_INDEX(step_value) || step_value == 0.0) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Invalid slice start=%g stop=%g step=%g: values must be finite integers, start and stop must be non-negative, and step must be > 0", start_value, stop_value, step_value);
    }

    uint32_t axis = axis_spec.index;
    uint32_t start = (uint32_t) start_value;
    uint32_t stop = (uint32_t) stop_value;
    uint32_t step = (uint32_t) step_value;
    uint32_t extent = array->shape[axis];
    if (start >= extent || stop > extent || stop <= start) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Invalid slice start=%u stop=%u step=%u on axis %u of extent %u: need 0 <= start < stop <= %u and step > 0", start, stop, step, axis, extent, extent);
    }

    uint32_t sliced_extent = 1U + (stop - start - 1U) / step;
    for (uint32_t i = 0; i < ndim; i++) {
        out_shape[i] = i == axis ? sliced_extent : array->shape[i];
    }

    if (get_array_size_from_shape(out_size, ndim, out_shape) != OK) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Slice shape is invalid or its element count exceeds the configured limit");
    }

    *out_axis = axis;
    *out_start = start;
    *out_step = step;
    return OK;
}

/* A string item is copied, never shared: source and slice each own their
   strings, and either may be rewritten or freed without the other. */
static int32_t slice_get_assign_value(CSOUND *csound, const CSN_ARRAY *source, CSN_ARRAY *destination, uint32_t ndim, uint32_t axis, uint32_t start, uint32_t step) {
    CSN_BROADCAST_ITER it;
    if (ND_ITER_INIT(&it, ndim, destination->shape, destination->size, NULL) != OK) return NOTOK;
    while (BROADCAST_ITER_NEXT(&it)) {
        size_t linear = it.linear_index;
        size_t source_index = 0;
        for (uint32_t d = 0; d < ndim; ++d) {
            size_t coord = d == axis ? (size_t) start + (size_t) it.coords[d] * step : it.coords[d];
            source_index += coord * source->strides[d];
        }
        if (source->itype == CSN_STRING) {
            const char *item = source->sdata[source_index];
            if (destination->sdata[linear] != NULL) csound->Free(csound, destination->sdata[linear]);
            destination->sdata[linear] = item != NULL ? csound->Strdup(csound, item) : NULL;
        } else if (source->itype == CSN_COMPLEX) {
            destination->data[linear * 2] = source->data[source_index * 2];
            destination->data[linear * 2 + 1] = source->data[source_index * 2 + 1];
        } else {
            destination->data[linear] = source->data[source_index];
        }
    }
    return OK;
}

/* A string slot that already holds the incoming string is left alone, as
   csnset leaves it: no write, no allocation. With refuse_alloc (a perf pass on
   a marked array) the first slot that would change stops the walk before
   anything is written, since the slots before it were all unchanged, and
   *refused says so. The copy is made before the old string is released: data
   may be the destination itself. */
static int32_t slice_set_assign_value(CSOUND *csound, const CSN_ARRAY *data, CSN_ARRAY *destination, uint32_t ndim, const uint32_t *slice_shape, uint32_t axis, uint32_t start, uint32_t step, bool refuse_alloc, bool *refused) {
    CSN_BROADCAST_ITER it;
    if (ND_ITER_INIT(&it, ndim, slice_shape, data->size, NULL) != OK) return NOTOK;
    while (BROADCAST_ITER_NEXT(&it)) {
        size_t linear = it.linear_index;
        size_t destination_index = 0;
        for (uint32_t d = 0; d < ndim; ++d) {
            size_t coord = d == axis ? (size_t) start + (size_t) it.coords[d] * step : it.coords[d];
            destination_index += coord * destination->strides[d];
        }

        switch (data->itype) {
            case CSN_REAL:
            case CSN_COMPLEX:
                if (data->itype == CSN_COMPLEX) {
                    destination->data[destination_index * 2] = data->data[linear * 2];
                    destination->data[destination_index * 2 + 1] = data->data[linear * 2 + 1];
                } else {
                    destination->data[destination_index] = data->data[linear];
                }
                break;
            case CSN_STRING: {
                const char *value = data->sdata[linear] != NULL ? data->sdata[linear] : "";
                char *stored = destination->sdata[destination_index];
                if (stored != NULL && strcmp(stored, value) == 0) break;
                if (refuse_alloc) {
                    *refused = true;
                    return OK;
                }
                destination->sdata[destination_index] = csound->Strdup(csound, value);
                if (stored != NULL) csound->Free(csound, stored);
                break;
            }
        }
    }

    return OK;
}

/* v[a:b] ends inclusive, and a negative end means the last element, as
   Csound's own slicearray: the exclusive stop validate_slice_spec takes. */
static int32_t native_slice_stop(CSOUND *csound, OPDS *perf_h, const CSN_ARRAY *arr, double axis, double start, double end, double *stop) {
    CSN_AXIS_SPEC ax = csn_normalize_axis_value(axis, arr->ndim);
    if (ax.kind != CSN_AXIS_INDEX) {
        *stop = end; // validate_slice_spec reports the axis
        return OK;
    }
    uint32_t extent = arr->shape[ax.index];
    if (end < 0) {
        *stop = (double) extent;
        return OK;
    }
    if (end >= (double) extent) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Slice end %g is past the last index %d of axis %u", end, (int32_t) extent - 1, ax.index);
    }
    if (end < start) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Slice end %g comes before its start %g; the end is included", end, start);
    }
    *stop = end + 1.0;
    return OK;
}

/* The slice behind csngetslice (exclusive stop) and v[a:b] (native: inclusive
   end). The k form slices here only with a spec known at init. A k argument
   reads 0 there, and it also fixes the slice's extents: the output keeps the
   source's layout reserved but empty (a slice keeps the rank), and the first
   performance pass slices. */
static int32_t slice_init(CSOUND *csound, OPDS *h, CSN_ARRAY *arr, CSN_ARRAY *out_arr, K_DATA *k_data, cs_float *p_axis, cs_float *p_start, cs_float *p_stop, cs_float *p_step, bool is_ktime, bool native) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    stamp_globals(csound, h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { arr, out_arr }, 2);

    uint32_t ndim = arr->ndim;
    uint32_t shape[CSN_MAX_DIMS] = {0};

    uint32_t axis = 0;
    uint32_t start = 0;
    uint32_t step = 0;
    size_t output_size = 0;
    memset(k_data, 0, sizeof(*k_data));
    k_data->registry = reg;
    if (is_ktime && !(csn_known_at_init(csound, p_axis) && csn_known_at_init(csound, p_start) && csn_known_at_init(csound, p_stop) && csn_known_at_init(csound, p_step))) {
        const CSN_ARRAY *ops[1] = { arr };
        CSNARR_RES out = prepare_output(csound, h, NULL, reg, out_arr, ndim, arr->shape, arr->itype, ops, 1, CSN_ALIAS_REFUSE);
        res = out.err;
        pres.locked |= out.locked;
        if (res == OK) reset_empty_csnarray(out_arr, ndim, arr->shape, arr->itype);
        goto done;
    }

    double in_axis = (double) *p_axis;
    double in_stop = (double) *p_stop;
    if (native && (res = native_slice_stop(csound, NULL, arr, in_axis, (double) *p_start, in_stop, &in_stop)) != OK) goto done;
    res = validate_slice_spec(csound, NULL, arr, in_axis, (double) *p_start, in_stop, (double) *p_step, &axis, &start, &step, shape, &output_size);
    if (res != OK) goto done;

    const CSN_ARRAY *ops[1] = { arr };
    CSNARR_RES out = prepare_output(csound, h, NULL, reg, out_arr, ndim, shape, arr->itype, ops, 1, CSN_ALIAS_REFUSE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    if (slice_get_assign_value(csound, arr, out_arr, ndim, axis, start, step) != OK) {
        res = csn_init_error(csound, "[csnarray] Invalid slice iterator layout");
        goto done;
    }

    SET_KDATA_BEGIN(*k_data, reg, shape, ndim, arr->itype);
    if (csn_is_k_form(h)) {
        PUBLISH_ELEMENTWISE(k_data, arr, NULL, out_arr, (double) start, (double) step);
        k_data->prev_axis_u = axis;
    }
    /* v[a:b] with i bounds has a perf pass all the same, to follow a source
       that changes at perf: prepare_output then makes its output k-rate, and
       it is k only when the source is, as for the operators. */
    if (!is_ktime && csn_is_k_form(h)) out_arr->rate = rate_of(is_krate(arr));

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

static int32_t slice_perf(CSOUND *csound, OPDS *h, CSN_ARRAY *arr, CSN_ARRAY *out_arr, K_DATA *k_data, const cs_float *p_axis, const cs_float *p_start, const cs_float *p_stop, const cs_float *p_step, bool native) {
    CSN_REGISTRY *reg = k_data->registry;
    CHECK_REGISTRY(csound, h, reg);

    int32_t res = OK;
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { arr, out_arr }, 2);

    uint32_t ndim = arr->ndim;
    uint32_t shape[CSN_MAX_DIMS] = {0};

    uint32_t axis = 0;
    uint32_t start = 0;
    uint32_t step = 0;
    size_t output_size = 0;
    double in_axis = (double) *p_axis;
    double in_stop = (double) *p_stop;
    if (native && (res = native_slice_stop(csound, h, arr, in_axis, (double) *p_start, in_stop, &in_stop)) != OK) goto done;
    res = validate_slice_spec(csound, h, arr, in_axis, (double) *p_start, in_stop, (double) *p_step, &axis, &start, &step, shape, &output_size);
    if (res != OK) goto done;

    /* stop is not compared on its own: with start and step fixed it only sets
       the sliced extent, which the recorded shape already holds, and two stops
       giving the same extent give the same slice */
    if (k_data->prev_axis_u == axis && k_data->prev_ndim == ndim
        && memcmp(k_data->prev_shape, shape, sizeof(uint32_t) * ndim) == 0
        && CAN_REUSE_ELEMENTWISE(k_data, arr, NULL, out_arr, (double) start, (double) step)) goto done;

    /* copying strings allocates, which a real-time path forbids */
    if (arr->itype == CSN_STRING && out_arr->rt_locked) {
        res = csn_locked_perf_error(csound, h, "[csnarray] Slicing a string array allocates at perf time and the slice is on a real-time path; slice it at i-rate, clear the mark with csnrtunlock, or pass irt=0 at the audio source it descends from");
        goto done;
    }

    const CSN_ARRAY *ops[1] = { arr };
    CSNARR_RES out = prepare_output(csound, h, h, reg, out_arr, ndim, shape, arr->itype, ops, 1, CSN_ALIAS_REFUSE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    if (slice_get_assign_value(csound, arr, out_arr, ndim, axis, start, step) != OK) {
        res = csn_locked_perf_error(csound, h, "[csnarray] Invalid slice iterator layout");
        goto done;
    }
    SET_KDATA_END(*k_data, shape, ndim, arr->itype);
    PUBLISH_ELEMENTWISE(k_data, arr, NULL, out_arr, (double) start, (double) step);
    k_data->prev_axis_u = axis;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_get_slice(CSOUND *csound, CSN_GET_SLICE *p) {
    return slice_init(csound, &p->h, p->source_handle, p->handle, &p->k_data, p->axis, p->start, p->stop, p->step, false, false);
}

int32_t csnarray_get_slice_k_init(CSOUND *csound, CSN_GET_SLICE *p) {
    return slice_init(csound, &p->h, p->source_handle, p->handle, &p->k_data, p->axis, p->start, p->stop, p->step, true, false);
}

int32_t csnarray_get_slice_k(CSOUND *csound, CSN_GET_SLICE *p) {
    return slice_perf(csound, &p->h, p->source_handle, p->handle, &p->k_data, p->axis, p->start, p->stop, p->step, false);
}

/* v[start:end, step, axis]: Csound compiles slice syntax to slicearray. */
int32_t csnslicearray_init(CSOUND *csound, CSN_SLICEARRAY *p) {
    return slice_init(csound, &p->h, p->source_handle, p->handle, &p->k_data, p->axis, p->start, p->stop, p->step, false, true);
}

int32_t csnslicearray_perf(CSOUND *csound, CSN_SLICEARRAY *p) {
    // The bounds are fixed: it slices again only while the source changes at perf
    if (!is_krate(p->source_handle)) return OK;
    return slice_perf(csound, &p->h, p->source_handle, p->handle, &p->k_data, p->axis, p->start, p->stop, p->step, true);
}

int32_t csnslicearray_k_init(CSOUND *csound, CSN_SLICEARRAY *p) {
    return slice_init(csound, &p->h, p->source_handle, p->handle, &p->k_data, p->axis, p->start, p->stop, p->step, true, true);
}

int32_t csnslicearray_k_perf(CSOUND *csound, CSN_SLICEARRAY *p) {
    return slice_perf(csound, &p->h, p->source_handle, p->handle, &p->k_data, p->axis, p->start, p->stop, p->step, true);
}

static int32_t csnarray_set_slice_locked(CSOUND *csound, OPDS *perf_h, CSN_SET_SLICE *p) {
    CSN_ARRAY *source_arr = p->source_handle;
    CSN_ARRAY *data_arr = p->data_handle;

    if (source_arr->itype != data_arr->itype) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Element type mismatch: one array is real and the other complex");
    }

    uint32_t source_ndim = source_arr->ndim;
    uint32_t data_ndim = data_arr->ndim;
    if (data_ndim != source_ndim) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Data block has %u dimensions but the source array has %u", data_ndim, source_ndim);
    }

    if (source_arr->size == 0) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Cannot assign a slice of an empty array");
    }

    uint32_t slice_shape[CSN_MAX_DIMS] = {0};
    uint32_t axis = 0;
    uint32_t start = 0;
    uint32_t step = 0;
    size_t slice_size = 0;
    int32_t res = validate_slice_spec(csound, perf_h, source_arr, (double) *p->axis, (double) *p->start, (double) *p->stop, (double) *p->step, &axis, &start, &step, slice_shape, &slice_size);
    if (res != OK) return res;

    for (uint32_t i = 0; i < source_ndim; i++) {
        if (data_arr->shape[i] != slice_shape[i]) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Data block extent %u on axis %u does not match the slice extent %u", data_arr->shape[i], i, slice_shape[i]);
        }
    }

    if (data_arr->size != slice_size) {
        char sbuf[CSN_SHAPE_STR_MAX];
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Data block holds %zu elements but the slice %s holds %zu", data_arr->size, shape_str(sbuf, sizeof(sbuf), slice_shape, source_ndim), slice_size);
    }

    bool refused = false;
    bool refuse_alloc = perf_h != NULL && source_arr->rt_locked && source_arr->itype == CSN_STRING;
    if (slice_set_assign_value(csound, data_arr, source_arr, source_ndim, slice_shape, axis, start, step, refuse_alloc, &refused) != OK) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Invalid slice iterator layout");
    }
    if (refused) {
        return csn_locked_perf_error(csound, perf_h, "[csnarray] Writing a string allocates at perf time and the array is on a real-time path; set it at i-rate, clear the mark with csnrtunlock, or pass irt=0 at the audio source it descends from");
    }

    update_array_data_version(&source_arr->version);
    return OK;
}

int32_t csnarray_set_slice(CSOUND *csound, CSN_SET_SLICE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    if (csn_is_k_form(&p->h)) csn_mark_krate(p->source_handle);
    p->registry = reg;

    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle, p->data_handle }, 2);

    int32_t res = csnarray_set_slice_locked(csound, NULL, p);

    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_set_slice_k(CSOUND *csound, CSN_SET_SLICE *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle, p->data_handle }, 2);

    int32_t res = csnarray_set_slice_locked(csound, &p->h, p);

    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

static int32_t push_check_body(CSOUND *csound, OPDS *perf_h, CSN_ARRAY *arr, bool is_complex) {
    if (arr->ndim != 1) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Push needs a 1-D array, got %u-D", arr->ndim);
    }

    if (!is_complex) {
        if (arr->itype != CSN_REAL) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Handle holds a complex array; push a :Complex; value instead");
        }
    } else {
        if (arr->itype != CSN_COMPLEX) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Handle holds a real array; push a float value instead");
        }
    }

    if (arr->size >= CSN_MAX_ELEMS) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Push would exceed the maximum element count: array already holds %zu of %zu elements", arr->size, (size_t) CSN_MAX_ELEMS);
    }

    return OK;
}

/* rt_locked is the mark of arr or, for an array private to an opcode, of the
   array it serves; external_lock covers the private arrays
   that carry their own flag. A marked array must already be large enough at
   perf time: growing it would call the allocator on the audio thread, so the
   growth is refused instead. Init-time growth stays allowed. */
int32_t ensure_mutation_capacity(CSOUND *csound, OPDS *perf_h, CSN_ARRAY *arr, size_t required_size, bool rt_locked) {
    if (required_size > arr->capacity) {
        if (arr->external_lock || (perf_h != NULL && rt_locked)) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Array is on a real-time path and cannot grow from %zu to %zu elements at perf time; clear the mark with csnrtunlock, or pass irt=0 at the audio source it descends from", arr->capacity, required_size);
        }
        size_t new_capacity = arr->capacity > 0 ? arr->capacity * 2 : 1;
        if (new_capacity < required_size) new_capacity = required_size;

        if (arr->itype == CSN_STRING) {
            char **new_data = csound->ReAlloc(csound, arr->sdata, sizeof(char *) * new_capacity);
            if (new_data == NULL) {
                return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Out of memory: allocation of %zu bytes failed", (size_t) (sizeof(char *) * new_capacity));
            }
            memset(new_data + arr->capacity, 0, sizeof(char *) * (new_capacity - arr->capacity));
            arr->sdata = new_data;
            arr->capacity = new_capacity;
        } else {
            double *new_data = csound->ReAlloc(csound, arr->data, sizeof(double) * new_capacity * arr->itype);
            if (new_data == NULL) {
                return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Out of memory: allocation of %zu bytes failed", (size_t) (sizeof(double) * new_capacity * arr->itype));
            }
            arr->data = new_data;
            arr->capacity = new_capacity;
        }
    }

    return OK;
}

/* The same rule for an opcode's private scratch, counted in items of
   item_size bytes: at perf time a scratch serving a marked array must have
   been reserved at init. A first reservation is exact; growth doubles, so a
   buffer that keeps growing off a marked path does not reallocate every
   pass. A shared operand has the lock held by the caller. */
int32_t csn_scratch_reserve(CSOUND *csound, OPDS *perf_h, bool rt_locked, CSN_SCRATCH *scratch, size_t required, size_t item_size) {
    if (scratch->scratch != NULL && required <= scratch->scratch_capacity) {
        return OK;
    }

    if (perf_h != NULL && rt_locked) {
        const char *name = get_out_name(perf_h);
        if (strcmp(name, "?") == 0) {
            return csn_locked_perf_error(csound, perf_h, "[csnarray] A working buffer needs %zu items but only %zu were reserved at init, and the array it serves is on a real-time path; clear the mark with csnrtunlock, or pass irt=0 at the audio source it descends from", required, scratch->scratch_capacity);
        }
        return csn_locked_perf_error(csound, perf_h, "[csnarray] '%s' needs a working buffer of %zu items but only %zu were reserved at init, and it is on a real-time path; clear the mark with csnrtunlock, or pass irt=0 at the audio source it descends from", name, required, scratch->scratch_capacity);
    }

    size_t new_capacity = scratch->scratch == NULL ? required : required * 2;
    if (new_capacity == 0) new_capacity = 1;
    void *data = csound->ReAlloc(csound, scratch->scratch, item_size * new_capacity);
    if (data == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Out of memory: allocation of %zu bytes failed", item_size * new_capacity);
    }

    scratch->scratch = data;
    scratch->scratch_capacity = new_capacity;
    return OK;
}

static int32_t push_in(CSOUND *csound, OPDS *perf_h, CSN_ARRAY *arr, const cs_float *in_rvalue, COMPLEXDAT *in_cvalue) {
    size_t new_size = arr->size + 1;
    int32_t res = ensure_mutation_capacity(csound, perf_h, arr, new_size, arr->rt_locked);
    if (res != OK) return res;

    if (in_cvalue == NULL) {
        arr->data[arr->size] = (double) *in_rvalue;
    } else {
        double re, im;
        complexdat_to_rect(in_cvalue, &re, &im);

        arr->data[arr->size * 2] = re;
        arr->data[arr->size * 2 + 1] = im;
    }

    arr->size = new_size;
    arr->shape[0] = (uint32_t) new_size;
    /* One element more: both the payload and the extent along axis 0 moved. */
    update_array_data_version(&arr->version);
    update_array_layout_version(&arr->version, true, false, false);
    return OK;
}

int32_t csnarray_push(CSOUND *csound, CSN_PUSH *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);

    CSN_ARRAY *arr = p->source_handle;
    res = push_check_body(csound, NULL, arr, false);
    if (res != OK) goto done;
    res = push_in(csound, NULL, arr, p->in_value, NULL);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_push_k_init(CSOUND *csound, CSN_PUSH_K *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    /* the target changes at perf: whatever reads it follows it there */
    csn_mark_krate(p->source_handle);
    p->registry = reg;
    stamp_globals(csound, &p->h);
    return OK;
}

int32_t csnarray_push_k(CSOUND *csound, CSN_PUSH_K *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_KTRIG(p->arg_a);

    int32_t res = OK;

    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);

    CSN_ARRAY *arr = p->source_handle;
    res = push_check_body(csound, &p->h, arr, false);
    if (res != OK) goto done;
    res = push_in(csound, &p->h, arr, p->in_value, NULL);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_pushcomp(CSOUND *csound, CSN_PUSHCOMPLEX *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);

    CSN_ARRAY *arr = p->source_handle;
    res = push_check_body(csound, NULL, arr, true);
    if (res != OK) goto done;
    res = push_in(csound, NULL, arr, NULL, p->in_value);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_pushcomp_k_init(CSOUND *csound, CSN_PUSHCOMPLEX_K *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    csn_mark_krate(p->source_handle);
    stamp_globals(csound, &p->h);
    p->registry = reg;
    return OK;
}

int32_t csnarray_pushcomp_k(CSOUND *csound, CSN_PUSHCOMPLEX_K *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_KTRIG(p->arg_a);

    int32_t res = OK;

    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);

    CSN_ARRAY *arr = p->source_handle;
    res = push_check_body(csound, &p->h, arr, true);
    if (res != OK) goto done;
    res = push_in(csound, &p->h, arr, NULL, p->in_value);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

static int32_t check_pop_body(CSOUND *csound, OPDS *perf_h, CSN_ARRAY *arr, bool is_complex) {
    if (arr->ndim != 1) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Pop needs a 1-D array, got %u-D", arr->ndim);
    }

    if (!is_complex) {
        if (arr->itype != CSN_REAL) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Handle holds a complex array; declare the popped value as :Complex;");
        }
    } else {
        if (arr->itype != CSN_COMPLEX) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Handle holds a real array; declare the popped value as i/k");
        }
    }

    if (arr->size == 0) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Cannot pop from an empty array");
    }

    return OK;
}

static void pop_out(CSN_ARRAY *arr, cs_float *out_rvalue, COMPLEXDAT *out_cvalue) {
    /* size and shape[0] are written from the same value, so they cannot drift
       apart the way a separate emptiness flag could. */
    size_t new_size = arr->size - 1;
    if (out_rvalue != NULL) {
        *out_rvalue = (cs_float) arr->data[new_size];
    } else {
        out_cvalue->real = (cs_float) arr->data[new_size * 2];
        out_cvalue->imag = (cs_float) arr->data[new_size * 2 + 1];
        out_cvalue->isPolar = 0;
    }
    arr->size = new_size;
    arr->shape[0] = (uint32_t) new_size;
    update_array_data_version(&arr->version);
    update_array_layout_version(&arr->version, true, false, false);
}

int32_t csnarray_pop(CSOUND *csound, CSN_POP *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);

    CSN_ARRAY *arr = p->source_handle;
    res = check_pop_body(csound, NULL, arr, false);
    if (res != OK) goto done;
    pop_out(arr, p->out_value, NULL);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_pop_k_init(CSOUND *csound, CSN_POP_K *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    csn_mark_krate(p->source_handle);
    stamp_globals(csound, &p->h);
    p->registry = reg;
    return OK;
}

int32_t csnarray_popcomp_k_init(CSOUND *csound, CSN_POPCOMPLEX_K *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    csn_mark_krate(p->source_handle);
    stamp_globals(csound, &p->h);
    p->registry = reg;
    return OK;
}

int32_t csnarray_pop_k(CSOUND *csound, CSN_POP_K *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_KTRIG(p->arg_a);

    int32_t res = OK;
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);

    CSN_ARRAY *arr = p->source_handle;
    res = check_pop_body(csound, &p->h, arr, false);
    if (res != OK) goto done;
    pop_out(arr, p->out_value, NULL);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_popcomp(CSOUND *csound, CSN_POPCOMPLEX *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);

    CSN_ARRAY *arr = p->source_handle;
    res = check_pop_body(csound, NULL, arr, true);
    if (res != OK) goto done;
    pop_out(arr, NULL, p->out_value);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_popcomp_k(CSOUND *csound, CSN_POPCOMPLEX_K *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_KTRIG(p->arg_a);

    int32_t res = OK;
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);

    CSN_ARRAY *arr = p->source_handle;
    res = check_pop_body(csound, &p->h, arr, true);
    if (res != OK) goto done;
    pop_out(arr, NULL, p->out_value);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

/*
 INSERT:
 dst_axis < index  -> src_axis = dst_axis
 dst_axis > index  -> src_axis = dst_axis - 1
 dst_axis == index -> block

 REMOVE:
 dst_axis < index  -> src_axis = dst_axis
 dst_axis >= index -> src_axis = dst_axis + 1
*/

static int32_t insert_value_locked(CSOUND *csound, OPDS *perf_h, CSN_ARRAY *arr, double index_value, const cs_float *in_rvalue, COMPLEXDAT *in_cvalue) {
    if (!IS_VALID_INDEX(index_value)) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Insert index %g is invalid; indexes must be finite non-negative integers", index_value);
    }

    bool is_complex = in_cvalue != NULL;
    if (arr->ndim != 1) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Insert needs a 1-D array, got %u-D", arr->ndim);
    }
    if ((!is_complex && arr->itype != CSN_REAL) || (is_complex && arr->itype != CSN_COMPLEX)) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, is_complex
            ? "[csnarray] Handle holds a real array; insert a real value instead"
            : "[csnarray] Handle holds a complex array; insert a :Complex; value instead");
    }
    if (arr->size >= CSN_MAX_ELEMS) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Insert would exceed the maximum element count: array already holds %zu of %zu elements", arr->size, (size_t) CSN_MAX_ELEMS);
    }

    size_t index = (size_t) index_value;
    /* Inclusive upper bound: index == size appends, which is also the only
       way to insert into an array that is currently empty. */
    if (index > arr->size) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Insert index %zu is out of range for an array of %zu elements (valid: 0..%zu, end included)", index, arr->size, arr->size);
    }

    size_t new_size = arr->size + 1;
    int32_t res = ensure_mutation_capacity(csound, perf_h, arr, new_size, arr->rt_locked);
    if (res != OK) return res;

    size_t width = (size_t) arr->itype;
    size_t count = arr->size - index;
    memmove(arr->data + (index + 1) * width, arr->data + index * width, sizeof(double) * count * width);
    if (is_complex) {
        double re, im;
        complexdat_to_rect(in_cvalue, &re, &im);
        arr->data[index * 2] = re;
        arr->data[index * 2 + 1] = im;
    } else {
        arr->data[index] = (double) *in_rvalue;
    }

    arr->size = new_size;
    arr->shape[0] = (uint32_t) new_size;
    update_array_data_version(&arr->version);
    update_array_layout_version(&arr->version, true, false, false);
    return OK;
}

int32_t csnarray_insert(CSOUND *csound, CSN_PUSH *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);
    int32_t res = insert_value_locked(csound, NULL, p->source_handle, (double) *p->index, p->in_value, NULL);
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_insert_k(CSOUND *csound, CSN_PUSH_K *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_KTRIG(p->arg_b);

    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);

    int32_t res = insert_value_locked(csound, &p->h, p->source_handle, (double) *p->arg_a, p->in_value, NULL);
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_insertcomp(CSOUND *csound, CSN_PUSHCOMPLEX *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);

    int32_t res = insert_value_locked(csound, NULL, p->source_handle, (double) *p->index, NULL, p->in_value);
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_insertcomp_k(CSOUND *csound, CSN_PUSHCOMPLEX_K *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_KTRIG(p->arg_b);

    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);

    int32_t res = insert_value_locked(csound, &p->h, p->source_handle, (double) *p->arg_a, NULL, p->in_value);
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

static int32_t remove_value_locked(CSOUND *csound, OPDS *perf_h, CSN_ARRAY *arr, double index_value, cs_float *out_rvalue, COMPLEXDAT *out_cvalue) {
    if (!IS_VALID_INDEX(index_value)) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Remove index %g is invalid; indexes must be finite non-negative integers", index_value);
    }

    bool is_complex = out_cvalue != NULL;
    if (arr->ndim != 1) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Remove needs a 1-D array, got %u-D", arr->ndim);
    }
    if ((!is_complex && arr->itype != CSN_REAL) || (is_complex && arr->itype != CSN_COMPLEX)) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, is_complex
            ? "[csnarray] Handle holds a real array; declare the removed value as i/k"
            : "[csnarray] Handle holds a complex array; declare the removed value as :Complex;");
    }
    if (arr->size == 0) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Cannot remove from an empty array");
    }

    size_t index = (size_t) index_value;
    if (index >= arr->size) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Remove index %zu is out of range for an array of %zu elements (valid: 0..%zu)", index, arr->size, arr->size - 1);
    }

    size_t width = (size_t) arr->itype;
    if (is_complex) {
        out_cvalue->real = (cs_float) arr->data[index * 2];
        out_cvalue->imag = (cs_float) arr->data[index * 2 + 1];
        out_cvalue->isPolar = 0;
    } else {
        *out_rvalue = (cs_float) arr->data[index];
    }

    size_t count = arr->size - index - 1;
    if (count > 0) {
        memmove(arr->data + index * width, arr->data + (index + 1) * width, sizeof(double) * count * width);
    }

    arr->size--;
    arr->shape[0] = (uint32_t) arr->size;
    update_array_data_version(&arr->version);
    update_array_layout_version(&arr->version, true, false, false);
    return OK;
}

int32_t csnarray_remove(CSOUND *csound, CSN_POP *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);

    int32_t res = remove_value_locked(csound, NULL, p->source_handle, (double) *p->index, p->out_value, NULL);
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_remove_k(CSOUND *csound, CSN_POP_K *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_KTRIG(p->arg_b);

    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);
    int32_t res = remove_value_locked(csound, &p->h, p->source_handle, (double) *p->arg_a, p->out_value, NULL);

    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_removecomp(CSOUND *csound, CSN_POPCOMPLEX *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);

    int32_t res = remove_value_locked(csound, NULL, p->source_handle, (double) *p->index, NULL, p->out_value);
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_removecomp_k(CSOUND *csound, CSN_POPCOMPLEX_K *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_KTRIG(p->arg_b);

    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);

    int32_t res = remove_value_locked(csound, &p->h, p->source_handle, (double) *p->arg_a, NULL, p->out_value);
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

static int32_t check_insert_block_body(CSOUND *csound, CSN_INSERT_BLOCK *p, OPDS *perf_h, CSN_ARRAY *source_arr, CSN_ARRAY *data_arr, uint32_t *temp_shape, uint32_t *axis, uint32_t *index) {
    if (source_arr->itype != data_arr->itype) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Element type mismatch: one array is real and the other complex");
    }

    uint32_t source_ndim = source_arr->ndim;
    uint32_t data_ndim = data_arr->ndim;

    if (data_ndim != source_ndim - 1) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Block is %u-D but inserting into a %u-D array needs a %u-D block", data_ndim, source_ndim, source_ndim - 1);
    }

    double axis_value = (double) *p->axis;
    CSN_AXIS_SPEC axis_spec = csn_normalize_axis_value(axis_value, source_ndim);
    if (axis_spec.kind != CSN_AXIS_INDEX) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: finite integers %d..%u)", axis_value, source_ndim, -(int32_t) source_ndim, source_ndim - 1);
    }
    *axis = axis_spec.index;

    double index_value = (double) *p->index;
    if (!IS_VALID_INDEX(index_value)) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Index %g is invalid; indexes must be finite non-negative integers", index_value);
    }

    uint32_t temp_index = (uint32_t) index_value;
    if (temp_index > source_arr->shape[*axis]) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Index %u is out of range for axis %u of extent %u (valid: 0..%u, end included)", temp_index, *axis, source_arr->shape[*axis], source_arr->shape[*axis]);
    }
    *index = temp_index;

    for (uint32_t i = 0, j = 0; i < source_ndim; i++) {
        if (i == *axis) continue;
        if (data_arr->shape[j++] != source_arr->shape[i]) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Block extent %u does not match the source extent %u on axis %u", data_arr->shape[j - 1], source_arr->shape[i], i);
        }
    }

    memcpy(temp_shape, source_arr->shape, sizeof(uint32_t) * (size_t) source_ndim);
    temp_shape[*axis]++;

    return OK;
}

static int32_t insert_block_assign_value(CSOUND *csound, CSN_ARRAY *temp, CSN_ARRAY *source_arr, CSN_ARRAY *data_arr, uint32_t source_ndim, uint32_t axis, uint32_t index) {
    CSN_BROADCAST_ITER it;
    if (ND_ITER_INIT(&it, temp->ndim, temp->shape, temp->size, NULL) != OK) return NOTOK;
    while (BROADCAST_ITER_NEXT(&it)) {
        size_t linear = it.linear_index;
        if (it.coords[axis] == index) {
            size_t block_off = 0;
            for (uint32_t i = 0, j = 0; i < source_ndim; ++i) {
                if (i != axis) block_off += (size_t) it.coords[i] * data_arr->strides[j++];
            }
            switch (source_arr->itype) {
                case CSN_REAL:
                case CSN_COMPLEX:
                    if (source_arr->itype == CSN_REAL) {
                        temp->data[linear] = data_arr->data[block_off];
                    } else {
                        temp->data[linear * 2] = data_arr->data[block_off * 2];
                        temp->data[linear * 2 + 1] = data_arr->data[block_off * 2 + 1];
                    }
                    break;
                case CSN_STRING:
                    if (temp->sdata[linear] != NULL) csound->Free(csound, temp->sdata[linear]);
                    temp->sdata[linear] = csound->Strdup(csound, data_arr->sdata[block_off]);
                    break;
            }
        } else {
            size_t source_off = 0;
            for (uint32_t d = 0; d < source_ndim; ++d) {
                uint32_t coord = it.coords[d] - (d == axis && it.coords[d] > index ? 1U : 0U);
                source_off += (size_t) coord * source_arr->strides[d];
            }
            switch (source_arr->itype) {
                case CSN_REAL:
                case CSN_COMPLEX:
                    if (source_arr->itype == CSN_REAL) {
                        temp->data[linear] = source_arr->data[source_off];
                    } else {
                        temp->data[linear * 2] = source_arr->data[source_off * 2];
                        temp->data[linear * 2 + 1] = source_arr->data[source_off * 2 + 1];
                    }
                    break;
                case CSN_STRING:
                    if (temp->sdata[linear] != NULL) csound->Free(csound, temp->sdata[linear]);
                    temp->sdata[linear] = csound->Strdup(csound, source_arr->sdata[source_off]);
                    break;
            }
        }
    }

    return OK;
}

/* An opcode's private CSN_ARRAY, taken with Calloc: frees its storage and the
   struct and clears the pointer. Csound hands a finished note's opcode memory
   to the next one without zeroing it, so a pointer left behind would be freed
   a second time by the next init or deinit. */
void free_private_array(CSOUND *csound, CSN_ARRAY **arr) {
    if (*arr == NULL) return;
    if ((*arr)->itype == CSN_STRING) {
        free_csnstrings(csound, *arr);
    } else if ((*arr)->data != NULL) {
        csound->Free(csound, (*arr)->data);
    }
    csound->Free(csound, *arr);
    *arr = NULL;
}

/* Fits an opcode's private array to the layout it is about to hold. Growth at
   perf time on a marked path would call the allocator on the audio thread, so
   it is refused there; init reserves enough for the passes that fit. A failed
   ReAlloc leaves the old block in place, so the array stays usable. The caller
   may hold the lock: errors go through csn_locked_perf_error. */
static int32_t fit_private_array(CSOUND *csound, OPDS *h, CSN_ARRAY *temp, uint32_t ndim, const uint32_t *shape, ITEM_TYPE itype, bool rt_locked) {
    size_t requested_size = 0;
    if (get_array_size_from_shape(&requested_size, ndim, shape) != OK) {
        return csn_locked_perf_error(csound, h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    if (temp->capacity < requested_size || temp->itype != itype) {
        if (rt_locked) {
            return csn_locked_perf_error(csound, h, "[csnarray] Array is on a real-time path and has grown to %zu elements, past what '%s' reserved at init; clear the mark with csnrtunlock, or pass irt=0 at the audio source it descends from", requested_size, get_out_name(h));
        }
        size_t new_capacity = requested_size > 0 ? requested_size * 2 : 1;
        if (itype == CSN_STRING) {
            /* a string scratch is never retyped; its new slots start NULL */
            char **sdata = csound->ReAlloc(csound, temp->sdata, sizeof(char *) * new_capacity);
            if (sdata == NULL) {
                return csn_locked_perf_error(csound, h, "[csnarray] Out of memory: allocation of %zu bytes failed", sizeof(char *) * new_capacity);
            }
            memset(sdata + temp->capacity, 0, sizeof(char *) * (new_capacity - temp->capacity));
            temp->sdata = sdata;
            temp->capacity = new_capacity;
        } else {
            double *data = csound->ReAlloc(csound, temp->data, sizeof(double) * new_capacity * itype);
            if (data == NULL) {
                return csn_locked_perf_error(csound, h, "[csnarray] Out of memory: allocation of %zu bytes failed", sizeof(double) * new_capacity * itype);
            }
            temp->data = data;
            temp->capacity = new_capacity;
        }
    }
    set_csnarray_layout(temp, ndim, shape, requested_size, itype);
    return OK;
}

int32_t csnarray_insert_block_deinit(CSOUND *csound, CSN_INSERT_BLOCK *p) {
    free_private_array(csound, &p->scratch);
    return OK;
}

int32_t csnarray_insert_block(CSOUND *csound, CSN_INSERT_BLOCK *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    free_private_array(csound, &p->scratch);
    CSN_ARRAY *temp = NULL;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle, p->data_handle }, 2);

    CSN_ARRAY *source_arr = p->source_handle;
    CSN_ARRAY *data_arr = p->data_handle;
    uint32_t temp_shape[CSN_MAX_DIMS] = {0};
    uint32_t axis = 0;
    uint32_t index = 0;
    res = check_insert_block_body(csound, p, NULL, source_arr, data_arr, temp_shape, &axis, &index);
    if (res != OK) goto done;

    temp = csound->Calloc(csound, sizeof(CSN_ARRAY));
    if (temp == NULL) {
        res = csn_init_error(csound, "[csnarray] Out of memory: allocation of %zu bytes failed", (size_t) (sizeof(CSN_ARRAY)));
        goto done;
    }

    uint32_t source_ndim = source_arr->ndim;
    int32_t alloc_temp = allocate_array(csound, temp, source_ndim, temp_shape, source_arr->itype);
    if (alloc_temp != OK) {
        char tbuf[CSN_SHAPE_STR_MAX];
        res = csn_init_error(csound, "[csnarray] Out of memory: could not allocate the %u-D temporary array %s", source_ndim, shape_str(tbuf, sizeof(tbuf), temp_shape, source_ndim));
        goto done;
    }

    if (insert_block_assign_value(csound, temp, source_arr, data_arr, source_ndim, axis, index) != OK) {
        res = csn_init_error(csound, "[csnarray] Invalid insert iterator layout");
        goto done;
    }

    res = ensure_mutation_capacity(csound, NULL, source_arr, temp->size, false);
    if (res != OK) goto done;

    /* the image is only needed for this one call, so its strings change
       owner instead of being copied again */
    if (source_arr->itype == CSN_STRING) {
        move_csnstrings(csound, source_arr, temp);
    } else {
        travase_csnarray(source_arr, temp);
    }

    p->registry = reg;

done:
    free_private_array(csound, &temp);
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_insert_block_k_init(CSOUND *csound, CSN_INSERT_BLOCK *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    csn_mark_krate(p->source_handle);

    int32_t res = OK;
    free_private_array(csound, &p->scratch);
    CSN_ARRAY *temp = NULL;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle, p->data_handle }, 2);

    CSN_ARRAY *source_arr = p->source_handle;
    CSN_ARRAY *data_arr = p->data_handle;
    uint32_t temp_shape[CSN_MAX_DIMS] = {0};
    uint32_t axis = 0;
    uint32_t index = 0;
    res = check_insert_block_body(csound, p, NULL, source_arr, data_arr, temp_shape, &axis, &index);
    if (res != OK) goto done;

    temp = csound->Calloc(csound, sizeof(CSN_ARRAY));
    if (temp == NULL) {
        res = csn_init_error(csound, "[csnarray] Out of memory: allocation of %zu bytes failed", (size_t) (sizeof(CSN_ARRAY)));
        goto done;
    }

    uint32_t source_ndim = source_arr->ndim;
    int32_t alloc_temp = allocate_array(csound, temp, source_ndim, temp_shape, source_arr->itype);
    if (alloc_temp != OK) {
        char tbuf[CSN_SHAPE_STR_MAX];
        res = csn_init_error(csound, "[csnarray] Out of memory: could not allocate the %u-D temporary array %s", source_ndim, shape_str(tbuf, sizeof(tbuf), temp_shape, source_ndim));
        goto done;
    }

    /* The image is copied back over the source, so it never needs more room
       than the source can take without reallocating. Reserving that much here
       keeps a marked source from being refused by its scratch first. */
    res = ensure_mutation_capacity(csound, NULL, temp, source_arr->capacity, false);
    if (res != OK) goto done;

    p->scratch = temp;
    temp = NULL;
    p->registry = reg;

done:
    free_private_array(csound, &temp);
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_insert_block_k(CSOUND *csound, CSN_INSERT_BLOCK *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    if (p->scratch == NULL) {
        return csn_perf_error(csound, &p->h, "[csnarray] Internal error: temporary buffer not available");
    }

    CHECK_KTRIG(p->trig);

    /* a pass that inserts strings copies them */
    if (p->source_handle->itype == CSN_STRING) {
        int32_t res_rt = check_strings_not_rt(csound, &p->h, p->source_handle);
        if (res_rt != OK) return res_rt;
    }

    int32_t res = OK;
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle, p->data_handle }, 2);

    CSN_ARRAY *source_arr = p->source_handle;
    CSN_ARRAY *data_arr = p->data_handle;
    uint32_t temp_shape[CSN_MAX_DIMS] = {0};
    uint32_t axis = 0;
    uint32_t index = 0;
    res = check_insert_block_body(csound, p, &p->h, source_arr, data_arr, temp_shape, &axis, &index);
    if (res != OK) goto done;

    uint32_t source_ndim = source_arr->ndim;
    res = fit_private_array(csound, &p->h, p->scratch, source_ndim, temp_shape, source_arr->itype, source_arr->rt_locked);
    if (res != OK) goto done;

    /* The source only reallocates when the image outgrows it, never on a pass
       that still fits. */
    res = ensure_mutation_capacity(csound, &p->h, source_arr, p->scratch->size, source_arr->rt_locked);
    if (res != OK) goto done;

    if (insert_block_assign_value(csound, p->scratch, source_arr, data_arr, source_ndim, axis, index) != OK) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Invalid insert iterator layout");
        goto done;
    }

    /* the scratch is rebuilt every pass: its strings change owner */
    if (source_arr->itype == CSN_STRING) {
        move_csnstrings(csound, source_arr, p->scratch);
    } else {
        travase_csnarray(source_arr, p->scratch);
    }

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

static int32_t check_remove_block_body(CSOUND *csound, CSN_TAKE *p, OPDS *perf_h, CSN_ARRAY *source_arr, uint32_t *temp_shape, uint32_t *axis, uint32_t *index) {
    uint32_t source_ndim = source_arr->ndim;

    double axis_value = (double) *p->axis;
    CSN_AXIS_SPEC axis_spec = csn_normalize_axis_value(axis_value, source_ndim);
    if (axis_spec.kind != CSN_AXIS_INDEX) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: finite integers %d..%u)", axis_value, source_ndim, -(int32_t) source_ndim, source_ndim - 1);
    }
    *axis = axis_spec.index;

    double index_value = (double) *p->index;
    if (!IS_VALID_INDEX(index_value)) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Index %g is invalid; indexes must be finite non-negative integers", index_value);
    }

    uint32_t extent = source_arr->shape[*axis];
    if (extent == 0) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Cannot remove a block from axis %u because its extent is zero", *axis);
    }

    uint32_t temp_index = (uint32_t) index_value;
    if (temp_index >= extent) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Index %u is out of range for axis %u of extent %u (valid: 0..%u)", temp_index, *axis, extent, extent - 1);
    }
    *index = temp_index;

    memcpy(temp_shape, source_arr->shape, sizeof(uint32_t) * (size_t) source_ndim);
    temp_shape[*axis]--;

    return OK;
}

static int32_t remove_block_assign_value(CSOUND *csound, CSN_ARRAY *source_arr, CSN_ARRAY *destination, uint32_t axis, uint32_t index) {
    CSN_BROADCAST_ITER it;
    if (ND_ITER_INIT(&it, destination->ndim, destination->shape, destination->size, NULL) != OK) return NOTOK;
    while (BROADCAST_ITER_NEXT(&it)) {
        size_t linear = it.linear_index;
        size_t source_off = 0;
        for (uint32_t d = 0; d < source_arr->ndim; ++d) {
            uint32_t coord = it.coords[d] + (d == axis && it.coords[d] >= index ? 1U : 0U);
            source_off += (size_t) coord * source_arr->strides[d];
        }
        switch (source_arr->itype) {
            case CSN_REAL:
            case CSN_COMPLEX:
                if (source_arr->itype == CSN_REAL) {
                    destination->data[linear] = source_arr->data[source_off];
                } else {
                    destination->data[linear * 2] = source_arr->data[source_off * 2];
                    destination->data[linear * 2 + 1] = source_arr->data[source_off * 2 + 1];
                }
                break;
            case CSN_STRING:
                if (destination->sdata[linear] != NULL) csound->Free(csound, destination->sdata[linear]);
                destination->sdata[linear] = csound->Strdup(csound, source_arr->sdata[source_off]);
                break;
        }
    }

    return OK;
}

int32_t csnarray_remove_block(CSOUND *csound, CSN_TAKE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle }, 2);

    CSN_ARRAY *source_arr = p->source_handle;
    uint32_t axis = 0;
    uint32_t index = 0;
    uint32_t temp_shape[CSN_MAX_DIMS] = {0};
    res = check_remove_block_body(csound, p, NULL, source_arr, temp_shape, &axis, &index);
    if (res != OK) goto done;

    uint32_t source_ndim = source_arr->ndim;
    const CSN_ARRAY *ops[1] = { source_arr };
    CSNARR_RES out = prepare_output(csound, &p->h, NULL, reg, p->handle, source_ndim, temp_shape, source_arr->itype, ops, 1, CSN_ALIAS_REFUSE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    if (remove_block_assign_value(csound, source_arr, p->handle, axis, index) != OK) {
        res = csn_init_error(csound, "[csnarray] Invalid remove iterator layout");
        goto done;
    }

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_remove_block_k_init(CSOUND *csound, CSN_TAKE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle }, 2);

    CSN_ARRAY *source_arr = p->source_handle;
    const CSN_ARRAY *ops[1] = { source_arr };
    CSNARR_RES out = prepare_output(csound, &p->h, NULL, reg, p->handle, source_arr->ndim, source_arr->shape, source_arr->itype, ops, 1, CSN_ALIAS_REFUSE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    p->handle->size = source_arr->size;
    CSN_MEMCPY(csound, p->handle, source_arr);
    SET_KDATA_BEGIN(p->k_data, reg, source_arr->shape, source_arr->ndim, source_arr->itype);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_remove_block_k(CSOUND *csound, CSN_TAKE *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    CHECK_KTRIG(p->trig);

    /* a pass that removes from strings copies the rest */
    if (p->source_handle->itype == CSN_STRING) {
        int32_t res_rt = check_strings_not_rt(csound, &p->h, p->handle);
        if (res_rt != OK) return res_rt;
    }

    int32_t res = OK;
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle }, 2);

    CSN_ARRAY *source_arr = p->source_handle;
    uint32_t axis = 0;
    uint32_t index = 0;
    uint32_t temp_shape[CSN_MAX_DIMS] = {0};
    res = check_remove_block_body(csound, p, &p->h, source_arr, temp_shape, &axis, &index);
    if (res != OK) goto done;

    if (p->k_data.prev_axis_u == axis && p->k_data.prev_index == index && CAN_REUSE_ELEMENTWISE(&p->k_data, source_arr, NULL, p->handle, 0.0, 0.0)) goto done;

    uint32_t source_ndim = source_arr->ndim;
    const CSN_ARRAY *ops[1] = { source_arr };
    CSNARR_RES out = prepare_output(csound, &p->h, &p->h, reg, p->handle, source_ndim, temp_shape, source_arr->itype, ops, 1, CSN_ALIAS_REFUSE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    if (remove_block_assign_value(csound, source_arr, p->handle, axis, index) != OK) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Invalid remove iterator layout");
        goto done;
    }

    SET_KDATA_END(p->k_data, temp_shape, source_ndim, source_arr->itype);
    PUBLISH_ELEMENTWISE(&p->k_data, source_arr, NULL, p->handle, 0.0, 0.0);
    p->k_data.prev_axis_u = axis;
    p->k_data.prev_index = index;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

static int32_t concat_flat_body(CSOUND *csound, OPDS *perf_h, const CSN_ARRAY *source_arr, const CSN_ARRAY *data_arr, uint32_t *out_shape) {
    if (source_arr->itype != data_arr->itype) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Element type mismatch: one array is real and the other complex");
    }

    uint32_t source_ndim = source_arr->ndim;
    uint32_t data_ndim = data_arr->ndim;

    if (source_ndim != 1U || data_ndim != 1U) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Both arrays must be 1-D, got %u-D and %u-D", source_ndim, data_ndim);
    }

    /* An empty array retains its requested physical shape, but contributes no
       logical elements to a concatenation. */
    if (source_arr->size > UINT32_MAX ||
        data_arr->size > UINT32_MAX - source_arr->size) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Concatenated length exceeds %u elements", UINT32_MAX);
    }
    out_shape[0] = (uint32_t) (source_arr->size + data_arr->size);

    return OK;
}

static void concat_flat_assign_value(CSOUND *csound, const CSN_ARRAY *source_arr, const CSN_ARRAY *data_arr, CSN_ARRAY *destination) {
    ITEM_TYPE itype = destination->itype;
    for (size_t linear = 0; linear < destination->size; ++linear) {
        switch (itype) {
            case CSN_REAL:
            case CSN_COMPLEX:
                if (destination->itype == CSN_COMPLEX) {
                    size_t src = linear < source_arr->size ? linear : linear - source_arr->size;
                    const double *from = linear < source_arr->size ? source_arr->data : data_arr->data;
                    destination->data[linear * 2] = from[src * 2];
                    destination->data[linear * 2 + 1] = from[src * 2 + 1];
                    continue;
                }

                destination->data[linear] = linear < source_arr->size
                    ? source_arr->data[linear]
                    : data_arr->data[linear - source_arr->size];
                break;
            case CSN_STRING: {
                const char *from = linear < source_arr->size
                    ? source_arr->sdata[linear]
                    : data_arr->sdata[linear - source_arr->size];
                if (destination->sdata[linear] != NULL) csound->Free(csound, destination->sdata[linear]);
                destination->sdata[linear] = csound->Strdup(csound, from);
                break;
            }
        }
    }
}

int32_t csnarray_concat_flat(CSOUND *csound, CSN_CONCAT *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle, p->data_handle }, 3);

    CSN_ARRAY *source_arr = p->source_handle;
    CSN_ARRAY *data_arr = p->data_handle;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    res = concat_flat_body(csound, NULL, source_arr, data_arr, new_shape);
    if (res != OK) goto done;

    /* The output grows past both operands, so it can be neither of them. */
    const CSN_ARRAY *ops[2] = { source_arr, data_arr };
    CSNARR_RES out = prepare_output(csound, &p->h, NULL, reg, p->handle, 1U, new_shape, source_arr->itype, ops, 2, CSN_ALIAS_REFUSE | CSN_OUTPUT_FULL);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    concat_flat_assign_value(csound, source_arr, data_arr, p->handle);
    SET_KDATA_BEGIN(p->k_data, reg, new_shape, 1U, p->handle->itype);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_concat_flat_k(CSOUND *csound, CSN_CONCAT *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    if (p->source_handle->itype == CSN_STRING && p->handle != p->source_handle) {
        int32_t res_rt = check_strings_not_rt(csound, &p->h, p->handle);
        if (res_rt != OK) return res_rt;
    }

    CHECK_KTRIG(p->arg_a);

    int32_t res = OK;
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle, p->data_handle }, 3);

    CSN_ARRAY *source_arr = p->source_handle;
    CSN_ARRAY *data_arr = p->data_handle;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    res = concat_flat_body(csound, &p->h, source_arr, data_arr, new_shape);
    if (res != OK) goto done;

    if (CAN_REUSE_ELEMENTWISE(&p->k_data, source_arr, data_arr, p->handle, 0.0, 0.0)) goto done;

    const CSN_ARRAY *ops[2] = { source_arr, data_arr };
    CSNARR_RES out = prepare_output(csound, &p->h, &p->h, reg, p->handle, 1U, new_shape, source_arr->itype, ops, 2, CSN_ALIAS_REFUSE | CSN_OUTPUT_FULL);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    concat_flat_assign_value(csound, source_arr, data_arr, p->handle);
    SET_KDATA_END(p->k_data, new_shape, 1U, source_arr->itype);
    PUBLISH_ELEMENTWISE(&p->k_data, source_arr, data_arr, p->handle, 0.0, 0.0);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

static int32_t concat_block_body(CSOUND *csound, OPDS *perf_h, const CSN_ARRAY *source_arr, const CSN_ARRAY *data_arr, uint32_t *out_shape, const cs_float *in_axis, uint32_t *out_axis) {
    if (source_arr->itype != data_arr->itype) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Element type mismatch: one array is real and the other complex");
    }

    uint32_t source_ndim = source_arr->ndim;
    uint32_t data_ndim = data_arr->ndim;

    if (data_ndim != source_ndim) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Arrays must have the same number of dimensions, got %u-D and %u-D", source_ndim, data_ndim);
    }

    const uint32_t *source_shape = source_arr->shape;
    const uint32_t *data_shape = data_arr->shape;

    double axis_value = (double) *in_axis;
    CSN_AXIS_SPEC axis_spec = csn_normalize_axis_value(axis_value, source_ndim);
    if (axis_spec.kind != CSN_AXIS_INDEX) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: finite integers %d..%u)", axis_value, source_ndim, -(int32_t) source_ndim, source_ndim - 1);
    }
    *out_axis = axis_spec.index;

    /* A logically empty operand contributes zero extent on the concatenation
       axis. Other axes still have to match its declared physical shape. */
    uint32_t source_axis_extent = source_arr->size == 0 ? 0U : source_shape[*out_axis];
    uint32_t data_axis_extent = data_arr->size == 0 ? 0U : data_shape[*out_axis];
    if (data_axis_extent > UINT32_MAX - source_axis_extent) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Concatenated extent on axis %u exceeds %u", *out_axis, UINT32_MAX);
    }

    for (uint32_t i = 0; i < source_ndim; i++) {
        if (i == *out_axis) {
            out_shape[i] = source_axis_extent + data_axis_extent;
        } else {
            if (source_shape[i] != data_shape[i]) {
                char sbuf[CSN_SHAPE_STR_MAX], dbuf[CSN_SHAPE_STR_MAX];
                return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Shapes %s and %s differ on axis %u (%u vs %u); only the concat axis %u may differ", shape_str(sbuf, sizeof(sbuf), source_shape, source_ndim), shape_str(dbuf, sizeof(dbuf), data_shape, data_ndim), i, source_shape[i], data_shape[i], *out_axis);
            }
            out_shape[i] = source_shape[i];
        }
    }

    return OK;
}

static int32_t concat_block_assign_value(const CSN_ARRAY *source_arr, const CSN_ARRAY *data_arr, CSN_ARRAY *destination, uint32_t axis) {
    uint32_t source_axis_extent = source_arr->size == 0 ? 0U : source_arr->shape[axis];
    CSN_BROADCAST_ITER it;
    if (ND_ITER_INIT(&it, destination->ndim, destination->shape, destination->size, NULL) != OK) return NOTOK;
    while (BROADCAST_ITER_NEXT(&it)) {
        size_t linear = it.linear_index;
        if (it.coords[axis] < source_axis_extent) {
            size_t source_off = 0;
            for (uint32_t d = 0; d < source_arr->ndim; ++d) {
                source_off += (size_t) it.coords[d] * source_arr->strides[d];
            }
            if (destination->itype == CSN_COMPLEX) {
                destination->data[linear * 2] = source_arr->data[source_off * 2];
                destination->data[linear * 2 + 1] = source_arr->data[source_off * 2 + 1];
            } else {
                destination->data[linear] = source_arr->data[source_off];
            }
        } else {
            size_t block_off = 0;
            for (uint32_t d = 0; d < data_arr->ndim; ++d) {
                uint32_t coord = it.coords[d] - (d == axis ? source_axis_extent : 0U);
                block_off += (size_t) coord * data_arr->strides[d];
            }
            if (destination->itype == CSN_COMPLEX) {
                destination->data[linear * 2] = data_arr->data[block_off * 2];
                destination->data[linear * 2 + 1] = data_arr->data[block_off * 2 + 1];
            } else {
                destination->data[linear] = data_arr->data[block_off];
            }
        }
    }

    return OK;
}

int32_t csnarray_concat_block(CSOUND *csound, CSN_CONCAT *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle, p->data_handle }, 3);

    CSN_ARRAY *source_arr = p->source_handle;
    CSN_ARRAY *data_arr = p->data_handle;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    uint32_t axis = 0;
    res = concat_block_body(csound, NULL, source_arr, data_arr, new_shape, p->arg_a, &axis);
    if (res != OK) goto done;

    const CSN_ARRAY *ops[2] = { source_arr, data_arr };
    CSNARR_RES out = prepare_output(csound, &p->h, NULL, reg, p->handle, source_arr->ndim, new_shape, source_arr->itype, ops, 2, CSN_ALIAS_REFUSE | CSN_OUTPUT_FULL);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    if (p->handle->itype == CSN_STRING) {
        res = concat_block_string_assign_value(csound, source_arr, data_arr, p->handle, axis);
    } else {
        res = concat_block_assign_value(source_arr, data_arr, p->handle, axis);
    }

    if (res != OK) {
        res = csn_init_error(csound, "[csnarray] Invalid concatenate iterator layout");
        goto done;
    }

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

/* The axis is a k-argument and usually still 0 here, so the output only takes
   the source's layout and contents; the first pass that fires concatenates. */
int32_t csnarray_concat_block_k_init(CSOUND *csound, CSN_CONCAT *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle, p->data_handle }, 3);

    CSN_ARRAY *source_arr = p->source_handle;
    CSN_ARRAY *data_arr = p->data_handle;
    const CSN_ARRAY *ops[2] = { source_arr, data_arr };
    memset(&p->k_data, 0, sizeof(p->k_data));
    p->k_data.registry = reg;

    /* A k axis is not known at init, and it fixes the joined extents: the
       output keeps the source's layout reserved but empty (joining keeps the
       rank) and the first pass joins. */
    if (!csn_known_at_init(csound, p->arg_b)) {
        CSNARR_RES out = prepare_output(csound, &p->h, NULL, reg, p->handle, source_arr->ndim, source_arr->shape, source_arr->itype, ops, 2, CSN_ALIAS_REFUSE);
        res = out.err;
        pres.locked |= out.locked;
        if (res == OK) reset_empty_csnarray(p->handle, source_arr->ndim, source_arr->shape, source_arr->itype);
        goto done;
    }

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    uint32_t axis = 0;
    res = concat_block_body(csound, NULL, source_arr, data_arr, new_shape, p->arg_b, &axis);
    if (res != OK) goto done;

    CSNARR_RES out = prepare_output(csound, &p->h, NULL, reg, p->handle, source_arr->ndim, new_shape, source_arr->itype, ops, 2, CSN_ALIAS_REFUSE | CSN_OUTPUT_FULL);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    if (p->handle->itype == CSN_STRING) {
        res = concat_block_string_assign_value(csound, source_arr, data_arr, p->handle, axis);
    } else {
        res = concat_block_assign_value(source_arr, data_arr, p->handle, axis);
    }

    if (res != OK) {
        res = csn_init_error(csound, "[csnarray] Invalid concatenate iterator layout");
        goto done;
    }

    SET_KDATA_BEGIN(p->k_data, reg, new_shape, p->handle->ndim, p->handle->itype);
    PUBLISH_ELEMENTWISE(&p->k_data, source_arr, data_arr, p->handle, (double) axis, 0.0);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_concat_block_k(CSOUND *csound, CSN_CONCAT *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    if (p->source_handle->itype == CSN_STRING && p->handle != p->source_handle) {
        int32_t res_rt = check_strings_not_rt(csound, &p->h, p->handle);
        if (res_rt != OK) return res_rt;
    }

    CHECK_KTRIG(p->arg_a);

    int32_t res = OK;
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle, p->data_handle }, 3);

    CSN_ARRAY *source_arr = p->source_handle;
    CSN_ARRAY *data_arr = p->data_handle;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    uint32_t axis = 0;
    res = concat_block_body(csound, &p->h, source_arr, data_arr, new_shape, p->arg_b, &axis);
    if (res != OK) goto done;

    if (CAN_REUSE_ELEMENTWISE(&p->k_data, source_arr, data_arr, p->handle, (double) axis, 0.0)) goto done;

    const CSN_ARRAY *ops[2] = { source_arr, data_arr };
    CSNARR_RES out = prepare_output(csound, &p->h, &p->h, reg, p->handle, source_arr->ndim, new_shape, source_arr->itype, ops, 2, CSN_ALIAS_REFUSE | CSN_OUTPUT_FULL);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    if (p->handle->itype == CSN_STRING) {
        res = concat_block_string_assign_value(csound, source_arr, data_arr, p->handle, axis);
    } else {
        res = concat_block_assign_value(source_arr, data_arr, p->handle, axis);
    }

    if (res != OK) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Invalid concatenate iterator layout");
        goto done;
    }

    SET_KDATA_END(p->k_data, new_shape, p->handle->ndim, p->handle->itype);
    PUBLISH_ELEMENTWISE(&p->k_data, source_arr, data_arr, p->handle, (double) axis, 0.0);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

/* The fill value's type picks the overload; the array's must match it. */
static int32_t pad_type_error(CSOUND *csound, OPDS *perf_h, ITEM_TYPE held, ITEM_TYPE expected) {
    if (expected == CSN_STRING || held == CSN_STRING) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] A CsnStr is padded with a string, a CsnArr with a number");
    }
    if (expected == CSN_COMPLEX) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Handle holds a real array; pad it with a real value");
    }
    return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Handle holds a complex array; pad it with a :Complex; value");
}

static int32_t pad_body(CSOUND *csound, OPDS *perf_h, const CSN_ARRAY *source_arr, const cs_float *axis_in, const cs_float *in_before, const cs_float *in_after, int32_t *out_axis, uint32_t *out_before, uint32_t *out_after, uint32_t *out_shape, ITEM_TYPE expected_type) {
    uint32_t source_ndim = source_arr->ndim;
    const uint32_t *source_shape = source_arr->shape;

    if (source_arr->itype != expected_type) return pad_type_error(csound, perf_h, source_arr->itype, expected_type);

    /* The widths are k-rate in the .k overloads, so they have to be screened
       for NaN, infinities and fractions before the cast to uint32_t. */
    double before_value = (double) *in_before;
    double after_value = (double) *in_after;
    if (!IS_VALID_INDEX(before_value) || !IS_VALID_INDEX(after_value)) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Pad widths must be finite non-negative integers, got before=%g and after=%g", before_value, after_value);
    }

    *out_before = (uint32_t) before_value;
    *out_after = (uint32_t) after_value;

    if (*out_before > UINT32_MAX - *out_after) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Combined pad width %g exceeds %u", before_value + after_value, UINT32_MAX);
    }
    uint32_t pad_extent = *out_before + *out_after;

    CSN_AXIS_SPEC axis_spec = csn_normalize_axis(axis_in, source_ndim, CSN_AXIS_DEFAULT_ALL);
    if (axis_spec.kind == CSN_AXIS_INVALID) {
        double value = axis_in == NULL ? 0.0 : (double) *axis_in;
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: finite integers %d..%u)", value, source_ndim, -(int32_t) source_ndim, source_ndim - 1);
    }
    int32_t axis = axis_spec.kind == CSN_AXIS_ALL ? -1 : (int32_t) axis_spec.index;
    *out_axis = axis;

    /* A logically empty operand carries no elements to copy, so it contributes
       zero extent on every axis and the result is padding only. */
    bool source_is_empty = source_arr->size == 0;

    for (uint32_t i = 0; i < source_ndim; i++) {
        uint32_t source_extent = source_is_empty ? 0U : source_shape[i];
        if (axis == -1 || (uint32_t) axis == i) {
            if (source_extent > UINT32_MAX - pad_extent) {
                return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Padded extent on axis %u exceeds %u", i, UINT32_MAX);
            }
            out_shape[i] = source_extent + pad_extent;
        } else {
            out_shape[i] = source_extent;
        }
    }

    return OK;
}

int32_t pad_assign_value(const CSN_ARRAY *source_arr, CSN_ARRAY *destination, double real_value, COMPLEXDAT *complex_value, int32_t axis, uint32_t before) {
    bool is_complex = destination->itype == CSN_COMPLEX;
    /* Mirrors the extents pad_body derived: an empty source has nothing to
       copy, so every destination cell is padding. */
    bool source_is_empty = source_arr->size == 0;
    double re = 0.0, im = 0.0;

    if (is_complex && complex_value != NULL) {
        complexdat_to_rect(complex_value, &re, &im);
    }

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

        if (!is_inside) {
            if (is_complex) {
                destination->data[linear * 2] = re;
                destination->data[linear * 2 + 1] = im;
            } else {
                destination->data[linear] = real_value;
            }
            continue;
        }

        if (is_complex) {
            destination->data[linear * 2] = source_arr->data[source_off * 2];
            destination->data[linear * 2 + 1] = source_arr->data[source_off * 2 + 1];
        } else {
            destination->data[linear] = source_arr->data[source_off];
        }
    }
    return OK;
}

int32_t csnarray_pad_helper(CSOUND *csound, OPDS *h, CSN_ARRAY *out_arr, CSN_ARRAY *source_arr, const cs_float *in_before, const cs_float *in_after, double value, COMPLEXDAT *valuecomp, STRINGDAT *valuestring, ITEM_TYPE expected_itype, const cs_float *axis_in) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    stamp_globals(csound, h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { out_arr, source_arr }, 2);

    int32_t axis = 0;
    uint32_t before = 0;
    uint32_t after = 0;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    res = pad_body(csound, NULL, source_arr, axis_in, in_before, in_after, &axis, &before, &after, new_shape, expected_itype);
    if (res != OK) goto done;

    /* Every cell moves, so writing over the source is not an option: the
       in-place form is csnpad.in. */
    const CSN_ARRAY *ops[1] = { source_arr };
    CSNARR_RES out = prepare_output(csound, h, NULL, reg, out_arr, source_arr->ndim, new_shape, source_arr->itype, ops, 1, CSN_ALIAS_REFUSE | CSN_OUTPUT_FULL);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    if (out_arr->itype != CSN_STRING) {
        res = pad_assign_value(source_arr, out_arr, value, valuecomp, axis, before);
    } else {
        res = pad_assign_string_value(csound, source_arr, out_arr, valuestring, axis, before);
    }

    if (res != OK) {
        res = csn_init_error(csound, "[csnarray] Invalid pad iterator layout");
        goto done;
    }

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

/* The widths and the axis of the k-rate pads are k-arguments, read here at
   their init values. When those already describe a valid pad, the output is
   created at the padded shape, so a perf pass that keeps the same widths needs
   no new storage, which a marked output could not take. Widths that only
   settle during performance (a plain `=` still reads 0 at init) keep the
   source's shape, and the first pass that pads reshapes the output as before.
   Anything invalid is left for the perf pass to report, as it always was: the
   checks below mirror pad_body's, so the call into it cannot raise. */
static bool pad_k_init_shape(CSOUND *csound, const CSN_ARRAY *source_arr, const cs_float *axis_in, const cs_float *in_before, const cs_float *in_after, ITEM_TYPE itype, int32_t *axis, uint32_t *before, uint32_t *after, uint32_t *shape) {
    double before_value = (double) *in_before;
    double after_value = (double) *in_after;
    if (!IS_VALID_INDEX(before_value) || !IS_VALID_INDEX(after_value)) return false;
    if (before_value + after_value > (double) UINT32_MAX) return false;
    CSN_AXIS_SPEC axis_spec = csn_normalize_axis(axis_in, source_arr->ndim, CSN_AXIS_DEFAULT_ALL);
    if (axis_spec.kind == CSN_AXIS_INVALID) return false;
    int32_t resolved_axis = axis_spec.kind == CSN_AXIS_ALL ? -1 : (int32_t) axis_spec.index;

    double pad_extent = before_value + after_value;
    for (uint32_t i = 0; i < source_arr->ndim; i++) {
        double extent = source_arr->size == 0 ? 0.0 : (double) source_arr->shape[i];
        if ((resolved_axis == -1 || (uint32_t) resolved_axis == i) && extent + pad_extent > (double) UINT32_MAX) return false;
    }

    return pad_body(csound, NULL, source_arr, axis_in, in_before, in_after, axis, before, after, shape, itype) == OK;
}

int32_t csnarray_pad_k_init_helper(CSOUND *csound, OPDS *h, K_DATA *k_data, CSN_ARRAY *out_arr, CSN_ARRAY *source_arr, const cs_float *in_before, const cs_float *in_after, const cs_float *value_arg, double value, COMPLEXDAT *valuecomp, STRINGDAT *valuestring, ITEM_TYPE expected_itype, const cs_float *axis_in) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    stamp_globals(csound, h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { out_arr, source_arr }, 2);

    if (source_arr->itype != expected_itype) {
        res = pad_type_error(csound, NULL, source_arr->itype, expected_itype);
        goto done;
    }

    memset(k_data, 0, sizeof(*k_data));
    k_data->registry = reg;
    const CSN_ARRAY *ops[1] = { source_arr };

    /* Padded here only when the widths, the value and the axis are known at
       init; they also fix the padded extents, so otherwise the output keeps
       the source's layout reserved but empty and the first pass pads. */
    bool know_val = expected_itype == CSN_STRING ? csn_known_at_init(csound, (void *) valuestring) : csn_known_at_init(csound, (void *) value_arg);
    bool known = csn_known_at_init(csound, (void *) in_before) && csn_known_at_init(csound, (void *) in_after)
        && know_val && csn_known_at_init(csound, (void *) axis_in);
    if (!known) {
        /* The init values are no result, but they are the best guess at the
           room the perf pass will need: a marked output keeps the storage it
           is created with, so it is created at the padded shape they give
           (when they give one) and then emptied. */
        uint32_t hint_shape[CSN_MAX_DIMS] = {0};
        memcpy(hint_shape, source_arr->shape, sizeof(hint_shape));
        int32_t hint_axis = -1;
        uint32_t hint_before = 0, hint_after = 0;
        if (!pad_k_init_shape(csound, source_arr, axis_in, in_before, in_after, expected_itype, &hint_axis, &hint_before, &hint_after, hint_shape)) {
            memcpy(hint_shape, source_arr->shape, sizeof(hint_shape));
        }
        CSNARR_RES out = prepare_output(csound, h, NULL, reg, out_arr, source_arr->ndim, hint_shape, source_arr->itype, ops, 1, CSN_ALIAS_REFUSE);
        res = out.err;
        pres.locked |= out.locked;
        if (res == OK) reset_empty_csnarray(out_arr, source_arr->ndim, source_arr->shape, source_arr->itype);
        goto done;
    }

    uint32_t shape[CSN_MAX_DIMS] = {0};
    memcpy(shape, source_arr->shape, sizeof(shape));
    int32_t axis = -1;
    uint32_t before = 0;
    uint32_t after = 0;
    if (!pad_k_init_shape(csound, source_arr, axis_in, in_before, in_after, expected_itype, &axis, &before, &after, shape)) {
        res = pad_body(csound, NULL, source_arr, axis_in, in_before, in_after, &axis, &before, &after, shape, expected_itype);
        if (res == OK) res = csn_init_error(csound, "[csnarray] Pad widths exceed the largest extent an array can have");
        goto done;
    }

    CSNARR_RES out = prepare_output(csound, h, NULL, reg, out_arr, source_arr->ndim, shape, source_arr->itype, ops, 1, CSN_ALIAS_REFUSE | CSN_OUTPUT_FULL);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    if (out_arr->itype != CSN_STRING) {
        res = pad_assign_value(source_arr, out_arr, value, valuecomp, axis, before);
    } else {
        res = pad_assign_string_value(csound, source_arr, out_arr, valuestring, axis, before);
    }

    if (res != OK) {
        res = csn_init_error(csound, "[csnarray] Invalid pad iterator layout");
        goto done;
    }

    SET_KDATA_BEGIN(*k_data, reg, shape, source_arr->ndim, source_arr->itype);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_pad_k_init(CSOUND *csound, CSN_PAD *p) {
    const cs_float *axis_in = p->INOCOUNT > 5 ? p->arg_b : NULL;
    return csnarray_pad_k_init_helper(csound, &p->h, &p->k_data, p->handle, p->source_handle, p->before, p->after, p->value, (double) *p->value, NULL, NULL, CSN_REAL, axis_in);
}

int32_t csnarray_padcomp_k_init(CSOUND *csound, CSN_PADCOMPLEX *p) {
    const cs_float *axis_in = p->INOCOUNT > 5 ? p->arg_b : NULL;
    return csnarray_pad_k_init_helper(csound, &p->h, &p->k_data, p->handle, p->source_handle, p->before, p->after, NULL, 0.0, p->value, NULL, CSN_COMPLEX, axis_in);
}

/* The fill value is folded into the two scalars of the elementwise check (the
   imaginary part is 0 for a real pad); the axis and the widths are compared
   on their own. */
int32_t csnarray_pad_k_helper(CSOUND *csound, OPDS *h, K_DATA *k_data, CSN_ARRAY *out_arr, CSN_ARRAY *source_arr, const cs_float *in_before, const cs_float *in_after, double value, COMPLEXDAT *valuecomp, STRINGDAT *valuestring, ITEM_TYPE expected_itype, const cs_float *axis_in, const cs_float *trig) {
    CSN_REGISTRY *reg = k_data->registry;
    CHECK_REGISTRY(csound, h, reg);
    CHECK_KTRIG(trig);

    int32_t res = OK;
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { out_arr, source_arr }, 2);

    int32_t axis = 0;
    uint32_t before = 0;
    uint32_t after = 0;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    res = pad_body(csound, h, source_arr, axis_in, in_before, in_after, &axis, &before, &after, new_shape, expected_itype);
    if (res != OK) goto done;

    double fill_re = value, fill_im = 0.0;
    if (valuecomp != NULL) complexdat_to_rect(valuecomp, &fill_re, &fill_im);
    if (source_arr->itype == CSN_STRING) {
        if (CAN_REUSE_STRING_ELEMENTWISE(k_data, source_arr, NULL, out_arr, valuestring->data, NULL)
            && k_data->prev_axis_i == axis && k_data->prev_index == before && k_data->prev_roll_shift == (int32_t) after) {
            goto done;
        }
    } else {
        if (CAN_REUSE_ELEMENTWISE(k_data, source_arr, NULL, out_arr, fill_re, fill_im)
            && k_data->prev_axis_i == axis && k_data->prev_index == before && k_data->prev_roll_shift == (int32_t) after) {
            goto done;
        }
    }

    /* Every cell of the padded shape is written below, so the logical size is
       the physical one even when the source is logically empty. */
    const CSN_ARRAY *ops[1] = { source_arr };
    CSNARR_RES out = prepare_output(csound, h, h, reg, out_arr, source_arr->ndim, new_shape, source_arr->itype, ops, 1, CSN_ALIAS_REFUSE | CSN_OUTPUT_FULL);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    if (out_arr->itype != CSN_STRING) {
        res = pad_assign_value(source_arr, out_arr, value, valuecomp, axis, before);
    } else {
        res = pad_assign_string_value(csound, source_arr, out_arr, valuestring, axis, before);
    }

    if (res != OK) {
        res = csn_locked_perf_error(csound, h, "[csnarray] Invalid pad iterator layout");
        goto done;
    }

    SET_KDATA_END(*k_data, new_shape, out_arr->ndim, out_arr->itype);
    if (out_arr->itype == CSN_STRING) {
        PUBLISH_STRING_ELEMENTWISE(csound, k_data, source_arr, NULL, out_arr, valuestring->data, NULL);
    } else {
        PUBLISH_ELEMENTWISE(k_data, source_arr, NULL, out_arr, fill_re, fill_im);
    }

    k_data->prev_axis_i = axis;
    k_data->prev_index = before;
    k_data->prev_roll_shift = (int32_t) after;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_pad_k(CSOUND *csound, CSN_PAD *p) {
    const cs_float *axis_in = p->INOCOUNT > 5 ? p->arg_b : NULL;
    return csnarray_pad_k_helper(csound, &p->h, &p->k_data, p->handle, p->source_handle, p->before, p->after, (double) *p->value, NULL, NULL, CSN_REAL, axis_in, p->arg_a);
}

int32_t csnarray_padcomp_k(CSOUND *csound, CSN_PADCOMPLEX *p) {
    const cs_float *axis_in = p->INOCOUNT > 5 ? p->arg_b : NULL;
    return csnarray_pad_k_helper(csound, &p->h, &p->k_data, p->handle, p->source_handle, p->before, p->after, 0.0, p->value, NULL, CSN_COMPLEX, axis_in, p->arg_a);
}

int32_t csnarray_pad(CSOUND *csound, CSN_PAD *p) {
    const cs_float *axis = p->INOCOUNT > 4 ? p->arg_a : NULL;
    return csnarray_pad_helper(csound, &p->h, p->handle, p->source_handle, p->before, p->after, (double) *p->value, NULL, NULL, CSN_REAL, axis);
}

int32_t csnarray_padcomp(CSOUND *csound, CSN_PADCOMPLEX *p) {
    const cs_float *axis = p->INOCOUNT > 4 ? p->arg_a : NULL;
    return csnarray_pad_helper(csound, &p->h, p->handle, p->source_handle, p->before, p->after, 0.0, p->value, NULL, CSN_COMPLEX, axis);
}

int32_t csnarray_pad_in_helper(CSOUND *csound, OPDS *h, CSN_ARRAY *source_arr, const cs_float *in_before, const cs_float *in_after, double value, COMPLEXDAT *valuecomp, STRINGDAT *valuestring, ITEM_TYPE expected_itype, const cs_float *axis_in) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    CSN_ARRAY *temp = NULL;
    stamp_globals(csound, h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { source_arr }, 1);

    int32_t axis = 0;
    uint32_t before = 0;
    uint32_t after = 0;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    res = pad_body(csound, NULL, source_arr, axis_in, in_before, in_after, &axis, &before, &after, new_shape, expected_itype);
    if (res != OK) goto done;

    uint32_t source_ndim = source_arr->ndim;

    /* every cell moves, so the image is built aside and copied back */
    temp = csound->Calloc(csound, sizeof(CSN_ARRAY));
    if (temp == NULL) {
        res = csn_init_error(csound, "[csnarray] Out of memory: allocation of %zu bytes failed", (size_t) (sizeof(CSN_ARRAY)));
        goto done;
    }

    if (allocate_array(csound, temp, source_ndim, new_shape, source_arr->itype) != OK) {
        char tbuf[CSN_SHAPE_STR_MAX];
        res = csn_init_error(csound, "[csnarray] Out of memory: could not allocate the %u-D temporary array %s", source_ndim, shape_str(tbuf, sizeof(tbuf), new_shape, source_ndim));
        goto done;
    }

    if (source_arr->itype == CSN_STRING) {
        res = pad_assign_string_value(csound, source_arr, temp, valuestring, axis, before);
    } else {
        res = pad_assign_value(source_arr, temp, value, valuecomp, axis, before);
    }

    if (res != OK) {
        res = csn_init_error(csound, "[csnarray] Invalid pad iterator layout");
        goto done;
    }

    res = ensure_mutation_capacity(csound, NULL, source_arr, temp->size, false);
    if (res != OK) goto done;
    if (source_arr->itype == CSN_STRING) {
        move_csnstrings(csound, source_arr, temp);
    } else {
        travase_csnarray(source_arr, temp);
    }

done:
    free_private_array(csound, &temp);
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_pad_in(CSOUND *csound, CSN_PAD_IN *p) {
    const cs_float *axis = p->INOCOUNT > 4 ? p->arg_a : NULL;
    return csnarray_pad_in_helper(csound, &p->h, p->source_handle, p->before, p->after, (double) *p->value, NULL, NULL, CSN_REAL, axis);
}

int32_t csnarray_padcomp_in(CSOUND *csound, CSN_PADCOMPLEX_IN *p) {
    const cs_float *axis = p->INOCOUNT > 4 ? p->arg_a : NULL;
    return csnarray_pad_in_helper(csound, &p->h, p->source_handle, p->before, p->after, 0.0, p->value, NULL, CSN_COMPLEX, axis);
}

/* The in-place k-rate pads build the padded image in a per-instance scratch
   array and then overwrite the source, which keeps its identity. */
int32_t csnarray_pad_in_k_scratch_init(CSOUND *csound, OPDS *h, CSN_ARRAY *source_arr, ITEM_TYPE expected_itype, CSN_REGISTRY **out_registry, CSN_ARRAY **out_scratch) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    csn_mark_krate(source_arr);

    int32_t res = OK;
    free_private_array(csound, out_scratch);
    CSN_ARRAY *temp = NULL;
    stamp_globals(csound, h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { source_arr }, 1);

    if (source_arr->itype != expected_itype) {
        res = pad_type_error(csound, NULL, source_arr->itype, expected_itype);
        goto done;
    }

    temp = csound->Calloc(csound, sizeof(CSN_ARRAY));
    if (temp == NULL) {
        res = csn_init_error(csound, "[csnarray] Out of memory: allocation of %zu bytes failed", (size_t) (sizeof(CSN_ARRAY)));
        goto done;
    }

    /* The widths are k-rate, so the scratch starts at the source shape and is
       grown by the perf pass whenever the requested padding needs more room.
       It starts at least as large as the source's capacity: the image lands
       back in the source, so a marked source is refused by its own capacity
       and never earlier by this scratch. */
    if (allocate_array(csound, temp, source_arr->ndim, source_arr->shape, source_arr->itype) != OK) {
        char tbuf[CSN_SHAPE_STR_MAX];
        res = csn_init_error(csound, "[csnarray] Out of memory: could not allocate the %u-D temporary array %s", source_arr->ndim, shape_str(tbuf, sizeof(tbuf), source_arr->shape, source_arr->ndim));
        goto done;
    }
    res = ensure_mutation_capacity(csound, NULL, temp, source_arr->capacity, false);
    if (res != OK) goto done;

    *out_scratch = temp;
    temp = NULL;
    *out_registry = reg;

done:
    free_private_array(csound, &temp);
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_pad_in_k_init(CSOUND *csound, CSN_PAD_IN *p) {
    return csnarray_pad_in_k_scratch_init(csound, &p->h, p->source_handle, CSN_REAL, &p->registry, &p->scratch);
}

int32_t csnarray_padcomp_in_k_init(CSOUND *csound, CSN_PADCOMPLEX_IN *p) {
    return csnarray_pad_in_k_scratch_init(csound, &p->h, p->source_handle, CSN_COMPLEX, &p->registry, &p->scratch);
}

int32_t csnarray_pad_in_k_deinit(CSOUND *csound, CSN_PAD_IN *p) {
    free_private_array(csound, &p->scratch);
    return OK;
}

int32_t csnarray_padcomp_in_k_deinit(CSOUND *csound, CSN_PADCOMPLEX_IN *p) {
    free_private_array(csound, &p->scratch);
    return OK;
}

/* Fits the scratch to the padded shape, fills it and copies it back over the
   source array. */
static int32_t pad_in_k_commit(CSOUND *csound, OPDS *h, CSN_ARRAY *temp, CSN_ARRAY *source_arr, const uint32_t *new_shape, double value, COMPLEXDAT *valuecomp, STRINGDAT *valuestring, int32_t axis, uint32_t before) {
    int32_t res = fit_private_array(csound, h, temp, source_arr->ndim, new_shape, source_arr->itype, source_arr->rt_locked);
    if (res != OK) return res;

    /* The source only reallocates when the padded image outgrows it, never on
       a pass that still fits. */
    res = ensure_mutation_capacity(csound, h, source_arr, temp->size, source_arr->rt_locked);
    if (res != OK) return res;

    if (source_arr->itype == CSN_STRING) {
        if (pad_assign_string_value(csound, source_arr, temp, valuestring, axis, before) != OK) {
            return csn_locked_perf_error(csound, h, "[csnarray] Invalid pad iterator layout");
        }
        /* the scratch hands its strings over and is left with none */
        move_csnstrings(csound, source_arr, temp);
        return OK;
    }

    if (pad_assign_value(source_arr, temp, value, valuecomp, axis, before) != OK) {
        return csn_locked_perf_error(csound, h, "[csnarray] Invalid pad iterator layout");
    }
    travase_csnarray(source_arr, temp);
    return OK;
}

int32_t csnarray_pad_in_k_helper(CSOUND *csound, OPDS *h, CSN_REGISTRY *reg, CSN_ARRAY *scratch, K_DATA *k_data, CSN_ARRAY *source_arr, const cs_float *in_before, const cs_float *in_after, double value, COMPLEXDAT *valuecomp, STRINGDAT *valuestring, ITEM_TYPE expected_itype, const cs_float *axis_in, const cs_float *trig) {
    CHECK_REGISTRY(csound, h, reg);
    if (scratch == NULL) {
        return csn_perf_error(csound, h, "[csnarray] Internal error: temporary buffer not available");
    }
    CHECK_KTRIG(trig);

    int32_t res = OK;
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { source_arr }, 1);

    int32_t axis = 0;
    uint32_t before = 0;
    uint32_t after = 0;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    res = pad_body(csound, h, source_arr, axis_in, in_before, in_after, &axis, &before, &after, new_shape, expected_itype);
    if (res != OK) goto done;

    /* The pad is not idempotent, so it runs once per write by someone else:
       its own write is recorded below and recognized on the next pass. */
    double fill_re = value, fill_im = 0.0;
    if (valuecomp != NULL) complexdat_to_rect(valuecomp, &fill_re, &fill_im);
    if (expected_itype == CSN_STRING) {
        if (CAN_REUSE_STRING_ELEMENTWISE(k_data, source_arr, NULL, NULL, valuestring->data, NULL)
            && k_data->prev_axis_i == axis && k_data->prev_index == before && k_data->prev_roll_shift == (int32_t) after) {
            goto done;
        }
    } else {
        if (CAN_REUSE_ELEMENTWISE(k_data, source_arr, NULL, NULL, fill_re, fill_im)
            && k_data->prev_axis_i == axis && k_data->prev_index == before && k_data->prev_roll_shift == (int32_t) after) {
            goto done;
        }
    }

    res = pad_in_k_commit(csound, h, scratch, source_arr, new_shape, value, valuecomp, valuestring, axis, before);
    if (res != OK) goto done;
    if (source_arr->itype == CSN_STRING) {
        PUBLISH_STRING_ELEMENTWISE(csound, k_data, source_arr, NULL, NULL, valuestring->data, NULL);
    } else {
        PUBLISH_ELEMENTWISE(k_data, source_arr, NULL, NULL, fill_re, fill_im);
    }
    k_data->prev_axis_i = axis;
    k_data->prev_index = before;
    k_data->prev_roll_shift = (int32_t) after;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_pad_in_k(CSOUND *csound, CSN_PAD_IN *p) {
    const cs_float *axis_in = p->INOCOUNT > 5 ? p->arg_b : NULL;
    return csnarray_pad_in_k_helper(csound, &p->h, p->registry, p->scratch, &p->k_data, p->source_handle, p->before, p->after, (double) *p->value, NULL, NULL, CSN_REAL, axis_in, p->arg_a);
}

int32_t csnarray_padcomp_in_k(CSOUND *csound, CSN_PADCOMPLEX_IN *p) {
    const cs_float *axis_in = p->INOCOUNT > 5 ? p->arg_b : NULL;
    return csnarray_pad_in_k_helper(csound, &p->h, p->registry, p->scratch, &p->k_data, p->source_handle, p->before, p->after, 0.0, p->value, NULL, CSN_COMPLEX, axis_in, p->arg_a);
}

/* Reports whether anything actually moved. The loop already reads every
   element, so an in-place caller gets an exact answer for free and can leave
   the array's version alone on the passes that clip nothing — a k-rate
   csnclip.in on a settled array would otherwise announce a new generation to
   every consumer on every pass. */
/* min(max(x, lo), hi), as np.clip: with min > max every value becomes max.
   Branch-free, so the loop vectorizes; a NaN fails both tests and is left as
   it is. */
static bool clip_value(double min_value, double max_value, CSN_ARRAY *arr) {
    size_t changed = 0;
    double *d = arr->data;
    for (size_t i = 0; i < arr->size; ++i) {
        double value = d[i];
        bool lo = value < min_value;
        double v = lo ? min_value : value;
        bool hi = v > max_value;
        v = hi ? max_value : v;
        changed |= (size_t) (lo | hi);
        d[i] = v;
    }
    return changed != 0;
}

/* Clipping never moves a value out of its cell, so the output may be its own
   source (ELEMENTWISE): there is then nothing to copy, and since prepare_output
   left it alone, its version moves here, and only if a value was actually
   clipped. A separate output already got its version from prepare_output. */
static void clip_into(CSN_ARRAY *out_arr, const CSN_ARRAY *source_arr, bool aliased, double min_value, double max_value) {
    if (!aliased) {
        out_arr->size = source_arr->size;
        memcpy(out_arr->data, source_arr->data, sizeof(double) * source_arr->size);
    }
    bool changed = clip_value(min_value, max_value, out_arr);
    if (aliased && changed) update_array_data_version(&out_arr->version);
}

int32_t csnarray_clip(CSOUND *csound, CSN_CLIP *p) {
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
    CSNARR_RES out = prepare_output(csound, &p->h, NULL, reg, p->handle, source_arr->ndim, source_arr->shape, CSN_REAL, ops, 1, CSN_ALIAS_ELEMENTWISE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    clip_into(p->handle, source_arr, out.aliased, (double) *p->min, (double) *p->max);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

/* The bounds are k-arguments, still at their init values here, so the output
   only takes the source's layout and contents; the first pass clips. */
int32_t csnarray_clip_k_init(CSOUND *csound, CSN_CLIP *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle }, 2);

    CSN_ARRAY *source_arr = p->source_handle;
    const CSN_ARRAY *ops[1] = { source_arr };
    CSNARR_RES out = prepare_output(csound, &p->h, NULL, reg, p->handle, source_arr->ndim, source_arr->shape, source_arr->itype, ops, 1, CSN_ALIAS_ELEMENTWISE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    memset(&p->k_data, 0, sizeof(p->k_data));
    SET_KDATA_BEGIN(p->k_data, reg, source_arr->shape, source_arr->ndim, source_arr->itype);

    /* Clipped here only with bounds known at init; an output that is its own
       source is clipped by every perf pass. Otherwise the layout is the
       source's and the values are left to the first pass. */
    if (out.aliased || source_arr->itype != CSN_REAL) goto done;
    if (!csn_known_at_init(csound, p->min) || !csn_known_at_init(csound, p->max)) goto done;
    double min_value = (double) *p->min;
    double max_value = (double) *p->max;
    clip_into(p->handle, source_arr, false, min_value, max_value);
    PUBLISH_ELEMENTWISE(&p->k_data, source_arr, NULL, p->handle, min_value, max_value);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_clip_k(CSOUND *csound, CSN_CLIP *p) {
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

    double min_value = (double) *p->min;
    double max_value = (double) *p->max;
    /* never true for an output that is its own source: clipping it again is
       cheap and leaves the version alone when nothing moves */
    if (CAN_REUSE_ELEMENTWISE(&p->k_data, source_arr, NULL, p->handle, min_value, max_value)) goto done;

    const CSN_ARRAY *ops[1] = { source_arr };
    CSNARR_RES out = prepare_output(csound, &p->h, &p->h, reg, p->handle, source_arr->ndim, source_arr->shape, CSN_REAL, ops, 1, CSN_ALIAS_ELEMENTWISE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    clip_into(p->handle, source_arr, out.aliased, min_value, max_value);
    SET_KDATA_END(p->k_data, source_arr->shape, source_arr->ndim, source_arr->itype);
    PUBLISH_ELEMENTWISE(&p->k_data, source_arr, NULL, p->handle, min_value, max_value);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_clip_in(CSOUND *csound, CSN_CLIP_IN *p) {
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

    if (clip_value((double) *p->min, (double) *p->max, source_arr)) {
        update_array_data_version(&source_arr->version);
    }

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_clip_in_k_init(CSOUND *csound, CSN_CLIP_IN *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    csn_mark_krate(p->source_handle);
    p->registry = reg;
    stamp_globals(csound, &p->h);
    return OK;
}

int32_t csnarray_clip_in_k(CSOUND *csound, CSN_CLIP_IN *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);
    CHECK_KTRIG(p->trig);

    int32_t res = OK;
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);

    CSN_ARRAY *source_arr = p->source_handle;
    if (source_arr->itype != CSN_REAL) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] This operation is not implemented for complex arrays");
        goto done;
    }

    double min_value = (double) *p->min;
    double max_value = (double) *p->max;
    if (CAN_REUSE_ELEMENTWISE(&p->k_data, source_arr, NULL, NULL, min_value, max_value)) goto done;

    if (clip_value(min_value, max_value, source_arr)) {
        update_array_data_version(&source_arr->version);
    }
    PUBLISH_ELEMENTWISE(&p->k_data, source_arr, NULL, NULL, min_value, max_value);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}
