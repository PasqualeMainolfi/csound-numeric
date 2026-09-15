/* Opcode implementations for the index family.
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

static int32_t get_index_offset(CSOUND *csound, OPDS *perf_h, size_t *offset, uint32_t ndim, const CSN_ARRAY *arr, const MYFLT *indexes) {
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

static int32_t csnarray_resolve_item(
    CSOUND *csound,
    OPDS *perf_h,
    CSN_REGISTRY *reg,
    uint32_t handle,
    ARRAYDAT *indexes,
    ITEM_TYPE expected_itype,
    CSN_ARRAY **out_arr,
    size_t *out_offset
) {
    CSN_SLOT *slot = get_slot(reg, handle);
    if (slot == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", handle);
    }

    CSN_ARRAY *arr = slot->array;
    uint32_t ndim = arr->ndim;

    if (arr->itype != expected_itype) {
        if (expected_itype == CSN_COMPLEX) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Handle holds a real array; use the real form of the accessor");
        }
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Handle holds a complex array; declare the value as :Complex; to read or write it");
    }

    if (indexes == NULL || indexes->data == NULL || indexes->sizes == NULL || indexes->dimensions != 1 || indexes->sizes[0] != (int32_t) ndim) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Index argument must be a 1-D array of exactly %u elements, one per dimension", ndim);
    }

    int32_t res = get_index_offset(csound, perf_h, out_offset, ndim, arr, indexes->data);
    if (res != OK) {
        return res;
    }

    if (*out_offset >= arr->size) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Index resolves to offset %zu, outside the logical array size %zu", *out_offset, arr->size);
    }

    *out_arr = arr;
    return OK;
}

static int32_t csnarray_get_set_locked(CSOUND *csound, OPDS *perf_h, CSN_REGISTRY *reg, uint32_t handle, ARRAYDAT *indexes, MYFLT *value, bool is_get) {
    if (value == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Internal error: null value pointer passed to the element accessor");
    }

    CSN_ARRAY *arr = NULL;
    size_t offset = 0;
    int32_t res = csnarray_resolve_item(csound, perf_h, reg, handle, indexes, CSN_REAL, &arr, &offset);
    if (res != OK) {
        return res;
    }

    if (is_get) {
        *value = (MYFLT) arr->data[offset];
    } else {
        /* Compared first, because a k-rate csnset that stores the same value
           every pass must not look like a write. Feeding an array to an
           in-place opcode that is not idempotent (csnflip.in, csnroll.in) the
           array would otherwise be flipped back and forth forever. One
           comparison, not a scan: this accessor only ever touches one item. */
        double stored = (double) *value;
        if (arr->data[offset] != stored) {
            arr->data[offset] = stored;
            update_array_data_version(&arr->version);
        }
    }

    return OK;
}

static int32_t csnarray_get_set_complex_locked(CSOUND *csound, OPDS *perf_h, CSN_REGISTRY *reg, uint32_t handle, ARRAYDAT *indexes, COMPLEXDAT *value, bool is_get) {
    if (value == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Internal error: null value pointer passed to the element accessor");
    }

    CSN_ARRAY *arr = NULL;
    size_t offset = 0;
    int32_t res = csnarray_resolve_item(csound, perf_h, reg, handle, indexes, CSN_COMPLEX, &arr, &offset);
    if (res != OK) {
        return res;
    }

    size_t at = offset * 2;
    if (is_get) {
        value->real = (MYFLT) arr->data[at];
        value->imag = (MYFLT) arr->data[at + 1];
        value->isPolar = 0;
    } else {
        double re, im;
        complexdat_to_rect(value, &re, &im);
        if (arr->data[at] != re || arr->data[at + 1] != im) {
            arr->data[at] = re;
            arr->data[at + 1] = im;
            update_array_data_version(&arr->version);
        }
    }

    return OK;
}

int32_t csnarray_get(CSOUND *csound, CSN_GET *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    uint32_t handle = p->source_handle->id;
    csound->LockMutex(reg->mutex);
    int32_t res = csnarray_get_set_locked(csound, NULL, reg, handle, p->indexes, p->value, true);
    p->k_data.registry = reg;
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_get_k(CSOUND *csound, CSN_GET *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    uint32_t handle = p->source_handle->id;
    csound->LockMutex(reg->mutex);
    int32_t res = csnarray_get_set_locked(csound, &p->h, reg, handle, p->indexes, p->value, true);
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_get_complex(CSOUND *csound, CSN_GETCOMPLEX *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t handle = p->source_handle->id;

    csound->LockMutex(reg->mutex);
    int32_t res = csnarray_get_set_complex_locked(csound, NULL, reg, handle, p->indexes, p->value, true);
    p->k_data.registry = reg;
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_get_complex_k(CSOUND *csound, CSN_GETCOMPLEX *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    uint32_t handle = p->source_handle->id;

    csound->LockMutex(reg->mutex);
    int32_t res = csnarray_get_set_complex_locked(csound, &p->h, reg, handle, p->indexes, p->value, true);
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_set(CSOUND *csound, CSN_SET *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t handle = p->source_handle->id;

    csound->LockMutex(reg->mutex);
    int32_t res = csnarray_get_set_locked(csound, NULL, reg, handle, p->indexes, p->value, false);
    p->k_data.registry = reg;
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_set_k(CSOUND *csound, CSN_SET *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    uint32_t handle = p->source_handle->id;

    csound->LockMutex(reg->mutex);
    int32_t res = csnarray_get_set_locked(csound, &p->h, reg, handle, p->indexes, p->value, false);
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_set_complex(CSOUND *csound, CSN_SETCOMPLEX *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t handle = p->source_handle->id;

    csound->LockMutex(reg->mutex);
    int32_t res = csnarray_get_set_complex_locked(csound, NULL, reg, handle, p->indexes, p->value, false);
    p->k_data.registry = reg;
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_set_complex_k(CSOUND *csound, CSN_SETCOMPLEX *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, &p->h, reg);

    uint32_t handle = p->source_handle->id;

    csound->LockMutex(reg->mutex);
    int32_t res = csnarray_get_set_complex_locked(csound, &p->h, reg, handle, p->indexes, p->value, false);
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_get_rowcol_helper(CSOUND *csound, CSN_GET_ROWCOL *p, bool is_row) {
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
    uint32_t source_dim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;
    size_t *source_strides = source_arr->strides;
    if (source_dim != 2U) {
        res = csound->InitError(csound, "[csnarray] get row/col requires 2D array");
        goto done;
    }

    if (!IS_VALID_INDEX((double) *p->index)) {
        res = csound->InitError(csound, "[csnarray] Not valid index");
        goto done;
    }

    uint32_t n_items = is_row ? source_shape[0] : source_shape[1];
    uint32_t index = (uint32_t) *p->index;
    if (index >= n_items) {
        res = csound->InitError(csound, "[csnarray] Index out of bounds");
        goto done;
    }

    uint32_t new_dim = 1U;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    new_shape[0] = is_row ? source_shape[1] : source_shape[0];
    ITEM_TYPE itype = source_arr->itype;

    uint32_t protect[1] = { source_handle };
    if (create_csnarray_locked(csound, reg, &p->h, new_dim, new_shape, &p->array, p->handle, protect, 1U, &err, itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;
    for (size_t i = 0; i < p->array->size; i++) {
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
    csound->UnlockMutex(reg->mutex);
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
    uint32_t source_dim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;
    if (source_dim != 2U) {
        res = csound->InitError(csound, "[csnarray] get row/col requires 2D array");
        goto done;
    }

    uint32_t shape[CSN_MAX_DIMS] = {0};
    shape[0] = is_row ? source_shape[1] : source_shape[0];

    uint32_t protect[1] = { source_handle };
    if (create_csnarray_locked(csound, reg, &p->h, 1U, shape, &p->array, p->handle, protect, 1U, &err, source_arr->itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    reset_empty_csnarray(p->array, 1U, shape, source_arr->itype);

    CSN_SLOT *out_slot = get_slot(reg, p->handle->id);
    if (out_slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", p->handle->id);
        goto done;
    }

    SET_KDATA_BEGIN(p, reg);
    set_array_version(&p->k_data.prev_output_version, &out_slot->array->version);
    set_array_version(&p->k_data.prev_source_version, &source_arr->version);
    p->is_published = false;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_get_rowcol_k_helper(CSOUND *csound, CSN_GET_ROWCOL *p, bool is_row) {
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
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
    }

    CSN_ARRAY *source_arr = slot->array;
    uint32_t source_dim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;
    size_t *source_strides = source_arr->strides;
    if (source_dim != 2U) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] get row/col requires 2D array");
    }

    if (!IS_VALID_INDEX((double) *p->index)) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Not valid index");
    }

    uint32_t n_items = is_row ? source_shape[0] : source_shape[1];
    uint32_t index = (uint32_t) *p->index;
    if (index >= n_items) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Index out of bounds");
    }

    if (p->is_published) {
        bool is_same_source = is_same_array_version(&p->k_data.prev_source_version, &source_arr->version);
        bool is_same_index = index == p->k_data.prev_index;
        bool is_same_result = false;
        CSN_SLOT *res_slot = get_slot(reg, owned_handle);
        if (res_slot != NULL) {
            is_same_result = is_same_array_version(&p->k_data.prev_output_version, &res_slot->array->version);
        }

        if (is_same_index && is_same_source && is_same_result) {
            p->handle->id = owned_handle;
            goto done;
        }
    }

    uint32_t new_dim = 1U;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    new_shape[0] = is_row ? source_shape[1] : source_shape[0];
    ITEM_TYPE itype = source_arr->itype;

    size_t req_size = 0;
    if (get_array_size_from_shape(&req_size, new_dim, new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSN_ARRAY *arr = NULL;
    size_t logical_size = source_arr->size == 0 ? 0 : req_size;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, new_dim, new_shape, logical_size, itype, err);
    if (res != OK) goto done;

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
    p->array = arr;

    update_array_data_version(&p->array->version);
    SET_KDATA_END(p, new_shape, 1U, itype);
    set_array_version(&p->k_data.prev_source_version, &source_arr->version);
    set_array_version(&p->k_data.prev_output_version, &p->array->version);
    p->k_data.prev_index = index;
    p->is_published = true;

done:
    csound->UnlockMutex(reg->mutex);
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

static int32_t check_take_flat_body(CSOUND *csound, OPDS *perf_h, CSN_REGISTRY *reg, CSN_SLOT **slot, CSN_ARRAY **arr, uint32_t handle, double index, bool is_complex) {
    *slot = get_slot(reg, handle);
    if ((*slot) == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", handle);
    }

    *arr = (*slot)->array;
    if (!is_complex) {
        if ((*arr)->itype == CSN_COMPLEX) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Handle holds a complex array; declare the taken value as :Complex;");
        }
    } else {
        if ((*arr)->itype != CSN_COMPLEX) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Handle holds a real array; declare the taken value as i or k");
        }
    }
    if (!IS_VALID_INDEX(index)) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Index %g is invalid; indexes must be finite non-negative integers", index);
    }

    uint32_t valid_index = (uint32_t) index;

    if (valid_index >= (*arr)->size) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Index %u is out of range", valid_index);
    }

    return OK;
}

static int32_t check_take_body(CSOUND *csound, OPDS *perf_h, CSN_REGISTRY *reg, CSN_SLOT **slot, CSN_ARRAY **arr, uint32_t *shape, uint32_t *out_ndim, uint32_t *out_axis, uint32_t *out_index, uint32_t handle, double index, double in_axis) {
    *slot = get_slot(reg, handle);
    if ((*slot) == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", handle);
    }

    *arr = (*slot)->array;
    uint32_t ndim = (*arr)->ndim;

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

    if (valid_index >= (*arr)->shape[axis]) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Index %u is out of range", valid_index);
    }
    *out_index = valid_index;

    // remove axis passed for new shape
    *out_ndim = ndim - 1;
    for (uint32_t i = 0, j = 0; i < ndim; i++) {
        if (i == axis) continue;
        shape[j++] = (*arr)->shape[i];
    }
    return OK;
}

static void take_assign_value(CSN_ARRAY *source, CSN_ARRAY *destination, uint32_t in_ndim, uint32_t out_ndim, uint32_t axis, uint32_t index) {
    for (size_t linear = 0; linear < destination->size; ++linear) {
        uint32_t dst_coords[CSN_MAX_DIMS] = {0};
        uint32_t src_coords[CSN_MAX_DIMS] = {0};

        from_linear_to_coords(dst_coords, destination->shape, linear, out_ndim);

        uint32_t k = 0;
        for (uint32_t i = 0; i < in_ndim; ++i) {
            src_coords[i] = (i == axis) ? index : dst_coords[k++];
        }

        size_t src_index = from_coords_to_offset(src_coords, source->strides, in_ndim);
        if (source->itype == CSN_COMPLEX) {
            destination->data[linear * 2] = source->data[src_index * 2];
            destination->data[linear * 2 + 1] = source->data[src_index * 2 + 1];
        } else {
            destination->data[linear] = source->data[src_index];
        }
    }
}

int32_t csnarray_take(CSOUND *csound, CSN_TAKE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;
    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = NULL;
    CSN_ARRAY *arr = NULL;
    uint32_t out_ndim = 0;
    uint32_t axis = 0;
    uint32_t index = 0;
    uint32_t shape[CSN_MAX_DIMS] = {0};
    res = check_take_body(csound, NULL, reg, &slot, &arr, shape, &out_ndim, &axis, &index, source_handle, (double) *p->index, (double) *p->axis);
    if (res != OK) goto done;

    /* _locked: the registry mutex is already held and is not recursive. */
    if (create_csnarray_locked(csound, reg, &p->h, out_ndim, shape, &p->array, p->handle, &source_handle, 1U, &err, arr->itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *dst = p->array;
    dst->size = arr->size == 0 ? 0 : dst->size;

    /* Walk the destination, which is smaller than the source by exactly the
       extent of the dropped axis. */
    take_assign_value(arr, dst, arr->ndim, out_ndim, axis, index);
    SET_KDATA_BEGIN(p, reg);
    p->k_data.prev_axis_u = axis;
    p->k_data.prev_index = index;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_take_k(CSOUND *csound, CSN_TAKE *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REG_HANDLE(csound, &p->h, reg, p->k_data.owned_handle);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = CHECK_SELF_ALIAS(csound, &p->h, &p->k_data, source_handle, 0);
    if (res != OK) return res;

    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = NULL;
    CSN_ARRAY *arr = NULL;
    uint32_t out_ndim = 0;
    uint32_t axis = 0;
    uint32_t index = 0;
    uint32_t shape[CSN_MAX_DIMS] = {0};
    res = check_take_body(csound, &p->h, reg, &slot, &arr, shape, &out_ndim, &axis, &index, source_handle, (double) *p->index, (double) *p->axis);
    if (res != OK) goto done;

    uint32_t ndim = arr->ndim;
    ITEM_TYPE itype = arr->itype;

    CSN_ARRAY *dst = p->array;
    size_t output_size = 0;
    if (get_array_size_from_shape(&output_size, out_ndim, shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }
    size_t logical_size = arr->size == 0 ? 0 : output_size;
    CSN_SLOT *reuse_slot = get_slot(reg, p->k_data.owned_handle);
    if (reuse_slot != NULL && CAN_REUSE_ELEMENTWISE(&p->k_data, source_handle, arr, 0, NULL, reuse_slot->array, (double) axis, (double) index)) {
        p->handle->id = p->k_data.owned_handle;
        goto done;
    }

    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &dst, &p->k_data, NULL, out_ndim, shape, logical_size, itype, err);
    if (res != OK) goto done;

    take_assign_value(arr, dst, ndim, out_ndim, axis, index);
    SET_KDATA_END(p, shape, out_ndim, itype);
    p->k_data.prev_axis_u = axis;
    p->k_data.prev_index = index;
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle, arr, 0, NULL, dst, (double) axis, (double) index);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

/* Two-argument form: indexes the flattened array and yields a scalar, matching
   np.take(a, i) with axis=None. This is the rank-1 case the axis form cannot
   express, since dropping the only axis would leave nothing behind. */
int32_t csnarray_take_flat(CSOUND *csound, CSN_TAKE_FLAT *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;
    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = NULL;
    CSN_ARRAY *arr = NULL;
    res = check_take_flat_body(csound, NULL, reg, &slot, &arr, source_handle, (double) *p->index, false);
    if (res != OK) goto done;

    *p->value = (MYFLT) arr->data[(size_t) *p->index];
    p->k_data.registry = reg;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_take_flat_k(CSOUND *csound, CSN_TAKE_FLAT *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    uint32_t source_handle = p->source_handle->id;
    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = NULL;
    CSN_ARRAY *arr = NULL;
    res = check_take_flat_body(csound, &p->h, reg, &slot, &arr, source_handle, (double) *p->index, false);
    if (res != OK) goto done;

    *p->value = (MYFLT) arr->data[(size_t) *p->index];
    p->k_data.registry = reg;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_takecomp_flat(CSOUND *csound, CSN_TAKECOMPLEX_FLAT *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;
    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = NULL;
    CSN_ARRAY *arr = NULL;
    res = check_take_flat_body(csound, NULL, reg, &slot, &arr, source_handle, (double) *p->index, true);
    if (res != OK) goto done;

    size_t index = (size_t) *p->index;
    p->value->real = (MYFLT) arr->data[index * 2];
    p->value->imag = (MYFLT) arr->data[index * 2 + 1];
    p->value->isPolar = 0;
    p->k_data.registry = reg;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_takecomp_flat_k(CSOUND *csound, CSN_TAKECOMPLEX_FLAT *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    uint32_t source_handle = p->source_handle->id;
    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = NULL;
    CSN_ARRAY *arr = NULL;
    res = check_take_flat_body(csound, &p->h, reg, &slot, &arr, source_handle, (double) *p->index, true);
    if (res != OK) goto done;

    size_t index = (size_t) *p->index;
    p->value->real = (MYFLT) arr->data[index * 2];
    p->value->imag = (MYFLT) arr->data[index * 2 + 1];
    p->value->isPolar = 0;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t validate_slice_spec(
    CSOUND *csound,
    OPDS *perf_h,
    const CSN_ARRAY *array,
    double axis_value,
    double start_value,
    double stop_value,
    double step_value,
    uint32_t *out_axis,
    uint32_t *out_start,
    uint32_t *out_step,
    uint32_t *out_shape,
    size_t *out_size
) {
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

static void slice_get_assign_value(const CSN_ARRAY *source, CSN_ARRAY *destination, uint32_t ndim, uint32_t axis, uint32_t start, uint32_t step) {
    for (size_t linear = 0; linear < destination->size; ++linear) {
        uint32_t coords[CSN_MAX_DIMS] = {0};
        from_linear_to_coords(coords, destination->shape, linear, ndim);
        coords[axis] = start + coords[axis] * step;

        size_t source_index = from_coords_to_offset(coords, source->strides, ndim);
        if (source->itype == CSN_COMPLEX) {
            destination->data[linear * 2] = source->data[source_index * 2];
            destination->data[linear * 2 + 1] = source->data[source_index * 2 + 1];
        } else {
            destination->data[linear] = source->data[source_index];
        }
    }
}

static void slice_set_assign_value(const CSN_ARRAY *data, CSN_ARRAY *destination, uint32_t ndim, const uint32_t *slice_shape, uint32_t axis, uint32_t start, uint32_t step) {
    for (size_t linear = 0; linear < data->size; ++linear) {
        uint32_t coords[CSN_MAX_DIMS] = {0};
        from_linear_to_coords(coords, slice_shape, linear, ndim);
        coords[axis] = start + coords[axis] * step;

        size_t destination_index = from_coords_to_offset(coords, destination->strides, ndim);
        if (data->itype == CSN_COMPLEX) {
            destination->data[destination_index * 2] = data->data[linear * 2];
            destination->data[destination_index * 2 + 1] = data->data[linear * 2 + 1];
        } else {
            destination->data[destination_index] = data->data[linear];
        }
    }
}

static int32_t csnarray_get_slice_impl(CSOUND *csound, CSN_GET_SLICE *p, bool is_ktime) {
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
    uint32_t shape[CSN_MAX_DIMS] = {0};

    uint32_t axis = 0;
    uint32_t start = 0;
    uint32_t step = 0;
    size_t output_size = 0;
    /* The k form takes the whole spec at k-rate, so during the init pass it
       normally still reads start=stop=step=0, which no validation can accept.
       Stand in the full extent of axis 0 there: the slot only has to exist with
       a usable layout, and the first performance pass republishes it. */
    double in_start = (double) *p->start;
    double in_stop = (double) *p->stop;
    double in_step = (double) *p->step;
    double in_axis = (double) *p->axis;
    if (is_ktime && in_step == 0.0 && in_start == 0.0 && in_stop == 0.0) {
        in_axis = 0.0;
        in_stop = (double) arr->shape[0];
        in_step = 1.0;
    }
    res = validate_slice_spec(csound, NULL, arr, in_axis, in_start, in_stop, in_step, &axis, &start, &step, shape, &output_size);
    if (res != OK) goto done;

    /* _locked: the registry mutex is already held and is not recursive. */
    if (create_csnarray_locked(csound, reg, &p->h, ndim, shape, &p->array, p->handle, &source_handle, 1U, &err, arr->itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *dst = p->array;
    dst->size = arr->size == 0 ? 0 : output_size;
    slice_get_assign_value(arr, dst, ndim, axis, start, step);
    SET_KDATA_BEGIN(p, reg);
    p->k_data.prev_size = dst->size;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_get_slice(CSOUND *csound, CSN_GET_SLICE *p) {
    return csnarray_get_slice_impl(csound, p, false);
}

int32_t csnarray_get_slice_k_init(CSOUND *csound, CSN_GET_SLICE *p) {
    return csnarray_get_slice_impl(csound, p, true);
}

int32_t csnarray_get_slice_k(CSOUND *csound, CSN_GET_SLICE *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REG_HANDLE(csound, &p->h, reg, p->k_data.owned_handle);

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
    uint32_t shape[CSN_MAX_DIMS] = {0};

    uint32_t axis = 0;
    uint32_t start = 0;
    uint32_t step = 0;
    size_t output_size = 0;
    res = validate_slice_spec(csound, &p->h, arr, (double) *p->axis, (double) *p->start, (double) *p->stop, (double) *p->step, &axis, &start, &step, shape, &output_size);
    if (res != OK) goto done;
    size_t logical_size = arr->size == 0 ? 0 : output_size;

    CSN_ARRAY *dst = p->array;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &dst, &p->k_data, NULL, ndim, shape, logical_size, arr->itype, err);
    if (res != OK) goto done;

    slice_get_assign_value(arr, dst, ndim, axis, start, step);
    SET_KDATA_END(p, shape, ndim, arr->itype);
    p->k_data.prev_size = arr->size;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_set_slice_locked(CSOUND *csound, OPDS *perf_h, CSN_REGISTRY *reg, CSN_SET_SLICE *p) {
    uint32_t source_handle = p->source_handle->id;
    uint32_t data_handle = p->data_handle->id;

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
    }

    CSN_SLOT *data_slot = get_slot(reg, data_handle);
    if (data_slot == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", data_handle);
    }

    CSN_ARRAY *source_arr = source_slot->array;
    CSN_ARRAY *data_arr = data_slot->array;

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

    slice_set_assign_value(data_arr, source_arr, source_ndim, slice_shape, axis, start, step);
    update_array_data_version(&source_arr->version);
    SET_KDATA_WITH_ID_BEGIN(p, reg, slice_shape, data_ndim, source_arr->itype, source_handle);
    p->k_data.owned_data_handle = data_handle;
    return OK;
}

int32_t csnarray_set_slice(CSOUND *csound, CSN_SET_SLICE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    csound->LockMutex(reg->mutex);
    int32_t res = csnarray_set_slice_locked(csound, NULL, reg, p);
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_set_slice_k(CSOUND *csound, CSN_SET_SLICE *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REG_HANDLE(csound, &p->h, reg, p->k_data.owned_handle);

    csound->LockMutex(reg->mutex);
    int32_t res = csnarray_set_slice_locked(csound, &p->h, reg, p);
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t push_check_body(CSOUND *csound, OPDS *perf_h, CSN_SLOT **slot, CSN_ARRAY **arr, CSN_REGISTRY *reg, uint32_t handle, bool is_complex) {
    *slot = get_slot(reg, handle);
    if ((*slot) == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", handle);
    }

    *arr = (*slot)->array;
    if ((*arr)->ndim != 1) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Push needs a 1-D array, got %u-D", (*arr)->ndim);
    }

    if (!is_complex) {
        if ((*arr)->itype != CSN_REAL) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Handle holds a complex array; push a :Complex; value instead");
        }
    } else {
        if ((*arr)->itype != CSN_COMPLEX) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Handle holds a real array; push a float value instead");
        }
    }

    if ((*arr)->size >= CSN_MAX_ELEMS) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Push would exceed the maximum element count: array already holds %zu of %zu elements", (*arr)->size, (size_t) CSN_MAX_ELEMS);
    }
    return OK;
}

bool csn_slot_rt_locked(CSN_REGISTRY *reg, uint32_t handle) {
    CSN_SLOT *slot = get_slot(reg, handle);
    return slot != NULL && slot->rt_locked;
}

/* Registry arrays are named by handle; an opcode's private array has none, so
   it is named by the opcode's output. Csound's own report adds the opcode and
   the line either way. */
int32_t rt_growth_refused(CSOUND *csound, OPDS *perf_h, const CSN_ARRAY *arr, size_t required_size) {
    if (arr->array_id == 0) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] '%s' is on a real-time path and its working buffer cannot grow from %zu to %zu elements at perf time; clear the mark with csnrtunlock, or pass irt=0 at the audio source it descends from", get_out_name(perf_h), arr->capacity, required_size);
    }
    return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Array %u is on a real-time path and cannot grow from %zu to %zu elements at perf time; clear the mark with csnrtunlock, or pass irt=0 at the audio source it descends from", arr->array_id, arr->capacity, required_size);
}

/* rt_locked is the mark of the slot arr belongs to or, for an array private to
   an opcode, of the slot it serves; external_lock covers the private arrays
   that carry their own flag. A marked array must already be large enough at
   perf time: growing it would call the allocator on the audio thread, so the
   growth is refused instead. Init-time growth stays allowed. */
int32_t ensure_mutation_capacity(CSOUND *csound, OPDS *perf_h, CSN_ARRAY *arr, size_t required_size, bool rt_locked) {
    if (required_size > arr->capacity) {
        if (arr->external_lock || (perf_h != NULL && rt_locked)) {
            return rt_growth_refused(csound, perf_h, arr, required_size);
        }
        size_t new_capacity = arr->capacity > 0 ? arr->capacity * 2 : 1;
        if (new_capacity < required_size) new_capacity = required_size;
        double *new_data = csound->ReAlloc(csound, arr->data, sizeof(double) * new_capacity * arr->itype);
        if (new_data == NULL) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Out of memory: allocation of %zu bytes failed", (size_t) (sizeof(double) * new_capacity * arr->itype));
        }

        arr->data = new_data;
        arr->capacity = new_capacity;
    }
    return OK;
}

/* The same rule for an opcode's private scratch, counted in items of
   item_size bytes: at perf time a scratch serving a marked slot must have been
   reserved at init. A first reservation is exact; growth doubles, so a
   buffer that keeps growing off a marked path does not reallocate every
   pass. The caller holds the registry mutex. */
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

static int32_t push_in(CSOUND *csound, OPDS *perf_h, CSN_ARRAY *arr, bool rt_locked, const MYFLT *in_rvalue, COMPLEXDAT *in_cvalue) {
    size_t new_size = arr->size + 1;
    int32_t res = ensure_mutation_capacity(csound, perf_h, arr, new_size, rt_locked);
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

    uint32_t handle = p->source_handle->id;
    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = NULL;
    CSN_ARRAY *arr = NULL;
    res = push_check_body(csound, NULL, &slot, &arr, reg, handle, false);
    if (res != OK) goto done;
    res = push_in(csound, NULL, arr, slot->rt_locked, p->in_value, NULL);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_push_k_init(CSOUND *csound, CSN_PUSH_K *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    p->registry = reg;
    return OK;
}

int32_t csnarray_push_k(CSOUND *csound, CSN_PUSH_K *p) {
    CSN_REGISTRY *reg = p->registry;
    if ((double) *p->arg_a == 0.0) return OK;

    uint32_t handle = p->source_handle->id;
    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = NULL;
    CSN_ARRAY *arr = NULL;
    res = push_check_body(csound, &p->h, &slot, &arr, reg, handle, false);
    if (res != OK) goto done;
    res = push_in(csound, &p->h, arr, slot->rt_locked, p->in_value, NULL);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_pushcomp(CSOUND *csound, CSN_PUSHCOMPLEX *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t handle = p->source_handle->id;
    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = NULL;
    CSN_ARRAY *arr = NULL;
    res = push_check_body(csound, NULL, &slot, &arr, reg, handle, true);
    if (res != OK) goto done;
    res = push_in(csound, NULL, arr, slot->rt_locked, NULL, p->in_value);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_pushcomp_k_init(CSOUND *csound, CSN_PUSHCOMPLEX_K *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    p->registry = reg;
    return OK;
}

int32_t csnarray_pushcomp_k(CSOUND *csound, CSN_PUSHCOMPLEX_K *p) {
    CSN_REGISTRY *reg = p->registry;
    if ((double) *p->arg_a == 0.0) return OK;

    uint32_t handle = p->source_handle->id;
    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = NULL;
    CSN_ARRAY *arr = NULL;
    res = push_check_body(csound, &p->h, &slot, &arr, reg, handle, true);
    if (res != OK) goto done;
    res = push_in(csound, &p->h, arr, slot->rt_locked, NULL, p->in_value);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t check_pop_body(CSOUND *csound, OPDS *perf_h, CSN_REGISTRY *reg, CSN_SLOT **slot, CSN_ARRAY **arr, uint32_t handle, bool is_complex) {
    *slot = get_slot(reg, handle);
    if ((*slot) == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", handle);
    }

    *arr = (*slot)->array;
    if ((*arr)->ndim != 1) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Pop needs a 1-D array, got %u-D", (*arr)->ndim);
    }

    if (!is_complex) {
        if ((*arr)->itype != CSN_REAL) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Handle holds a complex array; declare the popped value as :Complex;");
        }
    } else {
        if ((*arr)->itype != CSN_COMPLEX) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Handle holds a real array; declare the popped value as i/k");
        }
    }

    if ((*arr)->size == 0) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Cannot pop from an empty array");
    }

    return OK;
}

static void pop_out(CSN_ARRAY *arr, MYFLT *out_rvalue, COMPLEXDAT *out_cvalue) {
    /* size and shape[0] are written from the same value, so they cannot drift
       apart the way a separate emptiness flag could. */
    size_t new_size = arr->size - 1;
    if (out_rvalue != NULL) {
        *out_rvalue = (MYFLT) arr->data[new_size];
    } else {
        out_cvalue->real = (MYFLT) arr->data[new_size * 2];
        out_cvalue->imag = (MYFLT) arr->data[new_size * 2 + 1];
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

    uint32_t handle = p->source_handle->id;
    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = NULL;
    CSN_ARRAY *arr = NULL;
    res = check_pop_body(csound, NULL, reg, &slot, &arr, handle, false);
    if (res != OK) goto done;
    pop_out(arr, p->out_value, NULL);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_pop_k_init(CSOUND *csound, CSN_POP_K *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    p->registry = reg;
    return OK;
}

int32_t csnarray_popcomp_k_init(CSOUND *csound, CSN_POPCOMPLEX_K *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    p->registry = reg;
    return OK;
}

int32_t csnarray_pop_k(CSOUND *csound, CSN_POP_K *p) {
    CSN_REGISTRY *reg = p->registry;
    if ((double) *p->arg_a == 0.0) return OK;

    uint32_t handle = p->source_handle->id;
    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = NULL;
    CSN_ARRAY *arr = NULL;
    res = check_pop_body(csound, &p->h, reg, &slot, &arr, handle, false);
    if (res != OK) goto done;
    pop_out(arr, p->out_value, NULL);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_popcomp(CSOUND *csound, CSN_POPCOMPLEX *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t handle = p->source_handle->id;
    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = NULL;
    CSN_ARRAY *arr = NULL;
    res = check_pop_body(csound, NULL, reg, &slot, &arr, handle, true);
    if (res != OK) goto done;
    pop_out(arr, NULL, p->out_value);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_popcomp_k(CSOUND *csound, CSN_POPCOMPLEX_K *p) {
    CSN_REGISTRY *reg = p->registry;
    if ((double) *p->arg_a == 0.0) return OK;

    uint32_t handle = p->source_handle->id;
    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = NULL;
    CSN_ARRAY *arr = NULL;
    res = check_pop_body(csound, &p->h, reg, &slot, &arr, handle, true);
    if (res != OK) goto done;
    pop_out(arr, NULL, p->out_value);

done:
    csound->UnlockMutex(reg->mutex);
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

static int32_t insert_value_locked(CSOUND *csound, OPDS *perf_h, CSN_REGISTRY *reg, uint32_t handle, double index_value, const MYFLT *in_rvalue, COMPLEXDAT *in_cvalue) {
    if (!IS_VALID_INDEX(index_value)) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Insert index %g is invalid; indexes must be finite non-negative integers", index_value);
    }

    CSN_SLOT *slot = get_slot(reg, handle);
    if (slot == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", handle);
    }

    CSN_ARRAY *arr = slot->array;
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
    int32_t res = ensure_mutation_capacity(csound, perf_h, arr, new_size, slot->rt_locked);
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

    csound->LockMutex(reg->mutex);
    int32_t res = insert_value_locked(csound, NULL, reg, p->source_handle->id, (double) *p->index, p->in_value, NULL);
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_insert_k(CSOUND *csound, CSN_PUSH_K *p) {
    if ((double) *p->arg_b == 0.0) return OK;

    CSN_REGISTRY *reg = p->registry;
    csound->LockMutex(reg->mutex);
    int32_t res = insert_value_locked(csound, &p->h, reg, p->source_handle->id, (double) *p->arg_a, p->in_value, NULL);
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_insertcomp(CSOUND *csound, CSN_PUSHCOMPLEX *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    csound->LockMutex(reg->mutex);
    int32_t res = insert_value_locked(csound, NULL, reg, p->source_handle->id, (double) *p->index, NULL, p->in_value);
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_insertcomp_k(CSOUND *csound, CSN_PUSHCOMPLEX_K *p) {
    if ((double) *p->arg_b == 0.0) return OK;

    CSN_REGISTRY *reg = p->registry;
    csound->LockMutex(reg->mutex);
    int32_t res = insert_value_locked(csound, &p->h, reg, p->source_handle->id, (double) *p->arg_a, NULL, p->in_value);
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t remove_value_locked(CSOUND *csound, OPDS *perf_h, CSN_REGISTRY *reg, uint32_t handle, double index_value, MYFLT *out_rvalue, COMPLEXDAT *out_cvalue) {
    if (!IS_VALID_INDEX(index_value)) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Remove index %g is invalid; indexes must be finite non-negative integers", index_value);
    }

    CSN_SLOT *slot = get_slot(reg, handle);
    if (slot == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", handle);
    }

    CSN_ARRAY *arr = slot->array;
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
        out_cvalue->real = (MYFLT) arr->data[index * 2];
        out_cvalue->imag = (MYFLT) arr->data[index * 2 + 1];
        out_cvalue->isPolar = 0;
    } else {
        *out_rvalue = (MYFLT) arr->data[index];
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

    csound->LockMutex(reg->mutex);
    int32_t res = remove_value_locked(csound, NULL, reg, p->source_handle->id, (double) *p->index, p->out_value, NULL);
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_remove_k(CSOUND *csound, CSN_POP_K *p) {
    if ((double) *p->arg_b == 0.0) return OK;

    CSN_REGISTRY *reg = p->registry;
    csound->LockMutex(reg->mutex);
    int32_t res = remove_value_locked(csound, &p->h, reg, p->source_handle->id, (double) *p->arg_a, p->out_value, NULL);
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_removecomp(CSOUND *csound, CSN_POPCOMPLEX *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    csound->LockMutex(reg->mutex);
    int32_t res = remove_value_locked(csound, NULL, reg, p->source_handle->id, (double) *p->index, NULL, p->out_value);
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_removecomp_k(CSOUND *csound, CSN_POPCOMPLEX_K *p) {
    if ((double) *p->arg_b == 0.0) return OK;

    CSN_REGISTRY *reg = p->registry;
    csound->LockMutex(reg->mutex);
    int32_t res = remove_value_locked(csound, &p->h, reg, p->source_handle->id, (double) *p->arg_a, NULL, p->out_value);
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t check_insert_block_body(CSOUND *csound, CSN_INSERT_BLOCK *p, OPDS *perf_h, CSN_REGISTRY *reg, CSN_ARRAY **source_array, CSN_ARRAY **data_array, uint32_t source_handle, uint32_t data_handle, uint32_t *temp_shape, uint32_t *axis, uint32_t *index) {
    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
    }

    CSN_SLOT *data_slot = get_slot(reg, data_handle);
    if (data_slot == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) data_handle);
    }

    *source_array = source_slot->array;
    *data_array = data_slot->array;
    CSN_ARRAY *source_arr = *source_array;
    CSN_ARRAY *data_arr = *data_array;

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

static void insert_block_assign_value(CSN_ARRAY *temp, CSN_ARRAY *source_arr, CSN_ARRAY *data_arr, uint32_t source_ndim, uint32_t axis, uint32_t index) {
    for (size_t linear = 0; linear < temp->size; ++linear) {
        uint32_t dst_coords[CSN_MAX_DIMS] = {0};
        uint32_t src_coords[CSN_MAX_DIMS] = {0};

        from_linear_to_coords(dst_coords, temp->shape, linear, temp->ndim);

        if (dst_coords[axis] == index) {
            for (uint32_t i = 0, j = 0; i < source_ndim; ++i) {
                if (i != axis) src_coords[j++] = dst_coords[i];
            }

            size_t block_off = from_coords_to_offset(src_coords, data_arr->strides, data_arr->ndim);
            if (source_arr->itype == CSN_REAL) {
                temp->data[linear] = data_arr->data[block_off];
            } else {
                temp->data[linear * 2] = data_arr->data[block_off * 2];
                temp->data[linear * 2 + 1] = data_arr->data[block_off * 2 + 1];
            }
        } else {
            memcpy(src_coords, dst_coords, sizeof(uint32_t) * source_ndim);
            if (dst_coords[axis] > index) src_coords[axis]--;
            size_t source_off = from_coords_to_offset(src_coords, source_arr->strides, source_arr->ndim);
            if (source_arr->itype == CSN_REAL) {
                temp->data[linear] = source_arr->data[source_off];
            } else {
                temp->data[linear * 2] = source_arr->data[source_off * 2];
                temp->data[linear * 2 + 1] = source_arr->data[source_off * 2 + 1];
            }
        }
    }
}

int32_t csnarray_insert_block_deinit(CSOUND *csound, CSN_INSERT_BLOCK *p) {
    if (p->scratch != NULL) {
        if (p->scratch->data != NULL) {
            csound->Free(csound, p->scratch->data);
        }
        csound->Free(csound, p->scratch);
    }
    return OK;
}

int32_t csnarray_insert_block(CSOUND *csound, CSN_INSERT_BLOCK *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;
    uint32_t data_handle = p->data_handle->id;

    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    CSN_ARRAY *data_arr = NULL;
    uint32_t temp_shape[CSN_MAX_DIMS] = {0};
    uint32_t axis = 0;
    uint32_t index = 0;
    res = check_insert_block_body(csound, p, NULL, reg, &source_arr, &data_arr, source_handle, data_handle, temp_shape, &axis, &index);
    if (res != OK) goto done;

    CSN_ARRAY *temp = csound->Calloc(csound, sizeof(CSN_ARRAY));
    if (temp == NULL) {
        res = csound->InitError(csound, "[csnarray] Out of memory: allocation of %zu bytes failed", (size_t) (sizeof(CSN_ARRAY)));
        goto done;
    }

    uint32_t source_ndim = source_arr->ndim;
    int32_t alloc_temp = allocate_array(csound, temp, source_ndim, temp_shape, source_arr->array_id, source_arr->itype);
    if (alloc_temp != OK) {
        char tbuf[CSN_SHAPE_STR_MAX];
        csound->Free(csound, temp);
        res = csound->InitError(csound, "[csnarray] Out of memory: could not allocate the %u-D temporary array %s", source_ndim, shape_str(tbuf, sizeof(tbuf), temp_shape, source_ndim));
        goto done;
    }

    insert_block_assign_value(temp, source_arr, data_arr, source_ndim, axis, index);

    res = ensure_mutation_capacity(csound, NULL, source_arr, temp->size, false);
    if (res != OK) {
        csound->Free(csound, temp->data);
        csound->Free(csound, temp);
        goto done;
    }

    travase_csnarray(source_arr, temp);
    p->scratch = temp;
    p->registry = reg;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_insert_block_k_init(CSOUND *csound, CSN_INSERT_BLOCK *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;
    uint32_t data_handle = p->data_handle->id;

    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    CSN_ARRAY *data_arr = NULL;
    uint32_t temp_shape[CSN_MAX_DIMS] = {0};
    uint32_t axis = 0;
    uint32_t index = 0;
    res = check_insert_block_body(csound, p, NULL, reg, &source_arr, &data_arr, source_handle, data_handle, temp_shape, &axis, &index);
    if (res != OK) goto done;

    CSN_ARRAY *temp = csound->Calloc(csound, sizeof(CSN_ARRAY));
    if (temp == NULL) {
        res = csound->InitError(csound, "[csnarray] Out of memory: allocation of %zu bytes failed", (size_t) (sizeof(CSN_ARRAY)));
        goto done;
    }

    uint32_t source_ndim = source_arr->ndim;
    int32_t alloc_temp = allocate_array(csound, temp, source_ndim, temp_shape, source_arr->array_id, source_arr->itype);
    if (alloc_temp != OK) {
        char tbuf[CSN_SHAPE_STR_MAX];
        csound->Free(csound, temp);
        res = csound->InitError(csound, "[csnarray] Out of memory: could not allocate the %u-D temporary array %s", source_ndim, shape_str(tbuf, sizeof(tbuf), temp_shape, source_ndim));
        goto done;
    }

    /* The image is copied back over the source, so it never needs more room
       than the source can take without reallocating. Reserving that much here
       keeps a marked source from being refused by its scratch first. */
    res = ensure_mutation_capacity(csound, NULL, temp, source_arr->capacity, false);
    if (res != OK) {
        csound->Free(csound, temp->data);
        csound->Free(csound, temp);
        goto done;
    }

    p->scratch = temp;
    p->registry = reg;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_insert_block_k(CSOUND *csound, CSN_INSERT_BLOCK *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    if (p->scratch == NULL) {
        return csound->PerfError(csound, &p->h, "[csnarray] Internal error: temporary buffer not available");
    }

    CHECK_KTRIG(p->trig);

    uint32_t source_handle = p->source_handle->id;
    uint32_t data_handle = p->data_handle->id;

    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    CSN_ARRAY *data_arr = NULL;
    uint32_t temp_shape[CSN_MAX_DIMS] = {0};
    uint32_t axis = 0;
    uint32_t index = 0;
    res = check_insert_block_body(csound, p, &p->h, reg, &source_arr, &data_arr, source_handle, data_handle, temp_shape, &axis, &index);
    if (res != OK) goto done;

    uint32_t source_ndim = source_arr->ndim;
    size_t requested_size = 0;
    if (get_array_size_from_shape(&requested_size, source_ndim, temp_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    bool rt_locked = csn_slot_rt_locked(reg, source_handle);
    bool have_same_shape = memcmp(p->scratch->shape, temp_shape, sizeof(uint32_t) * CSN_MAX_DIMS) == 0;
    bool request_changed = !have_same_shape || source_ndim != p->scratch->ndim || p->scratch->capacity < requested_size || source_arr->itype != p->scratch->itype;

    if (request_changed) {
        bool needs_realloc = p->scratch->capacity < requested_size || source_arr->itype != p->scratch->itype;
        if (needs_realloc) {
            if (rt_locked) {
                res = rt_growth_refused(csound, &p->h, source_arr, requested_size);
                goto done;
            }
            size_t new_capacity = requested_size > 0 ? requested_size * 2 : 1;
            double *new_realloc = csound->ReAlloc(csound, p->scratch->data, sizeof(double) * new_capacity * source_arr->itype);
            if (new_realloc == NULL) {
                csound->Free(csound, p->scratch->data);
                csound->Free(csound, p->scratch);
                p->scratch = NULL;
                csound->UnlockMutex(reg->mutex);
                return csound->PerfError(csound, &p->h, "[csnarray] Out of memory: allocation failed");
            }
            p->scratch->data = new_realloc;
            p->scratch->capacity = new_capacity;
        }
        set_csnarray_layout(p->scratch, source_arr->ndim, temp_shape, requested_size, source_arr->itype);
    }

    /* The source only reallocates when the image outgrows it, never on a pass
       that still fits. */
    res = ensure_mutation_capacity(csound, &p->h, source_arr, p->scratch->size, rt_locked);
    if (res != OK) goto done;

    insert_block_assign_value(p->scratch, source_arr, data_arr, source_ndim, axis, index);
    travase_csnarray(source_arr, p->scratch);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t check_remove_block_body(CSOUND *csound, CSN_TAKE *p, OPDS *perf_h, CSN_REGISTRY *reg, CSN_ARRAY **source_array, uint32_t handle, uint32_t *temp_shape, uint32_t *axis, uint32_t *index) {
    CSN_SLOT *source_slot = get_slot(reg, handle);
    if (source_slot == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) handle);
    }

    *source_array = source_slot->array;
    CSN_ARRAY *source_arr = *source_array;
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

static void remove_block_assign_value(CSN_ARRAY *source_arr, CSN_ARRAY *destination, uint32_t axis, uint32_t index) {
    for (size_t linear = 0; linear < destination->size; ++linear) {
        uint32_t dst_coords[CSN_MAX_DIMS] = {0};
        uint32_t src_coords[CSN_MAX_DIMS] = {0};

        from_linear_to_coords(dst_coords, destination->shape, linear, destination->ndim);
        memcpy(src_coords, dst_coords, sizeof(uint32_t) * source_arr->ndim);
        if (dst_coords[axis] >= index) src_coords[axis]++;
        size_t source_off = from_coords_to_offset(src_coords, source_arr->strides, source_arr->ndim);
        if (source_arr->itype == CSN_REAL) {
            destination->data[linear] = source_arr->data[source_off];
        } else {
            destination->data[linear * 2] = source_arr->data[source_off * 2];
            destination->data[linear * 2 + 1] = source_arr->data[source_off * 2 + 1];
        }
    }
}

int32_t csnarray_remove_block(CSOUND *csound, CSN_TAKE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;
    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    uint32_t axis = 0;
    uint32_t index = 0;
    uint32_t temp_shape[CSN_MAX_DIMS] = {0};
    res = check_remove_block_body(csound, p, NULL, reg, &source_arr, source_handle, temp_shape, &axis, &index);
    if (res != OK) goto done;

    uint32_t source_ndim = source_arr->ndim;
    if (create_csnarray_locked(csound, reg, &p->h, source_ndim, temp_shape, &p->array, p->handle, &source_handle, 1U, &err, source_arr->itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;
    arr->size = source_arr->size == 0 ? 0 : arr->size;
    remove_block_assign_value(source_arr, arr, axis, index);
    SET_KDATA_BEGIN(p, reg);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_remove_block_k_init(CSOUND *csound, CSN_TAKE *p) {
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

    CSN_ARRAY *source_arr = source_slot->array;
    if (create_csnarray_locked(csound, reg, &p->h, source_arr->ndim, source_arr->shape, &p->array, p->handle, &source_handle, 1U, &err, source_arr->itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    p->array->size = source_arr->size;
    memcpy(p->array->data, source_arr->data, sizeof(double) * source_arr->size * source_arr->itype);
    SET_KDATA_BEGIN(p, reg);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_remove_block_k(CSOUND *csound, CSN_TAKE *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REG_HANDLE(csound, &p->h, reg, p->k_data.owned_handle);

    CHECK_KTRIG(p->trig);

    if (p->array == NULL) {
        return csound->PerfError(csound, &p->h, "[csnarray] Internal error: output array is not available");
    }

    uint32_t source_handle = p->source_handle->id;
    int32_t res = CHECK_SELF_ALIAS(csound, &p->h, &p->k_data, source_handle, 0);
    if (res != OK) return res;

    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    uint32_t axis = 0;
    uint32_t index = 0;
    uint32_t temp_shape[CSN_MAX_DIMS] = {0};
    res = check_remove_block_body(csound, p, &p->h, reg, &source_arr, source_handle, temp_shape, &axis, &index);
    if (res != OK) goto done;

    uint32_t source_ndim = source_arr->ndim;
    CSN_ARRAY *arr = p->array;

    size_t requested_size = 0;
    if (get_array_size_from_shape(&requested_size, source_arr->ndim, temp_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    size_t logical_size = source_arr->size == 0 ? 0 : requested_size;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, source_ndim, temp_shape, logical_size, source_arr->itype, err);
    if (res != OK) goto done;

    remove_block_assign_value(source_arr, arr, axis, index);
    SET_KDATA_END(p, arr->shape, arr->ndim, arr->itype);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t concat_flat_body(CSOUND *csound, OPDS *perf_h, CSN_REGISTRY *reg, CSN_ARRAY **source_array, CSN_ARRAY **data_array, uint32_t source_handle, uint32_t data_handle, uint32_t *out_shape) {
    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
    }

    CSN_SLOT *data_slot = get_slot(reg, data_handle);
    if (data_slot == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) data_handle);
    }

    *source_array = source_slot->array;
    *data_array = data_slot->array;
    CSN_ARRAY *source_arr = (*source_array);
    CSN_ARRAY *data_arr = (*data_array);

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

static void concat_flat_assign_value(CSN_ARRAY *source_arr, CSN_ARRAY *data_arr, CSN_ARRAY *destination) {
    for (size_t linear = 0; linear < destination->size; ++linear) {
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
    }
}

int32_t csnarray_concat_flat(CSOUND *csound, CSN_CONCAT *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;
    uint32_t data_handle = p->data_handle->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    CSN_ARRAY *data_arr = NULL;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    res = concat_flat_body(csound, NULL, reg, &source_arr, &data_arr, source_handle, data_handle, new_shape);
    if (res != OK) goto done;

    /* Both operands are read by the copy loop below, so both are protected. */
    const uint32_t protect[2] = { source_handle, data_handle };
    if (create_csnarray_locked(csound, reg, &p->h, 1U, new_shape, &p->array, p->handle, protect, 2U, &err, source_arr->itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;
    concat_flat_assign_value(source_arr, data_arr, arr);
    SET_KDATA_BEGIN(p, reg);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_concat_flat_k(CSOUND *csound, CSN_CONCAT *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REG_HANDLE(csound, &p->h, reg, p->k_data.owned_handle);

    if ((double) *p->arg_a == 0.0) return OK;

    if (p->array == NULL) {
        return csound->PerfError(csound, &p->h, "[csnarray] Internal error: output array is not available");
    }

    uint32_t source_handle = p->source_handle->id;
    uint32_t data_handle = p->data_handle->id;

    int32_t res = CHECK_SELF_ALIAS(csound, &p->h, &p->k_data, source_handle, data_handle);
    if (res != OK) return res;

    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    CSN_ARRAY *data_arr = NULL;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    res = concat_flat_body(csound, &p->h, reg, &source_arr, &data_arr, source_handle, data_handle, new_shape);
    if (res != OK) goto done;

    size_t requested_size = 0;
    if (get_array_size_from_shape(&requested_size, 1U, new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    size_t logical_size = new_shape[0] == 0 ? 0 : requested_size;

    CSN_ARRAY *arr = p->array;
    CSN_SLOT *reuse_slot = get_slot(reg, p->k_data.owned_handle);
    if (reuse_slot != NULL && CAN_REUSE_ELEMENTWISE(&p->k_data, source_handle, source_arr, data_handle, data_arr, reuse_slot->array, 0.0, 0.0)) {
        p->handle->id = p->k_data.owned_handle;
        goto done;
    }

    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, 1U, new_shape, logical_size, source_arr->itype, err);
    if (res != OK) goto done;

    concat_flat_assign_value(source_arr, data_arr, arr);
    SET_KDATA_END(p, new_shape, 1U, source_arr->itype);
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle, source_arr, data_handle, data_arr, arr, 0.0, 0.0);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t concat_block_body(CSOUND *csound, OPDS *perf_h, CSN_REGISTRY *reg, CSN_ARRAY **source_array, CSN_ARRAY **data_array, uint32_t source_handle, uint32_t data_handle, uint32_t *out_shape, const MYFLT *in_axis, uint32_t *out_axis) {
    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
    }

    CSN_SLOT *data_slot = get_slot(reg, data_handle);
    if (data_slot == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h,"[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) data_handle);
    }

    *source_array = source_slot->array;
    *data_array = data_slot->array;
    CSN_ARRAY *source_arr = *source_array;
    CSN_ARRAY *data_arr = *data_array;

    if (source_arr->itype != data_arr->itype) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Element type mismatch: one array is real and the other complex");
    }

    uint32_t source_ndim = source_arr->ndim;
    uint32_t data_ndim = data_arr->ndim;

    if (data_ndim != source_ndim) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Arrays must have the same number of dimensions, got %u-D and %u-D", source_ndim, data_ndim);
    }

    uint32_t *source_shape = source_arr->shape;
    uint32_t *data_shape = data_arr->shape;

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

static void concat_block_assign_value(CSN_ARRAY *source_arr, CSN_ARRAY *data_arr, CSN_ARRAY *destination, uint32_t axis) {
    uint32_t source_axis_extent = source_arr->size == 0 ? 0U : source_arr->shape[axis];
    for (size_t linear = 0; linear < destination->size; ++linear) {
        uint32_t dst_coords[CSN_MAX_DIMS] = {0};
        uint32_t src_coords[CSN_MAX_DIMS] = {0};

        from_linear_to_coords(dst_coords, destination->shape, linear, destination->ndim);

        if (dst_coords[axis] < source_axis_extent) {
            memcpy(src_coords, dst_coords, sizeof(uint32_t) * source_arr->ndim);
            size_t source_off = from_coords_to_offset(src_coords, source_arr->strides, source_arr->ndim);
            if (destination->itype == CSN_COMPLEX) {
                destination->data[linear * 2] = source_arr->data[source_off * 2];
                destination->data[linear * 2 + 1] = source_arr->data[source_off * 2 + 1];
            } else {
                destination->data[linear] = source_arr->data[source_off];
            }
        } else {
            memcpy(src_coords, dst_coords, sizeof(uint32_t) * source_arr->ndim);
            src_coords[axis] -= source_axis_extent;
            size_t block_off = from_coords_to_offset(src_coords, data_arr->strides, data_arr->ndim);
            if (destination->itype == CSN_COMPLEX) {
                destination->data[linear * 2] = data_arr->data[block_off * 2];
                destination->data[linear * 2 + 1] = data_arr->data[block_off * 2 + 1];
            } else {
                destination->data[linear] = data_arr->data[block_off];
            }
        }
    }
}

int32_t csnarray_concat_block(CSOUND *csound, CSN_CONCAT *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;
    uint32_t data_handle = p->data_handle->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    CSN_ARRAY *data_arr = NULL;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    uint32_t axis = 0;
    res = concat_block_body(csound, NULL, reg, &source_arr, &data_arr, source_handle, data_handle, new_shape, p->arg_a, &axis);
    if (res != OK) goto done;

    /* Both operands are read by the copy loop below, so both are protected. */
    const uint32_t protect[2] = { source_handle, data_handle };

    if (create_csnarray_locked(csound, reg, &p->h, source_arr->ndim, new_shape, &p->array, p->handle, protect, 2U, &err, source_arr->itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;
    concat_block_assign_value(source_arr, data_arr, arr, axis);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_concat_block_k_init(CSOUND *csound, CSN_CONCAT *p) {
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

    const uint32_t protect[1] = { source_handle };
    if (create_csnarray_locked(csound, reg, &p->h, source_arr->ndim, source_arr->shape, &p->array, p->handle, protect, 1U, &err, source_arr->itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    p->array->size = source_arr->size;
    if (source_arr->size > 0) {
        memcpy(p->array->data, source_arr->data, sizeof(double) * source_arr->size * source_arr->itype);
    }
    SET_KDATA_BEGIN(p, reg);
    p->k_data.prev_size = source_arr->size;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_concat_block_k(CSOUND *csound, CSN_CONCAT *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REG_HANDLE(csound, &p->h, reg, p->k_data.owned_handle);

    if ((double) *p->arg_a == 0.0) return OK;

    if (p->array == NULL) {
        return csound->PerfError(csound, &p->h, "[csnarray] Internal error: temporary buffer is not available");
    }

    uint32_t source_handle = p->source_handle->id;
    uint32_t data_handle = p->data_handle->id;

    int32_t res = CHECK_SELF_ALIAS(csound, &p->h, &p->k_data, source_handle, data_handle);
    if (res != OK) return res;

    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    CSN_ARRAY *data_arr = NULL;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    uint32_t axis = 0;
    res = concat_block_body(csound, &p->h, reg, &source_arr, &data_arr, source_handle, data_handle, new_shape, p->arg_b, &axis);
    if (res != OK) goto done;

    size_t requested_size = 0;
    if (get_array_size_from_shape(&requested_size, source_arr->ndim, new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    size_t logical_size = requested_size;

    CSN_ARRAY *arr = p->array;
    CSN_SLOT *reuse_slot = get_slot(reg, p->k_data.owned_handle);
    if (reuse_slot != NULL && CAN_REUSE_ELEMENTWISE(&p->k_data, source_handle, source_arr, data_handle, data_arr, reuse_slot->array, (double) axis, 0.0)) {
        p->handle->id = p->k_data.owned_handle;
        goto done;
    }

    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, source_arr->ndim, new_shape, logical_size, source_arr->itype, err);
    if (res != OK) goto done;

    concat_block_assign_value(source_arr, data_arr, arr, axis);
    SET_KDATA_END(p, new_shape, arr->ndim, arr->itype);
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle, source_arr, data_handle, data_arr, arr, (double) axis, 0.0);
    p->k_data.prev_size = arr->size;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t pad_body(CSOUND *csound, OPDS *perf_h, CSN_REGISTRY *reg, CSN_ARRAY **source_array, uint32_t source_handle, const MYFLT *axis_in, const MYFLT *in_before, const MYFLT *in_after, int32_t *out_axis, uint32_t *out_before, uint32_t *out_after, uint32_t *out_shape, ITEM_TYPE expected_type) {
    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
    }

    *source_array = source_slot->array;
    CSN_ARRAY *source_arr = *source_array;
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    if (source_arr->itype != expected_type) {
        if (expected_type == CSN_COMPLEX) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Handle holds a real array; pad it with a real value");
        }
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Handle holds a complex array; pad it with a :Complex; value");
    }

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

void pad_assign_value(CSN_ARRAY *source_arr, CSN_ARRAY *destination, double real_value, COMPLEXDAT *complex_value, int32_t axis, uint32_t before) {
    bool is_complex = destination->itype == CSN_COMPLEX;
    /* Mirrors the extents pad_body derived: an empty source has nothing to
       copy, so every destination cell is padding. */
    bool source_is_empty = source_arr->size == 0;
    double re = 0.0, im = 0.0;

    if (is_complex && complex_value != NULL) {
        complexdat_to_rect(complex_value, &re, &im);
    }

    for (size_t linear = 0; linear < destination->size; ++linear) {
        uint32_t dst_coords[CSN_MAX_DIMS] = {0};
        uint32_t src_coords[CSN_MAX_DIMS] = {0};

        from_linear_to_coords(dst_coords, destination->shape, linear, destination->ndim);

        bool is_inside = !source_is_empty;
        for (uint32_t i = 0; is_inside && i < source_arr->ndim; i++) {
            bool is_padded_axis = axis == -1 || (uint32_t) axis == i;
            if (is_padded_axis) {
                if (dst_coords[i] < before || dst_coords[i] >= before + source_arr->shape[i]) {
                    is_inside = false;
                    break;
                }
                src_coords[i] = dst_coords[i] - before;
            } else {
                src_coords[i] = dst_coords[i];
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

        size_t source_off = from_coords_to_offset(src_coords, source_arr->strides, source_arr->ndim);
        if (is_complex) {
            destination->data[linear * 2] = source_arr->data[source_off * 2];
            destination->data[linear * 2 + 1] = source_arr->data[source_off * 2 + 1];
        } else {
            destination->data[linear] = source_arr->data[source_off];
        }
    }
}

static int32_t csnarray_pad_helper(CSOUND *csound, const OPDS *h, CSNREF *ohandle, CSNREF *shandle, const MYFLT *in_before, const MYFLT *in_after, double value, COMPLEXDAT *valuecomp, ITEM_TYPE expected_itype, const MYFLT *axis_in, CSN_ARRAY **array) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = shandle->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    int32_t axis = 0;
    uint32_t before = 0;
    uint32_t after = 0;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    res = pad_body(csound, NULL, reg, &source_arr, source_handle, axis_in, in_before, in_after, &axis, &before, &after, new_shape, expected_itype);
    if (res != OK) goto done;

    const uint32_t protect[1] = { source_handle };
    if (create_csnarray_locked(csound, reg, h, source_arr->ndim, new_shape, array, ohandle, protect, 1U, &err, source_arr->itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = *array;
    pad_assign_value(source_arr, arr, value, valuecomp, axis, before);

done:
    csound->UnlockMutex(reg->mutex);
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
static bool pad_k_init_shape(CSOUND *csound, CSN_REGISTRY *reg, uint32_t source_handle, const CSN_ARRAY *source_arr, const MYFLT *axis_in, const MYFLT *in_before, const MYFLT *in_after, ITEM_TYPE itype, int32_t *axis, uint32_t *before, uint32_t *after, uint32_t *shape) {
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

    CSN_ARRAY *checked = NULL;
    return pad_body(csound, NULL, reg, &checked, source_handle, axis_in, in_before, in_after, axis, before, after, shape, itype) == OK;
}

int32_t csnarray_pad_k_init(CSOUND *csound, CSN_PAD *p) {
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
        res = csound->InitError(csound, "[csnarray] Handle holds a complex array; pad it with a :Complex; value");
        goto done;
    }

    uint32_t shape[CSN_MAX_DIMS] = {0};
    memcpy(shape, source_arr->shape, sizeof(shape));
    int32_t axis = -1;
    uint32_t before = 0;
    uint32_t after = 0;
    const MYFLT *axis_in = p->INOCOUNT > 5 ? p->arg_b : NULL;
    bool padded = pad_k_init_shape(csound, reg, source_handle, source_arr, axis_in, p->before, p->after, CSN_REAL, &axis, &before, &after, shape);

    const uint32_t protect[1] = { source_handle };

    if (create_csnarray_locked(csound, reg, &p->h, source_arr->ndim, shape, &p->array, p->handle, protect, 1U, &err, source_arr->itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    if (padded) {
        pad_assign_value(source_arr, p->array, (double) *p->value, NULL, axis, before);
    } else {
        p->array->size = source_arr->size;
        if (source_arr->size > 0) {
            memcpy(p->array->data, source_arr->data, sizeof(double) * source_arr->size * source_arr->itype);
        }
    }
    SET_KDATA_BEGIN(p, reg);
    p->k_data.prev_size = p->array->size;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_padcomp_k_init(CSOUND *csound, CSN_PADCOMPLEX *p) {
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

    if (source_arr->itype != CSN_COMPLEX) {
        res = csound->InitError(csound, "[csnarray] Handle holds a real array; pad it with a real value");
        goto done;
    }

    uint32_t shape[CSN_MAX_DIMS] = {0};
    memcpy(shape, source_arr->shape, sizeof(shape));
    int32_t axis = -1;
    uint32_t before = 0;
    uint32_t after = 0;
    const MYFLT *axis_in = p->INOCOUNT > 5 ? p->arg_b : NULL;
    bool padded = pad_k_init_shape(csound, reg, source_handle, source_arr, axis_in, p->before, p->after, CSN_COMPLEX, &axis, &before, &after, shape);

    const uint32_t protect[1] = { source_handle };

    if (create_csnarray_locked(csound, reg, &p->h, source_arr->ndim, shape, &p->array, p->handle, protect, 1U, &err, source_arr->itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    if (padded) {
        pad_assign_value(source_arr, p->array, 0.0, p->value, axis, before);
    } else {
        p->array->size = source_arr->size;
        if (source_arr->size > 0) {
            memcpy(p->array->data, source_arr->data, sizeof(double) * source_arr->size * source_arr->itype);
        }
    }
    SET_KDATA_BEGIN(p, reg);
    p->k_data.prev_size = p->array->size;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_pad_k(CSOUND *csound, CSN_PAD *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REG_HANDLE(csound, &p->h, reg, p->k_data.owned_handle);
    if (p->array == NULL) {
        return csound->PerfError(csound, &p->h, "[csnarray] Internal error: output array is not available");
    }

    const MYFLT *axis_in = p->INOCOUNT > 5 ? p->arg_b : NULL;
    double trig = (double) *p->arg_a;

    if (trig == 0.0) return OK;

    uint32_t source_handle = p->source_handle->id;

    int32_t res = CHECK_SELF_ALIAS(csound, &p->h, &p->k_data, source_handle, 0);
    if (res != OK) return res;

    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    int32_t axis = 0;
    uint32_t before = 0;
    uint32_t after = 0;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    res = pad_body(csound, &p->h, reg, &source_arr, source_handle, axis_in, p->before, p->after, &axis, &before, &after, new_shape, CSN_REAL);
    if (res != OK) goto done;

    size_t requested_size = 0;
    if (get_array_size_from_shape(&requested_size, source_arr->ndim, new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSN_ARRAY *arr = p->array;

    /* Every cell of the padded shape is written below, so the logical size is
       the physical one even when the source is logically empty. */
    CSN_SLOT *reuse_slot = get_slot(reg, p->k_data.owned_handle);
    if (reuse_slot != NULL
        && CAN_REUSE_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, reuse_slot->array, (double) *p->value, (double) axis)
        && p->k_data.prev_index == before && p->k_data.prev_roll_shift == (int32_t) after) {
        p->handle->id = p->k_data.owned_handle;
        goto done;
    }

    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, source_arr->ndim, new_shape, requested_size, source_arr->itype, err);
    if (res != OK) goto done;

    pad_assign_value(source_arr, arr, (double) *p->value, NULL, axis, before);
    SET_KDATA_END(p, new_shape, arr->ndim, arr->itype);
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, arr, (double) *p->value, (double) axis);
    p->k_data.prev_index = before;
    p->k_data.prev_roll_shift = (int32_t) after;
    p->k_data.prev_size = arr->size;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_padcomp_k(CSOUND *csound, CSN_PADCOMPLEX *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REG_HANDLE(csound, &p->h, reg, p->k_data.owned_handle);
    if (p->array == NULL) {
        return csound->PerfError(csound, &p->h, "[csnarray] Internal error: output array is not available");
    }

    const MYFLT *axis_in = p->INOCOUNT > 5 ? p->arg_b : NULL;
    double trig = (double) *p->arg_a;

    if (trig == 0.0) return OK;

    uint32_t source_handle = p->source_handle->id;

    int32_t res = CHECK_SELF_ALIAS(csound, &p->h, &p->k_data, source_handle, 0);
    if (res != OK) return res;

    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    int32_t axis = 0;
    uint32_t before = 0;
    uint32_t after = 0;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    res = pad_body(csound, &p->h, reg, &source_arr, source_handle, axis_in, p->before, p->after, &axis, &before, &after, new_shape, CSN_COMPLEX);
    if (res != OK) goto done;

    size_t requested_size = 0;
    if (get_array_size_from_shape(&requested_size, source_arr->ndim, new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSN_ARRAY *arr = p->array;
    double fill_re = 0.0, fill_im = 0.0;
    complexdat_to_rect(p->value, &fill_re, &fill_im);
    CSN_SLOT *reuse_slot = get_slot(reg, p->k_data.owned_handle);
    if (reuse_slot != NULL
        && CAN_REUSE_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, reuse_slot->array, fill_re, fill_im)
        && (int32_t) p->k_data.prev_axis_u == axis
        && p->k_data.prev_index == before && p->k_data.prev_roll_shift == (int32_t) after) {
        p->handle->id = p->k_data.owned_handle;
        goto done;
    }

    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, source_arr->ndim, new_shape, requested_size, source_arr->itype, err);
    if (res != OK) goto done;

    pad_assign_value(source_arr, arr, 0.0, p->value, axis, before);
    SET_KDATA_END(p, new_shape, arr->ndim, arr->itype);
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, arr, fill_re, fill_im);
    p->k_data.prev_axis_u = (uint32_t) axis;
    p->k_data.prev_index = before;
    p->k_data.prev_roll_shift = (int32_t) after;
    p->k_data.prev_size = arr->size;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_pad(CSOUND *csound, CSN_PAD *p) {
    const MYFLT *axis = p->INOCOUNT > 4 ? p->arg_a : NULL;
    return csnarray_pad_helper(csound, &p->h, p->handle, p->source_handle, p->before, p->after, (double) *p->value, NULL, CSN_REAL, axis, &p->array);
}

int32_t csnarray_padcomp(CSOUND *csound, CSN_PADCOMPLEX *p) {
    const MYFLT *axis = p->INOCOUNT > 4 ? p->arg_a : NULL;
    return csnarray_pad_helper(csound, &p->h, p->handle, p->source_handle, p->before, p->after, 0.0, p->value, CSN_COMPLEX, axis, &p->array);
}

static int32_t csnarray_pad_in_helper(CSOUND *csound, CSNREF *shandle, const MYFLT *in_before, const MYFLT *in_after, double value, COMPLEXDAT *valuecomp, ITEM_TYPE expected_itype, const MYFLT *axis_in) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = shandle->id;

    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    int32_t axis = 0;
    uint32_t before = 0;
    uint32_t after = 0;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    res = pad_body(csound, NULL, reg, &source_arr, source_handle, axis_in, in_before, in_after, &axis, &before, &after, new_shape, expected_itype);
    if (res != OK) goto done;

    uint32_t source_ndim = source_arr->ndim;

    CSN_ARRAY *temp = csound->Calloc(csound, sizeof(CSN_ARRAY));
    if (temp == NULL) {
        res = csound->InitError(csound, "[csnarray] Out of memory: allocation of %zu bytes failed", (size_t) (sizeof(CSN_ARRAY)));
        goto done;
    }

    int32_t alloc_temp = allocate_array(csound, temp, source_ndim, new_shape, source_arr->array_id, source_arr->itype);
    if (alloc_temp != OK) {
        char tbuf[CSN_SHAPE_STR_MAX];
        csound->Free(csound, temp);
        res = csound->InitError(csound, "[csnarray] Out of memory: could not allocate the %u-D temporary array %s", source_ndim, shape_str(tbuf, sizeof(tbuf), new_shape, source_ndim));
        goto done;
    }

    pad_assign_value(source_arr, temp, value, valuecomp, axis, before);

    res = ensure_mutation_capacity(csound, NULL, source_arr, temp->size, false);
    if (res == OK) {
        travase_csnarray(source_arr, temp);
    }
    csound->Free(csound, temp->data);
    csound->Free(csound, temp);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_pad_in(CSOUND *csound, CSN_PAD_IN *p) {
    const MYFLT *axis = p->INOCOUNT > 4 ? p->arg_a : NULL;
    return csnarray_pad_in_helper(csound, p->source_handle, p->before, p->after, (double) *p->value, NULL, CSN_REAL, axis);
}

int32_t csnarray_padcomp_in(CSOUND *csound, CSN_PADCOMPLEX_IN *p) {
    const MYFLT *axis = p->INOCOUNT > 4 ? p->arg_a : NULL;
    return csnarray_pad_in_helper(csound, p->source_handle, p->before, p->after, 0.0, p->value, CSN_COMPLEX, axis);
}

/* The in-place k-rate pads build the padded image in a per-instance scratch
   array and then overwrite the source, so the registry slot never changes
   identity and no handle is created. */
static int32_t csnarray_pad_in_k_scratch_init(CSOUND *csound, CSNREF *shandle, ITEM_TYPE expected_itype, CSN_REGISTRY **out_registry, CSN_ARRAY **out_scratch) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = shandle->id;

    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = source_slot->array;

    if (source_arr->itype != expected_itype) {
        if (expected_itype == CSN_COMPLEX) {
            res = csound->InitError(csound, "[csnarray] Handle holds a real array; pad it with a real value");
            goto done;
        }
        res = csound->InitError(csound, "[csnarray] Handle holds a complex array; pad it with a :Complex; value");
        goto done;
    }

    CSN_ARRAY *temp = csound->Calloc(csound, sizeof(CSN_ARRAY));
    if (temp == NULL) {
        res = csound->InitError(csound, "[csnarray] Out of memory: allocation of %zu bytes failed", (size_t) (sizeof(CSN_ARRAY)));
        goto done;
    }

    /* The widths are k-rate, so the scratch starts at the source shape and is
       grown by the perf pass whenever the requested padding needs more room.
       It starts at least as large as the source's capacity: the image lands
       back in the source, so a marked source is refused by its own capacity
       and never earlier by this scratch. */
    if (allocate_array(csound, temp, source_arr->ndim, source_arr->shape, source_arr->array_id, source_arr->itype) != OK) {
        char tbuf[CSN_SHAPE_STR_MAX];
        csound->Free(csound, temp);
        res = csound->InitError(csound, "[csnarray] Out of memory: could not allocate the %u-D temporary array %s", source_arr->ndim, shape_str(tbuf, sizeof(tbuf), source_arr->shape, source_arr->ndim));
        goto done;
    }
    res = ensure_mutation_capacity(csound, NULL, temp, source_arr->capacity, false);
    if (res != OK) {
        csound->Free(csound, temp->data);
        csound->Free(csound, temp);
        goto done;
    }

    *out_scratch = temp;
    *out_registry = reg;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_pad_in_k_init(CSOUND *csound, CSN_PAD_IN *p) {
    return csnarray_pad_in_k_scratch_init(csound, p->source_handle, CSN_REAL, &p->registry, &p->scratch);
}

int32_t csnarray_padcomp_in_k_init(CSOUND *csound, CSN_PADCOMPLEX_IN *p) {
    return csnarray_pad_in_k_scratch_init(csound, p->source_handle, CSN_COMPLEX, &p->registry, &p->scratch);
}

int32_t csnarray_pad_in_k_deinit(CSOUND *csound, CSN_PAD_IN *p) {
    if (p->scratch != NULL) {
        if (p->scratch->data != NULL) {
            csound->Free(csound, p->scratch->data);
        }
        csound->Free(csound, p->scratch);
        p->scratch = NULL;
    }
    return OK;
}

int32_t csnarray_padcomp_in_k_deinit(CSOUND *csound, CSN_PADCOMPLEX_IN *p) {
    if (p->scratch != NULL) {
        if (p->scratch->data != NULL) {
            csound->Free(csound, p->scratch->data);
        }
        csound->Free(csound, p->scratch);
        p->scratch = NULL;
    }
    return OK;
}

/* Fits the scratch to the padded shape, fills it and copies it back over the
   source array. The caller holds the registry lock. */
static int32_t pad_in_k_commit(CSOUND *csound, OPDS *h, CSN_ARRAY **scratch, CSN_ARRAY *source_arr, bool rt_locked, const uint32_t *new_shape, double value, COMPLEXDAT *valuecomp, int32_t axis, uint32_t before) {
    uint32_t source_ndim = source_arr->ndim;
    size_t requested_size = 0;
    if (get_array_size_from_shape(&requested_size, source_ndim, new_shape) != OK) {
        return csn_locked_perf_error(csound, h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSN_ARRAY *temp = *scratch;
    bool have_same_shape = memcmp(temp->shape, new_shape, sizeof(uint32_t) * CSN_MAX_DIMS) == 0;
    bool request_changed = !have_same_shape || source_ndim != temp->ndim || temp->capacity < requested_size || source_arr->itype != temp->itype;

    if (request_changed) {
        bool needs_realloc = temp->capacity < requested_size || source_arr->itype != temp->itype;
        if (needs_realloc) {
            if (rt_locked) {
                return rt_growth_refused(csound, h, source_arr, requested_size);
            }
            size_t new_capacity = requested_size > 0 ? requested_size * 2 : 1;
            double *new_realloc = csound->ReAlloc(csound, temp->data, sizeof(double) * new_capacity * source_arr->itype);
            if (new_realloc == NULL) {
                csound->Free(csound, temp->data);
                csound->Free(csound, temp);
                *scratch = NULL;
                return csn_locked_perf_error(csound, h, "[csnarray] Out of memory: allocation failed");
            }
            temp->data = new_realloc;
            temp->capacity = new_capacity;
        }
        set_csnarray_layout(temp, source_ndim, new_shape, requested_size, source_arr->itype);
    }

    /* The source only reallocates when the padded image outgrows it, never on
       a pass that still fits. */
    int32_t res = ensure_mutation_capacity(csound, h, source_arr, requested_size, rt_locked);
    if (res != OK) return res;

    pad_assign_value(source_arr, temp, value, valuecomp, axis, before);
    travase_csnarray(source_arr, temp);
    return OK;
}

int32_t csnarray_pad_in_k(CSOUND *csound, CSN_PAD_IN *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    if (p->scratch == NULL) {
        return csound->PerfError(csound, &p->h, "[csnarray] Internal error: temporary buffer not available");
    }

    const MYFLT *axis_in = p->INOCOUNT > 5 ? p->arg_b : NULL;
    double trig = (double) *p->arg_a;

    if (trig == 0.0) return OK;

    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    int32_t axis = 0;
    uint32_t before = 0;
    uint32_t after = 0;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    res = pad_body(csound, &p->h, reg, &source_arr, p->source_handle->id, axis_in, p->before, p->after, &axis, &before, &after, new_shape, CSN_REAL);
    if (res != OK) goto done;

    if (CAN_REUSE_ELEMENTWISE(&p->k_data, p->source_handle->id, source_arr, 0, NULL, NULL, (double) *p->value, (double) axis)
        && p->k_data.prev_index == before && p->k_data.prev_roll_shift == (int32_t) after) {
        goto done;
    }

    res = pad_in_k_commit(csound, &p->h, &p->scratch, source_arr, csn_slot_rt_locked(reg, p->source_handle->id), new_shape, (double) *p->value, NULL, axis, before);
    if (res != OK) goto done;
    PUBLISH_ELEMENTWISE(&p->k_data, p->source_handle->id, source_arr, 0, NULL, NULL, (double) *p->value, (double) axis);
    p->k_data.prev_index = before;
    p->k_data.prev_roll_shift = (int32_t) after;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_padcomp_in_k(CSOUND *csound, CSN_PADCOMPLEX_IN *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    if (p->scratch == NULL) {
        return csound->PerfError(csound, &p->h, "[csnarray] Internal error: temporary buffer not available");
    }

    const MYFLT *axis_in = p->INOCOUNT > 5 ? p->arg_b : NULL;
    double trig = (double) *p->arg_a;

    if (trig == 0.0) return OK;
    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    int32_t axis = 0;
    uint32_t before = 0;
    uint32_t after = 0;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    res = pad_body(csound, &p->h, reg, &source_arr, p->source_handle->id, axis_in, p->before, p->after, &axis, &before, &after, new_shape, CSN_COMPLEX);
    if (res != OK) goto done;

    double fill_re = 0.0, fill_im = 0.0;
    complexdat_to_rect(p->value, &fill_re, &fill_im);
    if (CAN_REUSE_ELEMENTWISE(&p->k_data, p->source_handle->id, source_arr, 0, NULL, NULL, fill_re, fill_im)
        && (int32_t) p->k_data.prev_axis_u == axis
        && p->k_data.prev_index == before && p->k_data.prev_roll_shift == (int32_t) after) { goto done; }

    res = pad_in_k_commit(csound, &p->h, &p->scratch, source_arr, csn_slot_rt_locked(reg, p->source_handle->id), new_shape, 0.0, p->value, axis, before);
    if (res != OK) goto done;
    PUBLISH_ELEMENTWISE(&p->k_data, p->source_handle->id, source_arr, 0, NULL, NULL, fill_re, fill_im);
    p->k_data.prev_axis_u = (uint32_t) axis;
    p->k_data.prev_index = before;
    p->k_data.prev_roll_shift = (int32_t) after;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

/* Reports whether anything actually moved. The loop already reads every
   element, so an in-place caller gets an exact answer for free and can leave
   the array's version alone on the passes that clip nothing — a k-rate
   csnclip.in on a settled array would otherwise announce a new generation to
   every consumer on every pass. */
static bool clip_value(double min_value, double max_value, CSN_ARRAY *arr) {
    bool changed = false;
    for (size_t i = 0; i < arr->size; ++i) {
        double value = arr->data[i];
        if (value < min_value) {
            arr->data[i] = min_value;
            changed = true;
        }
        if (value > max_value) {
            arr->data[i] = max_value;
            changed = true;
        }
    }
    return changed;
}

int32_t csnarray_clip(CSOUND *csound, CSN_CLIP *p) {
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

    memcpy(p->array->data, source_arr->data, sizeof(double) * source_arr->size);
    (void) clip_value((double) *p->min, (double) *p->max, p->array);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_clip_k_init(CSOUND *csound, CSN_CLIP *p) {
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

int32_t csnarray_clip_k(CSOUND *csound, CSN_CLIP *p) {
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

    size_t req_size = 0;
    if (get_array_size_from_shape(&req_size, source_arr->ndim, source_arr->shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSN_ARRAY *arr = NULL;
    size_t logical_size = source_arr->size == 0 ? 0 : req_size;
    /* Clipping never moves a value out of its cell, so an array may clip
       itself as long as the layout holds still. */
    res = CHECK_SELF_ALIAS_CELL_LOCAL(csound, &p->h, &p->k_data, source_handle, source_arr, source_arr->ndim, source_arr->shape, source_arr->itype);
    if (res != OK) goto done;

    CSN_SLOT *reuse_slot = get_slot(p->k_data.registry, p->k_data.owned_handle);
    if (reuse_slot != NULL && CAN_REUSE_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, reuse_slot->array, (double) *p->min, (double) *p->max)) {
        p->handle->id = p->k_data.owned_handle;
        goto done;
    }

    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, source_arr->ndim, source_arr->shape, logical_size, source_arr->itype, err);
    if (res != OK) goto done;
    p->array = arr;

    /* Skipped when the source is the destination: memcpy over itself is
       undefined, and there is nothing to move. */
    if (p->array->data != source_arr->data) {
        memcpy(p->array->data, source_arr->data, sizeof(double) * source_arr->size);
    }
    (void) clip_value((double) *p->min, (double) *p->max, p->array);

    SET_KDATA_END(p, source_arr->shape, source_arr->ndim, source_arr->itype);
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, arr, (double) *p->min, (double) *p->max);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_clip_in(CSOUND *csound, CSN_CLIP_IN *p) {
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

    if (clip_value((double) *p->min, (double) *p->max, source_arr)) {
        update_array_data_version(&source_arr->version);
    }

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_clip_in_k_init(CSOUND *csound, CSN_CLIP_IN *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    p->registry = reg;
    return OK;
}

int32_t csnarray_clip_in_k(CSOUND *csound, CSN_CLIP_IN *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    uint32_t source_handle = p->source_handle->id;
    int32_t res = OK;

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

    if (CAN_REUSE_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, NULL, (double) *p->min, (double) *p->max)) {
        csound->UnlockMutex(reg->mutex);
        return res;
    }

    if (clip_value((double) *p->min, (double) *p->max, source_arr)) {
        update_array_data_version(&source_arr->version);
    }
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, NULL, (double) *p->min, (double) *p->max);

    csound->UnlockMutex(reg->mutex);
    return res;
}
