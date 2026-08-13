#include "csnum.h"
#include "csnregistry.h"
#include <float.h>
#include <csdl.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
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
        csound->ErrorMsg(csound, "[csnarray] Internal registry memory error\n");
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
    return csnarray_deinit_by_handle(csound, &p->handle_id, &p->array, &p->h);
}

static int32_t from_array_to_csnarray_deinit(CSOUND *csound, CSN_FROM_ARRAY *p) {
    return csnarray_deinit_by_handle(csound, &p->handle_id, &p->array, &p->h);
}

static int32_t csnarray_space_spaced_deinit(CSOUND *csound, CSN_SPACED_SPACE *p) {
    return csnarray_deinit_by_handle(csound, &p->handle_id, &p->array, &p->h);
}

static int32_t csnarray_shape_deinit(CSOUND *csound, CSN_RESHAPE *p) {
    return csnarray_deinit_by_handle(csound, &p->handle_id, &p->array, &p->h);
}

static int32_t csnarray_identity_deinit(CSOUND *csound, CSN_IDENTITY *p) {
    return csnarray_deinit_by_handle(csound, &p->handle_id, &p->array, &p->h);
}

static int32_t csnarray_flip_deinit(CSOUND *csound, CSN_FLIP_ROLL *p) {
    return csnarray_deinit_by_handle(csound, &p->handle_id, &p->array, &p->h);
}

static int32_t csnarray_take_deinit(CSOUND *csound, CSN_TAKE *p) {
    return csnarray_deinit_by_handle(csound, &p->handle_id, &p->array, &p->h);
}

static int32_t csnarray_slice_deinit(CSOUND *csound, CSN_GET_SLICE *p) {
    return csnarray_deinit_by_handle(csound, &p->handle_id, &p->array, &p->h);
}

static int32_t csnarray_concat_deinit(CSOUND *csound, CSN_CONCAT *p) {
    return csnarray_deinit_by_handle(csound, &p->handle_id, &p->array, &p->h);
}

static int32_t csnarray_pad_deinit(CSOUND *csound, CSN_PAD *p) {
    return csnarray_deinit_by_handle(csound, &p->handle_id, &p->array, &p->h);
}

static int32_t csnarray_clip_deinit(CSOUND *csound, CSN_CLIP *p) {
    return csnarray_deinit_by_handle(csound, &p->handle_id, &p->array, &p->h);
}

static int32_t csnarray_argwhere_deinit(CSOUND *csound, CSN_ARGWHERE *p) {
    return csnarray_deinit_by_handle(csound, &p->handle_id, &p->array, &p->h);
}

static int32_t csnarray_compare_deinit(CSOUND *csound, CSN_COMPARE *p) {
    return csnarray_deinit_by_handle(csound, &p->handle_id, &p->array, &p->h);
}

static int32_t csnarray_reduction_deinit(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_deinit_by_handle(csound, &p->handle_id, &p->array, &p->h);
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
        return csound->InitError(csound, "[csnarray] Invalid shape array\n");
    }

    if (p_shape->sizes[0] > CSN_MAX_DIMS) {
        return csound->InitError(csound, "[csnarray] Shape dimension must be less or equal to %d\n", CSN_MAX_DIMS);
    }

    uint32_t ndim = (uint32_t) p_shape->sizes[0];
    for (uint32_t i = 0; i < ndim; i++) {
        MYFLT extent = p_shape->data[i];
        /* 0 is allowed: it produces a zero-length array, the empty stack. */
        if (extent < FL(0.0)) {
            return csound->InitError(csound, "[csnarray] Shape extent %u must be >= 0\n", i);
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
    uint32_t *p_handle_id,
    MYFLT *p_handle,
    const uint32_t *protect,
    uint32_t protect_count,
    const char **err
) {
    if (handle_out_is_global(h)) {
        uint32_t previous_handle = (uint32_t) *p_handle;
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
    *p_handle_id = handle;
    *p_handle = (MYFLT) handle;
    return OK;
}

/* Takes the lock itself; for opcodes that hold nothing on entry. */
static int32_t create_csnarray_init(
    CSOUND *csound,
    const OPDS *h,
    uint32_t ndim,
    const uint32_t *shape,
    CSN_ARRAY **p_array,
    uint32_t *p_handle_id,
    MYFLT *p_handle
) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] Internal registry memory error\n");
    }

    const char *err = NULL;

    csound->LockMutex(reg->mutex);
    /* Creation opcodes read no source array, so nothing needs protecting. */
    int32_t res = create_csnarray_locked(csound, reg, h, ndim, shape, p_array, p_handle_id, p_handle, NULL, 0, &err);
    csound->UnlockMutex(reg->mutex);

    if (res != OK) {
        return csound->InitError(csound, "[csnarray] %s\n", err);
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

    return create_csnarray_init(csound, &p->h, ndim, shape, &p->array, &p->handle_id, p->handle);
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
        return csound->InitError(csound, "[csnarray] Invalid source array\n");
    }

    uint32_t ndim = (uint32_t) p->source->dimensions;
    uint32_t shape[CSN_MAX_DIMS] = {0};
    size_t total_size = 1;

    for (uint32_t i = 0; i < ndim; i++) {
        int32_t size = p->source->sizes[i];
        if (size <= 0) {
            return csound->InitError(csound, "[csnarray] Source extent %u must be >= 1\n", i);
        }
        shape[i] = (uint32_t) size;
        total_size *= (size_t) size;
    }

    int32_t res_init = create_csnarray_init(csound, &p->h, ndim, shape, &p->array, &p->handle_id, p->handle);
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
        return csound->InitError(csound, "[csnarray] NULL registry\n");
    }

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, (uint32_t) *p->source_handle);
    if (slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->InitError(csound, "[csnarray] Invalid handle\n");
    }

    CSN_ARRAY *src = slot->array;
    uint32_t ndim = src->ndim;

    /* dimensions arrives pre-set from the declaration, with sizes[] already
       allocated to match. A 0 means the variable carries no rank yet, which
       tabinit would resolve to 1-D. */
    int32_t declared = p->array->dimensions > 0 ? p->array->dimensions : 1;
    if ((uint32_t) declared != ndim) {
        csound->UnlockMutex(reg->mutex);
        return csound->InitError(csound, "[csnarray] Handle holds a %u-D array but the output is declared %d-D; declare it with %u bracket pairs\n", ndim, declared, ndim);
    }

    size_t total_size = src->size;
    if (total_size > (size_t) INT32_MAX) {
        csound->UnlockMutex(reg->mutex);
        return csound->InitError(csound, "[csnarray] Array too large for an i-array output\n");
    }

    tabinit(csound, p->array, (int32_t) total_size, p->h.insdshead);
    if (p->array->data == NULL || p->array->sizes == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->InitError(csound, "[csnarray] Output array allocation failed\n");
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
        return csound->InitError(csound, "[csnarray] NULL registry\n");
    }

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, (uint32_t) *p->handle);
    if (slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->InitError(csound, "[csnarray] Invalid handle\n");
    }

    release_slot(csound, reg, slot);

    csound->UnlockMutex(reg->mutex);

    return OK;
}

// get dims
int32_t csnarray_dims(CSOUND *csound, CSN_SIZE_DIMS *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] NULL registry\n");
    }

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, (uint32_t) *p->handle);
    if (slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->InitError(csound, "[csnarray] Invalid handle\n");
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
        return csound->InitError(csound, "[csnarray] NULL registry\n");
    }

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, (uint32_t) *p->handle);
    if (slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->InitError(csound, "[csnarray] Invalid handle\n");
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
        return csound->InitError(csound, "[csnarray] NULL registry\n");
    }

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, (uint32_t) *p->handle);
    if (slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->InitError(csound, "[csnarray] Invalid handle\n");
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
        return csound->InitError(csound, "[csnarray] NULL registry\n");
    }

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, (uint32_t) *p->handle);
    if (slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->InitError(csound, "[csnarray] Invalid handle\n");
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
        return csound->InitError(csound, "[csnarray] step param cannot be zero");
    }

    double start = (double) *p->start;
    double stop = (double) *p->stop;
    if ((stop > start && step < 0) || (stop < start && step > 0)) {
        return csound->InitError(csound, "[csnarray] step has the wrong sign for the requested range");
    }

    int32_t size = (int32_t) ceil((stop - start) / step);
    if (size == 0) {
        return csound->InitError(csound, "[csnarray] Array size zero");
    }

    uint32_t usize = (uint32_t) size;

    int32_t res_init = create_csnarray_init(csound, &p->h, 1U, &usize, &p->array, &p->handle_id, p->handle);
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
        return csound->InitError(csound, "[csnarray] num param must be greater than zero");
    }

    double start = (double) *p->start;
    double stop = (double) *p->stop;

    uint32_t usize = (uint32_t) num;

    int32_t res_init = create_csnarray_init(csound, &p->h, 1U, &usize, &p->array, &p->handle_id, p->handle);
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
        return csound->InitError(csound, "[csnarray] num param must be greater than zero");
    }

    if (p->base == NULL) {
        return csound->InitError(csound, "[csnarray] Null base param");
    }

    double base = (double) *p->base;
    if (base <= 0) {
        return csound->InitError(csound, "[csnarray] base param must greater than zero");
    }

    double start = (double) *p->start;
    double stop = (double) *p->stop;

    uint32_t usize = (uint32_t) num;

    int32_t res_init = create_csnarray_init(csound, &p->h, 1U, &usize, &p->array, &p->handle_id, p->handle);
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
        return csound->InitError(csound, "[csnarray] num param must be greater than zero");
    }

    double start = (double) *p->start;
    double stop = (double) *p->stop;

    uint32_t usize = (uint32_t) num;

    int32_t res_init = create_csnarray_init(csound, &p->h, 1U, &usize, &p->array, &p->handle_id, p->handle);
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
        return csound->InitError(csound, "[csnarray] num param must be greater than zero");
    }

    uint32_t shape[CSN_MAX_DIMS] = {0};
    shape[0] = num;
    shape[1] = num;

    int32_t res_init = create_csnarray_init(csound, &p->h, 2U, shape, &p->array, &p->handle_id, p->handle);
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
        return csound->InitError(csound, "[csnarray] NULL registry");
    }

    uint32_t source_handle = (uint32_t) *p->source_handle;

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
        res = csound->InitError(csound, "[csnarray] Invalid source handle");
        goto done;
    }

    CSN_ARRAY *arr = slot->array;

    /* Validated before allocating, so a rejected reshape does not publish a
       handle to a destination nobody asked for. */
    if (new_size != arr->size) {
        res = csound->InitError(csound, "[csnarray] reshape size mismatch: source has %zu elements, new shape requires %zu", arr->size, new_size);
        goto done;
    }

    if (create_csnarray_locked(csound, reg, &p->h, ndim, shape, &p->array, &p->handle_id, p->handle, &source_handle, 1U, &err) != OK) {
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
        return csound->InitError(csound, "[csnarray] NULL registry");
    }

    uint32_t source_handle = (uint32_t) *p->source_handle;


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
        res = csound->InitError(csound, "[csnarray] Invalid source handle");
        goto done;
    }

    CSN_ARRAY *arr = slot->array;

    if (new_size != arr->size) {
        res = csound->InitError(csound, "[csnarray] reshape size mismatch: source has %zu elements, new shape requires %zu", arr->size, new_size);
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
        return csound->InitError(csound, "[csnarray] NULL registry");
    }

    uint32_t source_handle = (uint32_t) *p->source_handle;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Invalid source handle");
        goto done;
    }

    CSN_ARRAY *arr = slot->array;

    uint32_t shape[CSN_MAX_DIMS] = {0};
    shape[0] = (uint32_t) arr->size;

    /* _locked: the registry mutex is already held and is not recursive. */
    if (create_csnarray_locked(csound, reg, &p->h, 1U, shape, &p->array, &p->handle_id, p->handle, &source_handle, 1U, &err) != OK) {
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
        return csound->InitError(csound, "[csnarray] NULL registry");
    }

    uint32_t source_handle = (uint32_t) *p->source_handle;

    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Invalid source handle");
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
        return csound->InitError(csound, "[csnarray] NULL registry");
    }

    uint32_t source_handle = (uint32_t) *p->source_handle;

    /* INOCOUNT reflects what the orchestra actually passed, so the no-axes
       overload does not depend on new_shape happening to be NULL. */
    bool is_default = p->INOCOUNT < 2;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Invalid source handle");
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
            res = csound->InitError(csound, "[csnarray] Invalid axes configuration");
            goto done;
        }

        bool used[CSN_MAX_DIMS] = {false};

        for (uint32_t i = 0; i < ndim; ++i) {
            uint32_t axis = (uint32_t) p->new_shape->data[i];

            if (axis >= ndim || used[axis]) {
                res = csound->InitError(csound, "[csnarray] Invalid axes permutation");
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
    if (create_csnarray_locked(csound, reg, &p->h, ndim, shape, &p->array, &p->handle_id, p->handle, &source_handle, 1U, &err) != OK) {
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
        return csound->InitError(csound, "[csnarray] NULL registry");
    }

    uint32_t source_handle = (uint32_t) *p->source_handle;

    /* INOCOUNT reflects what the orchestra actually passed, so the no-axes
       overload does not depend on new_shape happening to be NULL. */
    bool is_default = p->INOCOUNT < 2;

    int32_t res = OK;
    double *data = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Invalid source handle");
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
            res = csound->InitError(csound, "[csnarray] Invalid axes configuration");
            goto done;
        }

        bool used[CSN_MAX_DIMS] = {false};

        for (uint32_t i = 0; i < ndim; ++i) {
            uint32_t axis = (uint32_t)p->new_shape->data[i];

            if (axis >= ndim || used[axis]) {
                res = csound->InitError(csound, "[csnarray] Invalid axes permutation");
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
        res = csound->InitError(csound, "[csnarray] Memory allocation failed");
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
        return csound->InitError(csound, "[csnarray] NULL registry");
    }

    uint32_t source_handle = (uint32_t) *p->source_handle;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Invalid source handle");
        goto done;
    }

    CSN_ARRAY *arr = slot->array;

    uint32_t ndim = arr->ndim;

    int32_t axis_flip = (int32_t) *p->param_a;
    if (axis_flip < -1 || axis_flip > (int32_t) ndim - 1) {
        res = csound->InitError(csound, "[csnarray] Invalid flip axis");
        goto done;
    }

    /* _locked: the registry mutex is already held and is not recursive. */
    if (create_csnarray_locked(csound, reg, &p->h, ndim, arr->shape, &p->array, &p->handle_id, p->handle, &source_handle, 1U, &err) != OK) {
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
        return csound->InitError(csound, "[csnarray] NULL registry");
    }

    uint32_t source_handle = (uint32_t) *p->source_handle;

    int32_t res = OK;
    double *data = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Invalid source handle");
        goto done;
    }

    CSN_ARRAY *arr = slot->array;

    uint32_t ndim = arr->ndim;

    int32_t axis_flip = (int32_t) *p->param_a;
    if (axis_flip < -1 || axis_flip > (int32_t) ndim - 1) {
        res = csound->InitError(csound, "[csnarray] Invalid flip axis");
        goto done;
    }

    /* Allocated only once the axes are known good, so the rejection paths
       above have nothing to release. */
    data = csound->Calloc(csound, sizeof(double) * arr->size);
    if (data == NULL) {
        res = csound->InitError(csound, "[csnarray] Memory allocation failed");
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
        return csound->InitError(csound, "[csnarray] NULL registry");
    }

    uint32_t source_handle = (uint32_t) *p->source_handle;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Invalid source handle");
        goto done;
    }

    CSN_ARRAY *arr = slot->array;
    uint32_t ndim = arr->ndim;
    int32_t shift = (int32_t) *p->param_a;

    /* _locked: the registry mutex is already held and is not recursive. */
    if (create_csnarray_locked(csound, reg, &p->h, ndim, arr->shape, &p->array, &p->handle_id, p->handle, &source_handle, 1U, &err) != OK) {
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
        return csound->InitError(csound, "[csnarray] NULL registry");
    }

    uint32_t source_handle = (uint32_t) *p->source_handle;

    int32_t res = OK;
    double *data = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Invalid source handle");
        goto done;
    }

    CSN_ARRAY *arr = slot->array;
    /* roll works on the flattened array, so the rank plays no part here. */
    int32_t shift = (int32_t) *p->param_a;

    data = csound->Calloc(csound, sizeof(double) * arr->size);
    if (data == NULL) {
        res = csound->InitError(csound, "[csnarray] Memory allocation failed");
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
        return csound->InitError(csound, "[csnarray] NULL registry");
    }

    uint32_t source_handle = (uint32_t) *p->source_handle;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Invalid source handle");
        goto done;
    }

    CSN_ARRAY *arr = slot->array;
    uint32_t ndim = arr->ndim;
    int32_t shift = (int32_t) *p->param_a;
    int32_t axis_roll = (int32_t) *p->param_b;
    /* -1 selects every axis; anything below it would index src_coords[] out
       of bounds in the else branch. */
    if (axis_roll < -1 || axis_roll >= (int32_t) ndim) {
        res = csound->InitError(csound, "[csnarray] Invalid source roll axis");
        goto done;
    }

    /* _locked: the registry mutex is already held and is not recursive. */
    if (create_csnarray_locked(csound, reg, &p->h, ndim, arr->shape, &p->array, &p->handle_id, p->handle, &source_handle, 1U, &err) != OK) {
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
        return csound->InitError(csound, "[csnarray] NULL registry");
    }

    uint32_t source_handle = (uint32_t) *p->source_handle;

    int32_t res = OK;
    double *data = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Invalid source handle");
        goto done;
    }

    CSN_ARRAY *arr = slot->array;
    uint32_t ndim = arr->ndim;
    int32_t shift = (int32_t) *p->param_a;
    int32_t axis_roll = (int32_t) *p->param_b;
    /* -1 selects every axis; anything below it would index src_coords[] out
       of bounds in the else branch. */
    if (axis_roll < -1 || axis_roll >= (int32_t) ndim) {
        res = csound->InitError(csound, "[csnarray] Invalid source roll axis");
        goto done;
    }

    /* Allocated only once the axes are known good, so the rejection paths
       above have nothing to release. */
    data = csound->Calloc(csound, sizeof(double) * arr->size);
    if (data == NULL) {
        res = csound->InitError(csound, "[csnarray] Memory allocation failed");
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
            return csound->InitError(csound, "[csnarray] Index must be positive");
        }

        if ((uint32_t) index >= arr->shape[i]) {
            return csound->InitError(csound, "[csnarray] Index out of bounds");
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
        return csound->InitError(csound, "[csnarray] NULL registry slot");
    }

    if (value == NULL) {
        return csound->InitError(csound, "[csnarray] NULL value pointer");
    }

    CSN_ARRAY *arr = slot->array;
    uint32_t ndim = arr->ndim;

    if (indexes == NULL || indexes->data == NULL || indexes->sizes == NULL || indexes->dimensions != 1 || indexes->sizes[0] != (int32_t) ndim) {
        return csound->InitError(csound, "[csnarray] Number of indexes must be equal to array dims");
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
        return csound->InitError(csound, "[csnarray] NULL registry");
    }

    uint32_t handle = (uint32_t) *p->source_handle;

    csound->LockMutex(reg->mutex);
    int32_t res = csnarray_get_set_locked(csound, reg, handle, p->indexes, p->value, true);
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_set(CSOUND *csound, CSN_SET *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] NULL registry");
    }

    uint32_t handle = (uint32_t) *p->source_handle;

    csound->LockMutex(reg->mutex);
    int32_t res = csnarray_get_set_locked(csound, reg, handle, p->indexes, p->value, false);
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_take(CSOUND *csound, CSN_TAKE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] NULL registry");
    }

    uint32_t source_handle = (uint32_t) *p->source_handle;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Invalid source handle");
        goto done;
    }

    CSN_ARRAY *arr = slot->array;
    uint32_t ndim = arr->ndim;
    uint32_t shape[CSN_MAX_DIMS] = {0};

    /* Dropping the only axis would leave a rank-0 array, which the registry
       cannot represent. That case is the two-argument form, which yields a
       plain scalar. */
    if (ndim < 2) {
        res = csound->InitError(csound, "[csnarray] take along an axis needs a 2-D or higher array; use the two-argument form for a scalar");
        goto done;
    }

    uint32_t axis = (uint32_t) *p->axis;
    if (*p->axis < 0 || axis > ndim - 1) {
        res = csound->InitError(csound, "[csnarray] Invalid axis");
        goto done;
    }

    uint32_t index = (uint32_t) *p->index;
    if (*p->index < 0 || index >= arr->shape[axis]) {
        res = csound->InitError(csound, "[csnarray] Index negative or out of bounds");
        goto done;
    }

    // remove axis passed for new shape
    uint32_t out_ndim = ndim - 1;
    for (uint32_t i = 0, j = 0; i < ndim; i++) {
        if (i == axis) continue;
        shape[j++] = arr->shape[i];
    }

    /* _locked: the registry mutex is already held and is not recursive. */
    if (create_csnarray_locked(csound, reg, &p->h, out_ndim, shape, &p->array, &p->handle_id, p->handle, &source_handle, 1U, &err) != OK) {
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
        return csound->InitError(csound, "[csnarray] NULL registry");
    }

    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, (uint32_t) *p->source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Invalid source handle");
        goto done;
    }

    CSN_ARRAY *arr = slot->array;

    if (*p->index < 0 || (size_t) *p->index >= arr->size) {
        res = csound->InitError(csound, "[csnarray] Index negative or out of bounds");
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
        return csound->InitError(csound, "[csnarray] NULL registry");
    }

    uint32_t source_handle = (uint32_t) *p->source_handle;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Invalid source handle");
        goto done;
    }

    CSN_ARRAY *arr = slot->array;
    uint32_t ndim = arr->ndim;
    uint32_t shape[CSN_MAX_DIMS] = {0};

    uint32_t axis = (uint32_t) *p->axis;
    if (*p->axis < 0 || axis > ndim - 1) {
        res = csound->InitError(csound, "[csnarray] Invalid axis");
        goto done;
    }

    uint32_t start = (uint32_t) *p->start;
    uint32_t stop = (uint32_t) *p->stop;
    uint32_t step = (uint32_t) *p->step;
    if (*p->start < 0 || start >= arr->shape[axis] || *p->stop < 0 || stop > arr->shape[axis] || stop <= start || step == 0) {
        res = csound->InitError(csound, "[csnarray] Index negative or out of bounds");
        goto done;
    }

    uint32_t new_size = (stop - start + step - 1) / step;
    for (uint32_t i = 0; i < ndim; i++) {
        shape[i] = i == axis ? new_size : arr->shape[i];
    }

    /* _locked: the registry mutex is already held and is not recursive. */
    if (create_csnarray_locked(csound, reg, &p->h, ndim, shape, &p->array, &p->handle_id, p->handle, &source_handle, 1U, &err) != OK) {
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
        return csound->InitError(csound, "[csnarray] NULL registry");
    }

    uint32_t source_handle = (uint32_t) *p->source_handle;
    uint32_t data_handle = (uint32_t) *p->data_handle;

    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Invalid source handle");
        goto done;
    }

    CSN_SLOT *data_slot = get_slot(reg, data_handle);
    if (data_slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Invalid data handle");
        goto done;
    }

    CSN_ARRAY *source_arr = source_slot->array;
    CSN_ARRAY *data_arr = data_slot->array;

    uint32_t source_ndim = source_arr->ndim;
    uint32_t data_ndim = data_arr->ndim;

    uint32_t slice_shape[CSN_MAX_DIMS] = {0};

    uint32_t axis = (uint32_t) *p->axis;
    if (*p->axis < 0 || axis > source_ndim - 1) {
        res = csound->InitError(csound, "[csnarray] Invalid axis");
        goto done;
    }

    uint32_t start = (uint32_t) *p->start;
    uint32_t stop = (uint32_t) *p->stop;
    uint32_t step = (uint32_t) *p->step;
    if (*p->start < 0 || start >= source_arr->shape[axis] || *p->stop < 0 || stop > source_arr->shape[axis] || stop <= start || step == 0) {
        res = csound->InitError(csound, "[csnarray] Index negative or out of bounds");
        goto done;
    }

    uint32_t new_size = (stop - start + step - 1) / step;
    uint32_t slice_size = 1;
    for (uint32_t i = 0; i < source_ndim; i++) {
        uint32_t size = i == axis ? new_size : source_arr->shape[i];
        slice_shape[i] = size;
        slice_size *= size;
        if (data_arr->shape[i] != slice_shape[i]) {
            res = csound->InitError(csound, "[csnarray] Slice block shape mismatch");
            goto done;
        }
    }

    if (data_arr->size != slice_size) {
        res = csound->InitError(csound, "[csnarray] Slice size mismatch with data array");
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
        return csound->InitError(csound, "[csnarray] NULL registry");
    }

    uint32_t handle = (uint32_t) *p->source_handle;
    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Invalid source handle");
        goto done;
    }

    CSN_ARRAY *arr = slot->array;
    if (arr->ndim != 1) {
        res = csound->InitError(csound, "[csnarray] push operation is only supported for 1-dim array");
        goto done;
    }

    if (arr->size >= CSN_MAX_ELEMS) {
        res = csound->InitError(csound, "[csnarray] push would exceed the maximum element count");
        goto done;
    }

    size_t new_size = arr->size + 1;
    if (new_size > arr->capacity) {
        size_t new_capacity = arr->capacity > 0 ? arr->capacity * 2 : 1;
        double *new_data = csound->ReAlloc(csound, arr->data, sizeof(double) * new_capacity);
        if (new_data == NULL) {
            res = csound->InitError(csound, "[csnarray] realloc memory allocation failed");
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
        return csound->InitError(csound, "[csnarray] NULL registry");
    }

    uint32_t handle = (uint32_t) *p->source_handle;
    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Invalid source handle");
        goto done;
    }

    CSN_ARRAY *arr = slot->array;
    if (arr->ndim != 1) {
        res = csound->InitError(csound, "[csnarray] pop operation is only supported for 1-dim array");
        goto done;
    }

    if (arr->size == 0) {
        res = csound->InitError(csound, "[csnarray] pop on empty array");
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
        return csound->InitError(csound, "[csnarray] NULL registry");
    }

    uint32_t handle = (uint32_t) *p->source_handle;
    if (*p->index < 0) {
        return csound->InitError(csound, "[csnarray] Index must be positive");
    }

    size_t index = (size_t) *p->index;
    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Invalid source handle");
        goto done;
    }

    CSN_ARRAY *arr = slot->array;
    if (arr->ndim != 1) {
        res = csound->InitError(csound, "[csnarray] insert operation is only supported for 1-dim array");
        goto done;
    }

    if (arr->size >= CSN_MAX_ELEMS) {
        res = csound->InitError(csound, "[csnarray] insert would exceed the maximum element count");
        goto done;
    }

    /* Inclusive upper bound: index == size appends, which is also the only
       way to insert into an array that is currently empty. */
    if (index > arr->size) {
        res = csound->InitError(csound, "[csnarray] index out of bounds");
        goto done;
    }

    size_t new_size = arr->size + 1;
    if (new_size > arr->capacity) {
        size_t new_capacity = arr->capacity > 0 ? arr->capacity * 2 : 1;
        double *new_data = csound->ReAlloc(csound, arr->data, sizeof(double) * new_capacity);
        if (new_data == NULL) {
            res = csound->InitError(csound, "[csnarray] realloc memory allocation failed");
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
        return csound->InitError(csound, "[csnarray] NULL registry");
    }

    uint32_t handle = (uint32_t) *p->source_handle;
    if (*p->index < 0) {
        return csound->InitError(csound, "[csnarray] Index must be positive");
    }

    size_t index = (size_t) *p->index;
    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Invalid source handle");
        goto done;
    }

    CSN_ARRAY *arr = slot->array;
    if (arr->ndim != 1) {
        res = csound->InitError(csound, "[csnarray] remove operation is only supported for 1-dim array");
        goto done;
    }

    if (arr->size == 0) {
        res = csound->InitError(csound, "[csnarray] remove on empty array");
        goto done;
    }

    if (index >= arr->size) {
        res = csound->InitError(csound, "[csnarray] index out of bounds");
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
        return csound->InitError(csound, "[csnarray] NULL registry");
    }

    uint32_t source_handle = (uint32_t) *p->source_handle;
    uint32_t data_handle = (uint32_t) *p->data_handle;

    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Invalid source handle");
        goto done;
    }

    CSN_SLOT *data_slot = get_slot(reg, data_handle);
    if (data_slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Invalid data handle");
        goto done;
    }

    CSN_ARRAY *source_arr = source_slot->array;
    CSN_ARRAY *data_arr = data_slot->array;

    uint32_t source_ndim = source_arr->ndim;
    uint32_t data_ndim = data_arr->ndim;

    if (data_ndim != source_ndim - 1) {
        res = csound->InitError(csound, "[csnarray] Dims mismatch: block must have source dim minus 1");
        goto done;
    }

    uint32_t axis = (uint32_t) *p->axis;
    if (*p->axis < 0 || axis > source_ndim - 1) {
        res = csound->InitError(csound, "[csnarray] Invalid axis");
        goto done;
    }

    uint32_t index = (uint32_t) *p->index;
    if (*p->index < 0 || index > source_arr->shape[axis]) {
        res = csound->InitError(csound, "[csnarray] Invalid index");
        goto done;
    }

    for (uint32_t i = 0, j = 0; i < source_ndim; i++) {
        if (i == axis) continue;
        if (data_arr->shape[j++] != source_arr->shape[i]) {
            res = csound->InitError(csound, "[csnarray] Shape mismatch between block and source array");
            goto done;
        }
    }

    uint32_t temp_shape[CSN_MAX_DIMS] = {0};
    memcpy(temp_shape, source_arr->shape, sizeof(uint32_t) * (size_t) source_ndim);
    temp_shape[axis]++;

    CSN_ARRAY *temp = csound->Calloc(csound, sizeof(CSN_ARRAY));
    if (temp == NULL) {
        res = csound->InitError(csound, "[csnarray] Memory allocation failed");
        goto done;
    }

    int32_t alloc_temp = allocate_array(csound, temp, source_ndim, temp_shape, source_arr->array_id);
    if (alloc_temp != OK) {
        csound->Free(csound, temp);
        res = csound->InitError(csound, "[csnarray] Memory allocation failed");
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
        res = csound->InitError(csound, "[csnarray] Memory allocation failed");
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
        return csound->InitError(csound, "[csnarray] NULL registry");
    }

    uint32_t source_handle = (uint32_t) *p->source_handle;
    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Invalid source handle");
        goto done;
    }

    CSN_ARRAY *source_arr = source_slot->array;
    uint32_t source_ndim = source_arr->ndim;

    uint32_t axis = (uint32_t) *p->axis;
    if (*p->axis < 0 || axis > source_ndim - 1 || source_arr->shape[axis] <= 1) {
        res = csound->InitError(csound, "[csnarray] Invalid axis");
        goto done;
    }

    uint32_t index = (uint32_t) *p->index;
    if (*p->index < 0 || index >= source_arr->shape[axis]) {
        res = csound->InitError(csound, "[csnarray] Invalid index");
        goto done;
    }

    uint32_t temp_shape[CSN_MAX_DIMS] = {0};
    memcpy(temp_shape, source_arr->shape, sizeof(uint32_t) * (size_t) source_ndim);
    temp_shape[axis]--;

    if (create_csnarray_locked(csound, reg, &p->h, source_ndim, temp_shape, &p->array, &p->handle_id, p->handle, &source_handle, 1U, &err) != OK) {
        res = csound->InitError(csound, "[csnarray] %s\n", err);
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
        return csound->InitError(csound, "[csnarray] NULL registry");
    }

    uint32_t source_handle = (uint32_t) *p->source_handle;
    uint32_t data_handle = (uint32_t) *p->data_handle;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Invalid source handle");
        goto done;
    }

    CSN_SLOT *data_slot = get_slot(reg, data_handle);
    if (data_slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Invalid data handle");
        goto done;
    }

    CSN_ARRAY *source_arr = source_slot->array;
    CSN_ARRAY *data_arr = data_slot->array;

    uint32_t source_ndim = source_arr->ndim;
    uint32_t data_ndim = data_arr->ndim;

    if (source_ndim != 1U || data_ndim != 1U) {
        res = csound->InitError(csound, "[csnarray] Invalid dims");
        goto done;
    }

    uint32_t *source_shape = source_arr->shape;
    uint32_t *data_shape = data_arr->shape;

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    new_shape[0] = source_shape[0] + data_shape[0];

    /* Both operands are read by the copy loop below, so both are protected. */
    const uint32_t protect[2] = { source_handle, data_handle };

    if (create_csnarray_locked(csound, reg, &p->h, 1U, new_shape, &p->array, &p->handle_id, p->handle, protect, 2U, &err) != OK) {
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
        return csound->InitError(csound, "[csnarray] NULL registry");
    }

    uint32_t source_handle = (uint32_t) *p->source_handle;
    uint32_t data_handle = (uint32_t) *p->data_handle;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Invalid source handle");
        goto done;
    }

    CSN_SLOT *data_slot = get_slot(reg, data_handle);
    if (data_slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Invalid data handle");
        goto done;
    }

    CSN_ARRAY *source_arr = source_slot->array;
    CSN_ARRAY *data_arr = data_slot->array;

    uint32_t source_ndim = source_arr->ndim;
    uint32_t data_ndim = data_arr->ndim;

    if (data_ndim != source_ndim) {
        res = csound->InitError(csound, "[csnarray] Dims mismatch");
        goto done;
    }

    uint32_t *source_shape = source_arr->shape;
    uint32_t *data_shape = data_arr->shape;

    uint32_t axis = (uint32_t) *p->axis;
    if (*p->axis < 0 || axis > source_ndim - 1) {
        res = csound->InitError(csound, "[csnarray] Invalid axis");
        goto done;
    }

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    for (uint32_t i = 0; i < source_ndim; i++) {
        if (i == axis) {
            new_shape[i] = source_shape[i] + data_shape[i];
        } else {
            if (source_shape[i] != data_shape[i]) {
                res = csound->InitError(csound, "[csnarray] Shape mismatch");
                goto done;
            }
            new_shape[i] = source_shape[i];
        }
    }

    /* Both operands are read by the copy loop below, so both are protected. */
    const uint32_t protect[2] = { source_handle, data_handle };

    if (create_csnarray_locked(csound, reg, &p->h, source_ndim, new_shape, &p->array, &p->handle_id, p->handle, protect, 2U, &err) != OK) {
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
        return csound->InitError(csound, "[csnarray] NULL registry");
    }

    uint32_t source_handle = (uint32_t) *p->source_handle;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Invalid source handle");
        goto done;
    }

    CSN_ARRAY *source_arr = source_slot->array;
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    if (*p->before < 0 || *p->after < 0) {
        res = csound->InitError(csound, "[csnarray] Param before and after must be positive");
        goto done;
    }

    uint32_t before = (uint32_t) *p->before;
    uint32_t after = (uint32_t) *p->after;

    int32_t axis = -1;
    if (p->INOCOUNT > 4) {
        axis = (int32_t) *p->axis;
        if (*p->axis < 0 || (uint32_t) axis > source_ndim - 1) {
            res = csound->InitError(csound, "[csnarray] Invalid axis");
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

    if (create_csnarray_locked(csound, reg, &p->h, source_ndim, new_shape, &p->array, &p->handle_id, p->handle, protect, 1U, &err) != OK) {
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
        return csound->InitError(csound, "[csnarray] NULL registry");
    }

    uint32_t source_handle = (uint32_t) *p->source_handle;

    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Invalid source handle");
        goto done;
    }

    CSN_ARRAY *source_arr = source_slot->array;
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    if (*p->before < 0 || *p->after < 0) {
        res = csound->InitError(csound, "[csnarray] Param before and after must be positive");
        goto done;
    }

    uint32_t before = (uint32_t) *p->before;
    uint32_t after = (uint32_t) *p->after;

    int32_t axis = -1;
    if (p->INOCOUNT > 4) {
        axis = (int32_t) *p->axis;
        if (*p->axis < 0 || (uint32_t) axis > source_ndim - 1) {
            res = csound->InitError(csound, "[csnarray] Invalid axis");
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
        res = csound->InitError(csound, "[csnarray] Memory allocation failed");
        goto done;
    }

    int32_t alloc_temp = allocate_array(csound, temp, source_ndim, new_shape, source_arr->array_id);
    if (alloc_temp != OK) {
        csound->Free(csound, temp);
        res = csound->InitError(csound, "[csnarray] Memory allocation failed");
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
        res = csound->InitError(csound, "[csnarray] Memory allocation failed");
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
        return csound->InitError(csound, "[csnarray] NULL registry");
    }

    uint32_t source_handle = (uint32_t) *p->source_handle;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Invalid source handle");
        goto done;
    }

    CSN_ARRAY *source_arr = source_slot->array;

    const uint32_t protect[1] = { source_handle };

    if (create_csnarray_locked(csound, reg, &p->h, source_arr->ndim, source_arr->shape, &p->array, &p->handle_id, p->handle, protect, 1U, &err) != OK) {
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
        return csound->InitError(csound, "[csnarray] NULL registry");
    }

    uint32_t source_handle = (uint32_t) *p->source_handle;
    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Invalid source handle");
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
        return csound->InitError(csound, "[csnarray] NULL registry");
    }

    uint32_t source_handle = (uint32_t) *p->source_handle;
    uint32_t data_handle = (uint32_t) *p->data_handle;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Invalid source handle");
        goto done;
    }

    CSN_SLOT *data_slot = get_slot(reg, data_handle);
    if (data_slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Invalid data handle");
        goto done;
    }

    CSN_ARRAY *source_arr = source_slot->array;
    CSN_ARRAY *data_arr = data_slot->array;

    uint32_t source_ndim = source_arr->ndim;
    uint32_t data_ndim = data_arr->ndim;

    if (data_ndim != 1U) {
        res = csound->InitError(csound, "[csnarray] Data array must be 1-D");
        goto done;
    }

    uint32_t *source_shape = source_arr->shape;

    /* No match yields a {0, ndim} array rather than an invalid handle, so the
       result stays usable in a chain. Matches np.argwhere. */
    size_t count = count_elements_from_array(source_arr, data_arr, EQUAL);

    uint32_t new_shape[2] = { (uint32_t) count, source_arr->ndim };

    const uint32_t protect[2] = { source_handle, data_handle };
    if (create_csnarray_locked(csound, reg, &p->h, 2U, new_shape, &p->array, &p->handle_id, p->handle, protect, 2U, &err) != OK) {
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
        return csound->InitError(csound, "[csnarray] NULL registry");
    }

    uint32_t source_handle = (uint32_t) *p->source_handle;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Invalid source handle");
        goto done;
    }

    CSN_ARRAY *source_arr = source_slot->array;
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    size_t count = count_elements_from_value(source_arr, 0.0, mode);
    uint32_t new_shape[2] = { (uint32_t) count, source_arr->ndim };

    const uint32_t protect[1] = { source_handle };
    if (create_csnarray_locked(csound, reg, &p->h, 2U, new_shape, &p->array, &p->handle_id, p->handle, protect, 1U, &err) != OK) {
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
        return csound->InitError(csound, "[csnarray] NULL registry");
    }

    uint32_t source_handle = (uint32_t) *p->source_handle;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Invalid source handle");
        goto done;
    }

    CSN_ARRAY *source_arr = source_slot->array;
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    ARRAY_ELEMENT *temp = csound->Calloc(csound, sizeof(ARRAY_ELEMENT) * source_arr->size);
    if (temp == NULL) {
        res = csound->InitError(csound, "[csnarray] Memory allocation failed");
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
    if (create_csnarray_locked(csound, reg, &p->h, 2U, new_shape, &p->array, &p->handle_id, p->handle, protect, 1U, &err) != OK) {
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
        return csound->InitError(csound, "[csnarray] NULL registry");
    }

    uint32_t source_handle = (uint32_t) *p->source_handle;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Invalid source handle");
        goto done;
    }

    CSN_ARRAY *source_arr = source_slot->array;

    ARRAY_ELEMENT *temp = csound->Calloc(csound, sizeof(ARRAY_ELEMENT) * source_arr->size);
    if (temp == NULL) {
        res = csound->InitError(csound, "[csnarray] Memory allocation failed");
        goto done;
    }

    for (size_t i = 0; i < source_arr->size; i++) {
        temp[i].value = source_arr->data[i];
        temp[i].linear_index = (uint32_t) i;
    }

    size_t count = count_unique(temp, source_arr->size);
    uint32_t new_shape[1] = { (uint32_t) count };

    const uint32_t protect[1] = { source_handle };
    if (create_csnarray_locked(csound, reg, &p->h, 1U, new_shape, &p->array, &p->handle_id, p->handle, protect, 1U, &err) != OK) {
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
        return csound->InitError(csound, "[csnarray] NULL registry");
    }

    uint32_t source_handle = (uint32_t) *p->source_handle;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Invalid source handle");
        goto done;
    }

    CSN_ARRAY *source_arr = source_slot->array;
    double cmp_value = (double) *p->cmp_value;

    size_t count = count_elements_from_value(source_arr, cmp_value, mode);
    uint32_t new_shape[1] = { (uint32_t) count };

    const uint32_t protect[1] = { source_handle };
    if (create_csnarray_locked(csound, reg, &p->h, 1U, new_shape, &p->array, &p->handle_id, p->handle, protect, 1U, &err) != OK) {
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

int32_t csnarray_compare_count_helper(CSOUND *csound, CSN_COMPARE *p, CSN_COMPARE_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] NULL registry");
    }

    uint32_t source_handle = (uint32_t) *p->source_handle;
    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Invalid source handle");
        goto done;
    }

    CSN_ARRAY *source_arr = source_slot->array;

    /* NONZERO and IS_NAN take no comparison value, so their opcodes have no
       such argument to read. */
    size_t count = (mode == NONZERO || mode == IS_NAN)
        ? count_elements_from_value(source_arr, 0.0, mode)
        : count_elements_from_value(source_arr, (double) *p->cmp_value, mode);

    *p->handle = (MYFLT) count;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_count_equal(CSOUND *csound, CSN_COMPARE *p) {
    return csnarray_compare_count_helper(csound, p, EQUAL);
}

int32_t csnarray_count_nonzero(CSOUND *csound, CSN_COMPARE *p) {
    return csnarray_compare_count_helper(csound, p, NONZERO);
}

int32_t csnarray_count_nan(CSOUND *csound, CSN_COMPARE *p) {
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

static int32_t csnarray_accumulate_reduction(CSOUND *csound, CSN_REDUCTION *p, CSN_REDUCTION_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] NULL registry");
    }

    uint32_t source_handle = (uint32_t) *p->source_handle;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Invalid source handle");
        goto done;
    }

    CSN_ARRAY *source_arr = source_slot->array;
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    int32_t axis = (int32_t) *p->axis;
    if (axis != -1 && (uint32_t) axis >= source_ndim) {
        res = csound->InitError(csound, "[csnarray] Invalid axis");
        goto done;
    }

    if (source_ndim <= 1 && axis > 0) {
        res = csound->InitError(csound, "[csnarray] Invalid axis");
        goto done;
    }

    /* min, max and the subtraction fold have no identity element to fall back
       on, so an empty reduction has no answer. numpy raises here too. */
    size_t reduced_extent = (axis == -1) ? source_arr->size : source_shape[axis];
    if (reduced_extent == 0 && (mode == RED_MIN || mode == RED_MAX || mode == RED_SUB)) {
        res = csound->InitError(csound, "[csnarray] Reduction of an empty array is undefined for min, max and sub");
        goto done;
    }

    CSN_ARRAY *arr = NULL;
    if (axis != -1) {
        uint32_t new_shape[CSN_MAX_DIMS] = {0};
        for (uint32_t i = 0, j = 0; i < source_ndim; i++) {
            if (i != (uint32_t) axis) new_shape[j++] = source_shape[i];
        }

        const uint32_t protect[1] = { source_handle };

        if (create_csnarray_locked(csound, reg, &p->h, source_ndim - 1, new_shape, &p->array, &p->handle_id, p->handle, protect, 1U, &err) != OK) {
            res = csound->InitError(csound, "[csnarray] %s", err);
            goto done;
        }

        arr = p->array;
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
        *p->handle = (MYFLT) value;
    }

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_sum(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction(csound, p, RED_SUM);
}

int32_t csnarray_prod(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction(csound, p, RED_PROD);
}

int32_t csnarray_sub(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction(csound, p, RED_SUB);
}

int32_t csnarray_mean(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction(csound, p, RED_MEAN);
}

int32_t csnarray_min(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction(csound, p, RED_MIN);
}

int32_t csnarray_max(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction(csound, p, RED_MAX);
}

int32_t csnarray_all(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction(csound, p, RED_ALL);
}

int32_t csnarray_any(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction(csound, p, RED_ANY);
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

static int32_t csnarray_stdvar_helper(CSOUND *csound, CSN_REDUCTION *p, CSN_REDUCTION_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] NULL registry");
    }

    uint32_t source_handle = (uint32_t) *p->source_handle;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Invalid source handle");
        goto done;
    }

    CSN_ARRAY *source_arr = source_slot->array;
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    int32_t axis = (int32_t) *p->axis;
    if (axis != -1 && (uint32_t) axis >= source_ndim) {
        res = csound->InitError(csound, "[csnarray] Invalid axis");
        goto done;
    }

    if (source_ndim <= 1 && axis > 0) {
        res = csound->InitError(csound, "[csnarray] Invalid axis");
        goto done;
    }

    CSN_ARRAY *arr = NULL;
    if (axis != -1) {
        uint32_t new_shape[CSN_MAX_DIMS] = {0};
        for (uint32_t i = 0, j = 0; i < source_ndim; i++) {
            if (i != (uint32_t) axis) new_shape[j++] = source_shape[i];
        }

        const uint32_t protect[1] = { source_handle };

        if (create_csnarray_locked(csound, reg, &p->h, source_ndim - 1, new_shape, &p->array, &p->handle_id, p->handle, protect, 1U, &err) != OK) {
            res = csound->InitError(csound, "[csnarray] %s", err);
            goto done;
        }

        arr = p->array;
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
        *p->handle = (MYFLT) value;
    }

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_std(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_stdvar_helper(csound, p, RED_STD);
}

int32_t csnarray_var(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_stdvar_helper(csound, p, RED_VAR);
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
        return csound->InitError(csound, "[csnarray] NULL registry");
    }

    uint32_t source_handle = (uint32_t) *p->source_handle;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Invalid source handle");
        goto done;
    }

    CSN_ARRAY *source_arr = source_slot->array;
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    int32_t axis = (int32_t) *p->axis;
    if (axis != -1 && (uint32_t) axis >= source_ndim) {
        res = csound->InitError(csound, "[csnarray] Invalid axis");
        goto done;
    }

    if (source_ndim <= 1 && axis > 0) {
        res = csound->InitError(csound, "[csnarray] Invalid axis");
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

    if (create_csnarray_locked(csound, reg, &p->h, 2U, new_shape, &p->array, &p->handle_id, p->handle, protect, 1U, &err) != OK) {
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

int32_t csnarray_median(CSOUND *csound, CSN_REDUCTION *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    if (reg == NULL) {
        return csound->InitError(csound, "[csnarray] NULL registry");
    }

    uint32_t source_handle = (uint32_t) *p->source_handle;

    int32_t res = OK;
    const char *err = NULL;
    double *scratch = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Invalid source handle");
        goto done;
    }

    CSN_ARRAY *source_arr = source_slot->array;
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    int32_t axis = (int32_t) *p->axis;
    if (axis != -1 && (uint32_t) axis >= source_ndim) {
        res = csound->InitError(csound, "[csnarray] Invalid axis");
        goto done;
    }

    /* Median needs a sorted copy, so it cannot stream like the folds do. */
    size_t run = (axis == -1) ? source_arr->size : source_shape[axis];
    scratch = csound->Calloc(csound, sizeof(double) * (run > 0 ? run : 1));
    if (scratch == NULL) {
        res = csound->InitError(csound, "[csnarray] Memory allocation failed");
        goto done;
    }

    if (axis == -1) {
        memcpy(scratch, source_arr->data, sizeof(double) * run);
        *p->handle = (MYFLT) median_of_scratch(scratch, run);
        goto done;
    }

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    for (uint32_t i = 0, j = 0; i < source_ndim; i++) {
        if (i != (uint32_t) axis) new_shape[j++] = source_shape[i];
    }

    const uint32_t protect[1] = { source_handle };
    if (create_csnarray_locked(csound, reg, &p->h, source_ndim - 1, new_shape, &p->array, &p->handle_id, p->handle, protect, 1U, &err) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;

    for (size_t linear = 0; linear < arr->size; ++linear) {
        uint32_t dst_coords[CSN_MAX_DIMS] = {0};
        uint32_t src_coords[CSN_MAX_DIMS] = {0};

        from_linear_to_coords(dst_coords, arr->shape, linear, arr->ndim);

        for (uint32_t k = 0; k < run; ++k) {
            for (uint32_t i = 0, j = 0; i < source_ndim; ++i) {
                src_coords[i] = (i == (uint32_t) axis) ? k : dst_coords[j++];
            }
            scratch[k] = source_arr->data[from_coords_to_offset(src_coords, source_arr->strides, source_ndim)];
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

int32_t csnarray_argmin(CSOUND *csound, CSN_REDUCTION *p) {
    return argminmax_helper(csound, p, RED_ARGMIN);
}

int32_t csnarray_argmax(CSOUND *csound, CSN_REDUCTION *p) {
    return argminmax_helper(csound, p, RED_ARGMAX);
}


// --- OENTRY ---

#define S(x) sizeof(x)

static OENTRY localops[] = {
    { "csnempty",           S(CSN_ARR_INIT),     0, "i",   "i[]",    (SUBR) create_empty_csnarray,   NULL, (SUBR) create_csnarray_deinit,        NULL, 0 },
    { "csnzeros",           S(CSN_ARR_INIT),     0, "i",   "i[]",    (SUBR) create_zeros_csnarray,   NULL, (SUBR) create_csnarray_deinit,        NULL, 0 },
    { "csnones",            S(CSN_ARR_INIT),     0, "i",   "i[]",    (SUBR) create_ones_csnarray,    NULL, (SUBR) create_csnarray_deinit,        NULL, 0 },
    { "csnfull",            S(CSN_ARR_INIT),     0, "i",   "i[]i",   (SUBR) create_full_csnarray,    NULL, (SUBR) create_csnarray_deinit,        NULL, 0 },
    { "csnfromarray",       S(CSN_FROM_ARRAY),   0, "i",   "i[]",    (SUBR) from_array_to_csnarray,  NULL, (SUBR) from_array_to_csnarray_deinit, NULL, 0 },
    { "csntoarray",         S(CSN_TO_ARRAY),     0, "i[]", "i",      (SUBR) from_csnarray_to_array,  NULL, NULL,                                 NULL, 0 },
    { "csnfree",            S(CSN_FREE),         0, "",    "i",      (SUBR) free_csnarray,           NULL, NULL,                                 NULL, 0 },
    { "csndims",            S(CSN_SIZE_DIMS),    0, "i",   "i",      (SUBR) csnarray_dims,           NULL, NULL,                                 NULL, 0 },
    { "csnsize",            S(CSN_SIZE_DIMS),    0, "i",   "i",      (SUBR) csnarray_size,           NULL, NULL,                                 NULL, 0 },
    { "csnisempty",         S(CSN_SIZE_DIMS),    0, "i",   "i",      (SUBR) csnarray_is_empty,       NULL, NULL,                                 NULL, 0 },
    { "csnshape",           S(CSN_SHAPE),        0, "i[]", "i",      (SUBR) csnarray_shape,          NULL, NULL,                                 NULL, 0 },
    { "csnarange",          S(CSN_SPACED_SPACE), 0, "i",   "iii",    (SUBR) csnarray_arange,         NULL, (SUBR) csnarray_space_spaced_deinit,  NULL, 0 },
    { "csnlinspace",        S(CSN_SPACED_SPACE), 0, "i",   "iii",    (SUBR) csnarray_linspace,       NULL, (SUBR) csnarray_space_spaced_deinit,  NULL, 0 },
    { "csnlogspace",        S(CSN_SPACED_SPACE), 0, "i",   "iiii",   (SUBR) csnarray_logspace,       NULL, (SUBR) csnarray_space_spaced_deinit,  NULL, 0 },
    { "csngeomspace",       S(CSN_SPACED_SPACE), 0, "i",   "iii",    (SUBR) csnarray_geomspace,      NULL, (SUBR) csnarray_space_spaced_deinit,  NULL, 0 },
    { "csnidentity",        S(CSN_IDENTITY),     0, "i",   "i",      (SUBR) csnarray_identity,       NULL, (SUBR) csnarray_identity_deinit,      NULL, 0 },
    // reshape -> new csnarray
    { "csnreshape",         S(CSN_RESHAPE),      0, "i",   "ii[]",   (SUBR) csnarray_reshape,        NULL, (SUBR) csnarray_shape_deinit,         NULL, 0 },
    // reshape.in -> modify in loco
    { "csnreshape.in",      S(CSN_RESHAPE_IN),   0, "",    "ii[]",   (SUBR) csnarray_reshape_in,     NULL, NULL,                                 NULL, 0 },
    { "csnflatten",         S(CSN_RESHAPE),      0, "i",   "i",      (SUBR) csnarray_flatten,        NULL, (SUBR) csnarray_shape_deinit,         NULL, 0 },
    { "csnflatten.in",      S(CSN_RESHAPE_IN),   0, "",    "i",      (SUBR) csnarray_flatten_in,     NULL, NULL,                                 NULL, 0 },
    { "csntranspose",       S(CSN_RESHAPE),      0, "i",   "i",      (SUBR) csnarray_transpose,      NULL, (SUBR) csnarray_shape_deinit,         NULL, 0 },
    { "csntranspose.ax",    S(CSN_RESHAPE),      0, "i",   "ii[]",   (SUBR) csnarray_transpose,      NULL, (SUBR) csnarray_shape_deinit,         NULL, 0 },
    { "csntranspose.in",    S(CSN_RESHAPE_IN),   0, "",    "i",      (SUBR) csnarray_transpose_in,   NULL, NULL,                                 NULL, 0 },
    { "csntranspose.ax.in", S(CSN_RESHAPE_IN),   0, "",    "ii[]",   (SUBR) csnarray_transpose_in,   NULL, NULL,                                 NULL, 0 },
    { "csnflip",            S(CSN_FLIP_ROLL),    0, "i",   "ij",     (SUBR) csnarray_flip,           NULL, (SUBR) csnarray_flip_deinit,          NULL, 0 },
    { "csnflip.in",         S(CSN_FLIP_ROLL_IN), 0, "",    "ij",     (SUBR) csnarray_flip_in,        NULL, NULL,                                 NULL, 0 },
    // roll -> on flattened array
    { "csnroll",            S(CSN_FLIP_ROLL),    0, "i",   "ii",     (SUBR) csnarray_roll,           NULL, (SUBR) csnarray_flip_deinit,          NULL, 0 },
    { "csnroll.in",         S(CSN_FLIP_ROLL_IN), 0, "",    "ii",     (SUBR) csnarray_roll_in,        NULL, NULL,                                 NULL, 0 },
    // roll -> on n-dims array
    { "csnroll.ax",         S(CSN_FLIP_ROLL),    0, "i",   "iij",    (SUBR) csnarray_rollaxis,       NULL, (SUBR) csnarray_flip_deinit,          NULL, 0 },
    { "csnroll.ax.in",      S(CSN_FLIP_ROLL_IN), 0, "",    "iij",    (SUBR) csnarray_rollaxis_in,    NULL, NULL,                                 NULL, 0 },
    { "csnget",             S(CSN_GET),          0, "i",   "ii[]",   (SUBR) csnarray_get,            NULL, NULL,                                 NULL, 0 },
    { "csnset",             S(CSN_SET),          0, "",    "ii[]i",  (SUBR) csnarray_set,            NULL, NULL,                                 NULL, 0 },
    { "csntake",            S(CSN_TAKE),         0, "i",   "iii",    (SUBR) csnarray_take,           NULL, (SUBR) csnarray_take_deinit,          NULL, 0 },
    { "csntake.flat",       S(CSN_TAKE_FLAT),    0, "i",   "ii",     (SUBR) csnarray_take_flat,      NULL, NULL,                                 NULL, 0 },
    { "csngetslice",        S(CSN_GET_SLICE),    0, "i",   "iiiii",  (SUBR) csnarray_get_slice,      NULL, (SUBR) csnarray_slice_deinit,         NULL, 0 },
    { "csnsetslice",        S(CSN_SET_SLICE),    0, "",    "iiiiii", (SUBR) csnarray_set_slice,      NULL, NULL,                                 NULL, 0 },
    { "csnpush",            S(CSN_PUSH),         0, "",    "ii",     (SUBR) csnarray_push,           NULL, NULL,                                 NULL, 0 },
    { "csnpop",             S(CSN_POP),          0, "i",   "i",      (SUBR) csnarray_pop,            NULL, NULL,                                 NULL, 0 },
    { "csninsert.flat",     S(CSN_PUSH),         0, "",    "iii",    (SUBR) csnarray_insert,         NULL, NULL,                                 NULL, 0 },
    { "csnremove.flat",     S(CSN_POP),          0, "i",   "ii",     (SUBR) csnarray_remove,         NULL, NULL,                                 NULL, 0 },
    { "csninsert.block",    S(CSN_INSERT_BLOCK), 0, "",    "iiii",   (SUBR) csnarray_insert_block,   NULL, NULL,                                 NULL, 0 },
    { "csnremove.block",    S(CSN_TAKE),         0, "i",   "iii",    (SUBR) csnarray_remove_block,   NULL, (SUBR) csnarray_take_deinit,          NULL, 0 },
    { "csnconcat.flat",     S(CSN_CONCAT),       0, "i",   "ii",     (SUBR) csnarray_concat_flat,    NULL, (SUBR) csnarray_concat_deinit,        NULL, 0 },
    { "csnconcat.block",    S(CSN_CONCAT),       0, "i",   "iii",    (SUBR) csnarray_concat_block,   NULL, (SUBR) csnarray_concat_deinit,        NULL, 0 },
    { "csnpad",             S(CSN_PAD),          0, "i",   "iiio",   (SUBR) csnarray_pad,            NULL, (SUBR) csnarray_pad_deinit,           NULL, 0 },
    { "csnpad.ax",          S(CSN_PAD),          0, "i",   "iiiii",  (SUBR) csnarray_pad,            NULL, (SUBR) csnarray_pad_deinit,           NULL, 0 },
    { "csnpad.in",          S(CSN_PAD_IN),       0, "",    "iiio",   (SUBR) csnarray_pad_in,         NULL, NULL,                                 NULL, 0 },
    { "csnpad.ax.in",       S(CSN_PAD_IN),       0, "",    "iiiii",  (SUBR) csnarray_pad_in,         NULL, NULL,                                 NULL, 0 },
    { "csnclip",            S(CSN_CLIP),         0, "i",   "iii",    (SUBR) csnarray_clip,           NULL, (SUBR) csnarray_clip_deinit,          NULL, 0 },
    { "csnclip.in",         S(CSN_CLIP_IN),      0, "",    "iii",    (SUBR) csnarray_clip_in,        NULL, NULL,                                 NULL, 0 },
    { "csnargwhere",        S(CSN_ARGWHERE),     0, "i",   "ii",     (SUBR) csnarray_argwhere,       NULL, (SUBR) csnarray_argwhere_deinit,      NULL, 0 },
    { "csnargnonzero",      S(CSN_ARGWHERE),     0, "i",   "i",      (SUBR) csnarray_argnonzero,     NULL, (SUBR) csnarray_argwhere_deinit,      NULL, 0 },
    { "csnargisnan",        S(CSN_ARGWHERE),     0, "i",   "i",      (SUBR) csnarray_argisnan,       NULL, (SUBR) csnarray_argwhere_deinit,      NULL, 0 },
    { "csnargunique",       S(CSN_ARGWHERE),     0, "i",   "i",      (SUBR) csnarray_argunique,      NULL, (SUBR) csnarray_argwhere_deinit,      NULL, 0 },
    { "csnunique",          S(CSN_COMPARE),      0, "i",   "i",      (SUBR) csnarray_unique,         NULL, (SUBR) csnarray_compare_deinit,       NULL, 0 },
    { "csngt",              S(CSN_COMPARE),      0, "i",   "ii",     (SUBR) csnarray_greater_than,   NULL, (SUBR) csnarray_compare_deinit,       NULL, 0 },
    { "csnlt",              S(CSN_COMPARE),      0, "i",   "ii",     (SUBR) csnarray_less_than,      NULL, (SUBR) csnarray_compare_deinit,       NULL, 0 },
    { "csnne",              S(CSN_COMPARE),      0, "i",   "ii",     (SUBR) csnarray_not_equal,      NULL, (SUBR) csnarray_compare_deinit,       NULL, 0 },
    { "csncnteq",           S(CSN_COMPARE),      0, "i",   "ii",     (SUBR) csnarray_count_equal,    NULL, NULL,                                 NULL, 0 },
    { "csncntnz",           S(CSN_COMPARE),      0, "i",   "i",      (SUBR) csnarray_count_nonzero,  NULL, NULL,                                 NULL, 0 },
    { "csncntnan",          S(CSN_COMPARE),      0, "i",   "i",      (SUBR) csnarray_count_nan,      NULL, NULL,                                 NULL, 0 },
    { "csnsum",             S(CSN_REDUCTION),    0, "i",   "ij",     (SUBR) csnarray_sum,            NULL, (SUBR) csnarray_reduction_deinit,     NULL, 0 },
    { "csnprod",            S(CSN_REDUCTION),    0, "i",   "ij",     (SUBR) csnarray_prod,           NULL, (SUBR) csnarray_reduction_deinit,     NULL, 0 },
    { "csnsub",             S(CSN_REDUCTION),    0, "i",   "ij",     (SUBR) csnarray_sub,            NULL, (SUBR) csnarray_reduction_deinit,     NULL, 0 },
    { "csnmean",            S(CSN_REDUCTION),    0, "i",   "ij",     (SUBR) csnarray_mean,           NULL, (SUBR) csnarray_reduction_deinit,     NULL, 0 },
    { "csnmin",             S(CSN_REDUCTION),    0, "i",   "ij",     (SUBR) csnarray_min,            NULL, (SUBR) csnarray_reduction_deinit,     NULL, 0 },
    { "csnmax",             S(CSN_REDUCTION),    0, "i",   "ij",     (SUBR) csnarray_max,            NULL, (SUBR) csnarray_reduction_deinit,     NULL, 0 },
    { "csnall",             S(CSN_REDUCTION),    0, "i",   "ij",     (SUBR) csnarray_all,            NULL, (SUBR) csnarray_reduction_deinit,     NULL, 0 },
    { "csnany",             S(CSN_REDUCTION),    0, "i",   "ij",     (SUBR) csnarray_any,            NULL, (SUBR) csnarray_reduction_deinit,     NULL, 0 },
    { "csnmedian",          S(CSN_REDUCTION),    0, "i",   "ij",     (SUBR) csnarray_median,         NULL, (SUBR) csnarray_reduction_deinit,     NULL, 0 },
    { "csnstd",             S(CSN_REDUCTION),    0, "i",   "ij",     (SUBR) csnarray_std,            NULL, (SUBR) csnarray_reduction_deinit,     NULL, 0 },
    { "csnvar",             S(CSN_REDUCTION),    0, "i",   "ij",     (SUBR) csnarray_var,            NULL, (SUBR) csnarray_reduction_deinit,     NULL, 0 },
    { "csnargmin",          S(CSN_REDUCTION),    0, "i",   "ij",     (SUBR) csnarray_argmin,         NULL, (SUBR) csnarray_reduction_deinit,     NULL, 0 },
    { "csnargmax",          S(CSN_REDUCTION),    0, "i",   "ij",     (SUBR) csnarray_argmax,         NULL, (SUBR) csnarray_reduction_deinit,     NULL, 0 },
};


LINKAGE;
