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


static inline void fill_csnarray(CSN_ARRAY *array, double value) {
    for (size_t i = 0; i < array->size; i++) array->data[i] = value;
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
        if (extent < FL(0.0)) {
            return csound->InitError(csound, "[csnarray] Shape extent %g at position %u is negative; extents must be >= 0", (double) extent, i);
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
    const char **err
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

    if (activate_slot(csound, reg, slot, ndim, shape, handle) != OK) {
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
    CSNREF *p_handle
) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

    const char *err = NULL;

    csound->LockMutex(reg->mutex);
    /* Creation opcodes read no source array, so nothing needs protecting. */
    int32_t res = create_csnarray_locked(csound, reg, h, ndim, shape, p_array, p_handle, NULL, 0, &err);
    csound->UnlockMutex(reg->mutex);

    if (res != OK) {
        return csound->InitError(csound, "[csnarray] %s", err);
    }

    return OK;
}

static int32_t create_csnarray_from_shape(CSOUND *csound, CSN_ARR_INIT *p) {
    uint32_t ndim = 0;
    uint32_t shape[CSN_MAX_DIMS] = {0};

    int32_t res_shape = parse_shape_array(csound, p->shape, &ndim, shape);
    if (res_shape != OK) {
        return res_shape;
    }

    return create_csnarray_init(csound, &p->h, ndim, shape, &p->array, p->handle);
}

int32_t create_empty_csnarray(CSOUND *csound, CSN_ARR_INIT *p) {
    return create_csnarray_from_shape(csound, p);
}

int32_t create_zeros_csnarray(CSOUND *csound, CSN_ARR_INIT *p) {
    int32_t res_init = create_csnarray_from_shape(csound, p);
    if (res_init != OK) {
        return res_init;
    }

    return OK;
}

int32_t create_ones_csnarray(CSOUND *csound, CSN_ARR_INIT *p) {
    int32_t res_init = create_csnarray_from_shape(csound, p);
    if (res_init != OK) {
        return res_init;
    }

    fill_csnarray(p->array, 1.0);
    return OK;
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
    if (create_csnarray_locked(csound, reg, &p->h, ndim, shape, &p->array, p->handle, protect, 1U, &err) != OK) {
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

    int32_t res = create_csnarray_init(csound, &p->h, ndim, shape, &p->array, p->handle);
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

int32_t create_full_csnarray(CSOUND *csound, CSN_ARR_INIT *p) {
    int32_t res_init = create_csnarray_from_shape(csound, p);
    if (res_init != OK) {
        return res_init;
    }

    fill_csnarray(p->array, (double) *p->value);
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
    size_t total_size = 1;

    for (uint32_t i = 0; i < ndim; i++) {
        int32_t size = p->source->sizes[i];
        if (size <= 0) {
            return csound->InitError(csound, "[csnarray] Source extent %u must be >= 1", i);
        }
        shape[i] = (uint32_t) size;
        total_size *= (size_t) size;
    }

    int32_t res_init = create_csnarray_init(csound, &p->h, ndim, shape, &p->array, p->handle);
    if (res_init != OK) {
        return res_init;
    }

    for (size_t i = 0; i < total_size; i++) {
        p->array->data[i] = (double) p->source->data[i];
    }


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

    int32_t res_init = create_csnarray_init(csound, &p->h, 1U, &usize, &p->array, p->handle);
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

    int32_t res_init = create_csnarray_init(csound, &p->h, 1U, &usize, &p->array, p->handle);
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

    int32_t res_init = create_csnarray_init(csound, &p->h, 1U, &usize, &p->array, p->handle);
    if (res_init != OK) {
        return res_init;
    }

    if (num == 1) {
        p->array->data[0] = start;
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

    int32_t res_init = create_csnarray_init(csound, &p->h, 1U, &usize, &p->array, p->handle);
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
int32_t csnarray_identity(CSOUND *csound, CSN_IDENTITY *p) {
    int32_t num = (int32_t) *p->num;
    if (num <= 0) {
        return csound->InitError(csound, "[csnarray] The num argument must be > 0, got %d", num);
    }

    uint32_t shape[CSN_MAX_DIMS] = {0};
    shape[0] = num;
    shape[1] = num;

    int32_t res_init = create_csnarray_init(csound, &p->h, 2U, shape, &p->array, p->handle);
    if (res_init != OK) {
        return res_init;
    }

    for (int32_t i = 0; i < num; i++) {
        int32_t row_offset = i * num;
        for (int32_t j = 0; j < num; j++) {
            if (j == i) {
                p->array->data[row_offset + j] =  1.0;
            }
        }
    }


    return OK;
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

    size_t new_size = 1;
    for (uint32_t i = 0; i < ndim; i++) {
        new_size *= shape[i];
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

    if (create_csnarray_locked(csound, reg, &p->h, ndim, shape, &p->array, p->handle, &source_handle, 1U, &err) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    memcpy(p->array->data, arr->data, sizeof(double) * arr->size);

done:
    csound->UnlockMutex(reg->mutex);
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

    size_t new_size = 1;
    for (uint32_t i = 0; i < ndim; i++) {
        new_size *= shape[i];
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
    if (create_csnarray_locked(csound, reg, &p->h, 1U, shape, &p->array, p->handle, &source_handle, 1U, &err) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    memcpy(p->array->data, arr->data, sizeof(double) * arr->size);

done:
    csound->UnlockMutex(reg->mutex);
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

done:
    csound->UnlockMutex(reg->mutex);
    return res;
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

        bool used[CSN_MAX_DIMS] = {false};

        for (uint32_t i = 0; i < ndim; ++i) {
            uint32_t axis = (uint32_t) p->new_shape->data[i];

            if (axis >= ndim || used[axis]) {
                res = csound->InitError(csound, "[csnarray] Axes argument is not a valid permutation: axis %u is out of range or repeated", axis);
                goto done;
            }

            used[axis] = true;
            axes[i] = axis;
        }
    }

    for (uint32_t i = 0; i < ndim; ++i) {
        shape[i] = arr->shape[axes[i]];
    }

    /* _locked: the registry mutex is already held and is not recursive. */
    if (create_csnarray_locked(csound, reg, &p->h, ndim, shape, &p->array, p->handle, &source_handle, 1U, &err) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *dst = p->array;

    for (size_t linear = 0; linear < arr->size; ++linear) {
        uint32_t dst_coords[CSN_MAX_DIMS] = {0};
        uint32_t src_coords[CSN_MAX_DIMS] = {0};

        from_linear_to_coords(dst_coords, dst->shape, linear, ndim);

        // dst axis i comes from source axis axes[i]
        for (uint32_t i = 0; i < ndim; ++i) {
            src_coords[axes[i]] = dst_coords[i];
        }

        size_t src_index = from_coords_to_offset(src_coords, arr->strides, ndim);
        dst->data[linear] = arr->data[src_index];
    }


done:
    csound->UnlockMutex(reg->mutex);
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

        bool used[CSN_MAX_DIMS] = {false};

        for (uint32_t i = 0; i < ndim; ++i) {
            uint32_t axis = (uint32_t)p->new_shape->data[i];

            if (axis >= ndim || used[axis]) {
                res = csound->InitError(csound, "[csnarray] Axes argument is not a valid permutation: axis %u is out of range or repeated", axis);
                goto done;
            }

            used[axis] = true;
            axes[i] = axis;
        }
    }

    /* Allocated only once the axes are known good, so the rejection paths
       above have nothing to release. */
    data = csound->Calloc(csound, sizeof(double) * arr->size);
    if (data == NULL) {
        res = csound->InitError(csound, "[csnarray] Out of memory: allocation of %zu bytes failed", (size_t) (sizeof(double) * arr->size));
        goto done;
    }

    for (uint32_t i = 0; i < ndim; ++i) {
        shape[i] = arr->shape[axes[i]];
    }

    compute_strides(shape, strides, ndim);

    for (size_t linear = 0; linear < arr->size; ++linear) {
        uint32_t dst_coords[CSN_MAX_DIMS] = {0};
        uint32_t src_coords[CSN_MAX_DIMS] = {0};

        from_linear_to_coords(dst_coords, shape, linear, ndim);

        // dst axis i comes from source axis axes[i]
        for (uint32_t i = 0; i < ndim; ++i) {
            src_coords[axes[i]] = dst_coords[i];
        }

        size_t src_index = from_coords_to_offset(src_coords, arr->strides, ndim);
        data[linear] = arr->data[src_index];
    }

    memcpy(arr->data, data, sizeof(double) * arr->size);

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

    int32_t axis_flip = (int32_t) *p->param_a;
    if (axis_flip < -1 || axis_flip > (int32_t) ndim - 1) {
        res = csound->InitError(csound, "[csnarray] Axis %d is out of range for a %u-D array (valid axes: -1 for all axes, or 0..%u)", axis_flip, ndim, ndim - 1);
        goto done;
    }

    /* _locked: the registry mutex is already held and is not recursive. */
    if (create_csnarray_locked(csound, reg, &p->h, ndim, arr->shape, &p->array, p->handle, &source_handle, 1U, &err) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *dst = p->array;

    for (size_t linear = 0; linear < arr->size; ++linear) {
        uint32_t dst_coords[CSN_MAX_DIMS] = {0};
        uint32_t src_coords[CSN_MAX_DIMS] = {0};

        from_linear_to_coords(dst_coords, dst->shape, linear, ndim);

        // flip coords
        for (uint32_t i = 0; i < ndim; ++i) {
            src_coords[i] = dst_coords[i];
        }

        if (axis_flip == -1) {
            for (uint32_t i = 0; i < ndim; ++i) {
                src_coords[i] = arr->shape[i] - 1 - src_coords[i];
            }
        } else {
            src_coords[axis_flip] = arr->shape[axis_flip] - 1 - src_coords[axis_flip];
        }

        size_t src_index = from_coords_to_offset(src_coords, arr->strides, ndim);
        dst->data[linear] = arr->data[src_index];
    }


done:
    csound->UnlockMutex(reg->mutex);
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

    int32_t axis_flip = (int32_t) *p->param_a;
    if (axis_flip < -1 || axis_flip > (int32_t) ndim - 1) {
        res = csound->InitError(csound, "[csnarray] Axis %d is out of range for a %u-D array (valid axes: -1 for all axes, or 0..%u)", axis_flip, ndim, ndim - 1);
        goto done;
    }

    /* Allocated only once the axes are known good, so the rejection paths
       above have nothing to release. */
    data = csound->Calloc(csound, sizeof(double) * arr->size);
    if (data == NULL) {
        res = csound->InitError(csound, "[csnarray] Out of memory: allocation of %zu bytes failed", (size_t) (sizeof(double) * arr->size));
        goto done;
    }

    for (size_t linear = 0; linear < arr->size; ++linear) {
        uint32_t dst_coords[CSN_MAX_DIMS] = {0};
        uint32_t src_coords[CSN_MAX_DIMS] = {0};

        from_linear_to_coords(dst_coords, arr->shape, linear, ndim);

        // flip coords
        for (uint32_t i = 0; i < ndim; ++i) {
            src_coords[i] = dst_coords[i];
        }

        if (axis_flip == -1) {
            for (uint32_t i = 0; i < ndim; ++i) {
                src_coords[i] = arr->shape[i] - 1 - src_coords[i];
            }
        } else {
            src_coords[axis_flip] = arr->shape[axis_flip] - 1 - src_coords[axis_flip];
        }

        size_t src_index = from_coords_to_offset(src_coords, arr->strides, ndim);
        data[linear] = arr->data[src_index];
    }

    memcpy(arr->data, data, sizeof(double) * arr->size);

done:
    csound->UnlockMutex(reg->mutex);
    if (data != NULL) {
        csound->Free(csound, data);
    }
    return res;
}

static uint32_t wrap_index(int64_t x, uint32_t size) {
    int64_t index = x % (int64_t) size;
    if (index < 0) index += size;
    return (uint32_t) index;
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
    int32_t shift = (int32_t) *p->param_a;

    /* _locked: the registry mutex is already held and is not recursive. */
    if (create_csnarray_locked(csound, reg, &p->h, ndim, arr->shape, &p->array, p->handle, &source_handle, 1U, &err) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *dst = p->array;

    for (size_t linear = 0; linear < arr->size; ++linear) {
        uint32_t src_index = wrap_index((int64_t) linear - shift, (uint32_t) arr->size);
        dst->data[linear] = arr->data[src_index];
    }


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
    int32_t shift = (int32_t) *p->param_a;

    data = csound->Calloc(csound, sizeof(double) * arr->size);
    if (data == NULL) {
        res = csound->InitError(csound, "[csnarray] Out of memory: allocation of %zu bytes failed", (size_t) (sizeof(double) * arr->size));
        goto done;
    }

    for (size_t linear = 0; linear < arr->size; ++linear) {
        uint32_t src_index = wrap_index((int64_t) linear - shift, (uint32_t) arr->size);
        data[linear] = arr->data[src_index];
    }

    memcpy(arr->data, data, sizeof(double) * arr->size);

done:
    csound->UnlockMutex(reg->mutex);
    if (data != NULL) {
        csound->Free(csound, data);
    }
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
    int32_t shift = (int32_t) *p->param_a;
    int32_t axis_roll = (int32_t) *p->param_b;
    /* -1 selects every axis; anything below it would index src_coords[] out
       of bounds in the else branch. */
    if (axis_roll < -1 || axis_roll >= (int32_t) ndim) {
        res = csound->InitError(csound, "[csnarray] Axis %d is out of range for a %u-D array (valid axes: -1 for all axes, or 0..%u)", axis_roll, ndim, ndim - 1);
        goto done;
    }

    /* _locked: the registry mutex is already held and is not recursive. */
    if (create_csnarray_locked(csound, reg, &p->h, ndim, arr->shape, &p->array, p->handle, &source_handle, 1U, &err) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *dst = p->array;

    for (size_t linear = 0; linear < arr->size; ++linear) {
        uint32_t dst_coords[CSN_MAX_DIMS] = {0};
        uint32_t src_coords[CSN_MAX_DIMS] = {0};

        from_linear_to_coords(dst_coords, dst->shape, linear, ndim);

        for (uint32_t i = 0; i < ndim; ++i) {
            src_coords[i] = dst_coords[i];
        }

        if (axis_roll == -1) {
            for (uint32_t i = 0; i < ndim; ++i) {
                src_coords[i] = wrap_index((int64_t) dst_coords[i] - shift, (uint32_t) arr->shape[i]);
            }
        } else {
            src_coords[axis_roll] = wrap_index((int64_t) dst_coords[axis_roll] - shift, (uint32_t) arr->shape[axis_roll]);
        }

        size_t src_index = from_coords_to_offset(src_coords, arr->strides, ndim);
        dst->data[linear] = arr->data[src_index];
    }


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
    int32_t shift = (int32_t) *p->param_a;
    int32_t axis_roll = (int32_t) *p->param_b;
    /* -1 selects every axis; anything below it would index src_coords[] out
       of bounds in the else branch. */
    if (axis_roll < -1 || axis_roll >= (int32_t) ndim) {
        res = csound->InitError(csound, "[csnarray] Axis %d is out of range for a %u-D array (valid axes: -1 for all axes, or 0..%u)", axis_roll, ndim, ndim - 1);
        goto done;
    }

    /* Allocated only once the axes are known good, so the rejection paths
       above have nothing to release. */
    data = csound->Calloc(csound, sizeof(double) * arr->size);
    if (data == NULL) {
        res = csound->InitError(csound, "[csnarray] Out of memory: allocation of %zu bytes failed", (size_t) (sizeof(double) * arr->size));
        goto done;
    }

    for (size_t linear = 0; linear < arr->size; ++linear) {
        uint32_t dst_coords[CSN_MAX_DIMS] = {0};
        uint32_t src_coords[CSN_MAX_DIMS] = {0};

        from_linear_to_coords(dst_coords, arr->shape, linear, ndim);

        for (uint32_t i = 0; i < ndim; ++i) {
            src_coords[i] = dst_coords[i];
        }

        if (axis_roll == -1) {
            for (uint32_t i = 0; i < ndim; ++i) {
                src_coords[i] = wrap_index((int64_t) dst_coords[i] - shift, (uint32_t) arr->shape[i]);
            }
        } else {
            src_coords[axis_roll] = wrap_index((int64_t) dst_coords[axis_roll] - shift, (uint32_t) arr->shape[axis_roll]);
        }

        size_t src_index = from_coords_to_offset(src_coords, arr->strides, ndim);
        data[linear] = arr->data[src_index];
    }

    memcpy(arr->data, data, sizeof(double) * arr->size);

done:
    csound->UnlockMutex(reg->mutex);
    if (data != NULL) {
        csound->Free(csound, data);
    }
    return res;
}


static int32_t get_index_offset(CSOUND *csound, size_t *offset, uint32_t ndim, const CSN_ARRAY *arr, const MYFLT *indexes) {
    size_t temp_offset = 0;
    for (uint32_t i = 0; i < ndim; i++) {
        MYFLT index = indexes[i];

        if (index < 0) {
            return csound->InitError(csound, "[csnarray] Index %g at position %u is negative; indexes must be >= 0", (double) indexes[i], i);
        }

        if ((uint32_t) index >= arr->shape[i]) {
            return csound->InitError(csound, "[csnarray] Index %u at position %u is out of range for extent %u (valid: 0..%u)", (uint32_t) index, i, arr->shape[i], arr->shape[i] - 1);
        }

        temp_offset += arr->strides[i] * (size_t) index;
    }

    *offset = temp_offset;
    return OK;
}

static int32_t csnarray_get_set_locked(CSOUND *csound, CSN_REGISTRY *reg, uint32_t handle, ARRAYDAT *indexes, MYFLT *value, bool is_get) {
    size_t offset;
    CSN_SLOT *slot = get_slot(reg, handle);
    if (slot == NULL) {
        return csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", handle);
    }

    if (value == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: null value pointer passed to the element accessor");
    }

    CSN_ARRAY *arr = slot->array;
    uint32_t ndim = arr->ndim;

    if (indexes == NULL || indexes->data == NULL || indexes->sizes == NULL || indexes->dimensions != 1 || indexes->sizes[0] != (int32_t) ndim) {
        return csound->InitError(csound, "[csnarray] Index argument must be a 1-D array of exactly %u elements, one per dimension", ndim);
    }

    int32_t res = get_index_offset(csound, &offset, ndim, arr, indexes->data);
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

int32_t csnarray_get(CSOUND *csound, CSN_GET *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

    uint32_t handle = p->source_handle->id;

    csound->LockMutex(reg->mutex);
    int32_t res = csnarray_get_set_locked(csound, reg, handle, p->indexes, p->value, true);
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
    int32_t res = csnarray_get_set_locked(csound, reg, handle, p->indexes, p->value, false);
    csound->UnlockMutex(reg->mutex);
    return res;
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

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
        goto done;
    }

    CSN_ARRAY *arr = slot->array;
    uint32_t ndim = arr->ndim;
    uint32_t shape[CSN_MAX_DIMS] = {0};

    /* Dropping the only axis would leave a rank-0 array, which the registry
       cannot represent. That case is the two-argument form, which yields a
       plain scalar. */
    if (ndim < 2) {
        res = csound->InitError(csound, "[csnarray] Take along an axis needs a 2-D or higher array; use the two-argument form for a scalar");
        goto done;
    }

    uint32_t axis = (uint32_t) *p->axis;
    if (*p->axis < 0 || axis > ndim - 1) {
        res = csound->InitError(csound, "[csnarray] Axis %d is out of range for a %u-D array (valid axes: 0..%u)", (int32_t) *p->axis, ndim, ndim - 1);
        goto done;
    }

    uint32_t index = (uint32_t) *p->index;
    if (*p->index < 0 || index >= arr->shape[axis]) {
        res = csound->InitError(csound, "[csnarray] Index %d is out of range for axis %u of extent %u (valid: 0..%u)", (int32_t) *p->index, axis, arr->shape[axis], arr->shape[axis] - 1);
        goto done;
    }

    // remove axis passed for new shape
    uint32_t out_ndim = ndim - 1;
    for (uint32_t i = 0, j = 0; i < ndim; i++) {
        if (i == axis) continue;
        shape[j++] = arr->shape[i];
    }

    /* _locked: the registry mutex is already held and is not recursive. */
    if (create_csnarray_locked(csound, reg, &p->h, out_ndim, shape, &p->array, p->handle, &source_handle, 1U, &err) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *dst = p->array;

    /* Walk the destination, which is smaller than the source by exactly the
       extent of the dropped axis. */
    for (size_t linear = 0; linear < dst->size; ++linear) {
        uint32_t dst_coords[CSN_MAX_DIMS] = {0};
        uint32_t src_coords[CSN_MAX_DIMS] = {0};

        from_linear_to_coords(dst_coords, dst->shape, linear, out_ndim);

        uint32_t k = 0;
        for (uint32_t i = 0; i < ndim; ++i) {
            src_coords[i] = (i == axis) ? index : dst_coords[k++];
        }

        size_t src_index = from_coords_to_offset(src_coords, arr->strides, ndim);
        dst->data[linear] = arr->data[src_index];
    }


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

    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, p->source_handle->id);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) p->source_handle->id);
        goto done;
    }

    CSN_ARRAY *arr = slot->array;

    if (*p->index < 0 || (size_t) *p->index >= arr->size) {
        res = csound->InitError(csound, "[csnarray] Index %d is out of range for an array of %zu elements (valid: 0..%zu)", (int32_t) *p->index, arr->size, arr->size - 1);
        goto done;
    }

    *p->value = (MYFLT) arr->data[(size_t) *p->index];

done:
    csound->UnlockMutex(reg->mutex);
    return res;
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

    uint32_t axis = (uint32_t) *p->axis;
    if (*p->axis < 0 || axis > ndim - 1) {
        res = csound->InitError(csound, "[csnarray] Axis %d is out of range for a %u-D array (valid axes: 0..%u)", (int32_t) *p->axis, ndim, ndim - 1);
        goto done;
    }

    uint32_t start = (uint32_t) *p->start;
    uint32_t stop = (uint32_t) *p->stop;
    uint32_t step = (uint32_t) *p->step;
    if (*p->start < 0 || start >= arr->shape[axis] || *p->stop < 0 || stop > arr->shape[axis] || stop <= start || step == 0) {
        res = csound->InitError(csound, "[csnarray] Invalid slice start=%d stop=%d step=%d on axis %u of extent %u: need 0 <= start < stop <= %u and step > 0", (int32_t) *p->start, (int32_t) *p->stop, (int32_t) *p->step, axis, arr->shape[axis], arr->shape[axis]);
        goto done;
    }

    uint32_t new_size = (stop - start + step - 1) / step;
    for (uint32_t i = 0; i < ndim; i++) {
        shape[i] = i == axis ? new_size : arr->shape[i];
    }

    /* _locked: the registry mutex is already held and is not recursive. */
    if (create_csnarray_locked(csound, reg, &p->h, ndim, shape, &p->array, p->handle, &source_handle, 1U, &err) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *dst = p->array;

    for (size_t linear = 0; linear < arr->size; ++linear) {
        uint32_t dst_coords[CSN_MAX_DIMS] = {0};
        uint32_t src_coords[CSN_MAX_DIMS] = {0};

        from_linear_to_coords(dst_coords, dst->shape, linear, ndim);

        for (uint32_t i = 0; i < ndim; ++i) {
            src_coords[i] = dst_coords[i];
        }

        src_coords[axis] = start + dst_coords[axis] * step;

        size_t src_index = from_coords_to_offset(src_coords, arr->strides, ndim);
        dst->data[linear] = arr->data[src_index];
    }


done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_set_slice(CSOUND *csound, CSN_SET_SLICE *p) {
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

    uint32_t source_ndim = source_arr->ndim;
    uint32_t data_ndim = data_arr->ndim;

    uint32_t slice_shape[CSN_MAX_DIMS] = {0};

    uint32_t axis = (uint32_t) *p->axis;
    if (*p->axis < 0 || axis > source_ndim - 1) {
        res = csound->InitError(csound, "[csnarray] Axis %d is out of range for a %u-D array (valid axes: 0..%u)", (int32_t) *p->axis, source_ndim, source_ndim - 1);
        goto done;
    }

    uint32_t start = (uint32_t) *p->start;
    uint32_t stop = (uint32_t) *p->stop;
    uint32_t step = (uint32_t) *p->step;
    if (*p->start < 0 || start >= source_arr->shape[axis] || *p->stop < 0 || stop > source_arr->shape[axis] || stop <= start || step == 0) {
        res = csound->InitError(csound, "[csnarray] Invalid slice start=%d stop=%d step=%d on axis %u of extent %u: need 0 <= start < stop <= %u and step > 0", (int32_t) *p->start, (int32_t) *p->stop, (int32_t) *p->step, axis, source_arr->shape[axis], source_arr->shape[axis]);
        goto done;
    }

    uint32_t new_size = (stop - start + step - 1) / step;
    uint32_t slice_size = 1;
    for (uint32_t i = 0; i < source_ndim; i++) {
        uint32_t size = i == axis ? new_size : source_arr->shape[i];
        slice_shape[i] = size;
        slice_size *= size;
        if (data_arr->shape[i] != slice_shape[i]) {
            res = csound->InitError(csound, "[csnarray] Data block extent %u on axis %u does not match the slice extent %u", data_arr->shape[i], i, slice_shape[i]);
            goto done;
        }
    }

    if (data_arr->size != slice_size) {
        char sbuf[CSN_SHAPE_STR_MAX];
        res = csound->InitError(csound, "[csnarray] Data block holds %zu elements but the slice %s holds %u", data_arr->size, shape_str(sbuf, sizeof(sbuf), slice_shape, source_ndim), slice_size);
        goto done;
    }

    for (size_t linear = 0; linear < data_arr->size; ++linear) {
        uint32_t dst_coords[CSN_MAX_DIMS] = {0};
        uint32_t src_coords[CSN_MAX_DIMS] = {0};

        from_linear_to_coords(dst_coords, slice_shape, linear, data_ndim);

        for (uint32_t i = 0; i < source_ndim; ++i) {
            src_coords[i] = dst_coords[i];
        }

        src_coords[axis] = start + dst_coords[axis] * step;

        size_t src_index = from_coords_to_offset(src_coords, source_arr->strides, source_ndim);
        source_arr->data[src_index] = data_arr->data[linear];
    }

done:
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
    memmove(arr->data + (size_t) index + 1, arr->data + (size_t) index, sizeof(double) * count);
    arr->data[index] = (double) *p->in_value;
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

    uint32_t source_ndim = source_arr->ndim;
    uint32_t data_ndim = data_arr->ndim;

    if (data_ndim != source_ndim - 1) {
        res = csound->InitError(csound, "[csnarray] Block is %u-D but inserting into a %u-D array needs a %u-D block", data_ndim, source_ndim, source_ndim - 1);
        goto done;
    }

    uint32_t axis = (uint32_t) *p->axis;
    if (*p->axis < 0 || axis > source_ndim - 1) {
        res = csound->InitError(csound, "[csnarray] Axis %d is out of range for a %u-D array (valid axes: 0..%u)", (int32_t) *p->axis, source_ndim, source_ndim - 1);
        goto done;
    }

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

    int32_t alloc_temp = allocate_array(csound, temp, source_ndim, temp_shape, source_arr->array_id);
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
            temp->data[linear] = data_arr->data[block_off];
        } else {
            memcpy(src_coords, dst_coords, sizeof(uint32_t) * source_ndim);
            if (dst_coords[axis] > index) src_coords[axis]--;
            size_t source_off = from_coords_to_offset(src_coords, source_arr->strides, source_arr->ndim);
            temp->data[linear] = source_arr->data[source_off];
        }
    }

    double *new_data = csound->ReAlloc(csound, source_arr->data, sizeof(double) * temp->capacity);
    if (new_data == NULL) {
        csound->Free(csound, temp->data);
        csound->Free(csound, temp);
        res = csound->InitError(csound, "[csnarray] Out of memory: allocation of %zu bytes failed", (size_t) (sizeof(double) * temp->capacity));
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

    uint32_t axis = (uint32_t) *p->axis;
    if (*p->axis < 0 || axis > source_ndim - 1 || source_arr->shape[axis] <= 1) {
        res = csound->InitError(csound, "[csnarray] Axis %d is not usable here: it must be in 0..%u and have extent > 1 (current extent %u)", (int32_t) *p->axis, source_ndim - 1, (axis < source_ndim ? source_arr->shape[axis] : 0U));
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

    if (create_csnarray_locked(csound, reg, &p->h, source_ndim, temp_shape, &p->array, p->handle, &source_handle, 1U, &err) != OK) {
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
        arr->data[linear] = source_arr->data[source_off];
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

    if (create_csnarray_locked(csound, reg, &p->h, 1U, new_shape, &p->array, p->handle, protect, 2U, &err) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;

    for (size_t linear = 0; linear < arr->size; ++linear) {
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

    uint32_t source_ndim = source_arr->ndim;
    uint32_t data_ndim = data_arr->ndim;

    if (data_ndim != source_ndim) {
        res = csound->InitError(csound, "[csnarray] Arrays must have the same number of dimensions, got %u-D and %u-D", source_ndim, data_ndim);
        goto done;
    }

    uint32_t *source_shape = source_arr->shape;
    uint32_t *data_shape = data_arr->shape;

    uint32_t axis = (uint32_t) *p->axis;
    if (*p->axis < 0 || axis > source_ndim - 1) {
        res = csound->InitError(csound, "[csnarray] Axis %d is out of range for a %u-D array (valid axes: 0..%u)", (int32_t) *p->axis, source_ndim, source_ndim - 1);
        goto done;
    }

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

    if (create_csnarray_locked(csound, reg, &p->h, source_ndim, new_shape, &p->array, p->handle, protect, 2U, &err) != OK) {
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
            arr->data[linear] = source_arr->data[source_off];
        } else {
            memcpy(src_coords, dst_coords, sizeof(uint32_t) * source_ndim);
            src_coords[axis] -= source_shape[axis];
            size_t block_off = from_coords_to_offset(src_coords, data_arr->strides, data_arr->ndim);
            arr->data[linear] = data_arr->data[block_off];
        }
    }

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_pad(CSOUND *csound, CSN_PAD *p) {
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

    if (*p->before < 0 || *p->after < 0) {
        res = csound->InitError(csound, "[csnarray] Pad widths must be >= 0, got before=%g and after=%g", (double) *p->before, (double) *p->after);
        goto done;
    }

    uint32_t before = (uint32_t) *p->before;
    uint32_t after = (uint32_t) *p->after;

    int32_t axis = -1;
    if (p->INOCOUNT > 4) {
        axis = (int32_t) *p->axis;
        if (*p->axis < 0 || (uint32_t) axis > source_ndim - 1) {
            res = csound->InitError(csound, "[csnarray] Axis %d is out of range for a %u-D array (valid axes: 0..%u)", (int32_t) *p->axis, source_ndim, source_ndim - 1);
            goto done;
        }
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

    if (create_csnarray_locked(csound, reg, &p->h, source_ndim, new_shape, &p->array, p->handle, protect, 1U, &err) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;

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
            arr->data[linear] = (double) *p->value;
            continue;
        }

        size_t source_off = from_coords_to_offset(src_coords, source_arr->strides, source_arr->ndim);
        arr->data[linear] = source_arr->data[source_off];
    }

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_pad_in(CSOUND *csound, CSN_PAD_IN *p) {
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
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    if (*p->before < 0 || *p->after < 0) {
        res = csound->InitError(csound, "[csnarray] Pad widths must be >= 0, got before=%g and after=%g", (double) *p->before, (double) *p->after);
        goto done;
    }

    uint32_t before = (uint32_t) *p->before;
    uint32_t after = (uint32_t) *p->after;

    int32_t axis = -1;
    if (p->INOCOUNT > 4) {
        axis = (int32_t) *p->axis;
        if (*p->axis < 0 || (uint32_t) axis > source_ndim - 1) {
            res = csound->InitError(csound, "[csnarray] Axis %d is out of range for a %u-D array (valid axes: 0..%u)", (int32_t) *p->axis, source_ndim, source_ndim - 1);
            goto done;
        }
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

    int32_t alloc_temp = allocate_array(csound, temp, source_ndim, new_shape, source_arr->array_id);
    if (alloc_temp != OK) {
        char tbuf[CSN_SHAPE_STR_MAX];
        csound->Free(csound, temp);
        res = csound->InitError(csound, "[csnarray] Out of memory: could not allocate the %u-D temporary array %s", source_ndim, shape_str(tbuf, sizeof(tbuf), new_shape, source_ndim));
        goto done;
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
            temp->data[linear] = (double) *p->value;
            continue;
        }

        size_t source_off = from_coords_to_offset(src_coords, source_arr->strides, source_arr->ndim);
        temp->data[linear] = source_arr->data[source_off];
    }

    double *new_data = csound->ReAlloc(csound, source_arr->data, sizeof(double) * temp->capacity);
    if (new_data == NULL) {
        csound->Free(csound, temp->data);
        csound->Free(csound, temp);
        res = csound->InitError(csound, "[csnarray] Out of memory: allocation of %zu bytes failed", (size_t) (sizeof(double) * temp->capacity));
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

    const uint32_t protect[1] = { source_handle };

    if (create_csnarray_locked(csound, reg, &p->h, source_arr->ndim, source_arr->shape, &p->array, p->handle, protect, 1U, &err) != OK) {
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

    clip_value((double) *p->min, (double) *p->max, source_slot->array);

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
        case GREATER_THAN: return value > cmp_value;
        case LESS_THAN:    return value < cmp_value;
        case EQUAL:        return value == cmp_value;
        case NOT_EQUAL:    return value != cmp_value;
        case NONZERO:      return value != 0.0;
        case IS_NAN:       return isnan(value) != 0;
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
    if (create_csnarray_locked(csound, reg, &p->h, 2U, new_shape, &p->array, p->handle, protect, 2U, &err) != OK) {
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
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    size_t count = count_elements_from_value(source_arr, 0.0, mode);
    uint32_t new_shape[2] = { (uint32_t) count, source_arr->ndim };

    const uint32_t protect[1] = { source_handle };
    if (create_csnarray_locked(csound, reg, &p->h, 2U, new_shape, &p->array, p->handle, protect, 1U, &err) != OK) {
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
    if (create_csnarray_locked(csound, reg, &p->h, 2U, new_shape, &p->array, p->handle, protect, 1U, &err) != OK) {
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
    if (create_csnarray_locked(csound, reg, &p->h, 1U, new_shape, &p->array, p->handle, protect, 1U, &err) != OK) {
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
    double cmp_value = (double) *p->cmp_value;

    size_t count = count_elements_from_value(source_arr, cmp_value, mode);
    uint32_t new_shape[1] = { (uint32_t) count };

    const uint32_t protect[1] = { source_handle };
    if (create_csnarray_locked(csound, reg, &p->h, 1U, new_shape, &p->array, p->handle, protect, 1U, &err) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;

    for (size_t i = 0, j = 0; i < source_arr->size; ++i) {
        double value = source_arr->data[i];
        if (compare_match(value, cmp_value, mode)) arr->data[j++] = value;
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

static void accumulate_reduction_scalar_helper(double *value, const CSN_ARRAY *source_arr, CSN_REDUCTION_MODE mode) {
    init_value_for_reduction(value, mode);
    for (size_t i = 0; i < source_arr->size; i++) {
        dispatch_value_for_reduction(value, source_arr->data[i], mode, i);
    }

    if (mode == RED_MEAN) *value /= (double) source_arr->size;
}

/* axis == -1 collapses to out_value; any other axis builds an array through
   out_handle/out_array. Exactly one pair is non-NULL, which is what keeps the
   two opcode families distinct at the type level. */
static int32_t csnarray_accumulate_reduction(CSOUND *csound, const OPDS *h, CSNREF *src_ref, int32_t axis, CSNREF *out_handle, CSN_ARRAY **out_array, MYFLT *out_value, CSN_REDUCTION_MODE mode) {
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

    if (axis != -1 && (uint32_t) axis >= source_ndim) {
        res = csound->InitError(csound, "[csnarray] Axis %d is out of range for a %u-D array (valid axes: -1 for all axes, or 0..%u)", axis, source_ndim, source_ndim - 1);
        goto done;
    }

    if (source_ndim <= 1 && axis > 0) {
        res = csound->InitError(csound, "[csnarray] Axis %d is not valid for a 1-D array (use -1 for all axes, or 0)", axis);
        goto done;
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

        if (create_csnarray_locked(csound, reg, h, source_ndim - 1, new_shape, out_array, out_handle, protect, 1U, &err) != OK) {
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
            double value = 0.0;
            accumulate_reduction_axis_helper(&value, arr, source_arr, src_coords, dst_coords, mode, axis);
            arr->data[linear] = value;
        }
    } else {
        double value = 0;
        accumulate_reduction_scalar_helper(&value, source_arr, mode);
        *out_value = (MYFLT) value;
    }

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_sum(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, (int32_t) *p->axis, p->handle, &p->array, NULL, RED_SUM);
}

int32_t csnarray_sum_all(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, RED_SUM);
}


int32_t csnarray_prod(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, (int32_t) *p->axis, p->handle, &p->array, NULL, RED_PROD);
}

int32_t csnarray_prod_all(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, RED_PROD);
}


int32_t csnarray_sub(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, (int32_t) *p->axis, p->handle, &p->array, NULL, RED_SUB);
}

int32_t csnarray_sub_all(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, RED_SUB);
}


int32_t csnarray_mean(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, (int32_t) *p->axis, p->handle, &p->array, NULL, RED_MEAN);
}

int32_t csnarray_mean_all(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, RED_MEAN);
}


int32_t csnarray_min(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, (int32_t) *p->axis, p->handle, &p->array, NULL, RED_MIN);
}

int32_t csnarray_min_all(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, RED_MIN);
}


int32_t csnarray_max(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, (int32_t) *p->axis, p->handle, &p->array, NULL, RED_MAX);
}

int32_t csnarray_max_all(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, RED_MAX);
}


int32_t csnarray_all(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, (int32_t) *p->axis, p->handle, &p->array, NULL, RED_ALL);
}

int32_t csnarray_all_all(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, RED_ALL);
}


int32_t csnarray_any(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, (int32_t) *p->axis, p->handle, &p->array, NULL, RED_ANY);
}

int32_t csnarray_any_all(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, RED_ANY);
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
static void stdvar_calculation_helper(double *value, uint32_t *src_coords, const uint32_t *dst_coords, const CSN_ARRAY *source_arr, uint32_t size, uint32_t axis, CSN_REDUCTION_MODE mode) {
    double mean = 0.0;
    double m_two = 0.0;
    for (uint32_t k = 0; k < size; ++k) {
        /* Place this slice's coordinates: k along the reduced axis, and the
           destination's coordinates across the axes that survive. Without this
           every output element would reduce the slice at the origin. */
        for (uint32_t i = 0, j = 0; i < source_arr->ndim; ++i) {
            src_coords[i] = (i == axis) ? k : dst_coords[j++];
        }

        size_t off = from_coords_to_offset(src_coords, source_arr->strides, source_arr->ndim);
        double x = source_arr->data[off];
        double delta = x - mean;
        /* Welford divides by the running count, not the total. */
        mean += delta / (double) (k + 1);
        double delta_two = x - mean;
        m_two += delta * delta_two;
    }

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
}

static void stdvar_calculation_scalar_helper(double *value, const CSN_ARRAY *source_arr, uint32_t size, CSN_REDUCTION_MODE mode) {
    double mean = 0.0;
    double m_two = 0.0;
    for (uint32_t k = 0; k < size; ++k) {
        double x = source_arr->data[k];
        double delta = x - mean;
        /* Welford divides by the running count, not the total. */
        mean += delta / (double) (k + 1);
        double delta_two = x - mean;
        m_two += delta * delta_two;
    }

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
}

static int32_t csnarray_stdvar_helper(CSOUND *csound, const OPDS *h, CSNREF *src_ref, int32_t axis, CSNREF *out_handle, CSN_ARRAY **out_array, MYFLT *out_value, CSN_REDUCTION_MODE mode) {
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

    if (axis != -1 && (uint32_t) axis >= source_ndim) {
        res = csound->InitError(csound, "[csnarray] Axis %d is out of range for a %u-D array (valid axes: -1 for all axes, or 0..%u)", axis, source_ndim, source_ndim - 1);
        goto done;
    }

    if (source_ndim <= 1 && axis > 0) {
        res = csound->InitError(csound, "[csnarray] Axis %d is not valid for a 1-D array (use -1 for all axes, or 0)", axis);
        goto done;
    }

    CSN_ARRAY *arr = NULL;
    if (axis != -1) {
        uint32_t new_shape[CSN_MAX_DIMS] = {0};
        for (uint32_t i = 0, j = 0; i < source_ndim; i++) {
            if (i != (uint32_t) axis) new_shape[j++] = source_shape[i];
        }

        const uint32_t protect[1] = { source_handle };

        if (create_csnarray_locked(csound, reg, h, source_ndim - 1, new_shape, out_array, out_handle, protect, 1U, &err) != OK) {
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
            stdvar_calculation_helper(&value, src_coords, dst_coords, source_arr, axis_size, axis, mode);
            arr->data[linear] = value;
        }
    } else {
        size_t size = source_arr->size;
        double value = 0;
        stdvar_calculation_scalar_helper(&value, source_arr, size, mode);
        *out_value = (MYFLT) value;
    }

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_std(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_stdvar_helper(csound, &p->h, p->source_handle, (int32_t) *p->axis, p->handle, &p->array, NULL, RED_STD);
}

int32_t csnarray_std_all(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_stdvar_helper(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, RED_STD);
}


int32_t csnarray_var(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_stdvar_helper(csound, &p->h, p->source_handle, (int32_t) *p->axis, p->handle, &p->array, NULL, RED_VAR);
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

    int32_t axis = (int32_t) *p->axis;
    if (axis != -1 && (uint32_t) axis >= source_ndim) {
        res = csound->InitError(csound, "[csnarray] Axis %d is out of range for a %u-D array (valid axes: -1 for all axes, or 0..%u)", axis, source_ndim, source_ndim - 1);
        goto done;
    }

    if (source_ndim <= 1 && axis > 0) {
        res = csound->InitError(csound, "[csnarray] Axis %d is not valid for a 1-D array (use -1 for all axes, or 0)", axis);
        goto done;
    }

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

    if (create_csnarray_locked(csound, reg, &p->h, 2U, new_shape, &p->array, p->handle, protect, 1U, &err) != OK) {
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

static int32_t csnarray_median_impl(CSOUND *csound, const OPDS *h, CSNREF *src_ref, int32_t axis, CSNREF *out_handle, CSN_ARRAY **out_array, MYFLT *out_value) {
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

    if (axis != -1 && (uint32_t) axis >= source_ndim) {
        res = csound->InitError(csound, "[csnarray] Axis %d is out of range for a %u-D array (valid axes: -1 for all axes, or 0..%u)", axis, source_ndim, source_ndim - 1);
        goto done;
    }

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
    if (create_csnarray_locked(csound, reg, h, source_ndim - 1, new_shape, out_array, out_handle, protect, 1U, &err) != OK) {
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
    return csnarray_median_impl(csound, &p->h, p->source_handle, (int32_t) *p->axis, p->handle, &p->array, NULL);
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

    const uint32_t protect[2] = { source_handle_a, source_handle_b };
    if (create_csnarray_locked(csound, reg, &p->h, new_ndim, new_shape, &p->array, p->handle, protect, 2U, &err) != OK) {
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

        double value = 0;
        double a = source_arr_a->data[off_a];
        double b = source_arr_b->data[off_b];
        switch (mode) {
            case CSN_ADD_HH:
                value = a + b;
                break;
            case CSN_SUB_HH:
                value = a - b;
                break;
            case CSN_MUL_HH:
                value = a * b;
                break;
            /* IEEE, as numpy: x/0 is an infinity and 0/0 is NaN. */
            case CSN_DIV_HH:
                value = a / b;
                break;
            case CSN_POW_HH:
                value = pow(a, b);
                break;
            case CSN_LOG_HH:
                /* Change of base elementwise: log of a in base b. */
                value = log(a) / log(b);
                break;
            default:
                break;
        }
        arr->data[i] = value;
    }

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

/* The handle and the scalar are passed explicitly rather than read off a
   cast struct: CSN_BINOP_SH lists them in the opposite order to CSN_BINOP_HS,
   so a single cast would silently swap them. */
static int32_t csnarray_binop_hs_sh_helper(CSOUND *csound, CSN_BINOP_COMMON *p, CSNREF *handle_arg, MYFLT *scalar_arg, CSN_BINOP_MODE mode) {
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

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    memcpy(new_shape, source_arr->shape, sizeof(uint32_t) * CSN_MAX_DIMS);

    const uint32_t protect[1] = { source_handle };
    if (create_csnarray_locked(csound, reg, &p->h, source_arr->ndim, new_shape, &p->array, p->handle, protect, 1U, &err) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;
    double scalar = (double) *scalar_arg;
    for (size_t i = 0; i < source_arr->size; i++) {
        double value = 0;
        double a = source_arr->data[i];
        switch (mode) {
            case CSN_ADD_HS:
            case CSN_ADD_SH:
                value = a + scalar;
                break;
            case CSN_SUB_HS:
                value = a - scalar;
                break;
            case CSN_SUB_SH:
                value = scalar - a;
                break;
            case CSN_MUL_HS:
            case CSN_MUL_SH:
                value = a * scalar;
                break;
            /* Division and log follow IEEE, as numpy does: x/0 is an infinity,
               0/0 and log of a negative are NaN. Raising here instead would
               make a note vanish mid-performance depending on its data. */
            case CSN_DIV_HS:
                value = a / scalar;
                break;
            case CSN_DIV_SH:
                value = scalar / a;
                break;
            case CSN_POW_HS:
                value = pow(a, scalar);
                break;
            case CSN_POW_SH:
                value = pow(scalar, a);
                break;
            case CSN_LOG_HS:
                /* Change of base. Base 1 divides by log(1) == 0 and yields an
                   infinity, which is also what numpy returns. */
                value = log(a) / log(scalar);
                break;
            case CSN_LOG_SH:
                /* The array supplies the base here, the scalar the argument. */
                value = log(scalar) / log(a);
                break;
            default:
                break;
        }
        arr->data[i] = value;
    }

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}


int32_t csnarray_add_hh(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_helper(csound, p, CSN_ADD_HH);
}

int32_t csnarray_add_hs(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, CSN_ADD_HS);
}

int32_t csnarray_subtract_hh(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_helper(csound, p, CSN_SUB_HH);
}

int32_t csnarray_subtract_hs(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, CSN_SUB_HS);
}

int32_t csnarray_subtract_sh(CSOUND *csound, CSN_BINOP_SH *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, CSN_SUB_SH);
}

int32_t csnarray_mul_hh(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_helper(csound, p, CSN_MUL_HH);
}

int32_t csnarray_mul_hs(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, CSN_MUL_HS);
}

int32_t csnarray_div_hh(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_helper(csound, p, CSN_DIV_HH);
}

int32_t csnarray_div_hs(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, CSN_DIV_HS);
}

int32_t csnarray_div_sh(CSOUND *csound, CSN_BINOP_SH *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, CSN_DIV_SH);
}

int32_t csnarray_pow_hh(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_helper(csound, p, CSN_POW_HH);
}

int32_t csnarray_pow_hs(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, CSN_POW_HS);
}

int32_t csnarray_pow_sh(CSOUND *csound, CSN_BINOP_SH *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, CSN_POW_SH);
}

int32_t csnarray_log_hh(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_helper(csound, p, CSN_LOG_HH);
}

int32_t csnarray_log_hs(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, CSN_LOG_HS);
}

int32_t csnarray_log_sh(CSOUND *csound, CSN_BINOP_SH *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, CSN_LOG_SH);
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

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    memcpy(new_shape, source_arr->shape, sizeof(uint32_t) * CSN_MAX_DIMS);

    const uint32_t protect[1] = { source_handle };
    if (create_csnarray_locked(csound, reg, &p->h, source_arr->ndim, new_shape, &p->array, p->handle, protect, 1U, &err) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;
    for (size_t i = 0; i < source_arr->size; i++) {
        double value = 0;
        double a = source_arr->data[i];
        switch (mode) {
            case CSN_SQRT:
                value = sqrt(a);
                break;
            case CSN_CBRT:
                value = cbrt(a);
                break;
            case CSN_ABS:
                value = fabs(a);
                break;
            case CSN_SIGN:
                /* numpy: sign(0) is 0 and sign(NaN) is NaN, so returning a
                   itself covers both without a special case. */
                value = (a > 0.0) ? 1.0 : ((a < 0.0) ? -1.0 : a);
                break;
            case CSN_EXP:
                value = exp(a);
                break;
            case CSN_SIN:
                value = sin(a);
                break;
            case CSN_COS:
                value = cos(a);
                break;
            case CSN_TAN:
                value = tan(a);
                break;
            case CSN_ASIN:
                value = asin(a);
                break;
            case CSN_ACOS:
                value = acos(a);
                break;
            case CSN_ATAN:
                value = atan(a);
                break;
            case CSN_SINH:
                value = sinh(a);
                break;
            case CSN_COSH:
                value = cosh(a);
                break;
            case CSN_TANH:
                value = tanh(a);
                break;
            case CSN_ASINH:
                value = asinh(a);
                break;
            case CSN_ACOSH:
                value = acosh(a);
                break;
            case CSN_ATANH:
                value = atanh(a);
                break;
            case CSN_FLOOR:
                value = floor(a);
                break;
            case CSN_CEIL:
                value = ceil(a);
                break;
            case CSN_ROUND:
                /* numpy rounds halves to even; C's round() sends them away
                   from zero, so 2.5 would become 3 instead of 2. rint follows
                   the current mode, which is round-to-nearest-even by default. */
                value = rint(a);
                break;
        }
        arr->data[i] = value;
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

static int32_t check_dot_shape(const uint32_t *shape_a, const uint32_t *shape_b, size_t dim_a, size_t dim_b) {
    size_t bk = (dim_b >= 2) ? dim_b - 2 : 0;
    return (shape_a[dim_a - 1] == shape_b[bk]) ? OK : NOTOK;
}

static void get_dot_inner_accum(double *acc, uint32_t *a_coords, uint32_t *b_coords, uint32_t off_dim_a, uint32_t off_dim_b, CSN_ARRAY *source_arr_a, CSN_ARRAY *source_arr_b, uint32_t source_dim_a, uint32_t source_dim_b, uint32_t loop_dim) {
    *acc = 0;
    for (uint32_t k = 0; k < loop_dim; ++k) {
        a_coords[off_dim_a] = k;
        b_coords[off_dim_b] = k;
        size_t off_a = from_coords_to_offset(a_coords, source_arr_a->strides, source_dim_a);
        size_t off_b = from_coords_to_offset(b_coords, source_arr_b->strides, source_dim_b);
        *acc += source_arr_a->data[off_a] * source_arr_b->data[off_b];
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
        double acc = 0.0;

        switch (mode) {
            case CSN_DOT:
                for (uint32_t i = 0; i + 1 < source_dim_a; ++i) {
                    a_coords[i] = dst_coords[d++];
                }

                for (uint32_t i = 0; i < source_dim_b; ++i) {
                    if (i != bk) b_coords[i] = dst_coords[d++];
                }

                get_dot_inner_accum(&acc, a_coords, b_coords, source_dim_a - 1, bk, source_arr_a, source_arr_b, source_dim_a, source_dim_b, ka_dim);
                out_arr->data[linear] = acc;
                break;
            case CSN_INNER:
                memcpy(a_coords, dst_coords, sizeof(uint32_t) * (source_dim_a - 1));
                memcpy(b_coords, dst_coords + (source_dim_a - 1), sizeof(uint32_t) * (source_dim_b - 1));
                get_dot_inner_accum(&acc, a_coords, b_coords, source_dim_a - 1, source_dim_b - 1, source_arr_a, source_arr_b, source_dim_a, source_dim_b, ka_dim);
                out_arr->data[linear] = acc;
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
    if (create_csnarray_locked(csound, reg, &p->h, new_ndim, new_shape, &p->array, p->handle, protect, 2U, &err) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;

    size_t size_a = source_arr_a->size;
    size_t size_b = source_arr_b->size;
    double dot_ab = 0.0;
    double dot_bb = 0.0;
    double *a = source_arr_a->data;
    double *b = source_arr_b->data;
    switch(mode) {
        case CSN_DOT:
        case CSN_INNER:
            dot_inner(arr, source_arr_a, source_arr_b, mode);
            break;
        case CSN_OUTER:
            for (size_t i = 0; i < size_a; ++i) {
                for (size_t j = 0; j < size_b; ++j) {
                    arr->data[i * size_b + j] = a[i] * b[j];
                }
            }
            break;
        case CSN_PAIR_DISTANCE:
            for (size_t i = 0; i < size_a; ++i) {
                arr->data[i] = fabs(a[i] - b[i]);
            }
            break;
        case CSN_PROJECT:
        case CSN_REJECT:
        case CSN_REFLECT:
            dot_ab = 0.0;
            dot_bb = 0.0;
            for (size_t i = 0; i < size_a; ++i) {
                dot_ab += a[i] * b[i];
                dot_bb += b[i] * b[i];
            }

            if (dot_bb != 0.0) {
                double scale = dot_ab / dot_bb;
                for (size_t i = 0; i < size_a; ++i) {
                    double proj = scale * b[i];
                    double value = 0.0;
                    switch (mode) {
                        case CSN_PROJECT:
                            value = proj;
                            break;
                        case CSN_REJECT:
                            value = a[i] - proj;
                            break;
                        case CSN_REFLECT:
                            value = a[i] - 2.0 * proj;
                            break;
                        default:
                            break;
                    }

                    arr->data[i] = value;
                }
            } else {
                res = csound->InitError(csound, "[csnarray] The second vector has zero length, so the projection onto it is undefined");
                goto done;
            }
            break;
        case CSN_CROSS:
            arr->data[0] = a[1] * b[2] - a[2] * b[1];
            arr->data[1] = a[2] * b[0] - a[0] * b[2];
            arr->data[2] = a[0] * b[1] - a[1] * b[0];
            break;
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

static int32_t csnarray_scalar_helper(CSOUND *csound, CSN_BINOP_HH_SCALAR *p, CSN_VECOP_MODE mode, double dist_order) {
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

    if (source_dim_a != 1 || source_dim_b != 1 || source_shape_a[0] != source_shape_b[0]) {
        char abuf[CSN_SHAPE_STR_MAX], bbuf[CSN_SHAPE_STR_MAX];
        res = csound->InitError(csound, "[csnarray] Both arrays must be 1-D with the same size, got %s and %s", shape_str(abuf, sizeof(abuf), source_shape_a, (uint32_t) source_dim_a), shape_str(bbuf, sizeof(bbuf), source_shape_b, (uint32_t) source_dim_b));
        goto done;
    }

    if (mode == CSN_DISTANCE && dist_order <= 0.0) {
        res = csound->InitError(csound, "[csnarray] Distance order must be >= 1, got %g", dist_order);
        goto done;
    }

    double value = 0.0;
    for (size_t i = 0; i < source_arr_a->size; i++) {
        switch (mode) {
            case CSN_DOT_SCALAR:
            case CSN_INNER_SCALAR:
                value += source_arr_a->data[i] * source_arr_b->data[i];
                break;
            case CSN_DISTANCE:
                value += pow(fabs(source_arr_a->data[i] - source_arr_b->data[i]), dist_order);
                break;
            default:
                break;
        }
    }

    if (mode == CSN_DISTANCE) value = pow(value, 1.0 / dist_order);
    *p->value = (MYFLT) value;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
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

static double norm_from_scratch(double *arr, size_t size, double order) {
    double acc = 0.0;
    for (size_t i = 0; i < size; i++) {
        double x = fabs(arr[i]);
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
    int32_t axis = (int32_t) *p->axis;
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

    if (axis <= -1 || (uint32_t) axis >= source_ndim) {
        res = csound->InitError(csound, "[csnarray] Axis %d is out of range for a %u-D array (valid axes: 0..%u)", axis, source_ndim, source_ndim - 1);
        goto done;
    }

    size_t run = source_shape[axis];
    scratch = csound->Calloc(csound, sizeof(double) * (run > 0 ? run : 1));
    if (scratch == NULL) {
        res = csound->InitError(csound, "[csnarray] Out of memory: allocation of %zu bytes failed", (size_t) (sizeof(double) * (run > 0 ? run : 1)));
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
    if (create_csnarray_locked(csound, reg, &p->h, new_ndim, new_shape, &p->array, p->handle, protect, 1U, &err) != OK) {
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
            scratch[k] = source_arr->data[off];
        }

        arr->data[linear] = norm_from_scratch(scratch, run, order);
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
    *p->value = (MYFLT) norm_from_scratch(source_arr->data, source_arr->size, order);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static void normalize_slice(double *dst, const double *src, size_t n, size_t stride, double order) {
    double acc = 0.0;
    for (size_t i = 0; i < n; ++i) {
        acc += pow(fabs(src[i * stride]), order);
    }

    double nrm = pow(acc, 1.0 / order);
    if (nrm == 0.0) {
        for (size_t i = 0; i < n; ++i) {
            dst[i * stride] = src[i * stride];
        }
        return;
    }

    for (size_t i = 0; i < n; ++i) {
        dst[i * stride] = src[i * stride] / nrm;
    }
}

static void cumsumprod_slice(double *dst, const double *src, size_t n, size_t stride, bool is_cumsum) {
    double acc = is_cumsum ? 0.0 : 1.0;
    for (size_t i = 0; i < n; ++i) {
        if (is_cumsum) acc += src[i * stride];
        else acc *= src[i * stride];
        dst[i * stride] = acc;
    }
}

static void diff_slice(double *dst, const double *src, size_t n, size_t src_stride, size_t dst_stride) {
    for (size_t i = 0; i + 1 < n; ++i) {
        dst[i * dst_stride] = src[(i + 1) * src_stride] - src[i * src_stride];
    }
}

static void gradient_slice(double *dst, const double *src, size_t n, size_t stride) {
    if (n == 0)
        return;
    if (n == 1) {
        dst[0] = 0.0;
        return;
    }
    dst[0] = (src[stride] - src[0]);
    for (size_t i = 1; i < n - 1; ++i) {
        dst[i * stride] = (src[(i + 1) * stride] -  src[(i - 1) * stride]) * 0.5;
    }
    dst[(n - 1) * stride] = src[(n - 1) * stride] - src[(n - 2) * stride];
}

static int32_t csnarray_unary_ax_helper(CSOUND *csound, const OPDS *h, CSNREF *src_ref, int32_t axis, double order, CSNREF *out_handle, CSN_ARRAY **out_array, CSN_UNARYOP_AX_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

    if (mode == CSN_NORMALIZE && order < 1.0) {
        return csound->InitError(csound, "[csnarray] Norm order must be >= 1, got %g", order);
    }

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, src_ref->id);
    if (source_slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) src_ref->id);
        goto done;
    }

    CSN_ARRAY *source_arr = source_slot->array;
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    if (axis != -1 && (uint32_t) axis >= source_ndim) {
        res = csound->InitError(csound, "[csnarray] Axis %d is out of range for a %u-D array (valid axes: -1 for all axes, or 0..%u)", axis, source_ndim, source_ndim - 1);
        goto done;
    }

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
        if (create_csnarray_locked(csound, reg, h, new_dim, new_shape, out_array, out_handle, protect, 1U, &err) != OK) {
            res = csound->InitError(csound, "[csnarray] %s", err);
            goto done;
        }
        arr = *out_array;
    }

    if (axis == -1) {
        switch (mode) {
            case CSN_NORMALIZE:
                normalize_slice(arr->data, source_arr->data, source_arr->size, 1, order);
                break;
            case CSN_DIFF:
                diff_slice(arr->data, source_arr->data, source_arr->size, 1, 1);
                break;
            case CSN_GRADIENT:
                gradient_slice(arr->data, source_arr->data, source_arr->size, 1);
                break;
            case CSN_CUMSUM:
            case CSN_CUMPROD: {
                bool is_cumsum = mode == CSN_CUMSUM;
                cumsumprod_slice(arr->data, source_arr->data, source_arr->size, 1, is_cumsum);
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
                normalize_slice(arr->data + dst_base, source_arr->data + src_base, source_shape[axis], src_stride, order);
                break;
            case CSN_DIFF:
                diff_slice(arr->data + dst_base, source_arr->data + src_base, source_shape[axis], src_stride, dst_stride);
                break;
            case CSN_GRADIENT:
                gradient_slice(arr->data + dst_base, source_arr->data + src_base, source_shape[axis], src_stride);
                break;
            case CSN_CUMSUM:
            case CSN_CUMPROD: {
                bool is_cumsum = mode == CSN_CUMSUM;
                cumsumprod_slice(arr->data + dst_base, source_arr->data + src_base, source_shape[axis], src_stride, is_cumsum);
                break;
            }
        }
    }

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_normalize(CSOUND *csound, CSN_UNARYOP_AX *p) {
    return csnarray_unary_ax_helper(csound, &p->h, p->source_handle, (int32_t) *p->axis, (double) *p->order, p->handle, &p->array, CSN_NORMALIZE);
}

int32_t csnarray_normalize_in(CSOUND *csound, CSN_UNARYOP_AX_IN *p) {
    return csnarray_unary_ax_helper(csound, &p->h, p->source_handle, (int32_t) *p->axis, (double) *p->order, NULL, NULL, CSN_NORMALIZE);
}

int32_t csnarray_distance(CSOUND *csound, CSN_BINOP_HH_SCALAR *p) {
    double dist_order = (double) *p->arg_a;
    return csnarray_scalar_helper(csound, p, CSN_DISTANCE, dist_order);
}

int32_t csnarray_pair_distance(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_vec_helper(csound, p, CSN_PAIR_DISTANCE);
}

int32_t csnarray_angle(CSOUND *csound, CSN_BINOP_HH_SCALAR *p) {
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

    double dot = 0.0;
    double norm_a = 0.0;
    double norm_b = 0.0;
    for (size_t i = 0; i < source_arr_a->size; i++) {
        double a = source_arr_a->data[i];
        double b = source_arr_b->data[i];
        dot += a * b;
        norm_a += a * a;
        norm_b += b * b;
    }

    if (norm_a == 0.0 || norm_b == 0.0) {
        csound->Message(csound, "[csnarray] Angle undefined for zero-length vectors");
        *p->value = (MYFLT) NAN;
        goto done;
    }

    norm_a = sqrt(norm_a);
    norm_b = sqrt(norm_b);

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
    return csnarray_unary_ax_helper(csound, &p->h, p->source_handle, (int32_t) *p->axis, 0.0, p->handle, &p->array, CSN_DIFF);
}

int32_t csnarray_gradient(CSOUND *csound, CSN_UNARYOP_AX *p) {
    return csnarray_unary_ax_helper(csound, &p->h, p->source_handle, (int32_t) *p->axis, 0.0, p->handle, &p->array, CSN_GRADIENT);
}

int32_t csnarray_cumsum(CSOUND *csound, CSN_UNARYOP_AX *p) {
    return csnarray_unary_ax_helper(csound, &p->h, p->source_handle, (int32_t) *p->axis, 0.0, p->handle, &p->array, CSN_CUMSUM);
}

int32_t csnarray_cumprod(CSOUND *csound, CSN_UNARYOP_AX *p) {
    return csnarray_unary_ax_helper(csound, &p->h, p->source_handle, (int32_t) *p->axis, 0.0, p->handle, &p->array, CSN_CUMPROD);
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

    uint32_t source_a_dim = source_arr_a->ndim;
    uint32_t source_b_dim = source_arr_b->ndim;

    /* Il rango va controllato prima di qualsiasi 'ndim - 2': su un array 1-D
       quella sottrazione va in underflow e l'indice finisce fuori da shape[]. */
    if (source_a_dim == 1 && source_b_dim == 1) {
        res = csound->InitError(csound, "[csnarray] The matrix product of two 1-D arrays is a scalar; declare the output as i, not as a handle");
        goto done;
    }

    /* Promozione come numpy: un operando 1-D a sinistra prende un asse davanti,
       a destra ne prende uno in coda; l'asse aggiunto sparisce dal risultato.
       Sull'asse promosso lo stride e' 0, tanto l'estensione e' 1. */
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

    /* Gli assi che precedono gli ultimi due sono il batch: si allineano da
       destra e si broadcastano, esattamente come in numpy. */
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

    /* Gli assi promossi non compaiono nel risultato. */
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
    if (create_csnarray_locked(csound, reg, &p->h, new_ndim, new_shape, &p->array, p->handle, protect, 2U, &err) != OK) {
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

        /* Offset del blocco 2-D dentro ciascun operando: un asse di batch
           lungo 1 viene stirato, quindi legge sempre l'indice 0. */
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

        /* L'output e' contiguo: gli assi promossi hanno estensione 1, quindi
           non spostano nulla nel layout lineare. */
        for (uint32_t r = 0; r < rows; ++r) {
            for (uint32_t c = 0; c < cols; ++c) {
                double acc = 0.0;
                for (uint32_t k = 0; k < inner; ++k) {
                    acc += source_arr_a->data[a_base + (size_t) r * a_row_stride + (size_t) k * a_col_stride]
                         * source_arr_b->data[b_base + (size_t) k * b_row_stride + (size_t) c * b_col_stride];
                }
                arr->data[(batch * rows + r) * cols + c] = acc;
            }
        }
    }

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_trace(CSOUND *csound, CSN_UNARYOP_SCALAR *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

    uint32_t source_handle = (uint32_t) p->source_handle->id;
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
    double sum = 0.0;
    for (uint32_t i = 0; i < n; i++) {
        coords[0] = i;
        coords[1] = i;

        size_t off = from_coords_to_offset(coords, source_arr->strides, 2U);
        sum += source_arr->data[off];
    }

    *p->value = (MYFLT) sum;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
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

    /* 1-D -> matrice diagonale n x n; 2-D -> vettore con la diagonale, lunga
       quanto il lato piu' corto (come numpy su matrici non quadrate). */
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    uint32_t new_dim = dim == 2U ? 1U : 2U;
    if (dim == 1) {
        new_shape[0] = source_shape[0];
        new_shape[1] = source_shape[0];
    } else {
        new_shape[0] = source_shape[0] < source_shape[1] ? source_shape[0] : source_shape[1];
    }

    const uint32_t protect[1] = { source_handle };
    if (create_csnarray_locked(csound, reg, &p->h, new_dim, new_shape, &p->array, p->handle, protect, 1U, &err) != OK) {
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
            arr->data[i] = source_arr->data[off];
        } else {
            size_t off = from_coords_to_offset(coords, arr->strides, 2U);
            arr->data[off] = source_arr->data[i];
        }
    }

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static void movmean_slice(double *dst, const double *src, size_t n, size_t stride, size_t win_size) {
    size_t left = win_size / 2;
    size_t right = win_size - left - 1;

    for (size_t i = 0; i < n; ++i) {
        size_t begin = i >= left ? i - left : 0;
        size_t end = i + right + 1;
        end = end > n ? n : end;
        double acc = 0.0;
        for (size_t j = begin; j < end; ++j) {
            acc += src[j * stride];
        }
        dst[i * stride] = acc / (double) (end - begin);
    }
}

static void movstdvar_slice(double *dst, const double *src, size_t n, size_t stride, size_t win_size, CSN_REDUCTION_MODE mode) {
    size_t left = win_size / 2;
    size_t right = win_size - left - 1;

    for (size_t i = 0; i < n; i++) {
        double mean = 0.0;
        double m_two = 0.0;
        size_t begin = i >= left ? i - left : 0;
        size_t end = i + right + 1;
        end = end > n ? n : end;
        for (size_t j = begin; j < end; ++j) {
            double x = src[j * stride];
            double delta = x - mean;
            /* Welford divides by the running count, not the total. */
            mean += delta / (double) ((j - begin) + 1);
            double delta_two = x - mean;
            m_two += delta * delta_two;
        }

        double var = m_two / (double) (end - begin);
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

    int32_t axis = *p->axis;
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
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    if (axis != -1 && (uint32_t) axis >= source_ndim) {
        res = csound->InitError(csound, "[csnarray] Axis %d is out of range for a %u-D array (valid axes: -1 for all axes, or 0..%u)", axis, source_ndim, source_ndim - 1);
        goto done;
    }

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
    if (create_csnarray_locked(csound, reg, &p->h, new_dim, new_shape, &p->array, p->handle, protect, 1U, &err) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;

    if (axis == -1) {
        switch (mode) {
            case CSN_MOVMEAN:
                movmean_slice(arr->data, source_arr->data, source_arr->size, 1, winsize);
                break;
            case CSN_MOVMEDIAN:
                movmedian_slice(arr->data, source_arr->data, median_buffer, source_arr->size, 1, winsize);
                break;
            case CSN_MOVSTD:
                movstdvar_slice(arr->data, source_arr->data, source_arr->size, 1, winsize, RED_STD);
                break;
            case CSN_MOVVAR:
                movstdvar_slice(arr->data, source_arr->data, source_arr->size, 1, winsize, RED_VAR);
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
                movmean_slice(arr->data + dst_base, source_arr->data + src_base, source_shape[axis], src_stride, winsize);
                break;
            case CSN_MOVMEDIAN:
                movmedian_slice(arr->data + dst_base, source_arr->data + src_base, median_buffer, source_shape[axis], src_stride, winsize);
                break;
            case CSN_MOVSTD:
                movstdvar_slice(arr->data + dst_base, source_arr->data + src_base, source_shape[axis], src_stride, winsize, RED_STD);
                break;
            case CSN_MOVVAR:
                movstdvar_slice(arr->data + dst_base, source_arr->data + src_base, source_shape[axis], src_stride, winsize, RED_VAR);
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

static void dispatch_movstats(double *dst, double *src, size_t size, uint32_t stride, size_t winsize, double *median_buffer, CSN_MOVSTATS_MODE mode) {
    switch (mode) {
        case CSN_MOVMEAN:
            movmean_slice(dst, src, size, stride, winsize);
            break;
        case CSN_MOVMEDIAN:
            movmedian_slice(dst, src, median_buffer, size, stride, winsize);
            break;
        case CSN_MOVSTD:
            movstdvar_slice(dst, src, size, stride, winsize, RED_STD);
            break;
        case CSN_MOVVAR:
            movstdvar_slice(dst, src, size, stride, winsize, RED_VAR);
            break;
        case CSN_MOVMIN:
            movminmax_slice(dst, src, size, stride, winsize, RED_MIN);
            break;
        case CSN_MOVMAX:
            movminmax_slice(dst, src, size, stride, winsize, RED_MAX);
            break;
    }
}


static int32_t csnarray_movstats_in_helper(CSOUND *csound, CSN_MOVSTATS_IN *p, CSN_MOVSTATS_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: the csnum array registry is not available");
    }

    int32_t res = OK;

    int32_t axis = *p->axis;
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
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    if (axis != -1 && (uint32_t) axis >= source_ndim) {
        res = csound->InitError(csound, "[csnarray] Axis %d is out of range for a %u-D array (valid axes: -1 for all axes, or 0..%u)", axis, source_ndim, source_ndim - 1);
        goto done;
    }

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
        dispatch_movstats(source_arr->data, source_arr->data, source_arr->size, 1, winsize, median_buffer, mode);
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
        dispatch_movstats(source_arr->data + src_base, source_arr->data + src_base, source_shape[axis], src_stride, winsize, median_buffer, mode);
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

// --- OENTRY ---

#define S(x) sizeof(x)

static OENTRY localops[] = {
    { "csnempty",              S(CSN_ARR_INIT),              0, ":CsnArr;",   "i[]",                  (SUBR) create_empty_csnarray,   NULL, (SUBR) create_csnarray_deinit,        NULL, 0 },
    { "csnzeros",              S(CSN_ARR_INIT),              0, ":CsnArr;",   "i[]",                  (SUBR) create_zeros_csnarray,   NULL, (SUBR) create_csnarray_deinit,        NULL, 0 },
    { "csnones",               S(CSN_ARR_INIT),              0, ":CsnArr;",   "i[]",                  (SUBR) create_ones_csnarray,    NULL, (SUBR) create_csnarray_deinit,        NULL, 0 },
    { "csnfull",               S(CSN_ARR_INIT),              0, ":CsnArr;",   "i[]i",                 (SUBR) create_full_csnarray,    NULL, (SUBR) create_csnarray_deinit,        NULL, 0 },
    { "csnlike",               S(CSN_ARR_INIT_LIKE),         0, ":CsnArr;",   ":CsnArr;i",            (SUBR) create_like_csnarray,    NULL, (SUBR) create_csnarray_like_deinit,   NULL, 0 },
    { "csnrand",               S(CSN_ARR_RND_INIT),          0, ":CsnArr;",   "i[]ii",                (SUBR) create_random_csnarray,  NULL, (SUBR) create_csnarray_random_deinit, NULL, 0 },
    { "csnfromarray",          S(CSN_FROM_ARRAY),            0, ":CsnArr;",   "i[]",                  (SUBR) from_array_to_csnarray,  NULL, (SUBR) from_array_to_csnarray_deinit, NULL, 0 },
    { "csntoarray",            S(CSN_TO_ARRAY),              0, "i[]",        ":CsnArr;",             (SUBR) from_csnarray_to_array,  NULL, NULL,                                 NULL, 0 },
    { "csnfree",               S(CSN_FREE),                  0, "",           ":CsnArr;",             (SUBR) free_csnarray,           NULL, NULL,                                 NULL, 0 },
    { "csndims",               S(CSN_SIZE_DIMS),             0, "i",          ":CsnArr;",             (SUBR) csnarray_dims,           NULL, NULL,                                 NULL, 0 },
    { "csnsize",               S(CSN_SIZE_DIMS),             0, "i",          ":CsnArr;",             (SUBR) csnarray_size,           NULL, NULL,                                 NULL, 0 },
    { "csnisempty",            S(CSN_SIZE_DIMS),             0, "i",          ":CsnArr;",             (SUBR) csnarray_is_empty,       NULL, NULL,                                 NULL, 0 },
    { "csnshape",              S(CSN_SHAPE),                 0, "i[]",        ":CsnArr;",             (SUBR) csnarray_shape,          NULL, NULL,                                 NULL, 0 },
    { "csnarange",             S(CSN_SPACED_SPACE),          0, ":CsnArr;",   "iii",                  (SUBR) csnarray_arange,         NULL, (SUBR) csnarray_space_spaced_deinit,  NULL, 0 },
    { "csnlinspace",           S(CSN_SPACED_SPACE),          0, ":CsnArr;",   "iii",                  (SUBR) csnarray_linspace,       NULL, (SUBR) csnarray_space_spaced_deinit,  NULL, 0 },
    { "csnlogspace",           S(CSN_SPACED_SPACE),          0, ":CsnArr;",   "iiii",                 (SUBR) csnarray_logspace,       NULL, (SUBR) csnarray_space_spaced_deinit,  NULL, 0 },
    { "csngeomspace",          S(CSN_SPACED_SPACE),          0, ":CsnArr;",   "iii",                  (SUBR) csnarray_geomspace,      NULL, (SUBR) csnarray_space_spaced_deinit,  NULL, 0 },
    { "csnidentity",           S(CSN_IDENTITY),              0, ":CsnArr;",   "i",                    (SUBR) csnarray_identity,       NULL, (SUBR) csnarray_identity_deinit,      NULL, 0 },
    { "csnreshape",            S(CSN_RESHAPE),               0, ":CsnArr;",   ":CsnArr;i[]",          (SUBR) csnarray_reshape,        NULL, (SUBR) csnarray_shape_deinit,         NULL, 0 },
    { "csnreshape.in",         S(CSN_RESHAPE_IN),            0, "",           ":CsnArr;i[]",          (SUBR) csnarray_reshape_in,     NULL, NULL,                                 NULL, 0 },
    { "csnflatten",            S(CSN_RESHAPE),               0, ":CsnArr;",   ":CsnArr;",             (SUBR) csnarray_flatten,        NULL, (SUBR) csnarray_shape_deinit,         NULL, 0 },
    { "csnflatten.in",         S(CSN_RESHAPE_IN),            0, "",           ":CsnArr;",             (SUBR) csnarray_flatten_in,     NULL, NULL,                                 NULL, 0 },
    { "csntranspose",          S(CSN_RESHAPE),               0, ":CsnArr;",   ":CsnArr;",             (SUBR) csnarray_transpose,      NULL, (SUBR) csnarray_shape_deinit,         NULL, 0 },
    { "csntranspose.ax",       S(CSN_RESHAPE),               0, ":CsnArr;",   ":CsnArr;i[]",          (SUBR) csnarray_transpose,      NULL, (SUBR) csnarray_shape_deinit,         NULL, 0 },
    { "csntranspose.in",       S(CSN_RESHAPE_IN),            0, "",           ":CsnArr;",             (SUBR) csnarray_transpose_in,   NULL, NULL,                                 NULL, 0 },
    { "csntranspose.ax.in",    S(CSN_RESHAPE_IN),            0, "",           ":CsnArr;i[]",          (SUBR) csnarray_transpose_in,   NULL, NULL,                                 NULL, 0 },
    { "csnflip",               S(CSN_FLIP_ROLL),             0, ":CsnArr;",   ":CsnArr;j",            (SUBR) csnarray_flip,           NULL, (SUBR) csnarray_flip_deinit,          NULL, 0 },
    { "csnflip.in",            S(CSN_FLIP_ROLL_IN),          0, "",           ":CsnArr;j",            (SUBR) csnarray_flip_in,        NULL, NULL,                                 NULL, 0 },
    { "csnroll",               S(CSN_FLIP_ROLL),             0, ":CsnArr;",   ":CsnArr;i",            (SUBR) csnarray_roll,           NULL, (SUBR) csnarray_flip_deinit,          NULL, 0 },
    { "csnroll.in",            S(CSN_FLIP_ROLL_IN),          0, "",           ":CsnArr;i",            (SUBR) csnarray_roll_in,        NULL, NULL,                                 NULL, 0 },
    { "csnroll.ax",            S(CSN_FLIP_ROLL),             0, ":CsnArr;",   ":CsnArr;ij",           (SUBR) csnarray_rollaxis,       NULL, (SUBR) csnarray_flip_deinit,          NULL, 0 },
    { "csnroll.ax.in",         S(CSN_FLIP_ROLL_IN),          0, "",           ":CsnArr;ij",           (SUBR) csnarray_rollaxis_in,    NULL, NULL,                                 NULL, 0 },
    { "csnget",                S(CSN_GET),                   0, "i",          ":CsnArr;i[]",          (SUBR) csnarray_get,            NULL, NULL,                                 NULL, 0 },
    { "csnset",                S(CSN_SET),                   0, "",           ":CsnArr;i[]i",         (SUBR) csnarray_set,            NULL, NULL,                                 NULL, 0 },
    { "csntake",               S(CSN_TAKE),                  0, ":CsnArr;",   ":CsnArr;ii",           (SUBR) csnarray_take,           NULL, (SUBR) csnarray_take_deinit,          NULL, 0 },
    { "csntake.flat",          S(CSN_TAKE_FLAT),             0, "i",          ":CsnArr;i",            (SUBR) csnarray_take_flat,      NULL, NULL,                                 NULL, 0 },
    { "csngetslice",           S(CSN_GET_SLICE),             0, ":CsnArr;",   ":CsnArr;iiii",         (SUBR) csnarray_get_slice,      NULL, (SUBR) csnarray_slice_deinit,         NULL, 0 },
    { "csnsetslice",           S(CSN_SET_SLICE),             0, "",           ":CsnArr;:CsnArr;iiii", (SUBR) csnarray_set_slice,      NULL, NULL,                                 NULL, 0 },
    { "csnpush",               S(CSN_PUSH),                  0, "",           ":CsnArr;i",            (SUBR) csnarray_push,           NULL, NULL,                                 NULL, 0 },
    { "csnpop",                S(CSN_POP),                   0, "i",          ":CsnArr;",             (SUBR) csnarray_pop,            NULL, NULL,                                 NULL, 0 },
    { "csninsert.flat",        S(CSN_PUSH),                  0, "",           ":CsnArr;ii",           (SUBR) csnarray_insert,         NULL, NULL,                                 NULL, 0 },
    { "csnremove.flat",        S(CSN_POP),                   0, "i",          ":CsnArr;i",            (SUBR) csnarray_remove,         NULL, NULL,                                 NULL, 0 },
    { "csninsert.block",       S(CSN_INSERT_BLOCK),          0, "",           ":CsnArr;:CsnArr;ii",   (SUBR) csnarray_insert_block,   NULL, NULL,                                 NULL, 0 },
    { "csnremove.block",       S(CSN_TAKE),                  0, ":CsnArr;",   ":CsnArr;ii",           (SUBR) csnarray_remove_block,   NULL, (SUBR) csnarray_take_deinit,          NULL, 0 },
    { "csnconcat.flat",        S(CSN_CONCAT),                0, ":CsnArr;",   ":CsnArr;:CsnArr;",     (SUBR) csnarray_concat_flat,    NULL, (SUBR) csnarray_concat_deinit,        NULL, 0 },
    { "csnconcat.block",       S(CSN_CONCAT),                0, ":CsnArr;",   ":CsnArr;:CsnArr;i",    (SUBR) csnarray_concat_block,   NULL, (SUBR) csnarray_concat_deinit,        NULL, 0 },
    { "csnpad",                S(CSN_PAD),                   0, ":CsnArr;",   ":CsnArr;iio",          (SUBR) csnarray_pad,            NULL, (SUBR) csnarray_pad_deinit,           NULL, 0 },
    { "csnpad.ax",             S(CSN_PAD),                   0, ":CsnArr;",   ":CsnArr;iiii",         (SUBR) csnarray_pad,            NULL, (SUBR) csnarray_pad_deinit,           NULL, 0 },
    { "csnpad.in",             S(CSN_PAD_IN),                0, "",           ":CsnArr;iio",          (SUBR) csnarray_pad_in,         NULL, NULL,                                 NULL, 0 },
    { "csnpad.ax.in",          S(CSN_PAD_IN),                0, "",           ":CsnArr;iiii",         (SUBR) csnarray_pad_in,         NULL, NULL,                                 NULL, 0 },
    { "csnclip",               S(CSN_CLIP),                  0, ":CsnArr;",   ":CsnArr;ii",           (SUBR) csnarray_clip,           NULL, (SUBR) csnarray_clip_deinit,          NULL, 0 },
    { "csnclip.in",            S(CSN_CLIP_IN),               0, "",           ":CsnArr;ii",           (SUBR) csnarray_clip_in,        NULL, NULL,                                 NULL, 0 },
    { "csnargwhere",           S(CSN_ARGWHERE),              0, ":CsnArr;",   ":CsnArr;:CsnArr;",     (SUBR) csnarray_argwhere,       NULL, (SUBR) csnarray_argwhere_deinit,      NULL, 0 },
    { "csnargnonzero",         S(CSN_ARGWHERE),              0, ":CsnArr;",   ":CsnArr;",             (SUBR) csnarray_argnonzero,     NULL, (SUBR) csnarray_argwhere_deinit,      NULL, 0 },
    { "csnargisnan",           S(CSN_ARGWHERE),              0, ":CsnArr;",   ":CsnArr;",             (SUBR) csnarray_argisnan,       NULL, (SUBR) csnarray_argwhere_deinit,      NULL, 0 },
    { "csnargunique",          S(CSN_ARGWHERE),              0, ":CsnArr;",   ":CsnArr;",             (SUBR) csnarray_argunique,      NULL, (SUBR) csnarray_argwhere_deinit,      NULL, 0 },
    { "csnunique",             S(CSN_COMPARE),               0, ":CsnArr;",   ":CsnArr;",             (SUBR) csnarray_unique,         NULL, (SUBR) csnarray_compare_deinit,       NULL, 0 },
    { "csngt",                 S(CSN_COMPARE),               0, ":CsnArr;",   ":CsnArr;i",            (SUBR) csnarray_greater_than,   NULL, (SUBR) csnarray_compare_deinit,       NULL, 0 },
    { "csnlt",                 S(CSN_COMPARE),               0, ":CsnArr;",   ":CsnArr;i",            (SUBR) csnarray_less_than,      NULL, (SUBR) csnarray_compare_deinit,       NULL, 0 },
    { "csnne",                 S(CSN_COMPARE),               0, ":CsnArr;",   ":CsnArr;i",            (SUBR) csnarray_not_equal,      NULL, (SUBR) csnarray_compare_deinit,       NULL, 0 },
    { "csncnteq",              S(CSN_COUNT),                 0, "i",          ":CsnArr;i",            (SUBR) csnarray_count_equal,    NULL, NULL,                                 NULL, 0 },
    { "csncntnz",              S(CSN_COUNT),                 0, "i",          ":CsnArr;",             (SUBR) csnarray_count_nonzero,  NULL, NULL,                                 NULL, 0 },
    { "csncntnan",             S(CSN_COUNT),                 0, "i",          ":CsnArr;",             (SUBR) csnarray_count_nan,      NULL, NULL,                                 NULL, 0 },
    { "csnsum",                S(CSN_REDUCTION),             0, ":CsnArr;",   ":CsnArr;i",            (SUBR) csnarray_sum,            NULL, (SUBR) csnarray_reduction_deinit,     NULL, 0 },
    { "csnprod",               S(CSN_REDUCTION_SCALAR),      0, "i",          ":CsnArr;",             (SUBR) csnarray_prod_all,       NULL, NULL,                                 NULL, 0 },
    { "csnsub",                S(CSN_REDUCTION_SCALAR),      0, "i",          ":CsnArr;",             (SUBR) csnarray_sub_all,        NULL, NULL,                                 NULL, 0 },
    { "csnmean",               S(CSN_REDUCTION_SCALAR),      0, "i",          ":CsnArr;",             (SUBR) csnarray_mean_all,       NULL, NULL,                                 NULL, 0 },
    { "csnmin",                S(CSN_REDUCTION_SCALAR),      0, "i",          ":CsnArr;",             (SUBR) csnarray_min_all,        NULL, NULL,                                 NULL, 0 },
    { "csnmax",                S(CSN_REDUCTION_SCALAR),      0, "i",          ":CsnArr;",             (SUBR) csnarray_max_all,        NULL, NULL,                                 NULL, 0 },
    { "csnall",                S(CSN_REDUCTION_SCALAR),      0, "i",          ":CsnArr;",             (SUBR) csnarray_all_all,        NULL, NULL,                                 NULL, 0 },
    { "csnany",                S(CSN_REDUCTION_SCALAR),      0, "i",          ":CsnArr;",             (SUBR) csnarray_any_all,        NULL, NULL,                                 NULL, 0 },
    { "csnmedian",             S(CSN_REDUCTION_SCALAR),      0, "i",          ":CsnArr;",             (SUBR) csnarray_median_all,     NULL, NULL,                                 NULL, 0 },
    { "csnstd",                S(CSN_REDUCTION_SCALAR),      0, "i",          ":CsnArr;",             (SUBR) csnarray_std_all,        NULL, NULL,                                 NULL, 0 },
    { "csnvar",                S(CSN_REDUCTION_SCALAR),      0, "i",          ":CsnArr;",             (SUBR) csnarray_var_all,        NULL, NULL,                                 NULL, 0 },
    { "csnsum.ax",             S(CSN_REDUCTION_SCALAR),      0, "i",          ":CsnArr;",             (SUBR) csnarray_sum_all,        NULL, NULL,                                 NULL, 0 },
    { "csnprod.ax",            S(CSN_REDUCTION),             0, ":CsnArr;",   ":CsnArr;i",            (SUBR) csnarray_prod,           NULL, (SUBR) csnarray_reduction_deinit,     NULL, 0 },
    { "csnsub.ax",             S(CSN_REDUCTION),             0, ":CsnArr;",   ":CsnArr;i",            (SUBR) csnarray_sub,            NULL, (SUBR) csnarray_reduction_deinit,     NULL, 0 },
    { "csnmean.ax",            S(CSN_REDUCTION),             0, ":CsnArr;",   ":CsnArr;i",            (SUBR) csnarray_mean,           NULL, (SUBR) csnarray_reduction_deinit,     NULL, 0 },
    { "csnmin.ax",             S(CSN_REDUCTION),             0, ":CsnArr;",   ":CsnArr;i",            (SUBR) csnarray_min,            NULL, (SUBR) csnarray_reduction_deinit,     NULL, 0 },
    { "csnmax.ax",             S(CSN_REDUCTION),             0, ":CsnArr;",   ":CsnArr;i",            (SUBR) csnarray_max,            NULL, (SUBR) csnarray_reduction_deinit,     NULL, 0 },
    { "csnany.ax",             S(CSN_REDUCTION),             0, ":CsnArr;",   ":CsnArr;i",            (SUBR) csnarray_any,            NULL, (SUBR) csnarray_reduction_deinit,     NULL, 0 },
    { "csnall.ax",             S(CSN_REDUCTION),             0, ":CsnArr;",   ":CsnArr;i",            (SUBR) csnarray_all,            NULL, (SUBR) csnarray_reduction_deinit,     NULL, 0 },
    { "csnmedian.ax",          S(CSN_REDUCTION),             0, ":CsnArr;",   ":CsnArr;i",            (SUBR) csnarray_median,         NULL, (SUBR) csnarray_reduction_deinit,     NULL, 0 },
    { "csnstd.ax",             S(CSN_REDUCTION),             0, ":CsnArr;",   ":CsnArr;i",            (SUBR) csnarray_std,            NULL, (SUBR) csnarray_reduction_deinit,     NULL, 0 },
    { "csnvar.ax",             S(CSN_REDUCTION),             0, ":CsnArr;",   ":CsnArr;i",            (SUBR) csnarray_var,            NULL, (SUBR) csnarray_reduction_deinit,     NULL, 0 },
    { "csnargmin",             S(CSN_REDUCTION),             0, ":CsnArr;",   ":CsnArr;j",            (SUBR) csnarray_argmin,         NULL, (SUBR) csnarray_reduction_deinit,     NULL, 0 },
    { "csnargmax",             S(CSN_REDUCTION),             0, ":CsnArr;",   ":CsnArr;j",            (SUBR) csnarray_argmax,         NULL, (SUBR) csnarray_reduction_deinit,     NULL, 0 },
    { "csnadd",                S(CSN_BINOP_HH),              0, ":CsnArr;",   ":CsnArr;:CsnArr;",     (SUBR) csnarray_add_hh,         NULL, (SUBR) csnarray_opbin_deinit,         NULL, 0 },
    { "csnadd.hs",             S(CSN_BINOP_HS),              0, ":CsnArr;",   ":CsnArr;i",            (SUBR) csnarray_add_hs,         NULL, (SUBR) csnarray_opbin_deinit,         NULL, 0 },
    { "csnsubtract.hh",        S(CSN_BINOP_HH),              0, ":CsnArr;",   ":CsnArr;:CsnArr;",     (SUBR) csnarray_subtract_hh,    NULL, (SUBR) csnarray_opbin_deinit,         NULL, 0 },
    { "csnsubtract.hs",        S(CSN_BINOP_HS),              0, ":CsnArr;",   ":CsnArr;i",            (SUBR) csnarray_subtract_hs,    NULL, (SUBR) csnarray_opbin_deinit,         NULL, 0 },
    { "csnsubtract.sh",        S(CSN_BINOP_SH),              0, ":CsnArr;",   "i:CsnArr;",            (SUBR) csnarray_subtract_sh,    NULL, (SUBR) csnarray_opbin_deinit,         NULL, 0 },
    { "csnmul.hh",             S(CSN_BINOP_HH),              0, ":CsnArr;",   ":CsnArr;:CsnArr;",     (SUBR) csnarray_mul_hh,         NULL, (SUBR) csnarray_opbin_deinit,         NULL, 0 },
    { "csnmul.hs",             S(CSN_BINOP_HS),              0, ":CsnArr;",   ":CsnArr;i",            (SUBR) csnarray_mul_hs,         NULL, (SUBR) csnarray_opbin_deinit,         NULL, 0 },
    { "csndiv.hh",             S(CSN_BINOP_HH),              0, ":CsnArr;",   ":CsnArr;:CsnArr;",     (SUBR) csnarray_div_hh,         NULL, (SUBR) csnarray_opbin_deinit,         NULL, 0 },
    { "csndiv.hs",             S(CSN_BINOP_HS),              0, ":CsnArr;",   ":CsnArr;i",            (SUBR) csnarray_div_hs,         NULL, (SUBR) csnarray_opbin_deinit,         NULL, 0 },
    { "csndiv.sh",             S(CSN_BINOP_SH),              0, ":CsnArr;",   "i:CsnArr;",            (SUBR) csnarray_div_sh,         NULL, (SUBR) csnarray_opbin_deinit,         NULL, 0 },
    { "csnpow.hh",             S(CSN_BINOP_HH),              0, ":CsnArr;",   ":CsnArr;:CsnArr;",     (SUBR) csnarray_pow_hh,         NULL, (SUBR) csnarray_opbin_deinit,         NULL, 0 },
    { "csnpow.hs",             S(CSN_BINOP_HS),              0, ":CsnArr;",   ":CsnArr;i",            (SUBR) csnarray_pow_hs,         NULL, (SUBR) csnarray_opbin_deinit,         NULL, 0 },
    { "csnpow.sh",             S(CSN_BINOP_SH),              0, ":CsnArr;",   "i:CsnArr;",            (SUBR) csnarray_pow_sh,         NULL, (SUBR) csnarray_opbin_deinit,         NULL, 0 },
    { "csnlog.hh",             S(CSN_BINOP_HH),              0, ":CsnArr;",   ":CsnArr;:CsnArr;",     (SUBR) csnarray_log_hh,         NULL, (SUBR) csnarray_opbin_deinit,         NULL, 0 },
    { "csnlog.hs",             S(CSN_BINOP_HS),              0, ":CsnArr;",   ":CsnArr;i",            (SUBR) csnarray_log_hs,         NULL, (SUBR) csnarray_opbin_deinit,         NULL, 0 },
    { "csnlog.sh",             S(CSN_BINOP_SH),              0, ":CsnArr;",   "i:CsnArr;",            (SUBR) csnarray_log_sh,         NULL, (SUBR) csnarray_opbin_deinit,         NULL, 0 },
    { "csnsqrt",               S(CSN_UNARYOP),               0, ":CsnArr;",   ":CsnArr;",             (SUBR) csnarray_sqrt,           NULL, (SUBR) csnarray_opunary_deinit,       NULL, 0 },
    { "csncbrt",               S(CSN_UNARYOP),               0, ":CsnArr;",   ":CsnArr;",             (SUBR) csnarray_cbrt,           NULL, (SUBR) csnarray_opunary_deinit,       NULL, 0 },
    { "csnabs",                S(CSN_UNARYOP),               0, ":CsnArr;",   ":CsnArr;",             (SUBR) csnarray_abs,            NULL, (SUBR) csnarray_opunary_deinit,       NULL, 0 },
    { "csnexp",                S(CSN_UNARYOP),               0, ":CsnArr;",   ":CsnArr;",             (SUBR) csnarray_exp,            NULL, (SUBR) csnarray_opunary_deinit,       NULL, 0 },
    { "csnsin",                S(CSN_UNARYOP),               0, ":CsnArr;",   ":CsnArr;",             (SUBR) csnarray_sin,            NULL, (SUBR) csnarray_opunary_deinit,       NULL, 0 },
    { "csncos",                S(CSN_UNARYOP),               0, ":CsnArr;",   ":CsnArr;",             (SUBR) csnarray_cos,            NULL, (SUBR) csnarray_opunary_deinit,       NULL, 0 },
    { "csntan",                S(CSN_UNARYOP),               0, ":CsnArr;",   ":CsnArr;",             (SUBR) csnarray_tan,            NULL, (SUBR) csnarray_opunary_deinit,       NULL, 0 },
    { "csnasin",               S(CSN_UNARYOP),               0, ":CsnArr;",   ":CsnArr;",             (SUBR) csnarray_asin,           NULL, (SUBR) csnarray_opunary_deinit,       NULL, 0 },
    { "csnacos",               S(CSN_UNARYOP),               0, ":CsnArr;",   ":CsnArr;",             (SUBR) csnarray_acos,           NULL, (SUBR) csnarray_opunary_deinit,       NULL, 0 },
    { "csnatan",               S(CSN_UNARYOP),               0, ":CsnArr;",   ":CsnArr;",             (SUBR) csnarray_atan,           NULL, (SUBR) csnarray_opunary_deinit,       NULL, 0 },
    { "csnsinh",               S(CSN_UNARYOP),               0, ":CsnArr;",   ":CsnArr;",             (SUBR) csnarray_sinh,           NULL, (SUBR) csnarray_opunary_deinit,       NULL, 0 },
    { "csncosh",               S(CSN_UNARYOP),               0, ":CsnArr;",   ":CsnArr;",             (SUBR) csnarray_cosh,           NULL, (SUBR) csnarray_opunary_deinit,       NULL, 0 },
    { "csntanh",               S(CSN_UNARYOP),               0, ":CsnArr;",   ":CsnArr;",             (SUBR) csnarray_tanh,           NULL, (SUBR) csnarray_opunary_deinit,       NULL, 0 },
    { "csnasinh",              S(CSN_UNARYOP),               0, ":CsnArr;",   ":CsnArr;",             (SUBR) csnarray_asinh,          NULL, (SUBR) csnarray_opunary_deinit,       NULL, 0 },
    { "csnacosh",              S(CSN_UNARYOP),               0, ":CsnArr;",   ":CsnArr;",             (SUBR) csnarray_acosh,          NULL, (SUBR) csnarray_opunary_deinit,       NULL, 0 },
    { "csnatanh",              S(CSN_UNARYOP),               0, ":CsnArr;",   ":CsnArr;",             (SUBR) csnarray_atanh,          NULL, (SUBR) csnarray_opunary_deinit,       NULL, 0 },
    { "csnfloor",              S(CSN_UNARYOP),               0, ":CsnArr;",   ":CsnArr;",             (SUBR) csnarray_floor,          NULL, (SUBR) csnarray_opunary_deinit,       NULL, 0 },
    { "csnceil",               S(CSN_UNARYOP),               0, ":CsnArr;",   ":CsnArr;",             (SUBR) csnarray_ceil,           NULL, (SUBR) csnarray_opunary_deinit,       NULL, 0 },
    { "csnround",              S(CSN_UNARYOP),               0, ":CsnArr;",   ":CsnArr;",             (SUBR) csnarray_round,          NULL, (SUBR) csnarray_opunary_deinit,       NULL, 0 },
    { "csnsign",               S(CSN_UNARYOP),               0, ":CsnArr;",   ":CsnArr;",             (SUBR) csnarray_sign,           NULL, (SUBR) csnarray_opunary_deinit,       NULL, 0 },
    { "csndot",                S(CSN_BINOP_HH),              0, ":CsnArr;",   ":CsnArr;:CsnArr;",     (SUBR) csnarray_dot,            NULL, (SUBR) csnarray_opbin_deinit,         NULL, 0 },
    { "csndot.s",              S(CSN_BINOP_HH_SCALAR),       0, "i",          ":CsnArr;:CsnArr;",     (SUBR) csnarray_dot_scalar,     NULL, NULL,                                 NULL, 0 },
    { "csninner",              S(CSN_BINOP_HH),              0, ":CsnArr;",   ":CsnArr;:CsnArr;",     (SUBR) csnarray_inner,          NULL, (SUBR) csnarray_opbin_deinit,         NULL, 0 },
    { "csninner.s",            S(CSN_BINOP_HH_SCALAR),       0, "i",          ":CsnArr;:CsnArr;",     (SUBR) csnarray_inner_scalar,   NULL, NULL,                                 NULL, 0 },
    { "csnouter",              S(CSN_BINOP_HH),              0, ":CsnArr;",   ":CsnArr;:CsnArr;",     (SUBR) csnarray_outer,          NULL, (SUBR) csnarray_opbin_deinit,         NULL, 0 },
    { "csnnorm",               S(CSN_NORM_REDUCTION),        0, ":CsnArr;",   ":CsnArr;ip",           (SUBR) csnarray_norm,           NULL, (SUBR) csnarray_norm_deinit,          NULL, 0 },
    { "csnnorm.s",             S(CSN_NORM_REDUCTION_SCALAR), 0, "i",          ":CsnArr;p",            (SUBR) csnarray_norm_scalar,    NULL, NULL,                                 NULL, 0 },
    { "csnnormalize",          S(CSN_UNARYOP_AX),            0, ":CsnArr;",   ":CsnArr;jp",           (SUBR) csnarray_normalize,      NULL, (SUBR) csnarray_opunary_ax_deinit,    NULL, 0 },
    { "csnnormalize.in",       S(CSN_UNARYOP_AX_IN),         0, "",           ":CsnArr;jp",           (SUBR) csnarray_normalize_in,   NULL, NULL,                                 NULL, 0 },
    { "csnpairdist",           S(CSN_BINOP_HH),              0, ":CsnArr;",   ":CsnArr;:CsnArr;",     (SUBR) csnarray_pair_distance,  NULL, (SUBR) csnarray_opbin_deinit,         NULL, 0 },
    { "csndist",               S(CSN_BINOP_HH_SCALAR),       0, "i",          ":CsnArr;:CsnArr;p",    (SUBR) csnarray_distance,       NULL, NULL,                                 NULL, 0 },
    { "csnangle",              S(CSN_BINOP_HH_SCALAR),       0, "i",          ":CsnArr;:CsnArr;",     (SUBR) csnarray_angle,          NULL, NULL,                                 NULL, 0 },
    { "csnproject",            S(CSN_BINOP_HH),              0, ":CsnArr;",   ":CsnArr;:CsnArr;",     (SUBR) csnarray_project,        NULL, (SUBR) csnarray_opbin_deinit,         NULL, 0 },
    { "csnreject",             S(CSN_BINOP_HH),              0, ":CsnArr;",   ":CsnArr;:CsnArr;",     (SUBR) csnarray_reject,         NULL, (SUBR) csnarray_opbin_deinit,         NULL, 0 },
    { "csnreflect",            S(CSN_BINOP_HH),              0, ":CsnArr;",   ":CsnArr;:CsnArr;",     (SUBR) csnarray_reflect,        NULL, (SUBR) csnarray_opbin_deinit,         NULL, 0 },
    { "csncross",              S(CSN_BINOP_HH),              0, ":CsnArr;",   ":CsnArr;:CsnArr;",     (SUBR) csnarray_cross,          NULL, (SUBR) csnarray_opbin_deinit,         NULL, 0 },
    { "csndiff",               S(CSN_UNARYOP_AX),            0, ":CsnArr;",   ":CsnArr;j",            (SUBR) csnarray_diff,           NULL, (SUBR) csnarray_opunary_ax_deinit,    NULL, 0 },
    { "csncumsum",             S(CSN_UNARYOP_AX),            0, ":CsnArr;",   ":CsnArr;j",            (SUBR) csnarray_cumsum,         NULL, (SUBR) csnarray_opunary_ax_deinit,    NULL, 0 },
    { "csncumprod",            S(CSN_UNARYOP_AX),            0, ":CsnArr;",   ":CsnArr;j",            (SUBR) csnarray_cumprod,        NULL, (SUBR) csnarray_opunary_ax_deinit,    NULL, 0 },
    { "csngrad",               S(CSN_UNARYOP_AX),            0, ":CsnArr;",   ":CsnArr;j",            (SUBR) csnarray_gradient,       NULL, (SUBR) csnarray_opunary_ax_deinit,    NULL, 0 },
    { "csnmatmul",             S(CSN_BINOP_HH),              0, ":CsnArr;",   ":CsnArr;:CsnArr;",     (SUBR) csnarray_matmul,         NULL, (SUBR) csnarray_opbin_deinit,         NULL, 0 },
    { "csnmatmul.s",           S(CSN_BINOP_HH_SCALAR),       0, "i",          ":CsnArr;:CsnArr;",     (SUBR) csnarray_matmul_scalar,  NULL, NULL,                                 NULL, 0 },
    { "csntrace",              S(CSN_UNARYOP_SCALAR),        0, "i",          ":CsnArr;",             (SUBR) csnarray_trace,          NULL, NULL,                                 NULL, 0 },
    { "csndiag",               S(CSN_UNARYOP),               0, ":CsnArr;",   ":CsnArr;",             (SUBR) csnarray_diag,           NULL, (SUBR) csnarray_opunary_deinit,       NULL, 0 },
    { "csnmovmean",            S(CSN_MOVSTATS),              0, ":CsnArr;",   ":CsnArr;ij",           (SUBR) csnarray_movmean,        NULL, (SUBR) csnarray_movstats_deinit,      NULL, 0 },
    { "csnmovmean.in",         S(CSN_MOVSTATS_IN),           0, "",           ":CsnArr;ij",           (SUBR) csnarray_movmean_in,     NULL, NULL,                                 NULL, 0 },
    { "csnmovmedian",          S(CSN_MOVSTATS),              0, ":CsnArr;",   ":CsnArr;ij",           (SUBR) csnarray_movmedian,      NULL, (SUBR) csnarray_movstats_deinit,      NULL, 0 },
    { "csnmovmedian.in",       S(CSN_MOVSTATS_IN),           0, "",           ":CsnArr;ij",           (SUBR) csnarray_movmedian_in,   NULL, NULL,                                 NULL, 0 },
    { "csnmovstd",             S(CSN_MOVSTATS),              0, ":CsnArr;",   ":CsnArr;ij",           (SUBR) csnarray_movstd,         NULL, (SUBR) csnarray_movstats_deinit,      NULL, 0 },
    { "csnmovstd.in",          S(CSN_MOVSTATS_IN),           0, "",           ":CsnArr;ij",           (SUBR) csnarray_movstd_in,      NULL, NULL,                                 NULL, 0 },
    { "csnmovvar",             S(CSN_MOVSTATS),              0, ":CsnArr;",   ":CsnArr;ij",           (SUBR) csnarray_movvar,         NULL, (SUBR) csnarray_movstats_deinit,      NULL, 0 },
    { "csnmovvar.in",          S(CSN_MOVSTATS_IN),           0, "",           ":CsnArr;ij",           (SUBR) csnarray_movvar_in,      NULL, NULL,                                 NULL, 0 },
    { "csnmovmin",             S(CSN_MOVSTATS),              0, ":CsnArr;",   ":CsnArr;ij",           (SUBR) csnarray_movmin,         NULL, (SUBR) csnarray_movstats_deinit,      NULL, 0 },
    { "csnmovmin.in",          S(CSN_MOVSTATS_IN),           0, "",           ":CsnArr;ij",           (SUBR) csnarray_movmin_in,      NULL, NULL,                                 NULL, 0 },
    { "csnmovmax",             S(CSN_MOVSTATS),              0, ":CsnArr;",   ":CsnArr;ij",           (SUBR) csnarray_movmax,         NULL, (SUBR) csnarray_movstats_deinit,      NULL, 0 },
    { "csnmovmax.in",          S(CSN_MOVSTATS_IN),           0, "",           ":CsnArr;ij",           (SUBR) csnarray_movmax_in,      NULL, NULL,                                 NULL, 0 },
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
