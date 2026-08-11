#include "csnum.h"
#include "csnregistry.h"
#include <csdl.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
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

   protect_handle is the handle the caller is still reading from, or 0. A global
   output has its previous array released here so a re-triggered creator does
   not strand it, but in `gih csntranspose gih` that previous value *is* the
   source: releasing it would free the array the caller is about to copy.

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
    uint32_t protect_handle,
    const char **err
) {
    if (handle_out_is_global(h)) {
        uint32_t previous_handle = (uint32_t) *p_handle;
        if (previous_handle != protect_handle) {
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
    int32_t res = create_csnarray_locked(csound, reg, h, ndim, shape, p_array, p_handle_id, p_handle, 0, &err);
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

    if (create_csnarray_locked(csound, reg, &p->h, ndim, shape, &p->array, &p->handle_id, p->handle, source_handle, &err) != OK) {
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
    if (create_csnarray_locked(csound, reg, &p->h, 1U, shape, &p->array, &p->handle_id, p->handle, source_handle, &err) != OK) {
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
    if (create_csnarray_locked(csound, reg, &p->h, ndim, shape, &p->array, &p->handle_id, p->handle, source_handle, &err) != OK) {
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
    if (create_csnarray_locked(csound, reg, &p->h, ndim, arr->shape, &p->array, &p->handle_id, p->handle, source_handle, &err) != OK) {
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
    if (create_csnarray_locked(csound, reg, &p->h, ndim, arr->shape, &p->array, &p->handle_id, p->handle, source_handle, &err) != OK) {
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
    if (create_csnarray_locked(csound, reg, &p->h, ndim, arr->shape, &p->array, &p->handle_id, p->handle, source_handle, &err) != OK) {
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
    uint32_t j = 0;
    for (uint32_t i = 0; i < ndim; i++) {
        if (i == axis) continue;
        shape[j++] = arr->shape[i];
    }

    /* _locked: the registry mutex is already held and is not recursive. */
    if (create_csnarray_locked(csound, reg, &p->h, out_ndim, shape, &p->array, &p->handle_id, p->handle, source_handle, &err) != OK) {
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
    if (create_csnarray_locked(csound, reg, &p->h, ndim, shape, &p->array, &p->handle_id, p->handle, source_handle, &err) != OK) {
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
        res = csound->InitError(csound, "[csnarray] Invalid source handle");
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
    { "csninsert",          S(CSN_PUSH),         0, "",    "iii",    (SUBR) csnarray_insert,         NULL, NULL,                                 NULL, 0 },
    { "csnremove",          S(CSN_POP),          0, "i",   "ii",     (SUBR) csnarray_remove,         NULL, NULL,                                 NULL, 0 },
};


LINKAGE;
