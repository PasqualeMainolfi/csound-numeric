#include "csnum.h"
#include "csnum_internal.h"
#include "csnlinalg.h"
#include "csnfft.h"
#include "csnset.h"
#include "csnregistry.h"
#include <float.h>
#include <csdl.h>
#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>


CSN_AXIS_SPEC csn_normalize_axis_value(double value, uint32_t ndim) {
    CSN_AXIS_SPEC spec = { .kind = CSN_AXIS_INVALID, .index = 0U };

    if (ndim == 0U || !isfinite(value) || trunc(value) != value || value < -(double) ndim || value >= (double) ndim) {
        return spec;
    }

    int32_t axis = (int32_t) value;
    spec.kind = CSN_AXIS_INDEX;
    spec.index = axis < 0 ? (uint32_t) (axis + (int32_t) ndim) : (uint32_t) axis;
    return spec;
}

CSN_AXIS_SPEC csn_normalize_axis(const MYFLT *axis_in, uint32_t ndim, CSN_AXIS_DEFAULT omitted_default) {
    CSN_AXIS_SPEC spec = { .kind = CSN_AXIS_INVALID, .index = 0U };

    if (axis_in == NULL) {
        switch (omitted_default) {
            case CSN_AXIS_DEFAULT_FLATTEN:
                spec.kind = CSN_AXIS_FLATTEN;
                break;
            case CSN_AXIS_DEFAULT_ALL:
                spec.kind = CSN_AXIS_ALL;
                break;
            case CSN_AXIS_DEFAULT_LAST:
                if (ndim > 0U) {
                    spec.kind = CSN_AXIS_INDEX;
                    spec.index = ndim - 1U;
                }
                break;
            case CSN_AXIS_DEFAULT_REQUIRED:
                break;
        }
        return spec;
    }

    return csn_normalize_axis_value((double) *axis_in, ndim);
}


/* shuffle algo */
void fisher_yates(PCG32_STATE *rng, double *data, size_t size) {
    for (size_t i = size - 1; i > 0; --i) {
        uint32_t j = pcg32_bounded_u32(rng, (uint32_t) (i + 1));
        double temp = data[i];
        data[i] = data[j];
        data[j] = temp;
    }
}

const char *get_out_name(OPDS *h) {
    if (h == NULL || h->optext == NULL) return "?";
    const ARGLST *out = h->optext->t.outlist;
    if (out == NULL || out->count < 1 || out->arg[0] == NULL) return "?";
    return out->arg[0];
}

void deinit_scratch(CSOUND *csound, CSN_SCRATCH *scratch) {
    if (scratch->scratch != NULL) csound->Free(csound, scratch->scratch);
    scratch->scratch = NULL;
    scratch->scratch_capacity = 0;
}

bool is_inarg_i_time(OPDS *h, uint32_t arg_index) {
    return h->optext != NULL && h->optext->t.oentry != NULL && h->optext->t.oentry->intypes[arg_index] == 'i';
}

/* Reports a perf-time error raised from a helper that runs with the registry
   mutex held.

   csoundPerfError does more than print: it ends with xturnoff_now, which runs
   the note's deinit chain right there, and csnarray_deinit_by_handle takes
   this same mutex — which is not recursive. Reporting with the lock held wedges
   Csound in that deinit. The report is therefore bracketed by an unlock and a
   relock, so the caller's own `done:` block still balances the lock it took and
   no existing error path has to change shape. The message is formatted before
   the unlock because the varargs may point at registry memory the deinit frees.

   Opcode bodies that hold the lock themselves do not need this: they release
   the lock and return the report directly. */
int32_t csn_locked_perf_error(CSOUND *csound, OPDS *h, const char *fmt, ...) {
    CSN_REGISTRY *reg = (CSN_REGISTRY *) csound->QueryGlobalVariable(csound, CSN_REGISTRY_NAME);
    char message[512];

    va_list args;
    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);

    bool locked = reg != NULL && reg->mutex != NULL;
    if (locked) csound->UnlockMutex(reg->mutex);
    int32_t res = csound->PerfError(csound, h, "%s", message);
    if (locked) csound->LockMutex(reg->mutex);

    return res;
}


/* Has anything written this array since the opcode last published into it?
   The handle travels with the version because a released slot comes back with
   a new array whose counters restart, and a bare version would compare equal
   against it. */
bool SOURCE_HAS_MOVED(const K_DATA *k_data, uint32_t source_handle, const CSN_ARRAY *arr) {
    return k_data->prev_source_handle != source_handle
        || !is_same_array_data_version(&arr->version, &k_data->prev_source_version);
}

/* Closes an in-place write: the array carries a new generation, and this
   opcode records that generation as its own so the next pass recognizes its
   own handiwork and leaves it alone. Both halves belong together — a bump
   without the record makes the opcode redo the work forever, a record without
   the bump hides the write from every other consumer. */
void PUBLISH_INPLACE_WRITE(K_DATA *k_data, uint32_t source_handle, CSN_ARRAY *arr, bool shape_changed, bool ndim_changed, bool itype_changed) {
    update_array_data_version(&arr->version);
    update_array_layout_version(&arr->version, shape_changed, ndim_changed, itype_changed);
    k_data->prev_source_handle = source_handle;
    set_array_version(&k_data->prev_source_version, &arr->version);
}

/* A k-rate opcode that derives a result from a source may republish last
   pass's result instead of recomputing it, but only when both ends have held
   still: the source has not been written, and nothing has disturbed the output
   slot since this opcode filled it. Pass NULL for out_arr on the scalar forms,
   whose result lives in a MYFLT that nothing else can reach. */
bool CAN_REUSE_LAST_RESULT(const K_DATA *k_data, uint32_t source_handle, const CSN_ARRAY *source_arr, const CSN_ARRAY *out_arr) {
    if (k_data->prev_source_handle != source_handle) return false;
    if (!is_same_array_data_version(&source_arr->version, &k_data->prev_source_version)) return false;
    if (out_arr == NULL) return true;
    return is_same_array_version(&out_arr->version, &k_data->prev_output_version);
}

/* Records both ends after a real computation, so the next pass can recognize
   an untouched source and an untouched result. */
void PUBLISH_DERIVED_RESULT(K_DATA *k_data, uint32_t source_handle, const CSN_ARRAY *source_arr, const CSN_ARRAY *out_arr) {
    k_data->prev_source_handle = source_handle;
    set_array_version(&k_data->prev_source_version, &source_arr->version);
    if (out_arr != NULL) set_array_version(&k_data->prev_output_version, &out_arr->version);
}

/* The elementwise families (unary, compare, the binops) reach here with one or
   two array operands plus at most two scalar parameters, and they compare the
   whole ARRAY_VERSION rather than only its data counter: they broadcast and
   they follow the source layout, so a source that changed shape alone changes
   the result too. Pass 0/NULL for the second operand on the one-operand forms.

   The self-alias case is deliberately never reused. These opcodes allow
   X = csnmul(X, 0.99) at k-rate, where each pass is meant to fold the result
   back into its own input; recognizing "the source has not moved" there would
   freeze the accumulator after the first pass. */
bool CAN_REUSE_ELEMENTWISE(const K_DATA *k_data, uint32_t handle_a, const CSN_ARRAY *arr_a, uint32_t handle_b, const CSN_ARRAY *arr_b, const CSN_ARRAY *out_arr, double scalar_a, double scalar_b) {
    uint32_t owned = k_data->owned_handle;
    if (owned != 0 && (handle_a == owned || handle_b == owned)) return false;
    if (k_data->prev_source_handle != handle_a || handle_a == 0) return false;
    if (k_data->prev_source_handle_b != handle_b) return false;
    if (!is_same_array_version(&arr_a->version, &k_data->prev_source_version)) return false;
    if (arr_b != NULL && !is_same_array_version(&arr_b->version, &k_data->prev_source_version_b)) return false;
    if (k_data->prev_scalar_param != scalar_a || k_data->prev_scalar_param_b != scalar_b) return false;
    if (out_arr == NULL) return true;
    return is_same_array_version(&out_arr->version, &k_data->prev_output_version);
}

/* Records what the result was derived from, so the next pass can recognize it. */
void PUBLISH_ELEMENTWISE(K_DATA *k_data, uint32_t handle_a, const CSN_ARRAY *arr_a, uint32_t handle_b, const CSN_ARRAY *arr_b, const CSN_ARRAY *out_arr, double scalar_a, double scalar_b) {
    k_data->prev_source_handle = handle_a;
    k_data->prev_source_handle_b = handle_b;
    k_data->prev_scalar_param = scalar_a;
    k_data->prev_scalar_param_b = scalar_b;
    set_array_version(&k_data->prev_source_version, &arr_a->version);
    if (arr_b != NULL) set_array_version(&k_data->prev_source_version_b, &arr_b->version);
    if (out_arr != NULL) set_array_version(&k_data->prev_output_version, &out_arr->version);
}

int32_t CHECK_IF_REALLOC_IN(CSOUND *csound, OPDS *h, K_DATA *k_data, CSN_ARRAY *arr, uint32_t source_handle, CSN_SCRATCH *scratch_ref, uint32_t ndim, ITEM_TYPE itype, bool is_value_changed, bool rt_locked) {
    /* The scratch lives in the caller's opcode struct. */
    size_t required = arr->size * (size_t) itype;
    bool layout_changed = IS_REQUEST_CHANGED(k_data, ndim, itype, arr->shape) || k_data->prev_size != arr->size;
    bool scratch_too_small = scratch_ref->scratch_capacity < required;

    /* These opcodes are not idempotent — flipping an already flipped array
       undoes it — so a pass must run exactly once per write by someone else.
       That used to be decided by comparing the whole payload against the copy
       in scratch on every k-pass; the version answers the same question in
       O(1). */
    bool data_changed = SOURCE_HAS_MOVED(k_data, source_handle, arr);

#ifdef CSN_VERSION_CROSSCHECK
    /* Only one direction is a defect. A version that reports "unchanged" while
       the payload differs leaves the opcode skipping work it owes; the reverse
       (a writer that stored identical bytes still bumped the counter) costs a
       recomputation and nothing else. */
    if (!data_changed && !layout_changed && !scratch_too_small && required > 0
        && memcmp(arr->data, scratch_ref->scratch, sizeof(double) * required) != 0) {
        csound->Message(csound, "[csnarray] VERSION CROSSCHECK: handle %u reports data version %llu unchanged while its payload differs from the last published copy\n",
                        source_handle, (unsigned long long) arr->version.data_version);
    }
#endif

    if (!layout_changed && !scratch_too_small && !data_changed && !is_value_changed) return NOTOK; // goto done

    /* The k init reserves the source's whole capacity, so on a marked source
       this can only trip once the source itself has been refused a larger
       buffer. */
    if (scratch_too_small) {
        return csn_scratch_reserve(csound, h, rt_locked, scratch_ref, required, sizeof(double));
    }
    return OK;
}

bool IS_VALID_SHIFT(double shift) {
    return isfinite(shift) && trunc(shift) == shift && shift >= (double) INT32_MIN && shift <= (double) INT32_MAX;
}

static bool IS_VALID_SEED(double seed) {
    return isfinite(seed) && trunc(seed) == seed && seed >= 0.0 && seed <= 9007199254740992.0; /* 2^53, exact in double */
}

bool IS_VALID_INDEX(double index) {
    return isfinite(index) && trunc(index) == index && index >= 0.0 && index <= (double) UINT32_MAX;
}

bool IS_VALID_LENGTH(double length) {
    return isfinite(length) && trunc(length) == length && length >= 0.0 && length <= (double) CSN_MAX_ELEMS;
}

bool IS_VALID_ZERO_ONE(double value) {
    return isfinite(value) && trunc(value) == value && (value == 0.0 || value == 1.0);
}

bool IS_VALID_VALUE(double value) {
    return isfinite(value) && !isnan(value);
}

bool IS_VALID_VALUE_INT32(double value) {
    return isfinite(value) && !isnan(value) && trunc(value) == value && value >= (double) INT32_MIN && value <= (double) INT32_MAX;
}

void fill_csnarray(CSN_ARRAY *array, double value) {
    if (array->itype == CSN_COMPLEX) {
        for (size_t i = 0; i < array->size; i++) {
            array->data[i * 2] = value;
            array->data[i * 2 + 1] = 0.0;
        }
        return;
    }

    for (size_t i = 0; i < array->size; i++) array->data[i] = value;
}

void fill_csnarray_complex(CSN_ARRAY *array, double re, double im) {
    for (size_t i = 0; i < array->size; i++) {
        array->data[i * 2] = re;
        array->data[i * 2 + 1] = im;
    }
}

/* Staged through a local copy because shape may alias array->shape: callers
   that republish an array's own layout (a source that is also the destination)
   would otherwise read back the buffer the memset just cleared. */
void set_csnarray_layout(CSN_ARRAY *array, uint32_t ndim, const uint32_t *shape, size_t size, ITEM_TYPE itype) {
    uint32_t requested[CSN_MAX_DIMS] = {0};
    memcpy(requested, shape, sizeof(uint32_t) * ndim);

    /* Compared before the write, never bumped unconditionally: this function
       is re-stamped on every k-pass by NEED_TO_UPDATE_SLOT even when nothing
       moved, and a blind bump there would make shape_version useless to any
       consumer caching an index map. */
    bool shape_changed = array->size != size || memcmp(array->shape, requested, sizeof(array->shape)) != 0;
    bool ndim_changed = array->ndim != ndim;
    bool itype_changed = array->itype != itype;

    array->size = size;
    array->ndim = ndim;
    array->itype = itype;
    memset(array->shape, 0, sizeof(array->shape));
    memset(array->strides, 0, sizeof(array->strides));
    memcpy(array->shape, requested, sizeof(uint32_t) * ndim);
    compute_strides(array->shape, array->strides, ndim);

    update_array_layout_version(&array->version, shape_changed, ndim_changed, itype_changed);
}

/* csnempty reserves the requested shape but exposes no logical elements yet.
   Shape describes the allocated/indexable layout; size is the number of
   elements currently present. */
void reset_empty_csnarray(CSN_ARRAY *array, uint32_t ndim, const uint32_t *requested_shape, ITEM_TYPE itype) {
    set_csnarray_layout(array, ndim, requested_shape, 0, itype);
}

/* An opcode that reads one array and republishes another cannot have the two be
   the same slot: NEED_TO_UPDATE_SLOT reallocates the destination and drops the
   data it is then asked to read, and the fills index source and destination with
   different layouts. It bites at k-rate only, where the output handle written on
   the previous pass comes back in as the input. The in-place overloads (no
   output handle) exist for that case and use their own scratch buffer.

   Pass 0 for handle_b when the opcode takes a single input; the scalar
   reduction forms own no slot, and a zero owned_handle never matches. */
int32_t CHECK_SELF_ALIAS(CSOUND *csound, OPDS *h, const K_DATA *k_data, uint32_t handle_a, uint32_t handle_b) {
    uint32_t owned = k_data->owned_handle;
    if (owned == 0) {
        return OK;
    }

    if (handle_a == owned || handle_b == owned) {
        return csound->PerfError(csound, h, "[csnarray] Input array %u is also this opcode's own output: assigning the result back to its own input is not supported at k-rate, use the in-place overload (the form without an output handle)", owned);
    }
    return OK;
}

/* Republishes the opcode's own k-rate output slot with the requested layout,
   taking new storage only when the one the slot has cannot hold it. The caller
   holds the registry mutex; *destination is an output, so no caller has to
   seed it. */
int32_t NEED_TO_UPDATE_SLOT(CSOUND *csound, OPDS *h, CSN_ARRAY **destination, K_DATA *k_data, uint32_t *owned_handle, uint32_t ndim, const uint32_t *shape, size_t logical_size, ITEM_TYPE itype, const char *err) {
    size_t requested_size = 0;
    if (get_array_size_from_shape(&requested_size, ndim, shape) != OK) {
        return csn_locked_perf_error(csound, h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    if (logical_size > requested_size) {
          return csn_locked_perf_error(csound, h, "[csnarray] Logical size %zu exceeds physical size %zu", logical_size, requested_size);
    }

    uint32_t req_owned_handle = owned_handle == NULL ? k_data->owned_handle : *owned_handle;

    CSN_SLOT *slot = get_slot(k_data->registry, req_owned_handle);
    if (slot == NULL) {
        return csn_locked_perf_error(csound, h, "[csnarray] k-rate output slot is no longer active");
    }
    *destination = slot->array;

    CSN_ARRAY *current = *destination;
    bool needs_storage = current->data == NULL || current->itype != itype || current->capacity < requested_size;
    if (needs_storage) {
        if (slot->rt_locked) {
            return csn_locked_perf_error(csound, h,  "[csnarray] '%s' (array %u) is on a real-time path and cannot be reallocated at perf time; clear the mark with csnrtunlock, or pass irt=0 at the audio source it descends from", get_out_name(h), req_owned_handle);
        }
        int32_t res = update_slot_array_locked(csound, k_data->registry, req_owned_handle, ndim, shape, itype, destination, &err);
        if (res != OK) {
            return csn_locked_perf_error(csound, h, "[csnarray] Could not update k-rate output slot: %s", err != NULL ? err : "unknown error");
        }
    } else if (IS_REQUEST_CHANGED(k_data, ndim, itype, shape)) {
        /* A new layout that fits the storage the slot already has reuses it, as
           csnreshape always has, so a marked output can change shape within
           its capacity without reaching the allocator. The region is cleared
           to what a fresh buffer would have held: a producer that writes only
           part of its output, the off-diagonal of an identity or the tail of a
           zero-padded transform, still finds zeros there. */
        memset(current->data, 0, sizeof(double) * requested_size * (size_t) itype);
    }

    /* Re-stamped on every pass, not only when the slot is reallocated: an
       in-place opcode (csnreshape_in, csnflatten_in, csntranspose_in) may have
       rewritten this same array's layout since the last update, and the check
       above short-circuits on an unchanged request. Capacity follows the
       physical shape, while size follows the caller's logical element count
       (notably zero for csnempty). */
    set_csnarray_layout(*destination, ndim, shape, logical_size, itype);

    /* Every k-rate producer routes through here, and only on a pass that goes
       on to write its output: the trigger check and the early error returns
       both come first. That makes this the point where the slot's contents
       become a new generation, even though the fill runs just after. A
       producer held at trig == 0 never reaches it, which is exactly what lets
       a downstream consumer skip its own work. */
    update_array_data_version(&(*destination)->version);
    return OK;
}

/* COMPLEXDAT isPolar -> rectangular */
void complexdat_to_rect(const COMPLEXDAT *c, double *re, double *im) {
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

int32_t csnarray_deinit_by_handle(CSOUND *csound, uint32_t *handle_id, CSN_ARRAY **array, const OPDS *h) {
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
    deinit_scratch(csound, &p->scratch);
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

static int32_t csnarray_where_deinit(CSOUND *csound, void *p) {
    CSN_WHERE_COMMON *ptr = (CSN_WHERE_COMMON *) p;
    deinit_scratch(csound, &ptr->scratch);
    return csnarray_deinit_by_handle(csound, &ptr->handle->id, &ptr->array, &ptr->h);
}

static int32_t csnarray_compare_deinit(CSOUND *csound, CSN_COMPARE *p) {
    deinit_scratch(csound, &p->scratch);
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

static int32_t csnarray_reduction_deinit(CSOUND *csound, CSN_REDUCTION *p) {
    deinit_scratch(csound, &p->scratch);
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
    deinit_scratch(csound, &p->scratch);
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

static int32_t csnarray_norm_deinit(CSOUND *csound, CSN_NORM_REDUCTION *p) {
    deinit_scratch(csound, &p->scratch);
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

static int32_t csnarray_movstats_deinit(CSOUND *csound, CSN_MOVSTATS *p) {
    deinit_scratch(csound, &p->scratch);
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
    deinit_scratch(csound, &p->scratch);
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

static int32_t csnarray_window_deinit(CSOUND *csound, CSN_WINDOW *p) {
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

static int32_t csnarray_divmod_deinit(CSOUND *csound, void *p) {
    CSN_DIVMOD_COMMON *ptr = (CSN_DIVMOD_COMMON *) p;
    if (csnarray_deinit_by_handle(csound, &ptr->handle_a->id, &ptr->array_a, &ptr->h) != OK) {
        return NOTOK;
    };
    return csnarray_deinit_by_handle(csound, &ptr->handle_b->id, &ptr->array_b, &ptr->h);
}

static int32_t csnarray_from_ftable_deinit(CSOUND *csound, CSN_FROM_FTABLE *p) {
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

static int32_t csnarray_remap_deinit(CSOUND *csound, CSN_REMAP *p) {
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

static int32_t csnarray_truncate_deinit(CSOUND *csound, CSN_TRUNCATE *p) {
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

static int32_t csnarray_resize_deinit(CSOUND *csound, CSN_RESIZE *p) {
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

static int32_t csnarray_resample_deinit(CSOUND *csound, CSN_RESAMPLE *p) {
    deinit_scratch(csound, &p->x_source_scratch);
    deinit_scratch(csound, &p->x_data_scratch);
    deinit_scratch(csound, &p->y_data_scratch);
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

static int32_t csnarray_load_deinit(CSOUND *csound, CSN_LOAD *p) {
    deinit_scratch(csound, &p->buffer_scratch);
    deinit_scratch(csound, &p->path_scratch);
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

static int32_t csnarray_from_audio_deinit(CSOUND *csound, void *p) {
    CSN_AUDIO_BRIDGE_COMMON *ptr = (CSN_AUDIO_BRIDGE_COMMON *) p;
    return csnarray_deinit_by_handle(csound, &ptr->handle->id, &ptr->array, &ptr->h);
}

static int32_t csnarray_frame_audio_deinit(CSOUND *csound, CSN_FRAME_AUDIO *p) {
    deinit_scratch(csound, &p->buffer);
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

static int32_t csnarray_get_rowcol_deinit(CSOUND *csound, CSN_GET_ROWCOL *p) {
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

static int32_t csnarray_stack_deinit(CSOUND *csound, CSN_STACK *p) {
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

static int32_t csnarray_stack_k_deinit(CSOUND *csound, CSN_STACK_K *p) {
    deinit_scratch(csound, &p->buffer_handles);
    deinit_scratch(csound, &p->buffer_sources);
    deinit_scratch(csound, &p->buffer_temp_sources);
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

const char *shape_str(char *buf, size_t buf_size, const uint32_t *shape, uint32_t ndim) {
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

/* The blanket rejection in CHECK_SELF_ALIAS is too strong for an opcode whose
   fill writes each result cell from the input cell sitting at the same index:
   X = csnsin(X) is perfectly well defined, and every cell is read before it is
   written. What such an opcode still cannot survive is a reallocation, which
   would hand the fill a fresh zeroed buffer and drop the data it is about to
   read. So the permission is conditional on the requested layout matching the
   aliased array's — the same test the elementwise binops already apply.

   Callers must resolve the source and compute the output layout first, then
   call this in place of CHECK_SELF_ALIAS. */
int32_t CHECK_SELF_ALIAS_CELL_LOCAL(CSOUND *csound, OPDS *h, const K_DATA *k_data, uint32_t source_handle, const CSN_ARRAY *source_arr, uint32_t new_ndim, const uint32_t *new_shape, ITEM_TYPE new_itype) {
    uint32_t owned = k_data->owned_handle;
    if (owned == 0 || source_handle != owned) {
        return OK;
    }

    if (source_arr->ndim != new_ndim
        || source_arr->itype != new_itype
        || memcmp(source_arr->shape, new_shape, sizeof(uint32_t) * new_ndim) != 0) {
        char abuf[CSN_SHAPE_STR_MAX], bbuf[CSN_SHAPE_STR_MAX];
        return csn_locked_perf_error(csound, h, "[csnarray] Input array %u is also this opcode's own output, so the result must keep its %s layout, not %s: assign the result to a different handle", owned, shape_str(abuf, sizeof(abuf), source_arr->shape, source_arr->ndim), shape_str(bbuf, sizeof(bbuf), new_shape, new_ndim));
    }
    return OK;
}

void from_linear_to_coords(uint32_t *coords, const uint32_t *shape, size_t linear, uint32_t ndim) {
    for (uint32_t i = ndim; i-- > 0;) {
        coords[i] = (uint32_t) (linear % shape[i]);
        linear /= shape[i];
    }
}

uint32_t from_coords_to_offset(uint32_t *coords, const size_t *strides, uint32_t ndim) {
    size_t src_offset = 0;
    for (uint32_t i = 0; i < ndim; ++i) {
        src_offset += (size_t) coords[i] * strides[i];
    }
    return src_offset;
}

int32_t parse_shape_array(CSOUND *csound, const ARRAYDAT *p_shape, uint32_t *out_ndim, uint32_t *out_shape) {
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

int32_t parse_shape_array_k(CSOUND *csound, OPDS *h, const ARRAYDAT *p_shape, uint32_t *out_ndim, uint32_t *out_shape) {
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
int32_t create_csnarray_locked(
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

    if (reg->rt_glock_locked && (reg->rt_glock_owner != NULL && reg->rt_glock_owner == h->insdshead)) {
        slot->rt_locked = true;
    }

    for (uint32_t i = 0; i < protect_count; i++) {
        CSN_SLOT *src = get_slot(reg, protect[i]);
        if (src != NULL && src->rt_locked) { slot->rt_locked = true; }
    }

    *p_array = slot->array;
    p_handle->id = handle;
    return OK;
}

/* Takes the lock itself; for opcodes that hold nothing on entry. */
int32_t create_csnarray_init(
    CSOUND *csound,
    const OPDS *h,
    uint32_t ndim,
    const uint32_t *shape,
    CSN_ARRAY **p_array,
    CSNREF *p_handle,
    ITEM_TYPE itype
) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

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

int32_t csnarray_set_seed(CSOUND *csound, CSN_SEED *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    double seed_temp = (double) *p->seed;
    if (!IS_VALID_SEED(seed_temp)) {
        return csound->InitError(csound, "[csnarray] Invalid seed %g: expected a non-negative integer up to 2^53 (0 seeds from the clock)", seed_temp);
    }
    uint64_t seed = (uint64_t) seed_temp;
    csound->LockMutex(reg->mutex);
    pcg32_random_init(&reg->rng, seed);
    csound->UnlockMutex(reg->mutex);
    return OK;
}

static int32_t csnarray_set_rtlock_helper(CSOUND *csound, OPDS *perf_h, CSN_RTLOCK *p, bool lock) {
    CSN_REGISTRY *reg = perf_h == NULL ? get_registry(csound) : p->registry;
    CHECK_REGISTRY(csound, perf_h, reg);
    if (perf_h != NULL) CHECK_KTRIG(p->trig);
    uint32_t source_handle = p->source_handle->id;
    int32_t res = OK;
    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
        goto done;
    }
    slot->rt_locked = lock;
    p->registry = reg;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_grtlock_deinit(CSOUND *csound, CSN_GRTLOCK *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, NULL, reg);
    if (reg->rt_glock_owner != NULL && (reg->rt_glock_owner == p->h.insdshead)) {
        reg->rt_glock_owner = NULL;
        reg->rt_glock_locked = false;
    }
    return OK;
}

static int32_t csnarray_set_grtlock_helper(CSOUND *csound, OPDS *perf_h, CSN_GRTLOCK *p, bool lock, bool all) {
    if (all && p->h.insdshead->insno != 0) {
        return csound->InitError(csound, "[csnarray] csnrtlockall belongs in the orchestra header, where it holds for the whole performance; inside an instrument use csnrtlockstart, which holds until csnrtlockend or the end of the note");
    }

    CSN_REGISTRY *reg = perf_h == NULL ? get_registry(csound) : p->registry;
    CHECK_REGISTRY(csound, perf_h, reg);
    if (perf_h != NULL) CHECK_KTRIG(p->trig);
    csound->LockMutex(reg->mutex);
    if (all) {
        reg->rt_glock_global = true;
    } else {
        if (lock) {
            reg->rt_glock_owner = p->h.insdshead;
            reg->rt_glock_locked = true;
        } else {
            if (reg->rt_glock_owner == p->h.insdshead) {
                reg->rt_glock_owner = NULL;
                reg->rt_glock_locked = false;
            }
        }
    }
    p->registry = reg;
    csound->UnlockMutex(reg->mutex);
    return OK;
}

int32_t csnarray_set_rtlock(CSOUND *csound, CSN_RTLOCK *p) {
    return csnarray_set_rtlock_helper(csound, NULL, p, true);
}

int32_t csnarray_set_rtunlock(CSOUND *csound, CSN_RTLOCK *p) {
    return csnarray_set_rtlock_helper(csound, NULL, p, false);
}

int32_t csnarray_set_grtlock(CSOUND *csound, CSN_GRTLOCK *p) {
    return csnarray_set_grtlock_helper(csound, NULL, p, true, false);
}

int32_t csnarray_set_grtunlock(CSOUND *csound, CSN_GRTLOCK *p) {
    return csnarray_set_grtlock_helper(csound, NULL, p, false, false);
}

int32_t csnarray_set_grtlock_all(CSOUND *csound, CSN_GRTLOCK *p) {
    return csnarray_set_grtlock_helper(csound, NULL, p, true, true);
}

static int32_t csnarray_set_rtlock_k_init_helper(CSOUND *csound, CSN_RTLOCK *p, bool lock) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    p->registry = reg;
    if ((double) *p->trig == 0.0) return OK;
    return csnarray_set_rtlock_helper(csound, NULL, p, lock);
}

static int32_t csnarray_set_grtlock_k_init_helper(CSOUND *csound, CSN_GRTLOCK *p, bool lock) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    p->registry = reg;
    if ((double) *p->trig == 0.0) return OK;
    return csnarray_set_grtlock_helper(csound, NULL, p, lock, false);
}

int32_t csnarray_set_rtlock_k_init(CSOUND *csound, CSN_RTLOCK *p) {
    return csnarray_set_rtlock_k_init_helper(csound, p, true);
}

int32_t csnarray_set_rtunlock_k_init(CSOUND *csound, CSN_RTLOCK *p) {
    return csnarray_set_rtlock_k_init_helper(csound, p, false);
}

int32_t csnarray_set_rtlock_k(CSOUND *csound, CSN_RTLOCK *p) {
    return csnarray_set_rtlock_helper(csound, &p->h, p, true);
}

int32_t csnarray_set_rtunlock_k(CSOUND *csound, CSN_RTLOCK *p) {
    return csnarray_set_rtlock_helper(csound, &p->h, p, false);
}


int32_t csnarray_set_grtlock_k_init(CSOUND *csound, CSN_GRTLOCK *p) {
    return csnarray_set_grtlock_k_init_helper(csound, p, true);
}

int32_t csnarray_set_grtunlock_k_init(CSOUND *csound, CSN_GRTLOCK *p) {
    return csnarray_set_grtlock_k_init_helper(csound, p, false);
}

int32_t csnarray_set_grtlock_k(CSOUND *csound, CSN_GRTLOCK *p) {
    return csnarray_set_grtlock_helper(csound, &p->h, p, true, false);
}

int32_t csnarray_set_grtunlock_k(CSOUND *csound, CSN_GRTLOCK *p) {
    return csnarray_set_grtlock_helper(csound, &p->h, p, false, false);
}

// --- OENTRY ---

#define S(x) sizeof(x)

static OENTRY localops[] = {
    { "csnseed",               S(CSN_SEED),                   0, "",                         "i",                             (SUBR) csnarray_set_seed,                    NULL,                                   NULL,                                    NULL, 0 },
    { "csnsave",               S(CSN_SAVE),                   0, "",                         ":CsnArr;S",                     (SUBR) csnarray_save,                        NULL,                                   NULL,                                    NULL, 0 },
    { "csnload",               S(CSN_LOAD),                   0, ":CsnArr;",                 "S",                             (SUBR) csnarray_load,                        NULL,                                   (SUBR) csnarray_load_deinit,             NULL, 0 },
    { "csnsave.k",             S(CSN_SAVE),                   0, "",                         ":CsnArr;Sk",                    (SUBR) csnarray_save_k_init,                 (SUBR) csnarray_save_k,                 (SUBR) csnarray_save_k_deinit,           NULL, 0 },
    { "csnload.k",             S(CSN_LOAD),                   0, ":CsnArr;",                 "Sk",                            (SUBR) csnarray_load_k_init,                 (SUBR) csnarray_load_k,                 (SUBR) csnarray_load_deinit,             NULL, 0 },
    { "csnrtlock",             S(CSN_RTLOCK),                 0, "",                         ":CsnArr;",                      (SUBR) csnarray_set_rtlock,                  NULL,                                   NULL,                                    NULL, 0 },
    { "csnrtunlock",           S(CSN_RTLOCK),                 0, "",                         ":CsnArr;",                      (SUBR) csnarray_set_rtunlock,                NULL,                                   NULL,                                    NULL, 0 },
    { "csnrtlock.k",           S(CSN_RTLOCK),                 0, "",                         ":CsnArr;P",                     (SUBR) csnarray_set_rtlock_k_init,           (SUBR) csnarray_set_rtlock_k,           NULL,                                    NULL, 0 },
    { "csnrtunlock.k",         S(CSN_RTLOCK),                 0, "",                         ":CsnArr;P",                     (SUBR) csnarray_set_rtunlock_k_init,         (SUBR) csnarray_set_rtunlock_k,         NULL,                                    NULL, 0 },
    { "csnrtlockstart",        S(CSN_GRTLOCK),                0, "",                         "",                              (SUBR) csnarray_set_grtlock,                 NULL,                                   (SUBR) csnarray_grtlock_deinit,          NULL, 0 },
    { "csnrtlockend",          S(CSN_GRTLOCK),                0, "",                         "",                              (SUBR) csnarray_set_grtunlock,               NULL,                                   (SUBR) csnarray_grtlock_deinit,          NULL, 0 },
    { "csnrtlockstart.k",      S(CSN_GRTLOCK),                0, "",                         "P",                             (SUBR) csnarray_set_grtlock_k_init,          (SUBR) csnarray_set_grtlock_k,          (SUBR) csnarray_grtlock_deinit,          NULL, 0 },
    { "csnrtlockend.k",        S(CSN_GRTLOCK),                0, "",                         "P",                             (SUBR) csnarray_set_grtunlock_k_init,        (SUBR) csnarray_set_grtunlock_k,        (SUBR) csnarray_grtlock_deinit,          NULL, 0 },
    { "csnrtlockall",          S(CSN_GRTLOCK),                0, "",                         "",                              (SUBR) csnarray_set_grtlock_all,             NULL,                                   NULL,                                    NULL, 0 },
    // REAL-ONLY
    { "csnfromaudio",          S(CSN_FROM_AUDIO),             0, ":CsnArr;",                 "ap",                            (SUBR) csnarray_from_audio_init,             (SUBR) csnarray_from_audio,             (SUBR) csnarray_from_audio_deinit,       NULL, 0 },
    { "csntoaudio",            S(CSN_TO_AUDIO),               0, "a",                        ":CsnArr;",                      (SUBR) csnarray_to_audio_init,               (SUBR) csnarray_to_audio,               NULL,                                    NULL, 0 },
    { "csnpack",               S(CSN_PACK_AUDIO),             0, ":CsnArr;",                 "a[]p",                          (SUBR) csnarray_pack_audio_init,             (SUBR) csnarray_pack_audio,             (SUBR) csnarray_from_audio_deinit,       NULL, 0 },
    { "csnunpack",             S(CSN_UNPACK_AUDIO),           0, "a[]",                      ":CsnArr;",                      (SUBR) csnarray_unpack_audio_init,           (SUBR) csnarray_unpack_audio,           NULL,                                    NULL, 0 },
    { "csnsnap",               S(CSN_FRAME_AUDIO),            0, ":CsnArr;k",                "aiop",                          (SUBR) csnarray_frame_audio_init,            (SUBR) csnarray_frame_audio,            (SUBR) csnarray_frame_audio_deinit,      NULL, 0 },
    { "csnstream",             S(CSN_OLA_AUDIO),              0, "ak",                       ":CsnArr;i",                     (SUBR) csnarray_ola_audio_init,              (SUBR) csnarray_ola_audio,              (SUBR) csnarray_ola_audio_deinit,        NULL, 0 },
    { "csnrand",               S(CSN_ARR_RND_INIT),           0, ":CsnArr;",                 "i[]ii",                         (SUBR) create_random_csnarray,               NULL,                                   (SUBR) create_csnarray_random_deinit,    NULL, 0 },
    { "csnrand.k",             S(CSN_ARR_RND_INIT),           0, ":CsnArr;",                 "k[]kkP",                        (SUBR) create_random_csnarray_k_init,        (SUBR) create_random_csnarray_k,        (SUBR) create_csnarray_random_deinit,    NULL, 0 },
    { "csnrandint",            S(CSN_ARR_RND_INIT),           0, ":CsnArr;",                 "i[]ii",                         (SUBR) create_randomint_csnarray,            NULL,                                   (SUBR) create_csnarray_random_deinit,    NULL, 0 },
    { "csnrandint.k",          S(CSN_ARR_RND_INIT),           0, ":CsnArr;",                 "k[]kkP",                        (SUBR) create_random_csnarray_k_init,        (SUBR) create_randomint_csnarray_k,     (SUBR) create_csnarray_random_deinit,    NULL, 0 },
    { "csnshuffle",            S(CSN_UNARYOP_IN),             0, "",                         ":CsnArr;",                      (SUBR) csnarray_shuffle,                     NULL,                                   NULL,                                    NULL, 0 },
    { "csnshuffle.k",          S(CSN_UNARYOP_IN),             0, "",                         ":CsnArr;P",                     (SUBR) csnarray_shuffle_k_init,              (SUBR) csnarray_shuffle_k,              NULL,                                    NULL, 0 },
    { "csnarange",             S(CSN_SPACED_SPACE),           0, ":CsnArr;",                 "iii",                           (SUBR) csnarray_arange,                      NULL,                                   (SUBR) csnarray_space_spaced_deinit,     NULL, 0 },
    { "csnlinspace",           S(CSN_SPACED_SPACE),           0, ":CsnArr;",                 "iii",                           (SUBR) csnarray_linspace,                    NULL,                                   (SUBR) csnarray_space_spaced_deinit,     NULL, 0 },
    { "csnlogspace",           S(CSN_SPACED_SPACE),           0, ":CsnArr;",                 "iiii",                          (SUBR) csnarray_logspace,                    NULL,                                   (SUBR) csnarray_space_spaced_deinit,     NULL, 0 },
    { "csngeomspace",          S(CSN_SPACED_SPACE),           0, ":CsnArr;",                 "iii",                           (SUBR) csnarray_geomspace,                   NULL,                                   (SUBR) csnarray_space_spaced_deinit,     NULL, 0 },
    { "csnarange.k",           S(CSN_SPACED_SPACE),           0, ":CsnArr;",                 "kkkk",                          (SUBR) csnarray_spaced_space_k_init,         (SUBR) csnarray_arange_k,               (SUBR) csnarray_space_spaced_deinit,     NULL, 0 },
    { "csnlinspace.k",         S(CSN_SPACED_SPACE),           0, ":CsnArr;",                 "kkkk",                          (SUBR) csnarray_spaced_space_k_init,         (SUBR) csnarray_linspace_k,             (SUBR) csnarray_space_spaced_deinit,     NULL, 0 },
    { "csnlogspace.k",         S(CSN_SPACED_SPACE),           0, ":CsnArr;",                 "kkkkk",                         (SUBR) csnarray_spaced_space_k_init,         (SUBR) csnarray_logspace_k,             (SUBR) csnarray_space_spaced_deinit,     NULL, 0 },
    { "csngeomspace.k",        S(CSN_SPACED_SPACE),           0, ":CsnArr;",                 "kkkk",                          (SUBR) csnarray_spaced_space_k_init,         (SUBR) csnarray_geomspace_k,            (SUBR) csnarray_space_spaced_deinit,     NULL, 0 },
    { "csnclip",               S(CSN_CLIP),                   0, ":CsnArr;",                 ":CsnArr;ii",                    (SUBR) csnarray_clip,                        NULL,                                   (SUBR) csnarray_clip_deinit,             NULL, 0 },
    { "csnclip.k",             S(CSN_CLIP),                   0, ":CsnArr;",                 ":CsnArr;kkP",                   (SUBR) csnarray_clip_k_init,                 (SUBR) csnarray_clip_k,                 (SUBR) csnarray_clip_deinit,             NULL, 0 },
    { "csnclip.in",            S(CSN_CLIP_IN),                0, "",                         ":CsnArr;ii",                    (SUBR) csnarray_clip_in,                     NULL,                                   NULL,                                    NULL, 0 },
    { "csnclip.in.k",          S(CSN_CLIP_IN),                0, "",                         ":CsnArr;kkP",                   (SUBR) csnarray_clip_in_k_init,              (SUBR) csnarray_clip_in_k,              NULL,                                    NULL, 0 },
    { "csnargwhere",           S(CSN_ARGWHERE),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;",              (SUBR) csnarray_argwhere,                    NULL,                                   (SUBR) csnarray_argwhere_deinit,         NULL, 0 },
    { "csnargwhere.k",         S(CSN_ARGWHERE),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;k",             (SUBR) csnarray_argwhere_k_init,             (SUBR) csnarray_argwhere_k,             (SUBR) csnarray_argwhere_deinit,         NULL, 0 },
    { "csnargnonzero",         S(CSN_ARGWHERE),               0, ":CsnArr;",                 ":CsnArr;",                      (SUBR) csnarray_argnonzero,                  NULL,                                   (SUBR) csnarray_argwhere_deinit,         NULL, 0 },
    { "csnargnonzero.k",       S(CSN_ARGWHERE),               0, ":CsnArr;",                 ":CsnArr;k",                     (SUBR) csnarray_argselect_k_init,            (SUBR) csnarray_argnonzero_k,           (SUBR) csnarray_argwhere_deinit,         NULL, 0 },
    { "csnargisnan",           S(CSN_ARGWHERE),               0, ":CsnArr;",                 ":CsnArr;",                      (SUBR) csnarray_argisnan,                    NULL,                                   (SUBR) csnarray_argwhere_deinit,         NULL, 0 },
    { "csnargisnan.k",         S(CSN_ARGWHERE),               0, ":CsnArr;",                 ":CsnArr;k",                     (SUBR) csnarray_argselect_k_init,            (SUBR) csnarray_argisnan_k,             (SUBR) csnarray_argwhere_deinit,         NULL, 0 },
    { "csnargunique",          S(CSN_ARGWHERE),               0, ":CsnArr;",                 ":CsnArr;",                      (SUBR) csnarray_argunique,                   NULL,                                   (SUBR) csnarray_argwhere_deinit,         NULL, 0 },
    { "csnargunique.k",        S(CSN_ARGWHERE),               0, ":CsnArr;",                 ":CsnArr;k",                     (SUBR) csnarray_argunique_k_init,            (SUBR) csnarray_argunique_k,            (SUBR) csnarray_argwhere_deinit,         NULL, 0 },
    { "csnunique",             S(CSN_COMPARE),                0, ":CsnArr;",                 ":CsnArr;",                      (SUBR) csnarray_unique,                      NULL,                                   (SUBR) csnarray_compare_deinit,          NULL, 0 },
    { "csnunique.k",           S(CSN_COMPARE),                0, ":CsnArr;",                 ":CsnArr;k",                     (SUBR) csnarray_unique_k_init,               (SUBR) csnarray_unique_k,               (SUBR) csnarray_compare_deinit,          NULL, 0 },
    { "csngt",                 S(CSN_COMPARE),                0, ":CsnArr;",                 ":CsnArr;i",                     (SUBR) csnarray_greater_than,                NULL,                                   (SUBR) csnarray_compare_deinit,          NULL, 0 },
    { "csnlt",                 S(CSN_COMPARE),                0, ":CsnArr;",                 ":CsnArr;i",                     (SUBR) csnarray_less_than,                   NULL,                                   (SUBR) csnarray_compare_deinit,          NULL, 0 },
    { "csnne",                 S(CSN_COMPARE),                0, ":CsnArr;",                 ":CsnArr;i",                     (SUBR) csnarray_not_equal,                   NULL,                                   (SUBR) csnarray_compare_deinit,          NULL, 0 },
    { "csnge",                 S(CSN_COMPARE),                0, ":CsnArr;",                 ":CsnArr;i",                     (SUBR) csnarray_greater_equal,               NULL,                                   (SUBR) csnarray_compare_deinit,          NULL, 0 },
    { "csnle",                 S(CSN_COMPARE),                0, ":CsnArr;",                 ":CsnArr;i",                     (SUBR) csnarray_less_equal,                  NULL,                                   (SUBR) csnarray_compare_deinit,          NULL, 0 },
    { "csneq",                 S(CSN_COMPARE),                0, ":CsnArr;",                 ":CsnArr;i",                     (SUBR) csnarray_equal,                       NULL,                                   (SUBR) csnarray_compare_deinit,          NULL, 0 },
    { "csngt.k",               S(CSN_COMPARE),                0, ":CsnArr;",                 ":CsnArr;kP",                    (SUBR) csnarray_compare_k_init,              (SUBR) csnarray_greater_than_k,         (SUBR) csnarray_compare_deinit,          NULL, 0 },
    { "csnlt.k",               S(CSN_COMPARE),                0, ":CsnArr;",                 ":CsnArr;kP",                    (SUBR) csnarray_compare_k_init,              (SUBR) csnarray_less_than_k,            (SUBR) csnarray_compare_deinit,          NULL, 0 },
    { "csnne.k",               S(CSN_COMPARE),                0, ":CsnArr;",                 ":CsnArr;kP",                    (SUBR) csnarray_compare_k_init,              (SUBR) csnarray_not_equal_k,            (SUBR) csnarray_compare_deinit,          NULL, 0 },
    { "csnge.k",               S(CSN_COMPARE),                0, ":CsnArr;",                 ":CsnArr;kP",                    (SUBR) csnarray_compare_k_init,              (SUBR) csnarray_greater_equal_k,        (SUBR) csnarray_compare_deinit,          NULL, 0 },
    { "csnle.k",               S(CSN_COMPARE),                0, ":CsnArr;",                 ":CsnArr;kP",                    (SUBR) csnarray_compare_k_init,              (SUBR) csnarray_less_equal_k,           (SUBR) csnarray_compare_deinit,          NULL, 0 },
    { "csneq.k",               S(CSN_COMPARE),                0, ":CsnArr;",                 ":CsnArr;kP",                    (SUBR) csnarray_compare_k_init,              (SUBR) csnarray_equal_k,                (SUBR) csnarray_compare_deinit,          NULL, 0 },
    { "csncnteq",              S(CSN_COUNT),                  0, "i",                        ":CsnArr;i",                     (SUBR) csnarray_count_equal,                 NULL,                                   NULL,                                    NULL, 0 },
    { "csncntnz",              S(CSN_COUNT),                  0, "i",                        ":CsnArr;",                      (SUBR) csnarray_count_nonzero,               NULL,                                   NULL,                                    NULL, 0 },
    { "csncntnan",             S(CSN_COUNT),                  0, "i",                        ":CsnArr;",                      (SUBR) csnarray_count_nan,                   NULL,                                   NULL,                                    NULL, 0 },
    { "csncnteq.k",            S(CSN_COUNT),                  0, "k",                        ":CsnArr;kk",                    (SUBR) csnarray_compare_count_k_init,        (SUBR) csnarray_count_equal_k,          NULL,                                    NULL, 0 },
    { "csncntnz.k",            S(CSN_COUNT),                  0, "k",                        ":CsnArr;k",                     (SUBR) csnarray_compare_count_k_init,        (SUBR) csnarray_count_nonzero_k,        NULL,                                    NULL, 0 },
    { "csncntnan.k",           S(CSN_COUNT),                  0, "k",                        ":CsnArr;k",                     (SUBR) csnarray_compare_count_k_init,        (SUBR) csnarray_count_nan_k,            NULL,                                    NULL, 0 },
    { "csnmin",                S(CSN_REDUCTION_SCALAR),       0, "i",                        ":CsnArr;",                      (SUBR) csnarray_min_all,                     NULL,                                   NULL,                                    NULL, 0 },
    { "csnmin.k",              S(CSN_REDUCTION_SCALAR),       0, "k",                        ":CsnArr;P",                     (SUBR) csnarray_min_all_k_init,              (SUBR) csnarray_min_all_k,              NULL,                                    NULL, 0 },
    { "csnmax",                S(CSN_REDUCTION_SCALAR),       0, "i",                        ":CsnArr;",                      (SUBR) csnarray_max_all,                     NULL,                                   NULL,                                    NULL, 0 },
    { "csnmax.k",              S(CSN_REDUCTION_SCALAR),       0, "k",                        ":CsnArr;P",                     (SUBR) csnarray_max_all_k_init,              (SUBR) csnarray_max_all_k,              NULL,                                    NULL, 0 },
    { "csnmedian",             S(CSN_REDUCTION_SCALAR),       0, "i",                        ":CsnArr;",                      (SUBR) csnarray_median_all,                  NULL,                                   NULL,                                    NULL, 0 },
    { "csnmedian.k",           S(CSN_REDUCTION_SCALAR),       0, "k",                        ":CsnArr;P",                     (SUBR) csnarray_median_all_k_init,           (SUBR) csnarray_median_all_k,           (SUBR) csnarray_median_scalar_k_deinit,  NULL, 0 },
    { "csnmin.ax",             S(CSN_REDUCTION),              0, ":CsnArr;",                 ":CsnArr;i",                     (SUBR) csnarray_min,                         NULL,                                   (SUBR) csnarray_reduction_deinit,        NULL, 0 },
    { "csnmin.ax.k",           S(CSN_REDUCTION),              0, ":CsnArr;",                 ":CsnArr;kk",                    (SUBR) csnarray_min_k_init,                  (SUBR) csnarray_min_k,                  (SUBR) csnarray_reduction_deinit,        NULL, 0 },
    { "csnmax.ax",             S(CSN_REDUCTION),              0, ":CsnArr;",                 ":CsnArr;i",                     (SUBR) csnarray_max,                         NULL,                                   (SUBR) csnarray_reduction_deinit,        NULL, 0 },
    { "csnmax.ax.k",           S(CSN_REDUCTION),              0, ":CsnArr;",                 ":CsnArr;kk",                    (SUBR) csnarray_max_k_init,                  (SUBR) csnarray_max_k,                  (SUBR) csnarray_reduction_deinit,        NULL, 0 },
    { "csnmedian.ax",          S(CSN_REDUCTION),              0, ":CsnArr;",                 ":CsnArr;i",                     (SUBR) csnarray_median,                      NULL,                                   (SUBR) csnarray_reduction_deinit,        NULL, 0 },
    { "csnmedian.ax.k",        S(CSN_REDUCTION),              0, ":CsnArr;",                 ":CsnArr;kk",                    (SUBR) csnarray_median_k_init,               (SUBR) csnarray_median_k,               (SUBR) csnarray_reduction_deinit,        NULL, 0 },
    { "csnargmin",             S(CSN_REDUCTION),              0, ":CsnArr;",                 ":CsnArr;",                      (SUBR) csnarray_argmin,                      NULL,                                   (SUBR) csnarray_reduction_deinit,        NULL, 0 },
    { "csnargmin",             S(CSN_REDUCTION),              0, ":CsnArr;",                 ":CsnArr;i",                     (SUBR) csnarray_argmin,                      NULL,                                   (SUBR) csnarray_reduction_deinit,        NULL, 0 },
    { "csnargmin.k",           S(CSN_REDUCTION),              0, ":CsnArr;",                 ":CsnArr;P",                     (SUBR) csnarray_argmin_k_init,               (SUBR) csnarray_argmin_k,               (SUBR) csnarray_reduction_deinit,        NULL, 0 },
    { "csnargmin.k",           S(CSN_REDUCTION),              0, ":CsnArr;",                 ":CsnArr;kk",                    (SUBR) csnarray_argmin_k_init,               (SUBR) csnarray_argmin_k,               (SUBR) csnarray_reduction_deinit,        NULL, 0 },
    { "csnargmax",             S(CSN_REDUCTION),              0, ":CsnArr;",                 ":CsnArr;",                      (SUBR) csnarray_argmax,                      NULL,                                   (SUBR) csnarray_reduction_deinit,        NULL, 0 },
    { "csnargmax",             S(CSN_REDUCTION),              0, ":CsnArr;",                 ":CsnArr;i",                     (SUBR) csnarray_argmax,                      NULL,                                   (SUBR) csnarray_reduction_deinit,        NULL, 0 },
    { "csnargmax.k",           S(CSN_REDUCTION),              0, ":CsnArr;",                 ":CsnArr;P",                     (SUBR) csnarray_argmax_k_init,               (SUBR) csnarray_argmax_k,               (SUBR) csnarray_reduction_deinit,        NULL, 0 },
    { "csnargmax.k",           S(CSN_REDUCTION),              0, ":CsnArr;",                 ":CsnArr;kk",                    (SUBR) csnarray_argmax_k_init,               (SUBR) csnarray_argmax_k,               (SUBR) csnarray_reduction_deinit,        NULL, 0 },
    { "csnfloor",              S(CSN_UNARYOP),                0, ":CsnArr;",                 ":CsnArr;",                      (SUBR) csnarray_floor,                       NULL,                                   (SUBR) csnarray_opunary_deinit,          NULL, 0 },
    { "csnceil",               S(CSN_UNARYOP),                0, ":CsnArr;",                 ":CsnArr;",                      (SUBR) csnarray_ceil,                        NULL,                                   (SUBR) csnarray_opunary_deinit,          NULL, 0 },
    { "csnround",              S(CSN_UNARYOP),                0, ":CsnArr;",                 ":CsnArr;",                      (SUBR) csnarray_round,                       NULL,                                   (SUBR) csnarray_opunary_deinit,          NULL, 0 },
    { "csnfloor.k",            S(CSN_UNARYOP),                0, ":CsnArr;",                 ":CsnArr;k",                     (SUBR) csnarray_floor,                       (SUBR) csnarray_floor_k,                (SUBR) csnarray_opunary_deinit,          NULL, 0 },
    { "csnceil.k",             S(CSN_UNARYOP),                0, ":CsnArr;",                 ":CsnArr;k",                     (SUBR) csnarray_ceil,                        (SUBR) csnarray_ceil_k,                 (SUBR) csnarray_opunary_deinit,          NULL, 0 },
    { "csnround.k",            S(CSN_UNARYOP),                0, ":CsnArr;",                 ":CsnArr;k",                     (SUBR) csnarray_round,                       (SUBR) csnarray_round_k,                (SUBR) csnarray_opunary_deinit,          NULL, 0 },
    { "csnproject",            S(CSN_BINOP_HH),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;",              (SUBR) csnarray_project,                     NULL,                                   (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csnproject.k",          S(CSN_BINOP_HH),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;k",             (SUBR) csnarray_project_k_init,              (SUBR) csnarray_project_k,              (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csnreject",             S(CSN_BINOP_HH),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;",              (SUBR) csnarray_reject,                      NULL,                                   (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csnreject.k",           S(CSN_BINOP_HH),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;k",             (SUBR) csnarray_reject_k_init,               (SUBR) csnarray_reject_k,               (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csncross",              S(CSN_BINOP_HH),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;",              (SUBR) csnarray_cross,                       NULL,                                   (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csncross.k",            S(CSN_BINOP_HH),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;k",             (SUBR) csnarray_cross_k_init,                (SUBR) csnarray_cross_k,                (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csngrad",               S(CSN_UNARYOP_AX),             0, ":CsnArr;",                 ":CsnArr;",                      (SUBR) csnarray_gradient,                    NULL,                                   (SUBR) csnarray_opunary_ax_deinit,       NULL, 0 },
    { "csngrad",               S(CSN_UNARYOP_AX),             0, ":CsnArr;",                 ":CsnArr;i",                     (SUBR) csnarray_gradient,                    NULL,                                   (SUBR) csnarray_opunary_ax_deinit,       NULL, 0 },
    { "csngrad.k",             S(CSN_UNARYOP_AX),             0, ":CsnArr;",                 ":CsnArr;P",                     (SUBR) csnarray_gradient_k_init,             (SUBR) csnarray_gradient_k,             (SUBR) csnarray_opunary_ax_deinit,       NULL, 0 },
    { "csngrad.k",             S(CSN_UNARYOP_AX),             0, ":CsnArr;",                 ":CsnArr;kk",                    (SUBR) csnarray_gradient_k_init,             (SUBR) csnarray_gradient_k,             (SUBR) csnarray_opunary_ax_deinit,       NULL, 0 },
    { "csnmovmedian",          S(CSN_MOVSTATS),               0, ":CsnArr;",                 ":CsnArr;i",                     (SUBR) csnarray_movmedian,                   NULL,                                   (SUBR) csnarray_movstats_deinit,         NULL, 0 },
    { "csnmovmedian",          S(CSN_MOVSTATS),               0, ":CsnArr;",                 ":CsnArr;ii",                    (SUBR) csnarray_movmedian,                   NULL,                                   (SUBR) csnarray_movstats_deinit,         NULL, 0 },
    { "csnmovmedian.k",        S(CSN_MOVSTATS),               0, ":CsnArr;",                 ":CsnArr;kP",                    (SUBR) csnarray_movmedian_k_init,            (SUBR) csnarray_movmedian_k,            (SUBR) csnarray_movstats_deinit,         NULL, 0 },
    { "csnmovmedian.k",        S(CSN_MOVSTATS),               0, ":CsnArr;",                 ":CsnArr;kkk",                   (SUBR) csnarray_movmedian_k_init,            (SUBR) csnarray_movmedian_k,            (SUBR) csnarray_movstats_deinit,         NULL, 0 },
    { "csnmovmedian.in",       S(CSN_MOVSTATS_IN),            0, "",                         ":CsnArr;i",                     (SUBR) csnarray_movmedian_in,                NULL,                                   NULL,                                    NULL, 0 },
    { "csnmovmedian.in",       S(CSN_MOVSTATS_IN),            0, "",                         ":CsnArr;ii",                    (SUBR) csnarray_movmedian_in,                NULL,                                   NULL,                                    NULL, 0 },
    { "csnmovmedian.in.k",     S(CSN_MOVSTATS_IN),            0, "",                         ":CsnArr;kP",                    (SUBR) csnarray_movmedian_in_k_init,         (SUBR) csnarray_movmedian_in_k,         (SUBR) csnarray_movstats_in_k_deinit,    NULL, 0 },
    { "csnmovmedian.in.k",     S(CSN_MOVSTATS_IN),            0, "",                         ":CsnArr;kkk",                   (SUBR) csnarray_movmedian_in_k_init,         (SUBR) csnarray_movmedian_in_k,         (SUBR) csnarray_movstats_in_k_deinit,    NULL, 0 },
    { "csnmovmin",             S(CSN_MOVSTATS),               0, ":CsnArr;",                 ":CsnArr;i",                     (SUBR) csnarray_movmin,                      NULL,                                   (SUBR) csnarray_movstats_deinit,         NULL, 0 },
    { "csnmovmin",             S(CSN_MOVSTATS),               0, ":CsnArr;",                 ":CsnArr;ii",                    (SUBR) csnarray_movmin,                      NULL,                                   (SUBR) csnarray_movstats_deinit,         NULL, 0 },
    { "csnmovmin.k",           S(CSN_MOVSTATS),               0, ":CsnArr;",                 ":CsnArr;kP",                    (SUBR) csnarray_movmin_k_init,               (SUBR) csnarray_movmin_k,               (SUBR) csnarray_movstats_deinit,         NULL, 0 },
    { "csnmovmin.k",           S(CSN_MOVSTATS),               0, ":CsnArr;",                 ":CsnArr;kkk",                   (SUBR) csnarray_movmin_k_init,               (SUBR) csnarray_movmin_k,               (SUBR) csnarray_movstats_deinit,         NULL, 0 },
    { "csnmovmin.in",          S(CSN_MOVSTATS_IN),            0, "",                         ":CsnArr;i",                     (SUBR) csnarray_movmin_in,                   NULL,                                   NULL,                                    NULL, 0 },
    { "csnmovmin.in",          S(CSN_MOVSTATS_IN),            0, "",                         ":CsnArr;ii",                    (SUBR) csnarray_movmin_in,                   NULL,                                   NULL,                                    NULL, 0 },
    { "csnmovmin.in.k",        S(CSN_MOVSTATS_IN),            0, "",                         ":CsnArr;kP",                    (SUBR) csnarray_movmin_in_k_init,            (SUBR) csnarray_movmin_in_k,            (SUBR) csnarray_movstats_in_k_deinit,    NULL, 0 },
    { "csnmovmin.in.k",        S(CSN_MOVSTATS_IN),            0, "",                         ":CsnArr;kkk",                   (SUBR) csnarray_movmin_in_k_init,            (SUBR) csnarray_movmin_in_k,            (SUBR) csnarray_movstats_in_k_deinit,    NULL, 0 },
    { "csnmovmax",             S(CSN_MOVSTATS),               0, ":CsnArr;",                 ":CsnArr;i",                     (SUBR) csnarray_movmax,                      NULL,                                   (SUBR) csnarray_movstats_deinit,         NULL, 0 },
    { "csnmovmax",             S(CSN_MOVSTATS),               0, ":CsnArr;",                 ":CsnArr;ii",                    (SUBR) csnarray_movmax,                      NULL,                                   (SUBR) csnarray_movstats_deinit,         NULL, 0 },
    { "csnmovmax.k",           S(CSN_MOVSTATS),               0, ":CsnArr;",                 ":CsnArr;kP",                    (SUBR) csnarray_movmax_k_init,               (SUBR) csnarray_movmax_k,               (SUBR) csnarray_movstats_deinit,         NULL, 0 },
    { "csnmovmax.k",           S(CSN_MOVSTATS),               0, ":CsnArr;",                 ":CsnArr;kkk",                   (SUBR) csnarray_movmax_k_init,               (SUBR) csnarray_movmax_k,               (SUBR) csnarray_movstats_deinit,         NULL, 0 },
    { "csnmovmax.in",          S(CSN_MOVSTATS_IN),            0, "",                         ":CsnArr;i",                     (SUBR) csnarray_movmax_in,                   NULL,                                   NULL,                                    NULL, 0 },
    { "csnmovmax.in",          S(CSN_MOVSTATS_IN),            0, "",                         ":CsnArr;ii",                    (SUBR) csnarray_movmax_in,                   NULL,                                   NULL,                                    NULL, 0 },
    { "csnmovmax.in.k",        S(CSN_MOVSTATS_IN),            0, "",                         ":CsnArr;kP",                    (SUBR) csnarray_movmax_in_k_init,            (SUBR) csnarray_movmax_in_k,            (SUBR) csnarray_movstats_in_k_deinit,    NULL, 0 },
    { "csnmovmax.in.k",        S(CSN_MOVSTATS_IN),            0, "",                         ":CsnArr;kkk",                   (SUBR) csnarray_movmax_in_k_init,            (SUBR) csnarray_movmax_in_k,            (SUBR) csnarray_movstats_in_k_deinit,    NULL, 0 },
    { "csnsort",               S(CSN_UNARYOP_AX),             0, ":CsnArr;",                 ":CsnArr;",                      (SUBR) csnarray_sort,                        NULL,                                   (SUBR) csnarray_opunary_ax_deinit,       NULL, 0 },
    { "csnsort",               S(CSN_UNARYOP_AX),             0, ":CsnArr;",                 ":CsnArr;i",                     (SUBR) csnarray_sort,                        NULL,                                   (SUBR) csnarray_opunary_ax_deinit,       NULL, 0 },
    { "csnsort.k",             S(CSN_UNARYOP_AX),             0, ":CsnArr;",                 ":CsnArr;P",                     (SUBR) csnarray_sort_k_init,                 (SUBR) csnarray_sort_k,                 (SUBR) csnarray_opunary_ax_deinit,       NULL, 0 },
    { "csnsort.k",             S(CSN_UNARYOP_AX),             0, ":CsnArr;",                 ":CsnArr;kk",                    (SUBR) csnarray_sort_k_init,                 (SUBR) csnarray_sort_k,                 (SUBR) csnarray_opunary_ax_deinit,       NULL, 0 },
    { "csnsort.in",            S(CSN_UNARYOP_AX_IN),          0, "",                         ":CsnArr;",                      (SUBR) csnarray_sort_in,                     NULL,                                   NULL,                                    NULL, 0 },
    { "csnsort.in",            S(CSN_UNARYOP_AX_IN),          0, "",                         ":CsnArr;i",                     (SUBR) csnarray_sort_in,                     NULL,                                   NULL,                                    NULL, 0 },
    { "csnsort.in.k",          S(CSN_UNARYOP_AX_IN),          0, "",                         ":CsnArr;P",                     (SUBR) csnarray_sort_in_k_init,              (SUBR) csnarray_sort_in_k,              (SUBR) opunary_ax_in_k_deinit,           NULL, 0 },
    { "csnsort.in.k",          S(CSN_UNARYOP_AX_IN),          0, "",                         ":CsnArr;kk",                    (SUBR) csnarray_sort_in_k_init,              (SUBR) csnarray_sort_in_k,              (SUBR) opunary_ax_in_k_deinit,           NULL, 0 },
    { "csnargsort",            S(CSN_UNARYOP_AX),             0, ":CsnArr;",                 ":CsnArr;",                      (SUBR) csnarray_argsort,                     NULL,                                   (SUBR) csnarray_opunary_ax_deinit,       NULL, 0 },
    { "csnargsort",            S(CSN_UNARYOP_AX),             0, ":CsnArr;",                 ":CsnArr;i",                     (SUBR) csnarray_argsort,                     NULL,                                   (SUBR) csnarray_opunary_ax_deinit,       NULL, 0 },
    { "csnargsort.k",          S(CSN_UNARYOP_AX),             0, ":CsnArr;",                 ":CsnArr;P",                     (SUBR) csnarray_argsort_k_init,              (SUBR) csnarray_argsort_k,              (SUBR) csnarray_opunary_ax_deinit,       NULL, 0 },
    { "csnargsort.k",          S(CSN_UNARYOP_AX),             0, ":CsnArr;",                 ":CsnArr;kk",                    (SUBR) csnarray_argsort_k_init,              (SUBR) csnarray_argsort_k,              (SUBR) csnarray_opunary_ax_deinit,       NULL, 0 },
    { "csnpercentile",         S(CSN_PERCQUANT),              0, "i",                        ":CsnArr;i",                     (SUBR) csnarray_percentile_scalar,           NULL,                                   NULL,                                    NULL, 0 },
    { "csnpercentile.ax",      S(CSN_PERCQUANT_AX),           0, ":CsnArr;",                 ":CsnArr;ii",                    (SUBR) csnarray_percentile,                  NULL,                                   (SUBR) csnarray_perquant_deinit,         NULL, 0 },
    { "csnquantile",           S(CSN_PERCQUANT),              0, "i",                        ":CsnArr;i",                     (SUBR) csnarray_quantile_scalar,             NULL,                                   NULL,                                    NULL, 0 },
    { "csnquantile.ax",        S(CSN_PERCQUANT_AX),           0, ":CsnArr;",                 ":CsnArr;ii",                    (SUBR) csnarray_quantile,                    NULL,                                   (SUBR) csnarray_perquant_deinit,         NULL, 0 },
    { "csnpercentile.k",       S(CSN_PERCQUANT),              0, "k",                        ":CsnArr;kP",                    (SUBR) csnarray_perquant_scalar_k_init,      (SUBR) csnarray_percentile_scalar_k,    (SUBR) csnarray_perquant_s_k_deinit,     NULL, 0 },
    { "csnpercentile.ax.k",    S(CSN_PERCQUANT_AX),           0, ":CsnArr;",                 ":CsnArr;kkk",                   (SUBR) csnarray_perquant_k_init,             (SUBR) csnarray_percentile_k,           (SUBR) csnarray_perquant_deinit,         NULL, 0 },
    { "csnquantile.k",         S(CSN_PERCQUANT),              0, "k",                        ":CsnArr;kP",                    (SUBR) csnarray_perquant_scalar_k_init,      (SUBR) csnarray_quantile_scalar_k,      (SUBR) csnarray_perquant_s_k_deinit,     NULL, 0 },
    { "csnquantile.ax.k",      S(CSN_PERCQUANT_AX),           0, ":CsnArr;",                 ":CsnArr;kkk",                   (SUBR) csnarray_perquant_k_init,             (SUBR) csnarray_quantile_k,             (SUBR) csnarray_perquant_deinit,         NULL, 0 },
    { "csnlogicand.hh",        S(CSN_BINOP_HH),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;",              (SUBR) csnarray_logical_and_hh,              NULL,                                   (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csnlogicand.hh.k",      S(CSN_BINOP_HH),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;k",             (SUBR) csnarray_logical_and_hh_k_init,       (SUBR) csnarray_logical_and_hh_k,       (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csnlogicor.hh",         S(CSN_BINOP_HH),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;",              (SUBR) csnarray_logical_or_hh,               NULL,                                   (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csnlogicor.hh.k",       S(CSN_BINOP_HH),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;k",             (SUBR) csnarray_logical_or_hh_k_init,        (SUBR) csnarray_logical_or_hh_k,        (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csnlogicand.hs",        S(CSN_BINOP_HS),               0, ":CsnArr;",                 ":CsnArr;i",                     (SUBR) csnarray_logical_and_hs,              NULL,                                   (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csnlogicor.hs",         S(CSN_BINOP_HS),               0, ":CsnArr;",                 ":CsnArr;i",                     (SUBR) csnarray_logical_or_hs,               NULL,                                   (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csnlogicand.sh",        S(CSN_BINOP_SH),               0, ":CsnArr;",                 "i:CsnArr;",                     (SUBR) csnarray_logical_and_sh,              NULL,                                   (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csnlogicor.sh",         S(CSN_BINOP_SH),               0, ":CsnArr;",                 "i:CsnArr;",                     (SUBR) csnarray_logical_or_sh,               NULL,                                   (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csnlogicand.hs.k",      S(CSN_BINOP_HS),               0, ":CsnArr;",                 ":CsnArr;kP",                    (SUBR) csnarray_logical_and_hs_k_init,       (SUBR) csnarray_logical_and_hs_k,       (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csnlogicor.hs.k",       S(CSN_BINOP_HS),               0, ":CsnArr;",                 ":CsnArr;kP",                    (SUBR) csnarray_logical_or_hs_k_init,        (SUBR) csnarray_logical_or_hs_k,        (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csnlogicand.sh.k",      S(CSN_BINOP_SH),               0, ":CsnArr;",                 "k:CsnArr;P",                    (SUBR) csnarray_logical_and_sh_k_init,       (SUBR) csnarray_logical_and_sh_k,       (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csnlogicor.sh.k",       S(CSN_BINOP_SH),               0, ":CsnArr;",                 "k:CsnArr;P",                    (SUBR) csnarray_logical_or_sh_k_init,        (SUBR) csnarray_logical_or_sh_k,        (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csnlogicnot",           S(CSN_UNARYOP),                0, ":CsnArr;",                 ":CsnArr;",                      (SUBR) csnarray_logical_not,                 NULL,                                   (SUBR) csnarray_opunary_deinit,          NULL, 0 },
    { "csnlogicnot.k",         S(CSN_UNARYOP),                0, ":CsnArr;",                 ":CsnArr;k",                     (SUBR) csnarray_logical_not,                 (SUBR) csnarray_logical_not_k,          (SUBR) csnarray_opunary_deinit,          NULL, 0 },
    { "csnhypot",              S(CSN_BINOP_HH),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;",              (SUBR) csnarray_hypot_hh,                    NULL,                                   (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csnhypot.k",            S(CSN_BINOP_HH),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;k",             (SUBR) csnarray_hypot_hh_k_init,             (SUBR) csnarray_hypot_hh_k,             (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csnhypot.hs",           S(CSN_BINOP_HS),               0, ":CsnArr;",                 ":CsnArr;i",                     (SUBR) csnarray_hypot_hs,                    NULL,                                   (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csnhypot.hs.k",         S(CSN_BINOP_HS),               0, ":CsnArr;",                 ":CsnArr;kP",                    (SUBR) csnarray_hypot_hs_k_init,             (SUBR) csnarray_hypot_hs_k,             (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csndegtorad",           S(CSN_UNARYOP),                0, ":CsnArr;",                 ":CsnArr;",                      (SUBR) csnarray_degtorad,                    NULL,                                   (SUBR) csnarray_opunary_deinit,          NULL, 0 },
    { "csndegtorad.k",         S(CSN_UNARYOP),                0, ":CsnArr;",                 ":CsnArr;k",                     (SUBR) csnarray_degtorad,                    (SUBR) csnarray_degtorad_k,             (SUBR) csnarray_opunary_deinit,          NULL, 0 },
    { "csndegtorad.in",        S(CSN_UNARYOP_IN),             0, "",                         ":CsnArr;",                      (SUBR) csnarray_degtorad_in,                 NULL,                                   NULL,                                    NULL, 0 },
    { "csndegtorad.in.k",      S(CSN_UNARYOP_IN),             0, "",                         ":CsnArr;k",                     (SUBR) csnarray_unaryop_in_k_init,           (SUBR) csnarray_degtorad_in_k,          NULL,                                    NULL, 0 },
    { "csnradtodeg",           S(CSN_UNARYOP),                0, ":CsnArr;",                 ":CsnArr;",                      (SUBR) csnarray_radtodeg,                    NULL,                                   (SUBR) csnarray_opunary_deinit,          NULL, 0 },
    { "csnradtodeg.k",         S(CSN_UNARYOP),                0, ":CsnArr;",                 ":CsnArr;k",                     (SUBR) csnarray_radtodeg,                    (SUBR) csnarray_radtodeg_k,             (SUBR) csnarray_opunary_deinit,          NULL, 0 },
    { "csnradtodeg.in",        S(CSN_UNARYOP_IN),             0, "",                         ":CsnArr;",                      (SUBR) csnarray_radtodeg_in,                 NULL,                                   NULL,                                    NULL, 0 },
    { "csnradtodeg.in.k",      S(CSN_UNARYOP_IN),             0, "",                         ":CsnArr;k",                     (SUBR) csnarray_unaryop_in_k_init,           (SUBR) csnarray_radtodeg_in_k,          NULL,                                    NULL, 0 },
    { "csnhanning",            S(CSN_WINDOW),                 0, ":CsnArr;",                 "i",                             (SUBR) csnarray_hanning,                     NULL,                                   (SUBR) csnarray_window_deinit,           NULL, 0 },
    { "csnhanning.k",          S(CSN_WINDOW),                 0, ":CsnArr;",                 "k",                             (SUBR) csnarray_window_function_k_init,      (SUBR) csnarray_hanning_k,              (SUBR) csnarray_window_deinit,           NULL, 0 },
    { "csnhamming",            S(CSN_WINDOW),                 0, ":CsnArr;",                 "i",                             (SUBR) csnarray_hamming,                     NULL,                                   (SUBR) csnarray_window_deinit,           NULL, 0 },
    { "csnhamming.k",          S(CSN_WINDOW),                 0, ":CsnArr;",                 "k",                             (SUBR) csnarray_window_function_k_init,      (SUBR) csnarray_hamming_k,              (SUBR) csnarray_window_deinit,           NULL, 0 },
    { "csnbartlett",           S(CSN_WINDOW),                 0, ":CsnArr;",                 "i",                             (SUBR) csnarray_bartlett,                    NULL,                                   (SUBR) csnarray_window_deinit,           NULL, 0 },
    { "csnbartlett.k",         S(CSN_WINDOW),                 0, ":CsnArr;",                 "k",                             (SUBR) csnarray_window_function_k_init,      (SUBR) csnarray_bartlett_k,             (SUBR) csnarray_window_deinit,           NULL, 0 },
    { "csnblackman",           S(CSN_WINDOW),                 0, ":CsnArr;",                 "i",                             (SUBR) csnarray_blackman,                    NULL,                                   (SUBR) csnarray_window_deinit,           NULL, 0 },
    { "csnblackman.k",         S(CSN_WINDOW),                 0, ":CsnArr;",                 "k",                             (SUBR) csnarray_window_function_k_init,      (SUBR) csnarray_blackman_k,             (SUBR) csnarray_window_deinit,           NULL, 0 },
    { "csnkaiser",             S(CSN_WINDOW),                 0, ":CsnArr;",                 "ii",                            (SUBR) csnarray_kaiser,                      NULL,                                   (SUBR) csnarray_window_deinit,           NULL, 0 },
    { "csnkaiser.k",           S(CSN_WINDOW),                 0, ":CsnArr;",                 "kk",                            (SUBR) csnarray_window_function_k_init,      (SUBR) csnarray_kaiser_k,               (SUBR) csnarray_window_deinit,           NULL, 0 },
    { "csnkaiser.ik",          S(CSN_WINDOW),                 0, ":CsnArr;",                 "ik",                            (SUBR) csnarray_window_function_k_init,      (SUBR) csnarray_kaiser_k,               (SUBR) csnarray_window_deinit,           NULL, 0 },
    { "csndivmod.hh",          S(CSN_DIVMOD_HH),              0, ":CsnArr;:CsnArr;",         ":CsnArr;:CsnArr;",              (SUBR) csnarray_divmod_hh,                   NULL,                                   (SUBR) csnarray_divmod_deinit,           NULL, 0 },
    { "csndivmod.hs",          S(CSN_DIVMOD_HS),              0, ":CsnArr;:CsnArr;",         ":CsnArr;i",                     (SUBR) csnarray_divmod_hs,                   NULL,                                   (SUBR) csnarray_divmod_deinit,           NULL, 0 },
    { "csndivmod.sh",          S(CSN_DIVMOD_SH),              0, ":CsnArr;:CsnArr;",         "i:CsnArr;",                     (SUBR) csnarray_divmod_sh,                   NULL,                                   (SUBR) csnarray_divmod_deinit,           NULL, 0 },
    { "csndivmod.hh.k",        S(CSN_DIVMOD_HH),              0, ":CsnArr;:CsnArr;",         ":CsnArr;:CsnArr;P",             (SUBR) csnarray_divmod_hh_k_init,            (SUBR) csnarray_divmod_hh_k,            (SUBR) csnarray_divmod_deinit,           NULL, 0 },
    { "csndivmod.hs.k",        S(CSN_DIVMOD_HS),              0, ":CsnArr;:CsnArr;",         ":CsnArr;k",                     (SUBR) csnarray_divmod_hs_k_init,            (SUBR) csnarray_divmod_hs_k,            (SUBR) csnarray_divmod_deinit,           NULL, 0 },
    { "csndivmod.sh.k",        S(CSN_DIVMOD_SH),              0, ":CsnArr;:CsnArr;",         "k:CsnArr;",                     (SUBR) csnarray_divmod_sh_k_init,            (SUBR) csnarray_divmod_sh_k,            (SUBR) csnarray_divmod_deinit,           NULL, 0 },
    { "csnfromftable",         S(CSN_FROM_FTABLE),            0, ":CsnArr;",                 "i",                             (SUBR) from_ftable_to_csnarray,              NULL,                                   (SUBR) csnarray_from_ftable_deinit,      NULL, 0 },
    { "csntoftable",           S(CSN_TO_FTABLE),              0, "",                         ":CsnArr;io",                    (SUBR) from_csnarray_to_ftable,              NULL,                                   NULL,                                    NULL, 0 },
    { "csninterp",             S(CSN_REMAP),                  0, ":CsnArr;",                 ":CsnArr;:CsnArr;:CsnArr;iioP",  (SUBR) csnarray_remap_k_init,                (SUBR) csnarray_remap_k,                (SUBR) csnarray_remap_deinit,            NULL, 0 },
    { "csninterp",             S(CSN_REMAP),                  0, ":CsnArr;",                 ":CsnArr;:CsnArr;:CsnArr;iiokk", (SUBR) csnarray_remap_k_init,                (SUBR) csnarray_remap_k,                (SUBR) csnarray_remap_deinit,            NULL, 0 },
    { "csninterp.s",           S(CSN_REMAP_SCALAR),           0, "i",                        "k:CsnArr;:CsnArr;iio",          (SUBR) csnarray_remap_scalar,                NULL,                                   NULL,                                    NULL, 0 },
    { "csninterp.s.k",         S(CSN_REMAP_SCALAR),           0, "k",                        "k:CsnArr;:CsnArr;iioP",         (SUBR) csnarray_remap_scalar_k_init,         (SUBR) csnarray_remap_scalar_k,         NULL,                                    NULL, 0 },
    { "csnresample",           S(CSN_RESAMPLE),               0, ":CsnArr;",                 ":CsnArr;iiio",                  (SUBR) csnarray_resample,                    NULL,                                   (SUBR) csnarray_resample_deinit,         NULL, 0 },
    { "csnresample",           S(CSN_RESAMPLE),               0, ":CsnArr;",                 ":CsnArr;iiioi",                 (SUBR) csnarray_resample,                    NULL,                                   (SUBR) csnarray_resample_deinit,         NULL, 0 },
    { "csnresample.k",         S(CSN_RESAMPLE),               0, ":CsnArr;",                 ":CsnArr;kiioP",                 (SUBR) csnarray_resample_k_init,             (SUBR) csnarray_resample_k,             (SUBR) csnarray_resample_deinit,         NULL, 0 },
    { "csnresample.k",         S(CSN_RESAMPLE),               0, ":CsnArr;",                 ":CsnArr;kiiokk",                (SUBR) csnarray_resample_k_init,             (SUBR) csnarray_resample_k,             (SUBR) csnarray_resample_deinit,         NULL, 0 },
    { "csnwhere.hh",           S(CSN_WHERE_HH),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;:CsnArr;",      (SUBR) csnarray_where_hh,                    NULL,                                   (SUBR) csnarray_where_deinit,            NULL, 0 },
    { "csnwhere.hs",           S(CSN_WHERE_HS),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;i",             (SUBR) csnarray_where_hs,                    NULL,                                   (SUBR) csnarray_where_deinit,            NULL, 0 },
    { "csnwhere.hh.k",         S(CSN_WHERE_HH),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;:CsnArr;P",     (SUBR) csnarray_where_hh_k_init,             (SUBR) csnarray_where_hh_k,             (SUBR) csnarray_where_deinit,            NULL, 0 },
    { "csnwhere.hs.k",         S(CSN_WHERE_HS),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;kP",            (SUBR) csnarray_where_hs_k_init,             (SUBR) csnarray_where_hs_k,             (SUBR) csnarray_where_deinit,            NULL, 0 },
    { "csnputmask.hh",         S(CSN_WHERE_HH_IN),            0, "",                         ":CsnArr;:CsnArr;:CsnArr;",      (SUBR) csnarray_where_in_hh,                 NULL,                                   NULL,                                    NULL, 0 },
    { "csnputmask.hs",         S(CSN_WHERE_HS_IN),            0, "",                         ":CsnArr;:CsnArr;i",             (SUBR) csnarray_where_in_hs,                 NULL,                                   NULL,                                    NULL, 0 },
    { "csnputmask.hh.k",       S(CSN_WHERE_HH_IN),            0, "",                         ":CsnArr;:CsnArr;:CsnArr;P",     (SUBR) csnarray_where_in_hh_k_init,          (SUBR) csnarray_where_in_hh_k,          NULL,                                    NULL, 0 },
    { "csnputmask.hs.k",       S(CSN_WHERE_HS_IN),            0, "",                         ":CsnArr;:CsnArr;kP",            (SUBR) csnarray_where_in_hs_k_init,          (SUBR) csnarray_where_in_hs_k,          NULL,                                    NULL, 0 },
    { "csnminimum.hh",         S(CSN_BINOP_HH),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;",              (SUBR) csnarray_minimum_hh,                  NULL,                                   (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csnminimum.hh.k",       S(CSN_BINOP_HH),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;P",             (SUBR) csnarray_minimum_hh_k_init,           (SUBR) csnarray_minimum_hh_k,           (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csnminimum.hs",         S(CSN_BINOP_HS),               0, ":CsnArr;",                 ":CsnArr;i",                     (SUBR) csnarray_minimum_hs,                  NULL,                                   (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csnminimum.hs.k",       S(CSN_BINOP_HS),               0, ":CsnArr;",                 ":CsnArr;kP",                    (SUBR) csnarray_minimum_hs_k_init,           (SUBR) csnarray_minimum_hs_k,           (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csnmaximum.hh",         S(CSN_BINOP_HH),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;",              (SUBR) csnarray_maximum_hh,                  NULL,                                   (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csnmaximum.hh.k",       S(CSN_BINOP_HH),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;P",             (SUBR) csnarray_maximum_hh_k_init,           (SUBR) csnarray_maximum_hh_k,           (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csnmaximum.hs",         S(CSN_BINOP_HS),               0, ":CsnArr;",                 ":CsnArr;i",                     (SUBR) csnarray_maximum_hs,                  NULL,                                   (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csnmaximum.hs.k",       S(CSN_BINOP_HS),               0, ":CsnArr;",                 ":CsnArr;kP",                    (SUBR) csnarray_maximum_hs_k_init,           (SUBR) csnarray_maximum_hs_k,           (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csnatan2.hh",           S(CSN_BINOP_HH),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;",              (SUBR) csnarray_atan2_hh,                    NULL,                                   (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csnatan2.hh.k",         S(CSN_BINOP_HH),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;P",             (SUBR) csnarray_atan2_hh_k_init,             (SUBR) csnarray_atan2_hh_k,             (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csnatan2.hs",           S(CSN_BINOP_HS),               0, ":CsnArr;",                 ":CsnArr;i",                     (SUBR) csnarray_atan2_hs,                    NULL,                                   (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csnatan2.hs.k",         S(CSN_BINOP_HS),               0, ":CsnArr;",                 ":CsnArr;kP",                    (SUBR) csnarray_atan2_hs_k_init,             (SUBR) csnarray_atan2_hs_k,             (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csnatan2.sh",           S(CSN_BINOP_SH),               0, ":CsnArr;",                 "i:CsnArr;",                     (SUBR) csnarray_atan2_sh,                    NULL,                                   (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csnatan2.sh.k",         S(CSN_BINOP_SH),               0, ":CsnArr;",                 "k:CsnArr;P",                    (SUBR) csnarray_atan2_sh_k_init,             (SUBR) csnarray_atan2_sh_k,             (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csnrms",                S(CSN_REDUCTION_SCALAR),       0, "i",                        ":CsnArr;",                      (SUBR) csnarray_rms_all,                     NULL,                                   NULL,                                    NULL, 0 },
    { "csnrms.ax",             S(CSN_REDUCTION),              0, ":CsnArr;",                 ":CsnArr;i",                     (SUBR) csnarray_rms,                         NULL,                                   (SUBR) csnarray_reduction_deinit,        NULL, 0 },
    { "csnrms.k",              S(CSN_REDUCTION_SCALAR),       0, "k",                        ":CsnArr;P",                     (SUBR) csnarray_rms_all_k_init,              (SUBR) csnarray_rms_all_k,              NULL,                                    NULL, 0 },
    { "csnrms.ax.k",           S(CSN_REDUCTION),              0, ":CsnArr;",                 ":CsnArr;kk",                    (SUBR) csnarray_rms_k_init,                  (SUBR) csnarray_rms_k,                  (SUBR) csnarray_reduction_deinit,        NULL, 0 },
    { "csnisnan",              S(CSN_UNARYOP),                0, ":CsnArr;",                 ":CsnArr;",                      (SUBR) csnarray_isnan,                       NULL,                                   (SUBR) csnarray_opunary_deinit,          NULL, 0 },
    { "csnisnan.k",            S(CSN_UNARYOP),                0, ":CsnArr;",                 ":CsnArr;P",                     (SUBR) csnarray_isnan,                       (SUBR) csnarray_isnan_k,                (SUBR) csnarray_opunary_deinit,          NULL, 0 },
    { "csnisinf",              S(CSN_UNARYOP),                0, ":CsnArr;",                 ":CsnArr;",                      (SUBR) csnarray_isinf,                       NULL,                                   (SUBR) csnarray_opunary_deinit,          NULL, 0 },
    { "csnisinf.k",            S(CSN_UNARYOP),                0, ":CsnArr;",                 ":CsnArr;P",                     (SUBR) csnarray_isinf,                       (SUBR) csnarray_isinf_k,                (SUBR) csnarray_opunary_deinit,          NULL, 0 },
    { "csnisfin",              S(CSN_UNARYOP),                0, ":CsnArr;",                 ":CsnArr;",                      (SUBR) csnarray_isfin,                       NULL,                                   (SUBR) csnarray_opunary_deinit,          NULL, 0 },
    { "csnisfin.k",            S(CSN_UNARYOP),                0, ":CsnArr;",                 ":CsnArr;P",                     (SUBR) csnarray_isfin,                       (SUBR) csnarray_isfin_k,                (SUBR) csnarray_opunary_deinit,          NULL, 0 },
    { "csnsavgol",             S(CSN_SAVGOL_MATRIX),          0, ":CsnArr;",                 "iip",                           (SUBR) csnarray_savgol_mat,                  NULL,                                   (SUBR) csnarray_savgol_mat_deinit,       NULL, 0 },
    { "csnmedfilt1d",          S(CSN_MEDFILT),                0, ":CsnArr;",                 ":CsnArr;i",                     (SUBR) csnarray_medfilt1d,                   NULL,                                   (SUBR) csnarray_medfilt_deinit,          NULL, 0 },
    { "csnmedfilt1d",          S(CSN_MEDFILT),                0, ":CsnArr;",                 ":CsnArr;ii",                    (SUBR) csnarray_medfilt1d,                   NULL,                                   (SUBR) csnarray_medfilt_deinit,          NULL, 0 },
    { "csnmedfilt1d.k",        S(CSN_MEDFILT),                0, ":CsnArr;",                 ":CsnArr;iP",                    (SUBR) csnarray_medfilt1d_k_init,            (SUBR) csnarray_medfilt1d_k,            (SUBR) csnarray_medfilt_deinit,          NULL, 0 },
    { "csnmedfilt1d.k",        S(CSN_MEDFILT),                0, ":CsnArr;",                 ":CsnArr;iki",                   (SUBR) csnarray_medfilt1d_k_init,            (SUBR) csnarray_medfilt1d_k,            (SUBR) csnarray_medfilt_deinit,          NULL, 0 },
    { "csnmedfilt1d.in",       S(CSN_MEDFILT_IN),             0, "",                         ":CsnArr;i",                     (SUBR) csnarray_medfilt1d_in,                NULL,                                   (SUBR) csnarray_medfilt_in_deinit,       NULL, 0 },
    { "csnmedfilt1d.in",       S(CSN_MEDFILT_IN),             0, "",                         ":CsnArr;ii",                    (SUBR) csnarray_medfilt1d_in,                NULL,                                   (SUBR) csnarray_medfilt_in_deinit,       NULL, 0 },
    { "csnmedfilt1d.in.k",     S(CSN_MEDFILT_IN),             0, "",                         ":CsnArr;iP",                    (SUBR) csnarray_medfilt1d_in_k_init,         (SUBR) csnarray_medfilt1d_in_k,         (SUBR) csnarray_medfilt_in_deinit,       NULL, 0 },
    { "csnmedfilt1d.in.k",     S(CSN_MEDFILT_IN),             0, "",                         ":CsnArr;iki",                   (SUBR) csnarray_medfilt1d_in_k_init,         (SUBR) csnarray_medfilt1d_in_k,         (SUBR) csnarray_medfilt_in_deinit,       NULL, 0 },
    { "csnmedfilt",            S(CSN_MEDFILT_ND),             0, ":CsnArr;",                 ":CsnArr;i",                     (SUBR) csnarray_medfilt,                     NULL,                                   (SUBR) csnarray_medfilt_nd_deinit,       NULL, 0 },
    { "csnmedfilt.k",          S(CSN_MEDFILT_ND),             0, ":CsnArr;",                 ":CsnArr;iP",                    (SUBR) csnarray_medfilt,                     (SUBR) csnarray_medfilt_k,              (SUBR) csnarray_medfilt_nd_deinit,       NULL, 0 },
    { "csnmedfilt.in",         S(CSN_MEDFILT_ND_IN),          0, "",                         ":CsnArr;i",                     (SUBR) csnarray_medfilt_in,                  NULL,                                   (SUBR) csnarray_medfilt_nd_in_deinit,    NULL, 0 },
    { "csnmedfilt.in.k",       S(CSN_MEDFILT_ND_IN),          0, "",                         ":CsnArr;iP",                    (SUBR) csnarray_medfilt_in_k_init,           (SUBR) csnarray_medfilt_in_k,           (SUBR) csnarray_medfilt_nd_in_deinit,    NULL, 0 },
    { "csnmedfilt.s",          S(CSN_MEDFILT_ND_ARR),         0, ":CsnArr;",                 ":CsnArr;i[]",                   (SUBR) csnarray_medfilt_arr,                 NULL,                                   (SUBR) csnarray_medfilt_ndarr_deinit,    NULL, 0 },
    { "csnmedfilt.s.k",        S(CSN_MEDFILT_ND_ARR),         0, ":CsnArr;",                 ":CsnArr;i[]P",                  (SUBR) csnarray_medfilt_arr,                 (SUBR) csnarray_medfilt_arr_k,          (SUBR) csnarray_medfilt_ndarr_deinit,    NULL, 0 },
    { "csnmedfilt.s.in",       S(CSN_MEDFILT_ND_ARR_IN),      0, "",                         ":CsnArr;i[]",                   (SUBR) csnarray_medfilt_arr_in,              NULL,                                   (SUBR) csnarray_medfilt_ndarr_in_deinit, NULL, 0 },
    { "csnmedfilt.s.in.k",     S(CSN_MEDFILT_ND_ARR_IN),      0, "",                         ":CsnArr;i[]P",                  (SUBR) csnarray_medfilt_arr_in_k_init,       (SUBR) csnarray_medfilt_arr_in_k,       (SUBR) csnarray_medfilt_ndarr_in_deinit, NULL, 0 },
    { "csnindexof",            S(CSN_ARGWHERE_INDEX),         0, ":CsnArr;",                 ":CsnArr;i",                     (SUBR) csnarray_indexof,                     NULL,                                   (SUBR) csnarray_indexof_deinit,          NULL, 0 },
    { "csnindexof.k",          S(CSN_ARGWHERE_INDEX),         0, ":CsnArr;",                 ":CsnArr;kP",                    (SUBR) csnarray_indexof_k_init,              (SUBR) csnarray_indexof_k,              (SUBR) csnarray_indexof_deinit,          NULL, 0 },
    { "csnbincount",           S(CSN_BINCOUNT_NO_WEIGHTS),    0, ":CsnArr;",                 ":CsnArr;",                      (SUBR) csnarray_bincount,                    NULL,                                   (SUBR) csnarray_bincount_no_w_deinit,    NULL, 0 },
    { "csnbincount.k",         S(CSN_BINCOUNT_NO_WEIGHTS),    0, ":CsnArr;",                 ":CsnArr;P",                     (SUBR) csnarray_bincount,                    (SUBR) csnarray_bincount_k,             (SUBR) csnarray_bincount_no_w_deinit,    NULL, 0 },
    { "csnbincount.w",         S(CSN_ARGWHERE),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;",              (SUBR) csnarray_bincount_w,                  NULL,                                   (SUBR) csnarray_argwhere_deinit,         NULL, 0 },
    { "csnbincount.w.k",       S(CSN_ARGWHERE),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;P",             (SUBR) csnarray_bincount_w,                  (SUBR) csnarray_bincount_w_k,           (SUBR) csnarray_argwhere_deinit,         NULL, 0 },
    // ---
    // REAL AND COMPLEX
    { "csnempty",              S(CSN_ARR_INIT),               0, ":CsnArr;",                 "i[]o",                          (SUBR) create_empty_csnarray,                NULL,                                   (SUBR) create_csnarray_deinit,           NULL, 0 },
    { "csnempty.k",            S(CSN_ARR_INIT),               0, ":CsnArr;",                 "k[]o",                          (SUBR) create_empty_csnarray_k_init,         (SUBR) create_empty_csnarray_k,         (SUBR) create_csnarray_deinit,           NULL, 0 },
    { "csnzeros",              S(CSN_ARR_INIT),               0, ":CsnArr;",                 "i[]o",                          (SUBR) create_zeros_csnarray,                NULL,                                   (SUBR) create_csnarray_deinit,           NULL, 0 },
    { "csnzeros.k",            S(CSN_ARR_INIT),               0, ":CsnArr;",                 "k[]o",                          (SUBR) create_zeros_csnarray_k_init,         (SUBR) create_zeros_csnarray_k,         (SUBR) create_csnarray_deinit,           NULL, 0 },
    { "csnones",               S(CSN_ARR_INIT),               0, ":CsnArr;",                 "i[]o",                          (SUBR) create_ones_csnarray,                 NULL,                                   (SUBR) create_csnarray_deinit,           NULL, 0 },
    { "csnones.k",             S(CSN_ARR_INIT),               0, ":CsnArr;",                 "k[]o",                          (SUBR) create_ones_csnarray_k_init,          (SUBR) create_ones_csnarray_k,          (SUBR) create_csnarray_deinit,           NULL, 0 },
    { "csnfull",               S(CSN_FULL),                   0, ":CsnArr;",                 "i[]io",                         (SUBR) create_full_csnarray,                 NULL,                                   (SUBR) create_csnarray_full_deinit,      NULL, 0 },
    { "csnfull.c",             S(CSN_FULLCOMPLEX),            0, ":CsnArr;",                 "i[]:Complex;" ,                 (SUBR) create_fullcomp_csnarray,             NULL,                                   (SUBR) create_csnarray_fullcomp_deinit,  NULL, 0 },
    { "csnfull.k",             S(CSN_FULL),                   0, ":CsnArr;",                 "k[]ko",                         (SUBR) create_full_csnarray_k_init,          (SUBR) create_full_csnarray_k,          (SUBR) create_csnarray_full_deinit,      NULL, 0 },
    { "csnfull.c.k",           S(CSN_FULLCOMPLEX),            0, ":CsnArr;",                 "k[]:Complex;" ,                 (SUBR) create_fullcomp_csnarray_k_init,      (SUBR) create_fullcomp_csnarray_k,      (SUBR) create_csnarray_fullcomp_deinit,  NULL, 0 },
    { "csnlike",               S(CSN_ARR_INIT_LIKE),          0, ":CsnArr;",                 ":CsnArr;i",                     (SUBR) create_like_csnarray,                 NULL,                                   (SUBR) create_csnarray_like_deinit,      NULL, 0 },
    { "csnlike.k",             S(CSN_ARR_INIT_LIKE),          0, ":CsnArr;",                 ":CsnArr;k",                     (SUBR) create_like_csnarray_k_init,          (SUBR) create_like_csnarray_k,          (SUBR) create_csnarray_like_deinit,      NULL, 0 },
    { "csnfromarray",          S(CSN_FROM_ARRAY),             0, ":CsnArr;",                 "i[]",                           (SUBR) from_array_to_csnarray,               NULL,                                   (SUBR) from_array_to_csnarray_deinit,    NULL, 0 },
    { "csnfromarray.c",        S(CSN_FROM_ARRAY),             0, ":CsnArr;",                 ":Complex;[]",                   (SUBR) from_complexarray_to_csnarray_k_init, (SUBR) from_complexarray_to_csnarray_k, (SUBR) from_array_to_csnarray_deinit,    NULL, 0 },
    { "csnfromarray.k",        S(CSN_FROM_ARRAY),             0, ":CsnArr;",                 "k[]",                           (SUBR) from_array_to_csnarray_k_init,        (SUBR) from_array_to_csnarray_k,        (SUBR) from_array_to_csnarray_deinit,    NULL, 0 },
    { "csntoarray",            S(CSN_TO_ARRAY),               0, "i[]",                      ":CsnArr;",                      (SUBR) from_csnarray_to_array,               NULL,                                   NULL,                                    NULL, 0 },
    { "csntoarray.k",          S(CSN_TO_ARRAY),               0, "k[]",                      ":CsnArr;",                      (SUBR) from_csnarray_to_array,               (SUBR) from_csnarray_to_array_k,        NULL,                                    NULL, 0 },
    { "csntoarray.c",          S(CSN_TO_ARRAY),               0, ":Complex;[]",              ":CsnArr;",                      (SUBR) from_csnarray_to_complexarray,        (SUBR) from_csnarray_to_complexarray_k, NULL,                                    NULL, 0 },
    { "csnfree",               S(CSN_FREE),                   0, "",                         ":CsnArr;",                      (SUBR) free_csnarray,                        NULL,                                   NULL,                                    NULL, 0 },
    { "csndims",               S(CSN_SIZE_DIMS),              0, "i",                        ":CsnArr;",                      (SUBR) csnarray_dims,                        NULL,                                   NULL,                                    NULL, 0 },
    { "csnsize",               S(CSN_SIZE_DIMS),              0, "i",                        ":CsnArr;",                      (SUBR) csnarray_size,                        NULL,                                   NULL,                                    NULL, 0 },
    { "csnisempty",            S(CSN_SIZE_DIMS),              0, "i",                        ":CsnArr;",                      (SUBR) csnarray_is_empty,                    NULL,                                   NULL,                                    NULL, 0 },
    { "csnshape",              S(CSN_SHAPE),                  0, "i[]",                      ":CsnArr;",                      (SUBR) csnarray_shape,                       NULL,                                   NULL,                                    NULL, 0 },
    { "csndims.k",             S(CSN_SIZE_DIMS),              0, "k",                        ":CsnArr;",                      NULL,                                        (SUBR) csnarray_dims_k,                 NULL,                                    NULL, 0 },
    { "csnsize.k",             S(CSN_SIZE_DIMS),              0, "k",                        ":CsnArr;",                      NULL,                                        (SUBR) csnarray_size_k,                 NULL,                                    NULL, 0 },
    { "csnisempty.k",          S(CSN_SIZE_DIMS),              0, "k",                        ":CsnArr;",                      NULL,                                        (SUBR) csnarray_is_empty_k,             NULL,                                    NULL, 0 },
    { "csnshape.k",            S(CSN_SHAPE),                  0, "k[]",                      ":CsnArr;",                      (SUBR) csnarray_shape,                       (SUBR) csnarray_shape_k,                NULL,                                    NULL, 0 },
    { "csnidentity",           S(CSN_IDENTITY),               0, ":CsnArr;",                 "io",                            (SUBR) csnarray_identity,                    NULL,                                   (SUBR) csnarray_identity_deinit,         NULL, 0 },
    { "csnidentity.k",         S(CSN_IDENTITY),               0, ":CsnArr;",                 "ko",                            (SUBR) csnarray_identity_k_init,             (SUBR) csnarray_identity_k,             (SUBR) csnarray_identity_deinit,         NULL, 0 },
    { "csnreshape",            S(CSN_RESHAPE),                0, ":CsnArr;",                 ":CsnArr;i[]",                   (SUBR) csnarray_reshape,                     NULL,                                   (SUBR) csnarray_shape_deinit,            NULL, 0 },
    { "csnreshape.in",         S(CSN_RESHAPE_IN),             0, "",                         ":CsnArr;i[]",                   (SUBR) csnarray_reshape_in,                  NULL,                                   NULL,                                    NULL, 0 },
    { "csnreshape.k",          S(CSN_RESHAPE),                0, ":CsnArr;",                 ":CsnArr;k[]",                   (SUBR) csnarray_reshape_k_init,              (SUBR) csnarray_reshape_k,              (SUBR) csnarray_shape_deinit,            NULL, 0 },
    { "csnreshape.in.k",       S(CSN_RESHAPE_IN),             0, "",                         ":CsnArr;k[]",                   (SUBR) csnarray_reshape_in_k_init,           (SUBR) csnarray_reshape_in_k,           NULL,                                    NULL, 0 },
    { "csnflatten",            S(CSN_RESHAPE),                0, ":CsnArr;",                 ":CsnArr;",                      (SUBR) csnarray_flatten,                     (SUBR) csnarray_flatten_k,              (SUBR) csnarray_shape_deinit,            NULL, 0 },
    { "csnflatten.in",         S(CSN_RESHAPE_IN),             0, "",                         ":CsnArr;",                      (SUBR) csnarray_flatten_in,                  (SUBR) csnarray_flatten_in_k,           NULL,                                    NULL, 0 },
    { "csntranspose",          S(CSN_RESHAPE),                0, ":CsnArr;",                 ":CsnArr;",                      (SUBR) csnarray_transpose,                   (SUBR) csnarray_transpose_k,            (SUBR) csnarray_shape_deinit,            NULL, 0 },
    { "csntranspose.ax",       S(CSN_RESHAPE),                0, ":CsnArr;",                 ":CsnArr;i[]",                   (SUBR) csnarray_transpose,                   (SUBR) csnarray_transpose_k,            (SUBR) csnarray_shape_deinit,            NULL, 0 },
    { "csntranspose.ax.k",     S(CSN_RESHAPE),                0, ":CsnArr;",                 ":CsnArr;k[]",                   (SUBR) csnarray_transpose,                   (SUBR) csnarray_transpose_k,            (SUBR) csnarray_shape_deinit,            NULL, 0 },
    { "csntranspose.in",       S(CSN_RESHAPE_IN),             0, "",                         ":CsnArr;",                      (SUBR) csnarray_transpose_in_k_init,         (SUBR) csnarray_transpose_in_k,         (SUBR) csnarray_transpose_in_k_deinit,   NULL, 0 },
    { "csntranspose.ax.in",    S(CSN_RESHAPE_IN),             0, "",                         ":CsnArr;i[]",                   (SUBR) csnarray_transpose_in,                NULL,                                   NULL,                                    NULL, 0 },
    { "csntranspose.ax.in.k",  S(CSN_RESHAPE_IN),             0, "",                         ":CsnArr;k[]",                   (SUBR) csnarray_transpose_in_k_init,         (SUBR) csnarray_transpose_in_k,         (SUBR) csnarray_transpose_in_k_deinit,   NULL, 0 },
    { "csnflip",               S(CSN_FLIP_ROLL),              0, ":CsnArr;",                 ":CsnArr;",                      (SUBR) csnarray_flip,                        NULL,                                   (SUBR) csnarray_flip_deinit,             NULL, 0 },
    { "csnflip",               S(CSN_FLIP_ROLL),              0, ":CsnArr;",                 ":CsnArr;i",                     (SUBR) csnarray_flip,                        NULL,                                   (SUBR) csnarray_flip_deinit,             NULL, 0 },
    { "csnflip.in",            S(CSN_FLIP_ROLL_IN),           0, "",                         ":CsnArr;",                      (SUBR) csnarray_flip_in,                     NULL,                                   NULL,                                    NULL, 0 },
    { "csnflip.in",            S(CSN_FLIP_ROLL_IN),           0, "",                         ":CsnArr;i",                     (SUBR) csnarray_flip_in,                     NULL,                                   NULL,                                    NULL, 0 },
    { "csnflip.k",             S(CSN_FLIP_ROLL),              0, ":CsnArr;",                 ":CsnArr;P",                     (SUBR) csnarray_flip_k_init,                 (SUBR) csnarray_flip_k,                 (SUBR) csnarray_flip_deinit,             NULL, 0 },
    { "csnflip.k",             S(CSN_FLIP_ROLL),              0, ":CsnArr;",                 ":CsnArr;kk",                    (SUBR) csnarray_flip_k_init,                 (SUBR) csnarray_flip_k,                 (SUBR) csnarray_flip_deinit,             NULL, 0 },
    { "csnflip.in.k",          S(CSN_FLIP_ROLL_IN),           0, "",                         ":CsnArr;P",                     (SUBR) csnarray_flip_in_k_init,              (SUBR) csnarray_flip_in_k,              (SUBR) csnarray_flip_in_k_deinit,        NULL, 0 },
    { "csnflip.in.k",          S(CSN_FLIP_ROLL_IN),           0, "",                         ":CsnArr;kk",                    (SUBR) csnarray_flip_in_k_init,              (SUBR) csnarray_flip_in_k,              (SUBR) csnarray_flip_in_k_deinit,        NULL, 0 },
    { "csnroll",               S(CSN_FLIP_ROLL),              0, ":CsnArr;",                 ":CsnArr;i",                     (SUBR) csnarray_roll,                        NULL,                                   (SUBR) csnarray_flip_deinit,             NULL, 0 },
    { "csnroll.in",            S(CSN_FLIP_ROLL_IN),           0, "",                         ":CsnArr;i",                     (SUBR) csnarray_roll_in,                     NULL,                                   NULL,                                    NULL, 0 },
    { "csnroll.ax",            S(CSN_FLIP_ROLL),              0, ":CsnArr;",                 ":CsnArr;ii",                    (SUBR) csnarray_rollaxis,                    NULL,                                   (SUBR) csnarray_flip_deinit,             NULL, 0 },
    { "csnroll.ax.in",         S(CSN_FLIP_ROLL_IN),           0, "",                         ":CsnArr;ii",                    (SUBR) csnarray_rollaxis_in,                 NULL,                                   NULL,                                    NULL, 0 },
    { "csnroll.k",             S(CSN_FLIP_ROLL),              0, ":CsnArr;",                 ":CsnArr;k",                     (SUBR) csnarray_roll,                        (SUBR) csnarray_roll_k,                 (SUBR) csnarray_flip_deinit,             NULL, 0 },
    { "csnroll.in.k",          S(CSN_FLIP_ROLL_IN),           0, "",                         ":CsnArr;k",                     (SUBR) csnarray_roll_in_k_init,              (SUBR) csnarray_roll_in_k,              (SUBR) csnarray_flip_in_k_deinit,        NULL, 0 },
    { "csnroll.ax.k",          S(CSN_FLIP_ROLL),              0, ":CsnArr;",                 ":CsnArr;kk",                    (SUBR) csnarray_rollaxis,                    (SUBR) csnarray_rollaxis_k,             (SUBR) csnarray_flip_deinit,             NULL, 0 },
    { "csnroll.ax.in.k",       S(CSN_FLIP_ROLL_IN),           0, "",                         ":CsnArr;kk",                    (SUBR) csnarray_rollaxis_in_k_init,          (SUBR) csnarray_rollaxis_in_k,          (SUBR) csnarray_flip_in_k_deinit,        NULL, 0 },
    { "csnget",                S(CSN_GET),                    0, "i",                        ":CsnArr;i[]",                   (SUBR) csnarray_get,                         NULL,                                   NULL,                                    NULL, 0 },
    { "csnget.c",              S(CSN_GETCOMPLEX),             0, ":Complex;",                ":CsnArr;i[]",                   (SUBR) csnarray_get_complex,                 NULL,                                   NULL,                                    NULL, 0 },
    { "csnget.k",              S(CSN_GET),                    0, "k",                        ":CsnArr;k[]",                   (SUBR) csnarray_get,                         (SUBR) csnarray_get_k,                  NULL,                                    NULL, 0 },
    { "csnget.c.k",            S(CSN_GETCOMPLEX),             0, ":Complex;",                ":CsnArr;k[]",                   (SUBR) csnarray_get_complex,                 (SUBR) csnarray_get_complex_k,          NULL,                                    NULL, 0 },
    { "csngetrow",             S(CSN_GET_ROWCOL),             0, ":CsnArr;",                 ":CsnArr;i",                     (SUBR) csnarray_get_row,                     NULL,                                   (SUBR) csnarray_get_rowcol_deinit,       NULL, 0 },
    { "csngetrow.k",           S(CSN_GET_ROWCOL),             0, ":CsnArr;",                 ":CsnArr;kP",                    (SUBR) csnarray_get_row_k_init,              (SUBR) csnarray_get_row_k,              (SUBR) csnarray_get_rowcol_deinit,       NULL, 0 },
    { "csngetcol",             S(CSN_GET_ROWCOL),             0, ":CsnArr;",                 ":CsnArr;i",                     (SUBR) csnarray_get_col,                     NULL,                                   (SUBR) csnarray_get_rowcol_deinit,       NULL, 0 },
    { "csngetcol.k",           S(CSN_GET_ROWCOL),             0, ":CsnArr;",                 ":CsnArr;kP",                    (SUBR) csnarray_get_col_k_init,              (SUBR) csnarray_get_col_k,              (SUBR) csnarray_get_rowcol_deinit,       NULL, 0 },
    { "csnset",                S(CSN_SET),                    0, "",                         ":CsnArr;i[]i",                  (SUBR) csnarray_set,                         NULL,                                   NULL,                                    NULL, 0 },
    { "csnset.c",              S(CSN_SETCOMPLEX),             0, "",                         ":CsnArr;i[]:Complex;",          (SUBR) csnarray_set_complex,                 NULL,                                   NULL,                                    NULL, 0 },
    { "csnset.kk",             S(CSN_SET),                    0, "",                         ":CsnArr;k[]k",                  (SUBR) csnarray_set,                         (SUBR) csnarray_set_k,                  NULL,                                    NULL, 0 },
    { "csnset.c.k",            S(CSN_SETCOMPLEX),             0, "",                         ":CsnArr;k[]:Complex;",          (SUBR) csnarray_set_complex,                 (SUBR) csnarray_set_complex_k,          NULL,                                    NULL, 0 },
    { "csnset.ik",             S(CSN_SET),                    0, "",                         ":CsnArr;i[]k",                  (SUBR) csnarray_set,                         (SUBR) csnarray_set_k,                  NULL,                                    NULL, 0 },
    { "csnset.ki",             S(CSN_SET),                    0, "",                         ":CsnArr;k[]i",                  (SUBR) csnarray_set,                         (SUBR) csnarray_set_k,                  NULL,                                    NULL, 0 },
    { "csntake",               S(CSN_TAKE),                   0, ":CsnArr;",                 ":CsnArr;ii",                    (SUBR) csnarray_take,                        NULL,                                   (SUBR) csnarray_take_deinit,             NULL, 0 },
    { "csntake.kk",            S(CSN_TAKE),                   0, ":CsnArr;",                 ":CsnArr;kk",                    (SUBR) csnarray_take,                        (SUBR) csnarray_take_k,                 (SUBR) csnarray_take_deinit,             NULL, 0 },
    { "csntake.ik",            S(CSN_TAKE),                   0, ":CsnArr;",                 ":CsnArr;ik",                    (SUBR) csnarray_take,                        (SUBR) csnarray_take_k,                 (SUBR) csnarray_take_deinit,             NULL, 0 },
    { "csntake.ki",            S(CSN_TAKE),                   0, ":CsnArr;",                 ":CsnArr;ki",                    (SUBR) csnarray_take,                        (SUBR) csnarray_take_k,                 (SUBR) csnarray_take_deinit,             NULL, 0 },
    { "csntake.flat",          S(CSN_TAKE_FLAT),              0, "i",                        ":CsnArr;i",                     (SUBR) csnarray_take_flat,                   NULL,                                   NULL,                                    NULL, 0 },
    { "csntake.flat.c",        S(CSN_TAKECOMPLEX_FLAT),       0, ":Complex;",                ":CsnArr;i",                     (SUBR) csnarray_takecomp_flat,               NULL,                                   NULL,                                    NULL, 0 },
    { "csntake.flat.k",        S(CSN_TAKE_FLAT),              0, "k",                        ":CsnArr;k",                     (SUBR) csnarray_take_flat,                   (SUBR) csnarray_take_flat_k,            NULL,                                    NULL, 0 },
    { "csntake.flat.c.k",      S(CSN_TAKECOMPLEX_FLAT),       0, ":Complex;",                ":CsnArr;k",                     (SUBR) csnarray_takecomp_flat,               (SUBR) csnarray_takecomp_flat_k,        NULL,                                    NULL, 0 },
    { "csngetslice",           S(CSN_GET_SLICE),              0, ":CsnArr;",                 ":CsnArr;iiii",                  (SUBR) csnarray_get_slice,                   NULL,                                   (SUBR) csnarray_slice_deinit,            NULL, 0 },
    { "csngetslice.k",         S(CSN_GET_SLICE),              0, ":CsnArr;",                 ":CsnArr;kkkk",                  (SUBR) csnarray_get_slice_k_init,                   (SUBR) csnarray_get_slice_k,     (SUBR) csnarray_slice_deinit,            NULL, 0 },
    { "csnsetslice",           S(CSN_SET_SLICE),              0, "",                         ":CsnArr;:CsnArr;iiii",          (SUBR) csnarray_set_slice,                   NULL,                                   NULL,                                    NULL, 0 },
    { "csnsetslice.k",         S(CSN_SET_SLICE),              0, "",                         ":CsnArr;:CsnArr;kkkk",          (SUBR) csnarray_set_slice,                   (SUBR) csnarray_set_slice_k,            NULL,                                    NULL, 0 },
    { "csnpush",               S(CSN_PUSH),                   0, "",                         ":CsnArr;i",                     (SUBR) csnarray_push,                        NULL,                                   NULL,                                    NULL, 0 },
    { "csnpush.c",             S(CSN_PUSHCOMPLEX),            0, "",                         ":CsnArr;:Complex;",             (SUBR) csnarray_pushcomp,                    NULL,                                   NULL,                                    NULL, 0 },
    { "csnpush.k",             S(CSN_PUSH_K),                 0, "",                         ":CsnArr;kk",                    (SUBR) csnarray_push_k_init,                 (SUBR) csnarray_push_k,                 NULL,                                    NULL, 0 },
    { "csnpush.c.k",           S(CSN_PUSHCOMPLEX_K),          0, "",                         ":CsnArr;:Complex;k",            (SUBR) csnarray_pushcomp_k_init,             (SUBR) csnarray_pushcomp_k,             NULL,                                    NULL, 0 },
    { "csnpop",                S(CSN_POP),                    0, "i",                        ":CsnArr;",                      (SUBR) csnarray_pop,                         NULL,                                   NULL,                                    NULL, 0 },
    { "csnpop.c",              S(CSN_POPCOMPLEX),             0, ":Complex;",                ":CsnArr;",                      (SUBR) csnarray_popcomp,                     NULL,                                   NULL,                                    NULL, 0 },
    { "csnpop.k",              S(CSN_POP_K),                  0, "k",                        ":CsnArr;k",                     (SUBR) csnarray_pop_k_init,                  (SUBR) csnarray_pop_k,                  NULL,                                    NULL, 0 },
    { "csnpop.c.k",            S(CSN_POPCOMPLEX_K),           0, ":Complex;",                ":CsnArr;k",                     (SUBR) csnarray_popcomp_k_init,              (SUBR) csnarray_popcomp_k,              NULL,                                    NULL, 0 },
    { "csninsert.flat",        S(CSN_PUSH),                   0, "",                         ":CsnArr;ii",                    (SUBR) csnarray_insert,                      NULL,                                   NULL,                                    NULL, 0 },
    { "csninsert.flat.c",      S(CSN_PUSHCOMPLEX),            0, "",                         ":CsnArr;:Complex;i",            (SUBR) csnarray_insertcomp,                  NULL,                                   NULL,                                    NULL, 0 },
    { "csninsert.flat.k",      S(CSN_PUSH_K),                 0, "",                         ":CsnArr;kkk",                   (SUBR) csnarray_push_k_init,                 (SUBR) csnarray_insert_k,               NULL,                                    NULL, 0 },
    { "csninsert.flat.c.k",    S(CSN_PUSHCOMPLEX_K),          0, "",                         ":CsnArr;:Complex;kk",           (SUBR) csnarray_pushcomp_k_init,             (SUBR) csnarray_insertcomp_k,           NULL,                                    NULL, 0 },
    { "csnremove.flat",        S(CSN_POP),                    0, "i",                        ":CsnArr;i",                     (SUBR) csnarray_remove,                      NULL,                                   NULL,                                    NULL, 0 },
    { "csnremove.flat.c",      S(CSN_POPCOMPLEX),             0, ":Complex;",                ":CsnArr;i",                     (SUBR) csnarray_removecomp,                  NULL,                                   NULL,                                    NULL, 0 },
    { "csnremove.flat.k",      S(CSN_POP_K),                  0, "k",                        ":CsnArr;kk",                    (SUBR) csnarray_pop_k_init,                  (SUBR) csnarray_remove_k,               NULL,                                    NULL, 0 },
    { "csnremove.flat.c.k",    S(CSN_POPCOMPLEX_K),           0, ":Complex;",                ":CsnArr;kk",                    (SUBR) csnarray_popcomp_k_init,              (SUBR) csnarray_removecomp_k,           NULL,                                    NULL, 0 },
    { "csninsert.block",       S(CSN_INSERT_BLOCK),           0, "",                         ":CsnArr;:CsnArr;ii",            (SUBR) csnarray_insert_block,                NULL,                                   (SUBR) csnarray_insert_block_deinit,     NULL, 0 },
    { "csninsert.block.k",     S(CSN_INSERT_BLOCK),           0, "",                         ":CsnArr;:CsnArr;kkP",           (SUBR) csnarray_insert_block_k_init,         (SUBR) csnarray_insert_block_k,         (SUBR) csnarray_insert_block_deinit,     NULL, 0 },
    { "csnremove.block",       S(CSN_TAKE),                   0, ":CsnArr;",                 ":CsnArr;ii",                    (SUBR) csnarray_remove_block,                NULL,                                   (SUBR) csnarray_take_deinit,             NULL, 0 },
    { "csnremove.block.k",     S(CSN_TAKE),                   0, ":CsnArr;",                 ":CsnArr;kkP",                   (SUBR) csnarray_remove_block_k_init,         (SUBR) csnarray_remove_block_k,         (SUBR) csnarray_take_deinit,             NULL, 0 },
    { "csnconcat.block",       S(CSN_CONCAT),                 0, ":CsnArr;",                 ":CsnArr;:CsnArr;i",             (SUBR) csnarray_concat_block,                NULL,                                   (SUBR) csnarray_concat_deinit,           NULL, 0 },
    { "csnconcat.block.k",     S(CSN_CONCAT),                 0, ":CsnArr;",                 ":CsnArr;:CsnArr;kk",            (SUBR) csnarray_concat_block_k_init,         (SUBR) csnarray_concat_block_k,         (SUBR) csnarray_concat_deinit,           NULL, 0 },
    { "csnconcat.flat",        S(CSN_CONCAT),                 0, ":CsnArr;",                 ":CsnArr;:CsnArr;",              (SUBR) csnarray_concat_flat,                 NULL,                                   (SUBR) csnarray_concat_deinit,           NULL, 0 },
    { "csnconcat.flat.k",      S(CSN_CONCAT),                 0, ":CsnArr;",                 ":CsnArr;:CsnArr;k",             (SUBR) csnarray_concat_flat,                 (SUBR) csnarray_concat_flat_k,          (SUBR) csnarray_concat_deinit,           NULL, 0 },
    { "csnpad",                S(CSN_PAD),                    0, ":CsnArr;",                 ":CsnArr;iio",                   (SUBR) csnarray_pad,                         NULL,                                   (SUBR) csnarray_pad_deinit,              NULL, 0 },
    { "csnpad.ax",             S(CSN_PAD),                    0, ":CsnArr;",                 ":CsnArr;iiii",                  (SUBR) csnarray_pad,                         NULL,                                   (SUBR) csnarray_pad_deinit,              NULL, 0 },
    { "csnpad.in",             S(CSN_PAD_IN),                 0, "",                         ":CsnArr;iio",                   (SUBR) csnarray_pad_in,                      NULL,                                   NULL,                                    NULL, 0 },
    { "csnpad.ax.in",          S(CSN_PAD_IN),                 0, "",                         ":CsnArr;iiii",                  (SUBR) csnarray_pad_in,                      NULL,                                   NULL,                                    NULL, 0 },
    { "csnpad.c",              S(CSN_PADCOMPLEX),             0, ":CsnArr;",                 ":CsnArr;ii:Complex;",           (SUBR) csnarray_padcomp,                     NULL,                                   (SUBR) csnarray_padcomp_deinit,          NULL, 0 },
    { "csnpad.ax.c",           S(CSN_PADCOMPLEX),             0, ":CsnArr;",                 ":CsnArr;ii:Complex;i",          (SUBR) csnarray_padcomp,                     NULL,                                   (SUBR) csnarray_padcomp_deinit,          NULL, 0 },
    { "csnpad.in.c",           S(CSN_PADCOMPLEX_IN),          0, "",                         ":CsnArr;ii:Complex;",           (SUBR) csnarray_padcomp_in,                  NULL,                                   NULL,                                    NULL, 0 },
    { "csnpad.ax.in.c",        S(CSN_PADCOMPLEX_IN),          0, "",                         ":CsnArr;ii:Complex;i",          (SUBR) csnarray_padcomp_in,                  NULL,                                   NULL,                                    NULL, 0 },
    { "csnpad.k",              S(CSN_PAD),                    0, ":CsnArr;",                 ":CsnArr;kkkk",                  (SUBR) csnarray_pad_k_init,                  (SUBR) csnarray_pad_k,                  (SUBR) csnarray_pad_deinit,              NULL, 0 },
    { "csnpad.ax.k",           S(CSN_PAD),                    0, ":CsnArr;",                 ":CsnArr;kkkkk",                 (SUBR) csnarray_pad_k_init,                  (SUBR) csnarray_pad_k,                  (SUBR) csnarray_pad_deinit,              NULL, 0 },
    { "csnpad.in.k",           S(CSN_PAD_IN),                 0, "",                         ":CsnArr;kkkk",                  (SUBR) csnarray_pad_in_k_init,               (SUBR) csnarray_pad_in_k,               (SUBR) csnarray_pad_in_k_deinit,         NULL, 0 },
    { "csnpad.ax.in.k",        S(CSN_PAD_IN),                 0, "",                         ":CsnArr;kkkkk",                 (SUBR) csnarray_pad_in_k_init,               (SUBR) csnarray_pad_in_k,               (SUBR) csnarray_pad_in_k_deinit,         NULL, 0 },
    { "csnpad.c.k",            S(CSN_PADCOMPLEX),             0, ":CsnArr;",                 ":CsnArr;kk:Complex;k",          (SUBR) csnarray_padcomp_k_init,              (SUBR) csnarray_padcomp_k,              (SUBR) csnarray_padcomp_deinit,          NULL, 0 },
    { "csnpad.ax.c.k",         S(CSN_PADCOMPLEX),             0, ":CsnArr;",                 ":CsnArr;kk:Complex;kk",         (SUBR) csnarray_padcomp_k_init,              (SUBR) csnarray_padcomp_k,              (SUBR) csnarray_padcomp_deinit,          NULL, 0 },
    { "csnpad.in.c.k",         S(CSN_PADCOMPLEX_IN),          0, "",                         ":CsnArr;kk:Complex;k",          (SUBR) csnarray_padcomp_in_k_init,           (SUBR) csnarray_padcomp_in_k,           (SUBR) csnarray_padcomp_in_k_deinit,     NULL, 0 },
    { "csnpad.ax.in.c.k",      S(CSN_PADCOMPLEX_IN),          0, "",                         ":CsnArr;kk:Complex;kk",         (SUBR) csnarray_padcomp_in_k_init,           (SUBR) csnarray_padcomp_in_k,           (SUBR) csnarray_padcomp_in_k_deinit,     NULL, 0 },
    { "csnsum",                S(CSN_REDUCTION_SCALAR),       0, "i",                        ":CsnArr;",                      (SUBR) csnarray_sum_all,                     NULL,                                   NULL,                                    NULL, 0 },
    { "csnsum.k",              S(CSN_REDUCTION_SCALAR),       0, "k",                        ":CsnArr;P",                     (SUBR) csnarray_sum_all_k_init,              (SUBR) csnarray_sum_all_k,              NULL,                                    NULL, 0 },
    { "csnprod",               S(CSN_REDUCTION_SCALAR),       0, "i",                        ":CsnArr;",                      (SUBR) csnarray_prod_all,                    NULL,                                   NULL,                                    NULL, 0 },
    { "csnprod.k",             S(CSN_REDUCTION_SCALAR),       0, "k",                        ":CsnArr;P",                     (SUBR) csnarray_prod_all_k_init,             (SUBR) csnarray_prod_all_k,             NULL,                                    NULL, 0 },
    { "csnsub",                S(CSN_REDUCTION_SCALAR),       0, "i",                        ":CsnArr;",                      (SUBR) csnarray_sub_all,                     NULL,                                   NULL,                                    NULL, 0 },
    { "csnsub.k",              S(CSN_REDUCTION_SCALAR),       0, "k",                        ":CsnArr;P",                     (SUBR) csnarray_sub_all_k_init,              (SUBR) csnarray_sub_all_k,              NULL,                                    NULL, 0 },
    { "csnmean",               S(CSN_REDUCTION_SCALAR),       0, "i",                        ":CsnArr;",                      (SUBR) csnarray_mean_all,                    NULL,                                   NULL,                                    NULL, 0 },
    { "csnmean.k",             S(CSN_REDUCTION_SCALAR),       0, "k",                        ":CsnArr;P",                     (SUBR) csnarray_mean_all_k_init,             (SUBR) csnarray_mean_all_k,             NULL,                                    NULL, 0 },
    { "csnall",                S(CSN_REDUCTION_SCALAR),       0, "i",                        ":CsnArr;",                      (SUBR) csnarray_all_all,                     NULL,                                   NULL,                                    NULL, 0 },
    { "csnall.k",              S(CSN_REDUCTION_SCALAR),       0, "k",                        ":CsnArr;P",                     (SUBR) csnarray_all_all_k_init,              (SUBR) csnarray_all_all_k,              NULL,                                    NULL, 0 },
    { "csnany",                S(CSN_REDUCTION_SCALAR),       0, "i",                        ":CsnArr;",                      (SUBR) csnarray_any_all,                     NULL,                                   NULL,                                    NULL, 0 },
    { "csnany.k",              S(CSN_REDUCTION_SCALAR),       0, "k",                        ":CsnArr;P",                     (SUBR) csnarray_any_all_k_init,              (SUBR) csnarray_any_all_k,              NULL,                                    NULL, 0 },
    { "csnstd",                S(CSN_REDUCTION_SCALAR),       0, "i",                        ":CsnArr;",                      (SUBR) csnarray_std_all,                     NULL,                                   NULL,                                    NULL, 0 },
    { "csnstd.k",              S(CSN_REDUCTION_SCALAR),       0, "k",                        ":CsnArr;P",                     (SUBR) csnarray_std_all_k_init,              (SUBR) csnarray_std_all_k,              NULL,                                    NULL, 0 },
    { "csnvar",                S(CSN_REDUCTION_SCALAR),       0, "i",                        ":CsnArr;",                      (SUBR) csnarray_var_all,                     NULL,                                   NULL,                                    NULL, 0 },
    { "csnvar.k",              S(CSN_REDUCTION_SCALAR),       0, "k",                        ":CsnArr;P",                     (SUBR) csnarray_var_all_k_init,              (SUBR) csnarray_var_all_k,              NULL,                                    NULL, 0 },
    { "csnsum.c",              S(CSN_REDUCTION_COMPLEX_S),    0, ":Complex;",                ":CsnArr;",                      (SUBR) csnarray_sumcomp_all,                 NULL,                                   NULL,                                    NULL, 0 },
    { "csnprod.c",             S(CSN_REDUCTION_COMPLEX_S),    0, ":Complex;",                ":CsnArr;",                      (SUBR) csnarray_prodcomp_all,                NULL,                                   NULL,                                    NULL, 0 },
    { "csnsub.c",              S(CSN_REDUCTION_COMPLEX_S),    0, ":Complex;",                ":CsnArr;",                      (SUBR) csnarray_subcomp_all,                 NULL,                                   NULL,                                    NULL, 0 },
    { "csnmean.c",             S(CSN_REDUCTION_COMPLEX_S),    0, ":Complex;",                ":CsnArr;",                      (SUBR) csnarray_meancomp_all,                NULL,                                   NULL,                                    NULL, 0 },
    { "csnsum.c.k",            S(CSN_REDUCTION_COMPLEX_S),    0, ":Complex;",                ":CsnArr;k",                     (SUBR) csnarray_sumcomp_all_k_init,          (SUBR) csnarray_sumcomp_all_k,          NULL,                                    NULL, 0 },
    { "csnprod.c.k",           S(CSN_REDUCTION_COMPLEX_S),    0, ":Complex;",                ":CsnArr;k",                     (SUBR) csnarray_prodcomp_all_k_init,         (SUBR) csnarray_prodcomp_all_k,         NULL,                                    NULL, 0 },
    { "csnsub.c.k",            S(CSN_REDUCTION_COMPLEX_S),    0, ":Complex;",                ":CsnArr;k",                     (SUBR) csnarray_subcomp_all_k_init,          (SUBR) csnarray_subcomp_all_k,          NULL,                                    NULL, 0 },
    { "csnmean.c.k",           S(CSN_REDUCTION_COMPLEX_S),    0, ":Complex;",                ":CsnArr;k",                     (SUBR) csnarray_meancomp_all_k_init,         (SUBR) csnarray_meancomp_all_k,         NULL,                                    NULL, 0 },
    { "csnsum.ax",             S(CSN_REDUCTION),              0, ":CsnArr;",                 ":CsnArr;i",                     (SUBR) csnarray_sum,                         NULL,                                   (SUBR) csnarray_reduction_deinit,        NULL, 0 },
    { "csnsum.ax.k",           S(CSN_REDUCTION),              0, ":CsnArr;",                 ":CsnArr;kk",                    (SUBR) csnarray_sum_k_init,                  (SUBR) csnarray_sum_k,                  (SUBR) csnarray_reduction_deinit,        NULL, 0 },
    { "csnprod.ax",            S(CSN_REDUCTION),              0, ":CsnArr;",                 ":CsnArr;i",                     (SUBR) csnarray_prod,                        NULL,                                   (SUBR) csnarray_reduction_deinit,        NULL, 0 },
    { "csnprod.ax.k",          S(CSN_REDUCTION),              0, ":CsnArr;",                 ":CsnArr;kk",                    (SUBR) csnarray_prod_k_init,                 (SUBR) csnarray_prod_k,                 (SUBR) csnarray_reduction_deinit,        NULL, 0 },
    { "csnsub.ax",             S(CSN_REDUCTION),              0, ":CsnArr;",                 ":CsnArr;i",                     (SUBR) csnarray_sub,                         NULL,                                   (SUBR) csnarray_reduction_deinit,        NULL, 0 },
    { "csnsub.ax.k",           S(CSN_REDUCTION),              0, ":CsnArr;",                 ":CsnArr;kk",                    (SUBR) csnarray_sub_k_init,                  (SUBR) csnarray_sub_k,                  (SUBR) csnarray_reduction_deinit,        NULL, 0 },
    { "csnmean.ax",            S(CSN_REDUCTION),              0, ":CsnArr;",                 ":CsnArr;i",                     (SUBR) csnarray_mean,                        NULL,                                   (SUBR) csnarray_reduction_deinit,        NULL, 0 },
    { "csnmean.ax.k",          S(CSN_REDUCTION),              0, ":CsnArr;",                 ":CsnArr;kk",                    (SUBR) csnarray_mean_k_init,                 (SUBR) csnarray_mean_k,                 (SUBR) csnarray_reduction_deinit,        NULL, 0 },
    { "csnany.ax",             S(CSN_REDUCTION),              0, ":CsnArr;",                 ":CsnArr;i",                     (SUBR) csnarray_any,                         NULL,                                   (SUBR) csnarray_reduction_deinit,        NULL, 0 },
    { "csnany.ax.k",           S(CSN_REDUCTION),              0, ":CsnArr;",                 ":CsnArr;kk",                    (SUBR) csnarray_any_k_init,                  (SUBR) csnarray_any_k,                  (SUBR) csnarray_reduction_deinit,        NULL, 0 },
    { "csnall.ax",             S(CSN_REDUCTION),              0, ":CsnArr;",                 ":CsnArr;i",                     (SUBR) csnarray_all,                         NULL,                                   (SUBR) csnarray_reduction_deinit,        NULL, 0 },
    { "csnall.ax.k",           S(CSN_REDUCTION),              0, ":CsnArr;",                 ":CsnArr;kk",                    (SUBR) csnarray_all_k_init,                  (SUBR) csnarray_all_k,                  (SUBR) csnarray_reduction_deinit,        NULL, 0 },
    { "csnstd.ax",             S(CSN_REDUCTION),              0, ":CsnArr;",                 ":CsnArr;i",                     (SUBR) csnarray_std,                         NULL,                                   (SUBR) csnarray_reduction_deinit,        NULL, 0 },
    { "csnstd.ax.k",           S(CSN_REDUCTION),              0, ":CsnArr;",                 ":CsnArr;kk",                    (SUBR) csnarray_std_k_init,                  (SUBR) csnarray_std_k,                  (SUBR) csnarray_reduction_deinit,        NULL, 0 },
    { "csnvar.ax",             S(CSN_REDUCTION),              0, ":CsnArr;",                 ":CsnArr;i",                     (SUBR) csnarray_var,                         NULL,                                   (SUBR) csnarray_reduction_deinit,        NULL, 0 },
    { "csnvar.ax.k",           S(CSN_REDUCTION),              0, ":CsnArr;",                 ":CsnArr;kk",                    (SUBR) csnarray_var_k_init,                  (SUBR) csnarray_var_k,                  (SUBR) csnarray_reduction_deinit,        NULL, 0 },
    { "csnadd",                S(CSN_BINOP_HH),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;",              (SUBR) csnarray_add_hh,                      NULL,                                   (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csnadd.k",              S(CSN_BINOP_HH),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;k",             (SUBR) csnarray_add_hh_k_init,               (SUBR) csnarray_add_hh_k,               (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csnadd.hs",             S(CSN_BINOP_HS),               0, ":CsnArr;",                 ":CsnArr;i",                     (SUBR) csnarray_add_hs,                      NULL,                                   (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csnadd.hs.c",           S(CSN_BINOPCOMPLEX_HS),        0, ":CsnArr;",                 ":CsnArr;:Complex;",             (SUBR) csnarray_addcomp_hs,                  NULL,                                   (SUBR) csnarray_opbincomp_deinit,        NULL, 0 },
    { "csnadd.hs.k",           S(CSN_BINOP_HS),               0, ":CsnArr;",                 ":CsnArr;kP",                    (SUBR) csnarray_add_hs_k_init,               (SUBR) csnarray_add_hs_k,               (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csnadd.hs.c.k",         S(CSN_BINOPCOMPLEX_HS),        0, ":CsnArr;",                 ":CsnArr;:Complex;k",            (SUBR) csnarray_addcomp_hs_k_init,           (SUBR) csnarray_addcomp_hs_k,           (SUBR) csnarray_opbincomp_deinit,        NULL, 0 },
    { "csnsubtract.hh",        S(CSN_BINOP_HH),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;",              (SUBR) csnarray_subtract_hh,                 NULL,                                   (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csnsubtract.hh.k",      S(CSN_BINOP_HH),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;k",             (SUBR) csnarray_subtract_hh_k_init,          (SUBR) csnarray_subtract_hh_k,          (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csnsubtract.hs",        S(CSN_BINOP_HS),               0, ":CsnArr;",                 ":CsnArr;i",                     (SUBR) csnarray_subtract_hs,                 NULL,                                   (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csnsubtract.hs.c",      S(CSN_BINOPCOMPLEX_HS),        0, ":CsnArr;",                 ":CsnArr;:Complex;",             (SUBR) csnarray_subtractcomp_hs,             NULL,                                   (SUBR) csnarray_opbincomp_deinit,        NULL, 0 },
    { "csnsubtract.sh",        S(CSN_BINOP_SH),               0, ":CsnArr;",                 "i:CsnArr;",                     (SUBR) csnarray_subtract_sh,                 NULL,                                   (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csnsubtract.sh.c",      S(CSN_BINOPCOMPLEX_SH),        0, ":CsnArr;",                 ":Complex;:CsnArr;",             (SUBR) csnarray_subtractcomp_sh,             NULL,                                   (SUBR) csnarray_opbincomp_deinit,        NULL, 0 },
    { "csnsubtract.hs.k",      S(CSN_BINOP_HS),               0, ":CsnArr;",                 ":CsnArr;kP",                    (SUBR) csnarray_subtract_hs_k_init,          (SUBR) csnarray_subtract_hs_k,          (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csnsubtract.hs.c.k",    S(CSN_BINOPCOMPLEX_HS),        0, ":CsnArr;",                 ":CsnArr;:Complex;k",            (SUBR) csnarray_subtractcomp_hs_k_init,      (SUBR) csnarray_subtractcomp_hs_k,      (SUBR) csnarray_opbincomp_deinit,        NULL, 0 },
    { "csnsubtract.sh.k",      S(CSN_BINOP_SH),               0, ":CsnArr;",                 "k:CsnArr;P",                    (SUBR) csnarray_subtract_sh_k_init,          (SUBR) csnarray_subtract_sh_k,          (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csnsubtract.sh.c.k",    S(CSN_BINOPCOMPLEX_SH),        0, ":CsnArr;",                 ":Complex;:CsnArr;k",            (SUBR) csnarray_subtractcomp_sh_k_init,      (SUBR) csnarray_subtractcomp_sh_k,      (SUBR) csnarray_opbincomp_deinit,        NULL, 0 },
    { "csnmul.hh",             S(CSN_BINOP_HH),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;",              (SUBR) csnarray_mul_hh,                      NULL,                                   (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csnmul.hh.k",           S(CSN_BINOP_HH),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;k",             (SUBR) csnarray_mul_hh_k_init,               (SUBR) csnarray_mul_hh_k,               (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csnmul.hs",             S(CSN_BINOP_HS),               0, ":CsnArr;",                 ":CsnArr;i",                     (SUBR) csnarray_mul_hs,                      NULL,                                   (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csnmul.hs.c",           S(CSN_BINOPCOMPLEX_HS),        0, ":CsnArr;",                 ":CsnArr;:Complex;",             (SUBR) csnarray_mulcomp_hs,                  NULL,                                   (SUBR) csnarray_opbincomp_deinit,        NULL, 0 },
    { "csnmul.hs.k",           S(CSN_BINOP_HS),               0, ":CsnArr;",                 ":CsnArr;kP",                    (SUBR) csnarray_mul_hs_k_init,               (SUBR) csnarray_mul_hs_k,               (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csnmul.hs.c.k",         S(CSN_BINOPCOMPLEX_HS),        0, ":CsnArr;",                 ":CsnArr;:Complex;k",            (SUBR) csnarray_mulcomp_hs_k_init,           (SUBR) csnarray_mulcomp_hs_k,           (SUBR) csnarray_opbincomp_deinit,        NULL, 0 },
    { "csndiv.hh",             S(CSN_BINOP_HH),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;",              (SUBR) csnarray_div_hh,                      NULL,                                   (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csndiv.hh.k",           S(CSN_BINOP_HH),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;k",             (SUBR) csnarray_div_hh_k_init,               (SUBR) csnarray_div_hh_k,               (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csndiv.hs",             S(CSN_BINOP_HS),               0, ":CsnArr;",                 ":CsnArr;i",                     (SUBR) csnarray_div_hs,                      NULL,                                   (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csndiv.sh",             S(CSN_BINOP_SH),               0, ":CsnArr;",                 "i:CsnArr;",                     (SUBR) csnarray_div_sh,                      NULL,                                   (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csndiv.hs.c",           S(CSN_BINOPCOMPLEX_HS),        0, ":CsnArr;",                 ":CsnArr;:Complex;",             (SUBR) csnarray_divcomp_hs,                  NULL,                                   (SUBR) csnarray_opbincomp_deinit,        NULL, 0 },
    { "csndiv.sh.c",           S(CSN_BINOPCOMPLEX_SH),        0, ":CsnArr;",                 ":Complex;:CsnArr;",             (SUBR) csnarray_divcomp_sh,                  NULL,                                   (SUBR) csnarray_opbincomp_deinit,        NULL, 0 },
    { "csndiv.hs.k",           S(CSN_BINOP_HS),               0, ":CsnArr;",                 ":CsnArr;kP",                    (SUBR) csnarray_div_hs_k_init,               (SUBR) csnarray_div_hs_k,               (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csndiv.sh.k",           S(CSN_BINOP_SH),               0, ":CsnArr;",                 "k:CsnArr;P",                    (SUBR) csnarray_div_sh_k_init,               (SUBR) csnarray_div_sh_k,               (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csndiv.hs.c.k",         S(CSN_BINOPCOMPLEX_HS),        0, ":CsnArr;",                 ":CsnArr;:Complex;k",            (SUBR) csnarray_divcomp_hs_k_init,           (SUBR) csnarray_divcomp_hs_k,           (SUBR) csnarray_opbincomp_deinit,        NULL, 0 },
    { "csndiv.sh.c.k",         S(CSN_BINOPCOMPLEX_SH),        0, ":CsnArr;",                 ":Complex;:CsnArr;k",            (SUBR) csnarray_divcomp_sh_k_init,           (SUBR) csnarray_divcomp_sh_k,           (SUBR) csnarray_opbincomp_deinit,        NULL, 0 },
    { "csnpow.hh",             S(CSN_BINOP_HH),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;",              (SUBR) csnarray_pow_hh,                      NULL,                                   (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csnpow.hh.k",           S(CSN_BINOP_HH),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;k",             (SUBR) csnarray_pow_hh_k_init,               (SUBR) csnarray_pow_hh_k,               (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csnpow.hs",             S(CSN_BINOP_HS),               0, ":CsnArr;",                 ":CsnArr;i",                     (SUBR) csnarray_pow_hs,                      NULL,                                   (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csnpow.sh",             S(CSN_BINOP_SH),               0, ":CsnArr;",                 "i:CsnArr;",                     (SUBR) csnarray_pow_sh,                      NULL,                                   (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csnpow.hs.c",           S(CSN_BINOPCOMPLEX_HS),        0, ":CsnArr;",                 ":CsnArr;:Complex;",             (SUBR) csnarray_powcomp_hs,                  NULL,                                   (SUBR) csnarray_opbincomp_deinit,        NULL, 0 },
    { "csnpow.sh.c",           S(CSN_BINOPCOMPLEX_SH),        0, ":CsnArr;",                 ":Complex;:CsnArr;",             (SUBR) csnarray_powcomp_sh,                  NULL,                                   (SUBR) csnarray_opbincomp_deinit,        NULL, 0 },
    { "csnpow.hs.k",           S(CSN_BINOP_HS),               0, ":CsnArr;",                 ":CsnArr;kP",                    (SUBR) csnarray_pow_hs_k_init,               (SUBR) csnarray_pow_hs_k,               (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csnpow.sh.k",           S(CSN_BINOP_SH),               0, ":CsnArr;",                 "k:CsnArr;P",                    (SUBR) csnarray_pow_sh_k_init,               (SUBR) csnarray_pow_sh_k,               (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csnpow.hs.c.k",         S(CSN_BINOPCOMPLEX_HS),        0, ":CsnArr;",                 ":CsnArr;:Complex;k",            (SUBR) csnarray_powcomp_hs_k_init,           (SUBR) csnarray_powcomp_hs_k,           (SUBR) csnarray_opbincomp_deinit,        NULL, 0 },
    { "csnpow.sh.c.k",         S(CSN_BINOPCOMPLEX_SH),        0, ":CsnArr;",                 ":Complex;:CsnArr;k",            (SUBR) csnarray_powcomp_sh_k_init,           (SUBR) csnarray_powcomp_sh_k,           (SUBR) csnarray_opbincomp_deinit,        NULL, 0 },
    { "csnlog.hh",             S(CSN_BINOP_HH),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;",              (SUBR) csnarray_log_hh,                      NULL,                                   (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csnlog.hh.k",           S(CSN_BINOP_HH),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;k",             (SUBR) csnarray_log_hh_k_init,               (SUBR) csnarray_log_hh_k,               (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csnlog.hs",             S(CSN_BINOP_HS),               0, ":CsnArr;",                 ":CsnArr;i",                     (SUBR) csnarray_log_hs,                      NULL,                                   (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csnlog.sh",             S(CSN_BINOP_SH),               0, ":CsnArr;",                 "i:CsnArr;",                     (SUBR) csnarray_log_sh,                      NULL,                                   (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csnlog.hs.c",           S(CSN_BINOPCOMPLEX_HS),        0, ":CsnArr;",                 ":CsnArr;:Complex;",             (SUBR) csnarray_logcomp_hs,                  NULL,                                   (SUBR) csnarray_opbincomp_deinit,        NULL, 0 },
    { "csnlog.sh.c",           S(CSN_BINOPCOMPLEX_SH),        0, ":CsnArr;",                 ":Complex;:CsnArr;",             (SUBR) csnarray_logcomp_sh,                  NULL,                                   (SUBR) csnarray_opbincomp_deinit,        NULL, 0 },
    { "csnlog.hs.k",           S(CSN_BINOP_HS),               0, ":CsnArr;",                 ":CsnArr;kP",                    (SUBR) csnarray_log_hs_k_init,               (SUBR) csnarray_log_hs_k,               (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csnlog.sh.k",           S(CSN_BINOP_SH),               0, ":CsnArr;",                 "k:CsnArr;P",                    (SUBR) csnarray_log_sh_k_init,               (SUBR) csnarray_log_sh_k,               (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csnlog.hs.c.k",         S(CSN_BINOPCOMPLEX_HS),        0, ":CsnArr;",                 ":CsnArr;:Complex;k",            (SUBR) csnarray_logcomp_hs_k_init,           (SUBR) csnarray_logcomp_hs_k,           (SUBR) csnarray_opbincomp_deinit,        NULL, 0 },
    { "csnlog.sh.c.k",         S(CSN_BINOPCOMPLEX_SH),        0, ":CsnArr;",                 ":Complex;:CsnArr;k",            (SUBR) csnarray_logcomp_sh_k_init,           (SUBR) csnarray_logcomp_sh_k,           (SUBR) csnarray_opbincomp_deinit,        NULL, 0 },
    { "csnabs",                S(CSN_UNARYOP),                0, ":CsnArr;",                 ":CsnArr;",                      (SUBR) csnarray_abs,                         NULL,                                   (SUBR) csnarray_opunary_deinit,          NULL, 0 },
    { "csnexp",                S(CSN_UNARYOP),                0, ":CsnArr;",                 ":CsnArr;",                      (SUBR) csnarray_exp,                         NULL,                                   (SUBR) csnarray_opunary_deinit,          NULL, 0 },
    { "csnsqrt",               S(CSN_UNARYOP),                0, ":CsnArr;",                 ":CsnArr;",                      (SUBR) csnarray_sqrt,                        NULL,                                   (SUBR) csnarray_opunary_deinit,          NULL, 0 },
    { "csncbrt",               S(CSN_UNARYOP),                0, ":CsnArr;",                 ":CsnArr;",                      (SUBR) csnarray_cbrt,                        NULL,                                   (SUBR) csnarray_opunary_deinit,          NULL, 0 },
    { "csnsin",                S(CSN_UNARYOP),                0, ":CsnArr;",                 ":CsnArr;",                      (SUBR) csnarray_sin,                         NULL,                                   (SUBR) csnarray_opunary_deinit,          NULL, 0 },
    { "csncos",                S(CSN_UNARYOP),                0, ":CsnArr;",                 ":CsnArr;",                      (SUBR) csnarray_cos,                         NULL,                                   (SUBR) csnarray_opunary_deinit,          NULL, 0 },
    { "csntan",                S(CSN_UNARYOP),                0, ":CsnArr;",                 ":CsnArr;",                      (SUBR) csnarray_tan,                         NULL,                                   (SUBR) csnarray_opunary_deinit,          NULL, 0 },
    { "csnasin",               S(CSN_UNARYOP),                0, ":CsnArr;",                 ":CsnArr;",                      (SUBR) csnarray_asin,                        NULL,                                   (SUBR) csnarray_opunary_deinit,          NULL, 0 },
    { "csnacos",               S(CSN_UNARYOP),                0, ":CsnArr;",                 ":CsnArr;",                      (SUBR) csnarray_acos,                        NULL,                                   (SUBR) csnarray_opunary_deinit,          NULL, 0 },
    { "csnatan",               S(CSN_UNARYOP),                0, ":CsnArr;",                 ":CsnArr;",                      (SUBR) csnarray_atan,                        NULL,                                   (SUBR) csnarray_opunary_deinit,          NULL, 0 },
    { "csnsinh",               S(CSN_UNARYOP),                0, ":CsnArr;",                 ":CsnArr;",                      (SUBR) csnarray_sinh,                        NULL,                                   (SUBR) csnarray_opunary_deinit,          NULL, 0 },
    { "csncosh",               S(CSN_UNARYOP),                0, ":CsnArr;",                 ":CsnArr;",                      (SUBR) csnarray_cosh,                        NULL,                                   (SUBR) csnarray_opunary_deinit,          NULL, 0 },
    { "csntanh",               S(CSN_UNARYOP),                0, ":CsnArr;",                 ":CsnArr;",                      (SUBR) csnarray_tanh,                        NULL,                                   (SUBR) csnarray_opunary_deinit,          NULL, 0 },
    { "csnasinh",              S(CSN_UNARYOP),                0, ":CsnArr;",                 ":CsnArr;",                      (SUBR) csnarray_asinh,                       NULL,                                   (SUBR) csnarray_opunary_deinit,          NULL, 0 },
    { "csnacosh",              S(CSN_UNARYOP),                0, ":CsnArr;",                 ":CsnArr;",                      (SUBR) csnarray_acosh,                       NULL,                                   (SUBR) csnarray_opunary_deinit,          NULL, 0 },
    { "csnatanh",              S(CSN_UNARYOP),                0, ":CsnArr;",                 ":CsnArr;",                      (SUBR) csnarray_atanh,                       NULL,                                   (SUBR) csnarray_opunary_deinit,          NULL, 0 },
    { "csnsign",               S(CSN_UNARYOP),                0, ":CsnArr;",                 ":CsnArr;",                      (SUBR) csnarray_sign,                        NULL,                                   (SUBR) csnarray_opunary_deinit,          NULL, 0 },
    { "csnabs.k",              S(CSN_UNARYOP),                0, ":CsnArr;",                 ":CsnArr;k",                     (SUBR) csnarray_abs,                         (SUBR) csnarray_abs_k,                  (SUBR) csnarray_opunary_deinit,          NULL, 0 },
    { "csnexp.k",              S(CSN_UNARYOP),                0, ":CsnArr;",                 ":CsnArr;k",                     (SUBR) csnarray_exp,                         (SUBR) csnarray_exp_k,                  (SUBR) csnarray_opunary_deinit,          NULL, 0 },
    { "csnsqrt.k",             S(CSN_UNARYOP),                0, ":CsnArr;",                 ":CsnArr;k",                     (SUBR) csnarray_sqrt,                        (SUBR) csnarray_sqrt_k,                 (SUBR) csnarray_opunary_deinit,          NULL, 0 },
    { "csncbrt.k",             S(CSN_UNARYOP),                0, ":CsnArr;",                 ":CsnArr;k",                     (SUBR) csnarray_cbrt,                        (SUBR) csnarray_cbrt_k,                 (SUBR) csnarray_opunary_deinit,          NULL, 0 },
    { "csnsin.k",              S(CSN_UNARYOP),                0, ":CsnArr;",                 ":CsnArr;k",                     (SUBR) csnarray_sin,                         (SUBR) csnarray_sin_k,                  (SUBR) csnarray_opunary_deinit,          NULL, 0 },
    { "csncos.k",              S(CSN_UNARYOP),                0, ":CsnArr;",                 ":CsnArr;k",                     (SUBR) csnarray_cos,                         (SUBR) csnarray_cos_k,                  (SUBR) csnarray_opunary_deinit,          NULL, 0 },
    { "csntan.k",              S(CSN_UNARYOP),                0, ":CsnArr;",                 ":CsnArr;k",                     (SUBR) csnarray_tan,                         (SUBR) csnarray_tan_k,                  (SUBR) csnarray_opunary_deinit,          NULL, 0 },
    { "csnasin.k",             S(CSN_UNARYOP),                0, ":CsnArr;",                 ":CsnArr;k",                     (SUBR) csnarray_asin,                        (SUBR) csnarray_asin_k,                 (SUBR) csnarray_opunary_deinit,          NULL, 0 },
    { "csnacos.k",             S(CSN_UNARYOP),                0, ":CsnArr;",                 ":CsnArr;k",                     (SUBR) csnarray_acos,                        (SUBR) csnarray_acos_k,                 (SUBR) csnarray_opunary_deinit,          NULL, 0 },
    { "csnatan.k",             S(CSN_UNARYOP),                0, ":CsnArr;",                 ":CsnArr;k",                     (SUBR) csnarray_atan,                        (SUBR) csnarray_atan_k,                 (SUBR) csnarray_opunary_deinit,          NULL, 0 },
    { "csnsinh.k",             S(CSN_UNARYOP),                0, ":CsnArr;",                 ":CsnArr;k",                     (SUBR) csnarray_sinh,                        (SUBR) csnarray_sinh_k,                 (SUBR) csnarray_opunary_deinit,          NULL, 0 },
    { "csncosh.k",             S(CSN_UNARYOP),                0, ":CsnArr;",                 ":CsnArr;k",                     (SUBR) csnarray_cosh,                        (SUBR) csnarray_cosh_k,                 (SUBR) csnarray_opunary_deinit,          NULL, 0 },
    { "csntanh.k",             S(CSN_UNARYOP),                0, ":CsnArr;",                 ":CsnArr;k",                     (SUBR) csnarray_tanh,                        (SUBR) csnarray_tanh_k,                 (SUBR) csnarray_opunary_deinit,          NULL, 0 },
    { "csnasinh.k",            S(CSN_UNARYOP),                0, ":CsnArr;",                 ":CsnArr;k",                     (SUBR) csnarray_asinh,                       (SUBR) csnarray_asinh_k,                (SUBR) csnarray_opunary_deinit,          NULL, 0 },
    { "csnacosh.k",            S(CSN_UNARYOP),                0, ":CsnArr;",                 ":CsnArr;k",                     (SUBR) csnarray_acosh,                       (SUBR) csnarray_acosh_k,                (SUBR) csnarray_opunary_deinit,          NULL, 0 },
    { "csnatanh.k",            S(CSN_UNARYOP),                0, ":CsnArr;",                 ":CsnArr;k",                     (SUBR) csnarray_atanh,                       (SUBR) csnarray_atanh_k,                (SUBR) csnarray_opunary_deinit,          NULL, 0 },
    { "csnsign.k",             S(CSN_UNARYOP),                0, ":CsnArr;",                 ":CsnArr;k",                     (SUBR) csnarray_sign,                        (SUBR) csnarray_sign_k,                 (SUBR) csnarray_opunary_deinit,          NULL, 0 },
    { "csndot",                S(CSN_BINOP_HH),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;",              (SUBR) csnarray_dot,                         NULL,                                   (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csndot.k",              S(CSN_BINOP_HH),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;k",             (SUBR) csnarray_dot_k_init,                  (SUBR) csnarray_dot_k,                  (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csndot.s",              S(CSN_BINOP_HH_SCALAR),        0, "i",                        ":CsnArr;:CsnArr;",              (SUBR) csnarray_dot_scalar,                  NULL,                                   NULL,                                    NULL, 0 },
    { "csndot.s.k",            S(CSN_BINOP_HH_SCALAR),        0, "k",                        ":CsnArr;:CsnArr;k",             (SUBR) csnarray_dot_scalar,                  (SUBR) csnarray_dot_scalar_k,           NULL,                                    NULL, 0 },
    { "csndot.s.c",            S(CSN_BINOPCOMPLEX_HH_SCALAR), 0, ":Complex;",                ":CsnArr;:CsnArr;",              (SUBR) csnarray_dotcomp_scalar,              NULL,                                   NULL,                                    NULL, 0 },
    { "csndot.s.c.k",          S(CSN_BINOPCOMPLEX_HH_SCALAR), 0, ":Complex;",                ":CsnArr;:CsnArr;k",             (SUBR) csnarray_dotcomp_scalar,              (SUBR) csnarray_dotcomp_scalar_k,       NULL,                                    NULL, 0 },
    { "csninner",              S(CSN_BINOP_HH),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;",              (SUBR) csnarray_inner,                       NULL,                                   (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csninner.k",            S(CSN_BINOP_HH),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;k",             (SUBR) csnarray_inner_k_init,                (SUBR) csnarray_inner_k,                (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csninner.s",            S(CSN_BINOP_HH_SCALAR),        0, "i",                        ":CsnArr;:CsnArr;",              (SUBR) csnarray_inner_scalar,                NULL,                                   NULL,                                    NULL, 0 },
    { "csninner.s.k",          S(CSN_BINOP_HH_SCALAR),        0, "k",                        ":CsnArr;:CsnArr;k",             (SUBR) csnarray_inner_scalar,                (SUBR) csnarray_inner_scalar_k,         NULL,                                    NULL, 0 },
    { "csninner.s.c",          S(CSN_BINOPCOMPLEX_HH_SCALAR), 0, ":Complex;",                ":CsnArr;:CsnArr;",              (SUBR) csnarray_innercomp_scalar,            NULL,                                   NULL,                                    NULL, 0 },
    { "csninner.s.c.k",        S(CSN_BINOPCOMPLEX_HH_SCALAR), 0, ":Complex;",                ":CsnArr;:CsnArr;k",             (SUBR) csnarray_innercomp_scalar,            (SUBR) csnarray_innercomp_scalar_k,     NULL,                                    NULL, 0 },
    { "csnouter",              S(CSN_BINOP_HH),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;",              (SUBR) csnarray_outer,                       NULL,                                   (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csnouter.k",            S(CSN_BINOP_HH),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;k",             (SUBR) csnarray_outer_k_init,                (SUBR) csnarray_outer_k,                (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csnnorm",               S(CSN_NORM_REDUCTION),         0, ":CsnArr;",                 ":CsnArr;ip",                    (SUBR) csnarray_norm,                        NULL,                                   (SUBR) csnarray_norm_deinit,             NULL, 0 },
    { "csnnorm.k",             S(CSN_NORM_REDUCTION),         0, ":CsnArr;",                 ":CsnArr;kkk",                   (SUBR) csnarray_norm_k_init,                 (SUBR) csnarray_norm_k,                 (SUBR) csnarray_norm_deinit,             NULL, 0 },
    { "csnnorm.s",             S(CSN_NORM_REDUCTION_SCALAR),  0, "i",                        ":CsnArr;p",                     (SUBR) csnarray_norm_scalar,                 NULL,                                   NULL,                                    NULL, 0 },
    { "csnnorm.s.k",           S(CSN_NORM_REDUCTION_SCALAR),  0, "k",                        ":CsnArr;kP",                    (SUBR) csnarray_norm_scalar_k_init,          (SUBR) csnarray_norm_scalar_k,          NULL,                                    NULL, 0 },
    { "csnnormalize",          S(CSN_UNARYOP_AX),             0, ":CsnArr;",                 ":CsnArr;",                      (SUBR) csnarray_normalize,                   NULL,                                   (SUBR) csnarray_opunary_ax_deinit,       NULL, 0 },
    { "csnnormalize",          S(CSN_UNARYOP_AX),             0, ":CsnArr;",                 ":CsnArr;i",                     (SUBR) csnarray_normalize,                   NULL,                                   (SUBR) csnarray_opunary_ax_deinit,       NULL, 0 },
    { "csnnormalize",          S(CSN_UNARYOP_AX),             0, ":CsnArr;",                 ":CsnArr;ii",                    (SUBR) csnarray_normalize,                   NULL,                                   (SUBR) csnarray_opunary_ax_deinit,       NULL, 0 },
    { "csnnormalize.in",       S(CSN_UNARYOP_AX_IN),          0, "",                         ":CsnArr;",                      (SUBR) csnarray_normalize_in,                NULL,                                   NULL,                                    NULL, 0 },
    { "csnnormalize.in",       S(CSN_UNARYOP_AX_IN),          0, "",                         ":CsnArr;i",                     (SUBR) csnarray_normalize_in,                NULL,                                   NULL,                                    NULL, 0 },
    { "csnnormalize.in",       S(CSN_UNARYOP_AX_IN),          0, "",                         ":CsnArr;ii",                    (SUBR) csnarray_normalize_in,                NULL,                                   NULL,                                    NULL, 0 },
    { "csnnormalize.k",        S(CSN_UNARYOP_AX),             0, ":CsnArr;",                 ":CsnArr;kP",                    (SUBR) csnarray_normalize_k_init,            (SUBR) csnarray_normalize_k,            (SUBR) csnarray_opunary_ax_deinit,       NULL, 0 },
    { "csnnormalize.k",        S(CSN_UNARYOP_AX),             0, ":CsnArr;",                 ":CsnArr;kkk",                   (SUBR) csnarray_normalize_k_init,            (SUBR) csnarray_normalize_k,            (SUBR) csnarray_opunary_ax_deinit,       NULL, 0 },
    { "csnnormalize.in.k",     S(CSN_UNARYOP_AX_IN),          0, "",                         ":CsnArr;kP",                    (SUBR) csnarray_normalize_in_k_init,         (SUBR) csnarray_normalize_in_k,         (SUBR) opunary_ax_in_k_deinit,           NULL, 0 },
    { "csnnormalize.in.k",     S(CSN_UNARYOP_AX_IN),          0, "",                         ":CsnArr;kkk",                   (SUBR) csnarray_normalize_in_k_init,         (SUBR) csnarray_normalize_in_k,         (SUBR) opunary_ax_in_k_deinit,           NULL, 0 },
    { "csnpairdist",           S(CSN_BINOP_HH),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;",              (SUBR) csnarray_pair_distance,               NULL,                                   (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csnpairdist.k",         S(CSN_BINOP_HH),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;k",             (SUBR) csnarray_pair_distance_k_init,        (SUBR) csnarray_pair_distance_k,        (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csndist",               S(CSN_BINOP_HH_SCALAR),        0, "i",                        ":CsnArr;:CsnArr;p",             (SUBR) csnarray_distance,                    NULL,                                   NULL,                                    NULL, 0 },
    { "csndist.k",             S(CSN_BINOP_HH_SCALAR),        0, "k",                        ":CsnArr;:CsnArr;kk",            (SUBR) csnarray_distance_k_init,             (SUBR) csnarray_distance_k,             NULL,                                    NULL, 0 },
    { "csnangledist",          S(CSN_BINOP_HH_SCALAR),        0, "i",                        ":CsnArr;:CsnArr;",              (SUBR) csnarray_angle_distance,              NULL,                                   NULL,                                    NULL, 0 },
    { "csnangledist.k",        S(CSN_BINOP_HH_SCALAR),        0, "k",                        ":CsnArr;:CsnArr;k",             (SUBR) csnarray_angle_distance,              (SUBR) csnarray_angle_distance_k,       NULL,                                    NULL, 0 },
    { "csnreflect",            S(CSN_BINOP_HH),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;",              (SUBR) csnarray_reflect,                     NULL,                                   (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csnreflect.k",          S(CSN_BINOP_HH),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;k",             (SUBR) csnarray_reflect_k_init,              (SUBR) csnarray_reflect_k,              (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csndiff",               S(CSN_UNARYOP_AX),             0, ":CsnArr;",                 ":CsnArr;",                      (SUBR) csnarray_diff,                        NULL,                                   (SUBR) csnarray_opunary_ax_deinit,       NULL, 0 },
    { "csndiff",               S(CSN_UNARYOP_AX),             0, ":CsnArr;",                 ":CsnArr;i",                     (SUBR) csnarray_diff,                        NULL,                                   (SUBR) csnarray_opunary_ax_deinit,       NULL, 0 },
    { "csndiff.k",             S(CSN_UNARYOP_AX),             0, ":CsnArr;",                 ":CsnArr;P",                     (SUBR) csnarray_diff_k_init,                 (SUBR) csnarray_diff_k,                 (SUBR) csnarray_opunary_ax_deinit,       NULL, 0 },
    { "csndiff.k",             S(CSN_UNARYOP_AX),             0, ":CsnArr;",                 ":CsnArr;kk",                    (SUBR) csnarray_diff_k_init,                 (SUBR) csnarray_diff_k,                 (SUBR) csnarray_opunary_ax_deinit,       NULL, 0 },
    { "csncumsum",             S(CSN_UNARYOP_AX),             0, ":CsnArr;",                 ":CsnArr;",                      (SUBR) csnarray_cumsum,                      NULL,                                   (SUBR) csnarray_opunary_ax_deinit,       NULL, 0 },
    { "csncumsum",             S(CSN_UNARYOP_AX),             0, ":CsnArr;",                 ":CsnArr;i",                     (SUBR) csnarray_cumsum,                      NULL,                                   (SUBR) csnarray_opunary_ax_deinit,       NULL, 0 },
    { "csncumsum.k",           S(CSN_UNARYOP_AX),             0, ":CsnArr;",                 ":CsnArr;P",                     (SUBR) csnarray_cumsum_k_init,               (SUBR) csnarray_cumsum_k,               (SUBR) csnarray_opunary_ax_deinit,       NULL, 0 },
    { "csncumsum.k",           S(CSN_UNARYOP_AX),             0, ":CsnArr;",                 ":CsnArr;kk",                    (SUBR) csnarray_cumsum_k_init,               (SUBR) csnarray_cumsum_k,               (SUBR) csnarray_opunary_ax_deinit,       NULL, 0 },
    { "csncumprod",            S(CSN_UNARYOP_AX),             0, ":CsnArr;",                 ":CsnArr;",                      (SUBR) csnarray_cumprod,                     NULL,                                   (SUBR) csnarray_opunary_ax_deinit,       NULL, 0 },
    { "csncumprod",            S(CSN_UNARYOP_AX),             0, ":CsnArr;",                 ":CsnArr;i",                     (SUBR) csnarray_cumprod,                     NULL,                                   (SUBR) csnarray_opunary_ax_deinit,       NULL, 0 },
    { "csncumprod.k",          S(CSN_UNARYOP_AX),             0, ":CsnArr;",                 ":CsnArr;P",                     (SUBR) csnarray_cumprod_k_init,              (SUBR) csnarray_cumprod_k,              (SUBR) csnarray_opunary_ax_deinit,       NULL, 0 },
    { "csncumprod.k",          S(CSN_UNARYOP_AX),             0, ":CsnArr;",                 ":CsnArr;kk",                    (SUBR) csnarray_cumprod_k_init,              (SUBR) csnarray_cumprod_k,              (SUBR) csnarray_opunary_ax_deinit,       NULL, 0 },
    { "csnmatmul",             S(CSN_BINOP_HH),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;",              (SUBR) csnarray_matmul,                      NULL,                                   (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csnmatmul.k",           S(CSN_BINOP_HH),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;k",             (SUBR) csnarray_matmul,                      (SUBR) csnarray_matmul_k,               (SUBR) csnarray_opbin_deinit,            NULL, 0 },
    { "csnmatmul.s",           S(CSN_BINOP_HH_SCALAR),        0, "i",                        ":CsnArr;:CsnArr;",              (SUBR) csnarray_matmul_scalar,               NULL,                                   NULL,                                    NULL, 0 },
    { "csnmatmul.s.k",         S(CSN_BINOP_HH_SCALAR),        0, "k",                        ":CsnArr;:CsnArr;k",             (SUBR) csnarray_matmul_scalar,               (SUBR) csnarray_matmul_scalar_k,        NULL,                                    NULL, 0 },
    { "csntrace",              S(CSN_UNARYOP_SCALAR),         0, "i",                        ":CsnArr;",                      (SUBR) csnarray_trace,                       NULL,                                   NULL,                                    NULL, 0 },
    { "csntrace.c",            S(CSN_UNARYOPCOMPLEX_SCALAR),  0, ":Complex;",                ":CsnArr;",                      (SUBR) csnarray_tracecomp,                   NULL,                                   NULL,                                    NULL, 0 },
    { "csntrace.k",            S(CSN_UNARYOP_SCALAR),         0, "k",                        ":CsnArr;P",                     (SUBR) csnarray_trace,                       (SUBR) csnarray_trace_k,                NULL,                                    NULL, 0 },
    { "csntrace.c.k",          S(CSN_UNARYOPCOMPLEX_SCALAR),  0, ":Complex;",                ":CsnArr;k",                     (SUBR) csnarray_tracecomp,                   (SUBR) csnarray_tracecomp_k,            NULL,                                    NULL, 0 },
    { "csndiag",               S(CSN_UNARYOP),                0, ":CsnArr;",                 ":CsnArr;",                      (SUBR) csnarray_diag,                        NULL,                                   (SUBR) csnarray_opunary_deinit,          NULL, 0 },
    { "csndiag.k",             S(CSN_UNARYOP),                0, ":CsnArr;",                 ":CsnArr;k",                     (SUBR) csnarray_diag,                        (SUBR) csnarray_diag_k,                 (SUBR) csnarray_opunary_deinit,          NULL, 0 },
    { "csnmovmean",            S(CSN_MOVSTATS),               0, ":CsnArr;",                 ":CsnArr;i",                     (SUBR) csnarray_movmean,                     NULL,                                   (SUBR) csnarray_movstats_deinit,         NULL, 0 },
    { "csnmovmean",            S(CSN_MOVSTATS),               0, ":CsnArr;",                 ":CsnArr;ii",                    (SUBR) csnarray_movmean,                     NULL,                                   (SUBR) csnarray_movstats_deinit,         NULL, 0 },
    { "csnmovmean.k",          S(CSN_MOVSTATS),               0, ":CsnArr;",                 ":CsnArr;kP",                    (SUBR) csnarray_movmean_k_init,              (SUBR) csnarray_movmean_k,              (SUBR) csnarray_movstats_deinit,         NULL, 0 },
    { "csnmovmean.k",          S(CSN_MOVSTATS),               0, ":CsnArr;",                 ":CsnArr;kkk",                   (SUBR) csnarray_movmean_k_init,              (SUBR) csnarray_movmean_k,              (SUBR) csnarray_movstats_deinit,         NULL, 0 },
    { "csnmovmean.in",         S(CSN_MOVSTATS_IN),            0, "",                         ":CsnArr;i",                     (SUBR) csnarray_movmean_in,                  NULL,                                   NULL,                                    NULL, 0 },
    { "csnmovmean.in",         S(CSN_MOVSTATS_IN),            0, "",                         ":CsnArr;ii",                    (SUBR) csnarray_movmean_in,                  NULL,                                   NULL,                                    NULL, 0 },
    { "csnmovmean.in.k",       S(CSN_MOVSTATS_IN),            0, "",                         ":CsnArr;kP",                    (SUBR) csnarray_movmean_in_k_init,           (SUBR) csnarray_movmean_in_k,           (SUBR) csnarray_movstats_in_k_deinit,    NULL, 0 },
    { "csnmovmean.in.k",       S(CSN_MOVSTATS_IN),            0, "",                         ":CsnArr;kkk",                   (SUBR) csnarray_movmean_in_k_init,           (SUBR) csnarray_movmean_in_k,           (SUBR) csnarray_movstats_in_k_deinit,    NULL, 0 },
    { "csnmovstd",             S(CSN_MOVSTATS),               0, ":CsnArr;",                 ":CsnArr;i",                     (SUBR) csnarray_movstd,                      NULL,                                   (SUBR) csnarray_movstats_deinit,         NULL, 0 },
    { "csnmovstd",             S(CSN_MOVSTATS),               0, ":CsnArr;",                 ":CsnArr;ii",                    (SUBR) csnarray_movstd,                      NULL,                                   (SUBR) csnarray_movstats_deinit,         NULL, 0 },
    { "csnmovstd.k",           S(CSN_MOVSTATS),               0, ":CsnArr;",                 ":CsnArr;kP",                    (SUBR) csnarray_movstd_k_init,               (SUBR) csnarray_movstd_k,               (SUBR) csnarray_movstats_deinit,         NULL, 0 },
    { "csnmovstd.k",           S(CSN_MOVSTATS),               0, ":CsnArr;",                 ":CsnArr;kkk",                   (SUBR) csnarray_movstd_k_init,               (SUBR) csnarray_movstd_k,               (SUBR) csnarray_movstats_deinit,         NULL, 0 },
    { "csnmovstd.in",          S(CSN_MOVSTATS_IN),            0, "",                         ":CsnArr;i",                     (SUBR) csnarray_movstd_in,                   NULL,                                   NULL,                                    NULL, 0 },
    { "csnmovstd.in",          S(CSN_MOVSTATS_IN),            0, "",                         ":CsnArr;ii",                    (SUBR) csnarray_movstd_in,                   NULL,                                   NULL,                                    NULL, 0 },
    { "csnmovstd.in.k",        S(CSN_MOVSTATS_IN),            0, "",                         ":CsnArr;kP",                    (SUBR) csnarray_movstd_in_k_init,            (SUBR) csnarray_movstd_in_k,            (SUBR) csnarray_movstats_in_k_deinit,    NULL, 0 },
    { "csnmovstd.in.k",        S(CSN_MOVSTATS_IN),            0, "",                         ":CsnArr;kkk",                   (SUBR) csnarray_movstd_in_k_init,            (SUBR) csnarray_movstd_in_k,            (SUBR) csnarray_movstats_in_k_deinit,    NULL, 0 },
    { "csnmovvar",             S(CSN_MOVSTATS),               0, ":CsnArr;",                 ":CsnArr;i",                     (SUBR) csnarray_movvar,                      NULL,                                   (SUBR) csnarray_movstats_deinit,         NULL, 0 },
    { "csnmovvar",             S(CSN_MOVSTATS),               0, ":CsnArr;",                 ":CsnArr;ii",                    (SUBR) csnarray_movvar,                      NULL,                                   (SUBR) csnarray_movstats_deinit,         NULL, 0 },
    { "csnmovvar.k",           S(CSN_MOVSTATS),               0, ":CsnArr;",                 ":CsnArr;kP",                    (SUBR) csnarray_movvar_k_init,               (SUBR) csnarray_movvar_k,               (SUBR) csnarray_movstats_deinit,         NULL, 0 },
    { "csnmovvar.k",           S(CSN_MOVSTATS),               0, ":CsnArr;",                 ":CsnArr;kkk",                   (SUBR) csnarray_movvar_k_init,               (SUBR) csnarray_movvar_k,               (SUBR) csnarray_movstats_deinit,         NULL, 0 },
    { "csnmovvar.in",          S(CSN_MOVSTATS_IN),            0, "",                         ":CsnArr;i",                     (SUBR) csnarray_movvar_in,                   NULL,                                   NULL,                                    NULL, 0 },
    { "csnmovvar.in",          S(CSN_MOVSTATS_IN),            0, "",                         ":CsnArr;ii",                    (SUBR) csnarray_movvar_in,                   NULL,                                   NULL,                                    NULL, 0 },
    { "csnmovvar.in.k",        S(CSN_MOVSTATS_IN),            0, "",                         ":CsnArr;kP",                    (SUBR) csnarray_movvar_in_k_init,            (SUBR) csnarray_movvar_in_k,            (SUBR) csnarray_movstats_in_k_deinit,    NULL, 0 },
    { "csnmovvar.in.k",        S(CSN_MOVSTATS_IN),            0, "",                         ":CsnArr;kkk",                   (SUBR) csnarray_movvar_in_k_init,            (SUBR) csnarray_movvar_in_k,            (SUBR) csnarray_movstats_in_k_deinit,    NULL, 0 },
    { "csnreal",               S(CSN_UNARYOP),                0, ":CsnArr;",                 ":CsnArr;",                      (SUBR) csnarray_real,                        NULL,                                   (SUBR) csnarray_opunary_deinit,          NULL, 0 },
    { "csnreal.k",             S(CSN_UNARYOP),                0, ":CsnArr;",                 ":CsnArr;k",                     (SUBR) csnarray_real_k_init,                 (SUBR) csnarray_real_k,                 (SUBR) csnarray_opunary_deinit,          NULL, 0 },
    { "csnimag",               S(CSN_UNARYOP),                0, ":CsnArr;",                 ":CsnArr;",                      (SUBR) csnarray_imag,                        NULL,                                   (SUBR) csnarray_opunary_deinit,          NULL, 0 },
    { "csnimag.k",             S(CSN_UNARYOP),                0, ":CsnArr;",                 ":CsnArr;k",                     (SUBR) csnarray_imag_k_init,                 (SUBR) csnarray_imag_k,                 (SUBR) csnarray_opunary_deinit,          NULL, 0 },
    { "csntoreal",             S(CSN_UNARYOP),                0, ":CsnArr;",                 ":CsnArr;",                      (SUBR) csnarray_complex_to_real,             NULL,                                   (SUBR) csnarray_opunary_deinit,          NULL, 0 },
    { "csntoreal.k",           S(CSN_UNARYOP),                0, ":CsnArr;",                 ":CsnArr;k",                     (SUBR) csnarray_complex_to_real_k_init,      (SUBR) csnarray_complex_to_real_k,      (SUBR) csnarray_opunary_deinit,          NULL, 0 },
    { "csntocomplex",          S(CSN_UNARYOP),                0, ":CsnArr;",                 ":CsnArr;",                      (SUBR) csnarray_real_to_complex,             NULL,                                   (SUBR) csnarray_opunary_deinit,          NULL, 0 },
    { "csntocomplex.k",        S(CSN_UNARYOP),                0, ":CsnArr;",                 ":CsnArr;k",                     (SUBR) csnarray_real_to_complex_k_init,      (SUBR) csnarray_real_to_complex_k,      (SUBR) csnarray_opunary_deinit,          NULL, 0 },
    { "csnconj",               S(CSN_UNARYOP),                0, ":CsnArr;",                 ":CsnArr;",                      (SUBR) csnarray_conj,                        NULL,                                   (SUBR) csnarray_opunary_deinit,          NULL, 0 },
    { "csnconj.k",             S(CSN_UNARYOP),                0, ":CsnArr;",                 ":CsnArr;k",                     (SUBR) csnarray_conj_k_init,                 (SUBR) csnarray_conj_k,                 (SUBR) csnarray_opunary_deinit,          NULL, 0 },
    { "csnangle",              S(CSN_ANGLE),                  0, ":CsnArr;",                 ":CsnArr;",                      (SUBR) csnarray_angle,                       NULL,                                   (SUBR) csnarray_angle_deinit,            NULL, 0 },
    { "csnangle.k",            S(CSN_ANGLE),                  0, ":CsnArr;",                 ":CsnArr;k",                     (SUBR) csnarray_angle_k_init,                (SUBR) csnarray_angle_k,                (SUBR) csnarray_angle_deinit,            NULL, 0 },
    { "csnwrap",               S(CSN_ANGLE),                  0, ":CsnArr;",                 ":CsnArr;i",                     (SUBR) csnarray_wrap_angle,                  NULL,                                   (SUBR) csnarray_angle_deinit,            NULL, 0 },
    { "csnwrap.k",             S(CSN_ANGLE),                  0, ":CsnArr;",                 ":CsnArr;kk",                    (SUBR) csnarray_wrap_angle_k_init,           (SUBR) csnarray_wrap_angle_k,           (SUBR) csnarray_angle_deinit,            NULL, 0 },
    { "csnwrap.in",            S(CSN_ANGLE),                  0, "",                         ":CsnArr;i",                     (SUBR) csnarray_wrap_angle_in,               NULL,                                   NULL,                                    NULL, 0 },
    { "csnwrap.in.k",          S(CSN_ANGLE),                  0, "",                         ":CsnArr;kk",                    (SUBR) csnarray_wrap_angle_in,               (SUBR) csnarray_wrap_angle_in_k,        NULL,                                    NULL, 0 },
    { "csnunwrap",             S(CSN_ANGLE),                  0, ":CsnArr;",                 ":CsnArr;ii",                    (SUBR) csnarray_unwrap_angle,                NULL,                                   (SUBR) csnarray_angle_deinit,            NULL, 0 },
    { "csnunwrap",             S(CSN_ANGLE),                  0, ":CsnArr;",                 ":CsnArr;iii",                   (SUBR) csnarray_unwrap_angle,                NULL,                                   (SUBR) csnarray_angle_deinit,            NULL, 0 },
    { "csnunwrap.k",           S(CSN_ANGLE),                  0, ":CsnArr;",                 ":CsnArr;kkP",                   (SUBR) csnarray_unwrap_angle_k_init,         (SUBR) csnarray_unwrap_angle_k,         (SUBR) csnarray_angle_deinit,            NULL, 0 },
    { "csnunwrap.k",           S(CSN_ANGLE),                  0, ":CsnArr;",                 ":CsnArr;kkkk",                  (SUBR) csnarray_unwrap_angle_k_init,         (SUBR) csnarray_unwrap_angle_k,         (SUBR) csnarray_angle_deinit,            NULL, 0 },
    { "csnunwrap.in",          S(CSN_ANGLE),                  0, "",                         ":CsnArr;ii",                    (SUBR) csnarray_unwrap_angle_in,             NULL,                                   NULL,                                    NULL, 0 },
    { "csnunwrap.in",          S(CSN_ANGLE),                  0, "",                         ":CsnArr;iii",                   (SUBR) csnarray_unwrap_angle_in,             NULL,                                   NULL,                                    NULL, 0 },
    { "csnunwrap.in.k",        S(CSN_ANGLE),                  0, "",                         ":CsnArr;kkP",                   (SUBR) csnarray_unwrap_angle_in_k_init,      (SUBR) csnarray_unwrap_angle_in_k,      NULL,                                    NULL, 0 },
    { "csnunwrap.in.k",        S(CSN_ANGLE),                  0, "",                         ":CsnArr;kkkk",                  (SUBR) csnarray_unwrap_angle_in_k_init,      (SUBR) csnarray_unwrap_angle_in_k,      NULL,                                    NULL, 0 },
    { "csntype",               S(CSN_UNARYOP_SCALAR),         0, "i",                        ":CsnArr;",                      (SUBR) csnarray_type,                        NULL,                                   NULL,                                    NULL, 0 },
    { "csntype.k",             S(CSN_UNARYOP_SCALAR),         0, "k",                        ":CsnArr;P",                     (SUBR) csnarray_type,                        (SUBR) csnarray_type_k,                 NULL,                                    NULL, 0 },
    { "csncopy",               S(CSN_UNARYOP),                0, ":CsnArr;",                 ":CsnArr;",                      (SUBR) csnarray_copy,                        NULL,                                   (SUBR) csnarray_opunary_deinit,          NULL, 0 },
    { "csncopy.k",             S(CSN_UNARYOP),                0, ":CsnArr;",                 ":CsnArr;k",                     (SUBR) csnarray_copy_k_init,                 (SUBR) csnarray_copy_k,                 (SUBR) csnarray_opunary_deinit,          NULL, 0 },
    { "csnreverse",            S(CSN_UNARYOP),                0, ":CsnArr;",                 ":CsnArr;",                      (SUBR) csnarray_reverse,                     NULL,                                   (SUBR) csnarray_opunary_deinit,          NULL, 0 },
    { "csnreverse.k",          S(CSN_UNARYOP),                0, ":CsnArr;",                 ":CsnArr;k",                     (SUBR) csnarray_reverse_k_init,              (SUBR) csnarray_reverse_k,              (SUBR) csnarray_opunary_deinit,          NULL, 0 },
    { "csnreverse.in",         S(CSN_UNARYOP_IN),             0, "",                         ":CsnArr;",                      (SUBR) csnarray_reverse_in,                  NULL,                                   NULL,                                    NULL, 0 },
    { "csnreverse.in.k",       S(CSN_UNARYOP_IN),             0, "",                         ":CsnArr;k",                     (SUBR) csnarray_unaryop_in_k_init,           (SUBR) csnarray_reverse_in_k,           NULL,                                    NULL, 0 },
    { "csntruncate",           S(CSN_TRUNCATE),               0, ":CsnArr;",                 ":CsnArr;i",                     (SUBR) csnarray_truncate,                    NULL,                                   (SUBR) csnarray_truncate_deinit,         NULL, 0 },
    { "csntruncate",           S(CSN_TRUNCATE),               0, ":CsnArr;",                 ":CsnArr;ii",                    (SUBR) csnarray_truncate,                    NULL,                                   (SUBR) csnarray_truncate_deinit,         NULL, 0 },
    { "csntruncate.k",         S(CSN_TRUNCATE),               0, ":CsnArr;",                 ":CsnArr;kP",                    (SUBR) csnarray_truncate_k_init,             (SUBR) csnarray_truncate_k,             (SUBR) csnarray_truncate_deinit,         NULL, 0 },
    { "csntruncate.k",         S(CSN_TRUNCATE),               0, ":CsnArr;",                 ":CsnArr;kkk",                   (SUBR) csnarray_truncate_k_init,             (SUBR) csnarray_truncate_k,             (SUBR) csnarray_truncate_deinit,         NULL, 0 },
    { "csntruncate.in",        S(CSN_TRUNCATE_IN),            0, "",                         ":CsnArr;i",                     (SUBR) csnarray_truncate_in,                 NULL,                                   NULL,                                    NULL, 0 },
    { "csntruncate.in",        S(CSN_TRUNCATE_IN),            0, "",                         ":CsnArr;ii",                    (SUBR) csnarray_truncate_in,                 NULL,                                   NULL,                                    NULL, 0 },
    { "csntruncate.in.k",      S(CSN_TRUNCATE_IN),            0, "",                         ":CsnArr;kP",                    (SUBR) csnarray_truncate_in_k_init,          (SUBR) csnarray_truncate_in_k,          NULL,                                    NULL, 0 },
    { "csntruncate.in.k",      S(CSN_TRUNCATE_IN),            0, "",                         ":CsnArr;kkk",                   (SUBR) csnarray_truncate_in_k_init,          (SUBR) csnarray_truncate_in_k,          NULL,                                    NULL, 0 },
    { "csnresize",             S(CSN_RESIZE),                 0, ":CsnArr;",                 ":CsnArr;i[]",                   (SUBR) csnarray_resize,                      NULL,                                   (SUBR) csnarray_resize_deinit,           NULL, 0 },
    { "csnresize.k",           S(CSN_RESIZE),                 0, ":CsnArr;",                 ":CsnArr;k[]J",                  (SUBR) csnarray_resize_k_init,               (SUBR) csnarray_resize_k,               (SUBR) csnarray_resize_deinit,           NULL, 0 },
    { "csnresize.in",          S(CSN_RESIZE_IN),              0, "",                         ":CsnArr;i[]",                   (SUBR) csnarray_resize_in,                   NULL,                                   NULL,                                    NULL, 0 },
    { "csnresize.in.k",        S(CSN_RESIZE_IN),              0, "",                         ":CsnArr;k[]J",                  (SUBR) csnarray_resize_in_k_init,            (SUBR) csnarray_resize_in_k,            NULL,                                    NULL, 0 },
    { "csnhead",               S(CSN_TRUNCATE),               0, ":CsnArr;",                 ":CsnArr;i",                     (SUBR) csnarray_head,                        NULL,                                   (SUBR) csnarray_truncate_deinit,         NULL, 0 },
    { "csnhead.k",             S(CSN_TRUNCATE),               0, ":CsnArr;",                 ":CsnArr;kP",                    (SUBR) csnarray_head_k_init,                 (SUBR) csnarray_head_k,                 (SUBR) csnarray_truncate_deinit,         NULL, 0 },
    { "csnprint",              S(CSN_SHOW),                   0, "",                         ":CsnArr;",                      (SUBR) csnarray_show,                        NULL,                                   NULL,                                    NULL, 0 },
    { "csnprint.k",            S(CSN_SHOW),                   0, "",                         ":CsnArr;k",                     (SUBR) csnarray_show_k_init,                 (SUBR) csnarray_show_k,                 (SUBR) csnarray_show_k_deinit,           NULL, 0 },
    { "csncompress",           S(CSN_WHERE_HS),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;",              (SUBR) csnarray_compress,                    NULL,                                   (SUBR) csnarray_where_deinit,            NULL, 0 },
    { "csncompress",           S(CSN_WHERE_HS),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;i",             (SUBR) csnarray_compress,                    NULL,                                   (SUBR) csnarray_where_deinit,            NULL, 0 },
    { "csncompress.k",         S(CSN_WHERE_HS),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;P",             (SUBR) csnarray_compress_k_init,             (SUBR) csnarray_compress_k,             (SUBR) csnarray_where_deinit,            NULL, 0 },
    { "csncompress.k",         S(CSN_WHERE_HS),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;ki",            (SUBR) csnarray_compress_k_init,             (SUBR) csnarray_compress_k,             (SUBR) csnarray_where_deinit,            NULL, 0 },
    { "csnselect",             S(CSN_ARGWHERE),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;",              (SUBR) csnarray_select,                      NULL,                                   (SUBR) csnarray_argwhere_deinit,         NULL, 0 },
    { "csnselect.k",           S(CSN_ARGWHERE),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;P",             (SUBR) csnarray_select_k_init,               (SUBR) csnarray_select_k,               (SUBR) csnarray_argwhere_deinit,         NULL, 0 },
    { "csnstack",              S(CSN_STACK),                  0, ":CsnArr;",                 "i*",                            (SUBR) csnarray_stack,                       NULL,                                   (SUBR) csnarray_stack_deinit,            NULL, 0 },
    { "csnstack.k",            S(CSN_STACK_K),                0, ":CsnArr;",                 "kk*",                           (SUBR) csnarray_stack_k_init,                (SUBR) csnarray_stack_k,                (SUBR) csnarray_stack_k_deinit,          NULL, 0 },
    // set-operations
    { "csnlikeset",            S(CSNSET_UNARYOP),             0, ":CsnArr;",                 ":CsnArr;",                      (SUBR) csnarray_likeset,                     NULL,                                   (SUBR) csnarray_likeset_deinit,          NULL, 0 },
    { "csnlikeset.k",          S(CSNSET_UNARYOP),             0, ":CsnArr;",                 ":CsnArr;P",                     (SUBR) csnarray_likeset_k_init,              (SUBR) csnarray_likeset_k,              (SUBR) csnarray_likeset_deinit,          NULL, 0 },
    { "csnunlikeset",          S(CSNSET_UNARYOP_IN),          0, "",                         ":CsnArr;",                      (SUBR) csnarray_unlikeset,                   NULL,                                   NULL,                                    NULL, 0 },
    { "csnunlikeset.k",        S(CSNSET_UNARYOP_IN),          0, "",                         ":CsnArr;P",                     (SUBR) csnarray_unlikeset_k_init,            (SUBR) csnarray_unlikeset_k,            NULL,                                    NULL, 0 },
    { "csnsetinsert",          S(CSNSET_UNARYOP_IN),          0, "",                         ":CsnArr;i",                     (SUBR) csnarray_setinsert,                   NULL,                                   NULL,                                    NULL, 0 },
    { "csnsetinsert.k",        S(CSNSET_UNARYOP_IN),          0, "",                         ":CsnArr;kP",                    (SUBR) csnarray_setinsertremove_k_init,      (SUBR) csnarray_setinsert_k,            NULL,                                    NULL, 0 },
    { "csnsetremove",          S(CSNSET_UNARYOP_IN),          0, "",                         ":CsnArr;i",                     (SUBR) csnarray_setremove,                   NULL,                                   NULL,                                    NULL, 0 },
    { "csnsetremove.k",        S(CSNSET_UNARYOP_IN),          0, "",                         ":CsnArr;kP",                    (SUBR) csnarray_setinsertremove_k_init,      (SUBR) csnarray_setremove_k,            NULL,                                    NULL, 0 },
    { "csnsetcontains",        S(CSNSET_BINARYOP_SCALAR),     0, "i",                        ":CsnArr;i",                     (SUBR) csnarray_setcontains,                 NULL,                                   NULL,                                    NULL, 0 },
    { "csnsetcontains.k",      S(CSNSET_BINARYOP_SCALAR),     0, "k",                        ":CsnArr;kP",                    (SUBR) csnarray_setcontains_k_init,          (SUBR) csnarray_setcontains_k,          NULL,                                    NULL, 0 },
    { "csnsetunion",           S(CSNSET_BINARYOP),            0, ":CsnArr;",                 ":CsnArr;:CsnArr;",              (SUBR) csnarray_setunion,                    NULL,                                   (SUBR) csnarray_set_binaryop_deinit,     NULL, 0 },
    { "csnsetunion.k",         S(CSNSET_BINARYOP),            0, ":CsnArr;",                 ":CsnArr;:CsnArr;P",             (SUBR) csnarray_setunion,                    (SUBR) csnarray_setunion_k,             (SUBR) csnarray_set_binaryop_deinit,     NULL, 0 },
    { "csnsetintersect",       S(CSNSET_BINARYOP),            0, ":CsnArr;",                 ":CsnArr;:CsnArr;",              (SUBR) csnarray_setintersect,                NULL,                                   (SUBR) csnarray_set_binaryop_deinit,     NULL, 0 },
    { "csnsetintersect.k",     S(CSNSET_BINARYOP),            0, ":CsnArr;",                 ":CsnArr;:CsnArr;P",             (SUBR) csnarray_setintersect,                (SUBR) csnarray_setintersect_k,         (SUBR) csnarray_set_binaryop_deinit,     NULL, 0 },
    { "csnsetdiff",            S(CSNSET_BINARYOP),            0, ":CsnArr;",                 ":CsnArr;:CsnArr;",              (SUBR) csnarray_setdiff,                     NULL,                                   (SUBR) csnarray_set_binaryop_deinit,     NULL, 0 },
    { "csnsetdiff.k",          S(CSNSET_BINARYOP),            0, ":CsnArr;",                 ":CsnArr;:CsnArr;P",             (SUBR) csnarray_setdiff,                     (SUBR) csnarray_setdiff_k,              (SUBR) csnarray_set_binaryop_deinit,     NULL, 0 },
    { "csnsetsymdiff",         S(CSNSET_BINARYOP),            0, ":CsnArr;",                 ":CsnArr;:CsnArr;",              (SUBR) csnarray_setsymdiff,                  NULL,                                   (SUBR) csnarray_set_binaryop_deinit,     NULL, 0 },
    { "csnsetsymdiff.k",       S(CSNSET_BINARYOP),            0, ":CsnArr;",                 ":CsnArr;:CsnArr;P",             (SUBR) csnarray_setsymdiff,                  (SUBR) csnarray_setsymdiff_k,           (SUBR) csnarray_set_binaryop_deinit,     NULL, 0 },
    { "csnsetissubset",        S(CSNSET_BINARYOP_PREDICATE),  0, "i",                        ":CsnArr;:CsnArr;",              (SUBR) csnarray_setissubset,                 NULL,                                   (SUBR) csnarray_set_binaryop_p_deinit,   NULL, 0 },
    { "csnsetissubset.k",      S(CSNSET_BINARYOP_PREDICATE),  0, "k",                        ":CsnArr;:CsnArr;P",             (SUBR) csnarray_setissubset,                 (SUBR) csnarray_setissubset_k,          (SUBR) csnarray_set_binaryop_p_deinit,   NULL, 0 },
    { "csnsetissuperset",      S(CSNSET_BINARYOP_PREDICATE),  0, "i",                        ":CsnArr;:CsnArr;",              (SUBR) csnarray_setissuperset,               NULL,                                   (SUBR) csnarray_set_binaryop_p_deinit,   NULL, 0 },
    { "csnsetissuperset.k",    S(CSNSET_BINARYOP_PREDICATE),  0, "k",                        ":CsnArr;:CsnArr;P",             (SUBR) csnarray_setissuperset,               (SUBR) csnarray_setissuperset_k,        (SUBR) csnarray_set_binaryop_p_deinit,   NULL, 0 },
    { "csnsetisdisjoint",      S(CSNSET_BINARYOP_PREDICATE),  0, "i",                        ":CsnArr;:CsnArr;",              (SUBR) csnarray_setisdisjoint,               NULL,                                   (SUBR) csnarray_set_binaryop_p_deinit,   NULL, 0 },
    { "csnsetisdisjoint.k",    S(CSNSET_BINARYOP_PREDICATE),  0, "k",                        ":CsnArr;:CsnArr;P",             (SUBR) csnarray_setisdisjoint,               (SUBR) csnarray_setisdisjoint_k,        (SUBR) csnarray_set_binaryop_p_deinit,   NULL, 0 },
    { "csnsetisequal",         S(CSNSET_BINARYOP_PREDICATE),  0, "i",                        ":CsnArr;:CsnArr;",              (SUBR) csnarray_setisequal,                  NULL,                                   (SUBR) csnarray_set_binaryop_p_deinit,   NULL, 0 },
    { "csnsetisequal.k",       S(CSNSET_BINARYOP_PREDICATE),  0, "k",                        ":CsnArr;:CsnArr;P",             (SUBR) csnarray_setisequal,                  (SUBR) csnarray_setisequal_k,           (SUBR) csnarray_set_binaryop_p_deinit,   NULL, 0 },
    { "csnconvolve1d",         S(CSN_CORRCONV),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;o",             (SUBR) csnarray_convolve1d,                  NULL,                                   (SUBR) csnarray_corrconv_deinit,         NULL, 0 },
    { "csnconvolve1d",         S(CSN_CORRCONV),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;oi",            (SUBR) csnarray_convolve1d,                  NULL,                                   (SUBR) csnarray_corrconv_deinit,         NULL, 0 },
    { "csncorrelate1d",        S(CSN_CORRCONV),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;o",             (SUBR) csnarray_correlate1d,                 NULL,                                   (SUBR) csnarray_corrconv_deinit,         NULL, 0 },
    { "csncorrelate1d",        S(CSN_CORRCONV),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;oi",            (SUBR) csnarray_correlate1d,                 NULL,                                   (SUBR) csnarray_corrconv_deinit,         NULL, 0 },
    { "csnconvolve1d.k",       S(CSN_CORRCONV),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;oP",            (SUBR) csnarray_convolve1d_k_init,           (SUBR) csnarray_convolve1d_k,           (SUBR) csnarray_corrconv_deinit,         NULL, 0 },
    { "csnconvolve1d.k",       S(CSN_CORRCONV),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;oki",           (SUBR) csnarray_convolve1d_k_init,           (SUBR) csnarray_convolve1d_k,           (SUBR) csnarray_corrconv_deinit,         NULL, 0 },
    { "csncorrelate1d.k",      S(CSN_CORRCONV),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;oP",            (SUBR) csnarray_correlate1d_k_init,          (SUBR) csnarray_correlate1d_k,          (SUBR) csnarray_corrconv_deinit,         NULL, 0 },
    { "csncorrelate1d.k",      S(CSN_CORRCONV),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;oki",           (SUBR) csnarray_correlate1d_k_init,          (SUBR) csnarray_correlate1d_k,          (SUBR) csnarray_corrconv_deinit,         NULL, 0 },
    { "csnconvolve",           S(CSN_CORRCONV),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;o",             (SUBR) csnarray_convolve,                    NULL,                                   (SUBR) csnarray_corrconv_deinit,         NULL, 0 },
    { "csncorrelate",          S(CSN_CORRCONV),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;o",             (SUBR) csnarray_correlate,                   NULL,                                   (SUBR) csnarray_corrconv_deinit,         NULL, 0 },
    { "csnconvolve.k",         S(CSN_CORRCONV),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;oP",            (SUBR) csnarray_convolve,                    (SUBR) csnarray_convolve_k,             (SUBR) csnarray_corrconv_deinit,         NULL, 0 },
    { "csncorrelate.k",        S(CSN_CORRCONV),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;oP",            (SUBR) csnarray_correlate,                   (SUBR) csnarray_correlate_k,            (SUBR) csnarray_corrconv_deinit,         NULL, 0 },
    { "csnsolve",              S(CSN_LINALG_SOLVE),           0, ":CsnArr;",                 ":CsnArr;:CsnArr;",              (SUBR) csnarray_solve,                       NULL,                                   (SUBR) csnarray_solve_deinit,            NULL, 0 },
    { "csnsolve.k",            S(CSN_LINALG_SOLVE),           0, ":CsnArr;",                 ":CsnArr;:CsnArr;P",             (SUBR) csnarray_solve,                       (SUBR) csnarray_solve_k,                (SUBR) csnarray_solve_deinit,            NULL, 0 },
    { "csninv",                S(CSN_LINALG_INVERSE),         0, ":CsnArr;",                 ":CsnArr;",                      (SUBR) csnarray_inverse,                     NULL,                                   (SUBR) csnarray_inverse_deinit,          NULL, 0 },
    { "csninv.k",              S(CSN_LINALG_INVERSE),         0, ":CsnArr;",                 ":CsnArr;P",                     (SUBR) csnarray_inverse,                     (SUBR) csnarray_inverse_k,              (SUBR) csnarray_inverse_deinit,          NULL, 0 },
    { "csndet",                S(CSN_LINALG_DET_REAL),        0, "i",                        ":CsnArr;",                      (SUBR) csnarray_determinant_real,            NULL,                                   (SUBR) csnarray_det_real_deinit,         NULL, 0 },
    { "csndet.k",              S(CSN_LINALG_DET_REAL),        0, "k",                        ":CsnArr;P",                     (SUBR) csnarray_determinant_real,            (SUBR) csnarray_determinant_real_k,     (SUBR) csnarray_det_real_deinit,         NULL, 0 },
    { "csndet.c",              S(CSN_LINALG_DET_COMPLEX),     0, ":Complex;",                ":CsnArr;",                      (SUBR) csnarray_determinant_complex,         NULL,                                   (SUBR) csnarray_det_complex_deinit,      NULL, 0 },
    { "csndet.c.k",            S(CSN_LINALG_DET_COMPLEX),     0, ":Complex;",                ":CsnArr;P",                     (SUBR) csnarray_determinant_complex,         (SUBR) csnarray_determinant_complex_k,  (SUBR) csnarray_det_complex_deinit,      NULL, 0 },
    // fft
    { "csnfft",                S(CSN_FFT),                    0, ":CsnArr;",                 ":CsnArr;i",                     (SUBR) csnarray_fft,                         NULL,                                   (SUBR) csnarray_fft_deinit,              NULL, 0 },
    { "csnfft",                S(CSN_FFT),                    0, ":CsnArr;",                 ":CsnArr;ii",                    (SUBR) csnarray_fft,                         NULL,                                   (SUBR) csnarray_fft_deinit,              NULL, 0 },
    { "csnrfft",               S(CSN_FFT),                    0, ":CsnArr;",                 ":CsnArr;i",                     (SUBR) csnarray_rfft,                        NULL,                                   (SUBR) csnarray_fft_deinit,              NULL, 0 },
    { "csnrfft",               S(CSN_FFT),                    0, ":CsnArr;",                 ":CsnArr;ii",                    (SUBR) csnarray_rfft,                        NULL,                                   (SUBR) csnarray_fft_deinit,              NULL, 0 },
    { "csnifft",               S(CSN_FFT),                    0, ":CsnArr;",                 ":CsnArr;i",                     (SUBR) csnarray_ifft,                        NULL,                                   (SUBR) csnarray_fft_deinit,              NULL, 0 },
    { "csnifft",               S(CSN_FFT),                    0, ":CsnArr;",                 ":CsnArr;ii",                    (SUBR) csnarray_ifft,                        NULL,                                   (SUBR) csnarray_fft_deinit,              NULL, 0 },
    { "csnirfft",              S(CSN_FFT),                    0, ":CsnArr;",                 ":CsnArr;i",                     (SUBR) csnarray_irfft,                       NULL,                                   (SUBR) csnarray_fft_deinit,              NULL, 0 },
    { "csnirfft",              S(CSN_FFT),                    0, ":CsnArr;",                 ":CsnArr;ii",                    (SUBR) csnarray_irfft,                       NULL,                                   (SUBR) csnarray_fft_deinit,              NULL, 0 },
    { "csnfft2",               S(CSN_FFT2),                   0, ":CsnArr;",                 ":CsnArr;ii",                    (SUBR) csnarray_fft2,                        NULL,                                   (SUBR) csnarray_fft2_deinit,             NULL, 0 },
    { "csnrfft2",              S(CSN_FFT2),                   0, ":CsnArr;",                 ":CsnArr;ii",                    (SUBR) csnarray_rfft2,                       NULL,                                   (SUBR) csnarray_fft2_deinit,             NULL, 0 },
    { "csnifft2",              S(CSN_FFT2),                   0, ":CsnArr;",                 ":CsnArr;ii",                    (SUBR) csnarray_ifft2,                       NULL,                                   (SUBR) csnarray_fft2_deinit,             NULL, 0 },
    { "csnirfft2",             S(CSN_FFT2),                   0, ":CsnArr;",                 ":CsnArr;ii",                    (SUBR) csnarray_irfft2,                      NULL,                                   (SUBR) csnarray_fft2_deinit,             NULL, 0 },
    { "csnstft",               S(CSN_STFT),                   0, ":CsnArr;:CsnArr;:CsnArr;", ":CsnArr;iiip",                  (SUBR) csnarray_stft,                        NULL,                                   (SUBR) csnarray_stft_deinit,             NULL, 0 },
    { "csnistft",              S(CSN_ISTFT),                  0, ":CsnArr;:CsnArr;",         ":CsnArr;iiip",                  (SUBR) csnarray_istft,                       NULL,                                   (SUBR) csnarray_istft_deinit,            NULL, 0 },
    { "csnfftfreq",            S(CSN_FFTFREQ),                0, ":CsnArr;",                 "ii",                            (SUBR) csnarray_fftfreq,                     NULL,                                   (SUBR) csnarray_fftfreq_deinit,          NULL, 0 },
    { "csnrfftfreq",           S(CSN_FFTFREQ),                0, ":CsnArr;",                 "ii",                            (SUBR) csnarray_rfftfreq,                    NULL,                                   (SUBR) csnarray_fftfreq_deinit,          NULL, 0 },
    { "csnfftshift",           S(CSN_FFTSHIFT),               0, ":CsnArr;",                 ":CsnArr;",                      (SUBR) csnarray_fftshift,                    NULL,                                   (SUBR) csnarray_fftshift_deinit,         NULL, 0 },
    { "csnfftshift",           S(CSN_FFTSHIFT),               0, ":CsnArr;",                 ":CsnArr;i",                     (SUBR) csnarray_fftshift,                    NULL,                                   (SUBR) csnarray_fftshift_deinit,         NULL, 0 },
    { "csnifftshift",          S(CSN_FFTSHIFT),               0, ":CsnArr;",                 ":CsnArr;",                      (SUBR) csnarray_ifftshift,                   NULL,                                   (SUBR) csnarray_fftshift_deinit,         NULL, 0 },
    { "csnifftshift",          S(CSN_FFTSHIFT),               0, ":CsnArr;",                 ":CsnArr;i",                     (SUBR) csnarray_ifftshift,                   NULL,                                   (SUBR) csnarray_fftshift_deinit,         NULL, 0 },
    { "csnfft.k",              S(CSN_FFT),                    0, ":CsnArr;",                 ":CsnArr;iP",                    (SUBR) csnarray_fft_k_init,                  (SUBR) csnarray_fft_k,                  (SUBR) csnarray_fft_deinit,              NULL, 0 },
    { "csnfft.k",              S(CSN_FFT),                    0, ":CsnArr;",                 ":CsnArr;iki",                   (SUBR) csnarray_fft_k_init,                  (SUBR) csnarray_fft_k,                  (SUBR) csnarray_fft_deinit,              NULL, 0 },
    { "csnrfft.k",             S(CSN_FFT),                    0, ":CsnArr;",                 ":CsnArr;iP",                    (SUBR) csnarray_rfft_k_init,                 (SUBR) csnarray_rfft_k,                 (SUBR) csnarray_fft_deinit,              NULL, 0 },
    { "csnrfft.k",             S(CSN_FFT),                    0, ":CsnArr;",                 ":CsnArr;iki",                   (SUBR) csnarray_rfft_k_init,                 (SUBR) csnarray_rfft_k,                 (SUBR) csnarray_fft_deinit,              NULL, 0 },
    { "csnifft.k",             S(CSN_FFT),                    0, ":CsnArr;",                 ":CsnArr;iP",                    (SUBR) csnarray_ifft_k_init,                 (SUBR) csnarray_ifft_k,                 (SUBR) csnarray_fft_deinit,              NULL, 0 },
    { "csnifft.k",             S(CSN_FFT),                    0, ":CsnArr;",                 ":CsnArr;iki",                   (SUBR) csnarray_ifft_k_init,                 (SUBR) csnarray_ifft_k,                 (SUBR) csnarray_fft_deinit,              NULL, 0 },
    { "csnirfft.k",            S(CSN_FFT),                    0, ":CsnArr;",                 ":CsnArr;iP",                    (SUBR) csnarray_irfft_k_init,                (SUBR) csnarray_irfft_k,                (SUBR) csnarray_fft_deinit,              NULL, 0 },
    { "csnirfft.k",            S(CSN_FFT),                    0, ":CsnArr;",                 ":CsnArr;iki",                   (SUBR) csnarray_irfft_k_init,                (SUBR) csnarray_irfft_k,                (SUBR) csnarray_fft_deinit,              NULL, 0 },
    { "csnstft.k",             S(CSN_STFT),                   0, ":CsnArr;:CsnArr;:CsnArr;", ":CsnArr;iiipP",                 (SUBR) csnarray_stft,                        (SUBR) csnarray_stft_k,                 (SUBR) csnarray_stft_deinit,             NULL, 0 },
    { "csnistft.k",            S(CSN_ISTFT),                  0, ":CsnArr;:CsnArr;",         ":CsnArr;iiipP",                 (SUBR) csnarray_istft,                       (SUBR) csnarray_istft_k,                (SUBR) csnarray_istft_deinit,            NULL, 0 },
    { "csnfftfreq.k",          S(CSN_FFTFREQ),                0, ":CsnArr;",                 "kkP",                           (SUBR) csnarray_fftfreq_k_init,              (SUBR) csnarray_fftfreq_k,              (SUBR) csnarray_fftfreq_deinit,          NULL, 0 },
    { "csnrfftfreq.k",         S(CSN_FFTFREQ),                0, ":CsnArr;",                 "kkP",                           (SUBR) csnarray_rfftfreq_k_init,             (SUBR) csnarray_rfftfreq_k,             (SUBR) csnarray_fftfreq_deinit,          NULL, 0 },
    { "csnfftshift.k",         S(CSN_FFTSHIFT),               0, ":CsnArr;",                 ":CsnArr;P",                     (SUBR) csnarray_fftshift_k_init,             (SUBR) csnarray_fftshift_k,             (SUBR) csnarray_fftshift_deinit,         NULL, 0 },
    { "csnfftshift.k",         S(CSN_FFTSHIFT),               0, ":CsnArr;",                 ":CsnArr;ki",                    (SUBR) csnarray_fftshift_k_init,             (SUBR) csnarray_fftshift_k,             (SUBR) csnarray_fftshift_deinit,         NULL, 0 },
    { "csnifftshift.k",        S(CSN_FFTSHIFT),               0, ":CsnArr;",                 ":CsnArr;P",                     (SUBR) csnarray_ifftshift_k_init,            (SUBR) csnarray_ifftshift_k,            (SUBR) csnarray_fftshift_deinit,         NULL, 0 },
    { "csnifftshift.k",        S(CSN_FFTSHIFT),               0, ":CsnArr;",                 ":CsnArr;ki",                    (SUBR) csnarray_ifftshift_k_init,            (SUBR) csnarray_ifftshift_k,             (SUBR) csnarray_fftshift_deinit,         NULL, 0 },
    { "csnfft2.k",             S(CSN_FFT2),                   0, ":CsnArr;",                 ":CsnArr;iiP",                   (SUBR) csnarray_fft2,                        (SUBR) csnarray_fft2_k,                 (SUBR) csnarray_fft2_deinit,             NULL, 0 },
    { "csnrfft2.k",            S(CSN_FFT2),                   0, ":CsnArr;",                 ":CsnArr;iiP",                   (SUBR) csnarray_rfft2,                       (SUBR) csnarray_rfft2_k,                (SUBR) csnarray_fft2_deinit,             NULL, 0 },
    { "csnifft2.k",            S(CSN_FFT2),                   0, ":CsnArr;",                 ":CsnArr;iiP",                   (SUBR) csnarray_ifft2,                       (SUBR) csnarray_ifft2_k,                (SUBR) csnarray_fft2_deinit,             NULL, 0 },
    { "csnirfft2.k",           S(CSN_FFT2),                   0, ":CsnArr;",                 ":CsnArr;iiP",                   (SUBR) csnarray_irfft2,                      (SUBR) csnarray_irfft2_k,               (SUBR) csnarray_fft2_deinit,             NULL, 0 },
    { "csnfftconvolve1d",      S(CSN_CORRCONV),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;o",             (SUBR) csnarray_fftconvolve1d,               NULL,                                   (SUBR) csnarray_corrconv_deinit,         NULL, 0 },
    { "csnfftconvolve1d",      S(CSN_CORRCONV),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;oi",            (SUBR) csnarray_fftconvolve1d,               NULL,                                   (SUBR) csnarray_corrconv_deinit,         NULL, 0 },
    { "csnfftcorrelate1d",     S(CSN_CORRCONV),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;o",             (SUBR) csnarray_fftcorrelate1d,              NULL,                                   (SUBR) csnarray_corrconv_deinit,         NULL, 0 },
    { "csnfftcorrelate1d",     S(CSN_CORRCONV),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;oi",            (SUBR) csnarray_fftcorrelate1d,              NULL,                                   (SUBR) csnarray_corrconv_deinit,         NULL, 0 },
    { "csnfftconvolve1d.k",    S(CSN_CORRCONV),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;oP",            (SUBR) csnarray_fftconvolve1d_k_init,        (SUBR) csnarray_fftconvolve1d_k,        (SUBR) csnarray_corrconv_deinit,         NULL, 0 },
    { "csnfftconvolve1d.k",    S(CSN_CORRCONV),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;oki",           (SUBR) csnarray_fftconvolve1d_k_init,        (SUBR) csnarray_fftconvolve1d_k,        (SUBR) csnarray_corrconv_deinit,         NULL, 0 },
    { "csnfftcorrelate1d.k",   S(CSN_CORRCONV),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;oP",            (SUBR) csnarray_fftcorrelate1d_k_init,       (SUBR) csnarray_fftcorrelate1d_k,       (SUBR) csnarray_corrconv_deinit,         NULL, 0 },
    { "csnfftcorrelate1d.k",   S(CSN_CORRCONV),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;oki",           (SUBR) csnarray_fftcorrelate1d_k_init,       (SUBR) csnarray_fftcorrelate1d_k,       (SUBR) csnarray_corrconv_deinit,         NULL, 0 },
    { "csnfftconvolve",        S(CSN_CORRCONV),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;o",             (SUBR) csnarray_fftconvolve,                 NULL,                                   (SUBR) csnarray_corrconv_deinit,         NULL, 0 },
    { "csnfftcorrelate",       S(CSN_CORRCONV),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;o",             (SUBR) csnarray_fftcorrelate,                NULL,                                   (SUBR) csnarray_corrconv_deinit,         NULL, 0 },
    { "csnfftconvolve.k",      S(CSN_CORRCONV),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;oP",            (SUBR) csnarray_fftconvolve,                 (SUBR) csnarray_fftconvolve_k,          (SUBR) csnarray_corrconv_deinit,         NULL, 0 },
    { "csnfftcorrelate.k",     S(CSN_CORRCONV),               0, ":CsnArr;",                 ":CsnArr;:CsnArr;oP",            (SUBR) csnarray_fftcorrelate,                (SUBR) csnarray_fftcorrelate_k,         (SUBR) csnarray_corrconv_deinit,         NULL, 0 },
    { "csndctone1d",           S(CSN_DCST),                   0, ":CsnArr;",                 ":CsnArr;",                      (SUBR) csnarray_dct_one,                     NULL,                                   (SUBR) csnarray_dcst_deinit,             NULL, 0 },
    { "csndctone1d",           S(CSN_DCST),                   0, ":CsnArr;",                 ":CsnArr;i",                     (SUBR) csnarray_dct_one,                     NULL,                                   (SUBR) csnarray_dcst_deinit,             NULL, 0 },
    { "csndcttwo1d",           S(CSN_DCST),                   0, ":CsnArr;",                 ":CsnArr;",                      (SUBR) csnarray_dct_two,                     NULL,                                   (SUBR) csnarray_dcst_deinit,             NULL, 0 },
    { "csndcttwo1d",           S(CSN_DCST),                   0, ":CsnArr;",                 ":CsnArr;i",                     (SUBR) csnarray_dct_two,                     NULL,                                   (SUBR) csnarray_dcst_deinit,             NULL, 0 },
    { "csndstone1d",           S(CSN_DCST),                   0, ":CsnArr;",                 ":CsnArr;",                      (SUBR) csnarray_dst_one,                     NULL,                                   (SUBR) csnarray_dcst_deinit,             NULL, 0 },
    { "csndstone1d",           S(CSN_DCST),                   0, ":CsnArr;",                 ":CsnArr;i",                     (SUBR) csnarray_dst_one,                     NULL,                                   (SUBR) csnarray_dcst_deinit,             NULL, 0 },
    { "csndsttwo1d",           S(CSN_DCST),                   0, ":CsnArr;",                 ":CsnArr;",                      (SUBR) csnarray_dst_two,                     NULL,                                   (SUBR) csnarray_dcst_deinit,             NULL, 0 },
    { "csndsttwo1d",           S(CSN_DCST),                   0, ":CsnArr;",                 ":CsnArr;i",                     (SUBR) csnarray_dst_two,                     NULL,                                   (SUBR) csnarray_dcst_deinit,             NULL, 0 },
    { "csndctone1d.k",         S(CSN_DCST),                   0, ":CsnArr;",                 ":CsnArr;P",                     (SUBR) csnarray_dct_one_k_init,              (SUBR) csnarray_dct_one_k,              (SUBR) csnarray_dcst_deinit,             NULL, 0 },
    { "csndctone1d.k",         S(CSN_DCST),                   0, ":CsnArr;",                 ":CsnArr;ki",                    (SUBR) csnarray_dct_one_k_init,              (SUBR) csnarray_dct_one_k,              (SUBR) csnarray_dcst_deinit,             NULL, 0 },
    { "csndcttwo1d.k",         S(CSN_DCST),                   0, ":CsnArr;",                 ":CsnArr;P",                     (SUBR) csnarray_dct_two_k_init,              (SUBR) csnarray_dct_two_k,              (SUBR) csnarray_dcst_deinit,             NULL, 0 },
    { "csndcttwo1d.k",         S(CSN_DCST),                   0, ":CsnArr;",                 ":CsnArr;ki",                    (SUBR) csnarray_dct_two_k_init,              (SUBR) csnarray_dct_two_k,              (SUBR) csnarray_dcst_deinit,             NULL, 0 },
    { "csndstone1d.k",         S(CSN_DCST),                   0, ":CsnArr;",                 ":CsnArr;P",                     (SUBR) csnarray_dst_one_k_init,              (SUBR) csnarray_dst_one_k,              (SUBR) csnarray_dcst_deinit,             NULL, 0 },
    { "csndstone1d.k",         S(CSN_DCST),                   0, ":CsnArr;",                 ":CsnArr;ki",                    (SUBR) csnarray_dst_one_k_init,              (SUBR) csnarray_dst_one_k,              (SUBR) csnarray_dcst_deinit,             NULL, 0 },
    { "csndsttwo1d.k",         S(CSN_DCST),                   0, ":CsnArr;",                 ":CsnArr;P",                     (SUBR) csnarray_dst_two_k_init,              (SUBR) csnarray_dst_two_k,              (SUBR) csnarray_dcst_deinit,             NULL, 0 },
    { "csndsttwo1d.k",         S(CSN_DCST),                   0, ":CsnArr;",                 ":CsnArr;ki",                    (SUBR) csnarray_dst_two_k_init,              (SUBR) csnarray_dst_two_k,              (SUBR) csnarray_dcst_deinit,             NULL, 0 },
    { "csnmfcc",               S(CSN_MFCC),                   0, ":CsnArr;",                 ":CsnArr;iiiiiiii",              (SUBR) csnarray_mfcc,                        NULL,                                   (SUBR) csnarray_mfcc_deinit,             NULL, 0 },
    { "csnmfcc.k",             S(CSN_MFCC),                   0, ":CsnArr;",                 ":CsnArr;iiiiiiiiP",             (SUBR) csnarray_mfcc,                        (SUBR) csnarray_mfcc_k,                 (SUBR) csnarray_mfcc_deinit,             NULL, 0 },
    { "csnmfbank",             S(CSN_MFCC_FBANK),             0, ":CsnArr;",                 "iiiiii",                        (SUBR) csnarray_mfbank,                      NULL,                                   (SUBR) csnarray_mfbank_deinit,           NULL, 0 },
    { "csnmlogfbank",          S(CSN_MFCC_FBANK),             0, ":CsnArr;",                 "iiiiii",                        (SUBR) csnarray_mlogfbank,                   NULL,                                   (SUBR) csnarray_mfbank_deinit,           NULL, 0 },
    { "csnhilbert1d",          S(CSN_HILBERT),                0, ":CsnArr;",                 ":CsnArr;",                      (SUBR) csnarray_hilbert1d,                   NULL,                                   (SUBR) csnarray_hilbert_deinit,          NULL, 0 },
    { "csnhilbert1d",          S(CSN_HILBERT),                0, ":CsnArr;",                 ":CsnArr;i",                     (SUBR) csnarray_hilbert1d,                   NULL,                                   (SUBR) csnarray_hilbert_deinit,          NULL, 0 },
    { "csnhilbert1d.k",        S(CSN_HILBERT),                0, ":CsnArr;",                 ":CsnArr;P",                     (SUBR) csnarray_hilbert1d_k_init,            (SUBR) csnarray_hilbert1d_k,            (SUBR) csnarray_hilbert_deinit,          NULL, 0 },
    { "csnhilbert1d.k",        S(CSN_HILBERT),                0, ":CsnArr;",                 ":CsnArr;ki",                    (SUBR) csnarray_hilbert1d_k_init,            (SUBR) csnarray_hilbert1d_k,            (SUBR) csnarray_hilbert_deinit,          NULL, 0 },
    { "csnhilbert1dr",         S(CSN_HILBERT),                0, ":CsnArr;",                 ":CsnArr;",                      (SUBR) csnarray_hilbert1dr,                  NULL,                                   (SUBR) csnarray_hilbert_deinit,          NULL, 0 },
    { "csnhilbert1dr",         S(CSN_HILBERT),                0, ":CsnArr;",                 ":CsnArr;i",                     (SUBR) csnarray_hilbert1dr,                  NULL,                                   (SUBR) csnarray_hilbert_deinit,          NULL, 0 },
    { "csnhilbert1dr.k",       S(CSN_HILBERT),                0, ":CsnArr;",                 ":CsnArr;P",                     (SUBR) csnarray_hilbert1dr_k_init,           (SUBR) csnarray_hilbert1dr_k,           (SUBR) csnarray_hilbert_deinit,          NULL, 0 },
    { "csnhilbert1dr.k",       S(CSN_HILBERT),                0, ":CsnArr;",                 ":CsnArr;ki",                    (SUBR) csnarray_hilbert1dr_k_init,           (SUBR) csnarray_hilbert1dr_k,           (SUBR) csnarray_hilbert_deinit,          NULL, 0 },
    { "csnhilbert2",           S(CSN_HILBERT2),               0, ":CsnArr;",                 ":CsnArr;",                      (SUBR) csnarray_hilbert2,                    NULL,                                   (SUBR) csnarray_hilbert2_deinit,         NULL, 0 },
    { "csnhilbert2.k",         S(CSN_HILBERT2),               0, ":CsnArr;",                 ":CsnArr;P",                     (SUBR) csnarray_hilbert2,                    (SUBR) csnarray_hilbert2_k,             (SUBR) csnarray_hilbert2_deinit,         NULL, 0 },
    { "csnhilbertmat",         S(CSN_HILBERT_MAT),            0, ":CsnArr;",                 "i",                             (SUBR) csnarray_hilbertmat,                  NULL,                                   (SUBR) csnarray_hilbertmat_deinit,       NULL, 0 },
    { "csnhilbertmat.k",       S(CSN_HILBERT_MAT),            0, ":CsnArr;",                 "kP",                            (SUBR) csnarray_hilbertmat_k_init,           (SUBR) csnarray_hilbertmat_k,           (SUBR) csnarray_hilbertmat_deinit,       NULL, 0 },
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

    /* The table has no NULL-opname sentinel, so the entry count has to be
       passed explicitly: a non-positive n makes Csound scan past its end. */
    return csound->AppendOpcodes(csound, localops, (int32_t) (sizeof(localops) / sizeof(OENTRY)));
}

PUBLIC int32_t csoundModuleDestroy(CSOUND *csound) {
    (void) csound;
    return CSOUND_SUCCESS;
}

PUBLIC int32_t csoundModuleInfo(void) {
    return ((CS_VERSION << 16) + (CS_SUBVER << 8) + (int32_t) sizeof(MYFLT));
}
