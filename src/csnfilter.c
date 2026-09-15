/* Opcode implementations for the filter family.
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

static void medfilt1d_assign_value(CSN_ARRAY *y, CSN_ARRAY *x, CSN_SCRATCH *kernel, size_t kernel_size, int32_t axis) {
    if (axis == -1) {
        sliding_median_slice(y->data, x->data, kernel->scratch, x->size, 1U, kernel_size, CSN_MEDIAN_EDGE_ZERO);
        return;
    }

    uint32_t reduced_shape[CSN_MAX_DIMS] = {0};
    uint32_t reduced_ndim = 0;
    size_t slice_count = 1;
    for (uint32_t i = 0; i < x->ndim; ++i) {
        if (i != (uint32_t) axis) {
            reduced_shape[reduced_ndim++] = x->shape[i];
            slice_count *= x->shape[i];
        }
    }

    size_t src_stride = x->strides[axis];
    for (size_t linear = 0; linear < slice_count; ++linear) {
        uint32_t dst_coords[CSN_MAX_DIMS] = {0};
        uint32_t src_coords[CSN_MAX_DIMS] = {0};

        from_linear_to_coords(dst_coords, reduced_shape, linear, reduced_ndim);
        for (uint32_t i = 0, j = 0; i < x->ndim; ++i) {
            src_coords[i] = (i == (uint32_t) axis) ? 0 : dst_coords[j++];
        }

        size_t src_base = from_coords_to_offset(src_coords, x->strides, x->ndim);
        size_t dst_base = from_coords_to_offset(src_coords, y->strides, y->ndim);
        sliding_median_slice(y->data + dst_base, x->data + src_base, kernel->scratch, x->shape[axis], src_stride, kernel_size, CSN_MEDIAN_EDGE_ZERO);
    }
}

/* The box of kernel_shape is centred on each element and what falls outside the
   array counts as zero, as scipy.signal.medfilt does. kernel is the gather
   buffer, one cell per kernel element. */
static void medfilt_assign_value(CSN_ARRAY *y, CSN_ARRAY *x, double *kernel, size_t kernel_size, uint32_t *kernel_shape) {
    uint32_t dst_coords[CSN_MAX_DIMS] = {0};
    uint32_t knl_coords[CSN_MAX_DIMS] = {0};
    uint32_t src_coords[CSN_MAX_DIMS] = {0};
    for (size_t linear = 0; linear < y->size; ++linear) {
        from_linear_to_coords(dst_coords, y->shape, linear, y->ndim);
        size_t count = 0;
        for (size_t i = 0; i < kernel_size; i++) {
            from_linear_to_coords(knl_coords, kernel_shape, i, x->ndim);
            bool valid = true;
            for (uint32_t d = 0; d < x->ndim; d++) {
                int64_t coord = (int64_t) dst_coords[d] + knl_coords[d] - (int64_t) (kernel_shape[d] / 2);
                if (coord < 0 || coord >= (int64_t) x->shape[d]) {
                    valid = false;
                    break;
                }
                src_coords[d] = (uint32_t) coord;
            }

            if (valid) {
                size_t src_offset = from_coords_to_offset(src_coords, x->strides, x->ndim);
                kernel[count++] = x->data[src_offset];
            } else {
                kernel[count++] = 0.0;
            }
        }
        y->data[linear] = median_of_scratch(kernel, count);
    }
}

/* The N-D filter gathers a box around each element, so unlike the 1-D one it
   cannot read the array it is rewriting: src_copy holds the source as it was
   when the pass started. */
static void medfilt_in_assign_value(CSN_ARRAY *x, double *kernel, double *src_copy, size_t kernel_size, uint32_t *kernel_shape) {
    memcpy(src_copy, x->data, sizeof(double) * x->size);

    uint32_t dst_coords[CSN_MAX_DIMS] = {0};
    uint32_t knl_coords[CSN_MAX_DIMS] = {0};
    uint32_t src_coords[CSN_MAX_DIMS] = {0};
    for (size_t linear = 0; linear < x->size; ++linear) {
        from_linear_to_coords(dst_coords, x->shape, linear, x->ndim);
        size_t count = 0;
        for (size_t i = 0; i < kernel_size; i++) {
            from_linear_to_coords(knl_coords, kernel_shape, i, x->ndim);
            bool valid = true;
            for (uint32_t d = 0; d < x->ndim; d++) {
                int64_t coord = (int64_t) dst_coords[d] + knl_coords[d] - (int64_t) (kernel_shape[d] / 2);
                if (coord < 0 || coord >= (int64_t) x->shape[d]) {
                    valid = false;
                    break;
                }
                src_coords[d] = (uint32_t) coord;
            }

            if (valid) {
                size_t src_offset = from_coords_to_offset(src_coords, x->strides, x->ndim);
                kernel[count++] = src_copy[src_offset];
            } else {
                kernel[count++] = 0.0;
            }
        }
        x->data[linear] = median_of_scratch(kernel, count);
    }
}

int32_t csnarray_medfilt_deinit(CSOUND *csound, CSN_MEDFILT *p) {
    deinit_scratch(csound, &p->buffer);
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

static int32_t csnarray_medfilt1d_init_helper(CSOUND *csound, CSN_MEDFILT *p, const MYFLT *axis_in) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    const char *err = NULL;

    if (!IS_VALID_LENGTH((double) *p->kernel_size)) {
        return csound->InitError(csound, "[csnarray] Invalid kernel size");
    }
    size_t kernel_size = (size_t) *p->kernel_size;
    if (kernel_size % 2 == 0) {
        return csound->InitError(csound, "[csnarray] Kernel size must be odd");
    }

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = slot->array;
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    if (source_arr->itype != CSN_REAL) {
        res = csound->InitError(csound, "[csnarray] medfilt requires real-array");
        goto done;
    }

    CSN_AXIS_SPEC axis_spec = csn_normalize_axis(axis_in, source_ndim, CSN_AXIS_DEFAULT_FLATTEN);
    if (axis_spec.kind == CSN_AXIS_INVALID) {
        double value = axis_in == NULL ? 0.0 : (double) *axis_in;
        res = csound->InitError(csound, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: finite integers %d..%u)", value, source_ndim, -(int32_t) source_ndim, source_ndim - 1U);
        goto done;
    }
    int32_t axis = axis_spec.kind == CSN_AXIS_FLATTEN ? -1 : (int32_t) axis_spec.index;

    p->buffer.scratch = csound->Calloc(csound, sizeof(double) * sliding_median_scratch_size(kernel_size));
    if (p->buffer.scratch == NULL) {
        res = csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
        goto done;
    }
    p->buffer.scratch_capacity = sliding_median_scratch_size(kernel_size);

    uint32_t new_ndim = axis == -1 ? 1U : source_ndim;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    if (axis == -1) {
        new_shape[0] = (uint32_t) source_arr->size;
    } else {
        memcpy(new_shape, source_shape, sizeof(new_shape));
    }

    if (create_csnarray_locked(csound, reg, &p->h, new_ndim, new_shape, &p->array, p->handle, &source_handle, 1U, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    medfilt1d_assign_value(p->array, source_arr, &p->buffer, kernel_size, axis);
    SET_KDATA_BEGIN(p, reg);
    set_array_version(&p->k_data.prev_output_version, &p->array->version);
    p->k_data.prev_axis_i = axis;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_medfilt1d(CSOUND *csound, CSN_MEDFILT *p) {
    const MYFLT *axis_in = p->INOCOUNT > 2 ? p->axis : NULL;
    return csnarray_medfilt1d_init_helper(csound, p, axis_in);
}

int32_t csnarray_medfilt1d_k_init(CSOUND *csound, CSN_MEDFILT *p) {
    const MYFLT *axis_in = p->INOCOUNT > 3 ? p->trig : NULL;
    return csnarray_medfilt1d_init_helper(csound, p, axis_in);
}

static int32_t medfilt1d_in_assign_value(CSN_ARRAY *source_arr, double *median_buffer, int32_t axis, size_t winsize) {
    double *source = source_arr->data;

    if (axis == -1) {
        sliding_median_slice(source_arr->data, source, median_buffer, source_arr->size, 1U, winsize, CSN_MEDIAN_EDGE_ZERO);
        return OK;
    }

    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    uint32_t reduced_shape[CSN_MAX_DIMS] = {0};
    uint32_t reduced_ndim = 0;
    size_t slice_count = 1;
    for (uint32_t i = 0; i < source_ndim; ++i) {
        if (i != (uint32_t) axis) {
            reduced_shape[reduced_ndim++] = source_shape[i];
            slice_count *= source_shape[i];
        }
    }

    size_t src_stride = source_arr->strides[axis];
    for (size_t linear = 0; linear < slice_count; ++linear) {
        uint32_t dst_coords[CSN_MAX_DIMS] = {0};
        uint32_t src_coords[CSN_MAX_DIMS] = {0};

        from_linear_to_coords(dst_coords, reduced_shape, linear, reduced_ndim);
        for (uint32_t i = 0, j = 0; i < source_ndim; ++i) {
            src_coords[i] = (i == (uint32_t) axis) ? 0 : dst_coords[j++];
        }

        size_t src_base = from_coords_to_offset(src_coords, source_arr->strides, source_ndim);
        sliding_median_slice(source_arr->data + src_base, source + src_base, median_buffer, source_shape[axis], src_stride, winsize, CSN_MEDIAN_EDGE_ZERO);
    }

    return OK;
}

static int32_t csnarray_medfilt1d_in_init_helper(CSOUND *csound, CSN_MEDFILT_IN *p, const MYFLT *axis_in) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;
    int32_t res = OK;

    if (!IS_VALID_LENGTH((double) *p->kernel_size)) {
        return csound->InitError(csound, "[csnarray] Invalid kernel size");
    }
    size_t kernel_size = (size_t) *p->kernel_size;
    if (kernel_size % 2 == 0) {
        return csound->InitError(csound, "[csnarray] Kernel size must be odd");
    }

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = slot->array;
    uint32_t source_ndim = source_arr->ndim;

    if (source_arr->itype != CSN_REAL) {
        res = csound->InitError(csound, "[csnarray] medfilt requires real-array");
        goto done;
    }

    CSN_AXIS_SPEC axis_spec = csn_normalize_axis(axis_in, source_ndim, CSN_AXIS_DEFAULT_FLATTEN);
    if (axis_spec.kind == CSN_AXIS_INVALID) {
        double value = axis_in == NULL ? 0.0 : (double) *axis_in;
        res = csound->InitError(csound, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: finite integers %d..%u)", value, source_ndim, -(int32_t) source_ndim, source_ndim - 1U);
        goto done;
    }
    int32_t axis = axis_spec.kind == CSN_AXIS_FLATTEN ? -1 : (int32_t) axis_spec.index;

    p->buffer.scratch = csound->Calloc(csound, sizeof(double) * sliding_median_scratch_size(kernel_size));
    if (p->buffer.scratch == NULL) {
        res = csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
        goto done;
    }
    p->buffer.scratch_capacity = sliding_median_scratch_size(kernel_size);

    medfilt1d_in_assign_value(source_arr, p->buffer.scratch, axis, kernel_size);
    update_array_data_version(&source_arr->version);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_medfilt1d_in(CSOUND *csound, CSN_MEDFILT_IN *p) {
    const MYFLT *axis_in = p->INOCOUNT > 2 ? p->axis : NULL;
    return csnarray_medfilt1d_in_init_helper(csound, p, axis_in);
}

int32_t csnarray_medfilt1d_k(CSOUND *csound, CSN_MEDFILT *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    uint32_t owned_handle = p->k_data.owned_handle;
    CHECK_REG_HANDLE(csound, &p->h, reg, owned_handle);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    const char *err = NULL;

    res = CHECK_SELF_ALIAS(csound, &p->h, &p->k_data, source_handle, 0);
    if (res != OK) return res;

    CHECK_KTRIG(p->axis);
    size_t kernel_size = (size_t) *p->kernel_size;

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
    }

    CSN_ARRAY *source_arr = slot->array;
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    if (source_arr->itype != CSN_REAL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] medfilt requires real-array");
    }

    const MYFLT *axis_in = p->INOCOUNT > 3 ? p->trig : NULL;
    CSN_AXIS_SPEC axis_spec = csn_normalize_axis(axis_in, source_ndim, CSN_AXIS_DEFAULT_FLATTEN);
    if (axis_spec.kind == CSN_AXIS_INVALID) {
        double value = axis_in == NULL ? 0.0 : (double) *axis_in;
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: finite integers %d..%u)", value, source_ndim, -(int32_t) source_ndim, source_ndim - 1U);
    }
    int32_t axis = axis_spec.kind == CSN_AXIS_FLATTEN ? -1 : (int32_t) axis_spec.index;

    bool is_same_axis = p->k_data.prev_axis_i == axis;
    if (p->array != NULL && is_same_axis && CAN_REUSE_LAST_RESULT(&p->k_data, source_handle, source_arr, p->array)) {
        p->handle->id = owned_handle;
        goto done;
    }

    uint32_t new_ndim = axis == -1 ? 1U : source_ndim;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    if (axis == -1) {
        new_shape[0] = (uint32_t) source_arr->size;
    } else {
        memcpy(new_shape, source_shape, sizeof(new_shape));
    }

    size_t req_size = 0;
    if (get_array_size_from_shape(&req_size, new_ndim, new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSN_ARRAY *arr = NULL;
    size_t logical_size = source_arr->size == 0 ? 0 : req_size;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, new_ndim, new_shape, logical_size, CSN_REAL, err);
    if (res != OK) goto done;
    p->array = arr;

    medfilt1d_assign_value(p->array, source_arr, &p->buffer, kernel_size, axis);

    SET_KDATA_END(p, new_shape, new_ndim, CSN_REAL);
    PUBLISH_DERIVED_RESULT(&p->k_data, source_handle, source_arr, p->array);
    p->k_data.prev_axis_i = axis;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_medfilt_in_deinit(CSOUND *csound, CSN_MEDFILT_IN *p) {
    deinit_scratch(csound, &p->buffer);
    return OK;
}

int32_t csnarray_medfilt1d_in_k_init(CSOUND *csound, CSN_MEDFILT_IN *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    p->k_data.registry = reg;
    int32_t res = OK;

    if (!IS_VALID_LENGTH((double) *p->kernel_size)) {
        return csound->InitError(csound, "[csnarray] Invalid kernel size");
    }
    size_t kernel_size = (size_t) *p->kernel_size;
    if (kernel_size % 2 == 0) {
        return csound->InitError(csound, "[csnarray] Kernel size must be odd");
    }

    p->buffer.scratch = NULL;
    p->buffer.scratch_capacity = 0;

    csound->LockMutex(reg->mutex);
    CSN_SCRATCH *reserved = &p->buffer;
    reserved->scratch = csound->Calloc(csound, sizeof(double) * sliding_median_scratch_size(kernel_size));
    if (reserved->scratch == NULL) {
        res = csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
        goto done;
    }
    reserved->scratch_capacity = sliding_median_scratch_size(kernel_size);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_medfilt1d_in_k(CSOUND *csound, CSN_MEDFILT_IN *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    uint32_t source_handle = p->source_handle->id;
    size_t kernel_size = (size_t) *p->kernel_size;
    int32_t res = OK;

    CHECK_KTRIG(p->axis);

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
    }

    CSN_ARRAY *source_arr = slot->array;
    uint32_t source_ndim = source_arr->ndim;

    if (source_arr->itype != CSN_REAL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] medfilt requires real-array");
    }

    const MYFLT *axis_in = p->INOCOUNT > 3 ? p->trig : NULL;
    CSN_AXIS_SPEC axis_spec = csn_normalize_axis(axis_in, source_ndim, CSN_AXIS_DEFAULT_FLATTEN);
    if (axis_spec.kind == CSN_AXIS_INVALID) {
        double value = axis_in == NULL ? 0.0 : (double) *axis_in;
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: finite integers %d..%u)", value, source_ndim, -(int32_t) source_ndim, source_ndim - 1U);
    }
    int32_t axis = axis_spec.kind == CSN_AXIS_FLATTEN ? -1 : (int32_t) axis_spec.index;
    bool moved = SOURCE_HAS_MOVED(&p->k_data, source_handle, source_arr);
    bool is_same_axis = axis == p->k_data.prev_axis_i;
    if (is_same_axis && !moved) goto done;

    medfilt1d_in_assign_value(source_arr, p->buffer.scratch, axis, kernel_size);
    PUBLISH_INPLACE_WRITE(&p->k_data, source_handle, source_arr, false, false, false);
    p->k_data.prev_axis_i = axis;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_medfilt_nd_deinit(CSOUND *csound, CSN_MEDFILT_ND *p) {
    deinit_scratch(csound, &p->buffer);
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

int32_t csnarray_medfilt_ndarr_deinit(CSOUND *csound, CSN_MEDFILT_ND_ARR *p) {
    deinit_scratch(csound, &p->buffer);
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

int32_t csnarray_medfilt_nd_in_deinit(CSOUND *csound, CSN_MEDFILT_ND_IN *p) {
    deinit_scratch(csound, &p->buffer);
    return OK;
}

int32_t csnarray_medfilt_ndarr_in_deinit(CSOUND *csound, CSN_MEDFILT_ND_ARR_IN *p) {
    deinit_scratch(csound, &p->buffer);
    return OK;
}

static int32_t csnarray_medfilt_helper(CSOUND *csound, OPDS *h, uint32_t source_handle, const MYFLT *kernel_size_in, ARRAYDAT *kernel_shape_in, CSN_SCRATCH *buffer, CSNREF *p_handle, CSN_ARRAY **p_array, K_DATA *k_data, bool is_kernel_shape, uint32_t *kernel_shape, size_t *kernel_total_size) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    const char *err = NULL;

    size_t kernel_size = 0;
    if (kernel_size_in != NULL) {
        if (!IS_VALID_LENGTH((double) *kernel_size_in)) {
            return csound->InitError(csound, "[csnarray] Invalid kernel size");
        }
        kernel_size = (size_t) *kernel_size_in;
        if (kernel_size % 2 == 0) {
            return csound->InitError(csound, "[csnarray] Kernel size must be odd");
        }
    }

    if (is_kernel_shape && kernel_shape_in == NULL) {
        return csound->InitError(csound, "[csnarray] Invalid kernel shape, NULL passed");
    }

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = slot->array;
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    if (source_arr->itype != CSN_REAL) {
        res = csound->InitError(csound, "[csnarray] medfilt requires real-array");
        goto done;
    }

    uint32_t new_ndim = source_ndim;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    memcpy(new_shape, source_shape, sizeof(new_shape));

    if (create_csnarray_locked(csound, reg, h, new_ndim, new_shape, p_array, p_handle, &source_handle, 1U, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    if (is_kernel_shape) {
        if (kernel_shape_in == NULL || kernel_shape_in->data == NULL || kernel_shape_in->sizes == NULL) {
            res = csound->InitError(csound, "[csnarray] Invalid kernel shape: NULL passed");
            goto done;
        }
        if ((uint32_t) kernel_shape_in->sizes[0] != source_ndim) {
            res = csound->InitError(csound, "[csnarray] Kernel shape holds %d sizes but the source is %u-D: one size per axis", kernel_shape_in->sizes[0], source_ndim);
            goto done;
        }
        for (uint32_t i = 0; i < source_ndim; i++) {
            /* Screened before the cast, so a fractional size is refused rather
               than quietly truncated. */
            double extent = (double) kernel_shape_in->data[i];
            if (!IS_VALID_LENGTH(extent)) {
                res = csound->InitError(csound, "[csnarray] Invalid kernel size");
                goto done;
            }
            if (((size_t) extent) % 2 == 0) {
                res = csound->InitError(csound, "[csnarray] Kernel size must be odd");
                goto done;
            }
            kernel_shape[i] = (uint32_t) extent;
        }
    } else {
        for (uint32_t i = 0; i < source_ndim; i++) {
            kernel_shape[i] = (uint32_t) kernel_size;
        }
    }

    if (get_array_size_from_shape(kernel_total_size, source_ndim, kernel_shape) != OK) {
        res = csound->InitError(csound, "[csnarray] Invalid shape or element count exceeds the configured limit");
        goto done;
    }

    size_t kernel_cap = *kernel_total_size > 0 ? *kernel_total_size : 1;
    buffer->scratch = csound->Calloc(csound, sizeof(double) * kernel_cap);
    if (buffer->scratch == NULL) {
        res = csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
        goto done;
    }
    buffer->scratch_capacity = kernel_cap;

    medfilt_assign_value(*p_array, source_arr, (double *) buffer->scratch, *kernel_total_size, kernel_shape);
    SET_FROM_KDATA_WITH_ID_BEGIN(*k_data, reg, new_shape, new_ndim, CSN_REAL, p_handle->id);
    set_array_version(&k_data->prev_output_version, &(*p_array)->version);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_medfilt_in_helper(CSOUND *csound, uint32_t source_handle, const MYFLT *kernel_size_in, ARRAYDAT *kernel_shape_in, CSN_SCRATCH *buffer, bool is_kernel_shape) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;

    size_t kernel_size = 0;
    if (kernel_size_in != NULL) {
        if (!IS_VALID_LENGTH((double) *kernel_size_in)) {
            return csound->InitError(csound, "[csnarray] Invalid kernel size");
        }
        kernel_size = (size_t) *kernel_size_in;
        if (kernel_size % 2 == 0) {
            return csound->InitError(csound, "[csnarray] Kernel size must be odd");
        }
    }

    if (is_kernel_shape && kernel_shape_in == NULL) {
        return csound->InitError(csound, "[csnarray] Invalid kernel shape, NULL passed");
    }

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = slot->array;
    uint32_t source_ndim = source_arr->ndim;

    if (source_arr->itype != CSN_REAL) {
        res = csound->InitError(csound, "[csnarray] medfilt requires real-array");
        goto done;
    }

    uint32_t kernel_shape[CSN_MAX_DIMS] = {0};
    if (is_kernel_shape) {
        if (kernel_shape_in == NULL || kernel_shape_in->data == NULL || kernel_shape_in->sizes == NULL) {
            res = csound->InitError(csound, "[csnarray] Invalid kernel shape: NULL passed");
            goto done;
        }
        if ((uint32_t) kernel_shape_in->sizes[0] != source_ndim) {
            res = csound->InitError(csound, "[csnarray] Kernel shape holds %d sizes but the source is %u-D: one size per axis", kernel_shape_in->sizes[0], source_ndim);
            goto done;
        }
        for (uint32_t i = 0; i < source_ndim; i++) {
            /* Screened before the cast, so a fractional size is refused rather
               than quietly truncated. */
            double extent = (double) kernel_shape_in->data[i];
            if (!IS_VALID_LENGTH(extent)) {
                res = csound->InitError(csound, "[csnarray] Invalid kernel size");
                goto done;
            }
            if (((size_t) extent) % 2 == 0) {
                res = csound->InitError(csound, "[csnarray] Kernel size must be odd");
                goto done;
            }
            kernel_shape[i] = (uint32_t) extent;
        }
    } else {
        for (uint32_t i = 0; i < source_ndim; i++) {
            kernel_shape[i] = (uint32_t) kernel_size;
        }
    }

    size_t kernel_total_size = 0;
    if (get_array_size_from_shape(&kernel_total_size, source_ndim, kernel_shape) != OK) {
        res = csound->InitError(csound, "[csnarray] Invalid shape or element count exceeds the configured limit");
        goto done;
    }

    size_t gather = kernel_total_size > 0 ? kernel_total_size : 1;
    size_t kernel_cap = gather + source_arr->capacity;
    buffer->scratch = csound->Calloc(csound, sizeof(double) * kernel_cap);
    if (buffer->scratch == NULL) {
        res = csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
        goto done;
    }
    buffer->scratch_capacity = kernel_cap;

    double *kernel = (double *) buffer->scratch;
    medfilt_in_assign_value(source_arr, kernel, kernel + gather, kernel_total_size, kernel_shape);
    update_array_data_version(&source_arr->version);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}


int32_t csnarray_medfilt(CSOUND *csound, CSN_MEDFILT_ND *p) {
    return csnarray_medfilt_helper(csound, &p->h, p->source_handle->id, p->kernel_size, NULL, &p->buffer, p->handle, &p->array, &p->k_data, false, p->kernel_shape, &p->kernel_total_size);
}

int32_t csnarray_medfilt_arr(CSOUND *csound, CSN_MEDFILT_ND_ARR *p) {
    return csnarray_medfilt_helper(csound, &p->h, p->source_handle->id, NULL, p->kernel_sizes, &p->buffer, p->handle, &p->array, &p->k_data, true, p->kernel_shape, &p->kernel_total_size);
}

int32_t csnarray_medfilt_in(CSOUND *csound, CSN_MEDFILT_ND_IN *p) {
    return csnarray_medfilt_in_helper(csound, p->source_handle->id, p->kernel_size, NULL, &p->buffer, false);
}

int32_t csnarray_medfilt_arr_in(CSOUND *csound, CSN_MEDFILT_ND_ARR_IN *p) {
    return csnarray_medfilt_in_helper(csound, p->source_handle->id, NULL, p->kernel_sizes, &p->buffer, true);
}

static int32_t csnarray_medfilt_k_helper(CSOUND *csound, OPDS *h, uint32_t source_handle, size_t kernel_total_size, uint32_t *kernel_shape, CSN_SCRATCH *buffer, CSNREF *p_handle, CSN_ARRAY **p_array, K_DATA *k_data, const MYFLT *trig) {
    CSN_REGISTRY *reg = k_data->registry;
    uint32_t owned_handle = k_data->owned_handle;
    CHECK_REG_HANDLE(csound, h, reg, owned_handle);

    int32_t res = OK;
    const char *err = NULL;

    res = CHECK_SELF_ALIAS(csound, h, k_data, source_handle, 0);
    if (res != OK) return res;

    CHECK_KTRIG(trig);

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
    }

    CSN_ARRAY *source_arr = slot->array;
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    if (source_arr->itype != CSN_REAL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, h, "[csnarray] medfilt requires real-array");
    }

    if (source_ndim != k_data->prev_ndim) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, h, "[csnarray] Kernel was shaped for a %u-D source, which is now %u-D", k_data->prev_ndim, source_ndim);
    }

    if (CAN_REUSE_LAST_RESULT(k_data, source_handle, source_arr, *p_array)) {
        p_handle->id = owned_handle;
        goto done;
    }

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    memcpy(new_shape, source_shape, sizeof(new_shape));

    size_t requested_size = 0;
    if (get_array_size_from_shape(&requested_size, source_ndim, new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSN_ARRAY *arr = *p_array;
    size_t logical_size = source_arr->size == 0 ? 0 : requested_size;
    res = NEED_TO_UPDATE_SLOT(csound, h, &arr, k_data, NULL, source_ndim, new_shape, logical_size, CSN_REAL, err);
    if (res != OK) goto done;
    *p_array = arr;

    medfilt_assign_value(arr, source_arr, (double *) buffer->scratch, kernel_total_size, kernel_shape);
    SET_FROM_KDATA_END_WITH_ID(*k_data, p_handle, new_shape, source_ndim, CSN_REAL);
    PUBLISH_DERIVED_RESULT(k_data, source_handle, source_arr, arr);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_medfilt_k(CSOUND *csound, CSN_MEDFILT_ND *p) {
    return csnarray_medfilt_k_helper(csound, &p->h, p->source_handle->id, p->kernel_total_size, p->kernel_shape, &p->buffer, p->handle, &p->array, &p->k_data, p->trig);
}

int32_t csnarray_medfilt_arr_k(CSOUND *csound, CSN_MEDFILT_ND_ARR *p) {
    return csnarray_medfilt_k_helper(csound, &p->h, p->source_handle->id, p->kernel_total_size, p->kernel_shape, &p->buffer, p->handle, &p->array, &p->k_data, p->trig);
}

static int32_t csnarray_medfilt_in_k_init_helper(CSOUND *csound, K_DATA *k_data, uint32_t source_handle, const MYFLT *kernel_size_in, ARRAYDAT *kernel_shape_in, CSN_SCRATCH *buffer, bool is_kernel_shape, size_t *kernel_total_size, uint32_t *kernel_shape) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    k_data->registry = reg;
    int32_t res = OK;

    size_t kernel_size = 0;
    if (kernel_size_in != NULL) {
        if (!IS_VALID_LENGTH((double) *kernel_size_in)) {
            return csound->InitError(csound, "[csnarray] Invalid kernel size");
        }
        kernel_size = (size_t) *kernel_size_in;
        if (kernel_size % 2 == 0) {
            return csound->InitError(csound, "[csnarray] Kernel size must be odd");
        }
    }

    if (is_kernel_shape && kernel_shape_in == NULL) {
        return csound->InitError(csound, "[csnarray] Invalid kernel shape, NULL passed");
    }

    buffer->scratch = NULL;
    buffer->scratch_capacity = 0;

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = slot->array;
    uint32_t source_ndim = source_arr->ndim;

    if (source_arr->itype != CSN_REAL) {
        res = csound->InitError(csound, "[csnarray] medfilt requires real-array");
        goto done;
    }

    if (is_kernel_shape) {
        if (kernel_shape_in == NULL || kernel_shape_in->data == NULL || kernel_shape_in->sizes == NULL) {
            res = csound->InitError(csound, "[csnarray] Invalid kernel shape: NULL passed");
            goto done;
        }
        if ((uint32_t) kernel_shape_in->sizes[0] != source_ndim) {
            res = csound->InitError(csound, "[csnarray] Kernel shape holds %d sizes but the source is %u-D: one size per axis", kernel_shape_in->sizes[0], source_ndim);
            goto done;
        }
        for (uint32_t i = 0; i < source_ndim; i++) {
            /* Screened before the cast, so a fractional size is refused rather
               than quietly truncated. */
            double extent = (double) kernel_shape_in->data[i];
            if (!IS_VALID_LENGTH(extent)) {
                res = csound->InitError(csound, "[csnarray] Invalid kernel size");
                goto done;
            }
            if (((size_t) extent) % 2 == 0) {
                res = csound->InitError(csound, "[csnarray] Kernel size must be odd");
                goto done;
            }
            kernel_shape[i] = (uint32_t) extent;
        }
    } else {
        for (uint32_t i = 0; i < source_ndim; i++) {
            kernel_shape[i] = (uint32_t) kernel_size;
        }
    }

    if (get_array_size_from_shape(kernel_total_size, source_ndim, kernel_shape) != OK) {
        res = csound->InitError(csound, "[csnarray] Invalid shape or element count exceeds the configured limit");
        goto done;
    }

    /* The buffer holds the gather box followed by a copy of the source, which
       the box reads from: filtering in place would otherwise feed already
       filtered values back into the window. The copy is sized from the
       capacity, so a source that grows up to it needs no perf-time
       allocation. */
    size_t gather = *kernel_total_size > 0 ? *kernel_total_size : 1;
    size_t kernel_cap = gather + source_arr->capacity;
    CSN_SCRATCH *reserved = buffer;
    reserved->scratch = csound->Calloc(csound, sizeof(double) * kernel_cap);
    if (reserved->scratch == NULL) {
        res = csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
        goto done;
    }
    reserved->scratch_capacity = kernel_cap;
    k_data->prev_ndim = source_ndim;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_medfilt_in_k_init(CSOUND *csound, CSN_MEDFILT_ND_IN *p) {
    return csnarray_medfilt_in_k_init_helper(csound, &p->k_data, p->source_handle->id, p->kernel_size, NULL, &p->buffer, false, &p->kernel_total_size, p->kernel_shape);
}

int32_t csnarray_medfilt_arr_in_k_init(CSOUND *csound, CSN_MEDFILT_ND_ARR_IN *p) {
    return csnarray_medfilt_in_k_init_helper(csound, &p->k_data, p->source_handle->id, NULL, p->kernel_sizes, &p->buffer, true, &p->kernel_total_size, p->kernel_shape);
}

static int32_t csnarray_medfilt_in_k_helper(CSOUND *csound, OPDS *h, uint32_t source_handle, K_DATA *k_data, CSN_SCRATCH *buffer, const MYFLT *trig, size_t kernel_total_size, uint32_t *kernel_shape) {
    CSN_REGISTRY *reg = k_data->registry;
    CHECK_REGISTRY(csound, h, reg);

    int32_t res = OK;
    CHECK_KTRIG(trig);

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
    }

    CSN_ARRAY *source_arr = slot->array;
    uint32_t source_ndim = source_arr->ndim;

    if (source_arr->itype != CSN_REAL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, h, "[csnarray] medfilt requires real-array");
    }

    if (source_ndim != k_data->prev_ndim) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, h, "[csnarray] Kernel was shaped for a %u-D source, which is now %u-D", k_data->prev_ndim, source_ndim);
    }

    if (!SOURCE_HAS_MOVED(k_data, source_handle, source_arr)) goto done;

    /* The source may have grown past what init reserved, if something replaced
       the array in the slot. Refused outright on a real-time path. */
    size_t gather = kernel_total_size > 0 ? kernel_total_size : 1;
    res = csn_scratch_reserve(csound, h, slot->rt_locked, buffer, gather + source_arr->size, sizeof(double));
    if (res != OK) goto done;

    double *kernel = (double *) buffer->scratch;
    medfilt_in_assign_value(source_arr, kernel, kernel + gather, kernel_total_size, kernel_shape);
    PUBLISH_INPLACE_WRITE(k_data, source_handle, source_arr, false, false, false);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}


int32_t csnarray_medfilt_in_k(CSOUND *csound, CSN_MEDFILT_ND_IN *p) {
    return csnarray_medfilt_in_k_helper(csound, &p->h, p->source_handle->id, &p->k_data, &p->buffer, p->trig, p->kernel_total_size, p->kernel_shape);
}

int32_t csnarray_medfilt_arr_in_k(CSOUND *csound, CSN_MEDFILT_ND_ARR_IN *p) {
    return csnarray_medfilt_in_k_helper(csound, &p->h, p->source_handle->id, &p->k_data, &p->buffer, p->trig, p->kernel_total_size, p->kernel_shape);
}

