#include "csnum.h"
#include "csnregistry.h"
#include <float.h>
#include <csdl.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "arrays.h"


static int32_t NEED_TO_UPDATE_SLOT(CSOUND *csound, OPDS *h, CSN_ARRAY **destination, K_DATA *k_data, uint32_t ndim, const uint32_t *shape, size_t logical_size, ITEM_TYPE itype, const char *err) {
    int32_t res = OK;
    size_t requested_size = 0;
    res = get_array_size_from_shape(&requested_size, ndim, shape);
    if (res != OK) {
        return csound->PerfError(csound, h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    if (logical_size > requested_size) {
          return csound->PerfError(csound, h, "[csnarray] Logical size %zu exceeds physical size %zu", logical_size, requested_size);
    }

    bool request_changed = IS_REQUEST_CHANGED(k_data, ndim, itype, shape);
    if (SHOULD_SLOT_BE_UPDATED(request_changed, *destination, itype, requested_size)) {
        int32_t res = update_slot_array_locked(csound, k_data->registry, k_data->owned_handle, ndim, shape, itype, destination, &err);
        if (res != OK) {
            return csound->PerfError(csound, h, "[csnarray] Could not update k-rate output slot: %s", err != NULL ? err : "unknown error");
        }
    }
    /* Capacity follows the physical shape, while size follows the source's
       logical element count (notably zero for csnempty). */
    (*destination)->size = logical_size;
    return OK;
}

static int32_t CHECK_IF_REALLOC_IN(CSOUND *csound, OPDS *h, K_DATA *k_data, CSN_ARRAY *arr, double **scratch, size_t *scratch_capacity, uint32_t ndim, ITEM_TYPE itype, bool is_value_changed) {
    size_t required = arr->size * (size_t) itype;
    bool layout_changed = IS_REQUEST_CHANGED(k_data, ndim, itype, arr->shape) || k_data->prev_size != arr->size;
    bool scratch_too_small = *scratch_capacity < required;
    bool data_changed = false;
    if (!layout_changed && !scratch_too_small && required > 0) {
        data_changed = memcmp(arr->data, *scratch, sizeof(double) * required) != 0;
    }

    if (!layout_changed && !scratch_too_small && !data_changed && !is_value_changed) return NOTOK; // goto done

    if (scratch_too_small) {
        size_t new_capacity = required > 0 ? required * 2 : 1;
        double *data = csound->ReAlloc(csound, *scratch, sizeof(double) * new_capacity);
        if (data == NULL) {
            return csound->PerfError(csound, h, "[csnarray] Memory allocation failed");
        }

        *scratch = data;
        *scratch_capacity = new_capacity;
    }
    return OK;
}

static bool IS_VALID_AXIS(double axis, uint32_t ndim) {
    if (!isfinite(axis) || trunc(axis) != axis || axis < 0.0 || axis >= (double) ndim) {
        return false;
    }
    return true;
}

static bool IS_VALID_SHIFT(double shift) {
    return isfinite(shift) && trunc(shift) == shift && shift >= (double) INT32_MIN && shift <= (double) INT32_MAX;
}

static bool IS_VALID_INDEX(double index) {
    return isfinite(index) && trunc(index) == index && index >= 0.0 && index <= (double) UINT32_MAX;
}

static inline void fill_csnarray(CSN_ARRAY *array, double value) {
    if (array->itype == CSN_COMPLEX) {
        for (size_t i = 0; i < array->size; i++) {
            array->data[i * 2] = value;
            array->data[i * 2 + 1] = 0.0;
        }
        return;
    }

    for (size_t i = 0; i < array->size; i++) array->data[i] = value;
}

static inline void fill_csnarray_complex(CSN_ARRAY *array, double re, double im) {
    for (size_t i = 0; i < array->size; i++) {
        array->data[i * 2] = re;
        array->data[i * 2 + 1] = im;
    }
}

static inline void set_csnarray_layout(CSN_ARRAY *array, uint32_t ndim, const uint32_t *shape, size_t size, ITEM_TYPE itype) {
    array->size = size;
    array->ndim = ndim;
    array->itype = itype;
    memset(array->shape, 0, sizeof(array->shape));
    memset(array->strides, 0, sizeof(array->strides));
    memcpy(array->shape, shape, sizeof(uint32_t) * ndim);
    compute_strides(array->shape, array->strides, ndim);
}

/* csnempty reserves the requested shape but exposes no logical elements yet.
   Shape describes the allocated/indexable layout; size is the number of
   elements currently present. */
static inline void reset_empty_csnarray(CSN_ARRAY *array, uint32_t ndim, const uint32_t *requested_shape, ITEM_TYPE itype) {
    set_csnarray_layout(array, ndim, requested_shape, 0, itype);
}

/* COMPLEXDAT isPolar -> rectangular */
static inline void complexdat_to_rect(const COMPLEXDAT *c, double *re, double *im) {
    if (c->isPolar) {
        *re = (double) c->real * cos((double) c->imag);
        *im = (double) c->real * sin((double) c->imag);
        return;
    }

    *re = (double) c->real;
    *im = (double) c->imag;
}

static inline int32_t handle_out_is_global(const OPDS *h) {
    if (h->optext == NULL || h->optext->t.outArgs == NULL) {
        return 0;
    }
    return h->optext->t.outArgs->type == ARG_GLOBAL;
}

static int32_t csnarray_deinit_by_handle(CSOUND *csound, uint32_t *handle_id, CSN_ARRAY **array, const OPDS *h) {
    if (*handle_id == 0) {
        return OK;
    }

    if (handle_out_is_global(h)) {
        *handle_id = 0;
        *array = NULL;
        return OK;
    }

    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        csound->ErrorMsg(csound, "[csnarray] Internal error: the csnum array registry is not available");
        return NOTOK;
    }

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, *handle_id);
    if (slot != NULL) {
        release_slot(csound, reg, slot);
    }

    csound->UnlockMutex(reg->mutex);

    *handle_id = 0;
    *array = NULL;

    return OK;
}

static int32_t create_csnarray_deinit(CSOUND *csound, CSN_ARR_INIT *p) {
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

static int32_t create_csnarray_full_deinit(CSOUND *csound, CSN_FULL *p) {
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

static int32_t create_csnarray_fullcomp_deinit(CSOUND *csound, CSN_FULLCOMPLEX *p) {
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

static int32_t create_csnarray_like_deinit(CSOUND *csound, CSN_ARR_INIT_LIKE *p) {
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

static int32_t create_csnarray_random_deinit(CSOUND *csound, CSN_ARR_RND_INIT *p) {
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

static int32_t from_array_to_csnarray_deinit(CSOUND *csound, CSN_FROM_ARRAY *p) {
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

static int32_t csnarray_space_spaced_deinit(CSOUND *csound, CSN_SPACED_SPACE *p) {
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

static int32_t csnarray_shape_deinit(CSOUND *csound, CSN_RESHAPE *p) {
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

static int32_t csnarray_identity_deinit(CSOUND *csound, CSN_IDENTITY *p) {
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

static int32_t csnarray_flip_deinit(CSOUND *csound, CSN_FLIP_ROLL *p) {
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

static int32_t csnarray_take_deinit(CSOUND *csound, CSN_TAKE *p) {
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

static int32_t csnarray_slice_deinit(CSOUND *csound, CSN_GET_SLICE *p) {
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

static int32_t csnarray_concat_deinit(CSOUND *csound, CSN_CONCAT *p) {
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

static int32_t csnarray_pad_deinit(CSOUND *csound, CSN_PAD *p) {
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

static int32_t csnarray_padcomp_deinit(CSOUND *csound, CSN_PADCOMPLEX *p) {
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

static int32_t csnarray_clip_deinit(CSOUND *csound, CSN_CLIP *p) {
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

static int32_t csnarray_argwhere_deinit(CSOUND *csound, CSN_ARGWHERE *p) {
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

static int32_t csnarray_compare_deinit(CSOUND *csound, CSN_COMPARE *p) {
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

static int32_t csnarray_reduction_deinit(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

static int32_t csnarray_opbin_deinit(CSOUND *csound, void *p) {
    CSN_BINOP_HH *ptr = (CSN_BINOP_HH *) p;
    return csnarray_deinit_by_handle(csound, &ptr->handle->id, &ptr->array, &ptr->h);
}

static int32_t csnarray_opunary_deinit(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

static int32_t csnarray_opunary_ax_deinit(CSOUND *csound, CSN_UNARYOP_AX *p) {
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

static int32_t csnarray_norm_deinit(CSOUND *csound, CSN_NORM_REDUCTION *p) {
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

static int32_t csnarray_movstats_deinit(CSOUND *csound, CSN_MOVSTATS *p) {
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

static int32_t csnarray_opbincomp_deinit(CSOUND *csound, void *p) {
    CSN_BINOPCOMPLEX_HS *ptr = (CSN_BINOPCOMPLEX_HS *) p;
    return csnarray_deinit_by_handle(csound, &ptr->handle->id, &ptr->array, &ptr->h);
}

static int32_t csnarray_angle_deinit(CSOUND *csound, CSN_ANGLE *p) {
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

static int32_t csnarray_perquant_deinit(CSOUND *csound, CSN_PERCQUANT_AX *p) {
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

static const char *shape_str(char *buf, size_t buf_size, const uint32_t *shape, uint32_t ndim) {
    size_t off = 0;
    int written = snprintf(buf, buf_size, "(");
    if (written > 0) off = (size_t) written;

    for (uint32_t i = 0; i < ndim && off + 1 < buf_size; ++i) {
        written = snprintf(buf + off, buf_size - off, "%s%u", i > 0 ? ", " : "", shape[i]);
        if (written < 0) break;
        off += (size_t) written;
    }

    if (off + 1 < buf_size) snprintf(buf + off, buf_size - off, ")");
    return buf;
}

static void from_linear_to_coords(uint32_t *coords, const uint32_t *shape, size_t linear, uint32_t ndim) {
    for (uint32_t i = ndim; i-- > 0;) {
        coords[i] = (uint32_t) (linear % shape[i]);
        linear /= shape[i];
    }
}

static uint32_t from_coords_to_offset(uint32_t *coords, const size_t *strides, uint32_t ndim) {
    size_t src_offset = 0;
    for (uint32_t i = 0; i < ndim; ++i) {
        src_offset += (size_t) coords[i] * strides[i];
    }
    return src_offset;
}

static int32_t parse_shape_array(CSOUND *csound, const ARRAYDAT *p_shape, uint32_t *out_ndim, uint32_t *out_shape) {
    if (p_shape == NULL
        || p_shape->dimensions != 1
        || p_shape->data == NULL
        || p_shape->sizes == NULL
        || p_shape->sizes[0] <= 0) {
        return csound->InitError(csound, "[csnarray] Shape argument must be a non-empty 1-D i-array");
    }

    if (p_shape->sizes[0] > CSN_MAX_DIMS) {
        return csound->InitError(csound, "[csnarray] Shape argument declares %d dimensions, the maximum is %d", (int32_t) p_shape->sizes[0], CSN_MAX_DIMS);
    }

    uint32_t ndim = (uint32_t) p_shape->sizes[0];
    for (uint32_t i = 0; i < ndim; i++) {
        MYFLT extent = p_shape->data[i];
        /* 0 is allowed: it produces a zero-length array, the empty stack. */
        double value = (double) extent;
        if (!isfinite(value) || value < 0.0 || value > (double) UINT32_MAX || trunc(value) != value) {
            return csound->InitError(csound, "[csnarray] Shape extent %g at position %u must be a finite integer in 0..%u", value, i, UINT32_MAX);
        }
        out_shape[i] = (uint32_t) extent;
    }

    *out_ndim = ndim;
    return OK;
}

static int32_t parse_shape_array_k(CSOUND *csound, OPDS *h, const ARRAYDAT *p_shape, uint32_t *out_ndim, uint32_t *out_shape) {
    if (p_shape == NULL
        || p_shape->dimensions != 1
        || p_shape->data == NULL
        || p_shape->sizes == NULL
        || p_shape->sizes[0] <= 0) {
        return csound->PerfError(csound, h, "[csnarray] Shape argument must be a non-empty 1-D k-array");
    }

    if (p_shape->sizes[0] > CSN_MAX_DIMS) {
        return csound->PerfError(csound, h, "[csnarray] Shape argument declares %d dimensions, the maximum is %d", (int32_t) p_shape->sizes[0], CSN_MAX_DIMS);
    }

    uint32_t ndim = (uint32_t) p_shape->sizes[0];
    for (uint32_t i = 0; i < ndim; i++) {
        MYFLT extent = p_shape->data[i];
        /* 0 is allowed: it produces a zero-length array, the empty stack. */
        double value = (double) extent;
        if (!isfinite(value) || value < 0.0 || value > (double) UINT32_MAX || trunc(value) != value) {
            return csound->PerfError(csound, h, "[csnarray] Shape extent %g at position %u must be a finite integer in 0..%u", value, i, UINT32_MAX);
        }
        out_shape[i] = (uint32_t) extent;
    }

    *out_ndim = ndim;
    return OK;
}

/* Reserves a slot and allocates its array. The caller must already hold
   reg->mutex: the registry mutex is not recursive, so an op that took the lock
   to inspect its source has to reach the allocator through this entry point
   rather than through create_csnarray_init.

   protect lists every handle the caller is still reading from (NULL/0 for the
   creation opcodes, which read none). A global output has its previous array
   released here so a re-triggered creator does not strand it, but in
   `gih csntranspose gih` — or `gih csnconcat ia, gih` — that previous value is
   an operand: releasing it would free an array the caller is about to read.
   Every operand must be listed, so binary and higher-arity ops are covered.

   On failure *err is set to a message the caller reports after unlocking. */
static int32_t create_csnarray_locked(
    CSOUND *csound,
    CSN_REGISTRY *reg,
    const OPDS *h,
    uint32_t ndim,
    const uint32_t *shape,
    CSN_ARRAY **p_array,
    CSNREF *p_handle,
    const uint32_t *protect,
    uint32_t protect_count,
    const char **err,
    ITEM_TYPE itype
) {
    if (handle_out_is_global(h)) {
        uint32_t previous_handle = p_handle->id;
        bool is_operand = false;
        for (uint32_t i = 0; i < protect_count; i++) {
            if (protect[i] == previous_handle) {
                is_operand = true;
                break;
            }
        }

        if (!is_operand) {
            CSN_SLOT *previous = get_slot(reg, previous_handle);
            if (previous != NULL) {
                release_slot(csound, reg, previous);
            }
        }
    }

    uint32_t handle = find_free_slot(reg);
    if (handle == (uint32_t) INVALID_HANDLE) {
        *err = "Invalid handle, registry full";
        return NOTOK;
    }

    uint32_t index = SLT_FROM_HANDLE(handle);
    CSN_SLOT *slot = &reg->slots[index];

    if (activate_slot(csound, reg, slot, ndim, shape, handle, itype) != OK) {
        *err = "Slot activation failed";
        return NOTOK;
    }

    *p_array = slot->array;
    p_handle->id = handle;
    return OK;
}

/* Takes the lock itself; for opcodes that hold nothing on entry. */
static int32_t create_csnarray_init(
    CSOUND *csound,
    const OPDS *h,
    uint32_t ndim,
    const uint32_t *shape,
    CSN_ARRAY **p_array,
    CSNREF *p_handle,
    ITEM_TYPE itype
) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

    const char *err = NULL;

    csound->LockMutex(reg->mutex);
    /* Creation opcodes read no source array, so nothing needs protecting. */
    int32_t res = create_csnarray_locked(csound, reg, h, ndim, shape, p_array, p_handle, NULL, 0, &err, itype);
    csound->UnlockMutex(reg->mutex);

    if (res != OK) {
        return csound->InitError(csound, "[csnarray] %s", err);
    }

    return OK;
}

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
    if (res != OK) {
        return res;
    }

    res = create_csnarray_init(csound, &p->h, ndim, shape, &p->array, p->handle, itype);
    if (res != OK) {
        return res;
    }

    reset_empty_csnarray(p->array, ndim, shape, itype);
    return OK;
}

static int32_t create_shape_csnarray_k_init(CSOUND *csound, CSN_ARR_INIT *p, CSN_K_SHAPE_INIT_MODE mode) {
    CHECK_ITYPE(csound, *p->itype);
    ITEM_TYPE itype = CSN_ITYPE_FROM_ARG(*p->itype);
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

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
    if (mode == CSN_K_EMPTY) {
        reset_empty_csnarray(p->array, ndim, shape, itype);
    } else {
        fill_csnarray(p->array, mode == CSN_K_ONES ? 1.0 : 0.0);
    }
    csound->UnlockMutex(reg->mutex);

    SET_KDATA_WITH_ID_BEGIN(p, reg, shape, ndim, itype, p->handle->id);
    return OK;
}

static int32_t create_empty_csnarray_k_init(CSOUND *csound, CSN_ARR_INIT *p) {
    return create_shape_csnarray_k_init(csound, p, CSN_K_EMPTY);
}

static int32_t create_zeros_csnarray_k_init(CSOUND *csound, CSN_ARR_INIT *p) {
    return create_shape_csnarray_k_init(csound, p, CSN_K_ZEROS);
}

static int32_t create_ones_csnarray_k_init(CSOUND *csound, CSN_ARR_INIT *p) {
    return create_shape_csnarray_k_init(csound, p, CSN_K_ONES);
}

static int32_t create_shape_csnarray_k(CSOUND *csound, CSN_ARR_INIT *p, CSN_K_SHAPE_INIT_MODE mode) {
    CHECK_REG_HANDLE(csound, h, p->k_data.registry, p->k_data.owned_handle);

    double itype_arg = (double) *p->itype;
    if (itype_arg != 0.0 && itype_arg != 1.0) {
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid array type %g: itype must be 0 (real) or 1 (complex)", itype_arg);
    }
    ITEM_TYPE itype = CSN_ITYPE_FROM_ARG(itype_arg);

    uint32_t ndim = 0;
    uint32_t shape[CSN_MAX_DIMS] = {0};
    int32_t res = parse_shape_array_k(csound, &p->h, p->shape, &ndim, shape);
    if (res != OK) {
        return res;
    }


    const char *err = NULL;
    bool request_changed = IS_REQUEST_CHANGED(&p->k_data, ndim, itype, shape);

    csound->LockMutex(p->k_data.registry->mutex);

    CSN_SLOT *slot = get_slot(p->k_data.registry, p->k_data.owned_handle);
    if (slot == NULL) {
        csound->UnlockMutex(p->k_data.registry->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] k-rate output slot is no longer active");
    }

    size_t requested_size = 0;
    if (get_array_size_from_shape(&requested_size, ndim, shape) != OK) {
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSN_ARRAY *array = slot->array;
    if (SHOULD_SLOT_BE_UPDATED(request_changed, array, itype, requested_size)) {
        res = update_slot_array_locked(csound, p->k_data.registry, p->k_data.owned_handle, ndim, shape, itype, &p->array, &err);
        if (res != OK) {
            csound->UnlockMutex(p->k_data.registry->mutex);
            return csound->PerfError(csound, &p->h, "[csnarray] Could not update k-rate output slot: %s", err != NULL ? err : "unknown error");
        }
        array = p->array;
    }

    if (mode == CSN_K_EMPTY) {
        reset_empty_csnarray(array, ndim, shape, itype);
    } else {
        set_csnarray_layout(array, ndim, shape, requested_size, itype);
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

static int32_t create_like_csnarray_k_init(CSOUND *csound, CSN_ARR_INIT_LIKE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

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
    CHECK_REG_HANDLE(csound, h, p->k_data.registry, p->k_data.owned_handle);

    uint32_t handle_from = p->handle_from->id;
    int32_t res = OK;

    csound->LockMutex(p->k_data.registry->mutex);

    CSN_SLOT *slot = get_slot(p->k_data.registry, handle_from);
    if (slot == NULL) {
        res = csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) handle_from);
        goto done;
    }

    CSN_ARRAY *source_arr = slot->array;
    uint32_t ndim = source_arr->ndim;
    ITEM_TYPE itype = source_arr->itype;
    if (itype != CSN_REAL && itype != CSN_COMPLEX) {
        res = csound->PerfError(csound, &p->h, "[csnarray] Source array has invalid internal type %u", itype);
        goto done;
    }

    uint32_t shape[CSN_MAX_DIMS] = {0};
    memcpy(shape, source_arr->shape, sizeof(uint32_t) * ndim);

    size_t requested_size = 0;
    if (get_array_size_from_shape(&requested_size, ndim, shape) != OK) {
        res = csound->PerfError(csound, &p->h, "[csnarray] Source array has an invalid shape");
        goto done;
    }

    CSN_SLOT *output_slot = get_slot(p->k_data.registry, p->k_data.owned_handle);
    if (output_slot == NULL) {
        res = csound->PerfError(csound, &p->h, "[csnarray] k-rate output slot is no longer active");
        goto done;
    }

    bool request_changed = IS_REQUEST_CHANGED(&p->k_data, ndim, itype, shape);

    CSN_ARRAY *output_arr = output_slot->array;
    if (SHOULD_SLOT_BE_UPDATED(request_changed, output_arr, itype, requested_size)) {
        const char *err = NULL;
        res = update_slot_array_locked(csound, p->k_data.registry, p->k_data.owned_handle, ndim, shape, itype, &p->array, &err);
        if (res != OK) {
            res = csound->PerfError(csound, &p->h, "[csnarray] Could not update k-rate output slot: %s", err != NULL ? err : "unknown error");
            goto done;
        }
        output_arr = p->array;
    }

    set_csnarray_layout(output_arr, ndim, shape, requested_size, itype);
    fill_csnarray(output_arr, (double) *p->value);

    SET_KDATA_END(p, shape, ndim, itype);

done:
    csound->UnlockMutex(p->k_data.registry->mutex);
    return res;
}

int32_t create_like_csnarray(CSOUND *csound, CSN_ARR_INIT_LIKE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

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

int32_t create_random_csnarray(CSOUND *csound, CSN_ARR_RND_INIT *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

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

    /* reg->rng is registry-wide state that every draw mutates, so the fill has
       to run under the same lock as everything else: two threads drawing at
       once would otherwise interleave into the generator and could hand out
       the same stream twice. */
    csound->LockMutex(reg->mutex);

    CSN_ARRAY *arr = p->array;
    for (size_t i = 0; i < arr->size; i++) {
        arr->data[i] = min + pcg32_random(&reg->rng) * (max - min);
    }

    csound->UnlockMutex(reg->mutex);

    return OK;
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

static int32_t create_full_csnarray_k_init(CSOUND *csound, CSN_FULL *p) {
    int32_t res = create_full_csnarray(csound, p);
    if (res != OK) {
        return res;
    };

    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] NULL registry");
    }

    SET_KDATA_BEGIN(p, reg);
    return OK;
}

int32_t create_full_csnarray_k(CSOUND *csound, CSN_FULL *p) {
    CHECK_REG_HANDLE(csound, h, p->k_data.registry, p->k_data.owned_handle);

    double itype_arg = (double) *p->itype;
    if (itype_arg != 0.0 && itype_arg != 1.0) {
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid array type %g: itype must be 0 (real) or 1 (complex)", itype_arg);
    }
    ITEM_TYPE itype = CSN_ITYPE_FROM_ARG(itype_arg);

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

    bool request_changed = IS_REQUEST_CHANGED(&p->k_data, ndim, itype, shape);

    csound->LockMutex(p->k_data.registry->mutex);

    CSN_SLOT *slot = get_slot(p->k_data.registry, p->k_data.owned_handle);
    if (slot == NULL) {
        csound->UnlockMutex(p->k_data.registry->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] k-rate output slot is no longer active");
    }

    CSN_ARRAY *array = slot->array;
    if (SHOULD_SLOT_BE_UPDATED(request_changed, array, itype, requested_size)) {
        const char *err = NULL;
        res = update_slot_array_locked(csound, p->k_data.registry, p->k_data.owned_handle, ndim, shape, itype, &p->array, &err);
        if (res != OK) {
            csound->UnlockMutex(p->k_data.registry->mutex);
            return csound->PerfError(csound, &p->h, "[csnarray] Could not update k-rate output slot: %s", err != NULL ? err : "unknown error");
        }
        array = p->array;
    }

    set_csnarray_layout(array, ndim, shape, requested_size, itype);
    fill_csnarray(array, (double) *p->value);

    SET_KDATA_END(p, shape, ndim, itype);

    csound->UnlockMutex(p->k_data.registry->mutex);
    return OK;
}

static int32_t create_fullcomp_csnarray_k_init(CSOUND *csound, CSN_FULLCOMPLEX *p) {
    int32_t res = create_fullcomp_csnarray(csound, p);
    if (res != OK) {
        return res;
    };

    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] NULL registry");
    }

    SET_KDATA_BEGIN(p, reg);
    return OK;
}

int32_t create_fullcomp_csnarray_k(CSOUND *csound, CSN_FULLCOMPLEX *p) {
    CHECK_REG_HANDLE(csound, h, p->k_data.registry, p->k_data.owned_handle);

    double itype_arg = (double) *p->itype;
    if (itype_arg != 1.0) {
        return csound->PerfError(csound, &p->h, "[csnarray] csnfull.c fills with a complex value, so itype must be 1 (complex), got %g", itype_arg);
    }

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

    bool request_changed = IS_REQUEST_CHANGED(&p->k_data, ndim, CSN_COMPLEX, shape);

    csound->LockMutex(p->k_data.registry->mutex);

    CSN_SLOT *slot = get_slot(p->k_data.registry, p->k_data.owned_handle);
    if (slot == NULL) {
        csound->UnlockMutex(p->k_data.registry->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] k-rate output slot is no longer active");
    }

    CSN_ARRAY *array = slot->array;
    if (SHOULD_SLOT_BE_UPDATED(request_changed, array, CSN_COMPLEX, requested_size)) {
        const char *err = NULL;
        res = update_slot_array_locked(csound, p->k_data.registry, p->k_data.owned_handle, ndim, shape, CSN_COMPLEX, &p->array, &err);
        if (res != OK) {
            csound->UnlockMutex(p->k_data.registry->mutex);
            return csound->PerfError(csound, &p->h, "[csnarray] Could not update k-rate output slot: %s", err != NULL ? err : "unknown error");
        }
        array = p->array;
    }

    set_csnarray_layout(array, ndim, shape, requested_size, CSN_COMPLEX);
    double re, im;
    complexdat_to_rect(p->value, &re, &im);
    fill_csnarray_complex(array, re, im);

    SET_KDATA_END(p, shape, ndim, CSN_COMPLEX);

    csound->UnlockMutex(p->k_data.registry->mutex);
    return OK;
}

int32_t create_fullcomp_csnarray(CSOUND *csound, CSN_FULLCOMPLEX *p) {
    CHECK_ITYPE(csound, *p->itype);
    if (CSN_ITYPE_FROM_ARG(*p->itype) != CSN_COMPLEX) {
        return csound->InitError(csound, "[csnarray] csnfull.c fills with a complex value, so itype must be 1 (complex), got %g", (double) *p->itype);
    }

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

static int32_t from_array_to_csnarray_k_init(CSOUND *csound, CSN_FROM_ARRAY *p) {
    int32_t res = from_array_to_csnarray(csound, p);
    if (res != OK) {
        return res;
    };

    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] NULL registry");
    }

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

    CHECK_REG_HANDLE(csound, h, p->k_data.registry, p->k_data.owned_handle);

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

    bool request_changed = IS_REQUEST_CHANGED(&p->k_data, ndim, CSN_REAL, shape);

    int32_t res = OK;
    csound->LockMutex(p->k_data.registry->mutex);

    CSN_SLOT *slot = get_slot(p->k_data.registry, p->k_data.owned_handle);
    if (slot == NULL) {
        csound->UnlockMutex(p->k_data.registry->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] k-rate output slot is no longer active");
    }

    CSN_ARRAY *array = slot->array;
    if (SHOULD_SLOT_BE_UPDATED(request_changed, array, CSN_REAL, requested_size)) {
        const char *err = NULL;
        res = update_slot_array_locked(csound, p->k_data.registry, p->k_data.owned_handle, ndim, shape, CSN_REAL, &p->array, &err);
        if (res != OK) {
            csound->UnlockMutex(p->k_data.registry->mutex);
            return csound->PerfError(csound, &p->h, "[csnarray] Could not update k-rate output slot: %s", err != NULL ? err : "unknown error");
        }
        array = p->array;
    }

    set_csnarray_layout(array, ndim, shape, requested_size, CSN_REAL);
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
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] NULL registry");
    }

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

    CHECK_REG_HANDLE(csound, h, p->k_data.registry, p->k_data.owned_handle);

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

    bool request_changed = IS_REQUEST_CHANGED(&p->k_data, ndim, CSN_COMPLEX, shape);

    int32_t res = OK;
    csound->LockMutex(p->k_data.registry->mutex);

    CSN_SLOT *slot = get_slot(p->k_data.registry, p->k_data.owned_handle);
    if (slot == NULL) {
        csound->UnlockMutex(p->k_data.registry->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] k-rate output slot is no longer active");
    }

    CSN_ARRAY *array = slot->array;
    if (SHOULD_SLOT_BE_UPDATED(request_changed, array, CSN_COMPLEX, requested_size)) {
        const char *err = NULL;
        res = update_slot_array_locked(csound, p->k_data.registry, p->k_data.owned_handle, ndim, shape, CSN_COMPLEX, &p->array, &err);
        if (res != OK) {
            csound->UnlockMutex(p->k_data.registry->mutex);
            return csound->PerfError(csound, &p->h, "[csnarray] Could not update k-rate output slot: %s", err != NULL ? err : "unknown error");
        }
        array = p->array;
    }

    set_csnarray_layout(array, ndim, shape, requested_size, CSN_COMPLEX);
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

int32_t from_csnarray_to_array(CSOUND *csound, CSN_TO_ARRAY *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

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
    if (p->array->data == NULL || p->array->sizes == NULL) {
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
    if (reg == NULL) {
        return csound->PerfError(csound, &p->h, "[csnarray] Internal error: the csnum array registry is not available");
    }

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
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

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
    if (p->array->data == NULL || p->array->sizes == NULL) {
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
    if (reg == NULL) {
        return csound->PerfError(csound, &p->h, "[csnarray] Internal error: the csnum array registry is not available");
    }

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
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

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
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

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
    if (reg == NULL) {
        return csound->PerfError(csound, &p->h, "[csnarray] Internal error: the csnum array registry is not available");
    }

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
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

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
    if (reg == NULL) {
        return csound->PerfError(csound, &p->h, "[csnarray] Internal error: the csnum array registry is not available");
    }

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
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

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
    if (reg == NULL) {
        return csound->PerfError(csound, &p->h, "[csnarray] Internal error: the csnum array registry is not available");
    }

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
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, p->handle->id);
    if (slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) p->handle->id);
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

int32_t csnarray_shape_k(CSOUND *csound, CSN_SHAPE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->PerfError(csound, &p->h, "[csnarray] Internal error: the csnum array registry is not available");
    }

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

int32_t csnarray_arange(CSOUND *csound, CSN_SPACED_SPACE *p) {
    double step = (double) *p->step_num;
    if (step == 0.0) {
        return csound->InitError(csound, "[csnarray] The step argument must not be zero");
    }

    double start = (double) *p->start;
    double stop = (double) *p->stop;
    if ((stop > start && step < 0) || (stop < start && step > 0)) {
        return csound->InitError(csound, "[csnarray] Step %g has the wrong sign to go from start %g to stop %g", step, start, stop);
    }

    int32_t size = (int32_t) ceil((stop - start) / step);
    if (size == 0) {
        return csound->InitError(csound, "[csnarray] Start %g, stop %g and step %g produce an empty array", start, stop, step);
    }

    uint32_t usize = (uint32_t) size;

    int32_t res_init = create_csnarray_init(csound, &p->h, 1U, &usize, &p->array, p->handle, CSN_REAL);
    if (res_init != OK) {
        return res_init;
    }

    for (int32_t i = 0; i < size; i++) {
        p->array->data[i] =  start + (i * step);
    }


    return OK;
}

int32_t csnarray_linspace(CSOUND *csound, CSN_SPACED_SPACE *p) {
    int32_t num = (int32_t) *p->step_num;
    if (num <= 0) {
        return csound->InitError(csound, "[csnarray] The num argument must be > 0, got %d", num);
    }

    double start = (double) *p->start;
    double stop = (double) *p->stop;

    uint32_t usize = (uint32_t) num;

    int32_t res_init = create_csnarray_init(csound, &p->h, 1U, &usize, &p->array, p->handle, CSN_REAL);
    if (res_init != OK) {
        return res_init;
    }

    if (num == 1) {
        p->array->data[0] = start;
    } else {
        double step = (stop - start) / (double) (num - 1);
        for (int32_t i = 0; i < num; i++) {
            p->array->data[i] =  start + (double) i * step;
        }
        p->array->data[num - 1] = stop;
    }


    return OK;
}

int32_t csnarray_logspace(CSOUND *csound, CSN_SPACED_SPACE *p) {
    int32_t num = (int32_t) *p->step_num;
    if (num <= 0) {
        return csound->InitError(csound, "[csnarray] The num argument must be > 0, got %d", num);
    }

    if (p->base == NULL) {
        return csound->InitError(csound, "[csnarray] The base argument is missing");
    }

    double base = (double) *p->base;
    if (base <= 0) {
        return csound->InitError(csound, "[csnarray] The base argument must be > 0, got %g", base);
    }

    double start = (double) *p->start;
    double stop = (double) *p->stop;

    uint32_t usize = (uint32_t) num;

    int32_t res_init = create_csnarray_init(csound, &p->h, 1U, &usize, &p->array, p->handle, CSN_REAL);
    if (res_init != OK) {
        return res_init;
    }

    if (num == 1) {
        p->array->data[0] = pow(base, start);
    } else {
        double step = (stop - start) / (double) (num - 1);
        for (int32_t i = 0; i < num; i++) {
            double exponent = start + (double) i * step;
            p->array->data[i] =  pow(base, exponent);
        }
        p->array->data[num - 1] = pow(base, stop);
    }


    return OK;
}

int32_t csnarray_geomspace(CSOUND *csound, CSN_SPACED_SPACE *p) {
    int32_t num = (int32_t) *p->step_num;
    if (num <= 0) {
        return csound->InitError(csound, "[csnarray] The num argument must be > 0, got %d", num);
    }

    double start = (double) *p->start;
    double stop = (double) *p->stop;

    uint32_t usize = (uint32_t) num;

    int32_t res_init = create_csnarray_init(csound, &p->h, 1U, &usize, &p->array, p->handle, CSN_REAL);
    if (res_init != OK) {
        return res_init;
    }

    double ratio = pow(stop / start, 1.0 / (double) (num - 1));

    if (num == 1) {
        p->array->data[0] = start;
    } else {
        for (int32_t i = 0; i < num; i++) {
            p->array->data[i] =  start * pow(ratio, i);
        }
        p->array->data[num - 1] = stop;
    }


    return OK;
}

// return matrix n x n
static int32_t csnarray_identity_helper(CSOUND *csound, CSN_IDENTITY *p, bool is_ktime) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] NULL registry");
    }

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

static int32_t csnarray_identity_k_init(CSOUND *csound, CSN_IDENTITY *p) {
    return csnarray_identity_helper(csound, p, true);
}

int32_t csnarray_identity_k(CSOUND *csound, CSN_IDENTITY *p) {
    CHECK_KTYPE(csound, &p->h, *p->itype);
    ITEM_TYPE itype = CSN_ITYPE_FROM_ARG(*p->itype);
    int32_t num = (int32_t) *p->num;
    if (num <= 0) {
        return csound->PerfError(csound, &p->h, "[csnarray] The num argument must be > 0, got %d", num);
    }

    CHECK_REG_HANDLE(csound, h, p->k_data.registry, p->k_data.owned_handle);

    uint32_t ndim = 2U;
    uint32_t shape[CSN_MAX_DIMS] = {0};
    shape[0] = num;
    shape[1] = num;

    size_t requested_size = 0;
    if (get_array_size_from_shape(&requested_size, ndim, shape) != OK) {
        return csound->PerfError(csound, &p->h, "[csnarray] Shape is invalid or its element count exceeds the configured limit");
    }

    bool request_changed = IS_REQUEST_CHANGED(&p->k_data, ndim, itype, shape);

    int32_t res = OK;
    csound->LockMutex(p->k_data.registry->mutex);

    CSN_SLOT *slot = get_slot(p->k_data.registry, p->k_data.owned_handle);
    if (slot == NULL) {
        res = csound->PerfError(csound, &p->h, "[csnarray] k-rate output slot is no longer active");
        goto done;
    }

    CSN_ARRAY *array = slot->array;
    if (SHOULD_SLOT_BE_UPDATED(request_changed, array, itype, requested_size)) {
        const char *err = NULL;
        res = update_slot_array_locked(csound, p->k_data.registry, p->k_data.owned_handle, ndim, shape, itype, &p->array, &err);
        if (res != OK) {
            res = csound->PerfError(csound, &p->h, "[csnarray] Could not update k-rate output slot: %s", err != NULL ? err : "unknown error");
            goto done;
        }
        array = p->array;
    }

    set_csnarray_layout(array, ndim, shape, requested_size, itype);
    fill_csnarray(array, 0.0);
    for (int32_t i = 0; i < num; i++) {
        size_t item = (size_t) i * (size_t) num + (size_t) i;
        p->array->data[item * itype] = 1.0;
    }

    SET_KDATA_END(p, shape, ndim, itype);

done:
    csound->UnlockMutex(p->k_data.registry->mutex);
    return res;
}


int32_t csnarray_reshape(CSOUND *csound, CSN_RESHAPE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

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
static int32_t csnarray_reshape_k_init(CSOUND *csound, CSN_RESHAPE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

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
    CHECK_REG_HANDLE(csound, h, p->k_data.registry, p->k_data.owned_handle);

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
        res = csound->PerfError(csound, &p->h, "[csnarray] Unknown source array handle %u: no array with this id is registered", source_handle);
        goto done;
    }

    CSN_SLOT *output_slot = get_slot(p->k_data.registry, p->k_data.owned_handle);
    if (output_slot == NULL) {
        res = csound->PerfError(csound, &p->h, "[csnarray] k-rate output slot is no longer active");
        goto done;
    }

    CSN_ARRAY *source = source_slot->array;
    CSN_ARRAY *output = output_slot->array;
    ITEM_TYPE itype = source->itype;

    if (requested_size != source->size) {
        res = csound->PerfError(csound, &p->h, "[csnarray] Reshape size mismatch: source has %zu elements, new shape requires %zu", source->size, requested_size);
        goto done;
    }

    /* A shape-only change never needs new storage: reshape preserves the
       element count. Allocate only if the source type changed or the output
       buffer itself is unusable. This also preserves data when source and
       output are the same slot. */
    if (output->data == NULL || output->itype != itype || output->capacity < requested_size) {
        const char *err = NULL;
        res = update_slot_array_locked(csound, p->k_data.registry, p->k_data.owned_handle, ndim, shape, itype, &p->array, &err);
        if (res != OK) {
            res = csound->PerfError(csound, &p->h, "[csnarray] Could not update k-rate output slot: %s", err != NULL ? err : "unknown error");
            goto done;
        }
        output = p->array;
    }

    set_csnarray_layout(output, ndim, shape, requested_size, itype);
    p->array = output;
    if (output != source) {
        memcpy(output->data, source->data, sizeof(double) * requested_size * itype);
    }

    SET_KDATA_END(p, shape, ndim, itype);

done:
    csound->UnlockMutex(p->k_data.registry->mutex);
    return res;
}

int32_t csnarray_reshape_in(CSOUND *csound, CSN_RESHAPE_IN *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

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

    memset(arr->shape, 0, sizeof(arr->shape));
    memset(arr->strides, 0, sizeof(arr->strides));

    arr->ndim = ndim;
    memcpy(arr->shape, shape, sizeof(uint32_t) * ndim);
    compute_strides(arr->shape, arr->strides, ndim);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

/* In-place k-rate reshape owns no output buffer. At init time only remember
   the current slot; a computed k-shape is applied during performance. */
static int32_t csnarray_reshape_in_k_init(CSOUND *csound, CSN_RESHAPE_IN *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

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
        res = csound->PerfError(csound, &p->h, "[csnarray] k-rate output slot is no longer active");
        goto done;
    }

    CSN_ARRAY *arr = slot->array;

    if (requested_size != arr->size) {
        res = csound->PerfError(csound, &p->h, "[csnarray] Reshape size mismatch: source has %zu elements, new shape requires %zu", arr->size, requested_size);
        goto done;
    }

    ITEM_TYPE itype = arr->itype;
    set_csnarray_layout(arr, ndim, shape, requested_size, itype);

    SET_KDATA_NO_ID_END(p, shape, ndim, itype);

done:
    csound->UnlockMutex(p->k_data.registry->mutex);
    return res;
}

int32_t csnarray_flatten(CSOUND *csound, CSN_RESHAPE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

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
    CHECK_REG_HANDLE(csound, h, p->k_data.registry, p->k_data.owned_handle);

    int32_t res = OK;
    csound->LockMutex(p->k_data.registry->mutex);

    uint32_t source_handle = p->source_handle->id;
    CSN_SLOT *source_slot = get_slot(p->k_data.registry, source_handle);
    if (source_slot == NULL) {
        res = csound->PerfError(csound, &p->h, "[csnarray] Unknown source array handle %u: no array with this id is registered", source_handle);
        goto done;
    }

    CSN_SLOT *output_slot = get_slot(p->k_data.registry, p->k_data.owned_handle);
    if (output_slot == NULL) {
        res = csound->PerfError(csound, &p->h, "[csnarray] Unknown output array handle %u: no array with this id is registered", p->k_data.owned_handle);
        goto done;
    }

    CSN_ARRAY *source = source_slot->array;
    CSN_ARRAY *output = output_slot->array;
    ITEM_TYPE itype = source->itype;

    uint32_t ndim = 1U;
    uint32_t shape[CSN_MAX_DIMS] = {0};
    shape[0] = (uint32_t) source->size;

    size_t requested_size = 0;
    if (get_array_size_from_shape(&requested_size, ndim, shape) != OK) {
        res = csound->PerfError(csound, &p->h, "[csnarray] Shape is invalid or its element count exceeds the configured limit");
        goto done;
    }

    bool request_changed = IS_REQUEST_CHANGED(&p->k_data, ndim, itype, shape);
    if (SHOULD_SLOT_BE_UPDATED(request_changed, output, itype, requested_size)) {
        const char *err = NULL;
        res = update_slot_array_locked(csound, p->k_data.registry, p->k_data.owned_handle, ndim, shape, itype, &p->array, &err);
        if (res != OK) {
            res = csound->PerfError(csound, &p->h, "[csnarray] Could not update k-rate output slot: %s", err != NULL ? err : "unknown error");
            goto done;
        }
        output = p->array;
    }

    set_csnarray_layout(output, ndim, shape, requested_size, itype);
    memcpy(output->data, source->data, sizeof(double) * requested_size * itype);

    SET_KDATA_END(p, shape, ndim, itype);

done:
    csound->UnlockMutex(p->k_data.registry->mutex);
    return res;
}

int32_t csnarray_flatten_in(CSOUND *csound, CSN_RESHAPE_IN *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
        goto done;
    }

    CSN_ARRAY *arr = slot->array;

    memset(arr->shape, 0, sizeof(arr->shape));
    memset(arr->strides, 0, sizeof(arr->strides));

    arr->ndim = 1U;
    arr->shape[0] = (uint32_t) arr->size;
    compute_strides(arr->shape, arr->strides, arr->ndim);

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
        res = csound->PerfError(csound, &p->h, "[csnarray] k-rate output slot is no longer active");
        goto done;
    }

    CSN_ARRAY *arr = slot->array;
    uint32_t ndim = 1U;
    uint32_t shape[CSN_MAX_DIMS] = {0};
    shape[0] = (uint32_t) arr->size;

    ITEM_TYPE itype = arr->itype;
    set_csnarray_layout(arr, ndim, shape, arr->size, itype);

    SET_KDATA_NO_ID_END(p, arr->shape, ndim, itype);

done:
    csound->UnlockMutex(p->k_data.registry->mutex);
    return res;
}

static void transpose_data_assign(const double *source, double *destination, size_t size, uint32_t ndim, const uint32_t *shape, const size_t *strides, const uint32_t *axes, ITEM_TYPE itype) {
    for (size_t linear = 0; linear < size; ++linear) {
        uint32_t dst_coords[CSN_MAX_DIMS] = {0};
        uint32_t src_coords[CSN_MAX_DIMS] = {0};

        from_linear_to_coords(dst_coords, shape, linear, ndim);

        for (uint32_t i = 0; i < ndim; ++i) {
            src_coords[axes[i]] = dst_coords[i];
        }

        size_t src_index = from_coords_to_offset(src_coords, strides, ndim);
        if (itype == CSN_REAL) {
            destination[linear] = source[src_index];
        } else {
            destination[linear * 2] = source[src_index * 2];
            destination[linear * 2 + 1] = source[src_index * 2 + 1];
        }
    }
}

static int32_t transpose_axes_assign(const ARRAYDAT *shape, uint32_t *axes, uint32_t ndim) {
    bool used[CSN_MAX_DIMS] = {false};
    for (uint32_t i = 0; i < ndim; ++i) {
        double axis_value = (double) shape->data[i];
        if (!IS_VALID_AXIS(axis_value, ndim)) {
            return NOTOK;
        }

        uint32_t axis = (uint32_t) axis_value;
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
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

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
    transpose_data_assign(arr->data, dst->data, arr->size, ndim, shape, arr->strides, axes, arr->itype);
    SET_KDATA_BEGIN(p, reg);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_transpose_k(CSOUND *csound, CSN_RESHAPE *p) {
    CHECK_REG_HANDLE(csound, h, p->k_data.registry, p->k_data.owned_handle);

    uint32_t source_handle = p->source_handle->id;

    bool is_default = p->INOCOUNT < 2;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(p->k_data.registry->mutex);

    CSN_SLOT *source_slot = get_slot(p->k_data.registry, source_handle);
    if (source_slot == NULL) {
        res = csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
        goto done;
    }

    CSN_SLOT *output_slot = get_slot(p->k_data.registry, p->k_data.owned_handle);
    if (output_slot == NULL) {
        res = csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) p->k_data.owned_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = source_slot->array;
    CSN_ARRAY *output_arr = output_slot->array;

    uint32_t ndim = source_arr->ndim;
    uint32_t shape[CSN_MAX_DIMS] = {0};
    uint32_t axes[CSN_MAX_DIMS] = {0};

    if (is_default) {
        for (uint32_t i = 0; i < ndim; ++i)
            axes[i] = ndim - 1 - i;
    }
    else {
        if (p->new_shape->dimensions != 1 || p->new_shape->sizes == NULL || p->new_shape->sizes[0] != (int32_t) ndim) {
            res = csound->PerfError(csound, &p->h, "[csnarray] Axes argument must be a 1-D array of exactly %u elements, one per dimension", ndim);
            goto done;
        }

        if (transpose_axes_assign(p->new_shape, axes, ndim) != OK) {
            res = csound->PerfError(csound, &p->h, "[csnarray] Axes argument is not a valid permutation");
            goto done;
        }
    }

    for (uint32_t i = 0; i < ndim; ++i) {
        shape[i] = source_arr->shape[axes[i]];
    }

    size_t requested_size = 0;
    res = get_array_size_from_shape(&requested_size, ndim, shape);
    if (res != OK) {
        res = csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
        goto done;
    }

    ITEM_TYPE itype = source_arr->itype;
    bool request_changed = IS_REQUEST_CHANGED(&p->k_data, ndim, itype, shape);
    if (SHOULD_SLOT_BE_UPDATED(request_changed, output_arr, itype, requested_size)) {
        res = update_slot_array_locked(csound, p->k_data.registry, p->k_data.owned_handle, ndim, shape, itype, &p->array, &err);
        if (res != OK) {
            res = csound->PerfError(csound, &p->h, "[csnarray] Could not update k-rate output slot: %s", err != NULL ? err : "unknown error");
            goto done;
        }
    }

    CSN_ARRAY *dst = p->array;
    dst->size = source_arr->size;
    transpose_data_assign(source_arr->data, dst->data, source_arr->size, ndim, shape, source_arr->strides, axes, source_arr->itype);

    SET_KDATA_END(p, shape, ndim, itype);

done:
    csound->UnlockMutex(p->k_data.registry->mutex);
    return res;
}

int32_t csnarray_transpose_in(CSOUND *csound, CSN_RESHAPE_IN *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

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
    transpose_data_assign(arr->data, data, arr->size, ndim, shape, arr->strides, axes, arr->itype);

    memcpy(arr->data, data, sizeof(double) * arr->size * arr->itype);
    memset(arr->shape, 0, sizeof(arr->shape));
    memset(arr->strides, 0, sizeof(arr->strides));

    for (uint32_t i = 0; i < ndim; ++i) {
        arr->shape[i] = shape[i];
        arr->strides[i] = strides[i];
    }

done:
    csound->UnlockMutex(reg->mutex);
    if (data != NULL) {
        csound->Free(csound, data);
    }
    return res;
}

static int32_t csnarray_transpose_in_k_init(CSOUND *csound, CSN_RESHAPE_IN *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

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

    size_t scratch_capacity = arr->size * 2 * arr->itype;
    data = csound->Calloc(csound, sizeof(double) * scratch_capacity);
    if (data == NULL) {
        res = csound->InitError(csound, "[csnarray] Out of memory: allocation of %zu bytes failed", (size_t) (sizeof(double) * arr->size * arr->itype));
        goto done;
    }
    p->scratch_capacity = scratch_capacity;
    p->scratch = data;

    for (uint32_t i = 0; i < ndim; ++i) {
        shape[i] = arr->shape[axes[i]];
    }
    transpose_data_assign(arr->data, p->scratch, arr->size, ndim, shape, arr->strides, axes, arr->itype);
    memcpy(arr->data, p->scratch, sizeof(double) * arr->size * arr->itype);

    compute_strides(shape, strides, ndim);

    memset(arr->shape, 0, sizeof(arr->shape));
    memset(arr->strides, 0, sizeof(arr->strides));

    for (uint32_t i = 0; i < ndim; ++i) {
        arr->shape[i] = shape[i];
        arr->strides[i] = strides[i];
    }

    memset(p->k_data.prev_shape, 0, sizeof(p->k_data.prev_shape));
    SET_KDATA_WITH_ID_BEGIN(p, reg, shape, ndim, arr->itype, source_handle);
    p->k_data.prev_size = arr->size;
    memset(p->k_data.prev_axes, 0, sizeof(p->k_data.prev_axes));
    memcpy(p->k_data.prev_axes, axes, sizeof(uint32_t) * arr->ndim);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_transpose_in_k_deinit(CSOUND *csound, CSN_RESHAPE_IN *p) {
    if (p->scratch != NULL) {
        csound->Free(csound, p->scratch);
    }

    return OK;
}

int32_t csnarray_transpose_in_k(CSOUND *csound, CSN_RESHAPE_IN *p) {
    CHECK_REG_HANDLE(csound, h, p->k_data.registry, p->k_data.owned_handle);

    uint32_t source_handle = p->source_handle->id;

    bool is_default = p->INOCOUNT < 2;
    int32_t res = OK;

    csound->LockMutex(p->k_data.registry->mutex);

    CSN_SLOT *slot = get_slot(p->k_data.registry, source_handle);
    if (slot == NULL) {
        res = csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
        goto done;
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
            res = csound->PerfError(csound, &p->h, "[csnarray] Axes argument must be a 1-D array of exactly %u elements, one per dimension", ndim);
            goto done;
        }

        if (transpose_axes_assign(p->new_shape, axes, ndim) != OK) {
            res = csound->PerfError(csound, &p->h, "[csnarray] Axes argument is not a valid permutation");
            goto done;
        }
    }

    for (uint32_t i = 0; i < ndim; ++i) {
        shape[i] = arr->shape[axes[i]];
    }

    compute_strides(shape, strides, ndim);

    bool axes_changed = memcmp(axes, p->k_data.prev_axes, sizeof(axes)) != 0;
    res = CHECK_IF_REALLOC_IN(csound, &p->h, &p->k_data, arr, &p->scratch, &p->scratch_capacity, ndim, itype, axes_changed);
    if (res != OK) {
        res = res == NOTOK ? OK : res;
        goto done;
    }

    transpose_data_assign(arr->data, p->scratch, arr->size, ndim, shape, arr->strides, axes, arr->itype);
    memcpy(arr->data, p->scratch, sizeof(double) * arr->size * arr->itype);

    memset(arr->shape, 0, sizeof(arr->shape));
    memset(arr->strides, 0, sizeof(arr->strides));

    for (uint32_t i = 0; i < ndim; ++i) {
        arr->shape[i] = shape[i];
        arr->strides[i] = strides[i];
    }

    memset(p->k_data.prev_shape, 0, sizeof(p->k_data.prev_shape));
    SET_KDATA_NO_ID_END(p, arr->shape, ndim, itype);
    p->k_data.prev_size = arr->size;
    memset(p->k_data.prev_axes, 0, sizeof(p->k_data.prev_axes));
    memcpy(p->k_data.prev_axes, axes, sizeof(axes));

done:
    csound->UnlockMutex(p->k_data.registry->mutex);
    return res;
}


static void flip_assign_value(CSN_ARRAY *source, CSN_ARRAY *destination, double *buffer, uint32_t *dest_shape, uint32_t ndim, uint32_t axis) {
    for (size_t linear = 0; linear < source->size; ++linear) {
        uint32_t dst_coords[CSN_MAX_DIMS] = {0};
        uint32_t src_coords[CSN_MAX_DIMS] = {0};

        from_linear_to_coords(dst_coords, dest_shape, linear, ndim);

        // flip coords
        for (uint32_t i = 0; i < ndim; ++i) {
            src_coords[i] = dst_coords[i];
        }

        if (axis == -1) {
            for (uint32_t i = 0; i < ndim; ++i) {
                src_coords[i] = source->shape[i] - 1 - src_coords[i];
            }
        } else {
            src_coords[axis] = source->shape[axis] - 1 - src_coords[axis];
        }

        size_t src_index = from_coords_to_offset(src_coords, source->strides, ndim);
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
}

int32_t csnarray_flip(CSOUND *csound, CSN_FLIP_ROLL *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

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

    double axis_value = (double) *p->param_a;
    if (axis_value != -1.0 && !IS_VALID_AXIS(axis_value, ndim)) {
        res = csound->InitError(csound, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: -1 for all axes, or finite integers 0..%u)", axis_value, ndim, ndim - 1);
        goto done;
    }
    int32_t axis_flip = (int32_t) axis_value;

    /* _locked: the registry mutex is already held and is not recursive. */
    if (create_csnarray_locked(csound, reg, &p->h, ndim, arr->shape, &p->array, p->handle, &source_handle, 1U, &err, arr->itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *dst = p->array;
    dst->size = arr->size;
    flip_assign_value(arr, dst, NULL, dst->shape, ndim, axis_flip);
    SET_KDATA_BEGIN(p, reg);
    p->k_data.prev_axis = axis_flip;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_flip_k(CSOUND *csound, CSN_FLIP_ROLL *p) {
    CHECK_REG_HANDLE(csound, h, p->k_data.registry, p->k_data.owned_handle);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(p->k_data.registry->mutex);

    CSN_SLOT *slot = get_slot(p->k_data.registry, source_handle);
    if (slot == NULL) {
        res = csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
        goto done;
    }

    CSN_ARRAY *arr = slot->array;
    uint32_t ndim = arr->ndim;

    double axis_value = (double) *p->param_a;
    if (axis_value != -1.0 && !IS_VALID_AXIS(axis_value, ndim)) {
        res = csound->PerfError(csound, &p->h, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: -1 for all axes, or finite integers 0..%u)", axis_value, ndim, ndim - 1);
        goto done;
    }
    int32_t axis_flip = (int32_t) axis_value;

    ITEM_TYPE itype = arr->itype;
    CSN_ARRAY *dst = p->array;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &dst, &p->k_data, ndim, arr->shape, arr->size, arr->itype, err);
    if (res != OK) goto done;

    flip_assign_value(arr, dst, NULL, dst->shape, ndim, axis_flip);
    SET_KDATA_END(p, arr->shape, ndim, arr->itype);
    p->k_data.prev_axis = axis_value;

done:
    csound->UnlockMutex(p->k_data.registry->mutex);
    return res;
}

int32_t csnarray_flip_in(CSOUND *csound, CSN_FLIP_ROLL_IN *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

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

    double axis_value = (double) *p->param_a;
    if (axis_value != -1.0 && !IS_VALID_AXIS(axis_value, ndim)) {
        res = csound->InitError(csound, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: -1 for all axes, or finite integers 0..%u)", axis_value, ndim, ndim - 1);
        goto done;
    }
    int32_t axis_flip = (int32_t) axis_value;

    /* Allocated only once the axes are known good, so the rejection paths
       above have nothing to release. */
    data = csound->Calloc(csound, sizeof(double) * arr->size * arr->itype);
    if (data == NULL) {
        res = csound->InitError(csound, "[csnarray] Out of memory: allocation of %zu bytes failed", (size_t) (sizeof(double) * arr->size * arr->itype));
        goto done;
    }

    flip_assign_value(arr, NULL, data, arr->shape, ndim, axis_flip);
    memcpy(arr->data, data, sizeof(double) * arr->size * arr->itype);

done:
    csound->UnlockMutex(reg->mutex);
    if (data != NULL) {
        csound->Free(csound, data);
    }
    return res;
}

static int32_t csnarray_flip_in_k_deinit(CSOUND *csound, CSN_FLIP_ROLL_IN *p) {
    if (p->scratch != NULL) {
        csound->Free(csound, p->scratch);
    }
    return OK;
}

static int32_t csnarray_flip_in_k_init(CSOUND *csound, CSN_FLIP_ROLL_IN *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

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

    double axis_value = (double) *p->param_a;
    if (axis_value != -1.0 && !IS_VALID_AXIS(axis_value, ndim)) {
        res = csound->InitError(csound, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: -1 for all axes, or finite integers 0..%u)", axis_value, ndim, ndim - 1);
        goto done;
    }
    int32_t axis_flip = (int32_t) axis_value;

    size_t required = arr->size * (size_t) arr->itype;
    size_t s_capacity = required > 0 ? required * 2 : 1;
    double *data = csound->Calloc(csound, sizeof(double) * s_capacity);
    if (data == NULL) {
        res = csound->InitError(csound, "[csnarray] Out of memory: allocation of %zu bytes failed", (size_t) (sizeof(double) * arr->size * arr->itype));
        goto done;
    }

    flip_assign_value(arr, NULL, data, arr->shape, ndim, axis_flip);
    memcpy(arr->data, data, sizeof(double) * arr->size * arr->itype);
    p->scratch = data;
    p->scratch_capacity = s_capacity;

    memset(p->k_data.prev_shape, 0, sizeof(p->k_data.prev_shape));
    SET_KDATA_WITH_ID_BEGIN(p, reg, arr->shape, ndim, arr->itype, source_handle);
    p->k_data.prev_size = arr->size;
    p->k_data.prev_axis = axis_flip;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_flip_in_k(CSOUND *csound, CSN_FLIP_ROLL_IN *p) {
    CHECK_REG_HANDLE(csound, h, p->k_data.registry, p->k_data.owned_handle);

    uint32_t source_handle = p->source_handle->id;
    int32_t res = OK;

    csound->LockMutex(p->k_data.registry->mutex);

    CSN_SLOT *slot = get_slot(p->k_data.registry, source_handle);
    if (slot == NULL) {
        res = csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
        goto done;
    }

    CSN_ARRAY *arr = slot->array;
    uint32_t ndim = arr->ndim;
    ITEM_TYPE itype = arr->itype;

    double axis_value = (double) *p->param_a;
    if (axis_value != -1.0 && !IS_VALID_AXIS(axis_value, ndim)) {
        res = csound->PerfError(csound, &p->h, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: -1 for all axes, or finite integers 0..%u)", axis_value, ndim, ndim - 1);
        goto done;
    }
    int32_t axis_flip = (int32_t) axis_value;

    bool axis_changed = axis_flip != p->k_data.prev_axis;
    res = CHECK_IF_REALLOC_IN(csound, &p->h, &p->k_data, arr, &p->scratch, &p->scratch_capacity, ndim, itype, axis_changed);
    if (res != OK) {
        res = res == NOTOK ? OK : res;
        goto done;
    }

    memset(p->scratch, 0, sizeof(double) * p->scratch_capacity);
    flip_assign_value(arr, NULL, p->scratch, arr->shape, ndim, axis_flip);

    memcpy(arr->data, p->scratch, sizeof(double) * arr->size * arr->itype);
    memset(p->k_data.prev_shape, 0, sizeof(p->k_data.prev_shape));
    SET_KDATA_NO_ID_END(p, arr->shape, ndim, itype);
    p->k_data.prev_size = arr->size;
    p->k_data.prev_axis = axis_flip;

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

static void rollaxis_assign_value(CSN_ARRAY *source, CSN_ARRAY *destination, double *buffer, uint32_t *dest_shape, uint32_t ndim, int32_t shift, int32_t axis) {
    for (size_t linear = 0; linear < source->size; ++linear) {
        uint32_t dst_coords[CSN_MAX_DIMS] = {0};
        uint32_t src_coords[CSN_MAX_DIMS] = {0};

        from_linear_to_coords(dst_coords, dest_shape, linear, ndim);

        for (uint32_t i = 0; i < ndim; ++i) {
            src_coords[i] = dst_coords[i];
        }

        if (axis == -1) {
            for (uint32_t i = 0; i < ndim; ++i) {
                src_coords[i] = wrap_index((int64_t) dst_coords[i] - shift, (uint32_t) source->shape[i]);
            }
        } else {
            src_coords[axis] = wrap_index((int64_t) dst_coords[axis] - shift, (uint32_t) source->shape[axis]);
        }

        size_t src_index = from_coords_to_offset(src_coords, source->strides, ndim);
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

int32_t csnarray_roll(CSOUND *csound, CSN_FLIP_ROLL *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

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
    CHECK_REG_HANDLE(csound, h, p->k_data.registry, p->k_data.owned_handle);

    CSN_REGISTRY *reg = p->k_data.registry;
    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
        goto done;
    }

    CSN_ARRAY *arr = slot->array;
    uint32_t ndim = arr->ndim;
    double shift_value = (double) *p->param_a;
    if (!IS_VALID_SHIFT(shift_value)) {
        res = csound->PerfError(csound, &p->h, "[csnarray] Shift %g is invalid; shift must be a finite integer in %d..%d", shift_value, INT32_MIN, INT32_MAX);
        goto done;
    }
    int32_t shift = (int32_t) shift_value;

    ITEM_TYPE itype = arr->itype;
    CSN_ARRAY *dst = p->array;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &dst, &p->k_data, ndim, arr->shape, arr->size, itype, err);
    if (res != OK) goto done;

    roll_assign_value(arr, dst, NULL, shift);

    SET_KDATA_END(p, arr->shape, ndim, itype);
    p->k_data.prev_roll_shift = shift;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}


int32_t csnarray_roll_in(CSOUND *csound, CSN_FLIP_ROLL_IN *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

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

done:
    csound->UnlockMutex(reg->mutex);
    if (data != NULL) {
        csound->Free(csound, data);
    }
    return res;
}

static int32_t csnarray_roll_in_k_init(CSOUND *csound, CSN_FLIP_ROLL_IN *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

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

    size_t required = arr->size * (size_t) arr->itype;
    size_t capacity = required > 0 ? required * 2 : 1;
    double *data = csound->Calloc(csound, sizeof(double) * capacity);
    if (data == NULL) {
        res = csound->InitError(csound, "[csnarray] Out of memory: allocation of %zu bytes failed", (size_t) (sizeof(double) * arr->size * arr->itype));
        goto done;
    }
    p->scratch = data;
    p->scratch_capacity = capacity;

    roll_assign_value(arr, NULL, p->scratch, shift);

    memcpy(arr->data, data, sizeof(double) * arr->size * arr->itype);
    SET_KDATA_WITH_ID_BEGIN(p, reg, arr->shape, arr->ndim, arr->itype, source_handle);
    p->k_data.prev_size = arr->size;
    p->k_data.prev_roll_shift = shift;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_roll_in_k(CSOUND *csound, CSN_FLIP_ROLL_IN *p) {
    CHECK_REG_HANDLE(csound, h, p->k_data.registry, p->k_data.owned_handle);

    CSN_REGISTRY *reg = p->k_data.registry;
    uint32_t source_handle = p->source_handle->id;
    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
        goto done;
    }

    CSN_ARRAY *arr = slot->array;
    uint32_t ndim = arr->ndim;
    uint32_t itype = arr->itype;
    double shift_value = (double) *p->param_a;
    if (!IS_VALID_SHIFT(shift_value)) {
        res = csound->PerfError(csound, &p->h, "[csnarray] Shift %g is invalid; shift must be a finite integer in %d..%d", shift_value, INT32_MIN, INT32_MAX);
        goto done;
    }
    int32_t shift = (int32_t) shift_value;

    bool shift_changed = shift != p->k_data.prev_roll_shift;

    res = CHECK_IF_REALLOC_IN(csound, &p->h, &p->k_data, arr, &p->scratch, &p->scratch_capacity, ndim, itype, shift_changed);
    if (res != OK) {
        res = res == NOTOK ? OK : res;
        goto done;
    }

    roll_assign_value(arr, NULL, p->scratch, shift);

    memcpy(arr->data, p->scratch, sizeof(double) * arr->size * arr->itype);
    SET_KDATA_WITH_ID_BEGIN(p, reg, arr->shape, arr->ndim, arr->itype, source_handle);
    p->k_data.prev_size = arr->size;
    p->k_data.prev_roll_shift = shift;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_rollaxis(CSOUND *csound, CSN_FLIP_ROLL *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

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
    if (axis_value != -1.0 && !IS_VALID_AXIS(axis_value, ndim)) {
        res = csound->InitError(csound, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: -1 for all axes, or finite integers 0..%u)", axis_value, ndim, ndim - 1);
        goto done;
    }
    int32_t axis_roll = (int32_t) axis_value;

    /* _locked: the registry mutex is already held and is not recursive. */
    if (create_csnarray_locked(csound, reg, &p->h, ndim, arr->shape, &p->array, p->handle, &source_handle, 1U, &err, arr->itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *dst = p->array;
    dst->size = arr->size;
    rollaxis_assign_value(arr, dst, NULL, dst->shape, ndim, shift, axis_roll);
    SET_KDATA_BEGIN(p, reg);
    p->k_data.prev_roll_shift = shift;
    p->k_data.prev_axis = axis_roll;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_rollaxis_k(CSOUND *csound, CSN_FLIP_ROLL *p) {
    CHECK_REG_HANDLE(csound, h, p->k_data.registry, p->k_data.owned_handle);

    uint32_t source_handle = p->source_handle->id;
    CSN_REGISTRY *reg = p->k_data.registry;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
        goto done;
    }

    CSN_ARRAY *arr = slot->array;
    uint32_t ndim = arr->ndim;
    double shift_value = (double) *p->param_a;
    if (!IS_VALID_SHIFT(shift_value)) {
        res = csound->PerfError(csound, &p->h, "[csnarray] Shift %g is invalid; shift must be a finite integer in %d..%d", shift_value, INT32_MIN, INT32_MAX);
        goto done;
    }
    int32_t shift = (int32_t) shift_value;
    double axis_value = (double) *p->param_b;
    if (axis_value != -1.0 && !IS_VALID_AXIS(axis_value, ndim)) {
        res = csound->PerfError(csound, &p->h, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: -1 for all axes, or finite integers 0..%u)", axis_value, ndim, ndim - 1);
        goto done;
    }
    int32_t axis_roll = (int32_t) axis_value;

    CSN_ARRAY *dst = p->array;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &dst, &p->k_data, ndim, arr->shape, arr->size, arr->itype, err);
    if (res != OK) goto done;

    rollaxis_assign_value(arr, dst, NULL, dst->shape, ndim, shift, axis_roll);
    SET_KDATA_END(p, arr->shape, ndim, arr->itype);
    p->k_data.prev_axis = axis_roll;
    p->k_data.prev_roll_shift = shift;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_rollaxis_in(CSOUND *csound, CSN_FLIP_ROLL_IN *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

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
    if (axis_value != -1.0 && !IS_VALID_AXIS(axis_value, ndim)) {
        res = csound->InitError(csound, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: -1 for all axes, or finite integers 0..%u)", axis_value, ndim, ndim - 1);
        goto done;
    }
    int32_t axis_roll = (int32_t) axis_value;

    /* Allocated only once the axes are known good, so the rejection paths
       above have nothing to release. */
    data = csound->Calloc(csound, sizeof(double) * arr->size * arr->itype);
    if (data == NULL) {
        res = csound->InitError(csound, "[csnarray] Out of memory: allocation of %zu bytes failed", (size_t) (sizeof(double) * arr->size * arr->itype));
        goto done;
    }

    rollaxis_assign_value(arr, NULL, data, arr->shape, ndim, shift, axis_roll);
    memcpy(arr->data, data, sizeof(double) * arr->size * arr->itype);

done:
    csound->UnlockMutex(reg->mutex);
    if (data != NULL) {
        csound->Free(csound, data);
    }
    return res;
}

static int32_t csnarray_rollaxis_in_k_init(CSOUND *csound, CSN_FLIP_ROLL_IN *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

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
    if (axis_value != -1.0 && !IS_VALID_AXIS(axis_value, ndim)) {
        res = csound->InitError(csound, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: -1 for all axes, or finite integers 0..%u)", axis_value, ndim, ndim - 1);
        goto done;
    }
    int32_t axis_roll = (int32_t) axis_value;

    size_t required = arr->size * (size_t) arr->itype;
    size_t capacity = required > 0 ? required * 2 : 1;
    double *data = csound->Calloc(csound, sizeof(double) * capacity);
    if (data == NULL) {
        res = csound->InitError(csound, "[csnarray] Out of memory: allocation of %zu bytes failed", (size_t) (sizeof(double) * arr->size * arr->itype));
        goto done;
    }
    p->scratch = data;
    p->scratch_capacity = capacity;

    rollaxis_assign_value(arr, NULL, p->scratch, arr->shape, ndim, shift, axis_roll);
    memcpy(arr->data, p->scratch, sizeof(double) * arr->size * arr->itype);
    SET_KDATA_WITH_ID_BEGIN(p, reg, arr->shape, ndim, arr->itype, source_handle);
    p->k_data.prev_size = arr->size;
    p->k_data.prev_axis = axis_roll;
    p->k_data.prev_roll_shift = shift;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_rollaxis_in_k(CSOUND *csound, CSN_FLIP_ROLL_IN *p) {
    CHECK_REG_HANDLE(csound, h, p->k_data.registry, p->k_data.owned_handle);
    uint32_t source_handle = p->source_handle->id;
    CSN_REGISTRY *reg = p->k_data.registry;

    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
        goto done;
    }

    CSN_ARRAY *arr = slot->array;
    uint32_t ndim = arr->ndim;
    double shift_value = (double) *p->param_a;
    if (!IS_VALID_SHIFT(shift_value)) {
        res = csound->PerfError(csound, &p->h, "[csnarray] Shift %g is invalid; shift must be a finite integer in %d..%d", shift_value, INT32_MIN, INT32_MAX);
        goto done;
    }
    int32_t shift = (int32_t) shift_value;
    double axis_value = (double) *p->param_b;
    if (axis_value != -1.0 && !IS_VALID_AXIS(axis_value, ndim)) {
        res = csound->PerfError(csound, &p->h, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: -1 for all axes, or finite integers 0..%u)", axis_value, ndim, ndim - 1);
        goto done;
    }
    int32_t axis_roll = (int32_t) axis_value;

    bool is_changed = (shift != p->k_data.prev_roll_shift) || (axis_roll != p->k_data.prev_axis);
    res = CHECK_IF_REALLOC_IN(csound, &p->h, &p->k_data, arr, &p->scratch, &p->scratch_capacity, ndim, arr->itype, is_changed);
    if (res != OK) {
        res = res == NOTOK ? OK : res;
        goto done;
    }

    rollaxis_assign_value(arr, NULL, p->scratch, arr->shape, ndim, shift, axis_roll);
    SET_KDATA_NO_ID_END(p, arr->shape, ndim, arr->itype);
    memcpy(arr->data, p->scratch, sizeof(double) * arr->size * arr->itype);
    p->k_data.prev_size = arr->size;
    p->k_data.prev_axis = axis_roll;
    p->k_data.prev_roll_shift = shift;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t get_index_offset(CSOUND *csound, OPDS *perf_h, size_t *offset, uint32_t ndim, const CSN_ARRAY *arr, const MYFLT *indexes) {
    size_t temp_offset = 0;
    for (uint32_t i = 0; i < ndim; i++) {
        double index_value = (double) indexes[i];

        if (!IS_VALID_INDEX(index_value)) {
            return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] Index %g at position %u is invalid; indexes must be finite non-negative integers", index_value, i);
        }

        uint32_t index = (uint32_t) index_value;
        if (index >= arr->shape[i]) {
            return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] Index %u at position %u is out of range for extent %u (valid: 0..%u)", index, i, arr->shape[i], arr->shape[i] - 1);
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
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", handle);
    }

    CSN_ARRAY *arr = slot->array;
    uint32_t ndim = arr->ndim;

    if (arr->itype != expected_itype) {
        if (expected_itype == CSN_COMPLEX) {
            return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] Handle holds a real array; use the real form of the accessor");
        }
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] Handle holds a complex array; declare the value as :Complex; to read or write it");
    }

    if (indexes == NULL || indexes->data == NULL || indexes->sizes == NULL || indexes->dimensions != 1 || indexes->sizes[0] != (int32_t) ndim) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] Index argument must be a 1-D array of exactly %u elements, one per dimension", ndim);
    }

    int32_t res = get_index_offset(csound, perf_h, out_offset, ndim, arr, indexes->data);
    if (res != OK) {
        return res;
    }

    if (*out_offset >= arr->size) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] Index resolves to offset %zu, outside the logical array size %zu", *out_offset, arr->size);
    }

    *out_arr = arr;
    return OK;
}

static int32_t csnarray_get_set_locked(CSOUND *csound, OPDS *perf_h, CSN_REGISTRY *reg, uint32_t handle, ARRAYDAT *indexes, MYFLT *value, bool is_get) {
    if (value == NULL) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] Internal error: null value pointer passed to the element accessor");
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
        arr->data[offset] = (double) *value;
    }

    return OK;
}

static int32_t csnarray_get_set_complex_locked(CSOUND *csound, OPDS *perf_h, CSN_REGISTRY *reg, uint32_t handle, ARRAYDAT *indexes, COMPLEXDAT *value, bool is_get) {
    if (value == NULL) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] Internal error: null value pointer passed to the element accessor");
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
        arr->data[at] = re;
        arr->data[at + 1] = im;
    }

    return OK;
}

int32_t csnarray_get(CSOUND *csound, CSN_GET *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }
    uint32_t handle = p->source_handle->id;
    csound->LockMutex(reg->mutex);
    int32_t res = csnarray_get_set_locked(csound, NULL, reg, handle, p->indexes, p->value, true);
    p->k_data.registry = reg;
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_get_k(CSOUND *csound, CSN_GET *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    if (reg == NULL) {
        return csound->PerfError(csound, &p->h, "[csnarray] [csnarray] Internal error: the csnum array registry is not available");
    }

    uint32_t handle = p->source_handle->id;
    csound->LockMutex(reg->mutex);
    int32_t res = csnarray_get_set_locked(csound, &p->h, reg, handle, p->indexes, p->value, true);
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_get_complex(CSOUND *csound, CSN_GETCOMPLEX *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

    uint32_t handle = p->source_handle->id;

    csound->LockMutex(reg->mutex);
    int32_t res = csnarray_get_set_complex_locked(csound, NULL, reg, handle, p->indexes, p->value, true);
    p->k_data.registry = reg;
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_get_complex_k(CSOUND *csound, CSN_GETCOMPLEX *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    if (reg == NULL) {
        return csound->PerfError(csound, &p->h, "[csnarray] Internal error: the csnum array registry is not available");
    }

    uint32_t handle = p->source_handle->id;

    csound->LockMutex(reg->mutex);
    int32_t res = csnarray_get_set_complex_locked(csound, &p->h, reg, handle, p->indexes, p->value, true);
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_set(CSOUND *csound, CSN_SET *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

    uint32_t handle = p->source_handle->id;

    csound->LockMutex(reg->mutex);
    int32_t res = csnarray_get_set_locked(csound, NULL, reg, handle, p->indexes, p->value, false);
    p->k_data.registry = reg;
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_set_k(CSOUND *csound, CSN_SET *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    if (reg == NULL) {
        return csound->PerfError(csound, &p->h, "[csnarray] Internal error: the csnum array registry is not available");
    }

    uint32_t handle = p->source_handle->id;

    csound->LockMutex(reg->mutex);
    int32_t res = csnarray_get_set_locked(csound, &p->h, reg, handle, p->indexes, p->value, false);
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_set_complex(CSOUND *csound, CSN_SETCOMPLEX *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

    uint32_t handle = p->source_handle->id;

    csound->LockMutex(reg->mutex);
    int32_t res = csnarray_get_set_complex_locked(csound, NULL, reg, handle, p->indexes, p->value, false);
    p->k_data.registry = reg;
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_set_complex_k(CSOUND *csound, CSN_SETCOMPLEX *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->PerfError(csound, &p->h, "[csnarray] Internal error: the csnum array registry is not available");
    }

    uint32_t handle = p->source_handle->id;

    csound->LockMutex(reg->mutex);
    int32_t res = csnarray_get_set_complex_locked(csound, &p->h, reg, handle, p->indexes, p->value, false);
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t check_take_flat_body(CSOUND *csound, OPDS *perf_h, CSN_REGISTRY *reg, CSN_SLOT **slot, CSN_ARRAY **arr, uint32_t handle, double index, bool is_complex) {
    *slot = get_slot(reg, handle);
    if ((*slot) == NULL) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", handle);
    }

    *arr = (*slot)->array;
    if (!is_complex) {
        if ((*arr)->itype == CSN_COMPLEX) {
            return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] Handle holds a complex array; declare the taken value as :Complex;");
        }
    } else {
        if ((*arr)->itype != CSN_COMPLEX) {
            return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] Handle holds a real array; declare the taken value as i or k");
        }
    }
    if (!IS_VALID_INDEX(index)) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] Index %g is invalid; indexes must be finite non-negative integers", index);
    }

    uint32_t valid_index = (uint32_t) index;

    if (valid_index >= (*arr)->size) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] Index %u is out of range", valid_index);
    }

    return OK;
}

static int32_t check_take_body(CSOUND *csound, OPDS *perf_h, CSN_REGISTRY *reg, CSN_SLOT **slot, CSN_ARRAY **arr, uint32_t *shape, uint32_t *out_ndim, uint32_t *out_axis, uint32_t *out_index, uint32_t handle, double index, double in_axis) {
    *slot = get_slot(reg, handle);
    if ((*slot) == NULL) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", handle);
    }

    *arr = (*slot)->array;
    uint32_t ndim = (*arr)->ndim;

    /* Dropping the only axis would leave a rank-0 array, which the registry
       cannot represent. That case is the two-argument form, which yields a
       plain scalar. */
    if (ndim < 2) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] Take along an axis needs a 2-D or higher array; use the two-argument form for a scalar");
    }

    if (!IS_VALID_AXIS(in_axis, ndim)) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: finite integers 0..%u)", in_axis, ndim, ndim - 1);
    }
    *out_axis = (uint32_t) in_axis;
    uint32_t axis = *out_axis;

    if (!IS_VALID_INDEX(index)) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] Index %g is invalid; indexes must be finite non-negative integers", index);
    }
    uint32_t valid_index = (uint32_t) index;

    if (valid_index >= (*arr)->shape[axis]) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] Index %u is out of range", valid_index);
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
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

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
    p->k_data.prev_axis = axis;
    p->k_data.prev_index = index;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_take_k(CSOUND *csound, CSN_TAKE *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REG_HANDLE(csound, h, reg, p->k_data.owned_handle);

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
    res = check_take_body(csound, &p->h, reg, &slot, &arr, shape, &out_ndim, &axis, &index, source_handle, (double) *p->index, (double) *p->axis);
    if (res != OK) goto done;

    uint32_t ndim = arr->ndim;
    ITEM_TYPE itype = arr->itype;

    CSN_ARRAY *dst = p->array;
    size_t output_size = 0;
    if (get_array_size_from_shape(&output_size, out_ndim, shape) != OK) {
        res = csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
        goto done;
    }
    size_t logical_size = arr->size == 0 ? 0 : output_size;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &dst, &p->k_data, out_ndim, shape, logical_size, itype, err);
    if (res != OK) goto done;

    take_assign_value(arr, dst, ndim, out_ndim, axis, index);
    SET_KDATA_END(p, shape, out_ndim, itype);
    p->k_data.prev_axis = axis;
    p->k_data.prev_index = index;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

/* Two-argument form: indexes the flattened array and yields a scalar, matching
   np.take(a, i) with axis=None. This is the rank-1 case the axis form cannot
   express, since dropping the only axis would leave nothing behind. */
int32_t csnarray_take_flat(CSOUND *csound, CSN_TAKE_FLAT *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

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
    if (reg == NULL) {
        return csound->PerfError(csound, &p->h, "[csnarray] Internal error: the csnum array registry is not available");
    }

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
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

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
    if (reg == NULL) {
        return csound->PerfError(csound, &p->h, "[csnarray] Internal error: the csnum array registry is not available");
    }

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
    if (!IS_VALID_AXIS(axis_value, ndim)) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: finite integers 0..%u)", axis_value, ndim, ndim - 1);
    }

    if (!IS_VALID_INDEX(start_value) || !IS_VALID_INDEX(stop_value) || !IS_VALID_INDEX(step_value) || step_value == 0.0) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] Invalid slice start=%g stop=%g step=%g: values must be finite integers, start and stop must be non-negative, and step must be > 0", start_value, stop_value, step_value);
    }

    uint32_t axis = (uint32_t) axis_value;
    uint32_t start = (uint32_t) start_value;
    uint32_t stop = (uint32_t) stop_value;
    uint32_t step = (uint32_t) step_value;
    uint32_t extent = array->shape[axis];
    if (start >= extent || stop > extent || stop <= start) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] Invalid slice start=%u stop=%u step=%u on axis %u of extent %u: need 0 <= start < stop <= %u and step > 0", start, stop, step, axis, extent, extent);
    }

    uint32_t sliced_extent = 1U + (stop - start - 1U) / step;
    for (uint32_t i = 0; i < ndim; i++) {
        out_shape[i] = i == axis ? sliced_extent : array->shape[i];
    }

    if (get_array_size_from_shape(out_size, ndim, out_shape) != OK) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] Slice shape is invalid or its element count exceeds the configured limit");
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

int32_t csnarray_get_slice(CSOUND *csound, CSN_GET_SLICE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

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
    res = validate_slice_spec(csound, NULL, arr, (double) *p->axis, (double) *p->start, (double) *p->stop, (double) *p->step, &axis, &start, &step, shape, &output_size);
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

int32_t csnarray_get_slice_k(CSOUND *csound, CSN_GET_SLICE *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REG_HANDLE(csound, h, reg, p->k_data.owned_handle);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
        goto done;
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
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &dst, &p->k_data, ndim, shape, logical_size, arr->itype, err);
    if (res != OK) goto done;

    slice_get_assign_value(arr, dst, ndim, axis, start, step);
    SET_KDATA_BEGIN(p, reg);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_set_slice_locked(CSOUND *csound, OPDS *perf_h, CSN_REGISTRY *reg, CSN_SET_SLICE *p) {
    uint32_t source_handle = p->source_handle->id;
    uint32_t data_handle = p->data_handle->id;

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
    }

    CSN_SLOT *data_slot = get_slot(reg, data_handle);
    if (data_slot == NULL) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", data_handle);
    }

    CSN_ARRAY *source_arr = source_slot->array;
    CSN_ARRAY *data_arr = data_slot->array;

    if (source_arr->itype != data_arr->itype) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] Element type mismatch: one array is real and the other complex");
    }

    uint32_t source_ndim = source_arr->ndim;
    uint32_t data_ndim = data_arr->ndim;
    if (data_ndim != source_ndim) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] Data block has %u dimensions but the source array has %u", data_ndim, source_ndim);
    }
    if (source_arr->size == 0) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] Cannot assign a slice of an empty array");
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
            return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] Data block extent %u on axis %u does not match the slice extent %u", data_arr->shape[i], i, slice_shape[i]);
        }
    }

    if (data_arr->size != slice_size) {
        char sbuf[CSN_SHAPE_STR_MAX];
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] Data block holds %zu elements but the slice %s holds %zu", data_arr->size, shape_str(sbuf, sizeof(sbuf), slice_shape, source_ndim), slice_size);
    }

    slice_set_assign_value(data_arr, source_arr, source_ndim, slice_shape, axis, start, step);
    SET_KDATA_WITH_ID_BEGIN(p, reg, slice_shape, data_ndim, source_arr->itype, source_handle);
    p->k_data.owned_data_handle = data_handle;
    return OK;
}

int32_t csnarray_set_slice(CSOUND *csound, CSN_SET_SLICE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

    csound->LockMutex(reg->mutex);
    int32_t res = csnarray_set_slice_locked(csound, NULL, reg, p);
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_set_slice_k(CSOUND *csound, CSN_SET_SLICE *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REG_HANDLE(csound, h, reg, p->k_data.owned_handle);

    csound->LockMutex(reg->mutex);
    int32_t res = csnarray_set_slice_locked(csound, &p->h, reg, p);
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_push(CSOUND *csound, CSN_PUSH *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

    uint32_t handle = p->source_handle->id;
    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) handle);
        goto done;
    }

    CSN_ARRAY *arr = slot->array;
    if (arr->ndim != 1) {
        res = csound->InitError(csound, "[csnarray] Push needs a 1-D array, got %u-D", arr->ndim);
        goto done;
    }

    if (arr->itype != CSN_REAL) {
        res = csound->InitError(csound, "[csnarray] Handle holds a complex array; push a :Complex; value instead");
        goto done;
    }

    if (arr->size >= CSN_MAX_ELEMS) {
        res = csound->InitError(csound, "[csnarray] Push would exceed the maximum element count: array already holds %zu of %zu elements", arr->size, (size_t) CSN_MAX_ELEMS);
        goto done;
    }

    size_t new_size = arr->size + 1;
    if (new_size > arr->capacity) {
        size_t new_capacity = arr->capacity > 0 ? arr->capacity * 2 : 1;
        double *new_data = csound->ReAlloc(csound, arr->data, sizeof(double) * new_capacity);
        if (new_data == NULL) {
            res = csound->InitError(csound, "[csnarray] Out of memory: allocation of %zu bytes failed", (size_t) (sizeof(double) * new_capacity));
            goto done;
        }

        arr->data = new_data;
        arr->capacity = new_capacity;
    }

    arr->data[arr->size] = (double) *p->in_value;
    arr->size = new_size;
    arr->shape[0] = (uint32_t) new_size;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_pushcomp(CSOUND *csound, CSN_PUSHCOMPLEX *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

    uint32_t handle = p->source_handle->id;
    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) handle);
        goto done;
    }

    CSN_ARRAY *arr = slot->array;
    if (arr->ndim != 1) {
        res = csound->InitError(csound, "[csnarray] Push needs a 1-D array, got %u-D", arr->ndim);
        goto done;
    }

    if (arr->itype != CSN_COMPLEX) {
        res = csound->InitError(csound, "[csnarray] Handle holds a real array; push a real value instead");
        goto done;
    }

    if (arr->size >= CSN_MAX_ELEMS) {
        res = csound->InitError(csound, "[csnarray] Push would exceed the maximum element count: array already holds %zu of %zu elements", arr->size, (size_t) CSN_MAX_ELEMS);
        goto done;
    }

    size_t new_size = arr->size + 1;
    if (new_size > arr->capacity) {
        size_t new_capacity = arr->capacity > 0 ? arr->capacity * 2 : 1;
        size_t bytes = sizeof(double) * new_capacity * arr->itype;
        double *new_data = csound->ReAlloc(csound, arr->data, bytes);
        if (new_data == NULL) {
            res = csound->InitError(csound, "[csnarray] Out of memory: allocation of %zu bytes failed", bytes);
            goto done;
        }

        arr->data = new_data;
        arr->capacity = new_capacity;
    }

    double re, im;
    complexdat_to_rect(p->in_value, &re, &im);

    arr->data[arr->size * 2] = re;
    arr->data[arr->size * 2 + 1] = im;
    arr->size = new_size;
    arr->shape[0] = (uint32_t) new_size;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_pop(CSOUND *csound, CSN_POP *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

    uint32_t handle = p->source_handle->id;
    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) handle);
        goto done;
    }

    CSN_ARRAY *arr = slot->array;
    if (arr->ndim != 1) {
        res = csound->InitError(csound, "[csnarray] Pop needs a 1-D array, got %u-D", arr->ndim);
        goto done;
    }

    if (arr->itype != CSN_REAL) {
        res = csound->InitError(csound, "[csnarray] Handle holds a complex array; declare the popped value as :Complex;");
        goto done;
    }

    if (arr->size == 0) {
        res = csound->InitError(csound, "[csnarray] Cannot pop from an empty array");
        goto done;
    }

    /* size and shape[0] are written from the same value, so they cannot drift
       apart the way a separate emptiness flag could. */

    size_t new_size = arr->size - 1;
    *p->out_value = (MYFLT) arr->data[new_size];
    arr->size = new_size;
    arr->shape[0] = (uint32_t) new_size;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_popcomp(CSOUND *csound, CSN_POPCOMPLEX *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

    uint32_t handle = p->source_handle->id;
    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) handle);
        goto done;
    }

    CSN_ARRAY *arr = slot->array;
    if (arr->ndim != 1) {
        res = csound->InitError(csound, "[csnarray] Pop needs a 1-D array, got %u-D", arr->ndim);
        goto done;
    }

    if (arr->itype != CSN_COMPLEX) {
        res = csound->InitError(csound, "[csnarray] Handle holds a real array; declare the popped value as i");
        goto done;
    }

    if (arr->size == 0) {
        res = csound->InitError(csound, "[csnarray] Cannot pop from an empty array");
        goto done;
    }

    size_t new_size = arr->size - 1;
    p->out_value->real = (MYFLT) arr->data[new_size * 2];
    p->out_value->imag = (MYFLT) arr->data[new_size * 2 + 1];
    p->out_value->isPolar = 0;
    arr->size = new_size;
    arr->shape[0] = (uint32_t) new_size;

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

int32_t csnarray_insert(CSOUND *csound, CSN_PUSH *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

    uint32_t handle = p->source_handle->id;
    if (*p->index < 0) {
        return csound->InitError(csound, "[csnarray] Insert index %d is negative; indexes must be >= 0", (int32_t) *p->index);
    }

    size_t index = (size_t) *p->index;
    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) handle);
        goto done;
    }

    CSN_ARRAY *arr = slot->array;
    if (arr->ndim != 1) {
        res = csound->InitError(csound, "[csnarray] Insert needs a 1-D array, got %u-D", arr->ndim);
        goto done;
    }

    if (arr->itype != CSN_REAL) {
        res = csound->InitError(csound, "[csnarray] Handle holds a complex array; insert a :Complex; value instead");
        goto done;
    }

    if (arr->size >= CSN_MAX_ELEMS) {
        res = csound->InitError(csound, "[csnarray] Insert would exceed the maximum element count: array already holds %zu of %zu elements", arr->size, (size_t) CSN_MAX_ELEMS);
        goto done;
    }

    /* Inclusive upper bound: index == size appends, which is also the only
       way to insert into an array that is currently empty. */
    if (index > arr->size) {
        res = csound->InitError(csound, "[csnarray] Insert index %zu is out of range for an array of %zu elements (valid: 0..%zu, end included)", index, arr->size, arr->size);
        goto done;
    }

    size_t new_size = arr->size + 1;
    if (new_size > arr->capacity) {
        size_t new_capacity = arr->capacity > 0 ? arr->capacity * 2 : 1;
        double *new_data = csound->ReAlloc(csound, arr->data, sizeof(double) * new_capacity);
        if (new_data == NULL) {
            res = csound->InitError(csound, "[csnarray] Out of memory: allocation of %zu bytes failed", (size_t) (sizeof(double) * new_capacity));
            goto done;
        }

        arr->data = new_data;
        arr->capacity = new_capacity;
    }

    size_t count = arr->size - index;
    memmove(arr->data + index + 1, arr->data + index, sizeof(double) * count);
    arr->data[index] = (double) *p->in_value;
    arr->size = new_size;
    arr->shape[0] = (uint32_t) new_size;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_insertcomp(CSOUND *csound, CSN_PUSHCOMPLEX *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

    uint32_t handle = p->source_handle->id;
    if (*p->index < 0) {
        return csound->InitError(csound, "[csnarray] Insert index %d is negative; indexes must be >= 0", (int32_t) *p->index);
    }

    size_t index = (size_t) *p->index;
    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) handle);
        goto done;
    }

    CSN_ARRAY *arr = slot->array;
    if (arr->ndim != 1) {
        res = csound->InitError(csound, "[csnarray] Insert needs a 1-D array, got %u-D", arr->ndim);
        goto done;
    }

    if (arr->itype != CSN_COMPLEX) {
        res = csound->InitError(csound, "[csnarray] Handle holds a real array; insert a real value instead");
        goto done;
    }

    if (arr->size >= CSN_MAX_ELEMS) {
        res = csound->InitError(csound, "[csnarray] Insert would exceed the maximum element count: array already holds %zu of %zu elements", arr->size, (size_t) CSN_MAX_ELEMS);
        goto done;
    }

    /* Inclusive upper bound: index == size appends, which is also the only
       way to insert into an array that is currently empty. */
    if (index > arr->size) {
        res = csound->InitError(csound, "[csnarray] Insert index %zu is out of range for an array of %zu elements (valid: 0..%zu, end included)", index, arr->size, arr->size);
        goto done;
    }

    /* index e size sono in item; dentro data ogni item vale due double. */
    size_t new_size = arr->size + 1;
    if (new_size > arr->capacity) {
        size_t new_capacity = arr->capacity > 0 ? arr->capacity * 2 : 1;
        size_t bytes = sizeof(double) * new_capacity * arr->itype;
        double *new_data = csound->ReAlloc(csound, arr->data, bytes);
        if (new_data == NULL) {
            res = csound->InitError(csound, "[csnarray] Out of memory: allocation of %zu bytes failed", bytes);
            goto done;
        }

        arr->data = new_data;
        arr->capacity = new_capacity;
    }

    size_t count = arr->size - index;
    memmove(arr->data + (index + 1) * 2, arr->data + index * 2, sizeof(double) * count * 2);

    double re, im;
    complexdat_to_rect(p->in_value, &re, &im);
    arr->data[index * 2] = re;
    arr->data[index * 2 + 1] = im;
    arr->size = new_size;
    arr->shape[0] = (uint32_t) new_size;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_remove(CSOUND *csound, CSN_POP *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

    uint32_t handle = p->source_handle->id;
    if (*p->index < 0) {
        return csound->InitError(csound, "[csnarray] Remove index %d is negative; indexes must be >= 0", (int32_t) *p->index);
    }

    size_t index = (size_t) *p->index;
    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) handle);
        goto done;
    }

    CSN_ARRAY *arr = slot->array;
    if (arr->ndim != 1) {
        res = csound->InitError(csound, "[csnarray] Remove needs a 1-D array, got %u-D", arr->ndim);
        goto done;
    }

    if (arr->itype != CSN_REAL) {
        res = csound->InitError(csound, "[csnarray] Handle holds a complex array; declare the removed value as :Complex;");
        goto done;
    }

    if (arr->size == 0) {
        res = csound->InitError(csound, "[csnarray] Cannot remove from an empty array");
        goto done;
    }

    if (index >= arr->size) {
        res = csound->InitError(csound, "[csnarray] Remove index %zu is out of range for an array of %zu elements (valid: 0..%zu)", index, arr->size, arr->size - 1);
        goto done;
    }

    *p->out_value = (MYFLT) arr->data[index];

    size_t count = arr->size - index - 1;
    if (count > 0) {
        memmove(arr->data + index, arr->data + index + 1, sizeof(double) * count);
    }

    arr->size--;
    arr->shape[0] = (uint32_t) arr->size;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_removecomp(CSOUND *csound, CSN_POPCOMPLEX *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

    uint32_t handle = p->source_handle->id;
    if (*p->index < 0) {
        return csound->InitError(csound, "[csnarray] Remove index %d is negative; indexes must be >= 0", (int32_t) *p->index);
    }

    size_t index = (size_t) *p->index;
    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) handle);
        goto done;
    }

    CSN_ARRAY *arr = slot->array;
    if (arr->ndim != 1) {
        res = csound->InitError(csound, "[csnarray] Remove needs a 1-D array, got %u-D", arr->ndim);
        goto done;
    }

    if (arr->itype != CSN_COMPLEX) {
        res = csound->InitError(csound, "[csnarray] Handle holds a real array; declare the removed value as i");
        goto done;
    }

    if (arr->size == 0) {
        res = csound->InitError(csound, "[csnarray] Cannot remove from an empty array");
        goto done;
    }

    if (index >= arr->size) {
        res = csound->InitError(csound, "[csnarray] Remove index %zu is out of range for an array of %zu elements (valid: 0..%zu)", index, arr->size, arr->size - 1);
        goto done;
    }

    /* index e size sono in item; dentro data ogni item vale due double. */
    p->out_value->real = (MYFLT) arr->data[index * 2];
    p->out_value->imag = (MYFLT) arr->data[index * 2 + 1];
    p->out_value->isPolar = 0;

    size_t count = arr->size - index - 1;
    if (count > 0) {
        memmove(arr->data + index * 2, arr->data + (index + 1) * 2, sizeof(double) * count * 2);
    }

    arr->size--;
    arr->shape[0] = (uint32_t) arr->size;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_insert_block(CSOUND *csound, CSN_INSERT_BLOCK *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

    uint32_t source_handle = p->source_handle->id;
    uint32_t data_handle = p->data_handle->id;

    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
        goto done;
    }

    CSN_SLOT *data_slot = get_slot(reg, data_handle);
    if (data_slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) data_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = source_slot->array;
    CSN_ARRAY *data_arr = data_slot->array;

    if (source_arr->itype != data_arr->itype) {
        res = csound->InitError(csound, "[csnarray] Element type mismatch: one array is real and the other complex");
        goto done;
    }

    uint32_t source_ndim = source_arr->ndim;
    uint32_t data_ndim = data_arr->ndim;

    if (data_ndim != source_ndim - 1) {
        res = csound->InitError(csound, "[csnarray] Block is %u-D but inserting into a %u-D array needs a %u-D block", data_ndim, source_ndim, source_ndim - 1);
        goto done;
    }

    double axis_value = (double) *p->axis;
    if (!IS_VALID_AXIS(axis_value, source_ndim)) {
        res = csound->InitError(csound, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: finite integers 0..%u)", axis_value, source_ndim, source_ndim - 1);
        goto done;
    }
    uint32_t axis = (uint32_t) axis_value;

    uint32_t index = (uint32_t) *p->index;
    if (*p->index < 0 || index > source_arr->shape[axis]) {
        res = csound->InitError(csound, "[csnarray] Index %d is out of range for axis %u of extent %u (valid: 0..%u, end included)", (int32_t) *p->index, axis, source_arr->shape[axis], source_arr->shape[axis]);
        goto done;
    }

    for (uint32_t i = 0, j = 0; i < source_ndim; i++) {
        if (i == axis) continue;
        if (data_arr->shape[j++] != source_arr->shape[i]) {
            res = csound->InitError(csound, "[csnarray] Block extent %u does not match the source extent %u on axis %u", data_arr->shape[j - 1], source_arr->shape[i], i);
            goto done;
        }
    }

    uint32_t temp_shape[CSN_MAX_DIMS] = {0};
    memcpy(temp_shape, source_arr->shape, sizeof(uint32_t) * (size_t) source_ndim);
    temp_shape[axis]++;

    CSN_ARRAY *temp = csound->Calloc(csound, sizeof(CSN_ARRAY));
    if (temp == NULL) {
        res = csound->InitError(csound, "[csnarray] Out of memory: allocation of %zu bytes failed", (size_t) (sizeof(CSN_ARRAY)));
        goto done;
    }

    int32_t alloc_temp = allocate_array(csound, temp, source_ndim, temp_shape, source_arr->array_id, source_arr->itype);
    if (alloc_temp != OK) {
        char tbuf[CSN_SHAPE_STR_MAX];
        csound->Free(csound, temp);
        res = csound->InitError(csound, "[csnarray] Out of memory: could not allocate the %u-D temporary array %s", source_ndim, shape_str(tbuf, sizeof(tbuf), temp_shape, source_ndim));
        goto done;
    }

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

    size_t bytes = sizeof(double) * temp->capacity * temp->itype;
    double *new_data = csound->ReAlloc(csound, source_arr->data, bytes);
    if (new_data == NULL) {
        csound->Free(csound, temp->data);
        csound->Free(csound, temp);
        res = csound->InitError(csound, "[csnarray] Out of memory: allocation of %zu bytes failed", bytes);
        goto done;
    }

    source_arr->data = new_data;
    travase_csnarray(source_arr, temp);
    csound->Free(csound, temp->data);
    csound->Free(csound, temp);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_remove_block(CSOUND *csound, CSN_TAKE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

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
    uint32_t source_ndim = source_arr->ndim;

    double axis_value = (double) *p->axis;
    if (!IS_VALID_AXIS(axis_value, source_ndim)) {
        res = csound->InitError(csound, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: finite integers 0..%u)", axis_value, source_ndim, source_ndim - 1);
        goto done;
    }
    uint32_t axis = (uint32_t) axis_value;
    if (source_arr->shape[axis] <= 1) {
        res = csound->InitError(csound, "[csnarray] Axis %u is not usable here: its extent must be > 1 (current extent %u)", axis, source_arr->shape[axis]);
        goto done;
    }

    uint32_t index = (uint32_t) *p->index;
    if (*p->index < 0 || index >= source_arr->shape[axis]) {
        res = csound->InitError(csound, "[csnarray] Index %d is out of range for axis %u of extent %u (valid: 0..%u)", (int32_t) *p->index, axis, source_arr->shape[axis], source_arr->shape[axis] - 1);
        goto done;
    }

    uint32_t temp_shape[CSN_MAX_DIMS] = {0};
    memcpy(temp_shape, source_arr->shape, sizeof(uint32_t) * (size_t) source_ndim);
    temp_shape[axis]--;

    if (create_csnarray_locked(csound, reg, &p->h, source_ndim, temp_shape, &p->array, p->handle, &source_handle, 1U, &err, source_arr->itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;

    for (size_t linear = 0; linear < arr->size; ++linear) {
        uint32_t dst_coords[CSN_MAX_DIMS] = {0};
        uint32_t src_coords[CSN_MAX_DIMS] = {0};

        from_linear_to_coords(dst_coords, arr->shape, linear, arr->ndim);
        memcpy(src_coords, dst_coords, sizeof(uint32_t) * source_ndim);
        if (dst_coords[axis] >= index) src_coords[axis]++;
        size_t source_off = from_coords_to_offset(src_coords, source_arr->strides, source_arr->ndim);
        if (source_arr->itype == CSN_REAL) {
            arr->data[linear] = source_arr->data[source_off];
        } else {
            arr->data[linear * 2] = source_arr->data[source_off * 2];
            arr->data[linear * 2 + 1] = source_arr->data[source_off * 2 + 1];
        }
    }

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_concat_flat(CSOUND *csound, CSN_CONCAT *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

    uint32_t source_handle = p->source_handle->id;
    uint32_t data_handle = p->data_handle->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
        goto done;
    }

    CSN_SLOT *data_slot = get_slot(reg, data_handle);
    if (data_slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) data_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = source_slot->array;
    CSN_ARRAY *data_arr = data_slot->array;

    if (source_arr->itype != data_arr->itype) {
        res = csound->InitError(csound, "[csnarray] Element type mismatch: one array is real and the other complex");
        goto done;
    }

    uint32_t source_ndim = source_arr->ndim;
    uint32_t data_ndim = data_arr->ndim;

    if (source_ndim != 1U || data_ndim != 1U) {
        res = csound->InitError(csound, "[csnarray] Both arrays must be 1-D, got %u-D and %u-D", source_ndim, data_ndim);
        goto done;
    }

    uint32_t *source_shape = source_arr->shape;
    uint32_t *data_shape = data_arr->shape;

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    new_shape[0] = source_shape[0] + data_shape[0];

    /* Both operands are read by the copy loop below, so both are protected. */
    const uint32_t protect[2] = { source_handle, data_handle };

    if (create_csnarray_locked(csound, reg, &p->h, 1U, new_shape, &p->array, p->handle, protect, 2U, &err, source_arr->itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;

    for (size_t linear = 0; linear < arr->size; ++linear) {
        if (arr->itype == CSN_COMPLEX) {
            size_t src = linear < source_arr->size ? linear : linear - source_arr->size;
            const double *from = linear < source_arr->size ? source_arr->data : data_arr->data;
            arr->data[linear * 2] = from[src * 2];
            arr->data[linear * 2 + 1] = from[src * 2 + 1];
            continue;
        }

        arr->data[linear] = linear < source_arr->size
            ? source_arr->data[linear]
            : data_arr->data[linear - source_arr->size];
    }

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_concat_block(CSOUND *csound, CSN_CONCAT *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

    uint32_t source_handle = p->source_handle->id;
    uint32_t data_handle = p->data_handle->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
        goto done;
    }

    CSN_SLOT *data_slot = get_slot(reg, data_handle);
    if (data_slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) data_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = source_slot->array;
    CSN_ARRAY *data_arr = data_slot->array;

    if (source_arr->itype != data_arr->itype) {
        res = csound->InitError(csound, "[csnarray] Element type mismatch: one array is real and the other complex");
        goto done;
    }

    uint32_t source_ndim = source_arr->ndim;
    uint32_t data_ndim = data_arr->ndim;

    if (data_ndim != source_ndim) {
        res = csound->InitError(csound, "[csnarray] Arrays must have the same number of dimensions, got %u-D and %u-D", source_ndim, data_ndim);
        goto done;
    }

    uint32_t *source_shape = source_arr->shape;
    uint32_t *data_shape = data_arr->shape;

    double axis_value = (double) *p->axis;
    if (!IS_VALID_AXIS(axis_value, source_ndim)) {
        res = csound->InitError(csound, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: finite integers 0..%u)", axis_value, source_ndim, source_ndim - 1);
        goto done;
    }
    uint32_t axis = (uint32_t) axis_value;

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    for (uint32_t i = 0; i < source_ndim; i++) {
        if (i == axis) {
            new_shape[i] = source_shape[i] + data_shape[i];
        } else {
            if (source_shape[i] != data_shape[i]) {
                char sbuf[CSN_SHAPE_STR_MAX], dbuf[CSN_SHAPE_STR_MAX];
                res = csound->InitError(csound, "[csnarray] Shapes %s and %s differ on axis %u (%u vs %u); only the concat axis %u may differ", shape_str(sbuf, sizeof(sbuf), source_shape, source_ndim), shape_str(dbuf, sizeof(dbuf), data_shape, data_ndim), i, source_shape[i], data_shape[i], axis);
                goto done;
            }
            new_shape[i] = source_shape[i];
        }
    }

    /* Both operands are read by the copy loop below, so both are protected. */
    const uint32_t protect[2] = { source_handle, data_handle };

    if (create_csnarray_locked(csound, reg, &p->h, source_ndim, new_shape, &p->array, p->handle, protect, 2U, &err, source_arr->itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;

    for (size_t linear = 0; linear < arr->size; ++linear) {
        uint32_t dst_coords[CSN_MAX_DIMS] = {0};
        uint32_t src_coords[CSN_MAX_DIMS] = {0};

        from_linear_to_coords(dst_coords, arr->shape, linear, arr->ndim);

        if (dst_coords[axis] < source_shape[axis]) {
            memcpy(src_coords, dst_coords, sizeof(uint32_t) * source_ndim);
            size_t source_off = from_coords_to_offset(src_coords, source_arr->strides, source_arr->ndim);
            if (arr->itype == CSN_COMPLEX) {
                arr->data[linear * 2] = source_arr->data[source_off * 2];
                arr->data[linear * 2 + 1] = source_arr->data[source_off * 2 + 1];
            } else {
                arr->data[linear] = source_arr->data[source_off];
            }
        } else {
            memcpy(src_coords, dst_coords, sizeof(uint32_t) * source_ndim);
            src_coords[axis] -= source_shape[axis];
            size_t block_off = from_coords_to_offset(src_coords, data_arr->strides, data_arr->ndim);
            if (arr->itype == CSN_COMPLEX) {
                arr->data[linear * 2] = data_arr->data[block_off * 2];
                arr->data[linear * 2 + 1] = data_arr->data[block_off * 2 + 1];
            } else {
                arr->data[linear] = data_arr->data[block_off];
            }
        }
    }

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_pad_helper(CSOUND *csound, const OPDS *h, uint32_t inocount, CSNREF *ohandle, CSNREF *shandle, double inbefore, double inafter, double value, COMPLEXDAT *valuecomp, ITEM_TYPE expected_itype, double inaxis, CSN_ARRAY **array) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

    uint32_t source_handle = shandle->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = source_slot->array;
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    if (source_arr->itype != expected_itype) {
        if (expected_itype == CSN_COMPLEX) {
            res = csound->InitError(csound, "[csnarray] Handle holds a real array; pad it with a real value");
            goto done;
        }
        res = csound->InitError(csound, "[csnarray] Handle holds a complex array; pad it with a :Complex; value");
        goto done;
    }

    if (inbefore < 0 || inafter < 0) {
        res = csound->InitError(csound, "[csnarray] Pad widths must be >= 0, got before=%g and after=%g", inbefore, inafter);
        goto done;
    }

    uint32_t before = (uint32_t) inbefore;
    uint32_t after = (uint32_t) inafter;

    int32_t axis = -1;
    if (inocount > 4) {
        if (!IS_VALID_AXIS(inaxis, source_ndim)) {
            res = csound->InitError(csound, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: finite integers 0..%u)", inaxis, source_ndim, source_ndim - 1);
            goto done;
        }
        axis = (int32_t) inaxis;
    }

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    for (uint32_t i = 0; i < source_ndim; i++) {
        if (i == (uint32_t) axis || axis == -1) {
            new_shape[i] = source_shape[i] + after + before;
        } else {
            new_shape[i] = source_shape[i];
        }
    }

    const uint32_t protect[1] = { source_handle };

    if (create_csnarray_locked(csound, reg, h, source_ndim, new_shape, array, ohandle, protect, 1U, &err, source_arr->itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = *array;

    COMPLEXDAT *c = NULL;
    double re, im;
    if (valuecomp != NULL) {
        c = valuecomp;
        complexdat_to_rect(c, &re, &im);
    }

    for (size_t linear = 0; linear < arr->size; ++linear) {
        uint32_t dst_coords[CSN_MAX_DIMS] = {0};
        uint32_t src_coords[CSN_MAX_DIMS] = {0};

        from_linear_to_coords(dst_coords, arr->shape, linear, arr->ndim);

        bool is_inside = true;
        for (uint32_t i = 0; i < source_ndim; i++) {
            bool is_padded_axis = axis == -1 || (uint32_t) axis == i;
            if (is_padded_axis) {
                if (dst_coords[i] < before || dst_coords[i] >= before + source_shape[i]) {
                    is_inside = false;
                    break;
                }
                src_coords[i] = dst_coords[i] - before;
            } else {
                src_coords[i] = dst_coords[i];
            }
        }

        if (!is_inside) {
            if (expected_itype == CSN_COMPLEX) {
                arr->data[linear * 2] = re;
                arr->data[linear * 2 + 1] = im;
            } else {
                arr->data[linear] = value;
            }
            continue;
        }

        size_t source_off = from_coords_to_offset(src_coords, source_arr->strides, source_arr->ndim);
        if (expected_itype == CSN_COMPLEX) {
            arr->data[linear * 2] = source_arr->data[source_off * 2];
            arr->data[linear * 2 + 1] = source_arr->data[source_off * 2 + 1];
        } else {
            arr->data[linear] = source_arr->data[source_off];
        }
    }

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_pad(CSOUND *csound, CSN_PAD *p) {
    double axis = p->INOCOUNT > 4 ? (double) *p->axis : -1.0;
    return csnarray_pad_helper(
        csound,
        &p->h,
        p->INOCOUNT,
        p->handle,
        p->source_handle,
        (double) *p->before,
        (double) *p->after,
        (double) *p->value,
        NULL,
        CSN_REAL,
        axis,
        &p->array);
}

int32_t csnarray_padcomp(CSOUND *csound, CSN_PADCOMPLEX *p) {
    double axis = p->INOCOUNT > 4 ? (double) *p->axis : -1.0;
    return csnarray_pad_helper(
        csound,
        &p->h,
        p->INOCOUNT,
        p->handle,
        p->source_handle,
        (double) *p->before,
        (double) *p->after,
        0.0,
        p->value,
        CSN_COMPLEX,
        axis,
        &p->array);
}

int32_t csnarray_pad_in_helper(CSOUND *csound, const OPDS *h, uint32_t inocount, CSNREF *shandle, double inbefore, double inafter, double value, COMPLEXDAT *valuecomp, ITEM_TYPE expected_itype, double inaxis) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

    uint32_t source_handle = shandle->id;

    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = source_slot->array;
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    if (source_arr->itype != expected_itype) {
        if (expected_itype == CSN_COMPLEX) {
            res = csound->InitError(csound, "[csnarray] Handle holds a real array; pad it with a real value");
            goto done;
        }
        res = csound->InitError(csound, "[csnarray] Handle holds a complex array; pad it with a :Complex; value");
        goto done;
    }

    if (inbefore < 0 || inafter < 0) {
        res = csound->InitError(csound, "[csnarray] Pad widths must be >= 0, got before=%g and after=%g", inbefore, inafter);
        goto done;
    }

    uint32_t before = (uint32_t) inbefore;
    uint32_t after = (uint32_t) inafter;

    int32_t axis = -1;
    if (inocount > 4) {
        if (!IS_VALID_AXIS(inaxis, source_ndim)) {
            res = csound->InitError(csound, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: finite integers 0..%u)", inaxis, source_ndim, source_ndim - 1);
            goto done;
        }
        axis = (int32_t) inaxis;
    }

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    for (uint32_t i = 0; i < source_ndim; i++) {
        if (i == (uint32_t) axis || axis == -1) {
            new_shape[i] = source_shape[i] + after + before;
        } else {
            new_shape[i] = source_shape[i];
        }
    }

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

    COMPLEXDAT *c = NULL;
    double re, im;
    if (valuecomp != NULL) {
        c = valuecomp;
        complexdat_to_rect(c, &re, &im);
    }

    for (size_t linear = 0; linear < temp->size; ++linear) {
        uint32_t dst_coords[CSN_MAX_DIMS] = {0};
        uint32_t src_coords[CSN_MAX_DIMS] = {0};

        from_linear_to_coords(dst_coords, temp->shape, linear, temp->ndim);

        bool is_inside = true;
        for (uint32_t i = 0; i < source_ndim; i++) {
            bool is_padded_axis = axis == -1 || (uint32_t) axis == i;
            if (is_padded_axis) {
                if (dst_coords[i] < before || dst_coords[i] >= before + source_shape[i]) {
                    is_inside = false;
                    break;
                }
                src_coords[i] = dst_coords[i] - before;
            } else {
                src_coords[i] = dst_coords[i];
            }
        }

        if (!is_inside) {
            if (expected_itype == CSN_COMPLEX) {
                temp->data[linear * 2] = re;
                temp->data[linear * 2 + 1] = im;
            } else {
                temp->data[linear] = value;
            }
            continue;
        }

        size_t source_off = from_coords_to_offset(src_coords, source_arr->strides, source_arr->ndim);
        if (expected_itype == CSN_COMPLEX) {
            temp->data[linear * 2] = source_arr->data[source_off * 2];
            temp->data[linear * 2 + 1] = source_arr->data[source_off * 2 + 1];
        } else {
            temp->data[linear] = source_arr->data[source_off];
        }
    }

    size_t bytes = sizeof(double) * temp->capacity * temp->itype;
    double *new_data = csound->ReAlloc(csound, source_arr->data, bytes);
    if (new_data == NULL) {
        csound->Free(csound, temp->data);
        csound->Free(csound, temp);
        res = csound->InitError(csound, "[csnarray] Out of memory: allocation of %zu bytes failed", bytes);
        goto done;
    }

    source_arr->data = new_data;
    travase_csnarray(source_arr, temp);
    csound->Free(csound, temp->data);
    csound->Free(csound, temp);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_pad_in(CSOUND *csound, CSN_PAD_IN *p) {
    double axis = p->INOCOUNT > 4 ? (double) *p->axis : -1.0;
    return csnarray_pad_in_helper(
        csound,
        &p->h,
        p->INOCOUNT,
        p->source_handle,
        (double) *p->before,
        (double) *p->after,
        (double) *p->value,
        NULL,
        CSN_REAL,
        axis);
}

int32_t csnarray_padcomp_in(CSOUND *csound, CSN_PADCOMPLEX_IN *p) {
    double axis = p->INOCOUNT > 4 ? (double) *p->axis : -1.0;
    return csnarray_pad_in_helper(
        csound,
        &p->h,
        p->INOCOUNT,
        p->source_handle,
        (double) *p->before,
        (double) *p->after,
        0.0,
        p->value,
        CSN_COMPLEX,
        axis);
}

static void clip_value(double min_value, double max_value, CSN_ARRAY *arr) {
    for (size_t i = 0; i < arr->size; ++i) {
        double value = arr->data[i];
        if (value < min_value) arr->data[i] = min_value;
        if (value > max_value) arr->data[i] = max_value;
    }
}

int32_t csnarray_clip(CSOUND *csound, CSN_CLIP *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

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
    clip_value((double) *p->min, (double) *p->max, p->array);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_clip_in(CSOUND *csound, CSN_CLIP_IN *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

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

    clip_value((double) *p->min, (double) *p->max, source_arr);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

/* The single definition of every comparison. The counting pass and the filling
   pass both go through here, so they cannot disagree about what matches.

   NaN follows IEEE, which is also what numpy reports: the ordered comparisons
   and EQUAL are false against a NaN, NOT_EQUAL is true, and NONZERO treats it
   as nonzero. IS_NAN exists because none of those can single a NaN out. */
static inline bool compare_match(double value, double cmp_value, CSN_COMPARE_MODE mode) {
    switch (mode) {
        case GREATER_THAN:  return value > cmp_value;
        case LESS_THAN:     return value < cmp_value;
        case EQUAL:         return value == cmp_value;
        case GREATER_EQUAL: return value >= cmp_value;
        case LESS_EQUAL:    return value <= cmp_value;
        case NOT_EQUAL:     return value != cmp_value;
        case NONZERO:       return value != 0.0;
        case IS_NAN:        return isnan(value) != 0;
    }
    return false;
}

static size_t count_elements_from_array(const CSN_ARRAY *source_arr, const CSN_ARRAY *data_arr, CSN_COMPARE_MODE mode) {
    size_t count = 0;
    for (size_t linear = 0; linear < source_arr->size; ++linear) {
        double value = source_arr->data[linear];
        for (size_t i = 0; i < data_arr->size; ++i) {
            if (compare_match(value, data_arr->data[i], mode)) {
                count++;
                break;
            }
        }
    }
    return count;
}

static size_t count_elements_from_value(const CSN_ARRAY *source_arr, double cmp_value, CSN_COMPARE_MODE mode) {
    size_t count = 0;
    for (size_t linear = 0; linear < source_arr->size; ++linear) {
        if (compare_match(source_arr->data[linear], cmp_value, mode)) count++;
    }
    return count;
}

int32_t csnarray_argwhere(CSOUND *csound, CSN_ARGWHERE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

    uint32_t source_handle = p->source_handle->id;
    uint32_t data_handle = p->data_handle->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
        goto done;
    }

    CSN_SLOT *data_slot = get_slot(reg, data_handle);
    if (data_slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) data_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = source_slot->array;

    if (source_arr->itype != CSN_REAL) {
        res = csound->InitError(csound, "[csnarray] This operation is not implemented for complex arrays");
        goto done;
    }
    CSN_ARRAY *data_arr = data_slot->array;

    uint32_t source_ndim = source_arr->ndim;
    uint32_t data_ndim = data_arr->ndim;

    if (data_ndim != 1U) {
        res = csound->InitError(csound, "[csnarray] Data array must be 1-D, got %u-D", data_ndim);
        goto done;
    }

    uint32_t *source_shape = source_arr->shape;

    /* No match yields a {0, ndim} array rather than an invalid handle, so the
       result stays usable in a chain. Matches np.argwhere. */
    size_t count = count_elements_from_array(source_arr, data_arr, EQUAL);

    uint32_t new_shape[2] = { (uint32_t) count, source_arr->ndim };

    const uint32_t protect[2] = { source_handle, data_handle };
    if (create_csnarray_locked(csound, reg, &p->h, 2U, new_shape, &p->array, p->handle, protect, 2U, &err, source_arr->itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;

    size_t match = 0;
    for (size_t linear = 0; linear < source_arr->size; ++linear) {
        double value = source_arr->data[linear];
        bool found = false;
        for (size_t i = 0; i < data_arr->size; ++i) {
            if (value == data_arr->data[i]) {
                found = true;
                break;
            }
        }

        if (!found) continue;

        uint32_t src_coords[CSN_MAX_DIMS] = {0};
        from_linear_to_coords(src_coords, source_shape, linear, source_arr->ndim);

        for (size_t j = 0; j < source_ndim; ++j) {
            arr->data[match * source_ndim + j] = (double) src_coords[j];
        }

        match++;
    }

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

/* Shared by csnargnonzero and csnargisnan: both select elements by a predicate
   that needs no comparison value, and both report the coordinates. */
static int32_t csnarray_argselect_helper(CSOUND *csound, CSN_ARGWHERE *p, CSN_COMPARE_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

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
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    size_t count = count_elements_from_value(source_arr, 0.0, mode);
    uint32_t new_shape[2] = { (uint32_t) count, source_arr->ndim };

    const uint32_t protect[1] = { source_handle };
    if (create_csnarray_locked(csound, reg, &p->h, 2U, new_shape, &p->array, p->handle, protect, 1U, &err, source_arr->itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;

    size_t match = 0;
    for (size_t linear = 0; linear < source_arr->size; ++linear) {
        if (compare_match(source_arr->data[linear], 0.0, mode)) {
            uint32_t src_coords[CSN_MAX_DIMS] = {0};
            from_linear_to_coords(src_coords, source_shape, linear, source_arr->ndim);

            for (size_t j = 0; j < source_ndim; ++j) {
                arr->data[match * source_ndim + j] = (double) src_coords[j];
            }

            match++;
        }
    }

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_argnonzero(CSOUND *csound, CSN_ARGWHERE *p) {
    return csnarray_argselect_helper(csound, p, NONZERO);
}

int32_t csnarray_argisnan(CSOUND *csound, CSN_ARGWHERE *p) {
    return csnarray_argselect_helper(csound, p, IS_NAN);
}

static int compare_double_from_array_elem(const void *a, const void *b) {
    ARRAY_ELEMENT x = *(const ARRAY_ELEMENT *) a;
    ARRAY_ELEMENT y = *(const ARRAY_ELEMENT *) b;
    double x_value = x.value;
    double y_value = y.value;
    if (isnan(x_value) && isnan(y_value)) return 0;
    if (isnan(x_value)) return 1;
    if (isnan(y_value)) return -1;
    if (x_value < y_value) return -1;
    if (x_value > y_value) return 1;
    return 0;
}

static size_t count_unique(ARRAY_ELEMENT *temp, size_t size) {
    qsort(temp, size, sizeof(ARRAY_ELEMENT), compare_double_from_array_elem);
    size_t count = 0;
    for (size_t i = 0; i < size; ++i) {
        /* Same equality the sort used, so NaNs collapse to one entry instead
           of surviving as duplicates: `NaN != NaN` would always be true. */
        if (i == 0 || compare_double_from_array_elem(&temp[i], &temp[i - 1]) != 0)
            temp[count++] = temp[i];
    }
    return count;
}

int32_t csnarray_argunique(CSOUND *csound, CSN_ARGWHERE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

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
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    ARRAY_ELEMENT *temp = csound->Calloc(csound, sizeof(ARRAY_ELEMENT) * source_arr->size);
    if (temp == NULL) {
        res = csound->InitError(csound, "[csnarray] Out of memory: allocation of %zu bytes failed", (size_t) (sizeof(ARRAY_ELEMENT) * source_arr->size));
        goto done;
    }

    for (size_t i = 0; i < source_arr->size; i++) {
        temp[i].value = source_arr->data[i];
        temp[i].linear_index = (uint32_t) i;
    }

    /* count == 0 needs no special case: the shape below becomes {0, ndim},
       a legal zero-length array, and the fill loop simply does not run. */
    size_t count = count_unique(temp, source_arr->size);

    uint32_t new_shape[2] = { (uint32_t) count, source_arr->ndim };

    const uint32_t protect[1] = { source_handle };
    if (create_csnarray_locked(csound, reg, &p->h, 2U, new_shape, &p->array, p->handle, protect, 1U, &err, source_arr->itype) != OK) {
        csound->Free(csound, temp);
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;

    for (size_t i = 0; i < count; ++i) {
        uint32_t src_coords[CSN_MAX_DIMS] = {0};

        ARRAY_ELEMENT *elem = &temp[i];
        from_linear_to_coords(src_coords, source_shape, elem->linear_index, source_arr->ndim);

        for (size_t j = 0; j < source_ndim; ++j) {
            arr->data[i * source_ndim + j] = (double) src_coords[j];
        }
    }

    csound->Free(csound, temp);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_unique(CSOUND *csound, CSN_COMPARE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

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

    ARRAY_ELEMENT *temp = csound->Calloc(csound, sizeof(ARRAY_ELEMENT) * source_arr->size);
    if (temp == NULL) {
        res = csound->InitError(csound, "[csnarray] Out of memory: allocation of %zu bytes failed", (size_t) (sizeof(ARRAY_ELEMENT) * source_arr->size));
        goto done;
    }

    for (size_t i = 0; i < source_arr->size; i++) {
        temp[i].value = source_arr->data[i];
        temp[i].linear_index = (uint32_t) i;
    }

    size_t count = count_unique(temp, source_arr->size);
    uint32_t new_shape[1] = { (uint32_t) count };

    const uint32_t protect[1] = { source_handle };
    if (create_csnarray_locked(csound, reg, &p->h, 1U, new_shape, &p->array, p->handle, protect, 1U, &err, source_arr->itype) != OK) {
        csound->Free(csound, temp);
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;

    for (size_t i = 0; i < count; ++i) {
        arr->data[i] = temp[i].value;
    }

    csound->Free(csound, temp);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_compare_helper(CSOUND *csound, CSN_COMPARE *p, CSN_COMPARE_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

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
    double cmp_value = (double) *p->cmp_value;

    // size_t count = count_elements_from_value(source_arr, cmp_value, mode);
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    memcpy(new_shape, source_arr->shape, sizeof(uint32_t) * CSN_MAX_DIMS);

    const uint32_t protect[1] = { source_handle };
    if (create_csnarray_locked(csound, reg, &p->h, source_arr->ndim, new_shape, &p->array, p->handle, protect, 1U, &err, source_arr->itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;

    for (size_t i = 0; i < source_arr->size; ++i) {
        double value = source_arr->data[i];
        if (compare_match(value, cmp_value, mode)) arr->data[i] = 1.0;
    }

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}


int32_t csnarray_greater_than(CSOUND *csound, CSN_COMPARE *p) {
    return csnarray_compare_helper(csound, p, GREATER_THAN);
}

int32_t csnarray_less_than(CSOUND *csound, CSN_COMPARE *p) {
    return csnarray_compare_helper(csound, p, LESS_THAN);
}

int32_t csnarray_not_equal(CSOUND *csound, CSN_COMPARE *p) {
    return csnarray_compare_helper(csound, p, NOT_EQUAL);
}

int32_t csnarray_greater_equal(CSOUND *csound, CSN_COMPARE *p) {
    return csnarray_compare_helper(csound, p, GREATER_EQUAL);
}

int32_t csnarray_less_equal(CSOUND *csound, CSN_COMPARE *p) {
    return csnarray_compare_helper(csound, p, LESS_EQUAL);
}

int32_t csnarray_equal(CSOUND *csound, CSN_COMPARE *p) {
    return csnarray_compare_helper(csound, p, EQUAL);
}

int32_t csnarray_compare_count_helper(CSOUND *csound, CSN_COUNT *p, CSN_COMPARE_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

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

    /* NONZERO and IS_NAN take no comparison value, so their opcodes have no
       such argument to read. */
    size_t count = (mode == NONZERO || mode == IS_NAN)
        ? count_elements_from_value(source_arr, 0.0, mode)
        : count_elements_from_value(source_arr, (double) *p->cmp_value, mode);

    *p->value = (MYFLT) count;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_count_equal(CSOUND *csound, CSN_COUNT *p) {
    return csnarray_compare_count_helper(csound, p, EQUAL);
}

int32_t csnarray_count_nonzero(CSOUND *csound, CSN_COUNT *p) {
    return csnarray_compare_count_helper(csound, p, NONZERO);
}

int32_t csnarray_count_nan(CSOUND *csound, CSN_COUNT *p) {
    return csnarray_compare_count_helper(csound, p, IS_NAN);
}

static void init_value_for_reduction(double *value, CSN_REDUCTION_MODE mode) {
    switch (mode) {
        case RED_SUM:
        case RED_MEAN:
        case RED_SUB:
            *value = 0.0;
            break;
        case RED_PROD:
            *value = 1.0;
            break;
        case RED_MIN:
            *value = DBL_MAX;
            break;
        case RED_MAX:
            *value = -DBL_MAX;
            break;
        case RED_ALL:
            *value = 1.0;
            break;
        case RED_ANY:
            *value = 0.0;
            break;
        default:
            break;
    }
}

/* NaN propagates, as in numpy: sum/prod/mean carry it through the arithmetic
   on their own, min/max need it forced because IEEE comparisons against a NaN
   are all false and would otherwise skip it, and all/any treat it as truthy
   because it is nonzero. idx is the position within the reduction, which the
   subtraction fold uses to seed from the first element. */
static void dispatch_value_for_reduction(double *value, const double x, CSN_REDUCTION_MODE mode, size_t idx) {
    switch (mode) {
        case RED_SUM:
        case RED_MEAN:
            *value += x;
            break;
        case RED_PROD:
            *value *= x;
            break;
        case RED_SUB:
            *value = (idx == 0) ? x : *value - x;
            break;
        case RED_MIN:
            if (isnan(x) || isnan(*value)) *value = NAN;
            else if (x < *value) *value = x;
            break;
        case RED_MAX:
            if (isnan(x) || isnan(*value)) *value = NAN;
            else if (x > *value) *value = x;
            break;
        case RED_ALL:
            if (x == 0.0) *value = 0.0;
            break;
        case RED_ANY:
            if (x != 0.0) *value = 1.0;
            break;
        default:
            break;
    };
}

static void complex_prod(CSN_COMPLEXDAT *out, CSN_COMPLEXDAT a, CSN_COMPLEXDAT b) {
    out->re =  a.re * b.re - a.im * b.im;
    out->im =  a.re * b.im + a.im * b.re;
}

static void complex_scalar_prod(CSN_COMPLEXDAT *out, CSN_COMPLEXDAT a, double b) {
    out->re =  a.re * b;
    out->im =  a.im * b;
}

static void complex_add(CSN_COMPLEXDAT *out, CSN_COMPLEXDAT a, CSN_COMPLEXDAT b) {
    out->re =  a.re + b.re;
    out->im =  a.im + b.im;
}

static void complex_sub(CSN_COMPLEXDAT *out, CSN_COMPLEXDAT a, CSN_COMPLEXDAT b) {
    out->re =  a.re - b.re;
    out->im =  a.im - b.im;
}


static int32_t complex_div(CSN_COMPLEXDAT *out, CSN_COMPLEXDAT a, CSN_COMPLEXDAT b) {
    double den = b.re * b.re + b.im * b.im;
    if (den == 0.0) return NOTOK;
    out->re = (a.re * b.re + a.im * b.im) / den;
    out->im = (a.im * b.re - a.re * b.im) / den;
    return OK;
}

static void complex_log(CSN_COMPLEXDAT *out, CSN_COMPLEXDAT z) {
    double modulus = hypot(z.re, z.im);
    double phase = atan2(z.im, z.re);
    out->re = log(modulus);
    out->im = phase;
}

static void complex_exp(CSN_COMPLEXDAT *out, CSN_COMPLEXDAT z) {
    double exp_re = exp(z.re);
    out->re = exp_re * cos(z.im);
    out->im = exp_re * sin(z.im);
}

static int32_t complex_pow(CSN_COMPLEXDAT *out, CSN_COMPLEXDAT base, CSN_COMPLEXDAT exponent) {
    if (base.re == 0.0 && base.im == 0.0) return NOTOK;

    CSN_COMPLEXDAT l = {0};
    CSN_COMPLEXDAT t = {0};
    complex_log(&l, base);
    complex_prod(&t, exponent, l);
    complex_exp(out, t);
    return OK;
}

static void complex_sqrt(CSN_COMPLEXDAT *out, CSN_COMPLEXDAT z) {
    double r = hypot(z.re, z.im);
    double real = sqrt((r + z.re) * 0.5);
    double imag = sqrt((r - z.re) * 0.5);

    if (z.im < 0.0) imag = -imag;

    out->re = real;
    out->im = imag;
}

static void complex_sign(CSN_COMPLEXDAT *out, CSN_COMPLEXDAT z) {
    double r = hypot(z.re, z.im);

    if (r == 0.0) {
        out->re = 0.0;
        out->im = 0.0;
        return;
    }

    out->re = z.re / r;
    out->im = z.im / r;
}

static void complex_sin(CSN_COMPLEXDAT *out, CSN_COMPLEXDAT z){
    out->re = sin(z.re) * cosh(z.im);
    out->im = cos(z.re) * sinh(z.im);
}

static void complex_cos(CSN_COMPLEXDAT *out, CSN_COMPLEXDAT z){
    out->re = cos(z.re) * cosh(z.im);
    out->im = -sin(z.re) * sinh(z.im);
}

static void complex_tan(CSN_COMPLEXDAT *out, CSN_COMPLEXDAT z){
    CSN_COMPLEXDAT s = {0};
    CSN_COMPLEXDAT c = {0};
    complex_sin(&s, z);
    complex_cos(&c, z);
    complex_div(out, s, c);
}

static void complex_sinh(CSN_COMPLEXDAT *out, CSN_COMPLEXDAT z){
    out->re = sinh(z.re) * cos(z.im);
    out->im = cosh(z.re) * sin(z.im);
}

static void complex_cosh(CSN_COMPLEXDAT *out, CSN_COMPLEXDAT z){
    out->re = cosh(z.re) * cos(z.im);
    out->im = sinh(z.re) * sin(z.im);
}

static void complex_tanh(CSN_COMPLEXDAT *out, CSN_COMPLEXDAT z){
    CSN_COMPLEXDAT s = {0};
    CSN_COMPLEXDAT c = {0};
    complex_sinh(&s, z);
    complex_cosh(&c, z);
    complex_div(out, s, c);
}

static inline void complex_asin(CSN_COMPLEXDAT *out, CSN_COMPLEXDAT z) {
    CSN_COMPLEXDAT z2 = {0};
    CSN_COMPLEXDAT one_minus_z2 = {0};
    CSN_COMPLEXDAT root = {0};
    CSN_COMPLEXDAT iz = {0};
    CSN_COMPLEXDAT inside = {0};
    CSN_COMPLEXDAT l = {0};

    CSN_COMPLEXDAT one = { 1.0, 0.0 };

    complex_prod(&z2, z, z);
    complex_sub(&one_minus_z2, one, z2);
    complex_sqrt(&root, one_minus_z2);

    /* i*z = -Im(z) + i*Re(z) */
    iz.re = -z.im;
    iz.im =  z.re;

    complex_add(&inside, iz, root);
    complex_log(&l, inside);

    /* -i * (a + bi) = b - ai */
    out->re =  l.im;
    out->im = -l.re;
}

static inline void complex_acos(CSN_COMPLEXDAT *out, CSN_COMPLEXDAT z) {
    CSN_COMPLEXDAT a = {0};
    complex_asin(&a, z);

    out->re = M_PI_2 - a.re;
    out->im = -a.im;
}

static inline void complex_atan(CSN_COMPLEXDAT *out, CSN_COMPLEXDAT z) {
    CSN_COMPLEXDAT one = { 1.0, 0.0 };
    CSN_COMPLEXDAT iz = { -z.im, z.re };

    CSN_COMPLEXDAT a = {0};
    CSN_COMPLEXDAT b = {0};
    CSN_COMPLEXDAT la = {0};
    CSN_COMPLEXDAT lb = {0};
    CSN_COMPLEXDAT d = {0};

    complex_add(&a, one, iz); /* 1 + iz */
    complex_sub(&b, one, iz); /* 1 - iz */

    complex_log(&la, a);
    complex_log(&lb, b);

    complex_sub(&d, la, lb);

    /* (-i/2) * (x + iy) = y/2 - i*x/2 */
    out->re =  0.5 * d.im;
    out->im = -0.5 * d.re;
}

static inline void complex_asinh(CSN_COMPLEXDAT *out, CSN_COMPLEXDAT z) {
    CSN_COMPLEXDAT one = { 1.0, 0.0 };
    CSN_COMPLEXDAT z2 = {0};
    CSN_COMPLEXDAT t = {0};
    CSN_COMPLEXDAT root = {0};
    CSN_COMPLEXDAT inside = {0};

    complex_prod(&z2, z, z);
    complex_add(&t, z2, one);
    complex_sqrt(&root, t);
    complex_add(&inside, z, root);
    complex_log(out, inside);
}

static inline void complex_acosh(CSN_COMPLEXDAT *out, CSN_COMPLEXDAT z) {
    CSN_COMPLEXDAT one = {1.0, 0.0};
    CSN_COMPLEXDAT zp1 = {0};
    CSN_COMPLEXDAT zm1 = {0};
    CSN_COMPLEXDAT r1 = {0};
    CSN_COMPLEXDAT r2 = {0};
    CSN_COMPLEXDAT product = {0};
    CSN_COMPLEXDAT inside = {0};

    complex_add(&zp1, z, one);
    complex_sub(&zm1, z, one);

    complex_sqrt(&r1, zp1);
    complex_sqrt(&r2, zm1);

    complex_prod(&product, r1, r2);
    complex_add(&inside, z, product);

    complex_log(out, inside);
}

static inline void complex_atanh(CSN_COMPLEXDAT *out, CSN_COMPLEXDAT z) {
    CSN_COMPLEXDAT one = {1.0, 0.0};
    CSN_COMPLEXDAT a = {0};
    CSN_COMPLEXDAT b = {0};
    CSN_COMPLEXDAT la = {0};
    CSN_COMPLEXDAT lb = {0};
    CSN_COMPLEXDAT d = {0};

    complex_add(&a, one, z);
    complex_sub(&b, one, z);

    complex_log(&la, a);
    complex_log(&lb, b);

    complex_sub(&d, la, lb);

    out->re = 0.5 * d.re;
    out->im = 0.5 * d.im;
}

static void init_value_for_reductioncomp(CSN_COMPLEXDAT *value, CSN_REDUCTION_MODE mode) {
    switch (mode) {
        case RED_PROD:
            value->re = 1.0;
            value->im = 0.0;
            break;
        default:
            value->re = 0.0;
            value->im = 0.0;
            break;
    }
}

static void dispatch_value_for_reductioncomp(CSN_COMPLEXDAT *value, const CSN_COMPLEXDAT x, CSN_REDUCTION_MODE mode, size_t idx) {
    switch (mode) {
        case RED_SUM:
        case RED_MEAN:
            complex_add(value, *value, x);
            break;
        case RED_PROD:
            complex_prod(value, *value, x);
            break;
        case RED_SUB:
            if (idx == 0) {
                value->re = x.re;
                value->im = x.im;
            } else {
                complex_sub(value, *value, x);
            }
            break;
        default:
            break;
    };
}

static void accumulate_reduction_axis_helper(double *value, CSN_ARRAY *out_arr, const CSN_ARRAY *source_arr, uint32_t *src_coords, const uint32_t *dst_coords, CSN_REDUCTION_MODE mode, uint32_t axis) {
    init_value_for_reduction(value, mode);
    for (uint32_t k = 0; k < source_arr->shape[axis]; ++k) {
        for (uint32_t i = 0, j = 0; i < source_arr->ndim; ++i) {
            if (i == axis)
                src_coords[i] = k;
            else
                src_coords[i] = dst_coords[j++];
        }
        size_t off = from_coords_to_offset(src_coords, source_arr->strides, source_arr->ndim);
        dispatch_value_for_reduction(value, source_arr->data[off], mode, k);
    }

    /* The divisor is how many elements were folded, which lives on the source:
       out_arr has one axis fewer, so out_arr->shape[axis] is a different
       extent entirely, and reads past the rank when axis is the last one. */
    (void) out_arr;
    if (mode == RED_MEAN) *value /= (double) source_arr->shape[axis];
}

static void accumulate_reductioncomp_axis_helper(CSN_COMPLEXDAT *c, CSN_ARRAY *out_arr, const CSN_ARRAY *source_arr, uint32_t *src_coords, const uint32_t *dst_coords, CSN_REDUCTION_MODE mode, uint32_t axis) {
    init_value_for_reductioncomp(c, mode);
    for (uint32_t k = 0; k < source_arr->shape[axis]; ++k) {
        for (uint32_t i = 0, j = 0; i < source_arr->ndim; ++i) {
            if (i == axis)
                src_coords[i] = k;
            else
                src_coords[i] = dst_coords[j++];
        }
        size_t off = from_coords_to_offset(src_coords, source_arr->strides, source_arr->ndim);
        CSN_COMPLEXDAT x = { source_arr->data[off * 2], source_arr->data[off * 2 + 1] };
        dispatch_value_for_reductioncomp(c, x, mode, k);
    }

    /* The divisor is how many elements were folded, which lives on the source:
       out_arr has one axis fewer, so out_arr->shape[axis] is a different
       extent entirely, and reads past the rank when axis is the last one. */
    (void) out_arr;
    if (mode == RED_MEAN) {
        CSN_COMPLEXDAT den = { (double) source_arr->shape[axis], 0.0 };
        complex_div(c, *c, den);
    }
}

static void accumulate_reduction_scalar_helper(double *value, const CSN_ARRAY *source_arr, CSN_REDUCTION_MODE mode) {
    init_value_for_reduction(value, mode);
    for (size_t i = 0; i < source_arr->size; i++) {
        dispatch_value_for_reduction(value, source_arr->data[i], mode, i);
    }

    if (mode == RED_MEAN) *value /= (double) source_arr->size;
}

static void accumulate_reductioncomp_scalar_helper(CSN_COMPLEXDAT *value, const CSN_ARRAY *source_arr, CSN_REDUCTION_MODE mode) {
    init_value_for_reductioncomp(value, mode);
    for (size_t i = 0; i < source_arr->size; i++) {
        CSN_COMPLEXDAT x = { source_arr->data[i * 2], source_arr->data[i * 2 + 1] };
        dispatch_value_for_reductioncomp(value, x, mode, i);
    }

    if (mode == RED_MEAN) {
        CSN_COMPLEXDAT den = { (double) source_arr->size, 0.0 };
        complex_div(value, *value, den);
    }
}

/* axis == -1 collapses to out_value; any other axis builds an array through
   out_handle/out_array. Exactly one pair is non-NULL, which is what keeps the
   two opcode families distinct at the type level. */
static int32_t csnarray_accumulate_reduction(CSOUND *csound, const OPDS *h, CSNREF *src_ref, double axis_value, CSNREF *out_handle, CSN_ARRAY **out_array, MYFLT *out_value, COMPLEXDAT *out_complex_value, CSN_REDUCTION_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

    uint32_t source_handle = src_ref->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = source_slot->array;
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    /* The public axis overload returns an array and therefore needs one real
       source axis.  -1 is only the internal marker used by the scalar
       overload, which has no axis argument. */
    if (out_handle != NULL && !IS_VALID_AXIS(axis_value, source_ndim)) {
        res = csound->InitError(csound, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: finite integers 0..%u)", axis_value, source_ndim, source_ndim - 1);
        goto done;
    }
    int32_t axis = (int32_t) axis_value;

    if (mode == RED_MIN || mode == RED_MAX || mode == RED_MEDIAN || mode == RED_ARGMIN || mode == RED_ARGMAX) {
        if (source_arr->itype == CSN_COMPLEX) {
            res = csound->InitError(csound, "[csnarray] Ordering is undefined for complex arrays, so this reduction is not available");
            goto done;
        }
    }

    if (axis == -1) {
        if (source_arr->itype == CSN_COMPLEX && out_complex_value == NULL) {
            res = csound->InitError(csound, "[csnarray] Handle holds a complex array; declare the result as :Complex;");
            goto done;
        }
        if (source_arr->itype == CSN_REAL && out_value == NULL) {
            res = csound->InitError(csound, "[csnarray] Handle holds a real array; declare the result as i");
            goto done;
        }
    }

    /* min, max and the subtraction fold have no identity element to fall back
       on, so an empty reduction has no answer. numpy raises here too. */
    size_t reduced_extent = (axis == -1) ? source_arr->size : source_shape[axis];
    if (reduced_extent == 0 && (mode == RED_MIN || mode == RED_MAX || mode == RED_SUB)) {
        res = csound->InitError(csound, "[csnarray] Min, max and sub are undefined over an empty extent (axis %d has 0 elements)", axis);
        goto done;
    }

    CSN_ARRAY *arr = NULL;
    if (axis != -1) {
        uint32_t new_shape[CSN_MAX_DIMS] = {0};
        for (uint32_t i = 0, j = 0; i < source_ndim; i++) {
            if (i != (uint32_t) axis) new_shape[j++] = source_shape[i];
        }

        const uint32_t protect[1] = { source_handle };

        if (create_csnarray_locked(csound, reg, h, source_ndim - 1, new_shape, out_array, out_handle, protect, 1U, &err, source_arr->itype) != OK) {
            res = csound->InitError(csound, "[csnarray] %s", err);
            goto done;
        }

        arr = *out_array;
    }

    if (arr != NULL) {
        for (size_t linear = 0; linear < arr->size; ++linear) {
            uint32_t dst_coords[CSN_MAX_DIMS] = {0};
            uint32_t src_coords[CSN_MAX_DIMS] = {0};

            from_linear_to_coords(dst_coords, arr->shape, linear, arr->ndim);
            if (source_arr->itype == CSN_REAL) {
                double value = 0.0;
                accumulate_reduction_axis_helper(&value, arr, source_arr, src_coords, dst_coords, mode, axis);
                arr->data[linear] = value;
            } else {
                CSN_COMPLEXDAT c = { 0.0, 0.0 };
                accumulate_reductioncomp_axis_helper(&c, arr, source_arr, src_coords, dst_coords, mode, axis);
                arr->data[linear * 2] = c.re;
                arr->data[linear * 2 + 1] = c.im;
            }
        }
    } else {
        if (source_arr->itype == CSN_REAL) {
            double value = 0;
            accumulate_reduction_scalar_helper(&value, source_arr, mode);
            *out_value = (MYFLT) value;
        } else {
            CSN_COMPLEXDAT c = { 0.0, 0.0 };
            accumulate_reductioncomp_scalar_helper(&c, source_arr, mode);
            out_complex_value->real = c.re;
            out_complex_value->imag = c.im;
            out_complex_value->isPolar = 0;
        }
    }

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_sum(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, (double) *p->axis, p->handle, &p->array, NULL, NULL, RED_SUM);
}

int32_t csnarray_sum_all(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, NULL, RED_SUM);
}

int32_t csnarray_sumcomp_all(CSOUND *csound, CSN_REDUCTION_COMPLEX_S *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, -1, NULL, NULL, NULL, p->value, RED_SUM);
}

int32_t csnarray_prod(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, (double) *p->axis, p->handle, &p->array, NULL, NULL, RED_PROD);
}

int32_t csnarray_prod_all(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, NULL, RED_PROD);
}

int32_t csnarray_prodcomp_all(CSOUND *csound, CSN_REDUCTION_COMPLEX_S *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, -1, NULL, NULL, NULL, p->value, RED_PROD);
}

int32_t csnarray_sub(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, (double) *p->axis, p->handle, &p->array, NULL, NULL, RED_SUB);
}

int32_t csnarray_sub_all(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, NULL, RED_SUB);
}

int32_t csnarray_subcomp_all(CSOUND *csound, CSN_REDUCTION_COMPLEX_S *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, -1, NULL, NULL, NULL, p->value, RED_SUB);
}

int32_t csnarray_mean(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, (double) *p->axis, p->handle, &p->array, NULL, NULL, RED_MEAN);
}

int32_t csnarray_mean_all(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, NULL, RED_MEAN);
}

int32_t csnarray_meancomp_all(CSOUND *csound, CSN_REDUCTION_COMPLEX_S *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, -1, NULL, NULL, NULL, p->value, RED_MEAN);
}

int32_t csnarray_min(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, (double) *p->axis, p->handle, &p->array, NULL, NULL, RED_MIN);
}

int32_t csnarray_min_all(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, NULL, RED_MIN);
}


int32_t csnarray_max(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, (double) *p->axis, p->handle, &p->array, NULL, NULL, RED_MAX);
}

int32_t csnarray_max_all(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, NULL, RED_MAX);
}


int32_t csnarray_all(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, (double) *p->axis, p->handle, &p->array, NULL, NULL, RED_ALL);
}

int32_t csnarray_all_all(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, NULL, RED_ALL);
}

int32_t csnarray_any(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, (double) *p->axis, p->handle, &p->array, NULL, NULL, RED_ANY);
}

int32_t csnarray_any_all(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, NULL, RED_ANY);
}


static int compare_double(const void *a, const void *b) {
    double x_value = *(const double *) a;
    double y_value = *(const double *) b;
    if (isnan(x_value) && isnan(y_value)) return 0;
    if (isnan(x_value)) return 1;
    if (isnan(y_value)) return -1;
    if (x_value < y_value) return -1;
    if (x_value > y_value) return 1;
    return 0;
}

// Welford algo
static int32_t stdvar_calculation_helper(double *value, uint32_t *src_coords, const uint32_t *dst_coords, const CSN_ARRAY *source_arr, uint32_t size, uint32_t axis, CSN_REDUCTION_MODE mode) {
    double mean = 0.0;
    CSN_COMPLEXDAT meancomp = { 0.0, 0.0 };
    double m_two = 0.0;
    for (uint32_t k = 0; k < size; ++k) {
        /* Place this slice's coordinates: k along the reduced axis, and the
           destination's coordinates across the axes that survive. Without this
           every output element would reduce the slice at the origin. */
        for (uint32_t i = 0, j = 0; i < source_arr->ndim; ++i) {
            src_coords[i] = (i == axis) ? k : dst_coords[j++];
        }

        size_t off = from_coords_to_offset(src_coords, source_arr->strides, source_arr->ndim);
        if (source_arr->itype == CSN_REAL) {
            double x = source_arr->data[off];
            double delta = x - mean;
            /* Welford divides by the running count, not the total. */
            mean += delta / (double) (k + 1);
            double delta_two = x - mean;
            m_two += delta * delta_two;
        } else {
            CSN_COMPLEXDAT c = { source_arr->data[off * 2], source_arr->data[off * 2 + 1] };
            CSN_COMPLEXDAT delta = {0};
            complex_sub(&delta, c, meancomp);
            double fac = (double) (k + 1);
            CSN_COMPLEXDAT delta_div = { delta.re / fac, delta.im / fac };
            complex_add(&meancomp, meancomp, delta_div);
            CSN_COMPLEXDAT delta_two = {0};
            complex_sub(&delta_two, c, meancomp);
            m_two += delta.re * delta_two.re + delta.im * delta_two.im;
        }
    }

    if (size == 0) return NOTOK;

    double var = m_two / (double) size;
    switch (mode) {
        case RED_VAR:
            *value = var;
            break;
        case RED_STD:
            *value = sqrt(var);
            break;
        default:
            break;
    }

    return OK;
}

static int32_t stdvar_calculation_scalar_helper(double *value, const CSN_ARRAY *source_arr, uint32_t size, CSN_REDUCTION_MODE mode) {
    double mean = 0.0;
    CSN_COMPLEXDAT meancomp = { 0.0, 0.0 };
    double m_two = 0.0;
    for (uint32_t k = 0; k < size; ++k) {
        if (source_arr->itype == CSN_REAL) {
            double x = source_arr->data[k];
            double delta = x - mean;
            /* Welford divides by the running count, not the total. */
            mean += delta / (double) (k + 1);
            double delta_two = x - mean;
            m_two += delta * delta_two;
        } else {
            CSN_COMPLEXDAT c = { source_arr->data[k * 2], source_arr->data[k * 2 + 1] };
            CSN_COMPLEXDAT delta = {0};
            complex_sub(&delta, c, meancomp);
            double fac = (double) (k + 1);
            CSN_COMPLEXDAT delta_div = { delta.re / fac, delta.im / fac };
            complex_add(&meancomp, meancomp, delta_div);
            CSN_COMPLEXDAT delta_two = {0};
            complex_sub(&delta_two, c, meancomp);
            m_two += delta.re * delta_two.re + delta.im * delta_two.im;
        }
    }

    if (size == 0) return NOTOK;

    double var = m_two / (double) size;
    switch (mode) {
        case RED_VAR:
            *value = var;
            break;
        case RED_STD:
            *value = sqrt(var);
            break;
        default:
            break;
    }

    return OK;
}

static int32_t csnarray_stdvar_helper(CSOUND *csound, const OPDS *h, CSNREF *src_ref, double axis_value, CSNREF *out_handle, CSN_ARRAY **out_array, MYFLT *out_value, CSN_REDUCTION_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

    uint32_t source_handle = src_ref->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = source_slot->array;
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    if (out_handle != NULL && !IS_VALID_AXIS(axis_value, source_ndim)) {
        res = csound->InitError(csound, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: finite integers 0..%u)", axis_value, source_ndim, source_ndim - 1);
        goto done;
    }
    int32_t axis = (int32_t) axis_value;

    CSN_ARRAY *arr = NULL;
    if (axis != -1) {
        uint32_t new_shape[CSN_MAX_DIMS] = {0};
        for (uint32_t i = 0, j = 0; i < source_ndim; i++) {
            if (i != (uint32_t) axis) new_shape[j++] = source_shape[i];
        }

        const uint32_t protect[1] = { source_handle };

        /* La varianza di dati complessi e' E[|z - media|^2], quindi reale anche
           quando l'ingresso non lo e': l'uscita va creata REAL, altrimenti si
           dichiara complessa e i valori restano scritti a passo reale. */
        if (create_csnarray_locked(csound, reg, h, source_ndim - 1, new_shape, out_array, out_handle, protect, 1U, &err, CSN_REAL) != OK) {
            res = csound->InitError(csound, "[csnarray] %s", err);
            goto done;
        }

        arr = *out_array;
    }

    if (arr != NULL) {
        for (size_t linear = 0; linear < arr->size; ++linear) {
            uint32_t dst_coords[CSN_MAX_DIMS] = {0};
            uint32_t src_coords[CSN_MAX_DIMS] = {0};

            from_linear_to_coords(dst_coords, arr->shape, linear, arr->ndim);
            uint32_t axis_size = source_arr->shape[axis];
            double value = 0.0;
            /* NOTOK vuol dire estensione vuota: senza propagarlo il valore
               resterebbe 0, cioe' una risposta finta a una varianza indefinita. */
            if (stdvar_calculation_helper(&value, src_coords, dst_coords, source_arr, axis_size, axis, mode) != OK) {
                res = csound->InitError(csound, "[csnarray] Variance is undefined over an empty extent (axis %d has 0 elements)", axis);
                goto done;
            }
            arr->data[linear] = value;
        }
    } else {
        size_t size = source_arr->size;
        double value = 0;
        if (stdvar_calculation_scalar_helper(&value, source_arr, (uint32_t) size, mode) != OK) {
            res = csound->InitError(csound, "[csnarray] Variance is undefined over an empty array (0 elements)");
            goto done;
        }
        *out_value = (MYFLT) value;
    }

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_std(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_stdvar_helper(csound, &p->h, p->source_handle, (double) *p->axis, p->handle, &p->array, NULL, RED_STD);
}

int32_t csnarray_std_all(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_stdvar_helper(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, RED_STD);
}


int32_t csnarray_var(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_stdvar_helper(csound, &p->h, p->source_handle, (double) *p->axis, p->handle, &p->array, NULL, RED_VAR);
}

int32_t csnarray_var_all(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_stdvar_helper(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, RED_VAR);
}


/* True when x should displace the incumbent. A NaN wins outright and, once
   seen, nothing displaces it: numpy reports the position of the first NaN. */
static inline bool argminmax_better(double x, double best, bool best_is_nan, CSN_REDUCTION_MODE mode) {
    if (best_is_nan) return false;
    if (isnan(x)) return true;
    return (mode == RED_ARGMIN) ? (x < best) : (x > best);
}

static void dispatch_argminmax(const CSN_ARRAY *source_arr, uint32_t axis, uint32_t *src_coords, const uint32_t *dst_coords, CSN_REDUCTION_MODE mode) {
    for (uint32_t i = 0, j = 0; i < source_arr->ndim; i++) {
        if (i == axis) continue;
        src_coords[i] = dst_coords[j++];
    }

    uint32_t best_index = 0;
    double best_value = (mode == RED_ARGMIN) ? DBL_MAX : -DBL_MAX;
    bool best_is_nan = false;

    for (uint32_t k = 0; k < source_arr->shape[axis]; ++k) {
        src_coords[axis] = k;
        size_t off = from_coords_to_offset(src_coords, source_arr->strides, source_arr->ndim);
        double x = source_arr->data[off];
        if (argminmax_better(x, best_value, best_is_nan, mode)) {
            best_value = x;
            best_is_nan = isnan(x) != 0;
            best_index = k;
        }
    }
    src_coords[axis] = best_index;
}

static void dispatch_argminmax_all_axes(const CSN_ARRAY *source_arr, uint32_t *src_coords, CSN_REDUCTION_MODE mode) {
    uint32_t best_index = 0;
    double best_value = (mode == RED_ARGMIN) ? DBL_MAX : -DBL_MAX;

    bool best_is_nan = false;
    for (size_t linear = 0; linear < source_arr->size; ++linear) {
        double x = source_arr->data[linear];
        if (argminmax_better(x, best_value, best_is_nan, mode)) {
            best_value = x;
            best_is_nan = isnan(x) != 0;
            best_index = (uint32_t) linear;
        }
    }

    from_linear_to_coords(src_coords, source_arr->shape, best_index, source_arr->ndim);
}

static int32_t argminmax_helper(CSOUND *csound, CSN_REDUCTION *p, CSN_REDUCTION_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

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
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    if (source_arr->itype == CSN_COMPLEX) {
        res = csound->InitError(csound, "[csnarray] argmin/argmax not allowed for complex array");
        goto done;
    }

    double axis_value = (double) *p->axis;
    if (axis_value != -1.0 && !IS_VALID_AXIS(axis_value, source_ndim)) {
        res = csound->InitError(csound, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: -1 for all axes, or finite integers 0..%u)", axis_value, source_ndim, source_ndim - 1);
        goto done;
    }
    int32_t axis = (int32_t) axis_value;

    /* The shape of the space being reduced over: the source minus the axis.
       Destination coordinates are decomposed against this, not against the
       result's own (count, ndim) layout. */
    size_t count = 1;
    uint32_t reduced_shape[CSN_MAX_DIMS] = {0};
    uint32_t reduced_ndim = 0;
    if (axis != -1) {
        for (uint32_t i = 0; i < source_ndim; i++) {
            if (i != (uint32_t) axis) {
                reduced_shape[reduced_ndim++] = source_shape[i];
                count *= source_shape[i];
            }
        }
    }

    /* One row of coordinates per reduced position, so the result is always
       2-D regardless of the source's rank. */
    uint32_t new_shape[2] = { (uint32_t) count, source_ndim };
    const uint32_t protect[1] = { source_handle };

    if (create_csnarray_locked(csound, reg, &p->h, 2U, new_shape, &p->array, p->handle, protect, 1U, &err, source_arr->itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;

    if (axis != -1) {
        for (size_t linear = 0; linear < count; ++linear) {
            uint32_t dst_coords[CSN_MAX_DIMS] = {0};
            uint32_t src_coords[CSN_MAX_DIMS] = {0};

            from_linear_to_coords(dst_coords, reduced_shape, linear, reduced_ndim);
            dispatch_argminmax(source_arr, axis, src_coords, dst_coords, mode);
            for (uint32_t d = 0; d < source_ndim; ++d) {
                arr->data[linear * source_ndim + d] = (double) src_coords[d];
            }
        }
    } else {
        uint32_t src_coords[CSN_MAX_DIMS] = {0};
        dispatch_argminmax_all_axes(source_arr, src_coords, mode);
        for (uint32_t d = 0; d < source_ndim; ++d) {
            arr->data[d] = (double) src_coords[d];
        }
    }

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

/* Sorts scratch in place and returns the median. A NaN anywhere makes the
   result NaN, as in numpy, and the early return also spares compare_double
   from having to order NaNs. */
static double median_of_scratch(double *scratch, size_t n) {
    if (n == 0) return NAN;

    for (size_t i = 0; i < n; ++i) {
        if (isnan(scratch[i])) return NAN;
    }

    qsort(scratch, n, sizeof(double), compare_double);

    if (n % 2 == 1) return scratch[n / 2];
    return 0.5 * (scratch[n / 2 - 1] + scratch[n / 2]);
}

static int32_t csnarray_median_impl(CSOUND *csound, const OPDS *h, CSNREF *src_ref, double axis_value, CSNREF *out_handle, CSN_ARRAY **out_array, MYFLT *out_value) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

    uint32_t source_handle = src_ref->id;

    int32_t res = OK;
    const char *err = NULL;
    double *scratch = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = source_slot->array;
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    if (source_arr->itype == CSN_COMPLEX) {
        res = csound->InitError(csound, "[csnarray] Median not allowed for complex array");
        goto done;
    }

    if (out_handle != NULL && !IS_VALID_AXIS(axis_value, source_ndim)) {
        res = csound->InitError(csound, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: finite integers 0..%u)", axis_value, source_ndim, source_ndim - 1);
        goto done;
    }
    int32_t axis = (int32_t) axis_value;

    /* Median needs a sorted copy, so it cannot stream like the folds do. */
    size_t run = (axis == -1) ? source_arr->size : source_shape[axis];
    scratch = csound->Calloc(csound, sizeof(double) * (run > 0 ? run : 1));
    if (scratch == NULL) {
        res = csound->InitError(csound, "[csnarray] Out of memory: allocation of %zu bytes failed", (size_t) (sizeof(double) * (run > 0 ? run : 1)));
        goto done;
    }

    if (axis == -1) {
        memcpy(scratch, source_arr->data, sizeof(double) * run);
        *out_value = (MYFLT) median_of_scratch(scratch, run);
        goto done;
    }

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    for (uint32_t i = 0, j = 0; i < source_ndim; i++) {
        if (i != (uint32_t) axis) new_shape[j++] = source_shape[i];
    }

    const uint32_t protect[1] = { source_handle };
    if (create_csnarray_locked(csound, reg, h, source_ndim - 1, new_shape, out_array, out_handle, protect, 1U, &err, source_arr->itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = *out_array;

    for (size_t linear = 0; linear < arr->size; ++linear) {
        uint32_t dst_coords[CSN_MAX_DIMS] = {0};
        uint32_t src_coords[CSN_MAX_DIMS] = {0};

        from_linear_to_coords(dst_coords, arr->shape, linear, arr->ndim);

        for (uint32_t k = 0; k < run; ++k) {
            for (uint32_t i = 0, j = 0; i < source_ndim; ++i) {
                src_coords[i] = (i == (uint32_t) axis) ? k : dst_coords[j++];
            }
            uint32_t off = from_coords_to_offset(src_coords, source_arr->strides, source_ndim);
            scratch[k] = source_arr->data[off];
        }

        arr->data[linear] = median_of_scratch(scratch, run);
    }

done:
    csound->UnlockMutex(reg->mutex);
    if (scratch != NULL) {
        csound->Free(csound, scratch);
    }
    return res;
}

int32_t csnarray_median(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_median_impl(csound, &p->h, p->source_handle, (double) *p->axis, p->handle, &p->array, NULL);
}

int32_t csnarray_median_all(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_median_impl(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value);
}

int32_t csnarray_argmin(CSOUND *csound, CSN_REDUCTION *p) {
    return argminmax_helper(csound, p, RED_ARGMIN);
}

int32_t csnarray_argmax(CSOUND *csound, CSN_REDUCTION *p) {
    return argminmax_helper(csound, p, RED_ARGMAX);
}

/* numpy's broadcasting rule: line the shapes up from the trailing axis, and
   accept a pair of extents when they match or when one of them is 1. Axes an
   operand does not have count as 1, so (2,3) pairs with (3,) but not with (2,).
   The result takes the larger extent on every axis. */
static int32_t broadcast_shape(const CSN_ARRAY *a, const CSN_ARRAY *b,  uint32_t *out_shape, uint32_t *out_ndim) {
    uint32_t n = (a->ndim > b->ndim) ? a->ndim : b->ndim;
    if (n > CSN_MAX_DIMS) {
        return NOTOK;
    }

    for (uint32_t i = 0; i < n; i++) {
        uint32_t ea = (i < a->ndim) ? a->shape[a->ndim - 1 - i] : 1;
        uint32_t eb = (i < b->ndim) ? b->shape[b->ndim - 1 - i] : 1;

        if (ea != eb && ea != 1 && eb != 1) {
            return NOTOK;
        }

        out_shape[n - 1 - i] = (ea > eb) ? ea : eb;
    }

    *out_ndim = n;
    return OK;
}

/* Where a destination coordinate reads from inside one operand. An axis of
   extent 1 is stretched, so it always reads index 0; leading axes the operand
   lacks are skipped. */
static size_t broadcast_offset(const CSN_ARRAY *arr, const uint32_t *dst_coords, uint32_t out_ndim) {
    size_t off = 0;
    uint32_t lead = out_ndim - arr->ndim;

    for (uint32_t i = 0; i < arr->ndim; i++) {
        uint32_t c = (arr->shape[i] == 1) ? 0 : dst_coords[lead + i];
        off += (size_t) c * arr->strides[i];
    }

    return off;
}

static int32_t csnarray_binop_hh_helper(CSOUND *csound, CSN_BINOP_HH *p, CSN_BINOP_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

    uint32_t source_handle_a = p->source_handle_a->id;
    uint32_t source_handle_b = p->source_handle_b->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot_a = get_slot(reg, source_handle_a);
    if (source_slot_a == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle_a);
        goto done;
    }

    CSN_SLOT *source_slot_b = get_slot(reg, source_handle_b);
    if (source_slot_b == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle_b);
        goto done;
    }

    CSN_ARRAY *source_arr_a = source_slot_a->array;
    CSN_ARRAY *source_arr_b = source_slot_b->array;

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    uint32_t new_ndim = 0;
    if (broadcast_shape(source_arr_a, source_arr_b, new_shape, &new_ndim) != OK) {
        char abuf[CSN_SHAPE_STR_MAX], bbuf[CSN_SHAPE_STR_MAX];
        res = csound->InitError(csound, "[csnarray] Shapes %s and %s cannot be broadcast together: aligned from the last axis, each pair must match or be 1", shape_str(abuf, sizeof(abuf), source_arr_a->shape, source_arr_a->ndim), shape_str(bbuf, sizeof(bbuf), source_arr_b->shape, source_arr_b->ndim));
        goto done;
    }

    bool is_logic = (mode == CSN_LOGICAL_AND_HH || mode == CSN_LOGICAL_OR_HH);
    if (is_logic) {
        if (source_arr_a->itype == CSN_COMPLEX || source_arr_b->itype == CSN_COMPLEX) {
            res = csound->InitError(csound, "[csnarray] Logical and and or supports real array only");
            goto done;
        }
    }

    bool type_mode = source_arr_a->itype == CSN_COMPLEX || source_arr_b->itype == CSN_COMPLEX;
    ITEM_TYPE itype = type_mode ? CSN_COMPLEX : CSN_REAL;

    const uint32_t protect[2] = { source_handle_a, source_handle_b };
    if (create_csnarray_locked(csound, reg, &p->h, new_ndim, new_shape, &p->array, p->handle, protect, 2U, &err, itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;

    /* Identical shapes are the common case and need no coordinate mapping, so
       they keep the flat walk over both buffers. */
    bool same_shape = source_arr_a->ndim == source_arr_b->ndim
        && memcmp(source_arr_a->shape, source_arr_b->shape, sizeof(uint32_t) * source_arr_a->ndim) == 0;

    for (size_t i = 0; i < arr->size; i++) {
        size_t off_a = i;
        size_t off_b = i;

        if (!same_shape) {
            uint32_t dst_coords[CSN_MAX_DIMS] = {0};
            from_linear_to_coords(dst_coords, arr->shape, i, arr->ndim);
            off_a = broadcast_offset(source_arr_a, dst_coords, arr->ndim);
            off_b = broadcast_offset(source_arr_b, dst_coords, arr->ndim);
        }

        double a = source_arr_a->data[off_a];
        double b = source_arr_b->data[off_b];
        CSN_COMPLEXDAT ca = {0};
        CSN_COMPLEXDAT cb = {0};
        CSN_COMPLEXDAT c = { 0.0, 0.0 };
        if (itype == CSN_COMPLEX) {
            if (source_arr_a->itype == CSN_COMPLEX) {
                ca.re = source_arr_a->data[off_a * 2];
                ca.im = source_arr_a->data[off_a * 2 + 1];
            } else {
                ca.re = a;
                ca.im = 0.0;
            }
            if (source_arr_b->itype == CSN_COMPLEX) {
                cb.re = source_arr_b->data[off_b * 2];
                cb.im = source_arr_b->data[off_b * 2 + 1];
            } else {
                cb.re = b;
                cb.im = 0.0;
            }
        }

        switch (mode) {
            case CSN_ADD_HH:
                if (itype == CSN_REAL) {
                    arr->data[i] = a + b;
                } else {
                    complex_add(&c, ca, cb);
                    arr->data[i * 2] = c.re;
                    arr->data[i * 2 + 1] = c.im;
                }
                break;
            case CSN_SUB_HH:
                if (itype == CSN_REAL) {
                    arr->data[i] = a - b;
                } else {
                    complex_sub(&c, ca, cb);
                    arr->data[i * 2] = c.re;
                    arr->data[i * 2 + 1] = c.im;
                }
                break;
            case CSN_MUL_HH:
                if (itype == CSN_REAL) {
                    arr->data[i] = a * b;
                } else {
                    if (source_arr_b->itype == CSN_REAL) {
                        complex_scalar_prod(&c, ca, b);
                    } else if (source_arr_a->itype == CSN_REAL) {
                        complex_scalar_prod(&c, cb, a);
                    } else {
                        complex_prod(&c, ca, cb);
                    }
                    arr->data[i * 2] = c.re;
                    arr->data[i * 2 + 1] = c.im;
                }
                break;
            /* IEEE, as numpy: x/0 is an infinity and 0/0 is NaN. */
            case CSN_DIV_HH:
                if (itype == CSN_REAL) {
                    if (b == 0.0) {
                        res = csound->InitError(csound, "[csnarray] Division by zero");
                        goto done;
                    }
                    arr->data[i] = a / b;
                } else {
                    if (complex_div(&c, ca, cb) != OK) {
                        res = csound->InitError(csound, "[csnarray] Division by zero");
                        goto done;
                    };
                    arr->data[i * 2] = c.re;
                    arr->data[i * 2 + 1] = c.im;
                }
                break;
            case CSN_POW_HH:
                if (itype == CSN_REAL) {
                    arr->data[i] = pow(a, b);
                } else {
                    if (complex_pow(&c, ca, cb) != OK) {
                        res = csound->InitError(csound, "[csnarray] Division by zero");
                        goto done;
                    };
                    arr->data[i * 2] = c.re;
                    arr->data[i * 2 + 1] = c.im;
                }
                break;
            case CSN_LOG_HH:
                /* Change of base elementwise: log of a in base b. */
                if (itype == CSN_REAL) {
                    arr->data[i] = log(a) / log(b);
                } else {
                    CSN_COMPLEXDAT log_a = {0};
                    CSN_COMPLEXDAT log_b = {0};
                    complex_log(&log_a, ca);
                    complex_log(&log_b, cb);
                    if (complex_div(&c, log_a, log_b) != OK) {
                        res = csound->InitError(csound, "[csnarray] Division by zero");
                        goto done;
                    }
                    arr->data[i * 2] = c.re;
                    arr->data[i * 2 + 1] = c.im;
                }
                break;
            case CSN_LOGICAL_AND_HH:
                arr->data[i] = (double) ((a != 0.0) && (b != 0.0));
                break;
            case CSN_LOGICAL_OR_HH:
                arr->data[i] = (double) ((a != 0.0) || (b != 0.0));
                break;
            default:
                break;
        }
    }

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

/* The handle and the scalar are passed explicitly rather than read off a
   cast struct: CSN_BINOP_SH lists them in the opposite order to CSN_BINOP_HS,
   so a single cast would silently swap them. */
static int32_t csnarray_binop_hs_sh_helper(CSOUND *csound, CSN_BINOP_COMMON *p, CSNREF *handle_arg, MYFLT *scalar_arg, COMPLEXDAT *complex_arg, CSN_BINOP_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

    uint32_t source_handle = handle_arg->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = source_slot->array;

    if ((mode == CSN_LOGICAL_AND_HS || mode == CSN_LOGICAL_OR_HS) && source_arr->itype == CSN_COMPLEX) {
        res = csound->InitError(csound, "[csnarray] Logical and/or supports real array only");
        goto done;
    }

    if (complex_arg != NULL && (mode == CSN_LOGICAL_AND_HS || mode == CSN_LOGICAL_OR_HS)) {
        res = csound->InitError(csound, "[csnarray] Logical and/or supports real scalar only");
        goto done;
    }

    if (complex_arg != NULL && source_arr->itype != CSN_COMPLEX) {
        res = csound->InitError(csound, "[csnarray] Handle holds a real array; use a real scalar instead of a :Complex; one");
        goto done;
    }

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    memcpy(new_shape, source_arr->shape, sizeof(uint32_t) * CSN_MAX_DIMS);

    const uint32_t protect[1] = { source_handle };
    if (create_csnarray_locked(csound, reg, &p->h, source_arr->ndim, new_shape, &p->array, p->handle, protect, 1U, &err, source_arr->itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;
    double real_scalar = 0.0;
    if (scalar_arg != NULL) {
        real_scalar = (double) *scalar_arg;
    }

    CSN_COMPLEXDAT complex_scalar = { 0.0, 0.0 };
    if (complex_arg != NULL) {
        double re, im;
        complexdat_to_rect(complex_arg, &re, &im);
        complex_scalar.re = re;
        complex_scalar.im = im;
    } else {
        complex_scalar.re = real_scalar;
    }
    for (size_t i = 0; i < source_arr->size; i++) {
        double a = source_arr->data[i];
        CSN_COMPLEXDAT ca = { 0.0, 0.0 };
        CSN_COMPLEXDAT c = {0};
        if (source_arr->itype == CSN_COMPLEX) {
            ca.re = source_arr->data[i * 2];
            ca.im = source_arr->data[i * 2 + 1];
        }
        switch (mode) {
            case CSN_ADD_HS:
            case CSN_ADD_SH:
                if (source_arr->itype == CSN_REAL) {
                    arr->data[i] = a + real_scalar;
                } else {
                    complex_add(&c, ca, complex_scalar);
                    arr->data[i * 2] = c.re;
                    arr->data[i * 2 + 1] = c.im;
                }
                break;
            case CSN_SUB_HS:
                if (source_arr->itype == CSN_REAL) {
                    arr->data[i] = a - real_scalar;
                } else {
                    complex_sub(&c, ca, complex_scalar);
                    arr->data[i * 2] = c.re;
                    arr->data[i * 2 + 1] = c.im;
                }
                break;
            case CSN_SUB_SH:
                if (source_arr->itype == CSN_REAL) {
                    arr->data[i] = real_scalar - a;
                } else {
                    complex_sub(&c, complex_scalar, ca);
                    arr->data[i * 2] = c.re;
                    arr->data[i * 2 + 1] = c.im;
                }
                break;
            case CSN_MUL_HS:
            case CSN_MUL_SH:
                if (source_arr->itype == CSN_REAL) {
                    arr->data[i] = a * real_scalar;
                } else {
                    complex_prod(&c, ca, complex_scalar);
                    arr->data[i * 2] = c.re;
                    arr->data[i * 2 + 1] = c.im;
                }
                break;
            /* Division and log follow IEEE, as numpy does: x/0 is an infinity,
               0/0 and log of a negative are NaN. Raising here instead would
               make a note vanish mid-performance depending on its data. */
            case CSN_DIV_HS:
                if (source_arr->itype == CSN_REAL) {
                    arr->data[i] = a / real_scalar;
                } else {
                    if (complex_div(&c, ca, complex_scalar) != OK) {
                        res = csound->InitError(csound, "[csnarray] Division by zero");
                        goto done;
                    };
                    arr->data[i * 2] = c.re;
                    arr->data[i * 2 + 1] = c.im;
                }
                break;
            case CSN_DIV_SH:
                if (source_arr->itype == CSN_REAL) {
                    arr->data[i] = real_scalar / a;
                } else {
                    if (complex_div(&c, complex_scalar, ca) != OK) {
                        res = csound->InitError(csound, "[csnarray] Division by zero");
                        goto done;
                    };
                    arr->data[i * 2] = c.re;
                    arr->data[i * 2 + 1] = c.im;
                }
                break;
            case CSN_POW_HS:
                if (source_arr->itype == CSN_REAL) {
                    arr->data[i] = pow(a, real_scalar);
                } else {
                    if (complex_pow(&c, ca, complex_scalar) != OK) {
                        res = csound->InitError(csound, "[csnarray] Division by zero");
                        goto done;
                    };
                    arr->data[i * 2] = c.re;
                    arr->data[i * 2 + 1] = c.im;
                }
                break;
            case CSN_POW_SH:
                if (source_arr->itype == CSN_REAL) {
                    arr->data[i] = pow(real_scalar, a);
                } else {
                    if (complex_pow(&c, complex_scalar, ca) != OK) {
                        res = csound->InitError(csound, "[csnarray] Division by zero");
                        goto done;
                    };
                    arr->data[i * 2] = c.re;
                    arr->data[i * 2 + 1] = c.im;
                }
                break;
            case CSN_LOG_HS:
                /* Change of base. Base 1 divides by log(1) == 0 and yields an
                   infinity, which is also what numpy returns. */
                if (source_arr->itype == CSN_REAL) {
                    arr->data[i] = log(a) / log(real_scalar);
                } else {
                    CSN_COMPLEXDAT log_a = {0};
                    CSN_COMPLEXDAT log_b = {0};
                    complex_log(&log_a, ca);
                    complex_log(&log_b, complex_scalar);
                    if (complex_div(&c, log_a, log_b) != OK) {
                        res = csound->InitError(csound, "[csnarray] Division by zero");
                        goto done;
                    }
                    arr->data[i * 2] = c.re;
                    arr->data[i * 2 + 1] = c.im;
                }
                break;
            case CSN_LOG_SH:
                /* The array supplies the base here, the scalar the argument. */
                if (source_arr->itype == CSN_REAL) {
                    arr->data[i] = log(real_scalar) / log(a);
                } else {
                    CSN_COMPLEXDAT log_a = {0};
                    CSN_COMPLEXDAT log_b = {0};
                    complex_log(&log_a, ca);
                    complex_log(&log_b, complex_scalar);
                    if (complex_div(&c, log_b, log_a) != OK) {
                        res = csound->InitError(csound, "[csnarray] Division by zero");
                        goto done;
                    }
                    arr->data[i * 2] = c.re;
                    arr->data[i * 2 + 1] = c.im;
                }
                break;
            case CSN_LOGICAL_AND_HS:
                arr->data[i] = (double) ((a != 0.0) && (real_scalar != 0.0));
                break;
            case CSN_LOGICAL_OR_HS:
                arr->data[i] = (double) ((a != 0.0) || (real_scalar != 0.0));
                break;
            default:
                break;
        }
    }

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_add_hh(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_helper(csound, p, CSN_ADD_HH);
}

int32_t csnarray_add_hs(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, CSN_ADD_HS);
}

int32_t csnarray_subtract_hh(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_helper(csound, p, CSN_SUB_HH);
}

int32_t csnarray_subtract_hs(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, CSN_SUB_HS);
}

int32_t csnarray_subtract_sh(CSOUND *csound, CSN_BINOP_SH *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, CSN_SUB_SH);
}

int32_t csnarray_mul_hh(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_helper(csound, p, CSN_MUL_HH);
}

int32_t csnarray_mul_hs(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, CSN_MUL_HS);
}

int32_t csnarray_div_hh(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_helper(csound, p, CSN_DIV_HH);
}

int32_t csnarray_div_hs(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, CSN_DIV_HS);
}

int32_t csnarray_div_sh(CSOUND *csound, CSN_BINOP_SH *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, CSN_DIV_SH);
}

int32_t csnarray_pow_hh(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_helper(csound, p, CSN_POW_HH);
}

int32_t csnarray_pow_hs(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, CSN_POW_HS);
}

int32_t csnarray_pow_sh(CSOUND *csound, CSN_BINOP_SH *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, CSN_POW_SH);
}

int32_t csnarray_log_hh(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_helper(csound, p, CSN_LOG_HH);
}

int32_t csnarray_log_hs(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, CSN_LOG_HS);
}

int32_t csnarray_log_sh(CSOUND *csound, CSN_BINOP_SH *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, CSN_LOG_SH);
}

int32_t csnarray_addcomp_hs(CSOUND *csound, CSN_BINOPCOMPLEX_HS *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, p->scalar, CSN_ADD_HS);
}

int32_t csnarray_subtractcomp_hs(CSOUND *csound, CSN_BINOPCOMPLEX_HS *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, p->scalar, CSN_SUB_HS);
}

int32_t csnarray_subtractcomp_sh(CSOUND *csound, CSN_BINOPCOMPLEX_SH *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, p->scalar, CSN_SUB_SH);
}

int32_t csnarray_mulcomp_hs(CSOUND *csound, CSN_BINOPCOMPLEX_HS *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, p->scalar, CSN_MUL_HS);
}

int32_t csnarray_divcomp_hs(CSOUND *csound, CSN_BINOPCOMPLEX_HS *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, p->scalar, CSN_DIV_HS);
}

int32_t csnarray_divcomp_sh(CSOUND *csound, CSN_BINOPCOMPLEX_SH *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, p->scalar, CSN_DIV_SH);
}

int32_t csnarray_powcomp_hs(CSOUND *csound, CSN_BINOPCOMPLEX_HS *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, p->scalar, CSN_POW_HS);
}

int32_t csnarray_powcomp_sh(CSOUND *csound, CSN_BINOPCOMPLEX_SH *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, p->scalar, CSN_POW_SH);
}

int32_t csnarray_logcomp_sh(CSOUND *csound, CSN_BINOPCOMPLEX_SH *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, p->scalar, CSN_LOG_SH);
}

int32_t csnarray_logcomp_hs(CSOUND *csound, CSN_BINOPCOMPLEX_HS *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, p->scalar, CSN_LOG_HS);
}

int32_t csnarray_logical_and_hh(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_helper(csound, p, CSN_LOGICAL_AND_HH);
}

int32_t csnarray_logical_or_hh(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_helper(csound, p, CSN_LOGICAL_OR_HH);
}

int32_t csnarray_logical_and_hs(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, CSN_LOGICAL_AND_HS);
}

int32_t csnarray_logical_or_hs(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, CSN_LOGICAL_OR_HS);
}

int32_t csnarray_logical_and_sh(CSOUND *csound, CSN_BINOP_SH *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, CSN_LOGICAL_AND_HS);
}

int32_t csnarray_logical_or_sh(CSOUND *csound, CSN_BINOP_SH *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, CSN_LOGICAL_OR_HS);
}

static int32_t csnarray_unaryop_helper(CSOUND *csound, CSN_UNARYOP *p, CSN_UNARY_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

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
    ITEM_TYPE itype = source_arr->itype;
    if (itype == CSN_COMPLEX && (mode == CSN_FLOOR || mode == CSN_CEIL || mode == CSN_ROUND || mode == CSN_LOGICAL_NOT)) {
        res = csound->InitError(csound, "[csnarray] floot, ceil, round and logical not operations not allowed for complex array");
        goto done;
    }

    ITEM_TYPE new_itype = (itype == CSN_COMPLEX && mode == CSN_ABS) ? CSN_REAL : itype;

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    memcpy(new_shape, source_arr->shape, sizeof(uint32_t) * CSN_MAX_DIMS);

    const uint32_t protect[1] = { source_handle };
    if (create_csnarray_locked(csound, reg, &p->h, source_arr->ndim, new_shape, &p->array, p->handle, protect, 1U, &err, new_itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;
    for (size_t i = 0; i < source_arr->size; i++) {
        double a = 0.0;
        CSN_COMPLEXDAT ca = { 0.0, 0.0 };
        CSN_COMPLEXDAT cout = { 0.0, 0.0 };
        if (itype == CSN_REAL) {
            a = source_arr->data[i];
        } else {
            ca.re = source_arr->data[i * 2];
            ca.im = source_arr->data[i * 2 + 1];
        }
        switch (mode) {
            case CSN_SQRT:
                if (itype == CSN_REAL) {
                    arr->data[i] = sqrt(a);
                } else {
                    complex_sqrt(&cout, ca);
                    arr->data[i * 2] = cout.re;
                    arr->data[i * 2 + 1] = cout.im;
                }
                break;
            case CSN_CBRT:
                if (itype == CSN_REAL) {
                    arr->data[i] = cbrt(a);
                } else {
                    CSN_COMPLEXDAT one_third = { 1.0 / 3.0, 0.0 };
                    complex_pow(&cout, ca, one_third);
                    arr->data[i * 2] = cout.re;
                    arr->data[i * 2 + 1] = cout.im;
                }
                break;
            case CSN_ABS:
                if (itype == CSN_REAL) {
                    arr->data[i] = fabs(a);
                } else {
                    arr->data[i] = hypot(ca.re, ca.im);
                }
                break;
            case CSN_SIGN:
                /* numpy: sign(0) is 0 and sign(NaN) is NaN, so returning a
                   itself covers both without a special case. */
                if (itype == CSN_REAL) {
                    arr->data[i] = (a > 0.0) ? 1.0 : ((a < 0.0) ? -1.0 : a);
                } else {
                    complex_sign(&cout, ca);
                    arr->data[i * 2] = cout.re;
                    arr->data[i * 2 + 1] = cout.im;
                }
                break;
            case CSN_EXP:
                if (itype == CSN_REAL) {
                    arr->data[i] = exp(a);
                } else {
                    complex_exp(&cout, ca);
                    arr->data[i * 2] = cout.re;
                    arr->data[i * 2 + 1] = cout.im;
                }
                break;
            case CSN_SIN:
                if (itype == CSN_REAL) {
                    arr->data[i] = sin(a);
                } else {
                    complex_sin(&cout, ca);
                    arr->data[i * 2] = cout.re;
                    arr->data[i * 2 + 1] = cout.im;
                }
                break;
            case CSN_COS:
                if (itype == CSN_REAL) {
                    arr->data[i] = cos(a);
                } else {
                    complex_cos(&cout, ca);
                    arr->data[i * 2] = cout.re;
                    arr->data[i * 2 + 1] = cout.im;
                }
                break;
            case CSN_TAN:
                if (itype == CSN_REAL) {
                    arr->data[i] = tan(a);
                } else {
                    complex_tan(&cout, ca);
                    arr->data[i * 2] = cout.re;
                    arr->data[i * 2 + 1] = cout.im;
                }
                break;
            case CSN_ASIN:
                if (itype == CSN_REAL) {
                    arr->data[i] = asin(a);
                } else {
                    complex_asin(&cout, ca);
                    arr->data[i * 2] = cout.re;
                    arr->data[i * 2 + 1] = cout.im;
                }
                break;
            case CSN_ACOS:
                if (itype == CSN_REAL) {
                    arr->data[i] = acos(a);
                } else {
                    complex_acos(&cout, ca);
                    arr->data[i * 2] = cout.re;
                    arr->data[i * 2 + 1] = cout.im;
                }
                break;
            case CSN_ATAN:
                if (itype == CSN_REAL) {
                    arr->data[i] = atan(a);
                } else {
                    complex_atan(&cout, ca);
                    arr->data[i * 2] = cout.re;
                    arr->data[i * 2 + 1] = cout.im;
                }
                break;
            case CSN_SINH:
                if (itype == CSN_REAL) {
                    arr->data[i] = sinh(a);
                } else {
                    complex_sinh(&cout, ca);
                    arr->data[i * 2] = cout.re;
                    arr->data[i * 2 + 1] = cout.im;
                }
                break;
            case CSN_COSH:
                if (itype == CSN_REAL) {
                    arr->data[i] = cosh(a);
                } else {
                    complex_cosh(&cout, ca);
                    arr->data[i * 2] = cout.re;
                    arr->data[i * 2 + 1] = cout.im;
                }
                break;
            case CSN_TANH:
                if (itype == CSN_REAL) {
                    arr->data[i] = tanh(a);
                } else {
                    complex_tanh(&cout, ca);
                    arr->data[i * 2] = cout.re;
                    arr->data[i * 2 + 1] = cout.im;
                }
                break;
            case CSN_ASINH:
                if (itype == CSN_REAL) {
                    arr->data[i] = asinh(a);
                } else {
                    complex_asinh(&cout, ca);
                    arr->data[i * 2] = cout.re;
                    arr->data[i * 2 + 1] = cout.im;
                }
                break;
            case CSN_ACOSH:
                if (itype == CSN_REAL) {
                    arr->data[i] = acosh(a);
                } else {
                    complex_acosh(&cout, ca);
                    arr->data[i * 2] = cout.re;
                    arr->data[i * 2 + 1] = cout.im;
                }
                break;
            case CSN_ATANH:
                if (itype == CSN_REAL) {
                    arr->data[i] = atanh(a);
                } else {
                    complex_atanh(&cout, ca);
                    arr->data[i * 2] = cout.re;
                    arr->data[i * 2 + 1] = cout.im;
                }
                break;
            case CSN_FLOOR:
                arr->data[i] = floor(a);
                break;
            case CSN_CEIL:
                arr->data[i] = ceil(a);
                break;
            case CSN_ROUND:
                /* numpy rounds halves to even; C's round() sends them away
                   from zero, so 2.5 would become 3 instead of 2. rint follows
                   the current mode, which is round-to-nearest-even by default. */
                arr->data[i] = rint(a);
                break;
            case CSN_LOGICAL_NOT:
                arr->data[i] = (double) !(a != 0.0);
                break;
        }
    }

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}


int32_t csnarray_sqrt(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_helper(csound, p, CSN_SQRT);
}

int32_t csnarray_cbrt(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_helper(csound, p, CSN_CBRT);
}

int32_t csnarray_abs(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_helper(csound, p, CSN_ABS);
}

int32_t csnarray_exp(CSOUND *csound, CSN_UNARYOP *p){
    return csnarray_unaryop_helper(csound, p, CSN_EXP);
}

int32_t csnarray_sin(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_helper(csound, p, CSN_SIN);
}

int32_t csnarray_cos(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_helper(csound, p, CSN_COS);
}

int32_t csnarray_tan(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_helper(csound, p, CSN_TAN);
}

int32_t csnarray_asin(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_helper(csound, p, CSN_ASIN);
}

int32_t csnarray_acos(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_helper(csound, p, CSN_ACOS);
}

int32_t csnarray_atan(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_helper(csound, p, CSN_ATAN);
}

int32_t csnarray_sinh(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_helper(csound, p, CSN_SINH);
}

int32_t csnarray_cosh(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_helper(csound, p, CSN_COSH);
}

int32_t csnarray_tanh(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_helper(csound, p, CSN_TANH);
}

int32_t csnarray_asinh(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_helper(csound, p, CSN_ASINH);
}

int32_t csnarray_acosh(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_helper(csound, p, CSN_ACOSH);
}

int32_t csnarray_atanh(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_helper(csound, p, CSN_ATANH);
}

int32_t csnarray_floor(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_helper(csound, p, CSN_FLOOR);
}

int32_t csnarray_ceil(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_helper(csound, p, CSN_CEIL);
}

int32_t csnarray_round(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_helper(csound, p, CSN_ROUND);
}

int32_t csnarray_sign(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_helper(csound, p, CSN_SIGN);
}

int32_t csnarray_logical_not(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_helper(csound, p, CSN_LOGICAL_NOT);
}

static int32_t check_dot_shape(const uint32_t *shape_a, const uint32_t *shape_b, size_t dim_a, size_t dim_b) {
    size_t bk = (dim_b >= 2) ? dim_b - 2 : 0;
    return (shape_a[dim_a - 1] == shape_b[bk]) ? OK : NOTOK;
}

static inline CSN_COMPLEXDAT item_at(const CSN_ARRAY *arr, size_t off) {
    CSN_COMPLEXDAT z = { arr->data[off * arr->itype], 0.0 };
    if (arr->itype == CSN_COMPLEX) z.im = arr->data[off * 2 + 1];
    return z;
}

static inline void item_set(CSN_ARRAY *arr, size_t off, CSN_COMPLEXDAT z) {
    arr->data[off * arr->itype] = z.re;
    if (arr->itype == CSN_COMPLEX) arr->data[off * 2 + 1] = z.im;
}

static void get_dot_inner_accum(CSN_COMPLEXDAT *acc, uint32_t *a_coords, uint32_t *b_coords, uint32_t off_dim_a, uint32_t off_dim_b, CSN_ARRAY *source_arr_a, CSN_ARRAY *source_arr_b, uint32_t source_dim_a, uint32_t source_dim_b, uint32_t loop_dim) {
    acc->re = 0.0;
    acc->im = 0.0;
    for (uint32_t k = 0; k < loop_dim; ++k) {
        a_coords[off_dim_a] = k;
        b_coords[off_dim_b] = k;
        size_t off_a = from_coords_to_offset(a_coords, source_arr_a->strides, source_dim_a);
        size_t off_b = from_coords_to_offset(b_coords, source_arr_b->strides, source_dim_b);
        CSN_COMPLEXDAT prod = {0};
        complex_prod(&prod, item_at(source_arr_a, off_a), item_at(source_arr_b, off_b));
        complex_add(acc, *acc, prod);
    }
}

static void dot_inner(CSN_ARRAY *out_arr, CSN_ARRAY *source_arr_a, CSN_ARRAY *source_arr_b, CSN_VECOP_MODE mode) {
    uint32_t source_dim_a = source_arr_a->ndim;
    uint32_t source_dim_b = source_arr_b->ndim;
    uint32_t *source_shape_a = source_arr_a->shape;
    uint32_t ka_dim  = source_shape_a[source_dim_a - 1];
    size_t bk = (source_dim_b >= 2) ? source_dim_b - 2 : 0;
    for (size_t linear = 0; linear < out_arr->size; linear++) {
        uint32_t dst_coords[CSN_MAX_DIMS] = {0};
        uint32_t a_coords[CSN_MAX_DIMS]   = {0};
        uint32_t b_coords[CSN_MAX_DIMS]   = {0};

        from_linear_to_coords(dst_coords, out_arr->shape, linear, out_arr->ndim);
        uint32_t d = 0;
        CSN_COMPLEXDAT acc = { 0.0, 0.0 };

        switch (mode) {
            case CSN_DOT:
                for (uint32_t i = 0; i + 1 < source_dim_a; ++i) {
                    a_coords[i] = dst_coords[d++];
                }

                for (uint32_t i = 0; i < source_dim_b; ++i) {
                    if (i != bk) b_coords[i] = dst_coords[d++];
                }

                get_dot_inner_accum(&acc, a_coords, b_coords, source_dim_a - 1, bk, source_arr_a, source_arr_b, source_dim_a, source_dim_b, ka_dim);
                item_set(out_arr, linear, acc);
                break;
            case CSN_INNER:
                memcpy(a_coords, dst_coords, sizeof(uint32_t) * (source_dim_a - 1));
                memcpy(b_coords, dst_coords + (source_dim_a - 1), sizeof(uint32_t) * (source_dim_b - 1));
                get_dot_inner_accum(&acc, a_coords, b_coords, source_dim_a - 1, source_dim_b - 1, source_arr_a, source_arr_b, source_dim_a, source_dim_b, ka_dim);
                item_set(out_arr, linear, acc);
                break;
            default:
                break;
        }
    }
}

static int32_t csnarray_vec_helper(CSOUND *csound, CSN_BINOP_HH *p, CSN_VECOP_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

    uint32_t source_handle_a = (uint32_t) p->source_handle_a->id;
    uint32_t source_handle_b = (uint32_t) p->source_handle_b->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot_a = get_slot(reg, source_handle_a);
    if (slot_a == NULL) {
        return csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle_a);
    }

    CSN_SLOT *slot_b = get_slot(reg, source_handle_b);
    if (slot_b == NULL) {
        return csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle_b);
    }

    CSN_ARRAY *source_arr_a = slot_a->array;
    if (source_arr_a->itype != slot_b->array->itype) {
        res = csound->InitError(csound, "[csnarray] Element type mismatch: one array is real and the other complex");
        goto done;
    }
    CSN_ARRAY *source_arr_b = slot_b->array;
    uint32_t *source_shape_a = source_arr_a->shape;
    uint32_t *source_shape_b = source_arr_b->shape;
    size_t source_dim_a = source_arr_a->ndim;
    size_t source_dim_b = source_arr_b->ndim;

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    uint32_t new_ndim = 0;
    uint32_t j = 0;

    size_t bk = (source_dim_b >= 2) ? source_dim_b - 2 : 0;
    switch (mode) {
        case CSN_DOT:
            if (source_dim_a == 1 && source_dim_b == 1) {
                res = csound->InitError(csound, "[csnarray] The dot product of two 1-D arrays is a scalar; declare the output as i, not as a handle");
                goto done;
            }

            if (check_dot_shape(source_shape_a, source_shape_b, source_dim_a, source_dim_b) != OK) {
                char abuf[CSN_SHAPE_STR_MAX], bbuf[CSN_SHAPE_STR_MAX];
                res = csound->InitError(csound, "[csnarray] Shapes %s and %s are not valid for a dot product: the last axis of the first must match the second-to-last axis of the second", shape_str(abuf, sizeof(abuf), source_shape_a, (uint32_t) source_dim_a), shape_str(bbuf, sizeof(bbuf), source_shape_b, (uint32_t) source_dim_b));
                goto done;
            }

            j = 0;
            for (uint32_t i = 0; i + 1 < source_dim_a; ++i){
                new_shape[j++] = source_shape_a[i];
            }
            for (uint32_t i = 0; i < source_dim_b; ++i){
                if (i != bk) new_shape[j++] = source_shape_b[i];
            }

            new_ndim = j;
            break;
        case CSN_INNER:
            if (source_dim_a == 1 && source_dim_b == 1) {
                res = csound->InitError(csound, "[csnarray] The inner product of two 1-D arrays is a scalar; declare the output as i, not as a handle");
                goto done;
            }

            if (source_shape_a[source_dim_a - 1] != source_shape_b[source_dim_b - 1]) {
                char abuf[CSN_SHAPE_STR_MAX], bbuf[CSN_SHAPE_STR_MAX];
                res = csound->InitError(csound, "[csnarray] Shapes %s and %s are not valid for an inner product: the last axis must match (%u vs %u)", shape_str(abuf, sizeof(abuf), source_shape_a, (uint32_t) source_dim_a), shape_str(bbuf, sizeof(bbuf), source_shape_b, (uint32_t) source_dim_b), source_shape_a[source_dim_a - 1], source_shape_b[source_dim_b - 1]);
                goto done;
            }

            j = 0;
            for (uint32_t i = 0; i < source_dim_a - 1; ++i) {
                new_shape[j++] = source_shape_a[i];
            }

            for (uint32_t i = 0; i < source_dim_b - 1; ++i) {
                new_shape[j++] = source_shape_b[i];
            }

            new_ndim = j;
            break;
        case CSN_OUTER:
            if (source_dim_a != 1 || source_dim_b != 1) {
                res = csound->InitError(csound, "[csnarray] Outer product needs two 1-D arrays, got %u-D and %u-D", (uint32_t) source_dim_a, (uint32_t) source_dim_b);
                goto done;
            }

            new_shape[0] = source_shape_a[0];
            new_shape[1] = source_shape_b[0];
            new_ndim = 2U;
            break;
        case CSN_PAIR_DISTANCE:
        case CSN_PROJECT:
        case CSN_REJECT:
        case CSN_REFLECT:
            if (source_dim_a != source_dim_b || memcmp(source_shape_a, source_shape_b, sizeof(uint32_t) * CSN_MAX_DIMS) != 0) {
                char abuf[CSN_SHAPE_STR_MAX], bbuf[CSN_SHAPE_STR_MAX];
                res = csound->InitError(csound, "[csnarray] Both arrays must have the same shape, got %s and %s", shape_str(abuf, sizeof(abuf), source_shape_a, (uint32_t) source_dim_a), shape_str(bbuf, sizeof(bbuf), source_shape_b, (uint32_t) source_dim_b));
                goto done;
            }

            memcpy(new_shape, source_shape_a, sizeof(uint32_t) * CSN_MAX_DIMS);
            new_ndim = source_dim_a;
            break;
        case CSN_CROSS:
            if (source_dim_a != 1 || source_dim_b != 1 || source_shape_a[0] != 3 || source_shape_b[0] != 3) {
                char abuf[CSN_SHAPE_STR_MAX], bbuf[CSN_SHAPE_STR_MAX];
                res = csound->InitError(csound, "[csnarray] Cross product needs two 1-D arrays of size 3, got %s and %s", shape_str(abuf, sizeof(abuf), source_shape_a, (uint32_t) source_dim_a), shape_str(bbuf, sizeof(bbuf), source_shape_b, (uint32_t) source_dim_b));
                goto done;
            }

            new_shape[0] = 3;
            new_ndim = 1;
            break;
        default:
            break;
    }

    const uint32_t protect[2] = { source_handle_a, source_handle_b };
    ITEM_TYPE out_itype = (mode == CSN_PAIR_DISTANCE) ? CSN_REAL : source_arr_a->itype;
    if (create_csnarray_locked(csound, reg, &p->h, new_ndim, new_shape, &p->array, p->handle, protect, 2U, &err, out_itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;

    size_t size_a = source_arr_a->size;
    size_t size_b = source_arr_b->size;
    CSN_COMPLEXDAT dot_ab = { 0.0, 0.0 };
    CSN_COMPLEXDAT dot_bb = { 0.0, 0.0 };
    switch(mode) {
        case CSN_DOT:
        case CSN_INNER:
            dot_inner(arr, source_arr_a, source_arr_b, mode);
            break;
        case CSN_OUTER:
            for (size_t i = 0; i < size_a; ++i) {
                for (size_t j = 0; j < size_b; ++j) {
                    CSN_COMPLEXDAT prod = {0};
                    complex_prod(&prod, item_at(source_arr_a, i), item_at(source_arr_b, j));
                    item_set(arr, i * size_b + j, prod);
                }
            }
            break;
        case CSN_PAIR_DISTANCE:
            for (size_t i = 0; i < size_a; ++i) {
                CSN_COMPLEXDAT d = {0};
                complex_sub(&d, item_at(source_arr_a, i), item_at(source_arr_b, i));
                arr->data[i] = hypot(d.re, d.im);
            }
            break;
        case CSN_PROJECT:
        case CSN_REJECT:
        case CSN_REFLECT: {
            for (size_t i = 0; i < size_a; ++i) {
                CSN_COMPLEXDAT ai = item_at(source_arr_a, i);
                CSN_COMPLEXDAT bi = item_at(source_arr_b, i);
                CSN_COMPLEXDAT bconj = { bi.re, -bi.im };
                CSN_COMPLEXDAT t = {0};
                complex_prod(&t, ai, bconj);
                complex_add(&dot_ab, dot_ab, t);
                complex_prod(&t, bi, bconj);
                complex_add(&dot_bb, dot_bb, t);
            }

            if (dot_bb.re == 0.0 && dot_bb.im == 0.0) {
                res = csound->InitError(csound, "[csnarray] The second vector has zero length, so the projection onto it is undefined");
                goto done;
            }

            CSN_COMPLEXDAT scale = {0};
            if (complex_div(&scale, dot_ab, dot_bb) != OK) {
                res = csound->InitError(csound, "[csnarray] The second vector has zero length, so the projection onto it is undefined");
                goto done;
            }

            for (size_t i = 0; i < size_a; ++i) {
                CSN_COMPLEXDAT ai = item_at(source_arr_a, i);
                CSN_COMPLEXDAT proj = {0};
                complex_prod(&proj, scale, item_at(source_arr_b, i));

                CSN_COMPLEXDAT value = {0};
                switch (mode) {
                    case CSN_PROJECT:
                        value = proj;
                        break;
                    case CSN_REJECT:
                        complex_sub(&value, ai, proj);
                        break;
                    case CSN_REFLECT: {
                        CSN_COMPLEXDAT twice = { proj.re * 2.0, proj.im * 2.0 };
                        complex_sub(&value, ai, twice);
                        break;
                    }
                    default:
                        break;
                }

                item_set(arr, i, value);
            }
            break;
        }
        case CSN_CROSS: {
            /* Bilinear */
            CSN_COMPLEXDAT a0 = item_at(source_arr_a, 0), a1 = item_at(source_arr_a, 1), a2 = item_at(source_arr_a, 2);
            CSN_COMPLEXDAT b0 = item_at(source_arr_b, 0), b1 = item_at(source_arr_b, 1), b2 = item_at(source_arr_b, 2);
            CSN_COMPLEXDAT l = {0}, r = {0}, c = {0};

            complex_prod(&l, a1, b2); complex_prod(&r, a2, b1);
            complex_sub(&c, l, r); item_set(arr, 0, c);

            complex_prod(&l, a2, b0); complex_prod(&r, a0, b2);
            complex_sub(&c, l, r); item_set(arr, 1, c);

            complex_prod(&l, a0, b1); complex_prod(&r, a1, b0);
            complex_sub(&c, l, r); item_set(arr, 2, c);
            break;
        }
        default:
            break;
    }
done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_dot(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_vec_helper(csound, p, CSN_DOT);
}

static int32_t csnarray_scalar_helper_impl(CSOUND *csound, CSNREF *ref_a, CSNREF *ref_b, MYFLT *out_value, COMPLEXDAT *out_complex, CSN_VECOP_MODE mode, double dist_order) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

    uint32_t source_handle_a = (uint32_t) ref_a->id;
    uint32_t source_handle_b = (uint32_t) ref_b->id;

    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot_a = get_slot(reg, source_handle_a);
    if (slot_a == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle_a);
        goto done;
    }

    CSN_SLOT *slot_b = get_slot(reg, source_handle_b);
    if (slot_b == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle_b);
        goto done;
    }

    CSN_ARRAY *source_arr_a = slot_a->array;
    CSN_ARRAY *source_arr_b = slot_b->array;
    uint32_t *source_shape_a = source_arr_a->shape;
    uint32_t *source_shape_b = source_arr_b->shape;
    size_t source_dim_a = source_arr_a->ndim;
    size_t source_dim_b = source_arr_b->ndim;

    if (source_dim_a != 1 || source_dim_b != 1 || source_shape_a[0] != source_shape_b[0]) {
        char abuf[CSN_SHAPE_STR_MAX], bbuf[CSN_SHAPE_STR_MAX];
        res = csound->InitError(csound, "[csnarray] Both arrays must be 1-D with the same size, got %s and %s", shape_str(abuf, sizeof(abuf), source_shape_a, (uint32_t) source_dim_a), shape_str(bbuf, sizeof(bbuf), source_shape_b, (uint32_t) source_dim_b));
        goto done;
    }

    if (source_arr_a->itype != source_arr_b->itype) {
        res = csound->InitError(csound, "[csnarray] Element type mismatch: one array is real and the other complex");
        goto done;
    }

    if (mode == CSN_DISTANCE && dist_order <= 0.0) {
        res = csound->InitError(csound, "[csnarray] Distance order must be >= 1, got %g", dist_order);
        goto done;
    }

    bool wants_complex = (mode != CSN_DISTANCE) && source_arr_a->itype == CSN_COMPLEX;
    if (wants_complex && out_complex == NULL) {
        res = csound->InitError(csound, "[csnarray] Handle holds a complex array; declare the result as :Complex;");
        goto done;
    }
    if (!wants_complex && out_value == NULL) {
        res = csound->InitError(csound, "[csnarray] Handle holds a real array; declare the result as i");
        goto done;
    }

    double value = 0.0;
    CSN_COMPLEXDAT acc = { 0.0, 0.0 };
    for (size_t i = 0; i < source_arr_a->size; i++) {
        CSN_COMPLEXDAT ai = item_at(source_arr_a, i);
        CSN_COMPLEXDAT bi = item_at(source_arr_b, i);
        switch (mode) {
            case CSN_DOT_SCALAR:
            case CSN_INNER_SCALAR: {
                /* Bilinear */
                CSN_COMPLEXDAT prod = {0};
                complex_prod(&prod, ai, bi);
                complex_add(&acc, acc, prod);
                break;
            }
            case CSN_DISTANCE: {
                CSN_COMPLEXDAT d = {0};
                complex_sub(&d, ai, bi);
                value += pow(hypot(d.re, d.im), dist_order);
                break;
            }
            default:
                break;
        }
    }

    if (mode == CSN_DISTANCE) {
        *out_value = (MYFLT) pow(value, 1.0 / dist_order);
    } else if (wants_complex) {
        out_complex->real = (MYFLT) acc.re;
        out_complex->imag = (MYFLT) acc.im;
        out_complex->isPolar = 0;
    } else {
        *out_value = (MYFLT) acc.re;
    }

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_scalar_helper(CSOUND *csound, CSN_BINOP_HH_SCALAR *p, CSN_VECOP_MODE mode, double dist_order) {
    return csnarray_scalar_helper_impl(csound, p->source_handle_a, p->source_handle_b, p->value, NULL, mode, dist_order);
}

int32_t csnarray_dotcomp_scalar(CSOUND *csound, CSN_BINOPCOMPLEX_HH_SCALAR *p) {
    return csnarray_scalar_helper_impl(csound, p->source_handle_a, p->source_handle_b, NULL, p->value, CSN_DOT_SCALAR, 0.0);
}

int32_t csnarray_innercomp_scalar(CSOUND *csound, CSN_BINOPCOMPLEX_HH_SCALAR *p) {
    return csnarray_scalar_helper_impl(csound, p->source_handle_a, p->source_handle_b, NULL, p->value, CSN_INNER_SCALAR, 0.0);
}

int32_t csnarray_dot_scalar(CSOUND *csound, CSN_BINOP_HH_SCALAR *p) {
    return csnarray_scalar_helper(csound, p, CSN_DOT_SCALAR, 0.0);
}

int32_t csnarray_inner(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_vec_helper(csound, p, CSN_INNER);
}

int32_t csnarray_inner_scalar(CSOUND *csound, CSN_BINOP_HH_SCALAR *p) {
    return csnarray_scalar_helper(csound, p, CSN_INNER_SCALAR, 0.0);
}

int32_t csnarray_outer(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_vec_helper(csound, p, CSN_OUTER);
}

int32_t csnarray_project(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_vec_helper(csound, p, CSN_PROJECT);
}

int32_t csnarray_reject(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_vec_helper(csound, p, CSN_REJECT);
}

int32_t csnarray_reflect(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_vec_helper(csound, p, CSN_REFLECT);
}

int32_t csnarray_cross(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_vec_helper(csound, p, CSN_CROSS);
}

static double norm_from_scratch(const double *arr, size_t size, double order, ITEM_TYPE itype) {
    double acc = 0.0;
    for (size_t i = 0; i < size; i++) {
        double x = itype == CSN_COMPLEX ? hypot(arr[i * 2], arr[i * 2 + 1]) : fabs(arr[i]);
        acc += pow(x, order);
    }
    return pow(acc, 1.0 / order);
}

int32_t csnarray_norm(CSOUND *csound, CSN_NORM_REDUCTION *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

    uint32_t source_handle = (uint32_t) p->source_handle->id;
    double axis_value = (double) *p->axis;
    double order = (double) *p->order;

    if (order < 1.0) {
        return csound->InitError(csound, "[csnarray] Norm order must be >= 1, got %g", order);
    }

    int32_t res = OK;
    const char *err = NULL;
    double *scratch = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = source_slot->array;
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    if (!IS_VALID_AXIS(axis_value, source_ndim)) {
        res = csound->InitError(csound, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: finite integers 0..%u)", axis_value, source_ndim, source_ndim - 1);
        goto done;
    }
    uint32_t axis = (uint32_t) axis_value;

    ITEM_TYPE itype = source_arr->itype;
    size_t run = source_shape[axis];
    size_t scratch_items = run > 0 ? run : 1;
    scratch = csound->Calloc(csound, sizeof(double) * scratch_items * itype);
    if (scratch == NULL) {
        res = csound->InitError(csound, "[csnarray] Out of memory: allocation of %zu bytes failed", (size_t) (sizeof(double) * scratch_items * itype));
        goto done;
    }

    /* Reducing along an axis drops it, as every other reduction does and as
       np.linalg.norm(a, axis=k) reports. */
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    uint32_t new_ndim = 0;
    for (uint32_t i = 0; i < source_ndim; ++i) {
        if (i != (uint32_t) axis) new_shape[new_ndim++] = source_shape[i];
    }

    const uint32_t protect[1] = { source_handle };
    if (create_csnarray_locked(csound, reg, &p->h, new_ndim, new_shape, &p->array, p->handle, protect, 1U, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;

    for (size_t linear = 0; linear < arr->size; ++linear) {
        uint32_t dst_coords[CSN_MAX_DIMS] = {0};
        uint32_t src_coords[CSN_MAX_DIMS] = {0};

        from_linear_to_coords(dst_coords, arr->shape, linear, arr->ndim);

        for (uint32_t k = 0; k < run; ++k) {
            /* k walks the reduced axis; the surviving axes take the
               destination's own coordinates. */
            for (uint32_t i = 0, j = 0; i < source_ndim; ++i) {
                src_coords[i] = (i == (uint32_t) axis) ? k : dst_coords[j++];
            }
            size_t off = from_coords_to_offset(src_coords, source_arr->strides, source_ndim);
            scratch[k * itype] = source_arr->data[off * itype];
            if (itype == CSN_COMPLEX) scratch[k * 2 + 1] = source_arr->data[off * 2 + 1];
        }

        arr->data[linear] = norm_from_scratch(scratch, run, order, itype);
    }

done:
    csound->UnlockMutex(reg->mutex);
    if (scratch != NULL) {
        csound->Free(csound, scratch);
    }
    return res;
}

int32_t csnarray_norm_scalar(CSOUND *csound, CSN_NORM_REDUCTION_SCALAR *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

    double order = (double) *p->order;
    if (order < 1.0) {
        return csound->InitError(csound, "[csnarray] Norm order must be >= 1, got %g", order);
    }

    uint32_t source_handle_a = (uint32_t) p->source_handle->id;
    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, source_handle_a);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle_a);
        goto done;
    }

    CSN_ARRAY *source_arr = slot->array;
    *p->value = (MYFLT) norm_from_scratch(source_arr->data, source_arr->size, order, source_arr->itype);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static inline CSN_COMPLEXDAT slice_get(const double *src, size_t i, size_t stride, ITEM_TYPE itype) {
    size_t at = i * stride * itype;
    CSN_COMPLEXDAT z = { src[at], itype == CSN_COMPLEX ? src[at + 1] : 0.0 };
    return z;
}

static inline void slice_put(double *dst, size_t i, size_t stride, ITEM_TYPE itype, CSN_COMPLEXDAT z) {
    size_t at = i * stride * itype;
    dst[at] = z.re;
    if (itype == CSN_COMPLEX) dst[at + 1] = z.im;
}

static void normalize_slice(double *dst, const double *src, size_t n, size_t stride, double order, ITEM_TYPE itype) {
    double acc = 0.0;
    for (size_t i = 0; i < n; ++i) {
        CSN_COMPLEXDAT z = slice_get(src, i, stride, itype);
        double mag = itype == CSN_COMPLEX ? hypot(z.re, z.im) : fabs(z.re);
        acc += pow(mag, order);
    }

    double nrm = pow(acc, 1.0 / order);
    for (size_t i = 0; i < n; ++i) {
        CSN_COMPLEXDAT z = slice_get(src, i, stride, itype);
        if (nrm != 0.0) {
            z.re /= nrm;
            z.im /= nrm;
        }
        slice_put(dst, i, stride, itype, z);
    }
}

static void cumsumprod_slice(double *dst, const double *src, size_t n, size_t stride, bool is_cumsum, ITEM_TYPE itype) {
    CSN_COMPLEXDAT acc = { is_cumsum ? 0.0 : 1.0, 0.0 };
    for (size_t i = 0; i < n; ++i) {
        CSN_COMPLEXDAT z = slice_get(src, i, stride, itype);
        if (is_cumsum) complex_add(&acc, acc, z);
        else complex_prod(&acc, acc, z);
        slice_put(dst, i, stride, itype, acc);
    }
}

static void diff_slice(double *dst, const double *src, size_t n, size_t src_stride, size_t dst_stride, ITEM_TYPE itype) {
    for (size_t i = 0; i + 1 < n; ++i) {
        CSN_COMPLEXDAT hi = slice_get(src, i + 1, src_stride, itype);
        CSN_COMPLEXDAT lo = slice_get(src, i, src_stride, itype);
        CSN_COMPLEXDAT d = {0};
        complex_sub(&d, hi, lo);
        slice_put(dst, i, dst_stride, itype, d);
    }
}

static void gradient_slice(double *dst, const double *src, size_t n, size_t stride, ITEM_TYPE itype) {
    if (n == 0)
        return;
    if (n == 1) {
        CSN_COMPLEXDAT zero = { 0.0, 0.0 };
        slice_put(dst, 0, stride, itype, zero);
        return;
    }

    CSN_COMPLEXDAT d = {0};
    complex_sub(&d, slice_get(src, 1, stride, itype), slice_get(src, 0, stride, itype));
    slice_put(dst, 0, stride, itype, d);

    for (size_t i = 1; i < n - 1; ++i) {
        CSN_COMPLEXDAT c = {0};
        complex_sub(&c, slice_get(src, i + 1, stride, itype), slice_get(src, i - 1, stride, itype));
        c.re *= 0.5;
        c.im *= 0.5;
        slice_put(dst, i, stride, itype, c);
    }

    CSN_COMPLEXDAT e = {0};
    complex_sub(&e, slice_get(src, n - 1, stride, itype), slice_get(src, n - 2, stride, itype));
    slice_put(dst, n - 1, stride, itype, e);
}

static void sort_slice(double *buffer, double *dst, const double *src, size_t n, size_t stride) {
    for (size_t i = 0; i < n; ++i) {
        buffer[i] = src[i * stride];
    }

    qsort(buffer, n, sizeof(double), compare_double);

    for (size_t i = 0; i < n; ++i) {
        dst[i * stride] = buffer[i];
    }
}

static void argsort_slice(ARRAY_ELEMENT *buffer, double *dst, const double *src, size_t n, size_t stride) {
    for (size_t i = 0; i < n; ++i) {
        buffer[i].value = src[i * stride];
        /* argsort returns coordinates within the selected axis, not offsets
           into the underlying flat storage. */
        buffer[i].linear_index = (uint32_t) i;
    }

    qsort(buffer, n, sizeof(ARRAY_ELEMENT), compare_double_from_array_elem);

    for (size_t i = 0; i < n; ++i) {
        dst[i * stride] = (double) buffer[i].linear_index;
    }
}

static int32_t csnarray_unary_ax_helper(CSOUND *csound, const OPDS *h, CSNREF *src_ref, double axis_value, double order, CSNREF *out_handle, CSN_ARRAY **out_array, CSN_UNARYOP_AX_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

    if (mode == CSN_NORMALIZE && order < 1.0) {
        return csound->InitError(csound, "[csnarray] Norm order must be >= 1, got %g", order);
    }

    int32_t res = OK;
    const char *err = NULL;
    void *temp = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, src_ref->id);
    if (source_slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) src_ref->id);
        goto done;
    }

    CSN_ARRAY *source_arr = source_slot->array;
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;
    ITEM_TYPE itype = source_arr->itype;

    if ((mode == CSN_SORT || mode == CSN_ARGSORT) && itype == CSN_COMPLEX) {
        res = csound->InitError(csound, "[csnarray] Sort operation is for real array only");
        goto done;
    }

    if (axis_value != -1.0 && !IS_VALID_AXIS(axis_value, source_ndim)) {
        res = csound->InitError(csound, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: -1 for all axes, or finite integers 0..%u)", axis_value, source_ndim, source_ndim - 1);
        goto done;
    }
    int32_t axis = (int32_t) axis_value;

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    uint32_t new_dim = source_ndim;
    memcpy(new_shape, source_shape, sizeof(uint32_t) * CSN_MAX_DIMS);
    if (mode == CSN_DIFF) {
        if (axis == -1) {
            if (source_arr->size < 2) {
                res = csound->InitError(csound, "[csnarray] Diff needs at least 2 elements, got %zu", source_arr->size);
                goto done;
            }

            memset(new_shape, 0, sizeof(uint32_t) * CSN_MAX_DIMS);
            new_shape[0] = (uint32_t) source_arr->size - 1;
            new_dim = 1U;
        } else {
            if (source_shape[axis] < 2) {
                res = csound->InitError(csound, "[csnarray] Diff needs at least 2 elements along axis %d, got %u", axis, source_shape[axis]);
                goto done;
            }
            new_shape[axis]--;
        }
    } else if ((mode == CSN_CUMSUM || mode == CSN_CUMPROD) && axis == -1) {
        memset(new_shape, 0, sizeof(new_shape));
        new_shape[0] = (uint32_t) source_arr->size;
        new_dim = 1U;
    }

    CSN_ARRAY *arr = source_arr;
    if (out_handle != NULL) {
        const uint32_t protect[1] = { src_ref->id };
        if (create_csnarray_locked(csound, reg, h, new_dim, new_shape, out_array, out_handle, protect, 1U, &err, source_arr->itype) != OK) {
            res = csound->InitError(csound, "[csnarray] %s", err);
            goto done;
        }
        arr = *out_array;
    }

    if (axis == -1) {
        switch (mode) {
            case CSN_NORMALIZE:
                normalize_slice(arr->data, source_arr->data, source_arr->size, 1, order, itype);
                break;
            case CSN_DIFF:
                diff_slice(arr->data, source_arr->data, source_arr->size, 1, 1, itype);
                break;
            case CSN_GRADIENT:
                gradient_slice(arr->data, source_arr->data, source_arr->size, 1, itype);
                break;
            case CSN_CUMSUM:
            case CSN_CUMPROD: {
                bool is_cumsum = mode == CSN_CUMSUM;
                cumsumprod_slice(arr->data, source_arr->data, source_arr->size, 1, is_cumsum, itype);
                break;
            }
            case CSN_SORT:
                memcpy(arr->data, source_arr->data, sizeof(double) * source_arr->size);
                qsort(arr->data, source_arr->size, sizeof(double), compare_double);
                break;
            case CSN_ARGSORT:{
                if (source_arr->size == 0U)
                    break;
                temp = csound->Calloc(csound, sizeof(ARRAY_ELEMENT) * source_arr->size);
                if (temp == NULL) {
                    res = csound->InitError(csound, "[csnarray] Memory allocation failed");
                    goto done;
                }
                argsort_slice((ARRAY_ELEMENT *) temp, arr->data, source_arr->data, source_arr->size, 1U);
                break;
            }
        }
        goto done;
    }

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
    size_t dst_stride = arr->strides[axis];
    if ((mode == CSN_SORT || mode == CSN_ARGSORT) && source_shape[axis] == 0U)
        goto done;
    if (mode == CSN_SORT) {
        temp = csound->Calloc(csound, sizeof(double) * source_shape[axis]);
        if (temp == NULL) {
            res = csound->InitError(csound, "Memory allocation failed");
            goto done;
        }
    }

    if (mode == CSN_ARGSORT) {
        temp = csound->Calloc(csound, sizeof(ARRAY_ELEMENT) * source_shape[axis]);
        if (temp == NULL) {
            res = csound->InitError(csound, "Memory allocation failed");
            goto done;
        }
    }

    for (size_t linear = 0; linear < slice_count; ++linear) {
        uint32_t dst_coords[CSN_MAX_DIMS] = {0};
        uint32_t src_coords[CSN_MAX_DIMS] = {0};

        from_linear_to_coords(dst_coords, reduced_shape, linear, reduced_ndim);
        for (uint32_t i = 0, j = 0; i < source_ndim; ++i) {
            src_coords[i] = (i == (uint32_t) axis) ? 0 : dst_coords[j++];
        }

        size_t src_base = from_coords_to_offset(src_coords, source_arr->strides, source_ndim);
        size_t dst_base = from_coords_to_offset(src_coords, arr->strides, source_ndim);
        switch (mode) {
            case CSN_NORMALIZE:
                normalize_slice(arr->data + dst_base * itype, source_arr->data + src_base * itype, source_shape[axis], src_stride, order, itype);
                break;
            case CSN_DIFF:
                diff_slice(arr->data + dst_base * itype, source_arr->data + src_base * itype, source_shape[axis], src_stride, dst_stride, itype);
                break;
            case CSN_GRADIENT:
                gradient_slice(arr->data + dst_base * itype, source_arr->data + src_base * itype, source_shape[axis], src_stride, itype);
                break;
            case CSN_CUMSUM:
            case CSN_CUMPROD: {
                bool is_cumsum = mode == CSN_CUMSUM;
                cumsumprod_slice(arr->data + dst_base * itype, source_arr->data + src_base * itype, source_shape[axis], src_stride, is_cumsum, itype);
                break;
            }
            case CSN_SORT:
                sort_slice((double *) temp, arr->data + dst_base, source_arr->data + src_base, source_shape[axis], src_stride);
                break;
            case CSN_ARGSORT:
                argsort_slice((ARRAY_ELEMENT *) temp, arr->data + dst_base, source_arr->data + src_base, source_shape[axis], src_stride);
                break;
        }
    }

done:
    if (temp != NULL) {
        csound->Free(csound, temp);
    }
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_normalize(CSOUND *csound, CSN_UNARYOP_AX *p) {
    return csnarray_unary_ax_helper(csound, &p->h, p->source_handle, (double) *p->axis, (double) *p->order, p->handle, &p->array, CSN_NORMALIZE);
}

int32_t csnarray_normalize_in(CSOUND *csound, CSN_UNARYOP_AX_IN *p) {
    return csnarray_unary_ax_helper(csound, &p->h, p->source_handle, (double) *p->axis, (double) *p->order, NULL, NULL, CSN_NORMALIZE);
}

int32_t csnarray_sort_in(CSOUND *csound, CSN_UNARYOP_AX_IN *p) {
    return csnarray_unary_ax_helper(csound, &p->h, p->source_handle, (double) *p->axis, 0.0, NULL, NULL, CSN_SORT);
}

int32_t csnarray_distance(CSOUND *csound, CSN_BINOP_HH_SCALAR *p) {
    double dist_order = (double) *p->arg_a;
    return csnarray_scalar_helper(csound, p, CSN_DISTANCE, dist_order);
}

int32_t csnarray_pair_distance(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_vec_helper(csound, p, CSN_PAIR_DISTANCE);
}

int32_t csnarray_angle_distance(CSOUND *csound, CSN_BINOP_HH_SCALAR *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

    uint32_t source_handle_a = (uint32_t) p->source_handle_a->id;
    uint32_t source_handle_b = (uint32_t) p->source_handle_b->id;

    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot_a = get_slot(reg, source_handle_a);
    if (slot_a == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle_a);
        goto done;
    }

    CSN_SLOT *slot_b = get_slot(reg, source_handle_b);
    if (slot_b == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle_b);
        goto done;
    }

    CSN_ARRAY *source_arr_a = slot_a->array;
    CSN_ARRAY *source_arr_b = slot_b->array;
    uint32_t *source_shape_a = source_arr_a->shape;
    uint32_t *source_shape_b = source_arr_b->shape;
    size_t source_dim_a = source_arr_a->ndim;
    size_t source_dim_b = source_arr_b->ndim;

    if (source_dim_a != source_dim_b || memcmp(source_shape_a, source_shape_b, sizeof(uint32_t) * CSN_MAX_DIMS) != 0) {
        char abuf[CSN_SHAPE_STR_MAX], bbuf[CSN_SHAPE_STR_MAX];
        res = csound->InitError(csound, "[csnarray] Both arrays must have the same shape, got %s and %s", shape_str(abuf, sizeof(abuf), source_shape_a, (uint32_t) source_dim_a), shape_str(bbuf, sizeof(bbuf), source_shape_b, (uint32_t) source_dim_b));
        goto done;
    }

    if (source_arr_a->itype != source_arr_b->itype) {
        res = csound->InitError(csound, "[csnarray] Element type mismatch: one array is real and the other complex");
        goto done;
    }

    CSN_COMPLEXDAT dot_ab = { 0.0, 0.0 };
    double norm_a = 0.0;
    double norm_b = 0.0;
    for (size_t i = 0; i < source_arr_a->size; i++) {
        CSN_COMPLEXDAT a = item_at(source_arr_a, i);
        CSN_COMPLEXDAT b = item_at(source_arr_b, i);
        CSN_COMPLEXDAT bconj = { b.re, -b.im };
        CSN_COMPLEXDAT t = {0};
        complex_prod(&t, a, bconj);
        complex_add(&dot_ab, dot_ab, t);
        norm_a += a.re * a.re + a.im * a.im;
        norm_b += b.re * b.re + b.im * b.im;
    }

    if (norm_a == 0.0 || norm_b == 0.0) {
        csound->Message(csound, "[csnarray] Angle undefined for zero-length vectors");
        *p->value = (MYFLT) NAN;
        goto done;
    }

    norm_a = sqrt(norm_a);
    norm_b = sqrt(norm_b);

    double dot = source_arr_a->itype == CSN_COMPLEX ? hypot(dot_ab.re, dot_ab.im) : dot_ab.re;
    double c = dot / (norm_a * norm_b);
    c = c > 1.0 ? 1.0 : c;
    c = c < -1.0 ? -1.0 : c;
    double theta = acos(c);

    *p->value = (MYFLT) theta;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_diff(CSOUND *csound, CSN_UNARYOP_AX *p) {
    return csnarray_unary_ax_helper(csound, &p->h, p->source_handle, (double) *p->axis, 0.0, p->handle, &p->array, CSN_DIFF);
}

int32_t csnarray_gradient(CSOUND *csound, CSN_UNARYOP_AX *p) {
    return csnarray_unary_ax_helper(csound, &p->h, p->source_handle, (double) *p->axis, 0.0, p->handle, &p->array, CSN_GRADIENT);
}

int32_t csnarray_cumsum(CSOUND *csound, CSN_UNARYOP_AX *p) {
    return csnarray_unary_ax_helper(csound, &p->h, p->source_handle, (double) *p->axis, 0.0, p->handle, &p->array, CSN_CUMSUM);
}

int32_t csnarray_cumprod(CSOUND *csound, CSN_UNARYOP_AX *p) {
    return csnarray_unary_ax_helper(csound, &p->h, p->source_handle, (double) *p->axis, 0.0, p->handle, &p->array, CSN_CUMPROD);
}

int32_t csnarray_sort(CSOUND *csound, CSN_UNARYOP_AX *p) {
    return csnarray_unary_ax_helper(csound, &p->h, p->source_handle, (double) *p->axis, 0.0, p->handle, &p->array, CSN_SORT);
}

int32_t csnarray_argsort(CSOUND *csound, CSN_UNARYOP_AX *p) {
    return csnarray_unary_ax_helper(csound, &p->h, p->source_handle, (double) *p->axis, 0.0, p->handle, &p->array, CSN_ARGSORT);
}

int32_t csnarray_matmul_scalar(CSOUND *csound, CSN_BINOP_HH_SCALAR *p) {
    return csnarray_scalar_helper(csound, p, CSN_DOT_SCALAR, 0.0);
}

int32_t csnarray_matmul(CSOUND *csound, CSN_BINOP_HH *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

    uint32_t source_handle_a = p->source_handle_a->id;
    uint32_t source_handle_b = p->source_handle_b->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot_a = get_slot(reg, source_handle_a);
    if (source_slot_a == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle_a);
        goto done;
    }

    CSN_SLOT *source_slot_b = get_slot(reg, source_handle_b);
    if (source_slot_b == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle_b);
        goto done;
    }

    CSN_ARRAY *source_arr_a = source_slot_a->array;
    CSN_ARRAY *source_arr_b = source_slot_b->array;

    if (source_arr_a->itype != source_arr_b->itype) {
        res = csound->InitError(csound, "[csnarray] Element type mismatch: one array is real and the other complex");
        goto done;
    }

    uint32_t source_a_dim = source_arr_a->ndim;
    uint32_t source_b_dim = source_arr_b->ndim;

    if (source_a_dim == 1 && source_b_dim == 1) {
        res = csound->InitError(csound, "[csnarray] The matrix product of two 1-D arrays is a scalar; declare the output as i, not as a handle");
        goto done;
    }

    uint32_t a_shape[CSN_MAX_DIMS] = {0};
    uint32_t b_shape[CSN_MAX_DIMS] = {0};
    size_t a_strides[CSN_MAX_DIMS] = {0};
    size_t b_strides[CSN_MAX_DIMS] = {0};
    uint32_t a_dim, b_dim;
    bool a_promoted = source_a_dim == 1;
    bool b_promoted = source_b_dim == 1;

    if (a_promoted) {
        a_dim = 2U;
        a_shape[0] = 1U;
        a_shape[1] = source_arr_a->shape[0];
        a_strides[0] = 0;
        a_strides[1] = source_arr_a->strides[0];
    } else {
        a_dim = source_a_dim;
        memcpy(a_shape, source_arr_a->shape, sizeof(uint32_t) * a_dim);
        memcpy(a_strides, source_arr_a->strides, sizeof(size_t) * a_dim);
    }

    if (b_promoted) {
        b_dim = 2U;
        b_shape[0] = source_arr_b->shape[0];
        b_shape[1] = 1U;
        b_strides[0] = source_arr_b->strides[0];
        b_strides[1] = 0;
    } else {
        b_dim = source_b_dim;
        memcpy(b_shape, source_arr_b->shape, sizeof(uint32_t) * b_dim);
        memcpy(b_strides, source_arr_b->strides, sizeof(size_t) * b_dim);
    }

    uint32_t rows = a_shape[a_dim - 2];
    uint32_t inner = a_shape[a_dim - 1];
    uint32_t cols = b_shape[b_dim - 1];

    if (inner != b_shape[b_dim - 2]) {
        char abuf[CSN_SHAPE_STR_MAX], bbuf[CSN_SHAPE_STR_MAX];
        res = csound->InitError(csound, "[csnarray] Shapes %s and %s are not valid for a matrix product: the last axis of the first (%u) must match the second-to-last of the second (%u)", shape_str(abuf, sizeof(abuf), source_arr_a->shape, source_a_dim), shape_str(bbuf, sizeof(bbuf), source_arr_b->shape, source_b_dim), inner, b_shape[b_dim - 2]);
        goto done;
    }

    uint32_t a_batch_ndim = a_dim - 2;
    uint32_t b_batch_ndim = b_dim - 2;
    uint32_t batch_ndim = a_batch_ndim > b_batch_ndim ? a_batch_ndim : b_batch_ndim;
    uint32_t batch_shape[CSN_MAX_DIMS] = {0};

    for (uint32_t i = 0; i < batch_ndim; ++i) {
        uint32_t ea = i < a_batch_ndim ? a_shape[a_batch_ndim - 1 - i] : 1U;
        uint32_t eb = i < b_batch_ndim ? b_shape[b_batch_ndim - 1 - i] : 1U;
        if (ea != eb && ea != 1U && eb != 1U) {
            char abuf[CSN_SHAPE_STR_MAX], bbuf[CSN_SHAPE_STR_MAX];
            res = csound->InitError(csound, "[csnarray] Batch shapes %s and %s cannot be broadcast together: outside the last two axes, aligned from the right, each pair must match or be 1", shape_str(abuf, sizeof(abuf), source_arr_a->shape, source_a_dim), shape_str(bbuf, sizeof(bbuf), source_arr_b->shape, source_b_dim));
            goto done;
        }
        batch_shape[batch_ndim - 1 - i] = ea > eb ? ea : eb;
    }

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    uint32_t new_ndim = 0;
    for (uint32_t i = 0; i < batch_ndim; ++i) {
        new_shape[new_ndim++] = batch_shape[i];
    }
    if (!a_promoted) new_shape[new_ndim++] = rows;
    if (!b_promoted) new_shape[new_ndim++] = cols;

    if (new_ndim > CSN_MAX_DIMS) {
        res = csound->InitError(csound, "[csnarray] The result would have %u dimensions, the maximum is %d", new_ndim, CSN_MAX_DIMS);
        goto done;
    }

    const uint32_t protect[2] = { source_handle_a, source_handle_b };
    if (create_csnarray_locked(csound, reg, &p->h, new_ndim, new_shape, &p->array, p->handle, protect, 2U, &err, source_arr_a->itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;

    size_t batch_count = 1;
    for (uint32_t i = 0; i < batch_ndim; ++i) {
        batch_count *= batch_shape[i];
    }

    size_t a_row_stride = a_strides[a_dim - 2];
    size_t a_col_stride = a_strides[a_dim - 1];
    size_t b_row_stride = b_strides[b_dim - 2];
    size_t b_col_stride = b_strides[b_dim - 1];

    for (size_t batch = 0; batch < batch_count; ++batch) {
        uint32_t batch_coords[CSN_MAX_DIMS] = {0};
        from_linear_to_coords(batch_coords, batch_shape, batch, batch_ndim);

        size_t a_base = 0;
        for (uint32_t i = 0; i < a_batch_ndim; ++i) {
            uint32_t c = a_shape[i] == 1U ? 0U : batch_coords[batch_ndim - a_batch_ndim + i];
            a_base += (size_t) c * a_strides[i];
        }

        size_t b_base = 0;
        for (uint32_t i = 0; i < b_batch_ndim; ++i) {
            uint32_t c = b_shape[i] == 1U ? 0U : batch_coords[batch_ndim - b_batch_ndim + i];
            b_base += (size_t) c * b_strides[i];
        }

        for (uint32_t r = 0; r < rows; ++r) {
            for (uint32_t c = 0; c < cols; ++c) {
                /* Bilineare, come numpy.matmul: nessun coniugio. */
                CSN_COMPLEXDAT acc = { 0.0, 0.0 };
                for (uint32_t k = 0; k < inner; ++k) {
                    size_t off_a = a_base + (size_t) r * a_row_stride + (size_t) k * a_col_stride;
                    size_t off_b = b_base + (size_t) k * b_row_stride + (size_t) c * b_col_stride;
                    CSN_COMPLEXDAT prod = {0};
                    complex_prod(&prod, item_at(source_arr_a, off_a), item_at(source_arr_b, off_b));
                    complex_add(&acc, acc, prod);
                }
                item_set(arr, (batch * rows + r) * cols + c, acc);
            }
        }
    }

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_trace_impl(CSOUND *csound, CSNREF *src_ref, MYFLT *out_value, COMPLEXDAT *out_complex) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

    uint32_t source_handle = (uint32_t) src_ref->id;
    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot_a = get_slot(reg, source_handle);
    if (slot_a == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = slot_a->array;
    uint32_t dim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    if (dim != 2) {
        res = csound->InitError(csound, "[csnarray] Trace needs a 2-D matrix, got %u-D", dim);
        goto done;
    }

    uint32_t coords[2] = { 0, 0 };
    uint32_t n = source_shape[0] < source_shape[1] ? source_shape[0] : source_shape[1];
    CSN_COMPLEXDAT sum = { 0.0, 0.0 };
    for (uint32_t i = 0; i < n; i++) {
        coords[0] = i;
        coords[1] = i;

        size_t off = from_coords_to_offset(coords, source_arr->strides, 2U);
        complex_add(&sum, sum, item_at(source_arr, off));
    }

    if (source_arr->itype == CSN_COMPLEX) {
        if (out_complex == NULL) {
            res = csound->InitError(csound, "[csnarray] Handle holds a complex array; declare the result as :Complex;");
            goto done;
        }
        out_complex->real = (MYFLT) sum.re;
        out_complex->imag = (MYFLT) sum.im;
        out_complex->isPolar = 0;
    } else {
        if (out_value == NULL) {
            res = csound->InitError(csound, "[csnarray] Handle holds a real array; declare the result as i");
            goto done;
        }
        *out_value = (MYFLT) sum.re;
    }

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_trace(CSOUND *csound, CSN_UNARYOP_SCALAR *p) {
    return csnarray_trace_impl(csound, p->source_handle, p->value, NULL);
}

int32_t csnarray_tracecomp(CSOUND *csound, CSN_UNARYOPCOMPLEX_SCALAR *p) {
    return csnarray_trace_impl(csound, p->source_handle, NULL, p->value);
}

int32_t csnarray_diag(CSOUND *csound, CSN_UNARYOP *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

    uint32_t source_handle = (uint32_t) p->source_handle->id;
    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot_a = get_slot(reg, source_handle);
    if (slot_a == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = slot_a->array;
    uint32_t dim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    if (dim == 0 || dim > 2) {
        res = csound->InitError(csound, "[csnarray] Diag needs a 1-D or 2-D array, got %u-D", dim);
        goto done;
    }

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    uint32_t new_dim = dim == 2U ? 1U : 2U;
    if (dim == 1) {
        new_shape[0] = source_shape[0];
        new_shape[1] = source_shape[0];
    } else {
        new_shape[0] = source_shape[0] < source_shape[1] ? source_shape[0] : source_shape[1];
    }

    const uint32_t protect[1] = { source_handle };
    if (create_csnarray_locked(csound, reg, &p->h, new_dim, new_shape, &p->array, p->handle, protect, 1U, &err, source_arr->itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;

    uint32_t coords[2] = { 0, 0 };

    for (uint32_t i = 0; i < new_shape[0]; i++) {
        coords[0] = i;
        coords[1] = i;
        if (dim == 2) {
            size_t off = from_coords_to_offset(coords, source_arr->strides, 2U);
            item_set(arr, i, item_at(source_arr, off));
        } else {
            size_t off = from_coords_to_offset(coords, arr->strides, 2U);
            item_set(arr, off, item_at(source_arr, i));
        }
    }

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static void movmean_slice(double *dst, const double *src, size_t n, size_t stride, size_t win_size, ITEM_TYPE itype) {
    size_t left = win_size / 2;
    size_t right = win_size - left - 1;

    for (size_t i = 0; i < n; ++i) {
        size_t begin = i >= left ? i - left : 0;
        size_t end = i + right + 1;
        end = end > n ? n : end;
        CSN_COMPLEXDAT acc = { 0.0, 0.0 };
        for (size_t j = begin; j < end; ++j) {
            complex_add(&acc, acc, slice_get(src, j, stride, itype));
        }
        double count = (double) (end - begin);
        acc.re /= count;
        acc.im /= count;
        slice_put(dst, i, stride, itype, acc);
    }
}

static int32_t movstdvar_slice(double *dst, const double *src, size_t n, size_t stride, size_t win_size, CSN_REDUCTION_MODE mode, ITEM_TYPE itype) {
    size_t left = win_size / 2;
    size_t right = win_size - left - 1;

    for (size_t i = 0; i < n; i++) {
        CSN_COMPLEXDAT mean = { 0.0, 0.0 };
        double m_two = 0.0;
        size_t begin = i >= left ? i - left : 0;
        size_t end = i + right + 1;
        end = end > n ? n : end;
        for (size_t j = begin; j < end; ++j) {
            CSN_COMPLEXDAT x = slice_get(src, j, stride, itype);
            CSN_COMPLEXDAT delta = {0};
            complex_sub(&delta, x, mean);
            /* Welford divides by the running count, not the total. */
            double fac = (double) ((j - begin) + 1);
            CSN_COMPLEXDAT step = { delta.re / fac, delta.im / fac };
            complex_add(&mean, mean, step);
            CSN_COMPLEXDAT delta_two = {0};
            complex_sub(&delta_two, x, mean);
            m_two += delta.re * delta_two.re + delta.im * delta_two.im;
        }

        double size = (double) (end - begin);
        if (size == 0.0) return NOTOK;

        double var = m_two / size;
        switch (mode) {
            case RED_VAR:
                dst[i * stride] = var;
                break;
            case RED_STD:
                dst[i * stride] = sqrt(var);
                break;
            default:
                break;
        }
    }

    return OK;
}

static void movminmax_slice(double *dst, const double *src, size_t n, size_t stride, size_t win_size, CSN_REDUCTION_MODE mode) {
    size_t left = win_size / 2;
    size_t right = win_size - left - 1;

    for (size_t i = 0; i < n; i++) {
        double min = DBL_MAX;
        double max = -DBL_MAX;
        size_t begin = i >= left ? i - left : 0;
        size_t end = i + right + 1;
        end = end > n ? n : end;
        for (size_t j = begin; j < end; ++j) {
            double x = src[j * stride];
            switch (mode) {
                case RED_MIN:
                    min = x < min ? x : min;
                    break;
                case RED_MAX:
                    max = x > max ? x : max;
                    break;
                default:
                    break;
            }
        }

        switch (mode) {
            case RED_MIN:
                dst[i * stride] = min;
                break;
            case RED_MAX:
                dst[i * stride] = max;
                break;
            default:
                break;
        }
    }
}

static void movmedian_slice(double *dst, const double *src, double *scratch, size_t n, size_t stride, size_t win_size) {
    size_t left = win_size / 2;
    size_t right = win_size - left - 1;

    for (size_t i = 0; i < n; i++) {
        size_t begin = i >= left ? i - left : 0;
        size_t end = i + right + 1;
        end = end > n ? n : end;
        for (size_t j = begin; j < end; ++j) {
            scratch[j - begin] = src[j * stride];
        }

        size_t buffer_size = end - begin;
        dst[i * stride] = median_of_scratch(scratch, buffer_size);
    }
}

static int32_t csnarray_movstats_helper(CSOUND *csound, CSN_MOVSTATS *p, CSN_MOVSTATS_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

    int32_t res = OK;
    const char *err = NULL;

    if (*p->winsize <= 0.0) {
        return csound->InitError(csound, "[csnarray] Invalid window size");
    }

    double axis_value = (double) *p->axis;
    size_t winsize = (size_t) *p->winsize;

    double *median_buffer = NULL;
    if (mode == CSN_MOVMEDIAN) {
        median_buffer = csound->Calloc(csound, sizeof(double) * winsize);
        if (median_buffer == NULL) {
            return csound->InitError(csound, "[csnarray] Memory allocation failed");
        }
    }

    csound->LockMutex(reg->mutex);

    uint32_t source_handle = p->source_handle->id;
    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = source_slot->array;
    ITEM_TYPE itype = source_arr->itype;

    if (itype == CSN_COMPLEX && (mode == CSN_MOVMIN || mode == CSN_MOVMAX || mode == CSN_MOVMEDIAN)) {
        res = csound->InitError(csound, "[csnarray] Ordering is undefined for complex arrays, so this moving statistic is not available");
        goto done;
    }
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    if (axis_value != -1.0 && !IS_VALID_AXIS(axis_value, source_ndim)) {
        res = csound->InitError(csound, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: -1 for all axes, or finite integers 0..%u)", axis_value, source_ndim, source_ndim - 1);
        goto done;
    }
    int32_t axis = (int32_t) axis_value;

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    uint32_t new_dim = source_ndim;
    memcpy(new_shape, source_shape, sizeof(uint32_t) * CSN_MAX_DIMS);

    if (axis == -1) {
        if (winsize == 0 || winsize > source_arr->size) {
            res = csound->InitError(csound, "[csnarray] Invalid window size");
            goto done;
        }

        memset(new_shape, 0, sizeof(new_shape));
        new_shape[0] = (uint32_t) source_arr->size;
        new_dim = 1U;
    } else {
        if (winsize > source_shape[axis]) {
            res = csound->InitError(csound, "[csnarray] Invalid window size");
            goto done;
        }
    }

    const uint32_t protect[1] = { source_handle };
    ITEM_TYPE out_itype = (mode == CSN_MOVSTD || mode == CSN_MOVVAR) ? CSN_REAL : itype;
    if (create_csnarray_locked(csound, reg, &p->h, new_dim, new_shape, &p->array, p->handle, protect, 1U, &err, out_itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;

    if (axis == -1) {
        switch (mode) {
            case CSN_MOVMEAN:
                movmean_slice(arr->data, source_arr->data, source_arr->size, 1, winsize, itype);
                break;
            case CSN_MOVMEDIAN:
                movmedian_slice(arr->data, source_arr->data, median_buffer, source_arr->size, 1, winsize);
                break;
            case CSN_MOVSTD:
                if (movstdvar_slice(arr->data, source_arr->data, source_arr->size, 1, winsize, RED_STD, itype) != OK) {
                    res = csound->InitError(csound, "[csnarray] Division by zero in movstd");
                    goto done;
                };
                break;
            case CSN_MOVVAR:
                if (movstdvar_slice(arr->data, source_arr->data, source_arr->size, 1, winsize, RED_VAR, itype) != OK) {
                    res = csound->InitError(csound, "[csnarray] Division by zero in movvar");
                    goto done;
                };
                break;
            case CSN_MOVMIN:
                movminmax_slice(arr->data, source_arr->data, source_arr->size, 1, winsize, RED_MIN);
                break;
            case CSN_MOVMAX:
                movminmax_slice(arr->data, source_arr->data, source_arr->size, 1, winsize, RED_MAX);
                break;
        }
        goto done;
    }

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
        size_t dst_base = from_coords_to_offset(src_coords, arr->strides, source_ndim);
        switch (mode) {
            case CSN_MOVMEAN:
                movmean_slice(arr->data + dst_base * itype, source_arr->data + src_base * itype, source_shape[axis], src_stride, winsize, itype);
                break;
            case CSN_MOVMEDIAN:
                movmedian_slice(arr->data + dst_base, source_arr->data + src_base, median_buffer, source_shape[axis], src_stride, winsize);
                break;
            case CSN_MOVSTD:
                if (movstdvar_slice(arr->data + dst_base, source_arr->data + src_base * itype, source_shape[axis], src_stride, winsize, RED_STD, itype) != OK) {
                    res = csound->InitError(csound, "[csnarray] Division by zero in movvar");
                    goto done;
                };
                break;
            case CSN_MOVVAR:
                if (movstdvar_slice(arr->data + dst_base, source_arr->data + src_base * itype, source_shape[axis], src_stride, winsize, RED_VAR, itype) != OK) {
                    res = csound->InitError(csound, "[csnarray] Division by zero in movvar");
                    goto done;
                };
                break;
            case CSN_MOVMIN:
                movminmax_slice(arr->data + dst_base, source_arr->data + src_base, source_shape[axis], src_stride, winsize, RED_MIN);
                break;
            case CSN_MOVMAX:
                movminmax_slice(arr->data + dst_base, source_arr->data + src_base, source_shape[axis], src_stride, winsize, RED_MAX);
                break;
        }
    }

done:
    if (median_buffer != NULL) {
        csound->Free(csound, median_buffer);
    }
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_movmean(CSOUND *csound, CSN_MOVSTATS *p) {
    return csnarray_movstats_helper(csound, p, CSN_MOVMEAN);
}

int32_t csnarray_movmedian(CSOUND *csound, CSN_MOVSTATS *p) {
    return csnarray_movstats_helper(csound, p, CSN_MOVMEDIAN);
}

int32_t csnarray_movstd(CSOUND *csound, CSN_MOVSTATS *p) {
    return csnarray_movstats_helper(csound, p, CSN_MOVSTD);
}

int32_t csnarray_movvar(CSOUND *csound, CSN_MOVSTATS *p) {
    return csnarray_movstats_helper(csound, p, CSN_MOVVAR);
}

int32_t csnarray_movmin(CSOUND *csound, CSN_MOVSTATS *p) {
    return csnarray_movstats_helper(csound, p, CSN_MOVMIN);
}

int32_t csnarray_movmax(CSOUND *csound, CSN_MOVSTATS *p) {
    return csnarray_movstats_helper(csound, p, CSN_MOVMAX);
}

static int32_t dispatch_movstats(double *dst, double *src, size_t size, uint32_t stride, size_t winsize, double *median_buffer, CSN_MOVSTATS_MODE mode, ITEM_TYPE itype) {
    switch (mode) {
        case CSN_MOVMEAN:
            movmean_slice(dst, src, size, stride, winsize, itype);
            break;
        case CSN_MOVMEDIAN:
            movmedian_slice(dst, src, median_buffer, size, stride, winsize);
            break;
        case CSN_MOVSTD:
            if (movstdvar_slice(dst, src, size, stride, winsize, RED_STD, itype) != OK) {
                return NOTOK;
            };
            break;
        case CSN_MOVVAR:
            if (movstdvar_slice(dst, src, size, stride, winsize, RED_VAR, itype) != OK) {
                return NOTOK;
            };
            break;
        case CSN_MOVMIN:
            movminmax_slice(dst, src, size, stride, winsize, RED_MIN);
            break;
        case CSN_MOVMAX:
            movminmax_slice(dst, src, size, stride, winsize, RED_MAX);
            break;
    }
    return OK;
}


static int32_t csnarray_movstats_in_helper(CSOUND *csound, CSN_MOVSTATS_IN *p, CSN_MOVSTATS_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

    int32_t res = OK;

    double axis_value = (double) *p->axis;
    size_t winsize = (size_t) *p->winsize;

    double *median_buffer = NULL;
    if (mode == CSN_MOVMEDIAN) {
        median_buffer = csound->Calloc(csound, sizeof(double) * winsize);
        if (median_buffer == NULL) {
            return csound->InitError(csound, "[csnarray] Memory allocation failed");
        }
    }

    csound->LockMutex(reg->mutex);

    uint32_t source_handle = p->source_handle->id;
    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = source_slot->array;
    ITEM_TYPE itype = source_arr->itype;

    if (itype == CSN_COMPLEX && (mode == CSN_MOVMIN || mode == CSN_MOVMAX || mode == CSN_MOVMEDIAN)) {
        res = csound->InitError(csound, "[csnarray] Ordering is undefined for complex arrays, so this moving statistic is not available");
        goto done;
    }

    if (itype == CSN_COMPLEX && (mode == CSN_MOVSTD || mode == CSN_MOVVAR)) {
        res = csound->InitError(csound, "[csnarray] Moving std and var of a complex array are real, so they cannot be written back in place");
        goto done;
    }
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    if (axis_value != -1.0 && !IS_VALID_AXIS(axis_value, source_ndim)) {
        res = csound->InitError(csound, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: -1 for all axes, or finite integers 0..%u)", axis_value, source_ndim, source_ndim - 1);
        goto done;
    }
    int32_t axis = (int32_t) axis_value;

    if (axis == -1) {
        if (winsize == 0 || winsize > source_arr->size) {
            res = csound->InitError(csound, "[csnarray] Invalid window size");
            goto done;
        }
    } else {
        if (winsize == 0 || winsize > source_shape[axis]) {
            res = csound->InitError(csound, "[csnarray] Invalid window size");
            goto done;
        }
    }

    if (axis == -1) {
        if (dispatch_movstats(source_arr->data, source_arr->data, source_arr->size, 1, winsize, median_buffer, mode, itype) != OK) {
            res = csound->InitError(csound, "[csnarray] Division by zero");
        };
        goto done;
    }

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
        if (dispatch_movstats(source_arr->data + src_base * itype, source_arr->data + src_base * itype, source_shape[axis], src_stride, winsize, median_buffer, mode, itype) != OK) {
            res = csound->InitError(csound, "[csnarray] Division by zero");
            goto done;
        };
    }

done:
    if (median_buffer != NULL) {
        csound->Free(csound, median_buffer);
    }
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_movmean_in(CSOUND *csound, CSN_MOVSTATS_IN *p) {
    return csnarray_movstats_in_helper(csound, p, CSN_MOVMEAN);
}

int32_t csnarray_movmedian_in(CSOUND *csound, CSN_MOVSTATS_IN *p) {
    return csnarray_movstats_in_helper(csound, p, CSN_MOVMEDIAN);
}

int32_t csnarray_movstd_in(CSOUND *csound, CSN_MOVSTATS_IN *p) {
    return csnarray_movstats_in_helper(csound, p, CSN_MOVSTD);
}

int32_t csnarray_movvar_in(CSOUND *csound, CSN_MOVSTATS_IN *p) {
    return csnarray_movstats_in_helper(csound, p, CSN_MOVVAR);
}

int32_t csnarray_movmin_in(CSOUND *csound, CSN_MOVSTATS_IN *p) {
    return csnarray_movstats_in_helper(csound, p, CSN_MOVMIN);
}

int32_t csnarray_movmax_in(CSOUND *csound, CSN_MOVSTATS_IN *p) {
    return csnarray_movstats_in_helper(csound, p, CSN_MOVMAX);
}

int32_t csnarray_complop_unary_helper(CSOUND *csound, CSN_UNARYOP *p, CSN_COMPLEXOP_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

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
    ITEM_TYPE itype = source_arr->itype;

    if (mode == CSN_REAL_TO_COMPLEX) {
        if (itype != CSN_REAL) {
            res = csound->InitError(csound, "[csnarray] Real-to-complex operation requires real array");
            goto done;
        }
    } else if (itype != CSN_COMPLEX) {
        res = csound->InitError(csound, "[csnarray] Operation requires complex array");
        goto done;
    }

    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    uint32_t new_dim = source_ndim;
    memcpy(new_shape, source_shape, sizeof(uint32_t) * CSN_MAX_DIMS);

    ITEM_TYPE out_itype = (mode == CSN_CONJ_PART || mode == CSN_REAL_TO_COMPLEX) ? CSN_COMPLEX : CSN_REAL;

    const uint32_t protect[1] = { source_handle };
    if (create_csnarray_locked(csound, reg, &p->h, new_dim, new_shape, &p->array, p->handle, protect, 1U, &err, out_itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;

    for (size_t i = 0; i < source_arr->size; i ++) {
        switch (mode) {
            case CSN_REAL_PART:
                arr->data[i] = source_arr->data[i * 2];
                break;
            case CSN_IMAG_PART:
                arr->data[i] = source_arr->data[i * 2 + 1];
                break;
            case CSN_REAL_TO_COMPLEX:
                arr->data[i * 2] = source_arr->data[i];
                arr->data[i * 2 + 1] = 0.0;
                break;
            case CSN_CONJ_PART:
                arr->data[i * 2] = source_arr->data[i * 2];
                arr->data[i * 2 + 1] = -source_arr->data[i * 2 + 1];
                break;
            default:
                break;
        }
    }

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_real(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_complop_unary_helper(csound, p, CSN_REAL_PART);
}

int32_t csnarray_imag(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_complop_unary_helper(csound, p, CSN_IMAG_PART);
}

int32_t csnarray_complex_to_real(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_complop_unary_helper(csound, p, CSN_REAL_PART);
}

int32_t csnarray_real_to_complex(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_complop_unary_helper(csound, p, CSN_REAL_TO_COMPLEX);
}

int32_t csnarray_conj(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_complop_unary_helper(csound, p, CSN_CONJ_PART);
}

static inline double wrap_angle(double angle, double period) {
    double half_period = period * 0.5;
    double x = fmod(half_period + angle, period);
    if (x < 0.0)  x += period;
    return x - half_period;
}

static void unwrap_slice(double *dst, const double *src, size_t n, size_t stride, double period, double discont) {
    if (n == 0) return;
    dst[0] = src[0];
    for (size_t i = 1; i < n; ++i) {
        double delta = src[i * stride] - src[(i - 1) * stride];
        double wrapped = wrap_angle(delta, period);
        double correction = fabs(delta) < discont ? 0.0 : wrapped - delta;
        dst[i * stride] = dst[(i - 1) * stride] + delta + correction;
    }
}

static int32_t csnarray_angle_helper(CSOUND *csound, CSN_ANGLE *p, CSN_COMPLEXOP_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

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
    ITEM_TYPE itype = source_arr->itype;

    if (mode == CSN_COMPLEX_TO_ANGLE) {
        if (itype != CSN_COMPLEX) {
            res = csound->InitError(csound, "[csnarray] Complex to angle requires complex array");
            goto done;
        }
    } else if (itype != CSN_REAL) {
        res = csound->InitError(csound, "[csnarray] Angle operations requires real array");
        goto done;
    }

    double period = 0.0;
    double discont = 0.0;
    double axis_value = 0.0;
    switch (mode) {
        case CSN_WRAP:
            period = (double) *p->period;
            break;
        case CSN_UNWRAP:
            period = (double) *p->period;
            discont = (double) *p->discount;
            if (discont <= 0.0) discont = period * 0.5;
            axis_value = (double) *p->axis;
            break;
        default:
            break;
    }

    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    if (axis_value != -1.0 && !IS_VALID_AXIS(axis_value, source_ndim)) {
        res = csound->InitError(csound, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: -1 for all axes, or finite integers 0..%u)", axis_value, source_ndim, source_ndim - 1);
        goto done;
    }
    int32_t axis = (int32_t) axis_value;

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    uint32_t new_dim = source_ndim;
    memcpy(new_shape, source_shape, sizeof(uint32_t) * CSN_MAX_DIMS);

    const uint32_t protect[1] = { source_handle };
    if (create_csnarray_locked(csound, reg, &p->h, new_dim, new_shape, &p->array, p->handle, protect, 1U, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;

    if (mode == CSN_COMPLEX_TO_ANGLE || mode == CSN_WRAP) {
        for (size_t i = 0; i < source_arr->size; i ++) {
            double angle = 0.0;
            if (mode == CSN_COMPLEX_TO_ANGLE) {
                double re = source_arr->data[i * 2];
                double im = source_arr->data[i * 2 + 1];
                angle = atan2(im, re);
            } else {
                double angle_temp = source_arr->data[i];
                angle = wrap_angle(angle_temp, period);
            }
            arr->data[i] = angle;
        }
    } else if (mode == CSN_UNWRAP) {
        if (source_arr->size < 2) {
            res = csound->InitError(csound, "[csnarray] Unwrap needs at least 2 elements, got %zu", source_arr->size);
            goto done;
        }

        if (axis == -1) {
            unwrap_slice(arr->data, source_arr->data, source_arr->size, 1, period, discont);
        } else {
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
                size_t dst_base = from_coords_to_offset(src_coords, arr->strides, source_ndim);
                unwrap_slice(arr->data + dst_base, source_arr->data + src_base, source_shape[axis], src_stride, period, discont);
            }
        }
    }
done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_angle_in_helper(CSOUND *csound, CSN_ANGLE_IN *p, CSN_COMPLEXOP_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

    uint32_t source_handle = p->source_handle->id;
    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = source_slot->array;
    ITEM_TYPE itype = source_arr->itype;

    if (itype == CSN_COMPLEX) {
        res = csound->InitError(csound, "[csnarray] Angle operations requires real array");
        goto done;
    }

    double period = 0.0;
    double discont = 0.0;
    double axis_value = 0.0;
    switch (mode) {
        case CSN_WRAP:
            period = (double) *p->period;
            break;
        case CSN_UNWRAP:
            period = (double) *p->period;
            discont = (double) *p->discount;
            if (discont <= 0.0) discont = period * 0.5;
            axis_value = (double) *p->axis;
            break;
        default:
            break;
    }

    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    if (axis_value != -1.0 && !IS_VALID_AXIS(axis_value, source_ndim)) {
        res = csound->InitError(csound, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: -1 for all axes, or finite integers 0..%u)", axis_value, source_ndim, source_ndim - 1);
        goto done;
    }
    int32_t axis = (int32_t) axis_value;

    if (mode == CSN_WRAP) {
        for (size_t i = 0; i < source_arr->size; i ++) {
            double angle_temp = source_arr->data[i];
            double angle = wrap_angle(angle_temp, period);
            source_arr->data[i] = angle;
        }
    } else if (mode == CSN_UNWRAP) {
        if (source_arr->size < 2) {
            res = csound->InitError(csound, "[csnarray] Unwrap needs at least 2 elements, got %zu", source_arr->size);
            goto done;
        }

        if (axis == -1) {
            unwrap_slice(source_arr->data, source_arr->data, source_arr->size, 1, period, discont);
        } else {
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
                size_t dst_base = from_coords_to_offset(src_coords, source_arr->strides, source_ndim);
                unwrap_slice(source_arr->data + dst_base, source_arr->data + src_base, source_shape[axis], src_stride, period, discont);
            }
        }
    }
done:
    csound->UnlockMutex(reg->mutex);
    return res;
}


int32_t csnarray_angle(CSOUND *csound, CSN_ANGLE *p) {
    return csnarray_angle_helper(csound, p, CSN_COMPLEX_TO_ANGLE);
}

int32_t csnarray_wrap_angle(CSOUND *csound, CSN_ANGLE *p) {
    return csnarray_angle_helper(csound, p, CSN_WRAP);
}

int32_t csnarray_unwrap_angle(CSOUND *csound, CSN_ANGLE *p) {
    return csnarray_angle_helper(csound, p, CSN_UNWRAP);
}

int32_t csnarray_wrap_angle_in(CSOUND *csound, CSN_ANGLE_IN *p) {
    return csnarray_angle_in_helper(csound, p, CSN_WRAP);
}

int32_t csnarray_unwrap_angle_in(CSOUND *csound, CSN_ANGLE_IN *p) {
    return csnarray_angle_in_helper(csound, p, CSN_UNWRAP);
}

int32_t csnarray_type(CSOUND *csound, CSN_UNARYOP_SCALAR *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

    uint32_t source_handle = p->source_handle->id;
    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = source_slot->array;
    ITEM_TYPE itype = source_arr->itype;
    *p->value = itype == CSN_REAL ? FL(0.0) : FL(1.0);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_copy_helper(CSOUND *csound, CSN_UNARYOP *p, bool reverse) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

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
    ITEM_TYPE itype = source_arr->itype;
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    uint32_t new_dim = source_ndim;
    memcpy(new_shape, source_shape, sizeof(uint32_t) * CSN_MAX_DIMS);

    const uint32_t protect[1] = { source_handle };
    if (create_csnarray_locked(csound, reg, &p->h, new_dim, new_shape, &p->array, p->handle, protect, 1U, &err, itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;
    if (!reverse) {
        memcpy(arr->data, source_arr->data, sizeof(double) * source_arr->size * (size_t) itype);
    } else {
        for (size_t i = 0; i < source_arr->size; i++) {
            size_t src = (source_arr->size - 1U - i) * (size_t) itype;
            size_t dst = i * (size_t) itype;
            arr->data[dst] = source_arr->data[src];
            if (itype == CSN_COMPLEX)
                arr->data[dst + 1U] = source_arr->data[src + 1U];
        }
    }

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_copy(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_copy_helper(csound, p, false);
}

int32_t csnarray_reverse(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_copy_helper(csound, p, true);
}

int32_t csnarray_reverse_in(CSOUND *csound, CSN_UNARYOP_IN *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = source_slot->array;
    ITEM_TYPE itype = source_arr->itype;
    for (size_t i = 0; i < source_arr->size / 2U; i++) {
        size_t lo = i * (size_t) itype;
        size_t hi = (source_arr->size - 1U - i) * (size_t) itype;
        double temp_re = source_arr->data[hi];
        source_arr->data[hi] = source_arr->data[lo];
        source_arr->data[lo] = temp_re;
        if (itype == CSN_COMPLEX) {
            double temp_im = source_arr->data[hi + 1U];
            source_arr->data[hi + 1U] = source_arr->data[lo + 1U];
            source_arr->data[lo + 1U] = temp_im;
        }
    }

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static void dispatch_value_for_perquant_reduction(double *value, const double *x, size_t size, bool is_percentile, double q) {
    /* compare_double places NaNs last. NumPy's percentile/quantile propagate
       NaN rather than silently computing from the remaining values. */
    if (isnan(x[size - 1U])) {
        *value = NAN;
        return;
    }

    double qt = is_percentile ? q / 100.0 : q;
    double h = qt * (double)(size - 1U);
    size_t lo = (size_t) floor(h);
    size_t hi = (size_t) ceil(h);
    double f = h - (double) lo;
    *value = x[lo] + f * (x[hi] - x[lo]);
}

static void accumulate_perquant_reduction_axis_helper(double *value, double q, double *buffer, const CSN_ARRAY *source_arr, uint32_t *src_coords, const uint32_t *dst_coords, bool is_percentile, uint32_t axis) {
    for (uint32_t k = 0; k < source_arr->shape[axis]; ++k) {
        for (uint32_t i = 0, j = 0; i < source_arr->ndim; ++i) {
            if (i == axis)
                src_coords[i] = k;
            else
                src_coords[i] = dst_coords[j++];
        }
        size_t off = from_coords_to_offset(src_coords, source_arr->strides, source_arr->ndim);
        buffer[k] = source_arr->data[off];
    }
    qsort(buffer, (size_t) source_arr->shape[axis], sizeof(double), compare_double);
    dispatch_value_for_perquant_reduction(value, buffer, (size_t) source_arr->shape[axis], is_percentile, q);
}

static void accumulate_perquant_reduction_scalar_helper(double *value, double q, double *buffer, const CSN_ARRAY *source_arr, bool is_percentile) {
    memcpy(buffer, source_arr->data, sizeof(double) * source_arr->size);
    qsort(buffer, source_arr->size, sizeof(double), compare_double);
    dispatch_value_for_perquant_reduction(value, buffer, source_arr->size, is_percentile, q);
}

static int32_t csnarray_perquant_reduction(CSOUND *csound, const OPDS *h, CSNREF *src_ref, double axis_value, CSNREF *out_handle, CSN_ARRAY **out_array, MYFLT *out_value, bool is_percentile, double q) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

    if (is_percentile) {
        if (q < 0.0 || q > 100) {
            return csound->InitError(csound, "[csnarray] Percentile must be in the range [0, 100]");
        }
    } else {
        if (q < 0.0 || q > 1.0) {
            return csound->InitError(csound, "[csnarray] Quantile must be in the range [0, 1]");
        }
    }

    uint32_t source_handle = src_ref->id;

    int32_t res = OK;
    const char *err = NULL;
    double *buffer = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = source_slot->array;
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    if (source_arr->size == 0U) {
        res = csound->InitError(csound, "[csnarray] Percentile and quantile are undefined for an empty array");
        goto done;
    }

    if (out_handle != NULL && !IS_VALID_AXIS(axis_value, source_ndim)) {
        res = csound->InitError(csound, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: finite integers 0..%u)", axis_value, source_ndim, source_ndim - 1);
        goto done;
    }
    int32_t axis = (int32_t) axis_value;

    if (source_arr->itype == CSN_COMPLEX) {
        res = csound->InitError(csound, "[csnarray] Percentile and quantile reductions require real arrays");
        goto done;
    }

    if (axis != -1 && source_ndim == 1U) {
        res = csound->InitError(csound, "[csnarray] Reducing a 1-D array produces a scalar; omit the axis argument");
        goto done;
    }

    CSN_ARRAY *arr = NULL;
    if (axis != -1) {
        uint32_t new_shape[CSN_MAX_DIMS] = {0};
        for (uint32_t i = 0, j = 0; i < source_ndim; i++) {
            if (i != (uint32_t) axis) new_shape[j++] = source_shape[i];
        }

        const uint32_t protect[1] = { source_handle };

        if (create_csnarray_locked(csound, reg, h, source_ndim - 1, new_shape, out_array, out_handle, protect, 1U, &err, source_arr->itype) != OK) {
            res = csound->InitError(csound, "[csnarray] %s", err);
            goto done;
        }

        arr = *out_array;
    }

    if (arr != NULL) {
        buffer = csound->Calloc(csound, sizeof(double) * (size_t) source_shape[axis]);
        if (buffer == NULL) {
            res = csound->InitError(csound, "Memory allocation failed");
            goto done;
        }
        for (size_t linear = 0; linear < arr->size; ++linear) {
            uint32_t dst_coords[CSN_MAX_DIMS] = {0};
            uint32_t src_coords[CSN_MAX_DIMS] = {0};

            from_linear_to_coords(dst_coords, arr->shape, linear, arr->ndim);
            double value = 0.0;
            accumulate_perquant_reduction_axis_helper(&value, q, buffer, source_arr, src_coords, dst_coords, is_percentile, (uint32_t) axis);
            arr->data[linear] = value;
        }
    } else {
        buffer = csound->Calloc(csound, sizeof(double) * source_arr->size);
        if (buffer == NULL) {
            res = csound->InitError(csound, "Memory allocation failed");
            goto done;
        }
        double value = 0;
        accumulate_perquant_reduction_scalar_helper(&value, q, buffer, source_arr, is_percentile);
        *out_value = (MYFLT) value;
    }

done:
    if (buffer != NULL) {
        csound->Free(csound, buffer);
    }
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_percentile(CSOUND *csound, CSN_PERCQUANT_AX *p) {
    return csnarray_perquant_reduction(csound, &p->h, p->source_handle, (double) *p->axis, p->handle, &p->array, NULL, true, (double) *p->quantity);
}

int32_t csnarray_percentile_scalar(CSOUND *csound, CSN_PERCQUANT *p) {
    return csnarray_perquant_reduction(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, true, (double) *p->quantity);
}

int32_t csnarray_quantile(CSOUND *csound, CSN_PERCQUANT_AX *p) {
    return csnarray_perquant_reduction(csound, &p->h, p->source_handle, (double) *p->axis, p->handle, &p->array, NULL, false, (double) *p->quantity);
}

int32_t csnarray_quantile_scalar(CSOUND *csound, CSN_PERCQUANT *p) {
    return csnarray_perquant_reduction(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, false, (double) *p->quantity);
}


// --- OENTRY ---

#define S(x) sizeof(x)

static OENTRY localops[] = {
    // REAL-ONLY
    { "csnrand",               S(CSN_ARR_RND_INIT),           0, ":CsnArr;",    "i[]ii",                (SUBR) create_random_csnarray,               NULL,                                   (SUBR) create_csnarray_random_deinit,   NULL, 0 },
    { "csnarange",             S(CSN_SPACED_SPACE),           0, ":CsnArr;",    "iii",                  (SUBR) csnarray_arange,                      NULL,                                   (SUBR) csnarray_space_spaced_deinit,    NULL, 0 },
    { "csnlinspace",           S(CSN_SPACED_SPACE),           0, ":CsnArr;",    "iii",                  (SUBR) csnarray_linspace,                    NULL,                                   (SUBR) csnarray_space_spaced_deinit,    NULL, 0 },
    { "csnlogspace",           S(CSN_SPACED_SPACE),           0, ":CsnArr;",    "iiii",                 (SUBR) csnarray_logspace,                    NULL,                                   (SUBR) csnarray_space_spaced_deinit,    NULL, 0 },
    { "csngeomspace",          S(CSN_SPACED_SPACE),           0, ":CsnArr;",    "iii",                  (SUBR) csnarray_geomspace,                   NULL,                                   (SUBR) csnarray_space_spaced_deinit,    NULL, 0 },
    { "csnclip",               S(CSN_CLIP),                   0, ":CsnArr;",    ":CsnArr;ii",           (SUBR) csnarray_clip,                        NULL,                                   (SUBR) csnarray_clip_deinit,            NULL, 0 },
    { "csnclip.in",            S(CSN_CLIP_IN),                0, "",            ":CsnArr;ii",           (SUBR) csnarray_clip_in,                     NULL,                                   NULL,                                   NULL, 0 },
    { "csnargwhere",           S(CSN_ARGWHERE),               0, ":CsnArr;",    ":CsnArr;:CsnArr;",     (SUBR) csnarray_argwhere,                    NULL,                                   (SUBR) csnarray_argwhere_deinit,        NULL, 0 },
    { "csnargnonzero",         S(CSN_ARGWHERE),               0, ":CsnArr;",    ":CsnArr;",             (SUBR) csnarray_argnonzero,                  NULL,                                   (SUBR) csnarray_argwhere_deinit,        NULL, 0 },
    { "csnargisnan",           S(CSN_ARGWHERE),               0, ":CsnArr;",    ":CsnArr;",             (SUBR) csnarray_argisnan,                    NULL,                                   (SUBR) csnarray_argwhere_deinit,        NULL, 0 },
    { "csnargunique",          S(CSN_ARGWHERE),               0, ":CsnArr;",    ":CsnArr;",             (SUBR) csnarray_argunique,                   NULL,                                   (SUBR) csnarray_argwhere_deinit,        NULL, 0 },
    { "csnunique",             S(CSN_COMPARE),                0, ":CsnArr;",    ":CsnArr;",             (SUBR) csnarray_unique,                      NULL,                                   (SUBR) csnarray_compare_deinit,         NULL, 0 },
    { "csngt",                 S(CSN_COMPARE),                0, ":CsnArr;",    ":CsnArr;i",            (SUBR) csnarray_greater_than,                NULL,                                   (SUBR) csnarray_compare_deinit,         NULL, 0 },
    { "csnlt",                 S(CSN_COMPARE),                0, ":CsnArr;",    ":CsnArr;i",            (SUBR) csnarray_less_than,                   NULL,                                   (SUBR) csnarray_compare_deinit,         NULL, 0 },
    { "csnne",                 S(CSN_COMPARE),                0, ":CsnArr;",    ":CsnArr;i",            (SUBR) csnarray_not_equal,                   NULL,                                   (SUBR) csnarray_compare_deinit,         NULL, 0 },
    { "csnge",                 S(CSN_COMPARE),                0, ":CsnArr;",    ":CsnArr;i",            (SUBR) csnarray_greater_equal,               NULL,                                   (SUBR) csnarray_compare_deinit,         NULL, 0 },
    { "csnle",                 S(CSN_COMPARE),                0, ":CsnArr;",    ":CsnArr;i",            (SUBR) csnarray_less_equal,                  NULL,                                   (SUBR) csnarray_compare_deinit,         NULL, 0 },
    { "csneq",                 S(CSN_COMPARE),                0, ":CsnArr;",    ":CsnArr;i",            (SUBR) csnarray_equal,                       NULL,                                   (SUBR) csnarray_compare_deinit,         NULL, 0 },
    { "csncnteq",              S(CSN_COUNT),                  0, "i",           ":CsnArr;i",            (SUBR) csnarray_count_equal,                 NULL,                                   NULL,                                   NULL, 0 },
    { "csncntnz",              S(CSN_COUNT),                  0, "i",           ":CsnArr;",             (SUBR) csnarray_count_nonzero,               NULL,                                   NULL,                                   NULL, 0 },
    { "csncntnan",             S(CSN_COUNT),                  0, "i",           ":CsnArr;",             (SUBR) csnarray_count_nan,                   NULL,                                   NULL,                                   NULL, 0 },
    { "csnmin",                S(CSN_REDUCTION_SCALAR),       0, "i",           ":CsnArr;",             (SUBR) csnarray_min_all,                     NULL,                                   NULL,                                   NULL, 0 },
    { "csnmax",                S(CSN_REDUCTION_SCALAR),       0, "i",           ":CsnArr;",             (SUBR) csnarray_max_all,                     NULL,                                   NULL,                                   NULL, 0 },
    { "csnmedian",             S(CSN_REDUCTION_SCALAR),       0, "i",           ":CsnArr;",             (SUBR) csnarray_median_all,                  NULL,                                   NULL,                                   NULL, 0 },
    { "csnmin.ax",             S(CSN_REDUCTION),              0, ":CsnArr;",   ":CsnArr;i",             (SUBR) csnarray_min,                         NULL,                                   (SUBR) csnarray_reduction_deinit,       NULL, 0 },
    { "csnmax.ax",             S(CSN_REDUCTION),              0, ":CsnArr;",   ":CsnArr;i",             (SUBR) csnarray_max,                         NULL,                                   (SUBR) csnarray_reduction_deinit,       NULL, 0 },
    { "csnmedian.ax",          S(CSN_REDUCTION),              0, ":CsnArr;",   ":CsnArr;i",             (SUBR) csnarray_median,                      NULL,                                   (SUBR) csnarray_reduction_deinit,       NULL, 0 },
    { "csnargmin",             S(CSN_REDUCTION),              0, ":CsnArr;",   ":CsnArr;j",             (SUBR) csnarray_argmin,                      NULL,                                   (SUBR) csnarray_reduction_deinit,       NULL, 0 },
    { "csnargmax",             S(CSN_REDUCTION),              0, ":CsnArr;",   ":CsnArr;j",             (SUBR) csnarray_argmax,                      NULL,                                   (SUBR) csnarray_reduction_deinit,       NULL, 0 },
    { "csnfloor",              S(CSN_UNARYOP),                0, ":CsnArr;",   ":CsnArr;",              (SUBR) csnarray_floor,                       NULL,                                   (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csnceil",               S(CSN_UNARYOP),                0, ":CsnArr;",   ":CsnArr;",              (SUBR) csnarray_ceil,                        NULL,                                   (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csnround",              S(CSN_UNARYOP),                0, ":CsnArr;",   ":CsnArr;",              (SUBR) csnarray_round,                       NULL,                                   (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csnproject",            S(CSN_BINOP_HH),               0, ":CsnArr;",   ":CsnArr;:CsnArr;",      (SUBR) csnarray_project,                     NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnreject",             S(CSN_BINOP_HH),               0, ":CsnArr;",   ":CsnArr;:CsnArr;",      (SUBR) csnarray_reject,                      NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csncross",              S(CSN_BINOP_HH),               0, ":CsnArr;",   ":CsnArr;:CsnArr;",      (SUBR) csnarray_cross,                       NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csngrad",               S(CSN_UNARYOP_AX),             0, ":CsnArr;",   ":CsnArr;j",             (SUBR) csnarray_gradient,                    NULL,                                   (SUBR) csnarray_opunary_ax_deinit,      NULL, 0 },
    { "csnmovmedian",          S(CSN_MOVSTATS),               0, ":CsnArr;",   ":CsnArr;ij",            (SUBR) csnarray_movmedian,                   NULL,                                   (SUBR) csnarray_movstats_deinit,        NULL, 0 },
    { "csnmovmedian.in",       S(CSN_MOVSTATS_IN),            0, "",           ":CsnArr;ij",            (SUBR) csnarray_movmedian_in,                NULL,                                   NULL,                                   NULL, 0 },
    { "csnmovmin",             S(CSN_MOVSTATS),               0, ":CsnArr;",   ":CsnArr;ij",            (SUBR) csnarray_movmin,                      NULL,                                   (SUBR) csnarray_movstats_deinit,        NULL, 0 },
    { "csnmovmin.in",          S(CSN_MOVSTATS_IN),            0, "",           ":CsnArr;ij",            (SUBR) csnarray_movmin_in,                   NULL,                                   NULL,                                   NULL, 0 },
    { "csnmovmax",             S(CSN_MOVSTATS),               0, ":CsnArr;",   ":CsnArr;ij",            (SUBR) csnarray_movmax,                      NULL,                                   (SUBR) csnarray_movstats_deinit,        NULL, 0 },
    { "csnmovmax.in",          S(CSN_MOVSTATS_IN),            0, "",           ":CsnArr;ij",            (SUBR) csnarray_movmax_in,                   NULL,                                   NULL,                                   NULL, 0 },
    { "csnsort",               S(CSN_UNARYOP_AX),             0, ":CsnArr;",   ":CsnArr;j",             (SUBR) csnarray_sort,                        NULL,                                   (SUBR) csnarray_opunary_ax_deinit,      NULL, 0 },
    { "csnsort.in",            S(CSN_UNARYOP_AX_IN),          0, "",           ":CsnArr;j",             (SUBR) csnarray_sort_in,                     NULL,                                   NULL,                                   NULL, 0 },
    { "csnargsort",            S(CSN_UNARYOP_AX),             0, ":CsnArr;",   ":CsnArr;j",             (SUBR) csnarray_argsort,                     NULL,                                   (SUBR) csnarray_opunary_ax_deinit,      NULL, 0 },
    { "csnpercentile",         S(CSN_PERCQUANT),              0, "i",          ":CsnArr;i",             (SUBR) csnarray_percentile_scalar,           NULL,                                   NULL,                                   NULL, 0 },
    { "csnpercentile.ax",      S(CSN_PERCQUANT_AX),           0, ":CsnArr;",   ":CsnArr;ij",            (SUBR) csnarray_percentile,                  NULL,                                   (SUBR) csnarray_perquant_deinit,        NULL, 0 },
    { "csnquantile",           S(CSN_PERCQUANT),              0, "i",          ":CsnArr;i",             (SUBR) csnarray_quantile_scalar,             NULL,                                   NULL,                                   NULL, 0 },
    { "csnquantile.ax",        S(CSN_PERCQUANT_AX),           0, ":CsnArr;",   ":CsnArr;ij",            (SUBR) csnarray_quantile,                    NULL,                                   (SUBR) csnarray_perquant_deinit,        NULL, 0 },
    { "csnlogicand.hh",        S(CSN_BINOP_HH),               0, ":CsnArr;",   ":CsnArr;:CsnArr;",      (SUBR) csnarray_logical_and_hh,              NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnlogicor.hh",         S(CSN_BINOP_HH),               0, ":CsnArr;",   ":CsnArr;:CsnArr;",      (SUBR) csnarray_logical_or_hh,               NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnlogicand.hs",        S(CSN_BINOP_HS),               0, ":CsnArr;",   ":CsnArr;i",             (SUBR) csnarray_logical_and_hs,              NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnlogicor.hs",         S(CSN_BINOP_HS),               0, ":CsnArr;",   ":CsnArr;i",             (SUBR) csnarray_logical_or_hs,               NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnlogicand.sh",        S(CSN_BINOP_SH),               0, ":CsnArr;",   "i:CsnArr;",             (SUBR) csnarray_logical_and_sh,              NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnlogicor.sh",         S(CSN_BINOP_SH),               0, ":CsnArr;",   "i:CsnArr;",             (SUBR) csnarray_logical_or_sh,               NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnlogicnot",           S(CSN_UNARYOP),                0, ":CsnArr;",   ":CsnArr;",              (SUBR) csnarray_logical_not,                 NULL,                                   (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    // ---
    // REAL AND COMPLEX
    { "csnempty",              S(CSN_ARR_INIT),               0, ":CsnArr;",    "i[]o",                 (SUBR) create_empty_csnarray,                NULL,                                   (SUBR) create_csnarray_deinit,          NULL, 0 },
    { "csnempty.k",            S(CSN_ARR_INIT),               0, ":CsnArr;",    "k[]O",                 (SUBR) create_empty_csnarray_k_init,         (SUBR) create_empty_csnarray_k,         (SUBR) create_csnarray_deinit,          NULL, 0 },
    { "csnzeros",              S(CSN_ARR_INIT),               0, ":CsnArr;",    "i[]o",                 (SUBR) create_zeros_csnarray,                NULL,                                   (SUBR) create_csnarray_deinit,          NULL, 0 },
    { "csnzeros.k",            S(CSN_ARR_INIT),               0, ":CsnArr;",    "k[]O",                 (SUBR) create_zeros_csnarray_k_init,         (SUBR) create_zeros_csnarray_k,         (SUBR) create_csnarray_deinit,          NULL, 0 },
    { "csnones",               S(CSN_ARR_INIT),               0, ":CsnArr;",    "i[]o",                 (SUBR) create_ones_csnarray,                 NULL,                                   (SUBR) create_csnarray_deinit,          NULL, 0 },
    { "csnones.k",             S(CSN_ARR_INIT),               0, ":CsnArr;",    "k[]O",                 (SUBR) create_ones_csnarray_k_init,          (SUBR) create_ones_csnarray_k,          (SUBR) create_csnarray_deinit,          NULL, 0 },
    { "csnfull",               S(CSN_FULL),                   0, ":CsnArr;",    "i[]io",                (SUBR) create_full_csnarray,                 NULL,                                   (SUBR) create_csnarray_full_deinit,     NULL, 0 },
    { "csnfull.c",             S(CSN_FULLCOMPLEX),            0, ":CsnArr;",    "i[]:Complex;p",        (SUBR) create_fullcomp_csnarray,             NULL,                                   (SUBR) create_csnarray_fullcomp_deinit, NULL, 0 },
    { "csnfull.k",             S(CSN_FULL),                   0, ":CsnArr;",    "k[]kO",                (SUBR) create_full_csnarray_k_init,          (SUBR) create_full_csnarray_k,          (SUBR) create_csnarray_full_deinit,     NULL, 0 },
    { "csnfull.c.k",           S(CSN_FULLCOMPLEX),            0, ":CsnArr;",    "k[]:Complex;P",        (SUBR) create_fullcomp_csnarray_k_init,      (SUBR) create_fullcomp_csnarray_k,      (SUBR) create_csnarray_fullcomp_deinit, NULL, 0 },
    { "csnlike",               S(CSN_ARR_INIT_LIKE),          0, ":CsnArr;",    ":CsnArr;i",            (SUBR) create_like_csnarray,                 NULL,                                   (SUBR) create_csnarray_like_deinit,     NULL, 0 },
    { "csnlike.k",             S(CSN_ARR_INIT_LIKE),          0, ":CsnArr;",    ":CsnArr;k",            (SUBR) create_like_csnarray_k_init,          (SUBR) create_like_csnarray_k,          (SUBR) create_csnarray_like_deinit,     NULL, 0 },
    { "csnfromarray",          S(CSN_FROM_ARRAY),             0, ":CsnArr;",    "i[]",                  (SUBR) from_array_to_csnarray,               NULL,                                   (SUBR) from_array_to_csnarray_deinit,   NULL, 0 },
    { "csnfromarray.c",        S(CSN_FROM_ARRAY),             0, ":CsnArr;",    ":Complex;[]",          (SUBR) from_complexarray_to_csnarray_k_init, (SUBR) from_complexarray_to_csnarray_k, (SUBR) from_array_to_csnarray_deinit,   NULL, 0 },
    { "csnfromarray.k",        S(CSN_FROM_ARRAY),             0, ":CsnArr;",    "k[]",                  (SUBR) from_array_to_csnarray_k_init,        (SUBR) from_array_to_csnarray_k,        (SUBR) from_array_to_csnarray_deinit,   NULL, 0 },
    { "csntoarray",            S(CSN_TO_ARRAY),               0, "i[]",         ":CsnArr;",             (SUBR) from_csnarray_to_array,               NULL,                                   NULL,                                   NULL, 0 },
    { "csntoarray.k",          S(CSN_TO_ARRAY),               0, "k[]",         ":CsnArr;",             (SUBR) from_csnarray_to_array,               (SUBR) from_csnarray_to_array_k,        NULL,                                   NULL, 0 },
    { "csntoarray.c",          S(CSN_TO_ARRAY),               0, ":Complex;[]", ":CsnArr;",             (SUBR) from_csnarray_to_complexarray,        (SUBR) from_csnarray_to_complexarray_k, NULL,                                   NULL, 0 },
    { "csnfree",               S(CSN_FREE),                   0, "",            ":CsnArr;",             (SUBR) free_csnarray,                        NULL,                                   NULL,                                   NULL, 0 },
    { "csndims",               S(CSN_SIZE_DIMS),              0, "i",           ":CsnArr;",             (SUBR) csnarray_dims,                        NULL,                                   NULL,                                   NULL, 0 },
    { "csnsize",               S(CSN_SIZE_DIMS),              0, "i",           ":CsnArr;",             (SUBR) csnarray_size,                        NULL,                                   NULL,                                   NULL, 0 },
    { "csnisempty",            S(CSN_SIZE_DIMS),              0, "i",           ":CsnArr;",             (SUBR) csnarray_is_empty,                    NULL,                                   NULL,                                   NULL, 0 },
    { "csnshape",              S(CSN_SHAPE),                  0, "i[]",         ":CsnArr;",             (SUBR) csnarray_shape,                       NULL,                                   NULL,                                   NULL, 0 },
    { "csndims.k",             S(CSN_SIZE_DIMS),              0, "k",           ":CsnArr;",             NULL,                                        (SUBR) csnarray_dims_k,                 NULL,                                   NULL, 0 },
    { "csnsize.k",             S(CSN_SIZE_DIMS),              0, "k",           ":CsnArr;",             NULL,                                        (SUBR) csnarray_size_k,                 NULL,                                   NULL, 0 },
    { "csnisempty.k",          S(CSN_SIZE_DIMS),              0, "k",           ":CsnArr;",             NULL,                                        (SUBR) csnarray_is_empty_k,             NULL,                                   NULL, 0 },
    { "csnshape.k",            S(CSN_SHAPE),                  0, "k[]",         ":CsnArr;",             (SUBR) csnarray_shape,                       (SUBR) csnarray_shape_k,                NULL,                                   NULL, 0 },
    { "csnidentity",           S(CSN_IDENTITY),               0, ":CsnArr;",    "io",                   (SUBR) csnarray_identity,                    NULL,                                   (SUBR) csnarray_identity_deinit,        NULL, 0 },
    { "csnidentity.k",         S(CSN_IDENTITY),               0, ":CsnArr;",    "kO",                   (SUBR) csnarray_identity_k_init,             (SUBR) csnarray_identity_k,             (SUBR) csnarray_identity_deinit,        NULL, 0 },
    { "csnreshape",            S(CSN_RESHAPE),                0, ":CsnArr;",    ":CsnArr;i[]",          (SUBR) csnarray_reshape,                     NULL,                                   (SUBR) csnarray_shape_deinit,           NULL, 0 },
    { "csnreshape.in",         S(CSN_RESHAPE_IN),             0, "",            ":CsnArr;i[]",          (SUBR) csnarray_reshape_in,                  NULL,                                   NULL,                                   NULL, 0 },
    { "csnreshape.k",          S(CSN_RESHAPE),                0, ":CsnArr;",    ":CsnArr;k[]",          (SUBR) csnarray_reshape_k_init,              (SUBR) csnarray_reshape_k,              (SUBR) csnarray_shape_deinit,           NULL, 0 },
    { "csnreshape.in.k",       S(CSN_RESHAPE_IN),             0, "",            ":CsnArr;k[]",          (SUBR) csnarray_reshape_in_k_init,           (SUBR) csnarray_reshape_in_k,           NULL,                                   NULL, 0 },
    { "csnflatten",            S(CSN_RESHAPE),                0, ":CsnArr;",    ":CsnArr;",             (SUBR) csnarray_flatten,                     (SUBR) csnarray_flatten_k,              (SUBR) csnarray_shape_deinit,           NULL, 0 },
    { "csnflatten.in",         S(CSN_RESHAPE_IN),             0, "",            ":CsnArr;",             (SUBR) csnarray_flatten_in,                  (SUBR) csnarray_flatten_in_k,           NULL,                                   NULL, 0 },
    { "csntranspose",          S(CSN_RESHAPE),                0, ":CsnArr;",    ":CsnArr;",             (SUBR) csnarray_transpose,                   (SUBR) csnarray_transpose_k,            (SUBR) csnarray_shape_deinit,           NULL, 0 },
    { "csntranspose.ax",       S(CSN_RESHAPE),                0, ":CsnArr;",    ":CsnArr;i[]",          (SUBR) csnarray_transpose,                   (SUBR) csnarray_transpose_k,            (SUBR) csnarray_shape_deinit,           NULL, 0 },
    { "csntranspose.ax.k",     S(CSN_RESHAPE),                0, ":CsnArr;",    ":CsnArr;k[]",          (SUBR) csnarray_transpose,                   (SUBR) csnarray_transpose_k,            (SUBR) csnarray_shape_deinit,           NULL, 0 },
    { "csntranspose.in",       S(CSN_RESHAPE_IN),             0, "",            ":CsnArr;",             (SUBR) csnarray_transpose_in_k_init,         (SUBR) csnarray_transpose_in_k,         (SUBR) csnarray_transpose_in_k_deinit,  NULL, 0 },
    { "csntranspose.ax.in",    S(CSN_RESHAPE_IN),             0, "",            ":CsnArr;i[]",          (SUBR) csnarray_transpose_in,                NULL,                                   NULL,                                   NULL, 0 },
    { "csntranspose.ax.in.k",  S(CSN_RESHAPE_IN),             0, "",            ":CsnArr;k[]",          (SUBR) csnarray_transpose_in_k_init,         (SUBR) csnarray_transpose_in_k,         (SUBR) csnarray_transpose_in_k_deinit,  NULL, 0 },
    { "csnflip",               S(CSN_FLIP_ROLL),              0, ":CsnArr;",    ":CsnArr;j",            (SUBR) csnarray_flip,                        NULL,                                   (SUBR) csnarray_flip_deinit,            NULL, 0 },
    { "csnflip.in",            S(CSN_FLIP_ROLL_IN),           0, "",            ":CsnArr;j",            (SUBR) csnarray_flip_in,                     NULL,                                   NULL,                                   NULL, 0 },
    { "csnflip.k",             S(CSN_FLIP_ROLL),              0, ":CsnArr;",    ":CsnArr;J",            (SUBR) csnarray_flip,                        (SUBR) csnarray_flip_k,                 (SUBR) csnarray_flip_deinit,            NULL, 0 },
    { "csnflip.in.k",          S(CSN_FLIP_ROLL_IN),           0, "",            ":CsnArr;J",            (SUBR) csnarray_flip_in_k_init,              (SUBR) csnarray_flip_in_k,              (SUBR) csnarray_flip_in_k_deinit,       NULL, 0 },
    { "csnroll",               S(CSN_FLIP_ROLL),              0, ":CsnArr;",    ":CsnArr;i",            (SUBR) csnarray_roll,                        NULL,                                   (SUBR) csnarray_flip_deinit,            NULL, 0 },
    { "csnroll.in",            S(CSN_FLIP_ROLL_IN),           0, "",            ":CsnArr;i",            (SUBR) csnarray_roll_in,                     NULL,                                   NULL,                                   NULL, 0 },
    { "csnroll.ax",            S(CSN_FLIP_ROLL),              0, ":CsnArr;",    ":CsnArr;ij",           (SUBR) csnarray_rollaxis,                    NULL,                                   (SUBR) csnarray_flip_deinit,            NULL, 0 },
    { "csnroll.ax.in",         S(CSN_FLIP_ROLL_IN),           0, "",            ":CsnArr;ij",           (SUBR) csnarray_rollaxis_in,                 NULL,                                   NULL,                                   NULL, 0 },
    { "csnroll.k",             S(CSN_FLIP_ROLL),              0, ":CsnArr;",    ":CsnArr;k",            (SUBR) csnarray_roll,                        (SUBR) csnarray_roll_k,                 (SUBR) csnarray_flip_deinit,            NULL, 0 },
    { "csnroll.in.k",          S(CSN_FLIP_ROLL_IN),           0, "",            ":CsnArr;k",            (SUBR) csnarray_roll_in_k_init,              (SUBR) csnarray_roll_in_k,              (SUBR) csnarray_flip_in_k_deinit,       NULL, 0 },
    { "csnroll.ax.k",          S(CSN_FLIP_ROLL),              0, ":CsnArr;",    ":CsnArr;kJ",           (SUBR) csnarray_rollaxis,                    (SUBR) csnarray_rollaxis_k,             (SUBR) csnarray_flip_deinit,            NULL, 0 },
    { "csnroll.ax.in.k",       S(CSN_FLIP_ROLL_IN),           0, "",            ":CsnArr;kJ",           (SUBR) csnarray_rollaxis_in_k_init,          (SUBR) csnarray_rollaxis_in_k,          (SUBR) csnarray_flip_in_k_deinit,       NULL, 0 },
    { "csnget",                S(CSN_GET),                    0, "i",           ":CsnArr;i[]",          (SUBR) csnarray_get,                         NULL,                                   NULL,                                   NULL, 0 },
    { "csnget.c",              S(CSN_GETCOMPLEX),             0, ":Complex;",   ":CsnArr;i[]",          (SUBR) csnarray_get_complex,                 NULL,                                   NULL,                                   NULL, 0 },
    { "csnget.k",              S(CSN_GET),                    0, "k",           ":CsnArr;k[]",          (SUBR) csnarray_get,                         (SUBR) csnarray_get_k,                  NULL,                                   NULL, 0 },
    { "csnget.c.k",            S(CSN_GETCOMPLEX),             0, ":Complex;",   ":CsnArr;k[]",          (SUBR) csnarray_get_complex,                 (SUBR) csnarray_get_complex_k,          NULL,                                   NULL, 0 },
    { "csnset",                S(CSN_SET),                    0, "",            ":CsnArr;i[]i",         (SUBR) csnarray_set,                         NULL,                                   NULL,                                   NULL, 0 },
    { "csnset.c",              S(CSN_SETCOMPLEX),             0, "",            ":CsnArr;i[]:Complex;", (SUBR) csnarray_set_complex,                 NULL,                                   NULL,                                   NULL, 0 },
    { "csnset.kk",             S(CSN_SET),                    0, "",            ":CsnArr;k[]k",         (SUBR) csnarray_set,                         (SUBR) csnarray_set_k,                  NULL,                                   NULL, 0 },
    { "csnset.c.k",            S(CSN_SETCOMPLEX),             0, "",            ":CsnArr;k[]:Complex;", (SUBR) csnarray_set_complex,                 (SUBR) csnarray_set_complex_k,          NULL,                                   NULL, 0 },
    { "csnset.ik",             S(CSN_SET),                    0, "",            ":CsnArr;i[]k",         (SUBR) csnarray_set,                         (SUBR) csnarray_set_k,                  NULL,                                   NULL, 0 },
    { "csnset.ki",             S(CSN_SET),                    0, "",            ":CsnArr;k[]i",         (SUBR) csnarray_set,                         (SUBR) csnarray_set_k,                  NULL,                                   NULL, 0 },
    { "csntake",               S(CSN_TAKE),                   0, ":CsnArr;",    ":CsnArr;ii",           (SUBR) csnarray_take,                        NULL,                                   (SUBR) csnarray_take_deinit,            NULL, 0 },
    { "csntake.kk",            S(CSN_TAKE),                   0, ":CsnArr;",    ":CsnArr;kk",           (SUBR) csnarray_take,                        (SUBR) csnarray_take_k,                 (SUBR) csnarray_take_deinit,            NULL, 0 },
    { "csntake.ik",            S(CSN_TAKE),                   0, ":CsnArr;",    ":CsnArr;ik",           (SUBR) csnarray_take,                        (SUBR) csnarray_take_k,                 (SUBR) csnarray_take_deinit,            NULL, 0 },
    { "csntake.ki",            S(CSN_TAKE),                   0, ":CsnArr;",    ":CsnArr;ki",           (SUBR) csnarray_take,                        (SUBR) csnarray_take_k,                 (SUBR) csnarray_take_deinit,            NULL, 0 },
    { "csntake.flat",          S(CSN_TAKE_FLAT),              0, "i",           ":CsnArr;i",            (SUBR) csnarray_take_flat,                   NULL,                                   NULL,                                   NULL, 0 },
    { "csntake.flat.c",        S(CSN_TAKECOMPLEX_FLAT),       0, ":Complex;",   ":CsnArr;i",            (SUBR) csnarray_takecomp_flat,               NULL,                                   NULL,                                   NULL, 0 },
    { "csntake.flat.k",        S(CSN_TAKE_FLAT),              0, "k",           ":CsnArr;k",            (SUBR) csnarray_take_flat,                   (SUBR) csnarray_take_flat_k,            NULL,                                   NULL, 0 },
    { "csntake.flat.c.k",      S(CSN_TAKECOMPLEX_FLAT),       0, ":Complex;",   ":CsnArr;k",            (SUBR) csnarray_takecomp_flat,               (SUBR) csnarray_takecomp_flat_k,        NULL,                                   NULL, 0 },
    { "csngetslice",           S(CSN_GET_SLICE),              0, ":CsnArr;",    ":CsnArr;iiii",         (SUBR) csnarray_get_slice,                   NULL,                                   (SUBR) csnarray_slice_deinit,           NULL, 0 },
    { "csngetslice.k",         S(CSN_GET_SLICE),              0, ":CsnArr;",    ":CsnArr;kkkk",         (SUBR) csnarray_get_slice,                   (SUBR) csnarray_get_slice_k,            (SUBR) csnarray_slice_deinit,           NULL, 0 },
    { "csnsetslice",           S(CSN_SET_SLICE),              0, "",            ":CsnArr;:CsnArr;iiii", (SUBR) csnarray_set_slice,                   NULL,                                   NULL,                                   NULL, 0 },
    { "csnsetslice.k",         S(CSN_SET_SLICE),              0, "",            ":CsnArr;:CsnArr;kkkk", (SUBR) csnarray_set_slice,                   (SUBR) csnarray_set_slice_k,            NULL,                                   NULL, 0 },
    { "csnpush",               S(CSN_PUSH),                   0, "",            ":CsnArr;i",            (SUBR) csnarray_push,                        NULL,                                   NULL,                                   NULL, 0 },
    { "csnpush.c",             S(CSN_PUSHCOMPLEX),            0, "",            ":CsnArr;:Complex;",    (SUBR) csnarray_pushcomp,                    NULL,                                   NULL,                                   NULL, 0 },
    { "csnpop",                S(CSN_POP),                    0, "i",           ":CsnArr;",             (SUBR) csnarray_pop,                         NULL,                                   NULL,                                   NULL, 0 },
    { "csnpop.c",              S(CSN_POPCOMPLEX),             0, ":Complex;",   ":CsnArr;",             (SUBR) csnarray_popcomp,                     NULL,                                   NULL,                                   NULL, 0 },
    { "csninsert.flat",        S(CSN_PUSH),                   0, "",            ":CsnArr;ii",           (SUBR) csnarray_insert,                      NULL,                                   NULL,                                   NULL, 0 },
    { "csninsert.flat.c",      S(CSN_PUSHCOMPLEX),            0, "",            ":CsnArr;:Complex;i",   (SUBR) csnarray_insertcomp,                  NULL,                                   NULL,                                   NULL, 0 },
    { "csnremove.flat",        S(CSN_POP),                    0, "i",           ":CsnArr;i",            (SUBR) csnarray_remove,                      NULL,                                   NULL,                                   NULL, 0 },
    { "csnremove.flat.c",      S(CSN_POPCOMPLEX),             0, ":Complex;",   ":CsnArr;i",            (SUBR) csnarray_removecomp,                  NULL,                                   NULL,                                   NULL, 0 },
    { "csninsert.block",       S(CSN_INSERT_BLOCK),           0, "",            ":CsnArr;:CsnArr;ii",   (SUBR) csnarray_insert_block,                NULL,                                   NULL,                                   NULL, 0 },
    { "csnremove.block",       S(CSN_TAKE),                   0, ":CsnArr;",    ":CsnArr;ii",           (SUBR) csnarray_remove_block,                NULL,                                   (SUBR) csnarray_take_deinit,            NULL, 0 },
    { "csnconcat.flat",        S(CSN_CONCAT),                 0, ":CsnArr;",    ":CsnArr;:CsnArr;",     (SUBR) csnarray_concat_flat,                 NULL,                                   (SUBR) csnarray_concat_deinit,          NULL, 0 },
    { "csnconcat.block",       S(CSN_CONCAT),                 0, ":CsnArr;",    ":CsnArr;:CsnArr;i",    (SUBR) csnarray_concat_block,                NULL,                                   (SUBR) csnarray_concat_deinit,          NULL, 0 },
    { "csnpad",                S(CSN_PAD),                    0, ":CsnArr;",    ":CsnArr;iio",          (SUBR) csnarray_pad,                         NULL,                                   (SUBR) csnarray_pad_deinit,             NULL, 0 },
    { "csnpad.ax",             S(CSN_PAD),                    0, ":CsnArr;",    ":CsnArr;iiii",         (SUBR) csnarray_pad,                         NULL,                                   (SUBR) csnarray_pad_deinit,             NULL, 0 },
    { "csnpad.in",             S(CSN_PAD_IN),                 0, "",            ":CsnArr;iio",          (SUBR) csnarray_pad_in,                      NULL,                                   NULL,                                   NULL, 0 },
    { "csnpad.ax.in",          S(CSN_PAD_IN),                 0, "",            ":CsnArr;iiii",         (SUBR) csnarray_pad_in,                      NULL,                                   NULL,                                   NULL, 0 },
    { "csnpad.c",              S(CSN_PADCOMPLEX),             0, ":CsnArr;",    ":CsnArr;ii:Complex;",  (SUBR) csnarray_padcomp,                     NULL,                                   (SUBR) csnarray_padcomp_deinit,         NULL, 0 },
    { "csnpad.ax.c",           S(CSN_PADCOMPLEX),             0, ":CsnArr;",    ":CsnArr;ii:Complex;i", (SUBR) csnarray_padcomp,                     NULL,                                   (SUBR) csnarray_padcomp_deinit,         NULL, 0 },
    { "csnpad.in.c",           S(CSN_PADCOMPLEX_IN),          0, "",            ":CsnArr;ii:Complex;",  (SUBR) csnarray_padcomp_in,                  NULL,                                   NULL,                                   NULL, 0 },
    { "csnpad.ax.in.c",        S(CSN_PADCOMPLEX_IN),          0, "",            ":CsnArr;ii:Complex;i", (SUBR) csnarray_padcomp_in,                  NULL,                                   NULL,                                   NULL, 0 },
    { "csnsum",                S(CSN_REDUCTION_SCALAR),       0, "i",           ":CsnArr;",             (SUBR) csnarray_sum_all,                     NULL,                                   NULL,                                   NULL, 0 },
    { "csnprod",               S(CSN_REDUCTION_SCALAR),       0, "i",           ":CsnArr;",             (SUBR) csnarray_prod_all,                    NULL,                                   NULL,                                   NULL, 0 },
    { "csnsub",                S(CSN_REDUCTION_SCALAR),       0, "i",           ":CsnArr;",             (SUBR) csnarray_sub_all,                     NULL,                                   NULL,                                   NULL, 0 },
    { "csnmean",               S(CSN_REDUCTION_SCALAR),       0, "i",           ":CsnArr;",             (SUBR) csnarray_mean_all,                    NULL,                                   NULL,                                   NULL, 0 },
    { "csnall",                S(CSN_REDUCTION_SCALAR),       0, "i",           ":CsnArr;",             (SUBR) csnarray_all_all,                     NULL,                                   NULL,                                   NULL, 0 },
    { "csnany",                S(CSN_REDUCTION_SCALAR),       0, "i",           ":CsnArr;",             (SUBR) csnarray_any_all,                     NULL,                                   NULL,                                   NULL, 0 },
    { "csnstd",                S(CSN_REDUCTION_SCALAR),       0, "i",           ":CsnArr;",             (SUBR) csnarray_std_all,                     NULL,                                   NULL,                                   NULL, 0 },
    { "csnvar",                S(CSN_REDUCTION_SCALAR),       0, "i",           ":CsnArr;",             (SUBR) csnarray_var_all,                     NULL,                                   NULL,                                   NULL, 0 },
    { "csnsum.c",              S(CSN_REDUCTION_COMPLEX_S),    0, ":Complex;",   ":CsnArr;",             (SUBR) csnarray_sumcomp_all,                 NULL,                                   NULL,                                   NULL, 0 },
    { "csnprod.c",             S(CSN_REDUCTION_COMPLEX_S),    0, ":Complex;",   ":CsnArr;",             (SUBR) csnarray_prodcomp_all,                NULL,                                   NULL,                                   NULL, 0 },
    { "csnsub.c",              S(CSN_REDUCTION_COMPLEX_S),    0, ":Complex;",   ":CsnArr;",             (SUBR) csnarray_subcomp_all,                 NULL,                                   NULL,                                   NULL, 0 },
    { "csnmean.c",             S(CSN_REDUCTION_COMPLEX_S),    0, ":Complex;",   ":CsnArr;",             (SUBR) csnarray_meancomp_all,                NULL,                                   NULL,                                   NULL, 0 },
    { "csnsum.ax",             S(CSN_REDUCTION),              0, ":CsnArr;",    ":CsnArr;i",            (SUBR) csnarray_sum,                         NULL,                                   (SUBR) csnarray_reduction_deinit,       NULL, 0 },
    { "csnprod.ax",            S(CSN_REDUCTION),              0, ":CsnArr;",    ":CsnArr;i",            (SUBR) csnarray_prod,                        NULL,                                   (SUBR) csnarray_reduction_deinit,       NULL, 0 },
    { "csnsub.ax",             S(CSN_REDUCTION),              0, ":CsnArr;",    ":CsnArr;i",            (SUBR) csnarray_sub,                         NULL,                                   (SUBR) csnarray_reduction_deinit,       NULL, 0 },
    { "csnmean.ax",            S(CSN_REDUCTION),              0, ":CsnArr;",    ":CsnArr;i",            (SUBR) csnarray_mean,                        NULL,                                   (SUBR) csnarray_reduction_deinit,       NULL, 0 },
    { "csnany.ax",             S(CSN_REDUCTION),              0, ":CsnArr;",    ":CsnArr;i",            (SUBR) csnarray_any,                         NULL,                                   (SUBR) csnarray_reduction_deinit,       NULL, 0 },
    { "csnall.ax",             S(CSN_REDUCTION),              0, ":CsnArr;",    ":CsnArr;i",            (SUBR) csnarray_all,                         NULL,                                   (SUBR) csnarray_reduction_deinit,       NULL, 0 },
    { "csnstd.ax",             S(CSN_REDUCTION),              0, ":CsnArr;",    ":CsnArr;i",            (SUBR) csnarray_std,                         NULL,                                   (SUBR) csnarray_reduction_deinit,       NULL, 0 },
    { "csnvar.ax",             S(CSN_REDUCTION),              0, ":CsnArr;",    ":CsnArr;i",            (SUBR) csnarray_var,                         NULL,                                   (SUBR) csnarray_reduction_deinit,       NULL, 0 },
    { "csnadd",                S(CSN_BINOP_HH),               0, ":CsnArr;",   ":CsnArr;:CsnArr;",      (SUBR) csnarray_add_hh,                      NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnadd.hs",             S(CSN_BINOP_HS),               0, ":CsnArr;",   ":CsnArr;i",             (SUBR) csnarray_add_hs,                      NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnadd.hs.c",           S(CSN_BINOPCOMPLEX_HS),        0, ":CsnArr;",   ":CsnArr;:Complex;",     (SUBR) csnarray_addcomp_hs,                  NULL,                                   (SUBR) csnarray_opbincomp_deinit,       NULL, 0 },
    { "csnsubtract.hh",        S(CSN_BINOP_HH),               0, ":CsnArr;",   ":CsnArr;:CsnArr;",      (SUBR) csnarray_subtract_hh,                 NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnsubtract.hs",        S(CSN_BINOP_HS),               0, ":CsnArr;",   ":CsnArr;i",             (SUBR) csnarray_subtract_hs,                 NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnsubtract.hs.c",      S(CSN_BINOPCOMPLEX_HS),        0, ":CsnArr;",   ":CsnArr;:Complex;",     (SUBR) csnarray_subtractcomp_hs,             NULL,                                   (SUBR) csnarray_opbincomp_deinit,       NULL, 0 },
    { "csnsubtract.sh",        S(CSN_BINOP_SH),               0, ":CsnArr;",   "i:CsnArr;",             (SUBR) csnarray_subtract_sh,                 NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnsubtract.sh.c",      S(CSN_BINOPCOMPLEX_SH),        0, ":CsnArr;",   ":Complex;:CsnArr;",     (SUBR) csnarray_subtractcomp_sh,             NULL,                                   (SUBR) csnarray_opbincomp_deinit,       NULL, 0 },
    { "csnmul.hh",             S(CSN_BINOP_HH),               0, ":CsnArr;",   ":CsnArr;:CsnArr;",      (SUBR) csnarray_mul_hh,                      NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnmul.hs",             S(CSN_BINOP_HS),               0, ":CsnArr;",   ":CsnArr;i",             (SUBR) csnarray_mul_hs,                      NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnmul.hs.c",           S(CSN_BINOPCOMPLEX_HS),        0, ":CsnArr;",   ":CsnArr;:Complex;",     (SUBR) csnarray_mulcomp_hs,                  NULL,                                   (SUBR) csnarray_opbincomp_deinit,       NULL, 0 },
    { "csndiv.hh",             S(CSN_BINOP_HH),               0, ":CsnArr;",   ":CsnArr;:CsnArr;",      (SUBR) csnarray_div_hh,                      NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csndiv.hs",             S(CSN_BINOP_HS),               0, ":CsnArr;",   ":CsnArr;i",             (SUBR) csnarray_div_hs,                      NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csndiv.sh",             S(CSN_BINOP_SH),               0, ":CsnArr;",   "i:CsnArr;",             (SUBR) csnarray_div_sh,                      NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csndiv.hs.c",           S(CSN_BINOPCOMPLEX_HS),        0, ":CsnArr;",   ":CsnArr;:Complex;",     (SUBR) csnarray_divcomp_hs,                  NULL,                                   (SUBR) csnarray_opbincomp_deinit,       NULL, 0 },
    { "csndiv.sh.c",           S(CSN_BINOPCOMPLEX_SH),        0, ":CsnArr;",   ":Complex;:CsnArr;",     (SUBR) csnarray_divcomp_sh,                  NULL,                                   (SUBR) csnarray_opbincomp_deinit,       NULL, 0 },
    { "csnpow.hh",             S(CSN_BINOP_HH),               0, ":CsnArr;",   ":CsnArr;:CsnArr;",      (SUBR) csnarray_pow_hh,                      NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnpow.hs",             S(CSN_BINOP_HS),               0, ":CsnArr;",   ":CsnArr;i",             (SUBR) csnarray_pow_hs,                      NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnpow.sh",             S(CSN_BINOP_SH),               0, ":CsnArr;",   "i:CsnArr;",             (SUBR) csnarray_pow_sh,                      NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnpow.hs.c",           S(CSN_BINOPCOMPLEX_HS),        0, ":CsnArr;",   ":CsnArr;:Complex;",     (SUBR) csnarray_powcomp_hs,                  NULL,                                   (SUBR) csnarray_opbincomp_deinit,       NULL, 0 },
    { "csnpow.sh.c",           S(CSN_BINOPCOMPLEX_SH),        0, ":CsnArr;",   ":Complex;:CsnArr;",     (SUBR) csnarray_powcomp_sh,                  NULL,                                   (SUBR) csnarray_opbincomp_deinit,       NULL, 0 },
    { "csnlog.hh",             S(CSN_BINOP_HH),               0, ":CsnArr;",   ":CsnArr;:CsnArr;",      (SUBR) csnarray_log_hh,                      NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnlog.hs",             S(CSN_BINOP_HS),               0, ":CsnArr;",   ":CsnArr;i",             (SUBR) csnarray_log_hs,                      NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnlog.sh",             S(CSN_BINOP_SH),               0, ":CsnArr;",   "i:CsnArr;",             (SUBR) csnarray_log_sh,                      NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnlog.hs.c",           S(CSN_BINOPCOMPLEX_HS),        0, ":CsnArr;",   ":CsnArr;:Complex;",     (SUBR) csnarray_logcomp_hs,                  NULL,                                   (SUBR) csnarray_opbincomp_deinit,       NULL, 0 },
    { "csnlog.sh.c",           S(CSN_BINOPCOMPLEX_SH),        0, ":CsnArr;",   ":Complex;:CsnArr;",     (SUBR) csnarray_logcomp_sh,                  NULL,                                   (SUBR) csnarray_opbincomp_deinit,       NULL, 0 },
    { "csnabs",                S(CSN_UNARYOP),                0, ":CsnArr;",   ":CsnArr;",              (SUBR) csnarray_abs,                         NULL,                                   (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csnexp",                S(CSN_UNARYOP),                0, ":CsnArr;",   ":CsnArr;",              (SUBR) csnarray_exp,                         NULL,                                   (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csnsqrt",               S(CSN_UNARYOP),                0, ":CsnArr;",   ":CsnArr;",              (SUBR) csnarray_sqrt,                        NULL,                                   (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csncbrt",               S(CSN_UNARYOP),                0, ":CsnArr;",   ":CsnArr;",              (SUBR) csnarray_cbrt,                        NULL,                                   (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csnsin",                S(CSN_UNARYOP),                0, ":CsnArr;",   ":CsnArr;",              (SUBR) csnarray_sin,                         NULL,                                   (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csncos",                S(CSN_UNARYOP),                0, ":CsnArr;",   ":CsnArr;",              (SUBR) csnarray_cos,                         NULL,                                   (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csntan",                S(CSN_UNARYOP),                0, ":CsnArr;",   ":CsnArr;",              (SUBR) csnarray_tan,                         NULL,                                   (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csnasin",               S(CSN_UNARYOP),                0, ":CsnArr;",   ":CsnArr;",              (SUBR) csnarray_asin,                        NULL,                                   (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csnacos",               S(CSN_UNARYOP),                0, ":CsnArr;",   ":CsnArr;",              (SUBR) csnarray_acos,                        NULL,                                   (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csnatan",               S(CSN_UNARYOP),                0, ":CsnArr;",   ":CsnArr;",              (SUBR) csnarray_atan,                        NULL,                                   (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csnsinh",               S(CSN_UNARYOP),                0, ":CsnArr;",   ":CsnArr;",              (SUBR) csnarray_sinh,                        NULL,                                   (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csncosh",               S(CSN_UNARYOP),                0, ":CsnArr;",   ":CsnArr;",              (SUBR) csnarray_cosh,                        NULL,                                   (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csntanh",               S(CSN_UNARYOP),                0, ":CsnArr;",   ":CsnArr;",              (SUBR) csnarray_tanh,                        NULL,                                   (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csnasinh",              S(CSN_UNARYOP),                0, ":CsnArr;",   ":CsnArr;",              (SUBR) csnarray_asinh,                       NULL,                                   (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csnacosh",              S(CSN_UNARYOP),                0, ":CsnArr;",   ":CsnArr;",              (SUBR) csnarray_acosh,                       NULL,                                   (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csnatanh",              S(CSN_UNARYOP),                0, ":CsnArr;",   ":CsnArr;",              (SUBR) csnarray_atanh,                       NULL,                                   (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csnsign",               S(CSN_UNARYOP),                0, ":CsnArr;",   ":CsnArr;",              (SUBR) csnarray_sign,                        NULL,                                   (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csndot",                S(CSN_BINOP_HH),               0, ":CsnArr;",   ":CsnArr;:CsnArr;",      (SUBR) csnarray_dot,                         NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csndot.s",              S(CSN_BINOP_HH_SCALAR),        0, "i",          ":CsnArr;:CsnArr;",      (SUBR) csnarray_dot_scalar,                  NULL,                                   NULL,                                   NULL, 0 },
    { "csndot.s.c",            S(CSN_BINOPCOMPLEX_HH_SCALAR), 0, ":Complex;",  ":CsnArr;:CsnArr;",      (SUBR) csnarray_dotcomp_scalar,              NULL,                                   NULL,                                   NULL, 0 },
    { "csninner",              S(CSN_BINOP_HH),               0, ":CsnArr;",   ":CsnArr;:CsnArr;",      (SUBR) csnarray_inner,                       NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csninner.s",            S(CSN_BINOP_HH_SCALAR),        0, "i",          ":CsnArr;:CsnArr;",      (SUBR) csnarray_inner_scalar,                NULL,                                   NULL,                                   NULL, 0 },
    { "csninner.s.c",          S(CSN_BINOPCOMPLEX_HH_SCALAR), 0, ":Complex;",  ":CsnArr;:CsnArr;",      (SUBR) csnarray_innercomp_scalar,            NULL,                                   NULL,                                   NULL, 0 },
    { "csnouter",              S(CSN_BINOP_HH),               0, ":CsnArr;",   ":CsnArr;:CsnArr;",      (SUBR) csnarray_outer,                       NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnnorm",               S(CSN_NORM_REDUCTION),         0, ":CsnArr;",   ":CsnArr;ip",            (SUBR) csnarray_norm,                        NULL,                                   (SUBR) csnarray_norm_deinit,            NULL, 0 },
    { "csnnorm.s",             S(CSN_NORM_REDUCTION_SCALAR),  0, "i",          ":CsnArr;p",             (SUBR) csnarray_norm_scalar,                 NULL,                                   NULL,                                   NULL, 0 },
    { "csnnormalize",          S(CSN_UNARYOP_AX),             0, ":CsnArr;",   ":CsnArr;jp",            (SUBR) csnarray_normalize,                   NULL,                                   (SUBR) csnarray_opunary_ax_deinit,      NULL, 0 },
    { "csnnormalize.in",       S(CSN_UNARYOP_AX_IN),          0, "",           ":CsnArr;jp",            (SUBR) csnarray_normalize_in,                NULL,                                   NULL,                                   NULL, 0 },
    { "csnpairdist",           S(CSN_BINOP_HH),               0, ":CsnArr;",   ":CsnArr;:CsnArr;",      (SUBR) csnarray_pair_distance,               NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csndist",               S(CSN_BINOP_HH_SCALAR),        0, "i",          ":CsnArr;:CsnArr;p",     (SUBR) csnarray_distance,                    NULL,                                   NULL,                                   NULL, 0 },
    { "csnangledist",          S(CSN_BINOP_HH_SCALAR),        0, "i",          ":CsnArr;:CsnArr;",      (SUBR) csnarray_angle_distance,              NULL,                                   NULL,                                   NULL, 0 },
    { "csnreflect",            S(CSN_BINOP_HH),               0, ":CsnArr;",   ":CsnArr;:CsnArr;",      (SUBR) csnarray_reflect,                     NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csndiff",               S(CSN_UNARYOP_AX),             0, ":CsnArr;",   ":CsnArr;j",             (SUBR) csnarray_diff,                        NULL,                                   (SUBR) csnarray_opunary_ax_deinit,      NULL, 0 },
    { "csncumsum",             S(CSN_UNARYOP_AX),             0, ":CsnArr;",   ":CsnArr;j",             (SUBR) csnarray_cumsum,                      NULL,                                   (SUBR) csnarray_opunary_ax_deinit,      NULL, 0 },
    { "csncumprod",            S(CSN_UNARYOP_AX),             0, ":CsnArr;",   ":CsnArr;j",             (SUBR) csnarray_cumprod,                     NULL,                                   (SUBR) csnarray_opunary_ax_deinit,      NULL, 0 },
    { "csnmatmul",             S(CSN_BINOP_HH),               0, ":CsnArr;",   ":CsnArr;:CsnArr;",      (SUBR) csnarray_matmul,                      NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnmatmul.s",           S(CSN_BINOP_HH_SCALAR),        0, "i",          ":CsnArr;:CsnArr;",      (SUBR) csnarray_matmul_scalar,               NULL,                                   NULL,                                   NULL, 0 },
    { "csntrace",              S(CSN_UNARYOP_SCALAR),         0, "i",          ":CsnArr;",              (SUBR) csnarray_trace,                       NULL,                                   NULL,                                   NULL, 0 },
    { "csntrace.c",            S(CSN_UNARYOPCOMPLEX_SCALAR),  0, ":Complex;",  ":CsnArr;",              (SUBR) csnarray_tracecomp,                   NULL,                                   NULL,                                   NULL, 0 },
    { "csndiag",               S(CSN_UNARYOP),                0, ":CsnArr;",   ":CsnArr;",              (SUBR) csnarray_diag,                        NULL,                                   (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csnmovmean",            S(CSN_MOVSTATS),               0, ":CsnArr;",   ":CsnArr;ij",            (SUBR) csnarray_movmean,                     NULL,                                   (SUBR) csnarray_movstats_deinit,        NULL, 0 },
    { "csnmovmean.in",         S(CSN_MOVSTATS_IN),            0, "",           ":CsnArr;ij",            (SUBR) csnarray_movmean_in,                  NULL,                                   NULL,                                   NULL, 0 },
    { "csnmovstd",             S(CSN_MOVSTATS),               0, ":CsnArr;",   ":CsnArr;ij",            (SUBR) csnarray_movstd,                      NULL,                                   (SUBR) csnarray_movstats_deinit,        NULL, 0 },
    { "csnmovstd.in",          S(CSN_MOVSTATS_IN),            0, "",           ":CsnArr;ij",            (SUBR) csnarray_movstd_in,                   NULL,                                   NULL,                                   NULL, 0 },
    { "csnmovvar",             S(CSN_MOVSTATS),               0, ":CsnArr;",   ":CsnArr;ij",            (SUBR) csnarray_movvar,                      NULL,                                   (SUBR) csnarray_movstats_deinit,        NULL, 0 },
    { "csnmovvar.in",          S(CSN_MOVSTATS_IN),            0, "",           ":CsnArr;ij",            (SUBR) csnarray_movvar_in,                   NULL,                                   NULL,                                   NULL, 0 },
    { "csnreal",               S(CSN_UNARYOP),                0, ":CsnArr;",   ":CsnArr;",              (SUBR) csnarray_real,                        NULL,                                   (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csnimag",               S(CSN_UNARYOP),                0, ":CsnArr;",   ":CsnArr;",              (SUBR) csnarray_imag,                        NULL,                                   (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csntoreal",             S(CSN_UNARYOP),                0, ":CsnArr;",   ":CsnArr;",              (SUBR) csnarray_complex_to_real,             NULL,                                   (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csntocomplex",          S(CSN_UNARYOP),                0, ":CsnArr;",   ":CsnArr;",              (SUBR) csnarray_real_to_complex,             NULL,                                   (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csnconj",               S(CSN_UNARYOP),                0, ":CsnArr;",   ":CsnArr;",              (SUBR) csnarray_conj,                        NULL,                                   (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csnangle",              S(CSN_ANGLE),                  0, ":CsnArr;",   ":CsnArr;",              (SUBR) csnarray_angle,                       NULL,                                   (SUBR) csnarray_angle_deinit,           NULL, 0 },
    { "csnwrap",               S(CSN_ANGLE),                  0, ":CsnArr;",   ":CsnArr;i",             (SUBR) csnarray_wrap_angle,                  NULL,                                   (SUBR) csnarray_angle_deinit,           NULL, 0 },
    { "csnwrap.in",            S(CSN_ANGLE),                  0, "",           ":CsnArr;i",             (SUBR) csnarray_wrap_angle_in,               NULL,                                   NULL,                                   NULL, 0 },
    { "csnunwrap",             S(CSN_ANGLE),                  0, ":CsnArr;",   ":CsnArr;iij",           (SUBR) csnarray_unwrap_angle,                NULL,                                   (SUBR) csnarray_angle_deinit,           NULL, 0 },
    { "csnunwrap.in",          S(CSN_ANGLE),                  0, "",           ":CsnArr;iij",           (SUBR) csnarray_unwrap_angle_in,             NULL,                                   NULL,                                   NULL, 0 },
    { "csntype",               S(CSN_UNARYOP_SCALAR),         0, "i",          ":CsnArr;",              (SUBR) csnarray_type,                        NULL,                                   NULL,                                   NULL, 0 },
    { "csncopy",               S(CSN_UNARYOP),                0, ":CsnArr;",   ":CsnArr;",              (SUBR) csnarray_copy,                        NULL,                                   (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csnreverse",            S(CSN_UNARYOP),                0, ":CsnArr;",   ":CsnArr;",              (SUBR) csnarray_reverse,                     NULL,                                   (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csnreverse_in",         S(CSN_UNARYOP_IN),             0, "",           ":CsnArr;",              (SUBR) csnarray_reverse_in,                  NULL,                                   NULL,                                   NULL, 0 },
    // ---
};


PUBLIC int32_t csoundModuleCreate(CSOUND *csound) {
    (void) csound;
    return CSOUND_SUCCESS;
}

PUBLIC int32_t csoundModuleInit(CSOUND *csound) {
    if (csound->GetTypePool(csound) == NULL) {
        return CSOUND_SUCCESS;
    }

    if (csn_register_type(csound) != OK) {
        csound->ErrorMsg(csound, "[csn] registrazione del tipo CsnArray fallita\n");
        return CSOUND_ERROR;
    }

    int32_t res = (int32_t) (sizeof(localops) / sizeof(OENTRY)) == 0 ? CSOUND_SUCCESS : CSOUND_ERROR;
    return csound->AppendOpcodes(csound, localops, res);
}

PUBLIC int32_t csoundModuleDestroy(CSOUND *csound) {
    (void) csound;
    return CSOUND_SUCCESS;
}

PUBLIC int32_t csoundModuleInfo(void) {
    return ((CS_VERSION << 16) + (CS_SUBVER << 8) + (int32_t) sizeof(MYFLT));
}
