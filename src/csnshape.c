/* Opcode implementations for the shape family.
   The public opcode inventory remains centralized in csnum.c. */
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
            return csound->InitError(csound, "[csnarray] The num argument must be >= 0, got %d", num);
        }
    }

    int32_t res = OK;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle }, 1);

    uint32_t shape[CSN_MAX_DIMS] = { num, num };
    CSNARR_RES out = prepare_output(csound, &p->h, NULL, reg, p->handle, 2U, shape, itype, NULL, 0, CSN_ALIAS_REFUSE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    for (int32_t i = 0; i < num; i++) {
        size_t item = (size_t) i * (size_t) num + (size_t) i;
        p->handle->data[item * itype] = 1.0;
    }

    set_array_version(&p->k_data.prev_output_version, &p->handle->version);
    SET_KDATA_BEGIN(p->k_data, reg, shape, 2U, itype);
    p->k_data.prev_size = (size_t) num;
    p->is_published = false;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_identity(CSOUND *csound, CSN_IDENTITY *p) {
    return csnarray_identity_helper(csound, p, false);
}

int32_t csnarray_identity_k_init(CSOUND *csound, CSN_IDENTITY *p) {
    return csnarray_identity_helper(csound, p, true);
}

int32_t csnarray_identity_k(CSOUND *csound, CSN_IDENTITY *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, p->k_data.registry);

    ITEM_TYPE itype = CSN_ITYPE_FROM_ARG(*p->itype);
    if (!IS_VALID_VALUE((double) *p->num)) {
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid matrix size");
    }

    int32_t num = (int32_t) *p->num;
    if (num <= 0) {
        return csound->PerfError(csound, &p->h, "[csnarray] The num argument must be > 0, got %d", num);
    }

    CHECK_KTRIG(p->trig);
    int32_t res = OK;

    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle }, 1);

    bool is_same_result = is_same_array_version(&p->k_data.prev_output_version, &p->handle->version);
    bool is_same_size = (size_t) num == p->k_data.prev_size;
    if (p->is_published && is_same_result && is_same_size) goto done;

    uint32_t ndim = 2U;
    uint32_t shape[CSN_MAX_DIMS] = { num, num };
    CSNARR_RES out = prepare_output(csound, &p->h, &p->h, reg, p->handle, ndim, shape, itype, NULL, 0, CSN_ALIAS_REFUSE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    /* prepare_output hands back a zeroed buffer */
    for (int32_t i = 0; i < num; i++) {
        size_t item = (size_t) i * (size_t) num + (size_t) i;
        p->handle->data[item * itype] = 1.0;
    }

    set_array_version(&p->k_data.prev_output_version, &p->handle->version);
    SET_KDATA_END(p->k_data, shape, ndim, itype);
    p->k_data.prev_size = (size_t) num;
    p->is_published = true;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_reshape(CSOUND *csound, CSN_RESHAPE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

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
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle }, 2);

    CSN_ARRAY *arr = p->source_handle;

    /* Validated before allocating, so a rejected reshape does not publish a
       handle to a destination nobody asked for. */
    if (new_size != arr->size) {
        res = csound->InitError(csound, "[csnarray] Reshape size mismatch: source has %zu elements, new shape requires %zu", arr->size, new_size);
        goto done;
    }

    const CSN_ARRAY *ops[1] = { arr };
    CSNARR_RES out = prepare_output(csound, &p->h, NULL, reg, p->handle, ndim, shape, arr->itype, ops, 1, CSN_ALIAS_CUSTOM);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    if (out.aliased) {
        set_csnarray_layout(arr, ndim, shape, arr->size, arr->itype);
    } else {
        memcpy(p->handle->data, arr->data, sizeof(double) * arr->size * arr->itype);
    }

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

/* A computed k-shape normally still contains zeros during the init pass.
   Publish a valid output by copying the source's current layout; the first
   performance pass applies the requested reshape. prev_ndim stays 0, which no
   parsed shape has, so that first pass always runs. */
int32_t csnarray_reshape_k_init(CSOUND *csound, CSN_RESHAPE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle }, 2);

    memset(&p->k_data, 0, sizeof(p->k_data));
    p->k_data.registry = reg;

    CSN_ARRAY *source = p->source_handle;

    const CSN_ARRAY *ops[1] = { source };
    CSNARR_RES out = prepare_output(csound, &p->h, NULL, reg, p->handle, source->ndim, source->shape, source->itype, ops, 1, CSN_ALIAS_CUSTOM);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    if (!out.aliased) memcpy(p->handle->data, source->data, sizeof(double) * source->size * source->itype);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_reshape_k(CSOUND *csound, CSN_RESHAPE *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);

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
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle }, 2);

    CSN_ARRAY *source = p->source_handle;
    ITEM_TYPE itype = source->itype;

    if (requested_size != source->size) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Reshape size mismatch: source has %zu elements, new shape requires %zu", source->size, requested_size);
        goto done;
    }

    /* An output that is its own source is never reused, but reshaping it again
       to the same shape changes nothing and bumps no version. */
    bool is_same_shape = !IS_REQUEST_CHANGED(&p->k_data, ndim, itype, shape);
    if (is_same_shape && CAN_REUSE_LAST_RESULT(&p->k_data, source, p->handle)) goto done;

    const CSN_ARRAY *ops[1] = { source };
    CSNARR_RES out = prepare_output(csound, &p->h, &p->h, reg, p->handle, ndim, shape, itype, ops, 1, CSN_ALIAS_CUSTOM);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    if (out.aliased) {
        set_csnarray_layout(source, ndim, shape, requested_size, itype);
    } else {
        memcpy(p->handle->data, source->data, sizeof(double) * source->size * source->itype);
    }

    SET_KDATA_END(p->k_data, shape, ndim, itype);
    PUBLISH_DERIVED_RESULT(&p->k_data, source, p->handle);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_reshape_in(CSOUND *csound, CSN_RESHAPE_IN *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t ndim = 0;
    uint32_t shape[CSN_MAX_DIMS] = {0};

    int32_t res_shape = parse_shape_array(csound, p->new_shape, &ndim, shape);
    if (res_shape != OK) return res_shape;

    size_t new_size = 0;
    if (get_array_size_from_shape(&new_size, ndim, shape) != OK) {
        return csound->InitError(csound, "[csnarray] Shape is invalid or its element count exceeds the configured limit");
    }

    int32_t res = OK;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);

    CSN_ARRAY *arr = p->source_handle;

    if (new_size != arr->size) {
        res = csound->InitError(csound, "[csnarray] Reshape size mismatch: source has %zu elements, new shape requires %zu", arr->size, new_size);
        goto done;
    }

    /* Through set_csnarray_layout rather than by hand, so the array's shape and
       ndim counters move with the relayout. */
    set_csnarray_layout(arr, ndim, shape, arr->size, arr->itype);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

/* In-place k-rate reshape owns no output buffer, and a computed k-shape is
   applied during performance: the init only marks the array k-rate. prev_ndim
   stays 0, so the first pass always runs. */
int32_t csnarray_reshape_in_k_init(CSOUND *csound, CSN_RESHAPE_IN *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    stamp_globals(csound, &p->h);
    bool locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);
    csn_mark_krate(p->source_handle);
    if (locked) csn_unlock(csound, reg);

    memset(&p->k_data, 0, sizeof(p->k_data));
    p->k_data.registry = reg;
    return OK;
}

int32_t csnarray_reshape_in_k(CSOUND *csound, CSN_RESHAPE_IN *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);

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
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);

    CSN_ARRAY *source = p->source_handle;

    /* the whole version: someone else reshaping the array must be undone */
    bool is_same_source = is_same_array_version(&p->k_data.prev_source_version, &source->version);
    bool is_same_shape = !IS_REQUEST_CHANGED(&p->k_data, ndim, source->itype, shape);
    if (is_same_source && is_same_shape) goto done;

    if (requested_size != source->size) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Reshape size mismatch: source has %zu elements, new shape requires %zu", source->size, requested_size);
        goto done;
    }

    ITEM_TYPE itype = source->itype;
    set_csnarray_layout(source, ndim, shape, requested_size, itype);

    SET_KDATA_END(p->k_data, shape, ndim, itype);
    set_array_version(&p->k_data.prev_source_version, &source->version);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_flatten(CSOUND *csound, CSN_RESHAPE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle }, 2);

    CSN_ARRAY *source = p->source_handle;

    uint32_t shape[CSN_MAX_DIMS] = {0};
    shape[0] = (uint32_t) source->size;

    const CSN_ARRAY *ops[1] = { source };
    CSNARR_RES out = prepare_output(csound, &p->h, NULL, reg, p->handle, 1U, shape, source->itype, ops, 1, CSN_ALIAS_CUSTOM);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    if (out.aliased) {
        set_csnarray_layout(source, 1U, shape, source->size, source->itype);
    } else {
        memcpy(p->handle->data, source->data, sizeof(double) * source->size * source->itype);
    }

    SET_KDATA_BEGIN(p->k_data, reg, shape, 1U, source->itype);
    PUBLISH_DERIVED_RESULT(&p->k_data, source, p->handle);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_flatten_k(CSOUND *csound, CSN_RESHAPE *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);
    CHECK_KTRIG(p->trig);

    int32_t res = OK;
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle }, 2);

    CSN_ARRAY *source = p->source_handle;
    ITEM_TYPE itype = source->itype;

    uint32_t ndim = 1U;
    uint32_t shape[CSN_MAX_DIMS] = { (uint32_t) source->size };
    size_t requested_size = source->size;

    if (CAN_REUSE_LAST_RESULT(&p->k_data, source, p->handle)) goto done;

    const CSN_ARRAY *ops[1] = { source };
    CSNARR_RES out = prepare_output(csound, &p->h, &p->h, reg, p->handle, ndim, shape, itype, ops, 1, CSN_ALIAS_CUSTOM);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    if (out.aliased) {
        set_csnarray_layout(source, ndim, shape, requested_size, itype);
    } else {
        memcpy(p->handle->data, source->data, sizeof(double) * requested_size * itype);
    }

    SET_KDATA_END(p->k_data, shape, ndim, itype);
    PUBLISH_DERIVED_RESULT(&p->k_data, source, p->handle);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_flatten_in(CSOUND *csound, CSN_RESHAPE_IN *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);

    CSN_ARRAY *source = p->source_handle;
    if (csn_is_k_form(&p->h)) csn_mark_krate(source);

    uint32_t flat_shape[CSN_MAX_DIMS] = { (uint32_t) source->size };
    set_csnarray_layout(source, 1U, flat_shape, source->size, source->itype);

    SET_KDATA_BEGIN(p->k_data, reg, flat_shape, 1U, source->itype);
    PUBLISH_DERIVED_RESULT(&p->k_data, source, NULL);

    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_flatten_in_k(CSOUND *csound, CSN_RESHAPE_IN *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);
    CHECK_KTRIG(p->trig);

    int32_t res = OK;
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);

    CSN_ARRAY *source = p->source_handle;
    if (is_same_array_version(&p->k_data.prev_source_version, &source->version)) goto done;

    uint32_t ndim = 1U;
    uint32_t shape[CSN_MAX_DIMS] = { (uint32_t) source->size };

    ITEM_TYPE itype = source->itype;
    set_csnarray_layout(source, ndim, shape, source->size, itype);

    SET_KDATA_END(p->k_data, shape, ndim, itype);
    PUBLISH_DERIVED_RESULT(&p->k_data, source, NULL);

done:
    if (pres.locked) csn_unlock(csound, reg);
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

typedef enum {
    CSN_TRANSPOSE_AXES_OK = 0,
    CSN_TRANSPOSE_AXES_BAD_SIZE,
    CSN_TRANSPOSE_AXES_BAD_PERMUTATION
} CSN_TRANSPOSE_AXES;

/* The axes permutation, or the reversed order when the opcode was called
   without one. Callers pass INOCOUNT < 2: it tells the overloads apart, so the
   no-axes form does not depend on axes_arg happening to be NULL. The axes array is zero-filled past
   ndim, so two permutations compare whole. */
static CSN_TRANSPOSE_AXES transpose_axes(bool is_default, const ARRAYDAT *axes_arg, uint32_t ndim, uint32_t *axes) {
    memset(axes, 0, sizeof(uint32_t) * CSN_MAX_DIMS);
    if (is_default) {
        for (uint32_t i = 0; i < ndim; ++i) axes[i] = ndim - 1 - i;
        return CSN_TRANSPOSE_AXES_OK;
    }
    if (axes_arg->dimensions != 1 || axes_arg->sizes == NULL || axes_arg->sizes[0] != (int32_t) ndim) {
        return CSN_TRANSPOSE_AXES_BAD_SIZE;
    }
    if (transpose_axes_assign(axes_arg, axes, ndim) != OK) return CSN_TRANSPOSE_AXES_BAD_PERMUTATION;
    return CSN_TRANSPOSE_AXES_OK;
}

static int32_t transpose_axes_error(CSOUND *csound, OPDS *perf_h, CSN_TRANSPOSE_AXES code, uint32_t ndim) {
    if (code == CSN_TRANSPOSE_AXES_BAD_SIZE) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Axes argument must be a 1-D array of exactly %u elements, one per dimension", ndim);
    }
    return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Axes argument is not a valid permutation");
}

static void transpose_shape(const CSN_ARRAY *source, const uint32_t *axes, uint32_t *shape) {
    memset(shape, 0, sizeof(uint32_t) * CSN_MAX_DIMS);
    for (uint32_t i = 0; i < source->ndim; ++i) shape[i] = source->shape[axes[i]];
}

/* Writes the transpose of source into out, which already has the transposed
   layout unless it is source itself: then the result goes through buffer
   (size * itype doubles) and source takes the new layout afterwards. Data
   versions are the caller's. */
static int32_t transpose_into(CSN_ARRAY *source, CSN_ARRAY *out, double *buffer, const uint32_t *shape, const uint32_t *axes) {
    uint32_t ndim = source->ndim;
    size_t count = source->size * (size_t) source->itype;
    if (source->size > 0) {
        double *destination = out == source ? buffer : out->data;
        if (transpose_data_assign(source->data, destination, source->size, ndim, shape, source->strides, axes, source->itype) != OK) return NOTOK;
        if (out == source) memcpy(source->data, buffer, sizeof(double) * count);
    }
    if (out == source) set_csnarray_layout(source, ndim, shape, source->size, source->itype);
    return OK;
}

/* Serves csntranspose and the k forms' init. An output that is its own source
   is transposed through p->scratch, reserved here for the source's whole
   capacity; a k form leaves that case to the first pass, which transposes it
   again on every pass. With axes_at_k the axes are a k-array that may not hold
   its values yet: if they are not a permutation at init, the output only takes
   the source's layout and the first pass computes. is_default comes from the
   caller: the no-axes k form holds its trigger where the axes would be, and
   INOCOUNT, which counts the padded trigger, cannot tell it apart. */
static int32_t transpose_init_helper(CSOUND *csound, CSN_RESHAPE *p, bool is_default, bool axes_at_k) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle }, 2);

    /* a reinit reuses the struct */
    deinit_scratch(csound, &p->scratch);
    memset(&p->k_data, 0, sizeof(p->k_data));
    p->k_data.registry = reg;

    CSN_ARRAY *source = p->source_handle;
    uint32_t ndim = source->ndim;
    ITEM_TYPE itype = source->itype;

    uint32_t axes[CSN_MAX_DIMS];
    uint32_t shape[CSN_MAX_DIMS];
    CSN_TRANSPOSE_AXES code = transpose_axes(is_default, p->new_shape, ndim, axes);
    bool has_axes = code == CSN_TRANSPOSE_AXES_OK;
    if (!has_axes && !axes_at_k) {
        res = transpose_axes_error(csound, NULL, code, ndim);
        goto done;
    }
    if (has_axes) {
        transpose_shape(source, axes, shape);
    } else {
        memcpy(shape, source->shape, sizeof(shape));
    }

    const CSN_ARRAY *ops[1] = { source };
    CSNARR_RES out = prepare_output(csound, &p->h, NULL, reg, p->handle, ndim, shape, itype, ops, 1, CSN_ALIAS_CUSTOM);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    if (out.aliased) {
        size_t capacity = source->capacity * (size_t) itype;
        res = csn_scratch_reserve(csound, NULL, false, &p->scratch, capacity > 0 ? capacity : 1, sizeof(double));
        if (res != OK) goto done;
    }

    if (!has_axes || (csn_is_k_form(&p->h) && out.aliased)) goto done;

    if (transpose_into(source, p->handle, (double *) p->scratch.scratch, shape, axes) != OK) {
        res = csound->InitError(csound, "[csnarray] Invalid transpose iterator layout");
        goto done;
    }
    if (out.aliased) update_array_data_version(&p->handle->version);

    SET_KDATA_END(p->k_data, shape, ndim, itype);
    PUBLISH_ELEMENTWISE(&p->k_data, source, NULL, p->handle, 0.0, 0.0);
    memcpy(p->k_data.prev_axes, axes, sizeof(axes));

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

/* csntranspose and csntranspose.ax, i-rate only */
int32_t csnarray_transpose(CSOUND *csound, CSN_RESHAPE *p) {
    return transpose_init_helper(csound, p, p->INOCOUNT < 2, false);
}

/* csntranspose.k: no axes, a trigger */
int32_t csnarray_transpose_noaxes_k_init(CSOUND *csound, CSN_RESHAPE *p) {
    return transpose_init_helper(csound, p, true, false);
}

/* csntranspose.ax.k: k-array axes */
int32_t csnarray_transpose_k_init(CSOUND *csound, CSN_RESHAPE *p) {
    return transpose_init_helper(csound, p, false, true);
}

int32_t csnarray_transpose_deinit(CSOUND *csound, CSN_RESHAPE *p) {
    deinit_scratch(csound, &p->scratch);
    return OK;
}

static int32_t transpose_k_helper(CSOUND *csound, CSN_RESHAPE *p, bool is_default) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    int32_t res = OK;
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle }, 2);

    CSN_ARRAY *source = p->source_handle;
    uint32_t ndim = source->ndim;
    ITEM_TYPE itype = source->itype;

    uint32_t axes[CSN_MAX_DIMS];
    uint32_t shape[CSN_MAX_DIMS];
    CSN_TRANSPOSE_AXES code = transpose_axes(is_default, p->new_shape, ndim, axes);
    if (code != CSN_TRANSPOSE_AXES_OK) {
        res = transpose_axes_error(csound, &p->h, code, ndim);
        goto done;
    }

    /* the source's layout matters, so the whole version is compared */
    bool is_same_axes = memcmp(p->k_data.prev_axes, axes, sizeof(axes)) == 0;
    if (is_same_axes && CAN_REUSE_ELEMENTWISE(&p->k_data, source, NULL, p->handle, 0.0, 0.0)) goto done;

    transpose_shape(source, axes, shape);
    const CSN_ARRAY *ops[1] = { source };
    CSNARR_RES out = prepare_output(csound, &p->h, &p->h, reg, p->handle, ndim, shape, itype, ops, 1, CSN_ALIAS_CUSTOM);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    if (out.aliased) {
        res = csn_scratch_reserve(csound, &p->h, source->rt_locked, &p->scratch, source->size * (size_t) itype, sizeof(double));
        if (res != OK) goto done;
    }

    if (transpose_into(source, p->handle, (double *) p->scratch.scratch, shape, axes) != OK) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Invalid transpose iterator layout");
        goto done;
    }
    if (out.aliased) update_array_data_version(&p->handle->version);

    SET_KDATA_END(p->k_data, shape, ndim, itype);
    PUBLISH_ELEMENTWISE(&p->k_data, source, NULL, p->handle, 0.0, 0.0);
    memcpy(p->k_data.prev_axes, axes, sizeof(axes));

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_transpose_noaxes_k(CSOUND *csound, CSN_RESHAPE *p) {
    CHECK_KTRIG(p->trig);
    return transpose_k_helper(csound, p, true);
}

int32_t csnarray_transpose_k(CSOUND *csound, CSN_RESHAPE *p) {
    return transpose_k_helper(csound, p, false);
}

/* csntranspose.in and csntranspose.ax.in, i-rate only */
int32_t csnarray_transpose_in(CSOUND *csound, CSN_RESHAPE_IN *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    double *buffer = NULL;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);

    CSN_ARRAY *arr = p->source_handle;
    uint32_t ndim = arr->ndim;

    uint32_t axes[CSN_MAX_DIMS];
    uint32_t shape[CSN_MAX_DIMS];
    CSN_TRANSPOSE_AXES code = transpose_axes(p->INOCOUNT < 2, p->new_shape, ndim, axes);
    if (code != CSN_TRANSPOSE_AXES_OK) {
        res = transpose_axes_error(csound, NULL, code, ndim);
        goto done;
    }
    transpose_shape(arr, axes, shape);

    size_t count = arr->size * (size_t) arr->itype;
    buffer = csound->Calloc(csound, sizeof(double) * (count > 0 ? count : 1));
    if (buffer == NULL) {
        res = csound->InitError(csound, "[csnarray] Out of memory: allocation of %zu bytes failed", sizeof(double) * count);
        goto done;
    }

    if (transpose_into(arr, arr, buffer, shape, axes) != OK) {
        res = csound->InitError(csound, "[csnarray] Invalid transpose iterator layout");
        goto done;
    }
    update_array_data_version(&arr->version);

done:
    if (pres.locked) csn_unlock(csound, reg);
    if (buffer != NULL) csound->Free(csound, buffer);
    return res;
}

/* Only reserves the working buffer, for the source's whole capacity so a
   marked source finds it big enough at perf time: the axes may be a k-array,
   and the first pass transposes. */
int32_t csnarray_transpose_in_k_init(CSOUND *csound, CSN_RESHAPE_IN *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);

    CSN_ARRAY *arr = p->source_handle;
    csn_mark_krate(arr);

    deinit_scratch(csound, &p->scratch);
    size_t capacity = arr->capacity * (size_t) arr->itype;
    res = csn_scratch_reserve(csound, NULL, false, &p->scratch, capacity > 0 ? capacity : 1, sizeof(double));
    if (res != OK) goto done;

    memset(&p->k_data, 0, sizeof(p->k_data));
    p->k_data.registry = reg;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_transpose_in_k_deinit(CSOUND *csound, CSN_RESHAPE_IN *p) {
    deinit_scratch(csound, &p->scratch);
    return OK;
}

static int32_t transpose_in_k_helper(CSOUND *csound, CSN_RESHAPE_IN *p, bool is_default) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    int32_t res = OK;
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);

    CSN_ARRAY *arr = p->source_handle;
    uint32_t ndim = arr->ndim;

    uint32_t axes[CSN_MAX_DIMS];
    uint32_t shape[CSN_MAX_DIMS];
    CSN_TRANSPOSE_AXES code = transpose_axes(is_default, p->new_shape, ndim, axes);
    if (code != CSN_TRANSPOSE_AXES_OK) {
        res = transpose_axes_error(csound, &p->h, code, ndim);
        goto done;
    }

    /* Not idempotent: transposing twice undoes it, so a pass runs once per
       write by someone else, or when the axes change. */
    bool is_same_axes = memcmp(p->k_data.prev_axes, axes, sizeof(axes)) == 0;
    if (is_same_axes && CAN_REUSE_ELEMENTWISE(&p->k_data, arr, NULL, NULL, 0.0, 0.0)) goto done;

    res = csn_scratch_reserve(csound, &p->h, arr->rt_locked, &p->scratch, arr->size * (size_t) arr->itype, sizeof(double));
    if (res != OK) goto done;

    transpose_shape(arr, axes, shape);
    if (transpose_into(arr, arr, (double *) p->scratch.scratch, shape, axes) != OK) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Invalid transpose iterator layout");
        goto done;
    }
    update_array_data_version(&arr->version);
    PUBLISH_ELEMENTWISE(&p->k_data, arr, NULL, NULL, 0.0, 0.0);
    memcpy(p->k_data.prev_axes, axes, sizeof(axes));

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

/* csntranspose.in.k: no axes, a trigger */
int32_t csnarray_transpose_noaxes_in_k(CSOUND *csound, CSN_RESHAPE_IN *p) {
    CHECK_KTRIG(p->trig);
    return transpose_in_k_helper(csound, p, true);
}

/* csntranspose.ax.in.k: k-array axes */
int32_t csnarray_transpose_in_k(CSOUND *csound, CSN_RESHAPE_IN *p) {
    return transpose_in_k_helper(csound, p, false);
}

/* flip, roll and rollaxis are permutations computed in place, with no working
   buffer: flip swaps mirrored pairs, roll rotates a run by three reversals. An
   output that is not its source is a copy of it first. Arrays are always
   C-contiguous, so strides are plain item counts. */
typedef enum {
    CSN_FLIP = 0,
    CSN_ROLL,
    CSN_ROLLAXIS
} CSN_FLIP_ROLL_MODE;

static void swap_items(double *data, size_t a, size_t b, uint32_t width) {
    for (uint32_t c = 0; c < width; c++) {
        double tmp = data[a * width + c];
        data[a * width + c] = data[b * width + c];
        data[b * width + c] = tmp;
    }
}

/* axis -1 flips every axis */
static void flip_in_place(CSN_ARRAY *arr, int32_t axis) {
    uint32_t width = (uint32_t) arr->itype;
    for (size_t linear = 0; linear < arr->size; linear++) {
        size_t rest = linear;
        size_t mirror = 0;
        for (uint32_t i = 0; i < arr->ndim; i++) {
            size_t coord = rest / arr->strides[i];
            rest %= arr->strides[i];
            if (axis < 0 || (uint32_t) axis == i) coord = arr->shape[i] - 1U - coord;
            mirror += coord * arr->strides[i];
        }
        if (linear < mirror) swap_items(arr->data, linear, mirror, width);
    }
}

static void reverse_run(double *data, size_t base, size_t count, size_t stride, uint32_t width) {
    if (count < 2) return;
    size_t lo = 0;
    size_t hi = count - 1;
    while (lo < hi) {
        swap_items(data, base + lo * stride, base + hi * stride, width);
        lo++;
        hi--;
    }
}

/* item i of the run moves to (i + shift) mod count, as numpy.roll does */
static void rotate_run(double *data, size_t base, size_t count, size_t stride, uint32_t width, int32_t shift) {
    if (count < 2) return;
    int64_t k = (int64_t) shift % (int64_t) count;
    if (k < 0) k += (int64_t) count;
    if (k == 0) return;
    reverse_run(data, base, count, stride, width);
    reverse_run(data, base, (size_t) k, stride, width);
    reverse_run(data, base + (size_t) k * stride, count - (size_t) k, stride, width);
}

static void flip_roll_apply(CSN_ARRAY *arr, CSN_FLIP_ROLL_MODE mode, int32_t axis, int32_t shift) {
    uint32_t width = (uint32_t) arr->itype;
    if (mode == CSN_FLIP) {
        flip_in_place(arr, axis);
    } else if (mode == CSN_ROLL) {
        /* roll without an axis works on the flattened array */
        rotate_run(arr->data, 0, arr->size, 1, width, shift);
    } else {
        size_t count = arr->shape[axis];
        size_t stride = arr->strides[axis];
        if (count < 2) return;
        for (size_t linear = 0; linear < arr->size; linear++) {
            if ((linear / stride) % count != 0) continue;
            rotate_run(arr->data, linear, count, stride, width, shift);
        }
    }
}

/* flip takes an optional axis (all axes when omitted), roll a shift, rollaxis a
   shift and an axis. The flip axis -1 stands for all axes. */
static int32_t flip_roll_params(CSOUND *csound, OPDS *perf_h, const CSN_ARRAY *arr, CSN_FLIP_ROLL_MODE mode, const cs_float *shift_in, const cs_float *axis_in, int32_t *shift, int32_t *axis) {
    uint32_t ndim = arr->ndim;
    *shift = 0;
    *axis = -1;
    if (mode == CSN_FLIP) {
        CSN_AXIS_SPEC spec = csn_normalize_axis(axis_in, ndim, CSN_AXIS_DEFAULT_ALL);
        if (spec.kind == CSN_AXIS_INVALID) {
            double value = axis_in == NULL ? 0.0 : (double) *axis_in;
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: finite integers %d..%u)", value, ndim, -(int32_t) ndim, ndim - 1);
        }
        if (spec.kind == CSN_AXIS_INDEX) *axis = (int32_t) spec.index;
        return OK;
    }

    double shift_value = (double) *shift_in;
    if (!IS_VALID_SHIFT(shift_value)) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Shift %g is invalid; shift must be a finite integer in %d..%d", shift_value, INT32_MIN, INT32_MAX);
    }
    *shift = (int32_t) shift_value;

    if (mode == CSN_ROLLAXIS) {
        double axis_value = (double) *axis_in;
        CSN_AXIS_SPEC spec = csn_normalize_axis_value(axis_value, ndim);
        if (spec.kind != CSN_AXIS_INDEX) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: finite integers %d..%u)", axis_value, ndim, -(int32_t) ndim, ndim - 1);
        }
        *axis = (int32_t) spec.index;
    }
    return OK;
}

/* Serves the i-rate forms and the k forms' init. A k form whose output is its
   own source leaves it to the first pass, which applies it again on every
   pass; computing here as well would apply it twice in the first cycle. */
static int32_t flip_roll_init_helper(CSOUND *csound, CSN_FLIP_ROLL *p, CSN_FLIP_ROLL_MODE mode, const cs_float *shift_in, const cs_float *axis_in) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle }, 2);

    memset(&p->k_data, 0, sizeof(p->k_data));
    p->k_data.registry = reg;

    CSN_ARRAY *source = p->source_handle;
    int32_t shift = 0;
    int32_t axis = -1;
    res = flip_roll_params(csound, NULL, source, mode, shift_in, axis_in, &shift, &axis);
    if (res != OK) goto done;

    const CSN_ARRAY *ops[1] = { source };
    CSNARR_RES out = prepare_output(csound, &p->h, NULL, reg, p->handle, source->ndim, source->shape, source->itype, ops, 1, CSN_ALIAS_CUSTOM);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    if (csn_is_k_form(&p->h) && out.aliased) goto done;

    if (!out.aliased) memcpy(p->handle->data, source->data, sizeof(double) * source->size * source->itype);
    flip_roll_apply(p->handle, mode, axis, shift);
    if (out.aliased && source->size > 0) update_array_data_version(&p->handle->version);

    SET_KDATA_END(p->k_data, source->shape, source->ndim, source->itype);
    PUBLISH_ELEMENTWISE(&p->k_data, source, NULL, p->handle, (double) shift, (double) axis);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

static int32_t flip_roll_k_helper(CSOUND *csound, CSN_FLIP_ROLL *p, CSN_FLIP_ROLL_MODE mode, const cs_float *shift_in, const cs_float *axis_in) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    int32_t res = OK;
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle }, 2);

    CSN_ARRAY *source = p->source_handle;
    int32_t shift = 0;
    int32_t axis = -1;
    res = flip_roll_params(csound, &p->h, source, mode, shift_in, axis_in, &shift, &axis);
    if (res != OK) goto done;

    /* rollaxis follows the source's layout, so the whole version is compared */
    if (CAN_REUSE_ELEMENTWISE(&p->k_data, source, NULL, p->handle, (double) shift, (double) axis)) goto done;

    const CSN_ARRAY *ops[1] = { source };
    CSNARR_RES out = prepare_output(csound, &p->h, &p->h, reg, p->handle, source->ndim, source->shape, source->itype, ops, 1, CSN_ALIAS_CUSTOM);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    if (!out.aliased) memcpy(p->handle->data, source->data, sizeof(double) * source->size * source->itype);
    flip_roll_apply(p->handle, mode, axis, shift);
    if (out.aliased && source->size > 0) update_array_data_version(&p->handle->version);

    SET_KDATA_END(p->k_data, source->shape, source->ndim, source->itype);
    PUBLISH_ELEMENTWISE(&p->k_data, source, NULL, p->handle, (double) shift, (double) axis);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

static int32_t flip_roll_in_helper(CSOUND *csound, CSN_FLIP_ROLL_IN *p, CSN_FLIP_ROLL_MODE mode, const cs_float *shift_in, const cs_float *axis_in) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);

    CSN_ARRAY *arr = p->source_handle;
    int32_t shift = 0;
    int32_t axis = -1;
    res = flip_roll_params(csound, NULL, arr, mode, shift_in, axis_in, &shift, &axis);
    if (res != OK) goto done;

    if (arr->size == 0) goto done;
    flip_roll_apply(arr, mode, axis, shift);
    update_array_data_version(&arr->version);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

/* The in-place k forms read k arguments, so the init only marks the array
   k-rate: the first pass applies. */
static int32_t flip_roll_in_k_init(CSOUND *csound, CSN_FLIP_ROLL_IN *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    stamp_globals(csound, &p->h);
    bool locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);
    csn_mark_krate(p->source_handle);
    if (locked) csn_unlock(csound, reg);

    memset(&p->k_data, 0, sizeof(p->k_data));
    p->k_data.registry = reg;
    return OK;
}

static int32_t flip_roll_in_k_helper(CSOUND *csound, CSN_FLIP_ROLL_IN *p, CSN_FLIP_ROLL_MODE mode, const cs_float *shift_in, const cs_float *axis_in) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    int32_t res = OK;
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);

    CSN_ARRAY *arr = p->source_handle;
    int32_t shift = 0;
    int32_t axis = -1;
    res = flip_roll_params(csound, &p->h, arr, mode, shift_in, axis_in, &shift, &axis);
    if (res != OK) goto done;

    if (arr->size == 0) goto done;

    /* Not idempotent: flipping twice undoes it and a roll shifts again, so a
       pass runs once per write by someone else, or when a parameter changes. */
    if (CAN_REUSE_ELEMENTWISE(&p->k_data, arr, NULL, NULL, (double) shift, (double) axis)) goto done;

    flip_roll_apply(arr, mode, axis, shift);
    update_array_data_version(&arr->version);
    PUBLISH_ELEMENTWISE(&p->k_data, arr, NULL, NULL, (double) shift, (double) axis);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

/* flip: param_a is the axis; flip.k: param_a the trigger, param_b the axis.
   INOCOUNT is the declared arity, so it tells the overloads apart. */
int32_t csnarray_flip(CSOUND *csound, CSN_FLIP_ROLL *p) {
    return flip_roll_init_helper(csound, p, CSN_FLIP, NULL, p->INOCOUNT > 1 ? p->param_a : NULL);
}

int32_t csnarray_flip_k_init(CSOUND *csound, CSN_FLIP_ROLL *p) {
    return flip_roll_init_helper(csound, p, CSN_FLIP, NULL, p->INOCOUNT > 2 ? p->param_b : NULL);
}

int32_t csnarray_flip_k(CSOUND *csound, CSN_FLIP_ROLL *p) {
    CHECK_KTRIG(p->param_a);
    return flip_roll_k_helper(csound, p, CSN_FLIP, NULL, p->INOCOUNT > 2 ? p->param_b : NULL);
}

int32_t csnarray_flip_in(CSOUND *csound, CSN_FLIP_ROLL_IN *p) {
    return flip_roll_in_helper(csound, p, CSN_FLIP, NULL, p->INOCOUNT > 1 ? p->param_a : NULL);
}

int32_t csnarray_flip_in_k_init(CSOUND *csound, CSN_FLIP_ROLL_IN *p) {
    return flip_roll_in_k_init(csound, p);
}

int32_t csnarray_flip_in_k(CSOUND *csound, CSN_FLIP_ROLL_IN *p) {
    CHECK_KTRIG(p->param_a);
    return flip_roll_in_k_helper(csound, p, CSN_FLIP, NULL, p->INOCOUNT > 2 ? p->param_b : NULL);
}

int32_t csnarray_roll(CSOUND *csound, CSN_FLIP_ROLL *p) {
    return flip_roll_init_helper(csound, p, CSN_ROLL, p->param_a, NULL);
}

int32_t csnarray_roll_k(CSOUND *csound, CSN_FLIP_ROLL *p) {
    return flip_roll_k_helper(csound, p, CSN_ROLL, p->param_a, NULL);
}

int32_t csnarray_roll_in(CSOUND *csound, CSN_FLIP_ROLL_IN *p) {
    return flip_roll_in_helper(csound, p, CSN_ROLL, p->param_a, NULL);
}

int32_t csnarray_roll_in_k_init(CSOUND *csound, CSN_FLIP_ROLL_IN *p) {
    return flip_roll_in_k_init(csound, p);
}

int32_t csnarray_roll_in_k(CSOUND *csound, CSN_FLIP_ROLL_IN *p) {
    return flip_roll_in_k_helper(csound, p, CSN_ROLL, p->param_a, NULL);
}

int32_t csnarray_rollaxis(CSOUND *csound, CSN_FLIP_ROLL *p) {
    return flip_roll_init_helper(csound, p, CSN_ROLLAXIS, p->param_a, p->param_b);
}

int32_t csnarray_rollaxis_k(CSOUND *csound, CSN_FLIP_ROLL *p) {
    return flip_roll_k_helper(csound, p, CSN_ROLLAXIS, p->param_a, p->param_b);
}

int32_t csnarray_rollaxis_in(CSOUND *csound, CSN_FLIP_ROLL_IN *p) {
    return flip_roll_in_helper(csound, p, CSN_ROLLAXIS, p->param_a, p->param_b);
}

int32_t csnarray_rollaxis_in_k_init(CSOUND *csound, CSN_FLIP_ROLL_IN *p) {
    return flip_roll_in_k_init(csound, p);
}

int32_t csnarray_rollaxis_in_k(CSOUND *csound, CSN_FLIP_ROLL_IN *p) {
    return flip_roll_in_k_helper(csound, p, CSN_ROLLAXIS, p->param_a, p->param_b);
}
