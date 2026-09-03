#include "csnum.h"
#include "csnfile.h"
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
#include "arrays.h"



static const char *get_out_name(OPDS *h) {
    if (h == NULL || h->optext == NULL) return "?";
    const ARGLST *out = h->optext->t.outlist;
    if (out == NULL || out->count < 1 || out->arg[0] == NULL) return "?";
    return out->arg[0];
}

static void deinit_scratch(CSOUND *csound, CSN_SCRATCH *scratch) {
    if (scratch->scratch != NULL) csound->Free(csound, scratch->scratch);
    scratch->scratch = NULL;
    scratch->scratch_capacity = 0;
}

static bool is_inarg_i_time(OPDS *h, uint32_t arg_index) {
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
static inline bool SOURCE_HAS_MOVED(const K_DATA *k_data, uint32_t source_handle, const CSN_ARRAY *arr) {
    return k_data->prev_source_handle != source_handle
        || !is_same_array_data_version(&arr->version, &k_data->prev_source_version);
}

/* Closes an in-place write: the array carries a new generation, and this
   opcode records that generation as its own so the next pass recognizes its
   own handiwork and leaves it alone. Both halves belong together — a bump
   without the record makes the opcode redo the work forever, a record without
   the bump hides the write from every other consumer. */
static inline void PUBLISH_INPLACE_WRITE(K_DATA *k_data, uint32_t source_handle, CSN_ARRAY *arr, bool shape_changed, bool ndim_changed, bool itype_changed) {
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
static inline bool CAN_REUSE_LAST_RESULT(const K_DATA *k_data, uint32_t source_handle, const CSN_ARRAY *source_arr, const CSN_ARRAY *out_arr) {
    if (k_data->prev_source_handle != source_handle) return false;
    if (!is_same_array_data_version(&source_arr->version, &k_data->prev_source_version)) return false;
    if (out_arr == NULL) return true;
    return is_same_array_version(&out_arr->version, &k_data->prev_output_version);
}

/* Records both ends after a real computation, so the next pass can recognize
   an untouched source and an untouched result. */
static inline void PUBLISH_DERIVED_RESULT(K_DATA *k_data, uint32_t source_handle, const CSN_ARRAY *source_arr, const CSN_ARRAY *out_arr) {
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
static inline bool CAN_REUSE_ELEMENTWISE(const K_DATA *k_data, uint32_t handle_a, const CSN_ARRAY *arr_a, uint32_t handle_b, const CSN_ARRAY *arr_b, const CSN_ARRAY *out_arr, double scalar_a, double scalar_b) {
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
static inline void PUBLISH_ELEMENTWISE(K_DATA *k_data, uint32_t handle_a, const CSN_ARRAY *arr_a, uint32_t handle_b, const CSN_ARRAY *arr_b, const CSN_ARRAY *out_arr, double scalar_a, double scalar_b) {
    k_data->prev_source_handle = handle_a;
    k_data->prev_source_handle_b = handle_b;
    k_data->prev_scalar_param = scalar_a;
    k_data->prev_scalar_param_b = scalar_b;
    set_array_version(&k_data->prev_source_version, &arr_a->version);
    if (arr_b != NULL) set_array_version(&k_data->prev_source_version_b, &arr_b->version);
    if (out_arr != NULL) set_array_version(&k_data->prev_output_version, &out_arr->version);
}

static int32_t CHECK_IF_REALLOC_IN(CSOUND *csound, OPDS *h, K_DATA *k_data, CSN_ARRAY *arr, uint32_t source_handle, CSN_SCRATCH *scratch_ref, uint32_t ndim, ITEM_TYPE itype, bool is_value_changed) {
    /* The scratch lives in the caller's opcode struct; these keep the buffer
       and its capacity moving together. */
    void **scratch = &scratch_ref->scratch;
    size_t *scratch_capacity = &scratch_ref->scratch_capacity;
    size_t required = arr->size * (size_t) itype;
    bool layout_changed = IS_REQUEST_CHANGED(k_data, ndim, itype, arr->shape) || k_data->prev_size != arr->size;
    bool scratch_too_small = *scratch_capacity < required;

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
        && memcmp(arr->data, *scratch, sizeof(double) * required) != 0) {
        csound->Message(csound, "[csnarray] VERSION CROSSCHECK: handle %u reports data version %llu unchanged while its payload differs from the last published copy\n",
                        source_handle, (unsigned long long) arr->version.data_version);
    }
#endif

    if (!layout_changed && !scratch_too_small && !data_changed && !is_value_changed) return NOTOK; // goto done

    if (scratch_too_small) {
        size_t new_capacity = required > 0 ? required * 2 : 1;
        double *data = csound->ReAlloc(csound, *scratch, sizeof(double) * new_capacity);
        if (data == NULL) {
            return csn_locked_perf_error(csound, h, "[csnarray] Memory allocation failed");
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

static bool IS_VALID_SEED(double seed) {
    return isfinite(seed) && trunc(seed) == seed && seed >= 0.0 && seed <= 9007199254740992.0; /* 2^53, exact in double */
}

static bool IS_VALID_INDEX(double index) {
    return isfinite(index) && trunc(index) == index && index >= 0.0 && index <= (double) UINT32_MAX;
}

static bool IS_VALID_LENGTH(double length) {
    return isfinite(length) && trunc(length) == length && length >= 0.0 && length <= (double) CSN_MAX_ELEMS;
}

static bool IS_VALID_ZERO_ONE(double value) {
    return isfinite(value) && trunc(value) == value && (value == 0.0 || value == 1.0);
}

static bool IS_VALID_VALUE(double value) {
    return isfinite(value) && !isnan(value);
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

/* Staged through a local copy because shape may alias array->shape: callers
   that republish an array's own layout (a source that is also the destination)
   would otherwise read back the buffer the memset just cleared. */
static inline void set_csnarray_layout(CSN_ARRAY *array, uint32_t ndim, const uint32_t *shape, size_t size, ITEM_TYPE itype) {
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
static inline void reset_empty_csnarray(CSN_ARRAY *array, uint32_t ndim, const uint32_t *requested_shape, ITEM_TYPE itype) {
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
static int32_t CHECK_SELF_ALIAS(CSOUND *csound, OPDS *h, const K_DATA *k_data, uint32_t handle_a, uint32_t handle_b) {
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
   reallocating only when the request or the buffer make it necessary. The
   caller holds the registry mutex; *destination is an output, so no caller has
   to seed it. */
static int32_t NEED_TO_UPDATE_SLOT(CSOUND *csound, OPDS *h, CSN_ARRAY **destination, K_DATA *k_data, uint32_t *owned_handle, uint32_t ndim, const uint32_t *shape, size_t logical_size, ITEM_TYPE itype, const char *err) {
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

    bool request_changed = IS_REQUEST_CHANGED(k_data, ndim, itype, shape);
    if (SHOULD_SLOT_BE_UPDATED(request_changed, *destination, itype, requested_size)) {
        if (slot->rt_locked) {
            return csn_locked_perf_error(csound, h,  "[csnarray] '%s' (array %u) is on a real-time path and cannot be reallocated at perf time; clear the mark with csnrtlock, or pass irt=0 at the audio source it descends from", get_out_name(h), req_owned_handle);
        }
        int32_t res = update_slot_array_locked(csound, k_data->registry, req_owned_handle, ndim, shape, itype, destination, &err);
        if (res != OK) {
            return csn_locked_perf_error(csound, h, "[csnarray] Could not update k-rate output slot: %s", err != NULL ? err : "unknown error");
        }
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

/* The blanket rejection in CHECK_SELF_ALIAS is too strong for an opcode whose
   fill writes each result cell from the input cell sitting at the same index:
   X = csnsin(X) is perfectly well defined, and every cell is read before it is
   written. What such an opcode still cannot survive is a reallocation, which
   would hand the fill a fresh zeroed buffer and drop the data it is about to
   read. So the permission is conditional on the requested layout matching the
   aliased array's — the same test the elementwise binops already apply.

   Callers must resolve the source and compute the output layout first, then
   call this in place of CHECK_SELF_ALIAS. */
static int32_t CHECK_SELF_ALIAS_CELL_LOCAL(CSOUND *csound, OPDS *h, const K_DATA *k_data, uint32_t source_handle, const CSN_ARRAY *source_arr, uint32_t new_ndim, const uint32_t *new_shape, ITEM_TYPE new_itype) {
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
    if (res != OK) return res;
    res = create_csnarray_init(csound, &p->h, ndim, shape, &p->array, p->handle, itype);
    if (res != OK) return res;
    reset_empty_csnarray(p->array, ndim, shape, itype);
    return OK;
}

static int32_t create_shape_csnarray_k_init(CSOUND *csound, CSN_ARR_INIT *p, CSN_K_SHAPE_INIT_MODE mode) {
    CHECK_ITYPE(csound, *p->itype);
    ITEM_TYPE itype = CSN_ITYPE_FROM_ARG(*p->itype);
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

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
    /* csnempty publishes the reserved shape with no elements; zeros and ones
       publish a filled array, so that reading the handle before the first
       triggered pass gives the same thing the i-rate form would. */
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
    CHECK_REG_HANDLE(csound, &p->h, p->k_data.registry, p->k_data.owned_handle);

    /* itype is an i-argument, so it cannot change between passes and the init
       has already validated it. */
    ITEM_TYPE itype = CSN_ITYPE_FROM_ARG(*p->itype);

    uint32_t ndim = 0;
    uint32_t shape[CSN_MAX_DIMS] = {0};
    int32_t res = parse_shape_array_k(csound, &p->h, p->shape, &ndim, shape);
    if (res != OK) {
        return res;
    }


    const char *err = NULL;
    size_t requested_size = 0;
    if (get_array_size_from_shape(&requested_size, ndim, shape) != OK) {
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    /* csnempty reserves the requested shape but publishes no element yet. */
    size_t logical_size = mode == CSN_K_EMPTY ? 0 : requested_size;

    csound->LockMutex(p->k_data.registry->mutex);

    CSN_ARRAY *array = NULL;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &array, &p->k_data, NULL, ndim, shape, logical_size, itype, err);
    if (res != OK) {
        csound->UnlockMutex(p->k_data.registry->mutex);
        return res;
    }
    p->array = array;

    if (mode != CSN_K_EMPTY) {
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
    CHECK_REGISTRY(csound, NULL, reg);

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
    CHECK_REG_HANDLE(csound, &p->h, p->k_data.registry, p->k_data.owned_handle);

    uint32_t handle_from = p->handle_from->id;
    int32_t res = OK;

    csound->LockMutex(p->k_data.registry->mutex);

    CSN_SLOT *slot = get_slot(p->k_data.registry, handle_from);
    if (slot == NULL) {
        csound->UnlockMutex(p->k_data.registry->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) handle_from);
    }

    CSN_ARRAY *source_arr = slot->array;
    uint32_t ndim = source_arr->ndim;
    ITEM_TYPE itype = source_arr->itype;
    if (itype != CSN_REAL && itype != CSN_COMPLEX) {
        csound->UnlockMutex(p->k_data.registry->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Source array has invalid internal type %u", itype);
    }

    uint32_t shape[CSN_MAX_DIMS] = {0};
    memcpy(shape, source_arr->shape, sizeof(uint32_t) * ndim);

    size_t requested_size = 0;
    if (get_array_size_from_shape(&requested_size, ndim, shape) != OK) {
        csound->UnlockMutex(p->k_data.registry->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Source array has an invalid shape");
    }

    const char *err = NULL;
    CSN_ARRAY *output_arr = NULL;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &output_arr, &p->k_data, NULL, ndim, shape, requested_size, itype, err);
    if (res != OK) goto done;
    p->array = output_arr;

    fill_csnarray(output_arr, (double) *p->value);

    SET_KDATA_END(p, shape, ndim, itype);

done:
    csound->UnlockMutex(p->k_data.registry->mutex);
    return res;
}

int32_t create_like_csnarray(CSOUND *csound, CSN_ARR_INIT_LIKE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

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
    CHECK_REGISTRY(csound, NULL, reg);

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

static int32_t create_random_csnarray_k_init(CSOUND *csound, CSN_ARR_RND_INIT *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t ndim = 1U;
    uint32_t shape[CSN_MAX_DIMS] = {0};
    shape[1] = 1U;

    int32_t res = create_csnarray_init(csound, &p->h, ndim, shape, &p->array, p->handle, CSN_REAL);
    if (res != OK) return res;

    csound->LockMutex(reg->mutex);

    reset_empty_csnarray(p->array, ndim, shape, CSN_REAL);
    SET_KDATA_WITH_ID_BEGIN(p, reg, shape, ndim, CSN_REAL, p->handle->id);

    csound->UnlockMutex(reg->mutex);
    return OK;
}

int32_t create_random_csnarray_k(CSOUND *csound, CSN_ARR_RND_INIT *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    uint32_t source_handle = p->k_data.owned_handle;
    CHECK_REG_HANDLE(csound, &p->h, reg, source_handle);

    CHECK_KTRIG(p->trig);

    int32_t res = OK;
    const char *err = NULL;

    uint32_t new_ndim = 0;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    res = parse_shape_array(csound, p->shape, &new_ndim, new_shape);
    if (res != OK) return res;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
    }

    size_t req_size = 0;
    if (get_array_size_from_shape(&req_size, new_ndim, new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSN_ARRAY *arr = NULL;
    size_t logical_size = req_size;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, new_ndim, new_shape, logical_size, CSN_REAL, err);
    if (res != OK) goto done;
    p->array = arr;

    double min = (double) *p->min;
    double max = (double) *p->max;
    for (size_t i = 0; i < arr->size; i++) {
        arr->data[i] = min + pcg32_random(&reg->rng) * (max - min);
    }

    SET_KDATA_END(p, new_shape, new_ndim, CSN_REAL);

done:
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
    CHECK_REGISTRY(csound, NULL, reg);

    SET_KDATA_BEGIN(p, reg);
    return OK;
}

int32_t create_full_csnarray_k(CSOUND *csound, CSN_FULL *p) {
    CHECK_REG_HANDLE(csound, &p->h, p->k_data.registry, p->k_data.owned_handle);

    /* Validated by the init: itype is an i-argument. */
    ITEM_TYPE itype = CSN_ITYPE_FROM_ARG(*p->itype);

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

    csound->LockMutex(p->k_data.registry->mutex);

    const char *err = NULL;
    CSN_ARRAY *array = NULL;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &array, &p->k_data, NULL, ndim, shape, requested_size, itype, err);
    if (res != OK) {
        csound->UnlockMutex(p->k_data.registry->mutex);
        return res;
    }
    p->array = array;

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
    CHECK_REGISTRY(csound, NULL, reg);

    SET_KDATA_BEGIN(p, reg);
    return OK;
}

int32_t create_fullcomp_csnarray_k(CSOUND *csound, CSN_FULLCOMPLEX *p) {
    CHECK_REG_HANDLE(csound, &p->h, p->k_data.registry, p->k_data.owned_handle);

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

    csound->LockMutex(p->k_data.registry->mutex);

    const char *err = NULL;
    CSN_ARRAY *array = NULL;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &array, &p->k_data, NULL, ndim,  shape, requested_size, CSN_COMPLEX, err);
    if (res != OK) {
        csound->UnlockMutex(p->k_data.registry->mutex);
        return res;
    }
    p->array = array;

    double re, im;
    complexdat_to_rect(p->value, &re, &im);
    fill_csnarray_complex(array, re, im);

    SET_KDATA_END(p, shape, ndim, CSN_COMPLEX);

    csound->UnlockMutex(p->k_data.registry->mutex);
    return OK;
}

int32_t create_fullcomp_csnarray(CSOUND *csound, CSN_FULLCOMPLEX *p) {
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
    CHECK_REGISTRY(csound, NULL, reg);

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

    CHECK_REG_HANDLE(csound, &p->h, p->k_data.registry, p->k_data.owned_handle);

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

    int32_t res = OK;
    csound->LockMutex(p->k_data.registry->mutex);

    const char *err = NULL;
    CSN_ARRAY *array = NULL;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &array, &p->k_data, NULL, ndim, shape, requested_size, CSN_REAL, err);
    if (res != OK) {
        csound->UnlockMutex(p->k_data.registry->mutex);
        return res;
    }
    p->array = array;

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
    CHECK_REGISTRY(csound, NULL, reg);

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

    CHECK_REG_HANDLE(csound, &p->h, p->k_data.registry, p->k_data.owned_handle);

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

    int32_t res = OK;
    csound->LockMutex(p->k_data.registry->mutex);

    const char *err = NULL;
    CSN_ARRAY *array = NULL;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &array, &p->k_data, NULL, ndim, shape, requested_size, CSN_COMPLEX, err);
    if (res != OK) {
        csound->UnlockMutex(p->k_data.registry->mutex);
        return res;
    }
    p->array = array;

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
    CHECK_REGISTRY(csound, NULL, reg);

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
    CHECK_REGISTRY(csound, &p->h, reg);

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
    CHECK_REGISTRY(csound, NULL, reg);

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
    CHECK_REGISTRY(csound, &p->h, reg);

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
    CHECK_REGISTRY(csound, NULL, reg);

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
    CHECK_REGISTRY(csound, NULL, reg);

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
    CHECK_REGISTRY(csound, &p->h, reg);

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
    CHECK_REGISTRY(csound, NULL, reg);

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
    CHECK_REGISTRY(csound, &p->h, reg);

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
    CHECK_REGISTRY(csound, NULL, reg);

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
    CHECK_REGISTRY(csound, &p->h, reg);

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
    CHECK_REGISTRY(csound, NULL, reg);

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
    CHECK_REGISTRY(csound, &p->h, reg);

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

static int32_t spaced_space_body(CSOUND *csound, OPDS *perf_h, double step_num, const MYFLT *in_start, const MYFLT *in_stop, const MYFLT *in_base, double *base, double *ratio, uint32_t *usize, CSN_SPACED_SPACE_MODE mode) {
    double start = (double) *in_start;
    double stop = (double) *in_stop;
    switch (mode) {
        case CSN_ARANGE:
            if ((stop > start && step_num < 0) || (stop < start && step_num > 0)) {
                return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Step %g has the wrong sign to go from start %g to stop %g", step_num, start, stop);
            }
            int32_t size = (int32_t) ceil((stop - start) / step_num);
            if (size == 0) {
                return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Start %g, stop %g and step %g produce an empty array", start, stop, step_num);
            }
            *usize = (uint32_t) size;
            break;
        case CSN_LINSPACE:
        case CSN_LOGSPACE:
        case CSN_GEOMSPACE:
            if (mode == CSN_LOGSPACE) {
                if (in_base == NULL) {
                    return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] The base argument is missing");
                }
                *base = (double) *in_base;
                if (*base <= 0) {
                    return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] The base argument must be > 0, got %g", *base);
                }
            }
            if (mode == CSN_GEOMSPACE) {
                *ratio = pow(stop / start, 1.0 / (step_num - 1));
            }
            *usize = (uint32_t) step_num;
            break;
    }

    return OK;
}

/* arange_step stays a double all the way down: a fractional step would floor to
   zero and a negative one would wrap if this took an unsigned integer, while
   the element count above is already computed in double. */
static void spaced_space_assign_value(CSN_ARRAY *array, uint32_t usize, double arange_step, double start, double stop, double base, double ratio, CSN_SPACED_SPACE_MODE mode) {
    switch (mode) {
        case CSN_ARANGE:
            for (uint32_t i = 0; i < usize; i++) {
                array->data[i] =  start + ((double) i * arange_step);
            }
            break;
        case CSN_LINSPACE:
            if (usize == 1) {
                array->data[0] = start;
            } else {
                double step = (stop - start) / (double) (usize - 1);
                for (uint32_t i = 0; i < usize; i++) {
                    array->data[i] =  (double) start + (i * step);
                }
                array->data[usize - 1] = stop;
            }
            break;
        case CSN_LOGSPACE:
            if (usize == 1) {
                array->data[0] = pow(base, start);
            } else {
                double step = (stop - start) / (double) (usize - 1);
                for (int32_t i = 0; i < usize; i++) {
                    double exponent = start + (double) i * step;
                    array->data[i] =  pow(base, exponent);
                }
                array->data[usize - 1] = pow(base, stop);
            }
            break;
        case CSN_GEOMSPACE:
            if (usize == 1) {
                array->data[0] = start;
            } else {
                for (int32_t i = 0; i < usize; i++) {
                    array->data[i] =  start * pow(ratio, i);
                }
                array->data[usize - 1] = stop;
            }
    }
}

static int32_t spaced_space_helper(CSOUND *csound, CSN_SPACED_SPACE *p, CSN_SPACED_SPACE_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    double num = (double) *p->step_num;
    if (mode == CSN_ARANGE) {
        if (num == 0.0) return csound->InitError(csound, "[csnarray] The step argument must not be zero");
    } else {
        if (num <= 0.0) return csound->InitError(csound, "[csnarray] The num argument must be > 0, got %d", (int32_t) num);
    }

    double arange_step = num;
    double base = 1.0;
    const MYFLT *in_base = mode == CSN_LOGSPACE ? p->arg_a : NULL;
    double ratio = 0.0;
    uint32_t usize = 0;
    /* Init pass, and the only caller that reaches spaced_space_body without
       the registry lock: the report has to go through InitError, both because
       that is the phase we are in and because the locked report would release
       a mutex this path never took. */
    int32_t res = spaced_space_body(csound, NULL, arange_step, p->start, p->stop, in_base, &base, &ratio, &usize, mode);
    if (res != OK) return res;

    int32_t res_init = create_csnarray_init(csound, &p->h, 1U, &usize, &p->array, p->handle, CSN_REAL);
    if (res_init != OK) return res_init;

    csound->LockMutex(reg->mutex);
    double start = (double) *p->start;
    double stop = (double) *p->stop;
    spaced_space_assign_value(p->array, usize, arange_step, start, stop, base, ratio, mode);
    csound->UnlockMutex(reg->mutex);
    return OK;
}

static int32_t spaced_space_k_init_helper(CSOUND *csound, CSN_SPACED_SPACE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    uint32_t shape[CSN_MAX_DIMS] = {0};
    shape[0] = 1;
    res = create_csnarray_init(csound, &p->h, 1U, shape, &p->array, p->handle, CSN_REAL);
    if (res != OK) {
        return res;
    }

    csound->LockMutex(reg->mutex);
    reset_empty_csnarray(p->array, 1U, shape, CSN_REAL);
    csound->UnlockMutex(reg->mutex);

    SET_KDATA_WITH_ID_BEGIN(p, reg, shape, 1U, CSN_REAL, p->handle->id);
    return OK;
}

static int32_t spaced_space_k_helper(CSOUND *csound, CSN_SPACED_SPACE *p, CSN_SPACED_SPACE_MODE mode) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REG_HANDLE(csound, &p->h, reg, p->k_data.owned_handle);

    const MYFLT *trig = mode == CSN_LOGSPACE ? p->arg_b : p->arg_a;
    CHECK_KTRIG(trig);

    double num = (double) *p->step_num;
    if (mode == CSN_ARANGE) {
        if (num == 0.0) return csound->PerfError(csound, &p->h, "[csnarray] The step argument must not be zero");
    } else {
        if (num <= 0.0) return csound->PerfError(csound, &p->h, "[csnarray] The num argument must be > 0, got %d", (int32_t) num);
    }

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    double arange_step = num;
    double base = 1.0;
    const MYFLT *in_base = mode == CSN_LOGSPACE ? p->arg_a : NULL;
    double ratio = 0.0;
    uint32_t usize = 0;
    res = spaced_space_body(csound, &p->h, arange_step, p->start, p->stop, in_base, &base, &ratio, &usize, mode);
    if (res != OK) goto done;

    uint32_t shape[CSN_MAX_DIMS] = {0};
    shape[0] = usize;

    size_t req_size = 0;
    if (get_array_size_from_shape(&req_size, 1U, shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSN_ARRAY *arr = NULL;
    size_t logical_size = usize == 0 ? 0 : req_size;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, 1U, shape, logical_size, CSN_REAL, err);
    if (res != OK) goto done;
    p->array = arr;

    double start = (double) *p->start;
    double stop = (double) *p->stop;
    spaced_space_assign_value(p->array, usize, arange_step, start, stop, base, ratio, mode);
    SET_KDATA_NO_ID_END(p, shape, 1U, CSN_REAL);

done:
    csound->UnlockMutex(reg->mutex);
    return OK;
}

static int32_t csnarray_spaced_space_k_init(CSOUND *csound, CSN_SPACED_SPACE *p) {
    return spaced_space_k_init_helper(csound, p);
}

int32_t csnarray_arange(CSOUND *csound, CSN_SPACED_SPACE *p) {
    return spaced_space_helper(csound, p, CSN_ARANGE);
}

int32_t csnarray_arange_k(CSOUND *csound, CSN_SPACED_SPACE *p) {
    return spaced_space_k_helper(csound, p, CSN_ARANGE);
}

int32_t csnarray_linspace(CSOUND *csound, CSN_SPACED_SPACE *p) {
    return spaced_space_helper(csound, p, CSN_LINSPACE);
}

int32_t csnarray_linspace_k(CSOUND *csound, CSN_SPACED_SPACE *p) {
    return spaced_space_k_helper(csound, p, CSN_LINSPACE);
}

int32_t csnarray_logspace(CSOUND *csound, CSN_SPACED_SPACE *p) {
    return spaced_space_helper(csound, p, CSN_LOGSPACE);
}

int32_t csnarray_logspace_k(CSOUND *csound, CSN_SPACED_SPACE *p) {
    return spaced_space_k_helper(csound, p, CSN_LOGSPACE);
}

int32_t csnarray_geomspace(CSOUND *csound, CSN_SPACED_SPACE *p) {
    return spaced_space_helper(csound, p, CSN_GEOMSPACE);
}

int32_t csnarray_geomspace_k(CSOUND *csound, CSN_SPACED_SPACE *p) {
    return spaced_space_k_helper(csound, p, CSN_GEOMSPACE);
}

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
    /* Validated by the init: itype is an i-argument. */
    ITEM_TYPE itype = CSN_ITYPE_FROM_ARG(*p->itype);
    if (!IS_VALID_VALUE((double) *p->num)) {
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid matrix size");
    }

    int32_t num = (int32_t) *p->num;
    if (num <= 0) {
        return csound->PerfError(csound, &p->h, "[csnarray] The num argument must be > 0, got %d", num);
    }

    CHECK_REG_HANDLE(csound, &p->h, p->k_data.registry, p->k_data.owned_handle);

    uint32_t ndim = 2U;
    uint32_t shape[CSN_MAX_DIMS] = {0};
    shape[0] = num;
    shape[1] = num;

    size_t requested_size = 0;
    if (get_array_size_from_shape(&requested_size, ndim, shape) != OK) {
        return csound->PerfError(csound, &p->h, "[csnarray] Shape is invalid or its element count exceeds the configured limit");
    }

    int32_t res = OK;
    csound->LockMutex(p->k_data.registry->mutex);

    const char *err = NULL;
    CSN_ARRAY *array = NULL;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &array, &p->k_data, NULL, ndim, shape, requested_size, itype, err);
    if (res != OK) goto done;
    p->array = array;

    fill_csnarray(array, 0.0);
    for (int32_t i = 0; i < num; i++) {
        size_t item = (size_t) i * (size_t) num + (size_t) i;
        array->data[item * itype] = 1.0;
    }

    SET_KDATA_END(p, shape, ndim, itype);

done:
    csound->UnlockMutex(p->k_data.registry->mutex);
    return res;
}


int32_t csnarray_reshape(CSOUND *csound, CSN_RESHAPE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

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
    CHECK_REG_HANDLE(csound, &p->h, p->k_data.registry, p->k_data.owned_handle);

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
        csound->UnlockMutex(p->k_data.registry->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown source array handle %u: no array with this id is registered", source_handle);
    }

    CSN_SLOT *output_slot = get_slot(p->k_data.registry, p->k_data.owned_handle);
    if (output_slot == NULL) {
        csound->UnlockMutex(p->k_data.registry->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] k-rate output slot is no longer active");
    }

    CSN_ARRAY *source = source_slot->array;
    CSN_ARRAY *output = output_slot->array;
    ITEM_TYPE itype = source->itype;

    if (requested_size != source->size) {
        csound->UnlockMutex(p->k_data.registry->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Reshape size mismatch: source has %zu elements, new shape requires %zu", source->size, requested_size);
    }

    /* A shape-only change never needs new storage: reshape preserves the
       element count. Allocate only if the source type changed or the output
       buffer itself is unusable. This also preserves data when source and
       output are the same slot. */
    if (output->data == NULL || output->itype != itype || output->capacity < requested_size) {
        const char *err = NULL;
        res = update_slot_array_locked(csound, p->k_data.registry, p->k_data.owned_handle, ndim, shape, itype, &p->array, &err);
        if (res != OK) {
            csound->UnlockMutex(p->k_data.registry->mutex);
            return csound->PerfError(csound, &p->h, "[csnarray] Could not update k-rate output slot: %s", err != NULL ? err : "unknown error");
        }
        output = p->array;
    }

    set_csnarray_layout(output, ndim, shape, requested_size, itype);
    p->array = output;
    if (output != source) {
        memcpy(output->data, source->data, sizeof(double) * requested_size * itype);
        /* This opcode reaches its output slot without NEED_TO_UPDATE_SLOT, so
           the data counter is its own responsibility: the copy above republishes
           the source's current values into a slot other opcodes read. */
        update_array_data_version(&output->version);
    }

    SET_KDATA_END(p, shape, ndim, itype);

    csound->UnlockMutex(p->k_data.registry->mutex);
    return res;
}

int32_t csnarray_reshape_in(CSOUND *csound, CSN_RESHAPE_IN *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

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

    /* Through set_csnarray_layout rather than by hand, so the array's shape and
       ndim counters move with the relayout. */
    set_csnarray_layout(arr, ndim, shape, arr->size, arr->itype);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

/* In-place k-rate reshape owns no output buffer. At init time only remember
   the current slot; a computed k-shape is applied during performance. */
static int32_t csnarray_reshape_in_k_init(CSOUND *csound, CSN_RESHAPE_IN *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

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
        csound->UnlockMutex(p->k_data.registry->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] k-rate output slot is no longer active");
    }

    CSN_ARRAY *arr = slot->array;

    if (requested_size != arr->size) {
        csound->UnlockMutex(p->k_data.registry->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Reshape size mismatch: source has %zu elements, new shape requires %zu", arr->size, requested_size);
    }

    ITEM_TYPE itype = arr->itype;
    set_csnarray_layout(arr, ndim, shape, requested_size, itype);

    SET_KDATA_NO_ID_END(p, shape, ndim, itype);

    csound->UnlockMutex(p->k_data.registry->mutex);
    return res;
}

int32_t csnarray_flatten(CSOUND *csound, CSN_RESHAPE *p) {
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
    CHECK_REG_HANDLE(csound, &p->h, p->k_data.registry, p->k_data.owned_handle);

    int32_t res = OK;
    csound->LockMutex(p->k_data.registry->mutex);

    uint32_t source_handle = p->source_handle->id;
    CSN_SLOT *source_slot = get_slot(p->k_data.registry, source_handle);
    if (source_slot == NULL) {
        csound->UnlockMutex(p->k_data.registry->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown source array handle %u: no array with this id is registered", source_handle);
    }

    CSN_ARRAY *source = source_slot->array;
    ITEM_TYPE itype = source->itype;

    uint32_t ndim = 1U;
    uint32_t shape[CSN_MAX_DIMS] = {0};
    shape[0] = (uint32_t) source->size;

    size_t requested_size = 0;
    if (get_array_size_from_shape(&requested_size, ndim, shape) != OK) {
        csound->UnlockMutex(p->k_data.registry->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Shape is invalid or its element count exceeds the configured limit");
    }

    const char *err = NULL;
    CSN_ARRAY *output = NULL;
    CSN_SLOT *reuse_slot = get_slot(p->k_data.registry, p->k_data.owned_handle);
    if (reuse_slot != NULL && CAN_REUSE_ELEMENTWISE(&p->k_data, source_handle, source, 0, NULL, reuse_slot->array, 0.0, 0.0)) {
        p->handle->id = p->k_data.owned_handle;
        goto done;
    }

    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &output, &p->k_data, NULL, ndim, shape, requested_size, itype, err);
    if (res != OK) goto done;
    p->array = output;

    memcpy(output->data, source->data, sizeof(double) * requested_size * itype);

    SET_KDATA_END(p, shape, ndim, itype);
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle, source, 0, NULL, output, 0.0, 0.0);

done:
    csound->UnlockMutex(p->k_data.registry->mutex);
    return res;
}

int32_t csnarray_flatten_in(CSOUND *csound, CSN_RESHAPE_IN *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
        goto done;
    }

    CSN_ARRAY *arr = slot->array;

    uint32_t flat_shape[CSN_MAX_DIMS] = {0};
    flat_shape[0] = (uint32_t) arr->size;
    set_csnarray_layout(arr, 1U, flat_shape, arr->size, arr->itype);

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
        csound->UnlockMutex(p->k_data.registry->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] k-rate output slot is no longer active");
    }

    CSN_ARRAY *arr = slot->array;
    uint32_t ndim = 1U;
    uint32_t shape[CSN_MAX_DIMS] = {0};
    shape[0] = (uint32_t) arr->size;

    ITEM_TYPE itype = arr->itype;
    set_csnarray_layout(arr, ndim, shape, arr->size, itype);

    SET_KDATA_NO_ID_END(p, arr->shape, ndim, itype);

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
    CHECK_REGISTRY(csound, NULL, reg);

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
    CHECK_REG_HANDLE(csound, &p->h, p->k_data.registry, p->k_data.owned_handle);

    uint32_t source_handle = p->source_handle->id;

    bool is_default = p->INOCOUNT < 2;

    int32_t res = CHECK_SELF_ALIAS(csound, &p->h, &p->k_data, source_handle, 0);
    if (res != OK) return res;

    const char *err = NULL;

    csound->LockMutex(p->k_data.registry->mutex);

    CSN_SLOT *source_slot = get_slot(p->k_data.registry, source_handle);
    if (source_slot == NULL) {
        csound->UnlockMutex(p->k_data.registry->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
    }

    CSN_ARRAY *source_arr = source_slot->array;

    uint32_t ndim = source_arr->ndim;
    uint32_t shape[CSN_MAX_DIMS] = {0};
    uint32_t axes[CSN_MAX_DIMS] = {0};

    if (is_default) {
        for (uint32_t i = 0; i < ndim; ++i)
            axes[i] = ndim - 1 - i;
    }
    else {
        if (p->new_shape->dimensions != 1 || p->new_shape->sizes == NULL || p->new_shape->sizes[0] != (int32_t) ndim) {
            csound->UnlockMutex(p->k_data.registry->mutex);
            return csound->PerfError(csound, &p->h, "[csnarray] Axes argument must be a 1-D array of exactly %u elements, one per dimension", ndim);
        }

        if (transpose_axes_assign(p->new_shape, axes, ndim) != OK) {
            csound->UnlockMutex(p->k_data.registry->mutex);
            return csound->PerfError(csound, &p->h, "[csnarray] Axes argument is not a valid permutation");
        }
    }

    for (uint32_t i = 0; i < ndim; ++i) {
        shape[i] = source_arr->shape[axes[i]];
    }

    size_t requested_size = 0;
    res = get_array_size_from_shape(&requested_size, ndim, shape);
    if (res != OK) {
        csound->UnlockMutex(p->k_data.registry->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    ITEM_TYPE itype = source_arr->itype;
    CSN_ARRAY *dst = NULL;
    CSN_SLOT *reuse_slot = get_slot(p->k_data.registry, p->k_data.owned_handle);
    if (reuse_slot != NULL
        && CAN_REUSE_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, reuse_slot->array, 0.0, 0.0)
        && memcmp(p->k_data.prev_axes, axes, sizeof(uint32_t) * ndim) == 0) {
        p->handle->id = p->k_data.owned_handle;
        goto done;
    }

    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &dst, &p->k_data, NULL, ndim, shape, source_arr->size, itype, err);
    if (res != OK) goto done;
    p->array = dst;

    transpose_data_assign(source_arr->data, dst->data, source_arr->size, ndim, shape, source_arr->strides, axes, source_arr->itype);

    SET_KDATA_END(p, shape, ndim, itype);
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, dst, 0.0, 0.0);
    memset(p->k_data.prev_axes, 0, sizeof(p->k_data.prev_axes));
    memcpy(p->k_data.prev_axes, axes, sizeof(uint32_t) * ndim);

done:
    csound->UnlockMutex(p->k_data.registry->mutex);
    return res;
}

int32_t csnarray_transpose_in(CSOUND *csound, CSN_RESHAPE_IN *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

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

    /* The i-rate in-place forms write an array they do not own, exactly as
       their k-rate twins do, so they owe the same counter bump. The axes
       permutation rewrites shape and strides by hand here, bypassing
       set_csnarray_layout, so the shape counter moves from here too. */
    update_array_data_version(&arr->version);
    update_array_layout_version(&arr->version, true, false, false);

done:
    csound->UnlockMutex(reg->mutex);
    if (data != NULL) {
        csound->Free(csound, data);
    }
    return res;
}

static int32_t csnarray_transpose_in_k_init(CSOUND *csound, CSN_RESHAPE_IN *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

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
    p->scratch.scratch_capacity = scratch_capacity;
    p->scratch.scratch = data;

    for (uint32_t i = 0; i < ndim; ++i) {
        shape[i] = arr->shape[axes[i]];
    }
    transpose_data_assign(arr->data, p->scratch.scratch, arr->size, ndim, shape, arr->strides, axes, arr->itype);
    memcpy(arr->data, p->scratch.scratch, sizeof(double) * arr->size * arr->itype);

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

    PUBLISH_INPLACE_WRITE(&p->k_data, source_handle, arr, true, false, false);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_transpose_in_k_deinit(CSOUND *csound, CSN_RESHAPE_IN *p) {
    if (p->scratch.scratch != NULL) {
        csound->Free(csound, p->scratch.scratch);
    }

    return OK;
}

int32_t csnarray_transpose_in_k(CSOUND *csound, CSN_RESHAPE_IN *p) {
    CHECK_REG_HANDLE(csound, &p->h, p->k_data.registry, p->k_data.owned_handle);

    uint32_t source_handle = p->source_handle->id;

    bool is_default = p->INOCOUNT < 2;
    int32_t res = OK;

    csound->LockMutex(p->k_data.registry->mutex);

    CSN_SLOT *slot = get_slot(p->k_data.registry, source_handle);
    if (slot == NULL) {
        csound->UnlockMutex(p->k_data.registry->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
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
            csound->UnlockMutex(p->k_data.registry->mutex);
            return csound->PerfError(csound, &p->h, "[csnarray] Axes argument must be a 1-D array of exactly %u elements, one per dimension", ndim);
        }

        if (transpose_axes_assign(p->new_shape, axes, ndim) != OK) {
            csound->UnlockMutex(p->k_data.registry->mutex);
            return csound->PerfError(csound, &p->h, "[csnarray] Axes argument is not a valid permutation");
        }
    }

    for (uint32_t i = 0; i < ndim; ++i) {
        shape[i] = arr->shape[axes[i]];
    }

    compute_strides(shape, strides, ndim);

    bool axes_changed = memcmp(axes, p->k_data.prev_axes, sizeof(axes)) != 0;
    res = CHECK_IF_REALLOC_IN(csound, &p->h, &p->k_data, arr, source_handle, &p->scratch, ndim, itype, axes_changed);
    if (res != OK) {
        res = res == NOTOK ? OK : res;
        goto done;
    }

    transpose_data_assign(arr->data, p->scratch.scratch, arr->size, ndim, shape, arr->strides, axes, arr->itype);
    memcpy(arr->data, p->scratch.scratch, sizeof(double) * arr->size * arr->itype);

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

    /* The axes permutation rewrote shape and strides straight onto the array,
       bypassing set_csnarray_layout, so the shape counter moves from here. */
    PUBLISH_INPLACE_WRITE(&p->k_data, source_handle, arr, true, false, false);

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
    CHECK_REG_HANDLE(csound, &p->h, p->k_data.registry, p->k_data.owned_handle);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = CHECK_SELF_ALIAS(csound, &p->h, &p->k_data, source_handle, 0);
    if (res != OK) return res;

    const char *err = NULL;

    csound->LockMutex(p->k_data.registry->mutex);

    CSN_SLOT *slot = get_slot(p->k_data.registry, source_handle);
    if (slot == NULL) {
        csound->UnlockMutex(p->k_data.registry->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
    }

    CSN_ARRAY *arr = slot->array;
    uint32_t ndim = arr->ndim;

    double axis_value = (double) *p->param_a;
    if (axis_value != -1.0 && !IS_VALID_AXIS(axis_value, ndim)) {
        csound->UnlockMutex(p->k_data.registry->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: -1 for all axes, or finite integers 0..%u)", axis_value, ndim, ndim - 1);
    }
    int32_t axis_flip = (int32_t) axis_value;

    CSN_ARRAY *dst = p->array;
    CSN_SLOT *reuse_slot = get_slot(p->k_data.registry, p->k_data.owned_handle);
    if (reuse_slot != NULL && CAN_REUSE_ELEMENTWISE(&p->k_data, source_handle, arr, 0, NULL, reuse_slot->array, axis_value, 0.0)) {
        p->handle->id = p->k_data.owned_handle;
        goto done;
    }

    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &dst, &p->k_data, NULL, ndim, arr->shape, arr->size, arr->itype, err);
    if (res != OK) goto done;

    flip_assign_value(arr, dst, NULL, dst->shape, ndim, axis_flip);
    SET_KDATA_END(p, arr->shape, ndim, arr->itype);
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle, arr, 0, NULL, dst, axis_value, 0.0);
    /* Through the int32_t, because the all-axes marker is -1 and converting
       that from a double straight into an unsigned is undefined. */
    p->k_data.prev_axis = (uint32_t) axis_flip;

done:
    csound->UnlockMutex(p->k_data.registry->mutex);
    return res;
}

int32_t csnarray_flip_in(CSOUND *csound, CSN_FLIP_ROLL_IN *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

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
    update_array_data_version(&arr->version);

done:
    csound->UnlockMutex(reg->mutex);
    if (data != NULL) {
        csound->Free(csound, data);
    }
    return res;
}

static int32_t csnarray_flip_in_k_deinit(CSOUND *csound, CSN_FLIP_ROLL_IN *p) {
    if (p->scratch.scratch != NULL) {
        csound->Free(csound, p->scratch.scratch);
    }
    return OK;
}

static int32_t csnarray_flip_in_k_init(CSOUND *csound, CSN_FLIP_ROLL_IN *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

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
    p->scratch.scratch = data;
    p->scratch.scratch_capacity = s_capacity;

    memset(p->k_data.prev_shape, 0, sizeof(p->k_data.prev_shape));
    SET_KDATA_WITH_ID_BEGIN(p, reg, arr->shape, ndim, arr->itype, source_handle);
    p->k_data.prev_size = arr->size;
    p->k_data.prev_axis = axis_flip;

    /* The init already flipped, so it has to publish like any other pass:
       without this the first k-pass sees a cache it has never filled, decides
       the array moved, and flips a second time straight back to the original. */
    PUBLISH_INPLACE_WRITE(&p->k_data, source_handle, arr, false, false, false);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_flip_in_k(CSOUND *csound, CSN_FLIP_ROLL_IN *p) {
    CHECK_REG_HANDLE(csound, &p->h, p->k_data.registry, p->k_data.owned_handle);

    uint32_t source_handle = p->source_handle->id;
    int32_t res = OK;

    csound->LockMutex(p->k_data.registry->mutex);

    CSN_SLOT *slot = get_slot(p->k_data.registry, source_handle);
    if (slot == NULL) {
        csound->UnlockMutex(p->k_data.registry->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
    }

    CSN_ARRAY *arr = slot->array;
    uint32_t ndim = arr->ndim;
    ITEM_TYPE itype = arr->itype;

    double axis_value = (double) *p->param_a;
    if (axis_value != -1.0 && !IS_VALID_AXIS(axis_value, ndim)) {
        csound->UnlockMutex(p->k_data.registry->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: -1 for all axes, or finite integers 0..%u)", axis_value, ndim, ndim - 1);
    }
    int32_t axis_flip = (int32_t) axis_value;

    bool axis_changed = axis_flip != p->k_data.prev_axis;
    res = CHECK_IF_REALLOC_IN(csound, &p->h, &p->k_data, arr, source_handle, &p->scratch, ndim, itype, axis_changed);
    if (res != OK) {
        res = res == NOTOK ? OK : res;
        goto done;
    }

    memset(p->scratch.scratch, 0, sizeof(double) * p->scratch.scratch_capacity);
    flip_assign_value(arr, NULL, p->scratch.scratch, arr->shape, ndim, axis_flip);

    memcpy(arr->data, p->scratch.scratch, sizeof(double) * arr->size * arr->itype);
    memset(p->k_data.prev_shape, 0, sizeof(p->k_data.prev_shape));
    SET_KDATA_NO_ID_END(p, arr->shape, ndim, itype);
    p->k_data.prev_size = arr->size;
    p->k_data.prev_axis = axis_flip;

    PUBLISH_INPLACE_WRITE(&p->k_data, source_handle, arr, false, false, false);

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
    CHECK_REG_HANDLE(csound, &p->h, p->k_data.registry, p->k_data.owned_handle);

    CSN_REGISTRY *reg = p->k_data.registry;
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
    double shift_value = (double) *p->param_a;
    if (!IS_VALID_SHIFT(shift_value)) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Shift %g is invalid; shift must be a finite integer in %d..%d", shift_value, INT32_MIN, INT32_MAX);
    }
    int32_t shift = (int32_t) shift_value;

    ITEM_TYPE itype = arr->itype;
    CSN_ARRAY *dst = p->array;
    CSN_SLOT *reuse_slot = get_slot(p->k_data.registry, p->k_data.owned_handle);
    if (reuse_slot != NULL && CAN_REUSE_ELEMENTWISE(&p->k_data, source_handle, arr, 0, NULL, reuse_slot->array, shift_value, 0.0)) {
        p->handle->id = p->k_data.owned_handle;
        goto done;
    }

    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &dst, &p->k_data, NULL, ndim, arr->shape, arr->size, itype, err);
    if (res != OK) goto done;

    roll_assign_value(arr, dst, NULL, shift);

    SET_KDATA_END(p, arr->shape, ndim, itype);
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle, arr, 0, NULL, dst, shift_value, 0.0);
    p->k_data.prev_roll_shift = shift;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}


int32_t csnarray_roll_in(CSOUND *csound, CSN_FLIP_ROLL_IN *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

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
    update_array_data_version(&arr->version);

done:
    csound->UnlockMutex(reg->mutex);
    if (data != NULL) {
        csound->Free(csound, data);
    }
    return res;
}

static int32_t csnarray_roll_in_k_init(CSOUND *csound, CSN_FLIP_ROLL_IN *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

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
    p->scratch.scratch = data;
    p->scratch.scratch_capacity = capacity;

    roll_assign_value(arr, NULL, p->scratch.scratch, shift);

    memcpy(arr->data, data, sizeof(double) * arr->size * arr->itype);
    SET_KDATA_WITH_ID_BEGIN(p, reg, arr->shape, arr->ndim, arr->itype, source_handle);
    p->k_data.prev_size = arr->size;
    p->k_data.prev_roll_shift = shift;

    PUBLISH_INPLACE_WRITE(&p->k_data, source_handle, arr, false, false, false);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_roll_in_k(CSOUND *csound, CSN_FLIP_ROLL_IN *p) {
    CHECK_REG_HANDLE(csound, &p->h, p->k_data.registry, p->k_data.owned_handle);

    CSN_REGISTRY *reg = p->k_data.registry;
    uint32_t source_handle = p->source_handle->id;
    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
    }

    CSN_ARRAY *arr = slot->array;
    uint32_t ndim = arr->ndim;
    uint32_t itype = arr->itype;
    double shift_value = (double) *p->param_a;
    if (!IS_VALID_SHIFT(shift_value)) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Shift %g is invalid; shift must be a finite integer in %d..%d", shift_value, INT32_MIN, INT32_MAX);
    }
    int32_t shift = (int32_t) shift_value;

    bool shift_changed = shift != p->k_data.prev_roll_shift;

    res = CHECK_IF_REALLOC_IN(csound, &p->h, &p->k_data, arr, source_handle, &p->scratch, ndim, itype, shift_changed);
    if (res != OK) {
        res = res == NOTOK ? OK : res;
        goto done;
    }

    roll_assign_value(arr, NULL, p->scratch.scratch, shift);

    memcpy(arr->data, p->scratch.scratch, sizeof(double) * arr->size * arr->itype);
    SET_KDATA_WITH_ID_BEGIN(p, reg, arr->shape, arr->ndim, arr->itype, source_handle);
    p->k_data.prev_size = arr->size;
    p->k_data.prev_roll_shift = shift;

    PUBLISH_INPLACE_WRITE(&p->k_data, source_handle, arr, false, false, false);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_rollaxis(CSOUND *csound, CSN_FLIP_ROLL *p) {
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
    CHECK_REG_HANDLE(csound, &p->h, p->k_data.registry, p->k_data.owned_handle);

    uint32_t source_handle = p->source_handle->id;
    CSN_REGISTRY *reg = p->k_data.registry;

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
    double shift_value = (double) *p->param_a;
    if (!IS_VALID_SHIFT(shift_value)) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Shift %g is invalid; shift must be a finite integer in %d..%d", shift_value, INT32_MIN, INT32_MAX);
    }
    int32_t shift = (int32_t) shift_value;
    double axis_value = (double) *p->param_b;
    if (axis_value != -1.0 && !IS_VALID_AXIS(axis_value, ndim)) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: -1 for all axes, or finite integers 0..%u)", axis_value, ndim, ndim - 1);
    }
    int32_t axis_roll = (int32_t) axis_value;

    CSN_ARRAY *dst = p->array;
    CSN_SLOT *reuse_slot = get_slot(p->k_data.registry, p->k_data.owned_handle);
    if (reuse_slot != NULL && CAN_REUSE_ELEMENTWISE(&p->k_data, source_handle, arr, 0, NULL, reuse_slot->array, shift_value, axis_value)) {
        p->handle->id = p->k_data.owned_handle;
        goto done;
    }

    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &dst, &p->k_data, NULL, ndim, arr->shape, arr->size, arr->itype, err);
    if (res != OK) goto done;

    rollaxis_assign_value(arr, dst, NULL, dst->shape, ndim, shift, axis_roll);
    SET_KDATA_END(p, arr->shape, ndim, arr->itype);
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle, arr, 0, NULL, dst, shift_value, axis_value);
    p->k_data.prev_axis = axis_roll;
    p->k_data.prev_roll_shift = shift;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_rollaxis_in(CSOUND *csound, CSN_FLIP_ROLL_IN *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

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
    update_array_data_version(&arr->version);

done:
    csound->UnlockMutex(reg->mutex);
    if (data != NULL) {
        csound->Free(csound, data);
    }
    return res;
}

static int32_t csnarray_rollaxis_in_k_init(CSOUND *csound, CSN_FLIP_ROLL_IN *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

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
    p->scratch.scratch = data;
    p->scratch.scratch_capacity = capacity;

    rollaxis_assign_value(arr, NULL, p->scratch.scratch, arr->shape, ndim, shift, axis_roll);
    memcpy(arr->data, p->scratch.scratch, sizeof(double) * arr->size * arr->itype);
    SET_KDATA_WITH_ID_BEGIN(p, reg, arr->shape, ndim, arr->itype, source_handle);
    p->k_data.prev_size = arr->size;
    p->k_data.prev_axis = axis_roll;
    p->k_data.prev_roll_shift = shift;

    PUBLISH_INPLACE_WRITE(&p->k_data, source_handle, arr, false, false, false);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_rollaxis_in_k(CSOUND *csound, CSN_FLIP_ROLL_IN *p) {
    CHECK_REG_HANDLE(csound, &p->h, p->k_data.registry, p->k_data.owned_handle);
    uint32_t source_handle = p->source_handle->id;
    CSN_REGISTRY *reg = p->k_data.registry;

    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
    }

    CSN_ARRAY *arr = slot->array;
    uint32_t ndim = arr->ndim;
    double shift_value = (double) *p->param_a;
    if (!IS_VALID_SHIFT(shift_value)) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Shift %g is invalid; shift must be a finite integer in %d..%d", shift_value, INT32_MIN, INT32_MAX);
    }
    int32_t shift = (int32_t) shift_value;
    double axis_value = (double) *p->param_b;
    if (axis_value != -1.0 && !IS_VALID_AXIS(axis_value, ndim)) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: -1 for all axes, or finite integers 0..%u)", axis_value, ndim, ndim - 1);
    }
    int32_t axis_roll = (int32_t) axis_value;

    bool is_changed = (shift != p->k_data.prev_roll_shift) || (axis_roll != p->k_data.prev_axis);
    res = CHECK_IF_REALLOC_IN(csound, &p->h, &p->k_data, arr, source_handle, &p->scratch, ndim, arr->itype, is_changed);
    if (res != OK) {
        res = res == NOTOK ? OK : res;
        goto done;
    }

    rollaxis_assign_value(arr, NULL, p->scratch.scratch, arr->shape, ndim, shift, axis_roll);
    SET_KDATA_NO_ID_END(p, arr->shape, ndim, arr->itype);
    memcpy(arr->data, p->scratch.scratch, sizeof(double) * arr->size * arr->itype);
    p->k_data.prev_size = arr->size;
    p->k_data.prev_axis = axis_roll;
    p->k_data.prev_roll_shift = shift;

    PUBLISH_INPLACE_WRITE(&p->k_data, source_handle, arr, false, false, false);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

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

    if (!IS_VALID_AXIS(in_axis, ndim)) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: finite integers 0..%u)", in_axis, ndim, ndim - 1);
    }
    *out_axis = (uint32_t) in_axis;
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
    p->k_data.prev_axis = axis;
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
    p->k_data.prev_axis = axis;
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
    if (!IS_VALID_AXIS(axis_value, ndim)) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: finite integers 0..%u)", axis_value, ndim, ndim - 1);
    }

    if (!IS_VALID_INDEX(start_value) || !IS_VALID_INDEX(stop_value) || !IS_VALID_INDEX(step_value) || step_value == 0.0) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Invalid slice start=%g stop=%g step=%g: values must be finite integers, start and stop must be non-negative, and step must be > 0", start_value, stop_value, step_value);
    }

    uint32_t axis = (uint32_t) axis_value;
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

static int32_t csnarray_get_slice_k_init(CSOUND *csound, CSN_GET_SLICE *p) {
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

static int32_t ensure_mutation_capacity(CSOUND *csound, OPDS *perf_h, CSN_ARRAY *arr, size_t required_size) {
    if (required_size > arr->capacity) {
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

static int32_t push_in(CSOUND *csound, OPDS *perf_h, CSN_ARRAY *arr, const MYFLT *in_rvalue, COMPLEXDAT *in_cvalue) {
    size_t new_size = arr->size + 1;
    int32_t res = ensure_mutation_capacity(csound, perf_h, arr, new_size);
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
    res = push_in(csound, NULL, arr, p->in_value, NULL);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_push_k_init(CSOUND *csound, CSN_PUSH_K *p) {
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
    res = push_in(csound, &p->h, arr, p->in_value, NULL);

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
    res = push_in(csound, NULL, arr, NULL, p->in_value);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_pushcomp_k_init(CSOUND *csound, CSN_PUSHCOMPLEX_K *p) {
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
    res = push_in(csound, &p->h, arr, NULL, p->in_value);

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

static int32_t csnarray_pop_k_init(CSOUND *csound, CSN_POP_K *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    p->registry = reg;
    return OK;
}

static int32_t csnarray_popcomp_k_init(CSOUND *csound, CSN_POPCOMPLEX_K *p) {
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
    int32_t res = ensure_mutation_capacity(csound, perf_h, arr, new_size);
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
    if (!IS_VALID_AXIS(axis_value, source_ndim)) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: finite integers 0..%u)", axis_value, source_ndim, source_ndim - 1);
    }
    *axis = (uint32_t) axis_value;

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

static int32_t csnarray_insert_block_deinit(CSOUND *csound, CSN_INSERT_BLOCK *p) {
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
    p->scratch = temp;
    p->registry = reg;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_insert_block_k_init(CSOUND *csound, CSN_INSERT_BLOCK *p) {
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

    bool have_same_shape = memcmp(p->scratch->shape, temp_shape, sizeof(uint32_t) * CSN_MAX_DIMS) == 0;
    bool request_changed = !have_same_shape || source_ndim != p->scratch->ndim || p->scratch->capacity < requested_size || source_arr->itype != p->scratch->itype;

    if (request_changed) {
        bool needs_realloc = p->scratch->capacity < requested_size || source_arr->itype != p->scratch->itype;
        if (needs_realloc) {
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

    insert_block_assign_value(p->scratch, source_arr, data_arr, source_ndim, axis, index);

    size_t bytes = sizeof(double) * p->scratch->capacity * p->scratch->itype;
    double *new_data = csound->ReAlloc(csound, source_arr->data, bytes);
    if (new_data == NULL) {
        csound->Free(csound, p->scratch->data);
        csound->Free(csound, p->scratch);
        p->scratch = NULL;
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Out of memory: allocation of %zu bytes failed", bytes);
    }

    source_arr->data = new_data;
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
    if (!IS_VALID_AXIS(axis_value, source_ndim)) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: finite integers 0..%u)", axis_value, source_ndim, source_ndim - 1);
    }
    *axis = (uint32_t) axis_value;

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

static int32_t csnarray_remove_block_k_init(CSOUND *csound, CSN_TAKE *p) {
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
    if (!IS_VALID_AXIS(axis_value, source_ndim)) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: finite integers 0..%u)", axis_value, source_ndim, source_ndim - 1);
    }
    *out_axis = (uint32_t) axis_value;

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

static int32_t csnarray_concat_block_k_init(CSOUND *csound, CSN_CONCAT *p) {
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

    if ((double) *p->arg_b == 0.0) return OK;

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
    res = concat_block_body(csound, &p->h, reg, &source_arr, &data_arr, source_handle, data_handle, new_shape, p->arg_a, &axis);
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

static int32_t pad_body(
    CSOUND *csound,
    OPDS *perf_h,
    CSN_REGISTRY *reg,
    CSN_ARRAY **source_array,
    uint32_t source_handle,
    double in_axis,
    const MYFLT *in_before,
    const MYFLT *in_after,
    int32_t *out_axis,
    uint32_t *out_before,
    uint32_t *out_after,
    uint32_t *out_shape,
    ITEM_TYPE expected_type
) {
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

    /* axis == -1 pads every axis; the .ax overloads may still ask for it. */
    int32_t axis = -1;
    if (in_axis != -1.0) {
        if (!IS_VALID_AXIS(in_axis, source_ndim)) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: finite integers 0..%u)", in_axis, source_ndim, source_ndim - 1);
        }
        axis = (int32_t) in_axis;
    }
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

static void pad_assign_value(CSN_ARRAY *source_arr, CSN_ARRAY *destination, double real_value, COMPLEXDAT *complex_value, int32_t axis, uint32_t before) {
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

static int32_t csnarray_pad_helper(CSOUND *csound, const OPDS *h, CSNREF *ohandle, CSNREF *shandle, const MYFLT *in_before, const MYFLT *in_after, double value, COMPLEXDAT *valuecomp, ITEM_TYPE expected_itype, double inaxis, CSN_ARRAY **array) {
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
    res = pad_body(csound, NULL, reg, &source_arr, source_handle, inaxis, in_before, in_after, &axis, &before, &after, new_shape, expected_itype);
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

static int32_t csnarray_pad_k_init(CSOUND *csound, CSN_PAD *p) {
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

static int32_t csnarray_padcomp_k_init(CSOUND *csound, CSN_PADCOMPLEX *p) {
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

int32_t csnarray_pad_k(CSOUND *csound, CSN_PAD *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REG_HANDLE(csound, &p->h, reg, p->k_data.owned_handle);
    if (p->array == NULL) {
        return csound->PerfError(csound, &p->h, "[csnarray] Internal error: output array is not available");
    }

    double in_axis = p->INOCOUNT > 5 ? (double) *p->arg_a : -1.0;
    double trig = p->INOCOUNT > 5 ? (double) *p->arg_b : (double) *p->arg_a;

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
    res = pad_body(csound, &p->h, reg, &source_arr, source_handle, in_axis, p->before, p->after, &axis, &before, &after, new_shape, CSN_REAL);
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

    double in_axis = p->INOCOUNT > 5 ? (double) *p->arg_a : -1.0;
    double trig = p->INOCOUNT > 5 ? (double) *p->arg_b : (double) *p->arg_a;

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
    res = pad_body(csound, &p->h, reg, &source_arr, source_handle, in_axis, p->before, p->after, &axis, &before, &after, new_shape, CSN_COMPLEX);
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
        && (int32_t) p->k_data.prev_axis == axis
        && p->k_data.prev_index == before && p->k_data.prev_roll_shift == (int32_t) after) {
        p->handle->id = p->k_data.owned_handle;
        goto done;
    }

    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, source_arr->ndim, new_shape, requested_size, source_arr->itype, err);
    if (res != OK) goto done;

    pad_assign_value(source_arr, arr, 0.0, p->value, axis, before);
    SET_KDATA_END(p, new_shape, arr->ndim, arr->itype);
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, arr, fill_re, fill_im);
    p->k_data.prev_axis = (uint32_t) axis;
    p->k_data.prev_index = before;
    p->k_data.prev_roll_shift = (int32_t) after;
    p->k_data.prev_size = arr->size;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_pad(CSOUND *csound, CSN_PAD *p) {
    double axis = p->INOCOUNT > 4 ? (double) *p->arg_a : -1.0;
    return csnarray_pad_helper(
        csound,
        &p->h,
        p->handle,
        p->source_handle,
        p->before,
        p->after,
        (double) *p->value,
        NULL,
        CSN_REAL,
        axis,
        &p->array);
}

int32_t csnarray_padcomp(CSOUND *csound, CSN_PADCOMPLEX *p) {
    double axis = p->INOCOUNT > 4 ? (double) *p->arg_a : -1.0;
    return csnarray_pad_helper(
        csound,
        &p->h,
        p->handle,
        p->source_handle,
        p->before,
        p->after,
        0.0,
        p->value,
        CSN_COMPLEX,
        axis,
        &p->array);
}

static int32_t csnarray_pad_in_helper(CSOUND *csound, CSNREF *shandle, const MYFLT *in_before, const MYFLT *in_after, double value, COMPLEXDAT *valuecomp, ITEM_TYPE expected_itype, double inaxis) {
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
    res = pad_body(csound, NULL, reg, &source_arr, source_handle, inaxis, in_before, in_after, &axis, &before, &after, new_shape, expected_itype);
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
    double axis = p->INOCOUNT > 4 ? (double) *p->arg_a : -1.0;
    return csnarray_pad_in_helper(
        csound,
        p->source_handle,
        p->before,
        p->after,
        (double) *p->value,
        NULL,
        CSN_REAL,
        axis);
}

int32_t csnarray_padcomp_in(CSOUND *csound, CSN_PADCOMPLEX_IN *p) {
    double axis = p->INOCOUNT > 4 ? (double) *p->arg_a : -1.0;
    return csnarray_pad_in_helper(
        csound,
        p->source_handle,
        p->before,
        p->after,
        0.0,
        p->value,
        CSN_COMPLEX,
        axis);
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
       grown by the perf pass whenever the requested padding needs more room. */
    if (allocate_array(csound, temp, source_arr->ndim, source_arr->shape, source_arr->array_id, source_arr->itype) != OK) {
        char tbuf[CSN_SHAPE_STR_MAX];
        csound->Free(csound, temp);
        res = csound->InitError(csound, "[csnarray] Out of memory: could not allocate the %u-D temporary array %s", source_arr->ndim, shape_str(tbuf, sizeof(tbuf), source_arr->shape, source_arr->ndim));
        goto done;
    }

    *out_scratch = temp;
    *out_registry = reg;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_pad_in_k_init(CSOUND *csound, CSN_PAD_IN *p) {
    return csnarray_pad_in_k_scratch_init(csound, p->source_handle, CSN_REAL, &p->registry, &p->scratch);
}

static int32_t csnarray_padcomp_in_k_init(CSOUND *csound, CSN_PADCOMPLEX_IN *p) {
    return csnarray_pad_in_k_scratch_init(csound, p->source_handle, CSN_COMPLEX, &p->registry, &p->scratch);
}

static int32_t csnarray_pad_in_k_deinit(CSOUND *csound, CSN_PAD_IN *p) {
    if (p->scratch != NULL) {
        if (p->scratch->data != NULL) {
            csound->Free(csound, p->scratch->data);
        }
        csound->Free(csound, p->scratch);
        p->scratch = NULL;
    }
    return OK;
}

static int32_t csnarray_padcomp_in_k_deinit(CSOUND *csound, CSN_PADCOMPLEX_IN *p) {
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
static int32_t pad_in_k_commit(CSOUND *csound, OPDS *h, CSN_ARRAY **scratch, CSN_ARRAY *source_arr, const uint32_t *new_shape, double value, COMPLEXDAT *valuecomp, int32_t axis, uint32_t before) {
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

    pad_assign_value(source_arr, temp, value, valuecomp, axis, before);

    size_t bytes = sizeof(double) * temp->capacity * temp->itype;
    double *new_data = csound->ReAlloc(csound, source_arr->data, bytes);
    if (new_data == NULL) {
        csound->Free(csound, temp->data);
        csound->Free(csound, temp);
        *scratch = NULL;
        return csn_locked_perf_error(csound, h, "[csnarray] Out of memory: allocation of %zu bytes failed", bytes);
    }

    source_arr->data = new_data;
    travase_csnarray(source_arr, temp);
    return OK;
}

int32_t csnarray_pad_in_k(CSOUND *csound, CSN_PAD_IN *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    if (p->scratch == NULL) {
        return csound->PerfError(csound, &p->h, "[csnarray] Internal error: temporary buffer not available");
    }

    double in_axis = p->INOCOUNT > 5 ? (double) *p->arg_a : -1.0;
    double trig = p->INOCOUNT > 5 ? (double) *p->arg_b : (double) *p->arg_a;

    if (trig == 0.0) return OK;

    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    int32_t axis = 0;
    uint32_t before = 0;
    uint32_t after = 0;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    res = pad_body(csound, &p->h, reg, &source_arr, p->source_handle->id, in_axis, p->before, p->after, &axis, &before, &after, new_shape, CSN_REAL);
    if (res != OK) goto done;

    if (CAN_REUSE_ELEMENTWISE(&p->k_data, p->source_handle->id, source_arr, 0, NULL, NULL, (double) *p->value, (double) axis)
        && p->k_data.prev_index == before && p->k_data.prev_roll_shift == (int32_t) after) {
        goto done;
    }

    res = pad_in_k_commit(csound, &p->h, &p->scratch, source_arr, new_shape, (double) *p->value, NULL, axis, before);
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

    double in_axis = p->INOCOUNT > 5 ? (double) *p->arg_a : -1.0;
    double trig = p->INOCOUNT > 5 ? (double) *p->arg_b : (double) *p->arg_a;

    if (trig == 0.0) return OK;
    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    int32_t axis = 0;
    uint32_t before = 0;
    uint32_t after = 0;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    res = pad_body(csound, &p->h, reg, &source_arr, p->source_handle->id, in_axis, p->before, p->after, &axis, &before, &after, new_shape, CSN_COMPLEX);
    if (res != OK) goto done;

    double fill_re = 0.0, fill_im = 0.0;
    complexdat_to_rect(p->value, &fill_re, &fill_im);
    if (CAN_REUSE_ELEMENTWISE(&p->k_data, p->source_handle->id, source_arr, 0, NULL, NULL, fill_re, fill_im)
        && (int32_t) p->k_data.prev_axis == axis
        && p->k_data.prev_index == before && p->k_data.prev_roll_shift == (int32_t) after) { goto done; }

    res = pad_in_k_commit(csound, &p->h, &p->scratch, source_arr, new_shape, 0.0, p->value, axis, before);
    PUBLISH_ELEMENTWISE(&p->k_data, p->source_handle->id, source_arr, 0, NULL, NULL, fill_re, fill_im);
    p->k_data.prev_axis = (uint32_t) axis;
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

static int32_t csnarray_clip_k_init(CSOUND *csound, CSN_CLIP *p) {
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

static int32_t csnarray_clip_in_k_init(CSOUND *csound, CSN_CLIP_IN *p) {
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
        case IS_FIN:        return isfinite(value) != 0;
        case IS_INF:        return isinf(value) != 0;
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

static int32_t argwhere_body(CSOUND *csound, OPDS *perf_h, CSN_REGISTRY *reg, CSN_ARRAY **source_array, CSN_ARRAY **data_array, uint32_t source_handle, uint32_t data_handle) {
    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
    }

    CSN_SLOT *data_slot = get_slot(reg, data_handle);
    if (data_slot == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) data_handle);
    }

    CSN_ARRAY *source_arr = source_slot->array;

    if (source_arr->itype != CSN_REAL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] This operation is not implemented for complex arrays");
    }

    CSN_ARRAY *data_arr = data_slot->array;

    uint32_t data_ndim = data_arr->ndim;

    if (data_ndim != 1U) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Data array must be 1-D, got %u-D", data_ndim);
    }

    *source_array = source_slot->array;
    *data_array = data_slot->array;
    return OK;
}

static void argwhere_assign_value(CSN_ARRAY *source_arr, CSN_ARRAY *data_arr, CSN_ARRAY *arr) {
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
        from_linear_to_coords(src_coords, source_arr->shape, linear, source_arr->ndim);

        for (size_t j = 0; j < source_arr->ndim; ++j) {
            arr->data[match * source_arr->ndim + j] = (double) src_coords[j];
        }

        match++;
    }
}

int32_t csnarray_argwhere(CSOUND *csound, CSN_ARGWHERE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;
    uint32_t data_handle = p->data_handle->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    CSN_ARRAY *data_arr = NULL;
    res = argwhere_body(csound, NULL, reg, &source_arr, &data_arr, source_handle, data_handle);
    if (res != OK) goto done;


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
    argwhere_assign_value(source_arr, data_arr, arr);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_argwhere_k_init(CSOUND *csound, CSN_ARGWHERE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;
    uint32_t data_handle = p->data_handle->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    CSN_ARRAY *data_arr = NULL;
    res = argwhere_body(csound, NULL, reg, &source_arr, &data_arr, source_handle, data_handle);
    if (res != OK) goto done;

    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    const uint32_t protect[2] = { source_handle, data_handle };
    if (create_csnarray_locked(csound, reg, &p->h, source_ndim, source_shape, &p->array, p->handle, protect, 2U, &err, source_arr->itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    if (source_arr->size > 0) {
        memcpy(p->array->data, source_arr->data, sizeof(double) * source_arr->size * source_arr->itype);
        p->array->size = source_arr->size;
    }

    SET_KDATA_BEGIN(p, reg);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

/* csnargwhere takes a mask array before the trigger; csnargunique, csnargnonzero
   and csnargisnan do not, so on those the trigger is bound one slot earlier,
   into the data_handle pointer. The declared arity tells them apart. */
static inline const MYFLT *argwhere_trig(const OPDS *h, CSNREF *data_handle, MYFLT *trig) {
    return h->optext->t.inArgCount > 2 ? trig : (const MYFLT *) data_handle;
}

/* Same story for the comparison family: csnunique has no compared value, so its
   trigger lands in cmp_value. */
static inline const MYFLT *compare_trig(const OPDS *h, MYFLT *cmp_value, MYFLT *trig) {
    return h->optext->t.inArgCount > 2 ? trig : cmp_value;
}

int32_t csnarray_argwhere_k(CSOUND *csound, CSN_ARGWHERE *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REG_HANDLE(csound, &p->h, reg, p->k_data.owned_handle);

    uint32_t source_handle = p->source_handle->id;
    uint32_t data_handle = p->data_handle->id;

    int32_t res = OK;
    const char *err = NULL;
    res = CHECK_SELF_ALIAS(csound, &p->h, &p->k_data, source_handle, data_handle);
    if (res != OK) return res;

    CHECK_KTRIG(p->trig);

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    CSN_ARRAY *data_arr = NULL;
    res = argwhere_body(csound, &p->h, reg, &source_arr, &data_arr, source_handle, data_handle);
    if (res != OK) goto done;


    size_t count = count_elements_from_array(source_arr, data_arr, EQUAL);
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    new_shape[0] = (uint32_t) count;
    new_shape[1] = source_arr->ndim;

    size_t req_size = 0;
    if (get_array_size_from_shape(&req_size, 2U, new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSN_ARRAY *arr = NULL;
    size_t logical_size = count == 0 ? 0 : req_size;
    CSN_SLOT *reuse_slot = get_slot(reg, p->k_data.owned_handle);
    if (reuse_slot != NULL && CAN_REUSE_ELEMENTWISE(&p->k_data, source_handle, source_arr, data_handle, data_arr, reuse_slot->array, 0.0, 0.0)) {
        p->handle->id = p->k_data.owned_handle;
        goto done;
    }

    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, 2U, new_shape, logical_size, source_arr->itype, err);
    if (res != OK) goto done;
    argwhere_assign_value(source_arr, data_arr, arr);
    p->array = arr;

    SET_KDATA_END(p, new_shape, 2U, source_arr->itype);
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle, source_arr, data_handle, data_arr, arr, 0.0, 0.0);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static void argselect_assign_value(CSN_ARRAY *source_arr, CSN_ARRAY *arr, CSN_COMPARE_MODE mode) {
    size_t match = 0;
    for (size_t linear = 0; linear < source_arr->size; ++linear) {
        if (compare_match(source_arr->data[linear], 0.0, mode)) {
            uint32_t src_coords[CSN_MAX_DIMS] = {0};
            from_linear_to_coords(src_coords, source_arr->shape, linear, source_arr->ndim);

            for (size_t j = 0; j < source_arr->ndim; ++j) {
                arr->data[match * source_arr->ndim + j] = (double) src_coords[j];
            }
            match++;
        }
    }
}

/* Shared by csnargnonzero and csnargisnan: both select elements by a predicate
   that needs no comparison value, and both report the coordinates. */
static int32_t csnarray_argselect_helper(CSOUND *csound, CSN_ARGWHERE *p, CSN_COMPARE_MODE mode) {
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

    size_t count = count_elements_from_value(source_arr, 0.0, mode);
    uint32_t new_shape[2] = { (uint32_t) count, source_arr->ndim };

    const uint32_t protect[1] = { source_handle };
    if (create_csnarray_locked(csound, reg, &p->h, 2U, new_shape, &p->array, p->handle, protect, 1U, &err, source_arr->itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;
    argselect_assign_value(source_arr, arr, mode);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_argselect_k_init(CSOUND *csound, CSN_ARGWHERE *p) {
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
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    const uint32_t protect[1] = { source_handle };
    if (create_csnarray_locked(csound, reg, &p->h, source_ndim, source_shape, &p->array, p->handle, protect, 1U, &err, source_arr->itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;
    if (source_arr->size > 0) {
        memcpy(arr->data, source_arr->data, sizeof(double) * source_arr->size); // REAL
        arr->size = source_arr->size;
    }

    SET_KDATA_BEGIN(p, reg);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_argselect_k_helper(CSOUND *csound, CSN_ARGWHERE *p, CSN_COMPARE_MODE mode) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REG_HANDLE(csound, &p->h, reg, p->source_handle);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    const char *err = NULL;
    res = CHECK_SELF_ALIAS(csound, &p->h, &p->k_data, source_handle, 0);
    if (res != OK) return res;

    CHECK_KTRIG(argwhere_trig(&p->h, p->data_handle, p->trig));

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

    size_t count = count_elements_from_value(source_arr, 0.0, mode);
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    new_shape[0] = (uint32_t) count;
    new_shape[1] = source_arr->ndim;

    size_t req_size = 0;
    if (get_array_size_from_shape(&req_size, 2U, new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSN_ARRAY *arr = NULL;
    size_t logical_size = count == 0 ? 0 : req_size;
    CSN_SLOT *reuse_slot = get_slot(reg, p->k_data.owned_handle);
    if (reuse_slot != NULL && CAN_REUSE_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, reuse_slot->array, 0.0, 0.0)) {
        p->handle->id = p->k_data.owned_handle;
        goto done;
    }

    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, 2U, new_shape, logical_size, source_arr->itype, err);
    if (res != OK) goto done;

    p->array = arr;
    argselect_assign_value(source_arr, arr, mode);
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, arr, 0.0, 0.0);

    SET_KDATA_END(p, new_shape, 2U, CSN_REAL);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_argnonzero(CSOUND *csound, CSN_ARGWHERE *p) {
    return csnarray_argselect_helper(csound, p, NONZERO);
}

int32_t csnarray_argnonzero_k(CSOUND *csound, CSN_ARGWHERE *p) {
    return csnarray_argselect_k_helper(csound, p, NONZERO);
}

int32_t csnarray_argisnan(CSOUND *csound, CSN_ARGWHERE *p) {
    return csnarray_argselect_helper(csound, p, IS_NAN);
}

int32_t csnarray_argisnan_k(CSOUND *csound, CSN_ARGWHERE *p) {
    return csnarray_argselect_k_helper(csound, p, IS_NAN);
}

static void mask_select_assign_value(const CSN_ARRAY *source_arr, CSN_ARRAY *arr, CSN_COMPARE_MODE mode) {
    for (size_t i = 0; i < source_arr->size; i++) {
        arr->data[i] = compare_match(source_arr->data[i], 0.0, mode) ? 1.0 : 0.0;
    }
}

static int32_t mask_select_helper(CSOUND *csound, CSN_UNARYOP *p, CSN_COMPARE_MODE mode) {
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

    CSN_ARRAY *arr = p->array;
    set_csnarray_layout(arr, source_arr->ndim, source_arr->shape, source_arr->size, CSN_REAL);
    mask_select_assign_value(source_arr, arr, mode);
    update_array_data_version(&arr->version);
    SET_KDATA_BEGIN(p, reg);
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, arr, 0.0, 0.0);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t mask_select_k_helper(CSOUND *csound, CSN_UNARYOP *p, CSN_COMPARE_MODE mode) {
    CSN_REGISTRY *reg = p->k_data.registry;
    uint32_t owned_handle = p->k_data.owned_handle;
    CHECK_REG_HANDLE(csound, &p->h, reg, owned_handle);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    const char *err = NULL;

    CHECK_KTRIG(p->trig);

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
        goto done;
    }
    CSN_ARRAY *source_arr = source_slot->array;

    if (source_arr->itype != CSN_REAL) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] This operation is not implemented for complex arrays");
        goto done;
    }

    res = CHECK_SELF_ALIAS_CELL_LOCAL(csound, &p->h, &p->k_data, source_handle, source_arr, source_arr->ndim, source_arr->shape, CSN_REAL);
    if (res != OK) goto done;

    CSN_SLOT *out_slot = get_slot(reg, owned_handle);
    if (out_slot != NULL && CAN_REUSE_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, out_slot->array, 0.0, 0.0)) {
        p->handle->id = owned_handle;
        goto done;
    }

    size_t req_size = 0;
    if (get_array_size_from_shape(&req_size, source_arr->ndim, source_arr->shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSN_ARRAY *arr = NULL;
    size_t logical_size = source_arr->size == 0 ? 0 : req_size;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, source_arr->ndim, source_arr->shape, logical_size, CSN_REAL, err);
    if (res != OK) goto done;
    p->array = arr;

    mask_select_assign_value(source_arr, arr, mode);

    SET_KDATA_END(p, source_arr->shape, source_arr->ndim, CSN_REAL);
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, arr, 0.0, 0.0);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_isnan(CSOUND *csound, CSN_UNARYOP *p) {
    return mask_select_helper(csound, p, IS_NAN);
}

int32_t csnarray_isnan_k(CSOUND *csound, CSN_UNARYOP *p) {
    return mask_select_k_helper(csound, p, IS_NAN);
}

int32_t csnarray_isinf(CSOUND *csound, CSN_UNARYOP *p) {
    return mask_select_helper(csound, p, IS_INF);
}

int32_t csnarray_isinf_k(CSOUND *csound, CSN_UNARYOP *p) {
    return mask_select_k_helper(csound, p, IS_INF);
}

int32_t csnarray_isfin(CSOUND *csound, CSN_UNARYOP *p) {
    return mask_select_helper(csound, p, IS_FIN);
}

int32_t csnarray_isfin_k(CSOUND *csound, CSN_UNARYOP *p) {
    return mask_select_k_helper(csound, p, IS_FIN);
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
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    const char *err = NULL;

    /* Declared before any goto: the done label frees it. */
    ARRAY_ELEMENT *temp = NULL;

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

    temp = csound->Calloc(csound, sizeof(ARRAY_ELEMENT) * source_arr->size);
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

done:
    if (temp != NULL) {
        csound->Free(csound, temp);
    }
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_argunique_k_init(CSOUND *csound, CSN_ARGWHERE *p) {
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

    size_t capacity = source_arr->size > 0 ? source_arr->size * 2 : 1;
    ARRAY_ELEMENT *temp = csound->Calloc(csound, sizeof(ARRAY_ELEMENT) * capacity);
    if (temp == NULL) {
        res = csound->InitError(csound, "[csnarray] Out of memory: allocation of %zu bytes failed", (size_t) (sizeof(ARRAY_ELEMENT) * source_arr->size));
        goto done;
    }

    p->scratch.scratch = temp;
    p->scratch.scratch_capacity = capacity;

    const uint32_t protect[1] = { source_handle };
    if (create_csnarray_locked(csound, reg, &p->h, 2U, source_arr->shape, &p->array, p->handle, protect, 1U, &err, source_arr->itype) != OK) {
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

int32_t csnarray_argunique_k(CSOUND *csound, CSN_ARGWHERE *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REG_HANDLE(csound, &p->h, reg, p->k_data.owned_handle);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    const char *err = NULL;
    res = CHECK_SELF_ALIAS(csound, &p->h, &p->k_data, source_handle, 0);
    if (res != OK) return res;

    CHECK_KTRIG(argwhere_trig(&p->h, p->data_handle, p->trig));

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
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;
    size_t source_size = source_arr->size;

    if (source_size > p->scratch.scratch_capacity) {
        size_t new_cap = source_size * 2;
        ARRAY_ELEMENT *temp = csound->ReAlloc(csound, p->scratch.scratch, sizeof(ARRAY_ELEMENT) * new_cap);
        if (temp == NULL) {
            csound->UnlockMutex(reg->mutex);
            return csound->PerfError(csound, &p->h, "[csnarray] Out of memory: allocation of %zu bytes failed", (size_t) (sizeof(ARRAY_ELEMENT) * source_arr->size));
        }
        p->scratch.scratch = temp;
        p->scratch.scratch_capacity = new_cap;
    }

    ARRAY_ELEMENT *scratch = (ARRAY_ELEMENT *) p->scratch.scratch;

    for (size_t i = 0; i < source_arr->size; i++) {
        scratch[i].value = source_arr->data[i];
        scratch[i].linear_index = (uint32_t) i;
    }

    size_t count = count_unique(p->scratch.scratch, source_arr->size);
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    new_shape[0] = (uint32_t) count;
    new_shape[1] = source_arr->ndim;

    size_t req_size = 0;
    if (get_array_size_from_shape(&req_size, 2U, new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSN_ARRAY *arr = p->array;
    size_t logical_size = count == 0 ? 0 : req_size;
    CSN_SLOT *out_slot = get_slot(reg, p->k_data.owned_handle);
    if (out_slot != NULL && CAN_REUSE_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, out_slot->array, 0.0, 0.0)) {
        p->handle->id = p->k_data.owned_handle;
        goto done;
    }

    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, 2U, new_shape, logical_size, source_arr->itype, err);
    if (res != OK) goto done;

    for (size_t i = 0; i < count; ++i) {
        uint32_t src_coords[CSN_MAX_DIMS] = {0};

        ARRAY_ELEMENT *elem = &p->scratch.scratch[i];
        from_linear_to_coords(src_coords, source_shape, elem->linear_index, source_arr->ndim);

        for (size_t j = 0; j < source_ndim; ++j) {
            arr->data[i * source_ndim + j] = (double) src_coords[j];
        }
    }

    SET_KDATA_END(p, new_shape, 2U, source_arr->itype);
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, arr, 0.0, 0.0);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_unique(CSOUND *csound, CSN_COMPARE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    const char *err = NULL;

    /* Declared before any goto: the done label frees it. */
    ARRAY_ELEMENT *temp = NULL;

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

    temp = csound->Calloc(csound, sizeof(ARRAY_ELEMENT) * source_arr->size);
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

done:
    csound->Free(csound, temp);
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_unique_k_init(CSOUND *csound, CSN_COMPARE *p) {
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

    size_t capacity = source_arr->size > 0 ? source_arr->size * 2 : 1;
    ARRAY_ELEMENT *temp = csound->Calloc(csound, sizeof(ARRAY_ELEMENT) * capacity);
    if (temp == NULL) {
        res = csound->InitError(csound, "[csnarray] Out of memory: allocation of %zu bytes failed", (size_t) (sizeof(ARRAY_ELEMENT) * source_arr->size));
        goto done;
    }
    p->scratch.scratch = temp;
    p->scratch.scratch_capacity = capacity;

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

int32_t csnarray_unique_k(CSOUND *csound, CSN_COMPARE *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REG_HANDLE(csound, &p->h, reg, p->k_data.owned_handle);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    const char *err = NULL;
    res = CHECK_SELF_ALIAS(csound, &p->h, &p->k_data, source_handle, 0);
    if (res != OK) return res;

    CHECK_KTRIG(compare_trig(&p->h, p->cmp_value, p->trig));

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

    size_t source_size = source_arr->size;
    if (source_size > p->scratch.scratch_capacity) {
        size_t new_cap = source_size * 2;
        ARRAY_ELEMENT *temp = csound->ReAlloc(csound, p->scratch.scratch, sizeof(ARRAY_ELEMENT) * new_cap);
        if (temp == NULL) {
            csound->UnlockMutex(reg->mutex);
            return csound->PerfError(csound, &p->h, "[csnarray] Out of memory: allocation of %zu bytes failed", (size_t) (sizeof(ARRAY_ELEMENT) * source_arr->size));
        }
         p->scratch.scratch = temp;
        p->scratch.scratch_capacity = new_cap;
    }

    ARRAY_ELEMENT *scratch = (ARRAY_ELEMENT *) p->scratch.scratch;

    for (size_t i = 0; i < source_arr->size; i++) {
        scratch[i].value = source_arr->data[i];
        scratch[i].linear_index = (uint32_t) i;
    }

    size_t count = count_unique(p->scratch.scratch, source_arr->size);
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    new_shape[0] = (uint32_t) count;

    size_t req_size = 0;
    if (get_array_size_from_shape(&req_size, 1U, new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSN_ARRAY *arr = NULL;
    size_t logical_size = count == 0 ? 0 : req_size;
    CSN_SLOT *out_slot = get_slot(reg, p->k_data.owned_handle);
    if (out_slot != NULL && CAN_REUSE_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, out_slot->array, 0.0, 0.0)) {
        p->handle->id = p->k_data.owned_handle;
        goto done;
    }

    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, 1U, new_shape, logical_size, source_arr->itype, err);
    if (res != OK) goto done;
    p->array = arr;

    for (size_t i = 0; i < count; ++i) {
        arr->data[i] = scratch[i].value;
    }

    SET_KDATA_END(p, new_shape, 1U, source_arr->itype);
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, arr, 0.0, 0.0);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_compare_helper(CSOUND *csound, CSN_COMPARE *p, CSN_COMPARE_MODE mode) {
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

static int32_t csnarray_compare_k_init(CSOUND *csound, CSN_COMPARE *p) {
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

    if (source_arr->size > 0) {
        memcpy(p->array->data, source_arr->data, sizeof(double) * source_arr->size);
        p->array->size = source_arr->size;
    }

    SET_KDATA_BEGIN(p, reg);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_compare_k_helper(CSOUND *csound, CSN_COMPARE *p, CSN_COMPARE_MODE mode) {
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
    double cmp_value = (double) *p->cmp_value;

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    memcpy(new_shape, source_arr->shape, sizeof(uint32_t) * CSN_MAX_DIMS);

    size_t req_size = 0;
    if (get_array_size_from_shape(&req_size, source_arr->ndim, new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    /* Comparing an array against a scalar writes each result cell from the cell
       it sits on, so feeding on its own output is legal as long as the layout
       holds still. */
    res = CHECK_SELF_ALIAS_CELL_LOCAL(csound, &p->h, &p->k_data, source_handle, source_arr, source_arr->ndim, new_shape, source_arr->itype);
    if (res != OK) goto done;

    CSN_SLOT *out_slot = get_slot(reg, p->k_data.owned_handle);
    if (out_slot != NULL && CAN_REUSE_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, out_slot->array, cmp_value, 0.0)) {
        p->handle->id = p->k_data.owned_handle;
        goto done;
    }

    CSN_ARRAY *arr = NULL;
    size_t logical_size = source_arr->size == 0 ? 0 : req_size;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, source_arr->ndim, new_shape, logical_size, source_arr->itype, err);
    if (res != OK) goto done;
    p->array = arr;

    /* Both branches are written, never just the matches: the slot is only
       reallocated when the request changes, so on a steady layout an unwritten
       cell would still be holding the 1.0 some earlier pass put there. */
    for (size_t i = 0; i < source_arr->size; ++i) {
        double value = source_arr->data[i];
        arr->data[i] = compare_match(value, cmp_value, mode) ? 1.0 : 0.0;
    }

    SET_KDATA_END(p, new_shape, source_arr->ndim, source_arr->itype);
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, arr, cmp_value, 0.0);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_greater_than(CSOUND *csound, CSN_COMPARE *p) {
    return csnarray_compare_helper(csound, p, GREATER_THAN);
}

int32_t csnarray_greater_than_k(CSOUND *csound, CSN_COMPARE *p) {
    return csnarray_compare_k_helper(csound, p, GREATER_THAN);
}

int32_t csnarray_less_than(CSOUND *csound, CSN_COMPARE *p) {
    return csnarray_compare_helper(csound, p, LESS_THAN);
}

int32_t csnarray_less_than_k(CSOUND *csound, CSN_COMPARE *p) {
    return csnarray_compare_k_helper(csound, p, LESS_THAN);
}

int32_t csnarray_not_equal(CSOUND *csound, CSN_COMPARE *p) {
    return csnarray_compare_helper(csound, p, NOT_EQUAL);
}

int32_t csnarray_not_equal_k(CSOUND *csound, CSN_COMPARE *p) {
    return csnarray_compare_k_helper(csound, p, NOT_EQUAL);
}

int32_t csnarray_greater_equal(CSOUND *csound, CSN_COMPARE *p) {
    return csnarray_compare_helper(csound, p, GREATER_EQUAL);
}

int32_t csnarray_greater_equal_k(CSOUND *csound, CSN_COMPARE *p) {
    return csnarray_compare_k_helper(csound, p, GREATER_EQUAL);
}

int32_t csnarray_less_equal(CSOUND *csound, CSN_COMPARE *p) {
    return csnarray_compare_helper(csound, p, LESS_EQUAL);
}

int32_t csnarray_less_equal_k(CSOUND *csound, CSN_COMPARE *p) {
    return csnarray_compare_k_helper(csound, p, LESS_EQUAL);
}

int32_t csnarray_equal(CSOUND *csound, CSN_COMPARE *p) {
    return csnarray_compare_helper(csound, p, EQUAL);
}

int32_t csnarray_equal_k(CSOUND *csound, CSN_COMPARE *p) {
    return csnarray_compare_k_helper(csound, p, EQUAL);
}

static int32_t csnarray_compare_count_helper(CSOUND *csound, CSN_COUNT *p, CSN_COMPARE_MODE mode) {
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

    /* NONZERO and IS_NAN take no comparison value, so their opcodes have no
       such argument to read. */
    size_t count = (mode == NONZERO || mode == IS_NAN)
        ? count_elements_from_value(source_arr, 0.0, mode)
        : count_elements_from_value(source_arr, (double) *p->arg_a, mode);

    *p->value = (MYFLT) count;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_compare_count_k_init(CSOUND *csound, CSN_COUNT *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    p->registry = reg;
    return OK;
}

static int32_t csnarray_compare_count_k_helper(CSOUND *csound, CSN_COUNT *p, CSN_COMPARE_MODE mode) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    uint32_t source_handle = p->source_handle->id;
    int32_t res = OK;

    const MYFLT *trig = mode == EQUAL ? p->arg_b : p->arg_a;
    CHECK_KTRIG(trig);

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

    double cmp_key = mode == EQUAL ? (double) *p->arg_a : 0.0;
    if (CAN_REUSE_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, NULL, cmp_key, 0.0)) {
        csound->UnlockMutex(reg->mutex);
        return res;
    }

    size_t count = (mode == NONZERO || mode == IS_NAN)
        ? count_elements_from_value(source_arr, 0.0, mode)
        : count_elements_from_value(source_arr, cmp_key, mode);

    *p->value = (MYFLT) count;
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, NULL, cmp_key, 0.0);

    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_count_equal(CSOUND *csound, CSN_COUNT *p) {
    return csnarray_compare_count_helper(csound, p, EQUAL);
}

int32_t csnarray_count_equal_k(CSOUND *csound, CSN_COUNT *p) {
    return csnarray_compare_count_k_helper(csound, p, EQUAL);
}

int32_t csnarray_count_nonzero(CSOUND *csound, CSN_COUNT *p) {
    return csnarray_compare_count_helper(csound, p, NONZERO);
}

int32_t csnarray_count_nonzero_k(CSOUND *csound, CSN_COUNT *p) {
    return csnarray_compare_count_k_helper(csound, p, NONZERO);
}

int32_t csnarray_count_nan(CSOUND *csound, CSN_COUNT *p) {
    return csnarray_compare_count_helper(csound, p, IS_NAN);
}

int32_t csnarray_count_nan_k(CSOUND *csound, CSN_COUNT *p) {
    return csnarray_compare_count_k_helper(csound, p, IS_NAN);
}

static void init_value_for_reduction(double *value, CSN_REDUCTION_MODE mode) {
    switch (mode) {
        case RED_SUM:
        case RED_MEAN:
        case RED_SUB:
        case RED_RMS:
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
        case RED_RMS:
            *value += x * x;
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
    if (mode == RED_MEAN || mode == RED_RMS) {
        double mean = *value / (double) source_arr->shape[axis];
        *value = mode == RED_RMS ? sqrt(mean) : mean;
    }
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

    if (mode == RED_MEAN || mode == RED_RMS) {
        double mean = *value / (double) source_arr->size;
        *value = mode == RED_RMS ? sqrt(mean) : mean;
    }
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


/* Validation only: the destination array is the caller's business, which is
   why no `out` parameter is handed back here. */
static int32_t accumulate_reduction_body(CSOUND *csound, OPDS *perf_h, CSN_REGISTRY *reg, uint32_t source_handle, CSNREF *out_handle, CSN_ARRAY **source_array, double in_axis, int32_t *out_axis, CSN_REDUCTION_MODE mode, MYFLT *out_value, COMPLEXDAT *out_complex_value) {
    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
    }

    *source_array = source_slot->array;
    CSN_ARRAY *source_arr = *source_array;
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    if (out_handle != NULL && !IS_VALID_AXIS(in_axis, source_ndim)) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: finite integers 0..%u)", in_axis, source_ndim, source_ndim - 1);
    }
    *out_axis = (int32_t) in_axis;

    if (mode == RED_MIN || mode == RED_MAX || mode == RED_MEDIAN || mode == RED_ARGMIN || mode == RED_ARGMAX) {
        if (source_arr->itype == CSN_COMPLEX) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Ordering is undefined for complex arrays, so this reduction is not available");
        }
    }

    if (source_arr->itype == CSN_COMPLEX && mode == RED_RMS) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Rms is defined for real array only");
    }

    if (*out_axis == -1) {
        if (source_arr->itype == CSN_COMPLEX && out_complex_value == NULL) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Handle holds a complex array; declare the result as :Complex;");
        }
        if (source_arr->itype == CSN_REAL && out_value == NULL) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Handle holds a real array; declare the result as i");
        }
    }

    size_t reduced_extent = (*out_axis == -1) ? source_arr->size : source_shape[*out_axis];
    if (reduced_extent == 0 && (mode == RED_MIN || mode == RED_MAX || mode == RED_SUB)) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Min, max and sub are undefined over an empty extent (axis %d has 0 elements)", *out_axis);
    }

    return OK;
}

static void accumulation_reduction_assign_value(CSN_ARRAY *source_arr, CSN_ARRAY *destination, int32_t axis, MYFLT *out_value, COMPLEXDAT *out_complex_value, CSN_REDUCTION_MODE mode) {
    if (destination != NULL) {
        for (size_t linear = 0; linear < destination->size; ++linear) {
            uint32_t dst_coords[CSN_MAX_DIMS] = {0};
            uint32_t src_coords[CSN_MAX_DIMS] = {0};

            from_linear_to_coords(dst_coords, destination->shape, linear, destination->ndim);
            if (source_arr->itype == CSN_REAL) {
                double value = 0.0;
                accumulate_reduction_axis_helper(&value, destination, source_arr, src_coords, dst_coords, mode, axis);
                destination->data[linear] = value;
            } else {
                CSN_COMPLEXDAT c = { 0.0, 0.0 };
                accumulate_reductioncomp_axis_helper(&c, destination, source_arr, src_coords, dst_coords, mode, axis);
                destination->data[linear * 2] = c.re;
                destination->data[linear * 2 + 1] = c.im;
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
}

/* axis == -1 collapses to out_value; any other axis builds an array through
   out_handle/out_array. Exactly one pair is non-NULL, which is what keeps the
   two opcode families distinct at the type level. */
static int32_t csnarray_accumulate_reduction(CSOUND *csound, const OPDS *h, CSNREF *src_ref, double axis_value, CSNREF *out_handle, CSN_ARRAY **out_array, MYFLT *out_value, COMPLEXDAT *out_complex_value, CSN_REDUCTION_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = src_ref->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    CSN_ARRAY *arr = NULL;
    int32_t axis = -1;
    res = accumulate_reduction_body(csound, NULL, reg, source_handle, out_handle, &source_arr, axis_value, &axis, mode, out_value, out_complex_value);
    if (res != OK) goto done;

    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

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

    accumulation_reduction_assign_value(source_arr, arr, axis, out_value, out_complex_value, mode);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_accumulate_reduction_k_init_helper(CSOUND *csound, const OPDS *h, CSNREF *src_ref, double axis_value, CSNREF *out_handle, CSN_ARRAY **out_array, MYFLT *out_value, COMPLEXDAT *out_complex_value, CSN_REDUCTION_MODE mode, K_DATA *k_data) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = src_ref->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    CSN_ARRAY *arr = NULL;
    int32_t axis = -1;
    res = accumulate_reduction_body(csound, NULL, reg, source_handle, out_handle, &source_arr, axis_value, &axis, mode, out_value, out_complex_value);
    if (res != OK) goto done;

    if (axis != -1) {
        const uint32_t protect[1] = { source_handle };
        if (create_csnarray_locked(csound, reg, h, source_arr->ndim, source_arr->shape, out_array, out_handle, protect, 1U, &err, source_arr->itype) != OK) {
            res = csound->InitError(csound, "[csnarray] %s", err);
            goto done;
        }

        arr = *out_array;
    }

    if (arr != NULL) {
        if (source_arr->size > 0 ) {
            memcpy(arr->data, source_arr->data, sizeof(double) * source_arr->size * source_arr->itype);
            arr->size = source_arr->size;
        }
        memset(k_data->prev_shape, 0, sizeof(k_data->prev_shape));
        memcpy(k_data->prev_shape, arr->shape, sizeof(k_data->prev_shape));
        k_data->prev_ndim = arr->ndim;
        k_data->owned_handle = out_handle->id;
    } else {
        /* Scalar forms publish a usable value already at i-time, so a gated
           .c.k overload is not left holding an unwritten output. */
        accumulation_reduction_assign_value(source_arr, NULL, axis, out_value, out_complex_value, mode);
    }

    /* The scalar forms own no slot, so owned_handle stays 0 and the itype has
       to come from the source: arr is NULL for them. */
    k_data->prev_itype = source_arr->itype;
    k_data->registry = reg;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_accumulate_reduction_k(CSOUND *csound, OPDS *h, CSNREF *src_ref, double axis_value, CSNREF *out_handle, CSN_ARRAY **out_array, MYFLT *out_value, COMPLEXDAT *out_complex_value, CSN_REDUCTION_MODE mode, K_DATA *k_data, const MYFLT *trig) {
    CSN_REGISTRY *reg = k_data->registry;
    /* Only the axis forms own an output slot; the scalar ones write a number
       and leave owned_handle at 0. */
    if (reg == NULL || (out_handle != NULL && k_data->owned_handle == 0)) {
        return csound->PerfError(csound, h, "[csnarray] k-rate output slot was not initialized");
    }

    CHECK_KTRIG(trig);

    uint32_t source_handle = src_ref->id;
    int32_t res = CHECK_SELF_ALIAS(csound, h, k_data, source_handle, 0);
    if (res != OK) return res;

    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    CSN_ARRAY *arr = NULL;
    int32_t axis = -1;
    res = accumulate_reduction_body(csound, h, reg, source_handle, out_handle, &source_arr, axis_value, &axis, mode, out_value, out_complex_value);
    if (res != OK) goto done;

    /* The axis forms answer from their own slot, the scalar ones from the
       MYFLT they wrote last time; either way nothing has to be walked again
       while the source and the axis hold still. */
    CSN_SLOT *out_slot = out_handle != NULL ? get_slot(reg, k_data->owned_handle) : NULL;
    if ((out_handle == NULL || out_slot != NULL)
        && CAN_REUSE_ELEMENTWISE(k_data, source_handle, source_arr, 0, NULL, out_slot != NULL ? out_slot->array : NULL, axis_value, 0.0)) {
        if (out_handle != NULL) out_handle->id = k_data->owned_handle;
        goto done;
    }

    uint32_t new_dim = source_arr->ndim - 1;
    if (axis != -1) {
        uint32_t new_shape[CSN_MAX_DIMS] = {0};
        for (uint32_t i = 0, j = 0; i < source_arr->ndim; i++) {
            if (i != (uint32_t) axis) new_shape[j++] = source_arr->shape[i];
        }

        size_t requested_size = 0;
        if (get_array_size_from_shape(&requested_size, new_dim, new_shape) != OK) {
            csound->UnlockMutex(reg->mutex);
            return csound->PerfError(csound, h, "[csnarray] Invalid shape or element count exceeds the configured limit");
        }

        size_t logical_size = source_arr->size == 0 ? 0 : requested_size;
        res = NEED_TO_UPDATE_SLOT(csound, h, &arr, k_data, NULL, new_dim, new_shape, logical_size, source_arr->itype, err);
        if (res != OK) goto done;

        *out_array = arr;
    }

    accumulation_reduction_assign_value(source_arr, arr, axis, out_value, out_complex_value, mode);

    if (arr != NULL) {
        memset(k_data->prev_shape, 0, sizeof(k_data->prev_shape));
        memcpy(k_data->prev_shape, arr->shape, sizeof(k_data->prev_shape));
        k_data->prev_ndim = arr->ndim;
        out_handle->id = k_data->owned_handle;
    }

    k_data->prev_itype = source_arr->itype;
    PUBLISH_ELEMENTWISE(k_data, source_handle, source_arr, 0, NULL, arr, axis_value, 0.0);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_sum(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, (double) *p->axis, p->handle, &p->array, NULL, NULL, RED_SUM);
}

static int32_t csnarray_sum_k_init(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction_k_init_helper(csound, &p->h, p->source_handle, (double) *p->axis, p->handle, &p->array, NULL, NULL, RED_SUM, &p->k_data);
}

int32_t csnarray_sum_k(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction_k(csound, &p->h, p->source_handle, (double) *p->axis, p->handle, &p->array, NULL, NULL, RED_SUM, &p->k_data, p->trig);
}

int32_t csnarray_sum_all(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, NULL, RED_SUM);
}

static int32_t csnarray_sum_all_k_init(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_accumulate_reduction_k_init_helper(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, NULL, RED_SUM, &p->k_data);
}

int32_t csnarray_sum_all_k(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_accumulate_reduction_k(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, NULL, RED_SUM, &p->k_data, p->trig);
}

int32_t csnarray_sumcomp_all(CSOUND *csound, CSN_REDUCTION_COMPLEX_S *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, -1, NULL, NULL, NULL, p->value, RED_SUM);
}

static int32_t csnarray_sumcomp_all_k_init(CSOUND *csound, CSN_REDUCTION_COMPLEX_S *p) {
    return csnarray_accumulate_reduction_k_init_helper(csound, &p->h, p->source_handle, -1, NULL, NULL, NULL, p->value, RED_SUM, &p->k_data);
}

int32_t csnarray_sumcomp_all_k(CSOUND *csound, CSN_REDUCTION_COMPLEX_S *p) {
    CHECK_KTRIG(p->trig);
    return csnarray_accumulate_reduction_k(csound, &p->h, p->source_handle, -1, NULL, NULL, NULL, p->value, RED_SUM, &p->k_data, p->trig);
}

int32_t csnarray_prod(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, (double) *p->axis, p->handle, &p->array, NULL, NULL, RED_PROD);
}

static int32_t csnarray_prod_k_init(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction_k_init_helper(csound, &p->h, p->source_handle, (double) *p->axis, p->handle, &p->array, NULL, NULL, RED_PROD, &p->k_data);
}

int32_t csnarray_prod_k(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction_k(csound, &p->h, p->source_handle, (double) *p->axis, p->handle, &p->array, NULL, NULL, RED_PROD, &p->k_data, p->trig);
}

int32_t csnarray_prod_all(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, NULL, RED_PROD);
}

static int32_t csnarray_prod_all_k_init(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_accumulate_reduction_k_init_helper(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, NULL, RED_PROD, &p->k_data);
}

int32_t csnarray_prod_all_k(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_accumulate_reduction_k(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, NULL, RED_PROD, &p->k_data, p->trig);
}

int32_t csnarray_prodcomp_all(CSOUND *csound, CSN_REDUCTION_COMPLEX_S *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, -1, NULL, NULL, NULL, p->value, RED_PROD);
}

static int32_t csnarray_prodcomp_all_k_init(CSOUND *csound, CSN_REDUCTION_COMPLEX_S *p) {
    return csnarray_accumulate_reduction_k_init_helper(csound, &p->h, p->source_handle, -1, NULL, NULL, NULL, p->value, RED_PROD, &p->k_data);
}

int32_t csnarray_prodcomp_all_k(CSOUND *csound, CSN_REDUCTION_COMPLEX_S *p) {
    CHECK_KTRIG(p->trig);
    return csnarray_accumulate_reduction_k(csound, &p->h, p->source_handle, -1, NULL, NULL, NULL, p->value, RED_PROD, &p->k_data, p->trig);
}

int32_t csnarray_sub(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, (double) *p->axis, p->handle, &p->array, NULL, NULL, RED_SUB);
}

static int32_t csnarray_sub_k_init(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction_k_init_helper(csound, &p->h, p->source_handle, (double) *p->axis, p->handle, &p->array, NULL, NULL, RED_SUB, &p->k_data);
}

int32_t csnarray_sub_k(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction_k(csound, &p->h, p->source_handle, (double) *p->axis, p->handle, &p->array, NULL, NULL, RED_SUB, &p->k_data, p->trig);
}

int32_t csnarray_sub_all(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, NULL, RED_SUB);
}

int32_t csnarray_sub_all_k(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_accumulate_reduction_k(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, NULL, RED_SUB, &p->k_data, p->trig);
}

int32_t csnarray_sub_all_k_init(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_accumulate_reduction_k_init_helper(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, NULL, RED_SUB, &p->k_data);
}

int32_t csnarray_subcomp_all(CSOUND *csound, CSN_REDUCTION_COMPLEX_S *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, -1, NULL, NULL, NULL, p->value, RED_SUB);
}

int32_t csnarray_subcomp_all_k(CSOUND *csound, CSN_REDUCTION_COMPLEX_S *p) {
    CHECK_KTRIG(p->trig);
    return csnarray_accumulate_reduction_k(csound, &p->h, p->source_handle, -1, NULL, NULL, NULL, p->value, RED_SUB, &p->k_data, p->trig);
}

static int32_t csnarray_subcomp_all_k_init(CSOUND *csound, CSN_REDUCTION_COMPLEX_S *p) {
    return csnarray_accumulate_reduction_k_init_helper(csound, &p->h, p->source_handle, -1, NULL, NULL, NULL, p->value, RED_SUB, &p->k_data);
}

int32_t csnarray_mean(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, (double) *p->axis, p->handle, &p->array, NULL, NULL, RED_MEAN);
}

int32_t csnarray_mean_k(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction_k(csound, &p->h, p->source_handle, (double) *p->axis, p->handle, &p->array, NULL, NULL, RED_MEAN, &p->k_data, p->trig);
}

static int32_t csnarray_mean_k_init(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction_k_init_helper(csound, &p->h, p->source_handle, (double) *p->axis, p->handle, &p->array, NULL, NULL, RED_MEAN, &p->k_data);
}

int32_t csnarray_mean_all(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, NULL, RED_MEAN);
}

int32_t csnarray_mean_all_k(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_accumulate_reduction_k(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, NULL, RED_MEAN, &p->k_data, p->trig);
}

static int32_t csnarray_mean_all_k_init(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_accumulate_reduction_k_init_helper(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, NULL, RED_MEAN, &p->k_data);
}

int32_t csnarray_meancomp_all(CSOUND *csound, CSN_REDUCTION_COMPLEX_S *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, -1, NULL, NULL, NULL, p->value, RED_MEAN);
}

static int32_t csnarray_meancomp_all_k_init(CSOUND *csound, CSN_REDUCTION_COMPLEX_S *p) {
    return csnarray_accumulate_reduction_k_init_helper(csound, &p->h, p->source_handle, -1, NULL, NULL, NULL, p->value, RED_MEAN, &p->k_data);
}

int32_t csnarray_meancomp_all_k(CSOUND *csound, CSN_REDUCTION_COMPLEX_S *p) {
    CHECK_KTRIG(p->trig);
    return csnarray_accumulate_reduction_k(csound, &p->h, p->source_handle, -1, NULL, NULL, NULL, p->value, RED_MEAN, &p->k_data, p->trig);
}

int32_t csnarray_min(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, (double) *p->axis, p->handle, &p->array, NULL, NULL, RED_MIN);
}

static int32_t csnarray_min_k_init(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction_k_init_helper(csound, &p->h, p->source_handle, (double) *p->axis, p->handle, &p->array, NULL, NULL, RED_MIN, &p->k_data);
}

int32_t csnarray_min_k(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction_k(csound, &p->h, p->source_handle, (double) *p->axis, p->handle, &p->array, NULL, NULL, RED_MIN, &p->k_data, p->trig);
}

int32_t csnarray_min_all(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, NULL, RED_MIN);
}

static int32_t csnarray_min_all_k_init(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_accumulate_reduction_k_init_helper(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, NULL, RED_MIN, &p->k_data);
}

int32_t csnarray_min_all_k(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_accumulate_reduction_k(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, NULL, RED_MIN, &p->k_data, p->trig);
}

int32_t csnarray_max(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, (double) *p->axis, p->handle, &p->array, NULL, NULL, RED_MAX);
}

static int32_t csnarray_max_k_init(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction_k_init_helper(csound, &p->h, p->source_handle, (double) *p->axis, p->handle, &p->array, NULL, NULL, RED_MAX, &p->k_data);
}

int32_t csnarray_max_k(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction_k(csound, &p->h, p->source_handle, (double) *p->axis, p->handle, &p->array, NULL, NULL, RED_MAX, &p->k_data, p->trig);
}

int32_t csnarray_max_all(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, NULL, RED_MAX);
}

static int32_t csnarray_max_all_k_init(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_accumulate_reduction_k_init_helper(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, NULL, RED_MAX, &p->k_data);
}

int32_t csnarray_max_all_k(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_accumulate_reduction_k(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, NULL, RED_MAX, &p->k_data, p->trig);
}

int32_t csnarray_all(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, (double) *p->axis, p->handle, &p->array, NULL, NULL, RED_ALL);
}

static int32_t csnarray_all_k_init(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction_k_init_helper(csound, &p->h, p->source_handle, (double) *p->axis, p->handle, &p->array, NULL, NULL, RED_ALL, &p->k_data);
}

int32_t csnarray_all_k(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction_k(csound, &p->h, p->source_handle, (double) *p->axis, p->handle, &p->array, NULL, NULL, RED_ALL, &p->k_data, p->trig);
}

int32_t csnarray_all_all(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, NULL, RED_ALL);
}

static int32_t csnarray_all_all_k_init(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_accumulate_reduction_k_init_helper(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, NULL, RED_ALL, &p->k_data);
}

int32_t csnarray_all_all_k(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_accumulate_reduction_k(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, NULL, RED_ALL, &p->k_data, p->trig);
}

int32_t csnarray_any(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, (double) *p->axis, p->handle, &p->array, NULL, NULL, RED_ANY);
}

int32_t csnarray_any_k(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction_k(csound, &p->h, p->source_handle, (double) *p->axis, p->handle, &p->array, NULL, NULL, RED_ANY, &p->k_data, p->trig);
}

static int32_t csnarray_any_k_init(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction_k_init_helper(csound, &p->h, p->source_handle, (double) *p->axis, p->handle, &p->array, NULL, NULL, RED_ANY, &p->k_data);
}

int32_t csnarray_any_all(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, NULL, RED_ANY);
}

static int32_t csnarray_any_all_k_init(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_accumulate_reduction_k_init_helper(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, NULL, RED_ANY, &p->k_data);
}

int32_t csnarray_any_all_k(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_accumulate_reduction_k(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, NULL, RED_ANY, &p->k_data, p->trig);
}

int32_t csnarray_rms(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, (double) *p->axis, p->handle, &p->array, NULL, NULL, RED_RMS);
}

static int32_t csnarray_rms_k_init(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction_k_init_helper(csound, &p->h, p->source_handle, (double) *p->axis, p->handle, &p->array, NULL, NULL, RED_RMS, &p->k_data);
}

int32_t csnarray_rms_k(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction_k(csound, &p->h, p->source_handle, (double) *p->axis, p->handle, &p->array, NULL, NULL, RED_RMS, &p->k_data, p->trig);
}

int32_t csnarray_rms_all(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, NULL, RED_RMS);
}

static int32_t csnarray_rms_all_k_init(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_accumulate_reduction_k_init_helper(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, NULL, RED_RMS, &p->k_data);
}

int32_t csnarray_rms_all_k(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_accumulate_reduction_k(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, NULL, RED_RMS, &p->k_data, p->trig);
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

static int32_t stdvar_body(CSOUND *csound, OPDS *perf_h, CSN_REGISTRY *reg, CSN_ARRAY **source_array, uint32_t source_handle, CSNREF *out_handle, const MYFLT *in_axis, int32_t *out_axis) {
    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
    }

    *source_array = source_slot->array;
    CSN_ARRAY *source_arr = *source_array;
    uint32_t source_ndim = source_arr->ndim;

    double axis_value = (double) *in_axis;
    if (out_handle != NULL && !IS_VALID_AXIS(axis_value, source_ndim)) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: finite integers 0..%u)", axis_value, source_ndim, source_ndim - 1);
    }
    *out_axis = (int32_t) *in_axis;
    return OK;
}

static int32_t stdvar_assign_value(CSOUND *csound, OPDS *perf_h, CSN_ARRAY *source_arr, CSN_ARRAY *destination, MYFLT *out_value, int32_t axis, CSN_REDUCTION_MODE mode) {
    if (destination != NULL) {
        for (size_t linear = 0; linear < destination->size; ++linear) {
            uint32_t dst_coords[CSN_MAX_DIMS] = {0};
            uint32_t src_coords[CSN_MAX_DIMS] = {0};

            from_linear_to_coords(dst_coords, destination->shape, linear, destination->ndim);
            uint32_t axis_size = source_arr->shape[axis];
            double value = 0.0;
            /* NOTOK -> empty extension */
            if (stdvar_calculation_helper(&value, src_coords, dst_coords, source_arr, axis_size, axis, mode) != OK) {
                return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Variance is undefined over an empty extent (axis %d has 0 elements)", axis);
            }
            destination->data[linear] = value;
        }
    } else {
        size_t size = source_arr->size;
        double value = 0;
        if (stdvar_calculation_scalar_helper(&value, source_arr, (uint32_t) size, mode) != OK) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Variance is undefined over an empty array (0 elements)");
        }
        *out_value = (MYFLT) value;
    }
    return OK;
}

static int32_t csnarray_stdvar_helper(CSOUND *csound, const OPDS *h, CSNREF *src_ref, double axis_value, CSNREF *out_handle, CSN_ARRAY **out_array, MYFLT *out_value, CSN_REDUCTION_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = src_ref->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    int32_t axis = 0;
    res = stdvar_body(csound, NULL, reg, &source_arr, source_handle, out_handle, &axis_value, &axis);
    if (res != OK) goto done;

    CSN_ARRAY *arr = NULL;
    if (axis != -1) {
        uint32_t new_shape[CSN_MAX_DIMS] = {0};
        for (uint32_t i = 0, j = 0; i < source_arr->ndim; i++) {
            if (i != (uint32_t) axis) new_shape[j++] = source_arr->shape[i];
        }

        const uint32_t protect[1] = { source_handle };

        /* E[|z - media|^2] */
        if (create_csnarray_locked(csound, reg, h, source_arr->ndim - 1, new_shape, out_array, out_handle, protect, 1U, &err, CSN_REAL) != OK) {
            res = csound->InitError(csound, "[csnarray] %s", err);
            goto done;
        }

        arr = *out_array;
    }

    res = stdvar_assign_value(csound, NULL, source_arr, arr, out_value, axis, mode);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_stdvar_k_helper(CSOUND *csound, OPDS *h, CSNREF *src_ref, double axis_value, CSNREF *out_handle, CSN_ARRAY **out_array, MYFLT *out_value, CSN_REDUCTION_MODE mode, K_DATA *k_data, const MYFLT *trig) {
    CSN_REGISTRY *reg = k_data->registry;
    if (reg == NULL || (out_handle != NULL && k_data->owned_handle == 0)) {
        return csound->PerfError(csound, h, "[csnarray] k-rate output slot was not initialized");
    }

    CHECK_KTRIG(trig);

    uint32_t source_handle = src_ref->id;

    int32_t res = CHECK_SELF_ALIAS(csound, h, k_data, source_handle, 0);
    if (res != OK) return res;

    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    int32_t axis = 0;
    res = stdvar_body(csound, h, reg, &source_arr, source_handle, out_handle, &axis_value, &axis);
    if (res != OK) goto done;

    CSN_SLOT *reuse_slot = out_handle != NULL ? get_slot(reg, k_data->owned_handle) : NULL;
    if ((out_handle == NULL || reuse_slot != NULL)
        && CAN_REUSE_ELEMENTWISE(k_data, source_handle, source_arr, 0, NULL, reuse_slot != NULL ? reuse_slot->array : NULL, axis_value, 0.0)) {
        if (out_handle != NULL) out_handle->id = k_data->owned_handle;
        goto done;
    }

    CSN_ARRAY *arr = NULL;
    if (axis != -1) {
        uint32_t new_dim = source_arr->ndim - 1;
        uint32_t new_shape[CSN_MAX_DIMS] = {0};
        for (uint32_t i = 0, j = 0; i < source_arr->ndim; i++) {
            if (i != (uint32_t) axis) new_shape[j++] = source_arr->shape[i];
        }

        size_t requested_size = 0;
        if (get_array_size_from_shape(&requested_size, new_dim, new_shape) != OK) {
            csound->UnlockMutex(reg->mutex);
            return csound->PerfError(csound, h, "[csnarray] Invalid shape or element count exceeds the configured limit");
        }

        size_t logical_size = source_arr->size == 0 ? 0 : requested_size;
        res = NEED_TO_UPDATE_SLOT(csound, h, &arr, k_data, NULL, new_dim, new_shape, logical_size, source_arr->itype, err);
        if (res != OK) goto done;

        *out_array = arr;
    }

    res = stdvar_assign_value(csound, h, source_arr, arr, out_value, axis, mode);
    if (res != OK) goto done;

    if (arr != NULL) {
        memset(k_data->prev_shape, 0, sizeof(k_data->prev_shape));
        memcpy(k_data->prev_shape, arr->shape, sizeof(k_data->prev_shape));
        k_data->prev_ndim = arr->ndim;
        out_handle->id = k_data->owned_handle;
    }

    k_data->prev_itype = source_arr->itype;
    PUBLISH_ELEMENTWISE(k_data, source_handle, source_arr, 0, NULL, arr, axis_value, 0.0);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

/* std and var cannot borrow the accumulate init: the streaming fold has no
   RED_STD/RED_VAR case, so it would leave the gated scalar output at zero
   until the first trigger. */
static int32_t csnarray_stdvar_k_init_helper(CSOUND *csound, const OPDS *h, CSNREF *src_ref, double axis_value, CSNREF *out_handle, CSN_ARRAY **out_array, MYFLT *out_value, CSN_REDUCTION_MODE mode, K_DATA *k_data) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = src_ref->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    CSN_ARRAY *arr = NULL;
    int32_t axis = -1;
    MYFLT in_axis = (MYFLT) axis_value;
    res = stdvar_body(csound, NULL, reg, &source_arr, source_handle, out_handle, &in_axis, &axis);
    if (res != OK) goto done;

    if (axis != -1) {
        const uint32_t protect[1] = { source_handle };
        if (create_csnarray_locked(csound, reg, h, source_arr->ndim, source_arr->shape, out_array, out_handle, protect, 1U, &err, source_arr->itype) != OK) {
            res = csound->InitError(csound, "[csnarray] %s", err);
            goto done;
        }

        arr = *out_array;
    }

    if (arr != NULL) {
        if (source_arr->size > 0) {
            memcpy(arr->data, source_arr->data, sizeof(double) * source_arr->size * source_arr->itype);
            arr->size = source_arr->size;
        }
        memset(k_data->prev_shape, 0, sizeof(k_data->prev_shape));
        memcpy(k_data->prev_shape, arr->shape, sizeof(k_data->prev_shape));
        k_data->prev_ndim = arr->ndim;
        k_data->owned_handle = out_handle->id;
    } else {
        res = stdvar_assign_value(csound, NULL, source_arr, NULL, out_value, axis, mode);
        if (res != OK) goto done;
    }

    k_data->prev_itype = source_arr->itype;
    k_data->registry = reg;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_std(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_stdvar_helper(csound, &p->h, p->source_handle, (double) *p->axis, p->handle, &p->array, NULL, RED_STD);
}

static int32_t csnarray_std_k_init(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_stdvar_k_init_helper(csound, &p->h, p->source_handle, (double) *p->axis, p->handle, &p->array, NULL, RED_STD, &p->k_data);
}

int32_t csnarray_std_k(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_stdvar_k_helper(csound, &p->h, p->source_handle, (double) *p->axis, p->handle, &p->array, NULL, RED_STD, &p->k_data, p->trig);
}

int32_t csnarray_std_all(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_stdvar_helper(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, RED_STD);
}

static int32_t csnarray_std_all_k_init(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_stdvar_k_init_helper(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, RED_STD, &p->k_data);
}

int32_t csnarray_std_all_k(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_stdvar_k_helper(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, RED_STD, &p->k_data, p->trig);
}

int32_t csnarray_var(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_stdvar_helper(csound, &p->h, p->source_handle, (double) *p->axis, p->handle, &p->array, NULL, RED_VAR);
}

static int32_t csnarray_var_k_init(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_stdvar_k_init_helper(csound, &p->h, p->source_handle, (double) *p->axis, p->handle, &p->array, NULL, RED_VAR, &p->k_data);
}

int32_t csnarray_var_k(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_stdvar_k_helper(csound, &p->h, p->source_handle, (double) *p->axis, p->handle, &p->array, NULL, RED_VAR, &p->k_data, p->trig);
}

int32_t csnarray_var_all(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_stdvar_helper(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, RED_VAR);
}

static int32_t csnarray_var_all_k_init(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_stdvar_k_init_helper(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, RED_VAR, &p->k_data);
}

int32_t csnarray_var_all_k(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_stdvar_k_helper(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, RED_VAR, &p->k_data, p->trig);
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

static int32_t argminmax_body(CSOUND *csound, OPDS *perf_h, uint32_t source_handle, CSN_REGISTRY *reg, CSN_ARRAY **source_array, double in_axis, int32_t *out_axis) {
    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
    }

    *source_array = source_slot->array;
    CSN_ARRAY *source_arr = *source_array;
    uint32_t source_ndim = source_arr->ndim;

    if (source_arr->itype == CSN_COMPLEX) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] argmin/argmax not allowed for complex array");
    }

    if (in_axis != -1.0 && !IS_VALID_AXIS(in_axis, source_ndim)) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: -1 for all axes, or finite integers 0..%u)", in_axis, source_ndim, source_ndim - 1);
    }
    *out_axis = (int32_t) in_axis;
    return OK;
}

static void argminmax_assign_value(CSN_ARRAY *source_arr, CSN_ARRAY *destination, int32_t axis, size_t count, uint32_t ndim, uint32_t *shape, CSN_REDUCTION_MODE mode) {
    if (axis != -1) {
        for (size_t linear = 0; linear < count; ++linear) {
            uint32_t dst_coords[CSN_MAX_DIMS] = {0};
            uint32_t src_coords[CSN_MAX_DIMS] = {0};

            from_linear_to_coords(dst_coords, shape, linear, ndim);
            dispatch_argminmax(source_arr, axis, src_coords, dst_coords, mode);
            for (uint32_t d = 0; d < source_arr->ndim; ++d) {
                destination->data[linear * source_arr->ndim + d] = (double) src_coords[d];
            }
        }
    } else {
        uint32_t src_coords[CSN_MAX_DIMS] = {0};
        dispatch_argminmax_all_axes(source_arr, src_coords, mode);
        for (uint32_t d = 0; d < source_arr->ndim; ++d) {
            destination->data[d] = (double) src_coords[d];
        }
    }
}

static int32_t argminmax_helper(CSOUND *csound, CSN_REDUCTION *p, CSN_REDUCTION_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    int32_t axis = 0;
    res = argminmax_body(csound, NULL, source_handle, reg, &source_arr, (double) *p->axis, &axis);
    if (res != OK) goto done;

    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

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
    argminmax_assign_value(source_arr, arr, axis, count, reduced_ndim, reduced_shape, mode);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t argminmax_k_init_helper(CSOUND *csound, CSN_REDUCTION *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    int32_t axis = 0;
    res = argminmax_body(csound, NULL, source_handle, reg, &source_arr, (double) *p->axis, &axis);
    if (res != OK) goto done;

    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

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
    if (arr->size > 0) {
        memset(arr->data, 0, sizeof(double) * p->array->size);
    }

    SET_KDATA_BEGIN(p, reg);
    p->k_data.prev_size = arr->size;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t argminmax_k_helper(CSOUND *csound, CSN_REDUCTION *p, CSN_REDUCTION_MODE mode) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REG_HANDLE(csound, &p->h, reg, p->k_data.owned_handle);

    CHECK_KTRIG(p->trig);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = CHECK_SELF_ALIAS(csound, &p->h, &p->k_data, source_handle, 0);
    if (res != OK) return res;

    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    int32_t axis = 0;
    res = argminmax_body(csound, &p->h, source_handle, reg, &source_arr, (double) *p->axis, &axis);
    if (res != OK) goto done;

    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

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

    /* One row of coordinates per reduced position, exactly like the i-rate
       form: reduced_shape only decomposes the destination coordinates, it is
       not the result's own layout. Sizing the slot from it would leave
       argminmax_assign_value writing count * source_ndim doubles into count. */
    /* Full width: SET_KDATA_END below copies CSN_MAX_DIMS entries out of it. */
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    new_shape[0] = (uint32_t) count;
    new_shape[1] = source_ndim;

    size_t requested_size = 0;
    if (get_array_size_from_shape(&requested_size, 2U, new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSN_ARRAY *arr = p->array;
    size_t logical_size = source_arr->size == 0 ? 0 : requested_size;
    CSN_SLOT *out_slot = get_slot(reg, p->k_data.owned_handle);
    if (out_slot != NULL && CAN_REUSE_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, out_slot->array, (double) axis, 0.0)) {
        p->handle->id = p->k_data.owned_handle;
        goto done;
    }

    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, 2U, new_shape, logical_size, source_arr->itype, err);
    if (res != OK) goto done;

    argminmax_assign_value(source_arr, arr, axis, count, reduced_ndim, reduced_shape, mode);
    SET_KDATA_END(p, new_shape, 2U, source_arr->itype);
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, arr, (double) axis, 0.0);
    p->k_data.prev_size = arr->size;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}


static int32_t median_body(CSOUND *csound, OPDS *perf_h, CSN_REGISTRY *reg, CSN_ARRAY **source_array, uint32_t source_handle, CSNREF *out_handle, double in_axis, int32_t *out_axis) {
    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
    }

    *source_array = source_slot->array;
    CSN_ARRAY *source_arr = *source_array;
    uint32_t source_ndim = source_arr->ndim;

    if (source_arr->itype == CSN_COMPLEX) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Median not allowed for complex array");
    }

    if (out_handle != NULL && !IS_VALID_AXIS(in_axis, source_ndim)) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: finite integers 0..%u)", in_axis, source_ndim, source_ndim - 1);
    }
    *out_axis = (int32_t) in_axis;

    return OK;
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

static void median_assign_value(CSN_ARRAY *source_arr, CSN_ARRAY *destination, double *scratch, size_t run_size, int32_t axis) {
    for (size_t linear = 0; linear < destination->size; ++linear) {
        uint32_t dst_coords[CSN_MAX_DIMS] = {0};
        uint32_t src_coords[CSN_MAX_DIMS] = {0};

        from_linear_to_coords(dst_coords, destination->shape, linear, destination->ndim);

        for (uint32_t k = 0; k < run_size; ++k) {
            for (uint32_t i = 0, j = 0; i < source_arr->ndim; ++i) {
                src_coords[i] = (i == (uint32_t) axis) ? k : dst_coords[j++];
            }
            uint32_t off = from_coords_to_offset(src_coords, source_arr->strides, source_arr->ndim);
            scratch[k] = source_arr->data[off];
        }

        destination->data[linear] = median_of_scratch(scratch, run_size);
    }
}

static int32_t csnarray_median_impl(CSOUND *csound, const OPDS *h, CSNREF *src_ref, double axis_value, CSNREF *out_handle, CSN_ARRAY **out_array, MYFLT *out_value) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = src_ref->id;

    int32_t res = OK;
    const char *err = NULL;
    double *scratch = NULL;

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    int32_t axis = 0;
    res = median_body(csound, NULL, reg, &source_arr, source_handle, out_handle, axis_value, &axis);
    if (res != OK) goto done;

    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

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
    median_assign_value(source_arr, arr, scratch, run, axis);

done:
    csound->UnlockMutex(reg->mutex);
    if (scratch != NULL) {
        csound->Free(csound, scratch);
    }
    return res;
}

static int32_t csnarray_median_scalar_k_deinit(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    if (p->scratch.scratch != NULL) {
        csound->Free(csound, p->scratch.scratch);
    }
    return OK;
}

static int32_t csnarray_median_impl_k_init(CSOUND *csound, OPDS *h, CSNREF *src_ref, double axis_value, CSNREF *out_handle, CSN_ARRAY **out_array, MYFLT *out_value, CSN_SCRATCH *scratch_ref, K_DATA *k_data) {
    /* The scratch lives in the caller's opcode struct; these keep the buffer
       and its capacity moving together. */
    void **scratch = &scratch_ref->scratch;
    size_t *scratch_capacity = &scratch_ref->scratch_capacity;
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = src_ref->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    int32_t axis = 0;
    res = median_body(csound, NULL, reg, &source_arr, source_handle, out_handle, axis_value, &axis);
    if (res != OK) goto done;

    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    /* Median needs a sorted copy, so it cannot stream like the folds do. */
    size_t run = (axis == -1) ? source_arr->size : source_shape[axis];
    size_t scratch_capacity_temp = (run > 0 ? run : 1) * 2;
    double *scratch_temp = csound->Calloc(csound, sizeof(double) * scratch_capacity_temp);
    if (scratch_temp == NULL) {
        res = csound->InitError(csound, "[csnarray] Out of memory: allocation of %zu bytes failed", (size_t) (sizeof(double) * (run > 0 ? run : 1)));
        goto done;
    }

    if (axis != -1) {
        const uint32_t protect[1] = { source_handle };
        if (create_csnarray_locked(csound, reg, h, source_ndim, source_arr->shape, out_array, out_handle, protect, 1U, &err, source_arr->itype) != OK) {
            csound->Free(csound, scratch_temp);
            res = csound->InitError(csound, "[csnarray] %s", err);
            goto done;
        }
        if (source_arr->size > 0) {
            memcpy((*out_array)->data, source_arr->data, sizeof(double) * source_arr->size);
            (*out_array)->size = source_arr->size;
        }

        memset(k_data->prev_shape, 0, sizeof(k_data->prev_shape));
        memcpy(k_data->prev_shape, source_shape, sizeof(k_data->prev_shape));
        k_data->prev_ndim = source_ndim;
        k_data->prev_itype = source_arr->itype;
        k_data->owned_handle = out_handle->id;
    } else {
        /* The trigger gates the perf pass, so the scalar form has to publish a
           real value at i-time rather than leaving the output at zero. */
        memcpy(scratch_temp, source_arr->data, sizeof(double) * run);
        *out_value = (MYFLT) median_of_scratch(scratch_temp, run);
    }

    k_data->prev_size = source_arr->size;
    k_data->registry = reg;
    *scratch = scratch_temp;
    *scratch_capacity = scratch_capacity_temp;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_median_impl_k(CSOUND *csound, OPDS *h, CSNREF *src_ref, double axis_value, CSNREF *out_handle, CSN_ARRAY **out_array, MYFLT *out_value, K_DATA *k_data, CSN_SCRATCH *scratch_ref, const MYFLT *trig) {
    /* The scratch lives in the caller's opcode struct; these keep the buffer
       and its capacity moving together. */
    void **scratch = &scratch_ref->scratch;
    size_t *scratch_capacity = &scratch_ref->scratch_capacity;
    CSN_REGISTRY *reg = k_data->registry;
    if (reg == NULL || (out_handle != NULL && k_data->owned_handle == 0)) {
        return csound->PerfError(csound, h, "[csnarray] k-rate output slot was not initialized");
    }

    CHECK_KTRIG(trig);

    uint32_t source_handle = src_ref->id;

    int32_t res = CHECK_SELF_ALIAS(csound, h, k_data, source_handle, 0);
    if (res != OK) return res;

    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    int32_t axis = 0;
    res = median_body(csound, h, reg, &source_arr, source_handle, out_handle, axis_value, &axis);
    if (res != OK) goto done;

    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    /* A median is a sort, so it is one of the expensive things this library
       does on a k-pass. When the source has not been written since the last
       one, and the axis is the same, and nothing has disturbed the result,
       last pass's answer is still the answer. */
    bool has_array_output = out_handle != NULL;
    if (axis == (int32_t) k_data->prev_axis
        && (!has_array_output || *out_array != NULL)
        && CAN_REUSE_LAST_RESULT(k_data, source_handle, source_arr, has_array_output ? *out_array : NULL)) {
        if (has_array_output) out_handle->id = k_data->owned_handle;
        goto done;
    }
    k_data->prev_axis = (uint32_t) axis;

    /* Median needs a sorted copy, so it cannot stream like the folds do. */
    memset(*scratch, 0, sizeof(double) * (*scratch_capacity));
    size_t runs_size = (axis == -1) ? source_arr->size : source_shape[axis];
    if (runs_size > *scratch_capacity) {
        size_t new_capacity = runs_size * 2;
        double *new_data = csound->ReAlloc(csound, *scratch, sizeof(double) * new_capacity);
        if (new_data == NULL) {
            csound->UnlockMutex(reg->mutex);
            return csound->PerfError(csound, h,"[csnarray] Internal buffer memory allocation failed");
        }
        *scratch = new_data;
        *scratch_capacity = new_capacity;
    }

    CSN_ARRAY *arr = NULL;
    if (axis == -1) {
        memcpy(*scratch, source_arr->data, sizeof(double) * runs_size);
        *out_value = (MYFLT) median_of_scratch(*scratch, runs_size);
        /* The scalar form's result lives in a MYFLT nothing else can reach, so
           there is no output generation to remember. */
        PUBLISH_DERIVED_RESULT(k_data, source_handle, source_arr, NULL);
        goto done;
    }

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    for (uint32_t i = 0, j = 0; i < source_ndim; i++) {
        if (i != (uint32_t) axis) new_shape[j++] = source_shape[i];
    }

    size_t requested_size = 0;
    if (get_array_size_from_shape(&requested_size, source_ndim - 1, new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    arr = *out_array;
    size_t logical_size = source_arr->size == 0 ? 0 : requested_size;
    res = NEED_TO_UPDATE_SLOT(csound, h, &arr, k_data, NULL, source_ndim - 1, new_shape, logical_size, source_arr->itype, err);
    if (res != OK) goto done;

    median_assign_value(source_arr, arr, *scratch, runs_size, axis);
    *out_array = arr;

    memset(k_data->prev_shape, 0, sizeof(k_data->prev_shape));
    memcpy(k_data->prev_shape, new_shape, sizeof(k_data->prev_shape));
    k_data->prev_ndim = source_ndim - 1;
    k_data->prev_itype = source_arr->itype;
    out_handle->id = k_data->owned_handle;

    PUBLISH_DERIVED_RESULT(k_data, source_handle, source_arr, arr);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_median(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_median_impl(csound, &p->h, p->source_handle, (double) *p->axis, p->handle, &p->array, NULL);
}

static int32_t csnarray_median_k_init(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_median_impl_k_init(csound, &p->h, p->source_handle, (double) *p->axis, p->handle, &p->array, NULL, &p->scratch, &p->k_data);
}

int32_t csnarray_median_k(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_median_impl_k(csound, &p->h, p->source_handle, (double) *p->axis, p->handle, &p->array, NULL, &p->k_data, &p->scratch, p->trig);
}

int32_t csnarray_median_all(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_median_impl(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value);
}

static int32_t csnarray_median_all_k_init(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_median_impl_k_init(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, &p->scratch, &p->k_data);
}

int32_t csnarray_median_all_k(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_median_impl_k(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, &p->k_data, &p->scratch, p->trig);
}

int32_t csnarray_argmin(CSOUND *csound, CSN_REDUCTION *p) {
    return argminmax_helper(csound, p, RED_ARGMIN);
}

static int32_t csnarray_argmin_k_init(CSOUND *csound, CSN_REDUCTION *p) {
    return argminmax_k_init_helper(csound, p);
}

int32_t csnarray_argmin_k(CSOUND *csound, CSN_REDUCTION *p) {
    return argminmax_k_helper(csound, p, RED_ARGMIN);
}

int32_t csnarray_argmax(CSOUND *csound, CSN_REDUCTION *p) {
    return argminmax_helper(csound, p, RED_ARGMAX);
}

static int32_t csnarray_argmax_k_init(CSOUND *csound, CSN_REDUCTION *p) {
    return argminmax_k_init_helper(csound, p);
}

int32_t csnarray_argmax_k(CSOUND *csound, CSN_REDUCTION *p) {
    return argminmax_k_helper(csound, p, RED_ARGMAX);
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

static int32_t binop_hh_assign_value(CSOUND *csound, OPDS *perf_h, const CSN_ARRAY *source_arr_a, const CSN_ARRAY *source_arr_b, CSN_ARRAY *arr, ITEM_TYPE itype, CSN_BINOP_MODE mode) {
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
                        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Division by zero");
                    }
                    arr->data[i] = a / b;
                } else {
                    if (complex_div(&c, ca, cb) != OK) {
                        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Division by zero");
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
                        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Division by zero");
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
                        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Division by zero");
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
            case CSN_HYPOT_HH:
                arr->data[i] = sqrt(a * a + b * b);
                break;
            case CSN_MINIMUM_HH:
                arr->data[i] = fmin(a, b);
                break;
            case CSN_MAXIMUM_HH:
                arr->data[i] = fmax(a, b);
                break;
            case CSN_ATAN2_HH:
                arr->data[i] = atan2(a, b);
                break;
            default:
                break;
        }
    }

    return OK;
}

static int32_t csnarray_binop_hh_helper(CSOUND *csound, CSN_BINOP_HH *p, CSN_BINOP_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

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

    if ((source_arr_a->itype == CSN_COMPLEX || source_arr_b->itype == CSN_COMPLEX) && mode == CSN_HYPOT_HH) {
        res = csound->InitError(csound, "[csnarray] Hypot supports real array only");
        goto done;
    }

    if ((source_arr_a->itype == CSN_COMPLEX || source_arr_b->itype == CSN_COMPLEX) && (mode == CSN_MINIMUM_HH || mode == CSN_MAXIMUM_HH)) {
        res = csound->InitError(csound, "[csnarray] Minimum/Maximum supports real array only");
        goto done;
    }

    if ((source_arr_a->itype == CSN_COMPLEX || source_arr_b->itype == CSN_COMPLEX) && mode == CSN_ATAN2_HH) {
        res = csound->InitError(csound, "[csnarray] atan2 supports real array only");
        goto done;
    }

    bool type_mode = source_arr_a->itype == CSN_COMPLEX || source_arr_b->itype == CSN_COMPLEX;
    ITEM_TYPE itype = type_mode ? CSN_COMPLEX : CSN_REAL;

    const uint32_t protect[2] = { source_handle_a, source_handle_b };
    if (create_csnarray_locked(csound, reg, &p->h, new_ndim, new_shape, &p->array, p->handle, protect, 2U, &err, itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;
    res = binop_hh_assign_value(csound, NULL, source_arr_a, source_arr_b, arr, itype, mode);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_binop_hh_k_init_helper(CSOUND *csound, CSN_BINOP_HH *p, CSN_BINOP_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

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

    /* Checked here as well as in the perf pass, which cannot be dropped: an
       operand's itype is dynamic at k-rate. Without this the init would publish
       a complex slot for an operation that rejects complex operands, and the
       first error a player sees would come from whatever reads that output
       rather than from here. */
    if (mode == CSN_LOGICAL_AND_HH || mode == CSN_LOGICAL_OR_HH) {
        if (source_arr_a->itype == CSN_COMPLEX || source_slot_b->array->itype == CSN_COMPLEX) {
            res = csound->InitError(csound, "[csnarray] Logical and and or supports real array only");
            goto done;
        }
    }

    if ((source_arr_a->itype == CSN_COMPLEX || source_slot_b->array->itype == CSN_COMPLEX) && mode == CSN_HYPOT_HH) {
        res = csound->InitError(csound, "[csnarray] Hypot supports real array only");
        goto done;
    }

    if ((source_arr_a->itype == CSN_COMPLEX || source_slot_b->array->itype == CSN_COMPLEX) && (mode == CSN_MINIMUM_HH || mode == CSN_MAXIMUM_HH)) {
        res = csound->InitError(csound, "[csnarray] Minimum/Maximum supports real array only");
        goto done;
    }

    if ((source_arr_a->itype == CSN_COMPLEX || source_slot_b->array->itype == CSN_COMPLEX) && mode == CSN_ATAN2_HH) {
        res = csound->InitError(csound, "[csnarray] atan2 supports real array only");
        goto done;
    }

    const uint32_t protect[2] = { source_handle_a, source_handle_b };
    if (create_csnarray_locked(csound, reg, &p->h, source_arr_a->ndim, source_arr_a->shape, &p->array, p->handle, protect, 2U, &err, source_arr_a->itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    if (source_arr_a->size > 0) {
        memcpy(p->array->data, source_arr_a->data, sizeof(double) * source_arr_a->size * source_arr_a->itype);
        p->array->size = source_arr_a->size;
    }

    SET_KDATA_BEGIN(p, reg);
    p->k_data.prev_size = source_arr_a->size;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_binop_hh_k_helper(CSOUND *csound, CSN_BINOP_HH *p, CSN_BINOP_MODE mode) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REG_HANDLE(csound, &p->h, reg, p->k_data.owned_handle);

    CHECK_KTRIG(p->trig);

    uint32_t source_handle_a = p->source_handle_a->id;
    uint32_t source_handle_b = p->source_handle_b->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot_a = get_slot(reg, source_handle_a);
    if (source_slot_a == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle_a);
    }

    CSN_SLOT *source_slot_b = get_slot(reg, source_handle_b);
    if (source_slot_b == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle_b);
    }

    CSN_ARRAY *source_arr_a = source_slot_a->array;
    CSN_ARRAY *source_arr_b = source_slot_b->array;

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    uint32_t new_ndim = 0;
    if (broadcast_shape(source_arr_a, source_arr_b, new_shape, &new_ndim) != OK) {
        char abuf[CSN_SHAPE_STR_MAX], bbuf[CSN_SHAPE_STR_MAX];
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Shapes %s and %s cannot be broadcast together: aligned from the last axis, each pair must match or be 1", shape_str(abuf, sizeof(abuf), source_arr_a->shape, source_arr_a->ndim), shape_str(bbuf, sizeof(bbuf), source_arr_b->shape, source_arr_b->ndim));
    }

    bool is_logic = (mode == CSN_LOGICAL_AND_HH || mode == CSN_LOGICAL_OR_HH);
    if (is_logic) {
        if (source_arr_a->itype == CSN_COMPLEX || source_arr_b->itype == CSN_COMPLEX) {
            csound->UnlockMutex(reg->mutex);
            return csound->PerfError(csound, &p->h, "[csnarray] Logical and and or supports real array only");
        }
    }

    if ((source_arr_a->itype == CSN_COMPLEX || source_arr_b->itype == CSN_COMPLEX) && mode == CSN_HYPOT_HH) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Hypot supports real array only");
    }

    if ((source_arr_a->itype == CSN_COMPLEX || source_arr_b->itype == CSN_COMPLEX) && (mode == CSN_MINIMUM_HH || mode == CSN_MAXIMUM_HH)) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Minimum/Maximum supports real array only");
    }

    if ((source_arr_a->itype == CSN_COMPLEX || source_arr_b->itype == CSN_COMPLEX) && mode == CSN_ATAN2_HH) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] atan2 supports real array only");
    }

    bool type_mode = source_arr_a->itype == CSN_COMPLEX || source_arr_b->itype == CSN_COMPLEX;
    ITEM_TYPE itype = type_mode ? CSN_COMPLEX : CSN_REAL;

    size_t requested_size = 0;
    if (get_array_size_from_shape(&requested_size, new_ndim, new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    size_t logical_size = (source_arr_a->size == 0 && source_arr_b->size == 0) ? 0 : requested_size;

    /* Unlike the permuting opcodes, an elementwise binop may feed on its own
       output (X = csnadd(X, B)): each result cell is written once from the cell
       it reads. That only holds while the result keeps the aliased operand's
       exact layout — a broadcast that grows it makes NEED_TO_UPDATE_SLOT
       reallocate and drop the data the fill is about to read. */
    uint32_t owned = p->k_data.owned_handle;
    if (source_handle_a == owned || source_handle_b == owned) {
        const CSN_ARRAY *aliased = source_handle_a == owned ? source_arr_a : source_arr_b;
        if (aliased->ndim != new_ndim
            || aliased->itype != itype
            || memcmp(aliased->shape, new_shape, sizeof(uint32_t) * new_ndim) != 0) {
            char abuf[CSN_SHAPE_STR_MAX], bbuf[CSN_SHAPE_STR_MAX];
            csound->UnlockMutex(reg->mutex);
            return csound->PerfError(csound, &p->h, "[csnarray] Input array %u is also this opcode's own output, so the result must keep its %s layout, not %s: assign the broadcast result to a different handle", owned, shape_str(abuf, sizeof(abuf), aliased->shape, aliased->ndim), shape_str(bbuf, sizeof(bbuf), new_shape, new_ndim));
        }
    }

    CSN_SLOT *out_slot = get_slot(reg, owned);
    if (out_slot != NULL && CAN_REUSE_ELEMENTWISE(&p->k_data, source_handle_a, source_arr_a, source_handle_b, source_arr_b, out_slot->array, 0.0, 0.0)) {
        p->handle->id = owned;
        goto done;
    }

    /* The slot the i-time pass registered is republished in place: allocating a
       new array here would register a fresh handle on every control period. */
    CSN_ARRAY *arr = NULL;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, new_ndim, new_shape, logical_size, itype, err);
    if (res != OK) goto done;
    p->array = arr;

    res = binop_hh_assign_value(csound, &p->h, source_arr_a, source_arr_b, arr, itype, mode);
    if (res != OK) goto done;
    SET_KDATA_END(p, new_shape, new_ndim, itype);
    p->k_data.prev_size = arr->size;
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle_a, source_arr_a, source_handle_b, source_arr_b, arr, 0.0, 0.0);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}


static int32_t binop_hs_sh_body(CSOUND *csound, OPDS *perf_h, CSN_REGISTRY *reg, CSN_ARRAY **source_array, uint32_t source_handle, COMPLEXDAT *complex_arg, CSN_BINOP_MODE mode) {
    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
    }

    *source_array = source_slot->array;
    CSN_ARRAY *source_arr = *source_array;

    if ((mode == CSN_LOGICAL_AND_HS || mode == CSN_LOGICAL_OR_HS) && source_arr->itype == CSN_COMPLEX) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Logical and/or supports real array only");
    }

    if (source_arr->itype == CSN_COMPLEX && mode == CSN_HYPOT_HS) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Hypot supports real array only");
    }

    if (source_arr->itype == CSN_COMPLEX && (mode == CSN_MINIMUM_HS || mode == CSN_MAXIMUM_HS)) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Minimum/Maximum supports real array only");
    }

    if (source_arr->itype == CSN_COMPLEX && (mode == CSN_ATAN2_HS || mode == CSN_ATAN2_SH)) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] atan2 supports real array only");
    }

    if (complex_arg != NULL && (mode == CSN_LOGICAL_AND_HS || mode == CSN_LOGICAL_OR_HS)) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Logical and/or supports real scalar only");
    }

    if (complex_arg != NULL && source_arr->itype != CSN_COMPLEX) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Handle holds a real array; use a real scalar instead of a :Complex; one");
    }

    return OK;
}

static int32_t binop_hs_sh_assign_value(CSOUND *csound, OPDS *perf_h, const MYFLT *scalar_arg, const COMPLEXDAT *complex_arg, CSN_ARRAY *source_arr, CSN_ARRAY *arr, CSN_BINOP_MODE mode) {
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
                        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Division by zero");
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
                        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Division by zero");
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
                        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Division by zero");
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
                        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Division by zero");
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
                        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Division by zero");
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
                        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Division by zero");
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
            case CSN_HYPOT_HS:
                arr->data[i] = (double) sqrt(a * a + real_scalar * real_scalar);
                break;
            case CSN_MINIMUM_HS:
                arr->data[i] = (double) fmin(a, real_scalar);
                break;
            case CSN_MAXIMUM_HS:
                arr->data[i] = (double) fmax(a, real_scalar);
                break;
            case CSN_ATAN2_HS:
                arr->data[i] = atan2(a, real_scalar);
                break;
            case CSN_ATAN2_SH:
                arr->data[i] = atan2(real_scalar, a);
                break;
            default:
                break;
        }
    }

    return OK;
}

/* The handle and the scalar are passed explicitly rather than read off a
   cast struct: CSN_BINOP_SH lists them in the opposite order to CSN_BINOP_HS,
   so a single cast would silently swap them. */
static int32_t csnarray_binop_hs_sh_helper(CSOUND *csound, CSN_BINOP_COMMON *p, CSNREF *handle_arg, MYFLT *scalar_arg, COMPLEXDAT *complex_arg, CSN_BINOP_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = handle_arg->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    res = binop_hs_sh_body(csound, NULL, reg, &source_arr, source_handle, complex_arg, mode);
    if (res != OK) goto done;

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    memcpy(new_shape, source_arr->shape, sizeof(uint32_t) * CSN_MAX_DIMS);

    const uint32_t protect[1] = { source_handle };
    if (create_csnarray_locked(csound, reg, &p->h, source_arr->ndim, new_shape, &p->array, p->handle, protect, 1U, &err, source_arr->itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;

    res = binop_hs_sh_assign_value(csound, NULL, scalar_arg, complex_arg, source_arr, arr, mode);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_binop_hs_sh_k_init_helper(CSOUND *csound, CSN_BINOP_COMMON *p, CSNREF *handle_arg, COMPLEXDAT *complex_arg, CSN_BINOP_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = handle_arg->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    res = binop_hs_sh_body(csound, NULL, reg, &source_arr, source_handle, complex_arg, mode);
    if (res != OK) goto done;

    const uint32_t protect[1] = { source_handle };
    if (create_csnarray_locked(csound, reg, &p->h, source_arr->ndim, source_arr->shape, &p->array, p->handle, protect, 1U, &err, source_arr->itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;
    if (source_arr->size > 0) {
        memcpy(arr->data, source_arr->data, sizeof(double) * source_arr->size * source_arr->itype);
        arr->size = source_arr->size;
    }

    SET_KDATA_BEGIN(p, reg);
    p->k_data.prev_size = source_arr->size;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_binop_hs_sh_k_helper(CSOUND *csound, CSN_BINOP_COMMON *p, CSNREF *handle_arg, MYFLT *scalar_arg, COMPLEXDAT *complex_arg, MYFLT *trig, CSN_BINOP_MODE mode) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REG_HANDLE(csound, &p->h, reg, p->k_data.owned_handle);

    CHECK_KTRIG(trig);

    uint32_t source_handle = handle_arg->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    res = binop_hs_sh_body(csound, &p->h, reg, &source_arr, source_handle, complex_arg, mode);
    if (res != OK) goto done;

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    memcpy(new_shape, source_arr->shape, sizeof(uint32_t) * CSN_MAX_DIMS);

    size_t requested_size = 0;
    if (get_array_size_from_shape(&requested_size, source_arr->ndim, new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    /* The scalar parameter belongs to the reuse key just as much as the array:
       the complex forms carry two components, the real ones only the first. */
    double scalar_key = scalar_arg != NULL ? (double) *scalar_arg : 0.0;
    double scalar_key_im = 0.0;
    if (complex_arg != NULL) {
        complexdat_to_rect(complex_arg, &scalar_key, &scalar_key_im);
    }

    CSN_SLOT *out_slot = get_slot(reg, p->k_data.owned_handle);
    if (out_slot != NULL && CAN_REUSE_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, out_slot->array, scalar_key, scalar_key_im)) {
        p->handle->id = p->k_data.owned_handle;
        goto done;
    }

    /* The scalar forms keep the source layout, so X = csnmul(X, k) needs no
       alias check: the request always matches the destination already. */
    CSN_ARRAY *arr = NULL;
    size_t logical_size = source_arr->size == 0 ? 0 : requested_size;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, source_arr->ndim, new_shape, logical_size, source_arr->itype, err);
    if (res != OK) goto done;
    p->array = arr;

    res = binop_hs_sh_assign_value(csound, &p->h, scalar_arg, complex_arg, source_arr, arr, mode);
    if (res != OK) goto done;

    SET_KDATA_END(p, new_shape, source_arr->ndim, source_arr->itype);
    p->k_data.prev_size = arr->size;
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, arr, scalar_key, scalar_key_im);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_add_hh(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_helper(csound, p, CSN_ADD_HH);
}

static int32_t csnarray_add_hh_k_init(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_k_init_helper(csound, p, CSN_ADD_HH);
}

int32_t csnarray_add_hh_k(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_k_helper(csound, p, CSN_ADD_HH);
}

int32_t csnarray_add_hs(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, CSN_ADD_HS);
}

static int32_t csnarray_add_hs_k_init(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_k_init_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, CSN_ADD_HS);
}

int32_t csnarray_add_hs_k(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_k_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, p->trig, CSN_ADD_HS);
}

int32_t csnarray_subtract_hh(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_helper(csound, p, CSN_SUB_HH);
}

static int32_t csnarray_subtract_hh_k_init(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_k_init_helper(csound, p, CSN_SUB_HH);
}

int32_t csnarray_subtract_hh_k(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_k_helper(csound, p, CSN_SUB_HH);
}

int32_t csnarray_subtract_hs(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, CSN_SUB_HS);
}

static int32_t csnarray_subtract_hs_k_init(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_k_init_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, CSN_SUB_HS);
}

int32_t csnarray_subtract_hs_k(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_k_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, p->trig, CSN_SUB_HS);
}

int32_t csnarray_subtract_sh(CSOUND *csound, CSN_BINOP_SH *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, CSN_SUB_SH);
}

static int32_t csnarray_subtract_sh_k_init(CSOUND *csound, CSN_BINOP_SH *p) {
    return csnarray_binop_hs_sh_k_init_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, CSN_SUB_SH);
}

int32_t csnarray_subtract_sh_k(CSOUND *csound, CSN_BINOP_SH *p) {
    return csnarray_binop_hs_sh_k_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, p->trig, CSN_SUB_SH);
}

int32_t csnarray_mul_hh(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_helper(csound, p, CSN_MUL_HH);
}

static int32_t csnarray_mul_hh_k_init(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_k_init_helper(csound, p, CSN_MUL_HH);
}

int32_t csnarray_mul_hh_k(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_k_helper(csound, p, CSN_MUL_HH);
}

int32_t csnarray_mul_hs(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, CSN_MUL_HS);
}

static int32_t csnarray_mul_hs_k_init(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_k_init_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, CSN_MUL_HS);
}

int32_t csnarray_mul_hs_k(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_k_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, p->trig, CSN_MUL_HS);
}

int32_t csnarray_div_hh(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_helper(csound, p, CSN_DIV_HH);
}

int32_t csnarray_div_hh_k_init(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_k_init_helper(csound, p, CSN_DIV_HH);
}

int32_t csnarray_div_hh_k(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_k_helper(csound, p, CSN_DIV_HH);
}

int32_t csnarray_div_hs(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, CSN_DIV_HS);
}

static int32_t csnarray_div_hs_k_init(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, CSN_DIV_HS);
}

int32_t csnarray_div_hs_k(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_k_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, p->trig, CSN_DIV_HS);
}

int32_t csnarray_div_sh(CSOUND *csound, CSN_BINOP_SH *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, CSN_DIV_SH);
}

static int32_t csnarray_div_sh_k_init(CSOUND *csound, CSN_BINOP_SH *p) {
    return csnarray_binop_hs_sh_k_init_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, CSN_DIV_SH);
}

int32_t csnarray_div_sh_k(CSOUND *csound, CSN_BINOP_SH *p) {
    return csnarray_binop_hs_sh_k_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, p->trig, CSN_DIV_SH);
}

int32_t csnarray_pow_hh(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_helper(csound, p, CSN_POW_HH);
}

static int32_t csnarray_pow_hh_k_init(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_k_init_helper(csound, p, CSN_POW_HH);
}

int32_t csnarray_pow_hh_k(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_k_helper(csound, p, CSN_POW_HH);
}

int32_t csnarray_pow_hs(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, CSN_POW_HS);
}

static int32_t csnarray_pow_hs_k_init(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_k_init_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, CSN_POW_HS);
}

int32_t csnarray_pow_hs_k(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_k_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, p->trig, CSN_POW_HS);
}

int32_t csnarray_pow_sh(CSOUND *csound, CSN_BINOP_SH *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, CSN_POW_SH);
}

static int32_t csnarray_pow_sh_k_init(CSOUND *csound, CSN_BINOP_SH *p) {
    return csnarray_binop_hs_sh_k_init_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, CSN_POW_SH);
}

int32_t csnarray_pow_sh_k(CSOUND *csound, CSN_BINOP_SH *p) {
    return csnarray_binop_hs_sh_k_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, p->trig, CSN_POW_SH);
}

int32_t csnarray_log_hh(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_helper(csound, p, CSN_LOG_HH);
}

static int32_t csnarray_log_hh_k_init(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_k_init_helper(csound, p, CSN_LOG_HH);
}

int32_t csnarray_log_hh_k(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_k_helper(csound, p, CSN_LOG_HH);
}

int32_t csnarray_log_hs(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, CSN_LOG_HS);
}

static int32_t csnarray_log_hs_k_init(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_k_init_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, CSN_LOG_HS);
}

int32_t csnarray_log_hs_k(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_k_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, p->trig, CSN_LOG_HS);
}

int32_t csnarray_log_sh(CSOUND *csound, CSN_BINOP_SH *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, CSN_LOG_SH);
}

static int32_t csnarray_log_sh_k_init(CSOUND *csound, CSN_BINOP_SH *p) {
    return csnarray_binop_hs_sh_k_init_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, CSN_LOG_SH);
}

int32_t csnarray_log_sh_k(CSOUND *csound, CSN_BINOP_SH *p) {
    return csnarray_binop_hs_sh_k_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, p->trig, CSN_LOG_SH);
}

int32_t csnarray_addcomp_hs(CSOUND *csound, CSN_BINOPCOMPLEX_HS *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, p->scalar, CSN_ADD_HS);
}

static int32_t csnarray_addcomp_hs_k_init(CSOUND *csound, CSN_BINOPCOMPLEX_HS *p) {
    return csnarray_binop_hs_sh_k_init_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, CSN_ADD_HS);
}

int32_t csnarray_addcomp_hs_k(CSOUND *csound, CSN_BINOPCOMPLEX_HS *p) {
    return csnarray_binop_hs_sh_k_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, p->scalar, p->trig, CSN_ADD_HS);
}

int32_t csnarray_subtractcomp_hs(CSOUND *csound, CSN_BINOPCOMPLEX_HS *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, p->scalar, CSN_SUB_HS);
}

static int32_t csnarray_subtractcomp_hs_k_init(CSOUND *csound, CSN_BINOPCOMPLEX_HS *p) {
    return csnarray_binop_hs_sh_k_init_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, CSN_SUB_HS);
}

int32_t csnarray_subtractcomp_hs_k(CSOUND *csound, CSN_BINOPCOMPLEX_HS *p) {
    return csnarray_binop_hs_sh_k_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, p->scalar, p->trig, CSN_SUB_HS);
}

int32_t csnarray_subtractcomp_sh(CSOUND *csound, CSN_BINOPCOMPLEX_SH *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, p->scalar, CSN_SUB_SH);
}

static int32_t csnarray_subtractcomp_sh_k_init(CSOUND *csound, CSN_BINOPCOMPLEX_SH *p) {
    return csnarray_binop_hs_sh_k_init_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, CSN_SUB_SH);
}

int32_t csnarray_subtractcomp_sh_k(CSOUND *csound, CSN_BINOPCOMPLEX_SH *p) {
    return csnarray_binop_hs_sh_k_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, p->scalar, p->trig, CSN_SUB_SH);
}

int32_t csnarray_mulcomp_hs(CSOUND *csound, CSN_BINOPCOMPLEX_HS *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, p->scalar, CSN_MUL_HS);
}

int32_t csnarray_mulcomp_hs_k_init(CSOUND *csound, CSN_BINOPCOMPLEX_HS *p) {
    return csnarray_binop_hs_sh_k_init_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, CSN_MUL_HS);
}

int32_t csnarray_mulcomp_hs_k(CSOUND *csound, CSN_BINOPCOMPLEX_HS *p) {
    return csnarray_binop_hs_sh_k_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, p->scalar, p->trig, CSN_MUL_HS);
}

int32_t csnarray_divcomp_hs(CSOUND *csound, CSN_BINOPCOMPLEX_HS *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, p->scalar, CSN_DIV_HS);
}

static int32_t csnarray_divcomp_hs_k_init(CSOUND *csound, CSN_BINOPCOMPLEX_HS *p) {
    return csnarray_binop_hs_sh_k_init_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, CSN_DIV_HS);
}

int32_t csnarray_divcomp_hs_k(CSOUND *csound, CSN_BINOPCOMPLEX_HS *p) {
    return csnarray_binop_hs_sh_k_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, p->scalar, p->trig, CSN_DIV_HS);
}

int32_t csnarray_divcomp_sh(CSOUND *csound, CSN_BINOPCOMPLEX_SH *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, p->scalar, CSN_DIV_SH);
}

static int32_t csnarray_divcomp_sh_k_init(CSOUND *csound, CSN_BINOPCOMPLEX_SH *p) {
    return csnarray_binop_hs_sh_k_init_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, CSN_DIV_SH);
}

int32_t csnarray_divcomp_sh_k(CSOUND *csound, CSN_BINOPCOMPLEX_SH *p) {
    return csnarray_binop_hs_sh_k_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, p->scalar, p->trig, CSN_DIV_SH);
}

int32_t csnarray_powcomp_hs(CSOUND *csound, CSN_BINOPCOMPLEX_HS *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, p->scalar, CSN_POW_HS);
}

static int32_t csnarray_powcomp_hs_k_init(CSOUND *csound, CSN_BINOPCOMPLEX_HS *p) {
    return csnarray_binop_hs_sh_k_init_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, CSN_POW_HS);
}

int32_t csnarray_powcomp_hs_k(CSOUND *csound, CSN_BINOPCOMPLEX_HS *p) {
    return csnarray_binop_hs_sh_k_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, p->scalar, p->trig, CSN_POW_HS);
}

int32_t csnarray_powcomp_sh(CSOUND *csound, CSN_BINOPCOMPLEX_SH *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, p->scalar, CSN_POW_SH);
}

int32_t csnarray_powcomp_sh_k_init(CSOUND *csound, CSN_BINOPCOMPLEX_SH *p) {
    return csnarray_binop_hs_sh_k_init_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, CSN_POW_SH);
}

int32_t csnarray_powcomp_sh_k(CSOUND *csound, CSN_BINOPCOMPLEX_SH *p) {
    return csnarray_binop_hs_sh_k_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, p->scalar, p->trig, CSN_POW_SH);
}

int32_t csnarray_logcomp_sh(CSOUND *csound, CSN_BINOPCOMPLEX_SH *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, p->scalar, CSN_LOG_SH);
}

static int32_t csnarray_logcomp_sh_k_init(CSOUND *csound, CSN_BINOPCOMPLEX_SH *p) {
    return csnarray_binop_hs_sh_k_init_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, CSN_LOG_SH);
}

int32_t csnarray_logcomp_sh_k(CSOUND *csound, CSN_BINOPCOMPLEX_SH *p) {
    return csnarray_binop_hs_sh_k_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, p->scalar, p->trig, CSN_LOG_SH);
}

int32_t csnarray_logcomp_hs(CSOUND *csound, CSN_BINOPCOMPLEX_HS *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, p->scalar, CSN_LOG_HS);
}

static int32_t csnarray_logcomp_hs_k_init(CSOUND *csound, CSN_BINOPCOMPLEX_HS *p) {
    return csnarray_binop_hs_sh_k_init_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, CSN_LOG_HS);
}

int32_t csnarray_logcomp_hs_k(CSOUND *csound, CSN_BINOPCOMPLEX_HS *p) {
    return csnarray_binop_hs_sh_k_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, p->scalar, p->trig, CSN_LOG_HS);
}

int32_t csnarray_logical_and_hh(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_helper(csound, p, CSN_LOGICAL_AND_HH);
}

static int32_t csnarray_logical_and_hh_k_init(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_k_init_helper(csound, p, CSN_LOGICAL_AND_HH);
}

int32_t csnarray_logical_and_hh_k(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_k_helper(csound, p, CSN_LOGICAL_AND_HH);
}

int32_t csnarray_logical_or_hh(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_helper(csound, p, CSN_LOGICAL_OR_HH);
}

static int32_t csnarray_logical_or_hh_k_init(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_k_init_helper(csound, p, CSN_LOGICAL_OR_HH);
}

int32_t csnarray_logical_or_hh_k(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_k_helper(csound, p, CSN_LOGICAL_OR_HH);
}

int32_t csnarray_logical_and_hs(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, CSN_LOGICAL_AND_HS);
}

static int32_t csnarray_logical_and_hs_k_init(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_k_init_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, CSN_LOGICAL_AND_HS);
}

int32_t csnarray_logical_and_hs_k(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_k_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, p->trig, CSN_LOGICAL_AND_HS);
}

int32_t csnarray_logical_or_hs(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, CSN_LOGICAL_OR_HS);
}

static int32_t csnarray_logical_or_hs_k_init(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_k_init_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, CSN_LOGICAL_OR_HS);
}

int32_t csnarray_logical_or_hs_k(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_k_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, p->trig, CSN_LOGICAL_OR_HS);
}

int32_t csnarray_logical_and_sh(CSOUND *csound, CSN_BINOP_SH *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, CSN_LOGICAL_AND_HS);
}

static int32_t csnarray_logical_and_sh_k_init(CSOUND *csound, CSN_BINOP_SH *p) {
    return csnarray_binop_hs_sh_k_init_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, CSN_LOGICAL_AND_HS);
}

int32_t csnarray_logical_and_sh_k(CSOUND *csound, CSN_BINOP_SH *p) {
    return csnarray_binop_hs_sh_k_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, p->trig, CSN_LOGICAL_AND_HS);
}

int32_t csnarray_logical_or_sh(CSOUND *csound, CSN_BINOP_SH *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, CSN_LOGICAL_OR_HS);
}

int32_t csnarray_logical_or_sh_k_init(CSOUND *csound, CSN_BINOP_SH *p) {
    return csnarray_binop_hs_sh_k_init_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, CSN_LOGICAL_OR_HS);
}

int32_t csnarray_logical_or_sh_k(CSOUND *csound, CSN_BINOP_SH *p) {
    return csnarray_binop_hs_sh_k_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, p->trig, CSN_LOGICAL_OR_HS);
}

int32_t csnarray_hypot_hh(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_helper(csound, p, CSN_HYPOT_HH);
}

int32_t csnarray_hypot_hs(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, CSN_HYPOT_HS);
}

static int32_t csnarray_hypot_hh_k_init(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_k_init_helper(csound, p, CSN_HYPOT_HH);
}

int32_t csnarray_hypot_hh_k(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_k_helper(csound, p, CSN_HYPOT_HH);
}

static int32_t csnarray_hypot_hs_k_init(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_k_init_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, CSN_HYPOT_HS);
}

int32_t csnarray_hypot_hs_k(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_k_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, p->trig, CSN_HYPOT_HS);
}

int32_t csnarray_minimum_hh(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_helper(csound, p, CSN_MINIMUM_HH);
}

int32_t csnarray_minimum_hs(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, CSN_MINIMUM_HS);
}

static int32_t csnarray_minimum_hh_k_init(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_k_init_helper(csound, p, CSN_MINIMUM_HH);
}

int32_t csnarray_minimum_hh_k(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_k_helper(csound, p, CSN_MINIMUM_HH);
}

static int32_t csnarray_minimum_hs_k_init(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_k_init_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, CSN_MINIMUM_HS);
}

int32_t csnarray_minimum_hs_k(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_k_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, p->trig, CSN_MINIMUM_HS);
}

int32_t csnarray_maximum_hh(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_helper(csound, p, CSN_MAXIMUM_HH);
}

int32_t csnarray_maximum_hs(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, CSN_MAXIMUM_HS);
}

static int32_t csnarray_maximum_hh_k_init(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_k_init_helper(csound, p, CSN_MAXIMUM_HH);
}

int32_t csnarray_maximum_hh_k(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_k_helper(csound, p, CSN_MAXIMUM_HH);
}

static int32_t csnarray_maximum_hs_k_init(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_k_init_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, CSN_MAXIMUM_HS);
}

int32_t csnarray_maximum_hs_k(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_k_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, p->trig, CSN_MAXIMUM_HS);
}

int32_t csnarray_atan2_hh(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_helper(csound, p, CSN_ATAN2_HH);
}

static int32_t csnarray_atan2_hh_k_init(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_k_init_helper(csound, p, CSN_ATAN2_HH);
}

int32_t csnarray_atan2_hh_k(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_binop_hh_k_helper(csound, p, CSN_ATAN2_HH);
}

int32_t csnarray_atan2_hs(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, CSN_ATAN2_HS);
}

static int32_t csnarray_atan2_hs_k_init(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_k_init_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, CSN_ATAN2_HS);
}

int32_t csnarray_atan2_hs_k(CSOUND *csound, CSN_BINOP_HS *p) {
    return csnarray_binop_hs_sh_k_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, p->trig, CSN_ATAN2_HS);
}

int32_t csnarray_atan2_sh(CSOUND *csound, CSN_BINOP_SH *p) {
    return csnarray_binop_hs_sh_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, CSN_ATAN2_SH);
}

static int32_t csnarray_atan2_sh_k_init(CSOUND *csound, CSN_BINOP_SH *p) {
    return csnarray_binop_hs_sh_k_init_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, NULL, CSN_ATAN2_SH);
}

int32_t csnarray_atan2_sh_k(CSOUND *csound, CSN_BINOP_SH *p) {
    return csnarray_binop_hs_sh_k_helper(csound, (CSN_BINOP_COMMON *) p, p->source_handle, p->scalar, NULL, p->trig, CSN_ATAN2_SH);
}

static int32_t unaryop_body(CSOUND *csound, OPDS *perf_h, CSN_ARRAY **source_array, CSN_REGISTRY *reg, uint32_t source_handle, ITEM_TYPE *new_itype, uint32_t *new_shape, CSN_UNARY_MODE mode) {
    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
    }

    *source_array = source_slot->array;
    CSN_ARRAY *source_arr = *source_array;
    ITEM_TYPE itype = source_arr->itype;
    if (itype == CSN_COMPLEX
        && (mode == CSN_FLOOR
            || mode == CSN_CEIL
            || mode == CSN_ROUND
            || mode == CSN_LOGICAL_NOT
            || mode == CSN_DEG2RAD
            || mode == CSN_RAD2DEG)
    ) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] floor, ceil, round, logical not and angle operations not allowed for complex array");
    }

    *new_itype = (itype == CSN_COMPLEX && mode == CSN_ABS) ? CSN_REAL : itype;
    memcpy(new_shape, source_arr->shape, sizeof(uint32_t) * CSN_MAX_DIMS);
    return OK;
}

/* itype is the *source's* element type, because it selects how each element is
   read out of source_arr. It is not always the destination's: csnabs over a
   complex array writes a real result, so passing the output type here would
   walk the interleaved source one double at a time. */
static void unaryop_assign_value(CSN_ARRAY *source_arr, CSN_ARRAY *arr, ITEM_TYPE itype, CSN_UNARY_MODE mode) {
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
            case CSN_DEG2RAD:
            case CSN_RAD2DEG:
                arr->data[i] = mode == CSN_DEG2RAD ? a * M_PI / 180.0 : a * 180.0 / M_PI;
                break;
            default:
                break;
        }
    }
}

static int32_t csnarray_unaryop_helper(CSOUND *csound, CSN_UNARYOP *p, CSN_UNARY_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    ITEM_TYPE new_itype;
    res = unaryop_body(csound, NULL, &source_arr, reg, source_handle, &new_itype, new_shape, mode);
    if (res != OK) goto done;

    const uint32_t protect[1] = { source_handle };
    if (create_csnarray_locked(csound, reg, &p->h, source_arr->ndim, new_shape, &p->array, p->handle, protect, 1U, &err, new_itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;
    unaryop_assign_value(source_arr, arr, source_arr->itype, mode);
    SET_KDATA_BEGIN(p, reg);
    p->k_data.prev_size = arr->size;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_unaryop_k_helper(CSOUND *csound, CSN_UNARYOP *p, CSN_UNARY_MODE mode) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REG_HANDLE(csound, &(p->h), reg, p->k_data.owned_handle);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    const char *err = NULL;

    CHECK_KTRIG(p->trig);

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    ITEM_TYPE new_itype;
    res = unaryop_body(csound, &p->h, &source_arr, reg, source_handle, &new_itype, new_shape, mode);
    if (res != OK) goto done;

    size_t requested_size = 0;
    if (get_array_size_from_shape(&requested_size, source_arr->ndim, new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSN_ARRAY *arr = p->array;
    size_t logical_size = source_arr->size == 0 ? 0 : requested_size;
    /* Every unary function writes the cell it reads and keeps the source's
       shape; the one layout change in the family is csnabs turning a complex
       array real, which the layout test below catches. */
    res = CHECK_SELF_ALIAS_CELL_LOCAL(csound, &p->h, &p->k_data, source_handle, source_arr, source_arr->ndim, new_shape, new_itype);
    if (res != OK) goto done;

    CSN_SLOT *out_slot = get_slot(reg, p->k_data.owned_handle);
    if (out_slot != NULL && CAN_REUSE_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, out_slot->array, 0.0, 0.0)) {
        p->handle->id = p->k_data.owned_handle;
        goto done;
    }

    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, source_arr->ndim, new_shape, logical_size, new_itype, err);
    if (res != OK) goto done;

    unaryop_assign_value(source_arr, arr, source_arr->itype, mode);
    SET_KDATA_END(p, new_shape, source_arr->ndim, new_itype);
    p->k_data.prev_size = arr->size;
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, arr, 0.0, 0.0);

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

int32_t csnarray_degtorad(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_helper(csound, p, CSN_DEG2RAD);
}

int32_t csnarray_radtodeg(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_helper(csound, p, CSN_RAD2DEG);
}

int32_t csnarray_sqrt_k(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_k_helper(csound, p, CSN_SQRT);
}

int32_t csnarray_cbrt_k(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_k_helper(csound, p, CSN_CBRT);
}

int32_t csnarray_abs_k(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_k_helper(csound, p, CSN_ABS);
}

int32_t csnarray_exp_k(CSOUND *csound, CSN_UNARYOP *p){
    return csnarray_unaryop_k_helper(csound, p, CSN_EXP);
}

int32_t csnarray_sin_k(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_k_helper(csound, p, CSN_SIN);
}

int32_t csnarray_cos_k(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_k_helper(csound, p, CSN_COS);
}

int32_t csnarray_tan_k(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_k_helper(csound, p, CSN_TAN);
}

int32_t csnarray_asin_k(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_k_helper(csound, p, CSN_ASIN);
}

int32_t csnarray_acos_k(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_k_helper(csound, p, CSN_ACOS);
}

int32_t csnarray_atan_k(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_k_helper(csound, p, CSN_ATAN);
}

int32_t csnarray_sinh_k(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_k_helper(csound, p, CSN_SINH);
}

int32_t csnarray_cosh_k(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_k_helper(csound, p, CSN_COSH);
}

int32_t csnarray_tanh_k(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_k_helper(csound, p, CSN_TANH);
}

int32_t csnarray_asinh_k(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_k_helper(csound, p, CSN_ASINH);
}

int32_t csnarray_acosh_k(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_k_helper(csound, p, CSN_ACOSH);
}

int32_t csnarray_atanh_k(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_k_helper(csound, p, CSN_ATANH);
}

int32_t csnarray_floor_k(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_k_helper(csound, p, CSN_FLOOR);
}

int32_t csnarray_ceil_k(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_k_helper(csound, p, CSN_CEIL);
}

int32_t csnarray_round_k(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_k_helper(csound, p, CSN_ROUND);
}

int32_t csnarray_sign_k(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_k_helper(csound, p, CSN_SIGN);
}

int32_t csnarray_logical_not_k(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_k_helper(csound, p, CSN_LOGICAL_NOT);
}

int32_t csnarray_degtorad_k(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_k_helper(csound, p, CSN_DEG2RAD);
}

int32_t csnarray_radtodeg_k(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_unaryop_k_helper(csound, p, CSN_RAD2DEG);
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

static int32_t vec_body(CSOUND *csound, OPDS *perf_h, CSN_REGISTRY *reg, uint32_t source_handle_a, uint32_t source_handle_b, CSN_ARRAY **source_array_a, CSN_ARRAY **source_array_b) {
    CSN_SLOT *slot_a = get_slot(reg, source_handle_a);
    if (slot_a == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle_a);
    }

    CSN_SLOT *slot_b = get_slot(reg, source_handle_b);
    if (slot_b == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle_b);
    }

    *source_array_a = slot_a->array;
    CSN_ARRAY *source_arr_a = *source_array_a;

    if (source_arr_a->itype != slot_b->array->itype) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Element type mismatch: one array is real and the other complex");
    }

    *source_array_b = slot_b->array;

    return OK;
}

static int32_t vec_assign_shape_and_dim(CSOUND *csound, OPDS *perf_h, const uint32_t *source_shape_a, const uint32_t *source_shape_b, const uint32_t source_dim_a, const uint32_t source_dim_b, uint32_t *new_shape, uint32_t *new_ndim, CSN_VECOP_MODE mode) {
    uint32_t j = 0;
    size_t bk = (source_dim_b >= 2) ? source_dim_b - 2 : 0;
    switch (mode) {
        case CSN_DOT:
            if (source_dim_a == 1 && source_dim_b == 1) {
                return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] The dot product of two 1-D arrays is a scalar; declare the output as i, not as a handle");
            }

            if (check_dot_shape(source_shape_a, source_shape_b, source_dim_a, source_dim_b) != OK) {
                char abuf[CSN_SHAPE_STR_MAX], bbuf[CSN_SHAPE_STR_MAX];
                return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Shapes %s and %s are not valid for a dot product: the last axis of the first must match the second-to-last axis of the second", shape_str(abuf, sizeof(abuf), source_shape_a, (uint32_t) source_dim_a), shape_str(bbuf, sizeof(bbuf), source_shape_b, (uint32_t) source_dim_b));
            }

            j = 0;
            for (uint32_t i = 0; i + 1 < source_dim_a; ++i){
                new_shape[j++] = source_shape_a[i];
            }
            for (uint32_t i = 0; i < source_dim_b; ++i){
                if (i != bk) new_shape[j++] = source_shape_b[i];
            }

            *new_ndim = j;
            break;
        case CSN_INNER:
            if (source_dim_a == 1 && source_dim_b == 1) {
                return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] The inner product of two 1-D arrays is a scalar; declare the output as i, not as a handle");
            }

            if (source_shape_a[source_dim_a - 1] != source_shape_b[source_dim_b - 1]) {
                char abuf[CSN_SHAPE_STR_MAX], bbuf[CSN_SHAPE_STR_MAX];
                return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Shapes %s and %s are not valid for an inner product: the last axis must match (%u vs %u)", shape_str(abuf, sizeof(abuf), source_shape_a, (uint32_t) source_dim_a), shape_str(bbuf, sizeof(bbuf), source_shape_b, (uint32_t) source_dim_b), source_shape_a[source_dim_a - 1], source_shape_b[source_dim_b - 1]);
            }

            j = 0;
            for (uint32_t i = 0; i < source_dim_a - 1; ++i) {
                new_shape[j++] = source_shape_a[i];
            }

            for (uint32_t i = 0; i < source_dim_b - 1; ++i) {
                new_shape[j++] = source_shape_b[i];
            }

            *new_ndim = j;
            break;
        case CSN_OUTER:
            if (source_dim_a != 1 || source_dim_b != 1) {
                return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Outer product needs two 1-D arrays, got %u-D and %u-D", (uint32_t) source_dim_a, (uint32_t) source_dim_b);
            }

            new_shape[0] = source_shape_a[0];
            new_shape[1] = source_shape_b[0];
            *new_ndim = 2U;
            break;
        case CSN_PAIR_DISTANCE:
        case CSN_PROJECT:
        case CSN_REJECT:
        case CSN_REFLECT:
            if (source_dim_a != source_dim_b || memcmp(source_shape_a, source_shape_b, sizeof(uint32_t) * CSN_MAX_DIMS) != 0) {
                char abuf[CSN_SHAPE_STR_MAX], bbuf[CSN_SHAPE_STR_MAX];
                return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Both arrays must have the same shape, got %s and %s", shape_str(abuf, sizeof(abuf), source_shape_a, (uint32_t) source_dim_a), shape_str(bbuf, sizeof(bbuf), source_shape_b, (uint32_t) source_dim_b));
            }

            memcpy(new_shape, source_shape_a, sizeof(uint32_t) * CSN_MAX_DIMS);
            *new_ndim = source_dim_a;
            break;
        case CSN_CROSS:
            if (source_dim_a != 1 || source_dim_b != 1 || source_shape_a[0] != 3 || source_shape_b[0] != 3) {
                char abuf[CSN_SHAPE_STR_MAX], bbuf[CSN_SHAPE_STR_MAX];
                return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Cross product needs two 1-D arrays of size 3, got %s and %s", shape_str(abuf, sizeof(abuf), source_shape_a, (uint32_t) source_dim_a), shape_str(bbuf, sizeof(bbuf), source_shape_b, (uint32_t) source_dim_b));
            }

            new_shape[0] = 3;
            *new_ndim = 1;
            break;
        default:
            break;
    }

    return OK;
}

static int32_t vec_assign_value(CSOUND *csound, OPDS *perf_h, CSN_ARRAY *source_arr_a, CSN_ARRAY *source_arr_b, CSN_ARRAY *arr, CSN_VECOP_MODE mode) {
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
                return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] The second vector has zero length, so the projection onto it is undefined");
            }

            CSN_COMPLEXDAT scale = {0};
            if (complex_div(&scale, dot_ab, dot_bb) != OK) {
                return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] The second vector has zero length, so the projection onto it is undefined");
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

    return OK;
}

/* project, reject and cross are defined here over real vectors only, so a
   complex operand is refused rather than silently treated as two real halves.
   The other vector operations carry complex operands through unchanged. */
static int32_t vec_reject_complex(CSOUND *csound, OPDS *perf_h, CSN_VECOP_MODE mode, const CSN_ARRAY *a, const CSN_ARRAY *b) {
    if (mode != CSN_PROJECT && mode != CSN_REJECT && mode != CSN_CROSS) {
        return OK;
    }

    if (a->itype != CSN_COMPLEX && b->itype != CSN_COMPLEX) {
        return OK;
    }

    return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] This operation is not implemented for complex arrays");
}

static int32_t csnarray_vec_helper(CSOUND *csound, CSN_BINOP_HH *p, CSN_VECOP_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle_a = (uint32_t) p->source_handle_a->id;
    uint32_t source_handle_b = (uint32_t) p->source_handle_b->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr_a = NULL;
    CSN_ARRAY *source_arr_b = NULL;
    res = vec_body(csound, NULL, reg, source_handle_a, source_handle_b, &source_arr_a, &source_arr_b);
    if (res != OK) goto done;

    res = vec_reject_complex(csound, NULL, mode, source_arr_a, source_arr_b);
    if (res != OK) goto done;

    uint32_t *source_shape_a = source_arr_a->shape;
    uint32_t *source_shape_b = source_arr_b->shape;
    size_t source_dim_a = source_arr_a->ndim;
    size_t source_dim_b = source_arr_b->ndim;

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    uint32_t new_ndim = 0;
    res = vec_assign_shape_and_dim(csound, NULL, source_shape_a, source_shape_b, source_dim_a, source_dim_b, new_shape, &new_ndim, mode);
    if (res != OK) goto done;

    const uint32_t protect[2] = { source_handle_a, source_handle_b };
    ITEM_TYPE out_itype = (mode == CSN_PAIR_DISTANCE) ? CSN_REAL : source_arr_a->itype;
    if (create_csnarray_locked(csound, reg, &p->h, new_ndim, new_shape, &p->array, p->handle, protect, 2U, &err, out_itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;
    res = vec_assign_value(csound, NULL, source_arr_a, source_arr_b, arr, mode);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_vec_k_init_helper(CSOUND *csound, CSN_BINOP_HH *p, CSN_VECOP_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle_a = (uint32_t) p->source_handle_a->id;
    uint32_t source_handle_b = (uint32_t) p->source_handle_b->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr_a = NULL;
    CSN_ARRAY *source_arr_b = NULL;
    res = vec_body(csound, NULL, reg, source_handle_a, source_handle_b, &source_arr_a, &source_arr_b);
    if (res != OK) goto done;

    res = vec_reject_complex(csound, NULL, mode, source_arr_a, source_arr_b);
    if (res != OK) goto done;

    uint32_t *source_shape_a = source_arr_a->shape;
    size_t source_dim_a = source_arr_a->ndim;

    /* The output is seeded from operand a and only takes its real shape on the
       first triggered pass, but the operand pair is checked here too: without
       it an unusable combination (a cross product of non 3-vectors, say) would
       surface at the first trigger, or not at all if the gate never opens. */
    uint32_t check_shape[CSN_MAX_DIMS] = {0};
    uint32_t check_ndim = 0;
    res = vec_assign_shape_and_dim(csound, NULL, source_shape_a, source_arr_b->shape, source_dim_a, source_arr_b->ndim, check_shape, &check_ndim, mode);
    if (res != OK) goto done;

    const uint32_t protect[2] = { source_handle_a, source_handle_b };
    ITEM_TYPE itype = source_arr_a->itype;
    if (create_csnarray_locked(csound, reg, &p->h, source_dim_a, source_shape_a, &p->array, p->handle, protect, 2U, &err, itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    if (source_arr_a->size > 0) {
        memcpy(p->array->data, source_arr_a->data, sizeof(double) * source_arr_a->size * itype);
        p->array->size = source_arr_a->size;
    }

    SET_KDATA_BEGIN(p, reg);
    p->k_data.prev_size = source_arr_a->size;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_vec_k_helper(CSOUND *csound, CSN_BINOP_HH *p, CSN_VECOP_MODE mode) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REG_HANDLE(csound, &p->h, reg, p->k_data.owned_handle);

    uint32_t source_handle_a = (uint32_t) p->source_handle_a->id;
    uint32_t source_handle_b = (uint32_t) p->source_handle_b->id;

    int32_t res = OK;
    const char *err = NULL;

    res = CHECK_SELF_ALIAS(csound, &p->h, &p->k_data, source_handle_a, source_handle_b);
    if (res != OK) return res;

    CHECK_KTRIG(p->trig);

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr_a = NULL;
    CSN_ARRAY *source_arr_b = NULL;
    res = vec_body(csound, &p->h, reg, source_handle_a, source_handle_b, &source_arr_a, &source_arr_b);
    if (res != OK) goto done;

    res = vec_reject_complex(csound, &p->h, mode, source_arr_a, source_arr_b);
    if (res != OK) goto done;

    uint32_t *source_shape_a = source_arr_a->shape;
    uint32_t *source_shape_b = source_arr_b->shape;
    size_t source_dim_a = source_arr_a->ndim;
    size_t source_dim_b = source_arr_b->ndim;

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    uint32_t new_ndim = 0;
    res = vec_assign_shape_and_dim(csound, &p->h, source_shape_a, source_shape_b, source_dim_a, source_dim_b, new_shape, &new_ndim, mode);
    if (res != OK) goto done;

    size_t requested_size = 0;
    if (get_array_size_from_shape(&requested_size, new_ndim, new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSN_SLOT *out_slot = get_slot(reg, p->k_data.owned_handle);
    if (out_slot != NULL && CAN_REUSE_ELEMENTWISE(&p->k_data, source_handle_a, source_arr_a, source_handle_b, source_arr_b, out_slot->array, 0.0, 0.0)) {
        p->handle->id = p->k_data.owned_handle;
        goto done;
    }

    CSN_ARRAY *arr = p->array;
    ITEM_TYPE out_itype = (mode == CSN_PAIR_DISTANCE) ? CSN_REAL : source_arr_a->itype;
    size_t logical_size = (source_arr_a->size == 0 || source_arr_b->size == 0) ? 0 : requested_size;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, new_ndim, new_shape, logical_size, out_itype, err);
    if (res != OK) goto done;

    res = vec_assign_value(csound, &p->h, source_arr_a, source_arr_b, arr, mode);
    if (res != OK) goto done;

    SET_KDATA_END(p, new_shape, new_ndim, out_itype);
    p->k_data.prev_size = arr->size;
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle_a, source_arr_a, source_handle_b, source_arr_b, arr, 0.0, 0.0);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_dot(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_vec_helper(csound, p, CSN_DOT);
}

static int32_t csnarray_dot_k_init(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_vec_k_init_helper(csound, p, CSN_DOT);
}

int32_t csnarray_dot_k(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_vec_k_helper(csound, p, CSN_DOT);
}

static int32_t scalar_helper_impl_check(CSOUND *csound, OPDS *perf_h, CSN_ARRAY *source_arr_a, CSN_ARRAY *source_arr_b, double dist_order, const MYFLT *out_value, const COMPLEXDAT *out_complex, CSN_VECOP_MODE mode) {
    uint32_t *source_shape_a = source_arr_a->shape;
    uint32_t *source_shape_b = source_arr_b->shape;
    size_t source_dim_a = source_arr_a->ndim;
    size_t source_dim_b = source_arr_b->ndim;

    if (source_dim_a != 1 || source_dim_b != 1 || source_shape_a[0] != source_shape_b[0]) {
        char abuf[CSN_SHAPE_STR_MAX], bbuf[CSN_SHAPE_STR_MAX];
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Both arrays must be 1-D with the same size, got %s and %s", shape_str(abuf, sizeof(abuf), source_shape_a, (uint32_t) source_dim_a), shape_str(bbuf, sizeof(bbuf), source_shape_b, (uint32_t) source_dim_b));
    }

    if (source_arr_a->itype != source_arr_b->itype) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Element type mismatch: one array is real and the other complex");
    }

    if (mode == CSN_DISTANCE && dist_order <= 0.0) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Distance order must be >= 1, got %g", dist_order);
    }

    bool wants_complex = (mode != CSN_DISTANCE) && source_arr_a->itype == CSN_COMPLEX;
    if (wants_complex && out_complex == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Handle holds a complex array; declare the result as :Complex;");
    }
    if (!wants_complex && out_value == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Handle holds a real array; declare the result as i");
    }

    return OK;
}

static void scalar_helper_assign_value(CSN_ARRAY *source_arr_a, CSN_ARRAY *source_arr_b, double dist_order, MYFLT *out_value, COMPLEXDAT *out_complex, CSN_VECOP_MODE mode) {
    bool wants_complex = (mode != CSN_DISTANCE) && source_arr_a->itype == CSN_COMPLEX;
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
}

static int32_t csnarray_scalar_helper_impl(CSOUND *csound, CSNREF *ref_a, CSNREF *ref_b, MYFLT *out_value, COMPLEXDAT *out_complex, CSN_VECOP_MODE mode, double dist_order, CSN_REGISTRY **registry) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

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
    res = scalar_helper_impl_check(csound, NULL, source_arr_a, source_arr_b, dist_order, out_value, out_complex, mode);
    if (res != OK) goto done;

    scalar_helper_assign_value(source_arr_a, source_arr_b, dist_order, out_value, out_complex, mode);
    *registry = reg;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_scalar_helper_k_impl(CSOUND *csound, CSNREF *ref_a, CSNREF *ref_b, MYFLT *out_value, COMPLEXDAT *out_complex, CSN_VECOP_MODE mode, double dist_order, OPDS *perf_h, CSN_REGISTRY *registry, const MYFLT *trig, K_DATA *k_data) {
    CSN_REGISTRY *reg = registry;
    if (registry == NULL) {
        return csound->PerfError(csound, perf_h, "[csnarray] Internal error: the csnum array registry is not available");
    }

    uint32_t source_handle_a = (uint32_t) ref_a->id;
    uint32_t source_handle_b = (uint32_t) ref_b->id;

    CHECK_KTRIG(trig);

    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot_a = get_slot(reg, source_handle_a);
    if (slot_a == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle_a);
    }

    CSN_SLOT *slot_b = get_slot(reg, source_handle_b);
    if (slot_b == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle_b);
    }

    CSN_ARRAY *source_arr_a = slot_a->array;
    CSN_ARRAY *source_arr_b = slot_b->array;
    res = scalar_helper_impl_check(csound, perf_h, source_arr_a, source_arr_b, dist_order, out_value, out_complex, mode);
    if (res != OK) goto done;

    if (CAN_REUSE_ELEMENTWISE(k_data, source_handle_a, source_arr_a, source_handle_b, source_arr_b, NULL, dist_order, 0.0)) {
        goto done;
    }

    scalar_helper_assign_value(source_arr_a, source_arr_b, dist_order, out_value, out_complex, mode);
    PUBLISH_ELEMENTWISE(k_data, source_handle_a, source_arr_a, source_handle_b, source_arr_b, NULL, dist_order, 0.0);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_scalar_helper(CSOUND *csound, CSN_BINOP_HH_SCALAR *p, CSN_VECOP_MODE mode, double dist_order) {
    return csnarray_scalar_helper_impl(csound, p->source_handle_a, p->source_handle_b, p->value, NULL, mode, dist_order, &p->registry);
}

static int32_t csnarray_scalar_helper_k(CSOUND *csound, CSN_BINOP_HH_SCALAR *p, CSN_VECOP_MODE mode, double dist_order, const MYFLT *trig) {
    return csnarray_scalar_helper_k_impl(csound, p->source_handle_a, p->source_handle_b, p->value, NULL, mode, dist_order, &p->h, p->registry, trig, &p->k_data);
}

int32_t csnarray_dotcomp_scalar(CSOUND *csound, CSN_BINOPCOMPLEX_HH_SCALAR *p) {
    return csnarray_scalar_helper_impl(csound, p->source_handle_a, p->source_handle_b, NULL, p->value, CSN_DOT_SCALAR, 0.0, &p->registry);
}

int32_t csnarray_dotcomp_scalar_k(CSOUND *csound, CSN_BINOPCOMPLEX_HH_SCALAR *p) {
    return csnarray_scalar_helper_k_impl(csound, p->source_handle_a, p->source_handle_b, NULL, p->value, CSN_DOT_SCALAR, 0.0, &p->h, p->registry, p->trig, &p->k_data);
}

int32_t csnarray_innercomp_scalar(CSOUND *csound, CSN_BINOPCOMPLEX_HH_SCALAR *p) {
    return csnarray_scalar_helper_impl(csound, p->source_handle_a, p->source_handle_b, NULL, p->value, CSN_INNER_SCALAR, 0.0, &p->registry);
}

int32_t csnarray_innercomp_scalar_k(CSOUND *csound, CSN_BINOPCOMPLEX_HH_SCALAR *p) {
    return csnarray_scalar_helper_k_impl(csound, p->source_handle_a, p->source_handle_b, NULL, p->value, CSN_INNER_SCALAR, 0.0, &p->h, p->registry, p->trig, &p->k_data);
}

int32_t csnarray_dot_scalar(CSOUND *csound, CSN_BINOP_HH_SCALAR *p) {
    return csnarray_scalar_helper(csound, p, CSN_DOT_SCALAR, 0.0);
}

int32_t csnarray_dot_scalar_k(CSOUND *csound, CSN_BINOP_HH_SCALAR *p) {
    return csnarray_scalar_helper_k(csound, p, CSN_DOT_SCALAR, 0.0, p->arg_a);
}

int32_t csnarray_inner(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_vec_helper(csound, p, CSN_INNER);
}

static int32_t csnarray_inner_k_init(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_vec_k_init_helper(csound, p, CSN_INNER);
}

int32_t csnarray_inner_k(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_vec_k_helper(csound, p, CSN_INNER);
}

int32_t csnarray_inner_scalar(CSOUND *csound, CSN_BINOP_HH_SCALAR *p) {
    return csnarray_scalar_helper(csound, p, CSN_INNER_SCALAR, 0.0);
}

int32_t csnarray_inner_scalar_k(CSOUND *csound, CSN_BINOP_HH_SCALAR *p) {
    return csnarray_scalar_helper_k(csound, p, CSN_INNER_SCALAR, 0.0, p->arg_a);
}

int32_t csnarray_outer(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_vec_helper(csound, p, CSN_OUTER);
}

static int32_t csnarray_outer_k_init(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_vec_k_init_helper(csound, p, CSN_OUTER);
}

int32_t csnarray_outer_k(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_vec_k_helper(csound, p, CSN_OUTER);
}

int32_t csnarray_project(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_vec_helper(csound, p, CSN_PROJECT);
}

static int32_t csnarray_project_k_init(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_vec_k_init_helper(csound, p, CSN_PROJECT);
}

int32_t csnarray_project_k(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_vec_k_helper(csound, p, CSN_PROJECT);
}

int32_t csnarray_reject(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_vec_helper(csound, p, CSN_REJECT);
}

static int32_t csnarray_reject_k_init(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_vec_k_init_helper(csound, p, CSN_REJECT);
}

int32_t csnarray_reject_k(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_vec_k_helper(csound, p, CSN_REJECT);
}

int32_t csnarray_reflect(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_vec_helper(csound, p, CSN_REFLECT);
}

static int32_t csnarray_reflect_k_init(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_vec_k_init_helper(csound, p, CSN_REFLECT);
}

int32_t csnarray_reflect_k(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_vec_k_helper(csound, p, CSN_REFLECT);
}

int32_t csnarray_cross(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_vec_helper(csound, p, CSN_CROSS);
}

int32_t csnarray_cross_k_init(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_vec_k_init_helper(csound, p, CSN_CROSS);
}

int32_t csnarray_cross_k(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_vec_k_helper(csound, p, CSN_CROSS);
}

static double norm_from_scratch(const double *arr, size_t size, double order, ITEM_TYPE itype) {
    double acc = 0.0;
    for (size_t i = 0; i < size; i++) {
        double x = itype == CSN_COMPLEX ? hypot(arr[i * 2], arr[i * 2 + 1]) : fabs(arr[i]);
        acc += pow(x, order);
    }
    return pow(acc, 1.0 / order);
}

static void norm_assign_value(CSN_ARRAY *source_arr, CSN_ARRAY *destination, double *scratch, int32_t axis, uint32_t run_size, ITEM_TYPE itype, double order) {
    for (size_t linear = 0; linear < destination->size; ++linear) {
        uint32_t dst_coords[CSN_MAX_DIMS] = {0};
        uint32_t src_coords[CSN_MAX_DIMS] = {0};

        from_linear_to_coords(dst_coords, destination->shape, linear, destination->ndim);

        for (uint32_t k = 0; k < run_size; ++k) {
            /* k walks the reduced axis; the surviving axes take the
               destination's own coordinates. */
            for (uint32_t i = 0, j = 0; i < source_arr->ndim; ++i) {
                src_coords[i] = (i == (uint32_t) axis) ? k : dst_coords[j++];
            }
            size_t off = from_coords_to_offset(src_coords, source_arr->strides, source_arr->ndim);
            scratch[k * itype] = source_arr->data[off * itype];
            if (itype == CSN_COMPLEX) scratch[k * 2 + 1] = source_arr->data[off * 2 + 1];
        }

        destination->data[linear] = norm_from_scratch(scratch, run_size, order, itype);
    }
}

int32_t csnarray_norm(CSOUND *csound, CSN_NORM_REDUCTION *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

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
    norm_assign_value(source_arr, arr, scratch, axis, run, itype, order);

done:
    csound->UnlockMutex(reg->mutex);
    if (scratch != NULL) {
        csound->Free(csound, scratch);
    }
    return res;
}

static int32_t csnarray_norm_k_init(CSOUND *csound, CSN_NORM_REDUCTION *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = (uint32_t) p->source_handle->id;
    double axis_value = (double) *p->axis;
    double order = (double) *p->order;

    /* order is a k-argument and normally still reads 0 during the init pass, so
       only a value the orchestra really set can be rejected here; the perf pass
       checks it again before every use. */
    if (order != 0.0 && order < 1.0) {
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
    size_t scratch_cap = scratch_items * 2;
    scratch = csound->Calloc(csound, sizeof(double) * scratch_cap * itype);
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
    /* With the order still unset the output keeps its shape and stays zeroed;
       the first triggered pass fills it. */
    if (order >= 1.0) {
        norm_assign_value(source_arr, arr, scratch, axis, run, itype, order);
    }
    SET_KDATA_BEGIN(p, reg);
    p->scratch.scratch = scratch;
    p->scratch.scratch_capacity = scratch_cap;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_norm_k(CSOUND *csound, CSN_NORM_REDUCTION *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REG_HANDLE(csound, &p->h, reg, p->k_data.owned_handle);

    uint32_t source_handle = (uint32_t) p->source_handle->id;
    double axis_value = (double) *p->axis;
    double order = (double) *p->order;

    if (order < 1.0) {
        return csound->PerfError(csound, &p->h, "[csnarray] Norm order must be >= 1, got %g", order);
    }

    CHECK_KTRIG(p->trig);

    int32_t res = OK;
    const char *err = NULL;

    res = CHECK_SELF_ALIAS(csound, &p->h, &p->k_data, source_handle, 0);
    if (res != OK) return res;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
    }

    CSN_ARRAY *source_arr = source_slot->array;
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    if (!IS_VALID_AXIS(axis_value, source_ndim)) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: finite integers 0..%u)", axis_value, source_ndim, source_ndim - 1);
    }
    uint32_t axis = (uint32_t) axis_value;

    ITEM_TYPE itype = source_arr->itype;
    size_t run = source_shape[axis];
    size_t scratch_items = run > 0 ? run : 1;

    if (scratch_items > p->scratch.scratch_capacity) {
        size_t scratch_cap = scratch_items * 2;
        double *scratch = csound->ReAlloc(csound, p->scratch.scratch, sizeof(double) * scratch_cap * itype);
        if (scratch == NULL) {
            csound->UnlockMutex(reg->mutex);
            return csound->PerfError(csound, &p->h, "[csnarray] Out of memory: allocation of %zu bytes failed", (size_t) (sizeof(double) * scratch_items * itype));
        }
        p->scratch.scratch = scratch;
        p->scratch.scratch_capacity = scratch_cap;
    }

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    uint32_t new_ndim = 0;
    for (uint32_t i = 0; i < source_ndim; ++i) {
        if (i != (uint32_t) axis) new_shape[new_ndim++] = source_shape[i];
    }

    size_t requested_size = 0;
    if (get_array_size_from_shape(&requested_size, new_ndim, new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSN_ARRAY *arr = p->array;
    size_t logical_size = source_arr->size == 0 ? 0 : requested_size;
    CSN_SLOT *out_slot = get_slot(reg, p->k_data.owned_handle);
    if (out_slot != NULL && CAN_REUSE_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, out_slot->array, (double) axis, order)) {
        p->handle->id = p->k_data.owned_handle;
        goto done;
    }

    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, new_ndim, new_shape, logical_size, itype, err);
    if (res != OK) goto done;

    norm_assign_value(source_arr, arr, p->scratch.scratch, axis, run, itype, order);
    SET_KDATA_END(p, new_shape, new_ndim, itype);
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, arr, (double) axis, order);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_norm_scalar(CSOUND *csound, CSN_NORM_REDUCTION_SCALAR *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

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
    p->registry = reg;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

/* Same as the axis form: the order arrives at k-rate and reads 0 at init. */
static int32_t csnarray_norm_scalar_k_init(CSOUND *csound, CSN_NORM_REDUCTION_SCALAR *p) {
    if ((double) *p->order >= 1.0) {
        return csnarray_norm_scalar(csound, p);
    }

    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    p->registry = reg;
    *p->value = FL(0.0);
    return OK;
}

int32_t csnarray_norm_scalar_k(CSOUND *csound, CSN_NORM_REDUCTION_SCALAR *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    double order = (double) *p->order;
    if (order < 1.0) {
        return csound->PerfError(csound, &p->h, "[csnarray] Norm order must be >= 1, got %g", order);
    }

    CHECK_KTRIG(p->trig);

    uint32_t source_handle_a = (uint32_t) p->source_handle->id;
    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, source_handle_a);
    if (slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle_a);
    }

    CSN_ARRAY *source_arr = slot->array;
    if (!CAN_REUSE_ELEMENTWISE(&p->k_data, source_handle_a, source_arr, 0, NULL, NULL, order, 0.0)) {
        *p->value = (MYFLT) norm_from_scratch(source_arr->data, source_arr->size, order, source_arr->itype);
        PUBLISH_ELEMENTWISE(&p->k_data, source_handle_a, source_arr, 0, NULL, NULL, order, 0.0);
    }
    p->registry = reg;

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

static int32_t unary_ax_assign_shape_and_dim(CSOUND *csound, OPDS *perf_h, const size_t source_size, const uint32_t *source_shape, uint32_t *new_dim, uint32_t *new_shape, int32_t axis, CSN_UNARYOP_AX_MODE mode) {
    memcpy(new_shape, source_shape, sizeof(uint32_t) * CSN_MAX_DIMS);
    if (mode == CSN_DIFF) {
        if (axis == -1) {
            if (source_size < 2) {
                return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Diff needs at least 2 elements, got %zu", source_size);
            }

            memset(new_shape, 0, sizeof(uint32_t) * CSN_MAX_DIMS);
            new_shape[0] = (uint32_t) source_size - 1;
            *new_dim = 1U;
        } else {
            if (source_shape[axis] < 2) {
                return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Diff needs at least 2 elements along axis %d, got %u", axis, source_shape[axis]);
            }
            new_shape[axis]--;
        }
    } else if ((mode == CSN_CUMSUM || mode == CSN_CUMPROD) && axis == -1) {
        memset(new_shape, 0, sizeof(uint32_t) * CSN_MAX_DIMS);
        new_shape[0] = (uint32_t) source_size;
        *new_dim = 1U;
    }

    return OK;
}

static int32_t unary_ax_assign_value(CSN_SCRATCH *scratch, CSN_ARRAY *source_arr, CSN_ARRAY *arr, int32_t axis, ITEM_TYPE itype, double order, CSN_UNARYOP_AX_MODE mode) {
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
                /* Skipped when the array is sorting itself: memcpy over itself
                   is undefined, and qsort then works in place. */
                if (arr->data != source_arr->data) {
                    memcpy(arr->data, source_arr->data, sizeof(double) * source_arr->size);
                }
                qsort(arr->data, source_arr->size, sizeof(double), compare_double);
                break;
            case CSN_ARGSORT:{
                if (source_arr->size == 0U)
                    break;
                argsort_slice((ARRAY_ELEMENT *) scratch->scratch, arr->data, source_arr->data, source_arr->size, 1U);
                break;
            }
        }

        return OK;
    }

    uint32_t reduced_shape[CSN_MAX_DIMS] = {0};
    uint32_t reduced_ndim = 0;
    size_t slice_count = 1;
    for (uint32_t i = 0; i < source_arr->ndim; ++i) {
        if (i != (uint32_t) axis) {
            reduced_shape[reduced_ndim++] = source_arr->shape[i];
            slice_count *= source_arr->shape[i];
        }
    }

    size_t src_stride = source_arr->strides[axis];
    size_t dst_stride = arr->strides[axis];
    if ((mode == CSN_SORT || mode == CSN_ARGSORT) && source_arr->shape[axis] == 0U) return OK;

    for (size_t linear = 0; linear < slice_count; ++linear) {
        uint32_t dst_coords[CSN_MAX_DIMS] = {0};
        uint32_t src_coords[CSN_MAX_DIMS] = {0};

        from_linear_to_coords(dst_coords, reduced_shape, linear, reduced_ndim);
        for (uint32_t i = 0, j = 0; i < source_arr->ndim; ++i) {
            src_coords[i] = (i == (uint32_t) axis) ? 0 : dst_coords[j++];
        }

        size_t src_base = from_coords_to_offset(src_coords, source_arr->strides, source_arr->ndim);
        size_t dst_base = from_coords_to_offset(src_coords, arr->strides, source_arr->ndim);
        switch (mode) {
            case CSN_NORMALIZE:
                normalize_slice(arr->data + dst_base * itype, source_arr->data + src_base * itype, source_arr->shape[axis], src_stride, order, itype);
                break;
            case CSN_DIFF:
                diff_slice(arr->data + dst_base * itype, source_arr->data + src_base * itype, source_arr->shape[axis], src_stride, dst_stride, itype);
                break;
            case CSN_GRADIENT:
                gradient_slice(arr->data + dst_base * itype, source_arr->data + src_base * itype, source_arr->shape[axis], src_stride, itype);
                break;
            case CSN_CUMSUM:
            case CSN_CUMPROD: {
                bool is_cumsum = mode == CSN_CUMSUM;
                cumsumprod_slice(arr->data + dst_base * itype, source_arr->data + src_base * itype, source_arr->shape[axis], src_stride, is_cumsum, itype);
                break;
            }
            case CSN_SORT:
                sort_slice((double *) scratch->scratch, arr->data + dst_base, source_arr->data + src_base, source_arr->shape[axis], src_stride);
                break;
            case CSN_ARGSORT:
                argsort_slice((ARRAY_ELEMENT *) scratch->scratch, arr->data + dst_base, source_arr->data + src_base, source_arr->shape[axis], src_stride);
                break;
        }
    }

    return OK;
}

static int32_t unaryop_allocate_scratch(CSOUND *csound, OPDS *perf_h, CSN_SCRATCH *scratch, size_t size, ITEM_TYPE itype, CSN_UNARYOP_AX_MODE mode) {
    /* Only the two sort modes stage a slice through the scratch buffer. Both
       index it by element and an opcode instance only ever runs one mode, so
       the capacity counts elements of that mode's own type. */
    if (mode != CSN_SORT && mode != CSN_ARGSORT) {
        return OK;
    }

    if (scratch->scratch != NULL && size <= scratch->scratch_capacity) {
        return OK;
    }

    size_t elem_size = mode == CSN_ARGSORT ? sizeof(ARRAY_ELEMENT) : sizeof(double);
    size_t new_cap = size > 0 ? size * 2 : 1;
    size_t bytes = elem_size * new_cap * (size_t) itype;
    void *temp = scratch->scratch == NULL
        ? csound->Calloc(csound, bytes)
        : csound->ReAlloc(csound, scratch->scratch, bytes);
    if (temp == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Memory allocation failed");
    }

    scratch->scratch = temp;
    scratch->scratch_capacity = new_cap;
    return OK;
}

static int32_t csnarray_unary_ax_helper(CSOUND *csound, const OPDS *h, CSNREF *src_ref, double axis_value, double order, CSNREF *out_handle, CSN_ARRAY **out_array, CSN_UNARYOP_AX_MODE mode, CSN_SCRATCH *scratch) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

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
    ITEM_TYPE itype = source_arr->itype;

    if ((mode == CSN_SORT || mode == CSN_ARGSORT) && itype == CSN_COMPLEX) {
        res = csound->InitError(csound, "[csnarray] Sort operation is for real array only");
        goto done;
    }

    if (mode == CSN_GRADIENT && itype == CSN_COMPLEX) {
        res = csound->InitError(csound, "[csnarray] This operation is not implemented for complex arrays");
        goto done;
    }

    if (axis_value != -1.0 && !IS_VALID_AXIS(axis_value, source_ndim)) {
        res = csound->InitError(csound, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: -1 for all axes, or finite integers 0..%u)", axis_value, source_ndim, source_ndim - 1);
        goto done;
    }

    int32_t axis = (int32_t) axis_value;

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    uint32_t new_dim = source_ndim;
    res = unary_ax_assign_shape_and_dim(csound, NULL, source_arr->size, source_shape, &new_dim, new_shape, axis, mode);
    if (res != OK) goto done;

    CSN_ARRAY *arr = source_arr;
    if (out_handle != NULL) {
        const uint32_t protect[1] = { src_ref->id };
        if (create_csnarray_locked(csound, reg, h, new_dim, new_shape, out_array, out_handle, protect, 1U, &err, source_arr->itype) != OK) {
            res = csound->InitError(csound, "[csnarray] %s", err);
            goto done;
        }
        arr = *out_array;
    }

    res = unaryop_allocate_scratch(csound, NULL, scratch, source_arr->size, itype, mode);
    if (res != OK) goto done;

    res = unary_ax_assign_value(scratch, source_arr, arr, axis, itype, order, mode);
    /* arr is source_arr on the in-place forms, so this is the write that a
       consumer of the source has to be told about. The out-of-place form fills
       an array it just created, which no cache can be holding yet. */
    if (res == OK && out_handle == NULL) update_array_data_version(&arr->version);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32 opunary_ax_in_k_deinit(CSOUND *csound, CSN_UNARYOP_AX_IN *p) {
    if (p->scratch.scratch != NULL) {
        csound->Free(csound, p->scratch.scratch);
    }

    return OK;
}

static int32_t csnarray_unary_ax_k_init_helper(CSOUND *csound, const OPDS *h, CSNREF *src_ref, double axis_value, double order, CSNREF *out_handle, CSN_ARRAY **out_array, CSN_UNARYOP_AX_MODE mode, CSN_SCRATCH *scratch, K_DATA *k_data) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    /* Only csnnormalize takes an order, and it is a k-argument that normally
       still reads 0 during the init pass: reject only a value the orchestra
       really set. The perf pass checks it again before every use. */
    if (mode == CSN_NORMALIZE && order != 0.0 && order < 1.0) {
        return csound->InitError(csound, "[csnarray] Norm order must be >= 1, got %g", order);
    }

    uint32_t source_handle = src_ref->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
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

    if (mode == CSN_GRADIENT && itype == CSN_COMPLEX) {
        res = csound->InitError(csound, "[csnarray] This operation is not implemented for complex arrays");
        goto done;
    }

    if (axis_value != -1.0 && !IS_VALID_AXIS(axis_value, source_ndim)) {
        res = csound->InitError(csound, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: -1 for all axes, or finite integers 0..%u)", axis_value, source_ndim, source_ndim - 1);
        goto done;
    }

    int32_t axis = (int32_t) axis_value;

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    uint32_t new_dim = source_ndim;
    res = unary_ax_assign_shape_and_dim(csound, NULL, source_arr->size, source_shape, &new_dim, new_shape, axis, mode);
    if (res != OK) goto done;

    CSN_ARRAY *arr = source_arr;
    if (out_handle != NULL) {
        const uint32_t protect[1] = { src_ref->id };
        if (create_csnarray_locked(csound, reg, h, new_dim, new_shape, out_array, out_handle, protect, 1U, &err, source_arr->itype) != OK) {
            res = csound->InitError(csound, "[csnarray] %s", err);
            goto done;
        }
        arr = *out_array;
    }

    res = unaryop_allocate_scratch(csound, NULL, scratch, source_arr->size, itype, mode);
    if (res != OK) goto done;

    /* csnnormalize with the order still unset leaves the output as created; the
       first triggered pass computes it. Every other mode ignores order. */
    if (mode != CSN_NORMALIZE || order >= 1.0) {
        res = unary_ax_assign_value(scratch, source_arr, arr, axis, itype, order, mode);
        if (res != OK) goto done;
        if (out_handle == NULL) update_array_data_version(&arr->version);
    }

    memset(k_data->prev_shape, 0, sizeof(k_data->prev_shape));
    memcpy(k_data->prev_shape, new_shape, sizeof(k_data->prev_shape));
    k_data->prev_ndim = new_dim;
    k_data->prev_itype = itype;
    k_data->registry = reg;
    k_data->prev_size = source_arr->size;
    if (out_handle != NULL) {
        k_data->owned_handle = out_handle->id;
    }

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

/* csnnormalize is the only mode with an order argument, so on every other form
   the trigger is bound one slot earlier — into the struct's order field. Both
   structs list the inputs in the same sequence, and INOCOUNT is the declared
   arity, so it tells the two apart. */
static inline MYFLT *unary_ax_trig(const OPDS *h, MYFLT *order, MYFLT *trig) {
    return h->optext->t.inArgCount > 3 ? trig : order;
}

static int32_t csnarray_unary_ax_k_helper(CSOUND *csound, OPDS *h, CSNREF *src_ref, double axis_value, double order, CSNREF *out_handle, CSN_ARRAY **out_array, CSN_UNARYOP_AX_MODE mode, CSN_SCRATCH *scratch, K_DATA *k_data, const MYFLT *trig) {
    CSN_REGISTRY *reg = k_data->registry;
    CHECK_REGISTRY(csound, h, reg);

    if (mode == CSN_NORMALIZE && order < 1.0) {
        return csound->PerfError(csound, h, "[csnarray] Norm order must be >= 1, got %g", order);
    }

    uint32_t source_handle = src_ref->id;

    int32_t res = OK;
    const char *err = NULL;

    /* Only csngrad reads a neighbour it has already written; every other mode
       here either finishes reading the whole slice before it writes any of it
       (the norm accumulates first, the two sorts stage through the scratch) or
       writes the cell it just read (the running totals). Those are checked
       against the layout instead, once the source is resolved — which is also
       what rejects csndiff, whose result is one element shorter. */
    if (out_handle != NULL && mode == CSN_GRADIENT) {
        res = CHECK_SELF_ALIAS(csound, h, k_data, source_handle, 0);
        if (res != OK) return res;
    }

    CHECK_KTRIG(trig);

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) src_ref->id);
    }

    CSN_ARRAY *source_arr = source_slot->array;
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;
    ITEM_TYPE itype = source_arr->itype;

    if ((mode == CSN_SORT || mode == CSN_ARGSORT) && itype == CSN_COMPLEX) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, h, "[csnarray] Sort operation is for real array only");
    }

    if (mode == CSN_GRADIENT && itype == CSN_COMPLEX) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, h, "[csnarray] This operation is not implemented for complex arrays");
    }

    if (axis_value != -1.0 && !IS_VALID_AXIS(axis_value, source_ndim)) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, h, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: -1 for all axes, or finite integers 0..%u)", axis_value, source_ndim, source_ndim - 1);
    }

    int32_t axis = (int32_t) axis_value;

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    uint32_t new_dim = source_ndim;
    res = unary_ax_assign_shape_and_dim(csound, h, source_arr->size, source_shape, &new_dim, new_shape, axis, mode);
    if (res != OK) goto done;


    CSN_SLOT *out_slot = out_handle != NULL ? get_slot(reg, k_data->owned_handle) : NULL;
    if ((out_handle == NULL || out_slot != NULL)
        && CAN_REUSE_ELEMENTWISE(k_data, source_handle, source_arr, 0, NULL, out_slot != NULL ? out_slot->array : NULL, axis_value, order)) {
        if (out_handle != NULL) out_handle->id = k_data->owned_handle;
        goto done;
    }

    CSN_ARRAY *arr = source_arr;
    if (out_handle != NULL) {
        size_t requested_size = 0;
        if (get_array_size_from_shape(&requested_size, new_dim, new_shape) != OK) {
            csound->UnlockMutex(reg->mutex);
            return csound->PerfError(csound, h, "[csnarray] Invalid shape or element count exceeds the configured limit");
        }
        if (mode != CSN_GRADIENT) {
            res = CHECK_SELF_ALIAS_CELL_LOCAL(csound, h, k_data, source_handle, source_arr, new_dim, new_shape, itype);
            if (res != OK) goto done;
        }

        size_t logical_size = source_arr->size == 0 ? 0 : requested_size;
        res = NEED_TO_UPDATE_SLOT(csound, h, out_array, k_data, NULL, new_dim, new_shape, logical_size, itype, err);
        if (res != OK) goto done;
        arr = *out_array;
    }

    res = unaryop_allocate_scratch(csound, h, scratch, source_arr->size, itype, mode);
    if (res != OK) goto done;

    res = unary_ax_assign_value(scratch, source_arr, arr, axis, itype, order, mode);
    if (res != OK) goto done;
    /* The out-of-place form already carries a new generation from
       NEED_TO_UPDATE_SLOT; only the in-place form writes an array nobody else
       has stamped. */
    if (out_handle == NULL) update_array_data_version(&arr->version);

    memset(k_data->prev_shape, 0, sizeof(k_data->prev_shape));
    memcpy(k_data->prev_shape, new_shape, sizeof(k_data->prev_shape));
    k_data->prev_ndim = new_dim;
    k_data->prev_itype = itype;
    k_data->prev_size = source_arr->size;
    if (out_handle != NULL) {
        out_handle->id = k_data->owned_handle;
    }
    PUBLISH_ELEMENTWISE(k_data, source_handle, source_arr, 0, NULL, out_handle != NULL ? arr : NULL, axis_value, order);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_normalize(CSOUND *csound, CSN_UNARYOP_AX *p) {
    return csnarray_unary_ax_helper(csound, &p->h, p->source_handle, (double) *p->axis, (double) *p->order, p->handle, &p->array, CSN_NORMALIZE, &p->scratch);
}

static int32_t csnarray_normalize_k_init(CSOUND *csound, CSN_UNARYOP_AX *p) {
    return csnarray_unary_ax_k_init_helper(csound, &p->h, p->source_handle, (double) *p->axis, (double) *p->order, p->handle, &p->array, CSN_NORMALIZE, &p->scratch, &p->k_data);
}

int32_t csnarray_normalize_k(CSOUND *csound, CSN_UNARYOP_AX *p) {
    return csnarray_unary_ax_k_helper(csound, &p->h, p->source_handle, (double) *p->axis, (double) *p->order, p->handle, &p->array, CSN_NORMALIZE, &p->scratch, &p->k_data, unary_ax_trig(&p->h, p->order, p->trig));
}

int32_t csnarray_normalize_in(CSOUND *csound, CSN_UNARYOP_AX_IN *p) {
    return csnarray_unary_ax_helper(csound, &p->h, p->source_handle, (double) *p->axis, (double) *p->order, NULL, NULL, CSN_NORMALIZE, &p->scratch);
}

static int32_t csnarray_normalize_in_k_init(CSOUND *csound, CSN_UNARYOP_AX_IN *p) {
    return csnarray_unary_ax_k_init_helper(csound, &p->h, p->source_handle, (double) *p->axis, (double) *p->order, NULL, NULL, CSN_NORMALIZE, &p->scratch, &p->k_data);
}

int32_t csnarray_normalize_in_k(CSOUND *csound, CSN_UNARYOP_AX_IN *p) {
    return csnarray_unary_ax_k_helper(csound, &p->h, p->source_handle, (double) *p->axis, (double) *p->order, NULL, NULL, CSN_NORMALIZE, &p->scratch, &p->k_data, unary_ax_trig(&p->h, p->order, p->trig));
}

int32_t csnarray_sort_in(CSOUND *csound, CSN_UNARYOP_AX_IN *p) {
    return csnarray_unary_ax_helper(csound, &p->h, p->source_handle, (double) *p->axis, 0.0, NULL, NULL, CSN_SORT, &p->scratch);
}

static int32_t csnarray_sort_in_k_init(CSOUND *csound, CSN_UNARYOP_AX_IN *p) {
    return csnarray_unary_ax_k_init_helper(csound, &p->h, p->source_handle, (double) *p->axis, 0.0, NULL, NULL, CSN_SORT, &p->scratch, &p->k_data);
}

int32_t csnarray_sort_in_k(CSOUND *csound, CSN_UNARYOP_AX_IN *p) {
    return csnarray_unary_ax_k_helper(csound, &p->h, p->source_handle, (double) *p->axis, 0.0, NULL, NULL, CSN_SORT, &p->scratch, &p->k_data, unary_ax_trig(&p->h, p->order, p->trig));
}

int32_t csnarray_distance(CSOUND *csound, CSN_BINOP_HH_SCALAR *p) {
    double dist_order = (double) *p->arg_a;
    return csnarray_scalar_helper(csound, p, CSN_DISTANCE, dist_order);
}

/* The Minkowski order is a k-argument here, so during the init pass it normally
   still reads 0, which no validation can accept. Stash the registry the perf
   pass needs, leave the scalar at 0, and let the first pass compute it. */
static int32_t csnarray_distance_k_init(CSOUND *csound, CSN_BINOP_HH_SCALAR *p) {
    double dist_order = (double) *p->arg_a;
    if (dist_order >= 1.0) {
        return csnarray_distance(csound, p);
    }

    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    p->registry = reg;
    *p->value = FL(0.0);
    return OK;
}

int32_t csnarray_distance_k(CSOUND *csound, CSN_BINOP_HH_SCALAR *p) {
    double dist_order = (double) *p->arg_a;
    return csnarray_scalar_helper_k(csound, p, CSN_DISTANCE, dist_order, p->arg_b);
}

int32_t csnarray_pair_distance(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_vec_helper(csound, p, CSN_PAIR_DISTANCE);
}

static int32_t csnarray_pair_distance_k_init(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_vec_k_init_helper(csound, p, CSN_PAIR_DISTANCE);
}

int32_t csnarray_pair_distance_k(CSOUND *csound, CSN_BINOP_HH *p) {
    return csnarray_vec_k_helper(csound, p, CSN_PAIR_DISTANCE);
}

int32_t csnarray_diff(CSOUND *csound, CSN_UNARYOP_AX *p) {
    return csnarray_unary_ax_helper(csound, &p->h, p->source_handle, (double) *p->axis, 0.0, p->handle, &p->array, CSN_DIFF, &p->scratch);
}

static int32_t csnarray_diff_k_init(CSOUND *csound, CSN_UNARYOP_AX *p) {
    return csnarray_unary_ax_k_init_helper(csound, &p->h, p->source_handle, (double) *p->axis, 0.0, p->handle, &p->array, CSN_DIFF, &p->scratch, &p->k_data);
}

int32_t csnarray_diff_k(CSOUND *csound, CSN_UNARYOP_AX *p) {
    return csnarray_unary_ax_k_helper(csound, &p->h, p->source_handle, (double) *p->axis, 0.0, p->handle, &p->array, CSN_DIFF, &p->scratch, &p->k_data, unary_ax_trig(&p->h, p->order, p->trig));
}

int32_t csnarray_gradient(CSOUND *csound, CSN_UNARYOP_AX *p) {
    return csnarray_unary_ax_helper(csound, &p->h, p->source_handle, (double) *p->axis, 0.0, p->handle, &p->array, CSN_GRADIENT, &p->scratch);
}

static int32_t csnarray_gradient_k_init(CSOUND *csound, CSN_UNARYOP_AX *p) {
    return csnarray_unary_ax_k_init_helper(csound, &p->h, p->source_handle, (double) *p->axis, 0.0, p->handle, &p->array, CSN_GRADIENT, &p->scratch, &p->k_data);
}

int32_t csnarray_gradient_k(CSOUND *csound, CSN_UNARYOP_AX *p) {
    return csnarray_unary_ax_k_helper(csound, &p->h, p->source_handle, (double) *p->axis, 0.0, p->handle, &p->array, CSN_GRADIENT, &p->scratch, &p->k_data, unary_ax_trig(&p->h, p->order, p->trig));
}

int32_t csnarray_cumsum(CSOUND *csound, CSN_UNARYOP_AX *p) {
    return csnarray_unary_ax_helper(csound, &p->h, p->source_handle, (double) *p->axis, 0.0, p->handle, &p->array, CSN_CUMSUM, &p->scratch);
}

static int32_t csnarray_cumsum_k_init(CSOUND *csound, CSN_UNARYOP_AX *p) {
    return csnarray_unary_ax_k_init_helper(csound, &p->h, p->source_handle, (double) *p->axis, 0.0, p->handle, &p->array, CSN_CUMSUM, &p->scratch, &p->k_data);
}

int32_t csnarray_cumsum_k(CSOUND *csound, CSN_UNARYOP_AX *p) {
    return csnarray_unary_ax_k_helper(csound, &p->h, p->source_handle, (double) *p->axis, 0.0, p->handle, &p->array, CSN_CUMSUM, &p->scratch, &p->k_data, unary_ax_trig(&p->h, p->order, p->trig));
}

int32_t csnarray_cumprod(CSOUND *csound, CSN_UNARYOP_AX *p) {
    return csnarray_unary_ax_helper(csound, &p->h, p->source_handle, (double) *p->axis, 0.0, p->handle, &p->array, CSN_CUMPROD, &p->scratch);
}

static int32_t csnarray_cumprod_k_init(CSOUND *csound, CSN_UNARYOP_AX *p) {
    return csnarray_unary_ax_k_init_helper(csound, &p->h, p->source_handle, (double) *p->axis, 0.0, p->handle, &p->array, CSN_CUMPROD, &p->scratch, &p->k_data);
}

int32_t csnarray_cumprod_k(CSOUND *csound, CSN_UNARYOP_AX *p) {
    return csnarray_unary_ax_k_helper(csound, &p->h, p->source_handle, (double) *p->axis, 0.0, p->handle, &p->array, CSN_CUMPROD, &p->scratch, &p->k_data, unary_ax_trig(&p->h, p->order, p->trig));
}

int32_t csnarray_sort(CSOUND *csound, CSN_UNARYOP_AX *p) {
    return csnarray_unary_ax_helper(csound, &p->h, p->source_handle, (double) *p->axis, 0.0, p->handle, &p->array, CSN_SORT, &p->scratch);
}

static int32_t csnarray_sort_k_init(CSOUND *csound, CSN_UNARYOP_AX *p) {
    return csnarray_unary_ax_k_init_helper(csound, &p->h, p->source_handle, (double) *p->axis, 0.0, p->handle, &p->array, CSN_SORT, &p->scratch, &p->k_data);
}

int32_t csnarray_sort_k(CSOUND *csound, CSN_UNARYOP_AX *p) {
    return csnarray_unary_ax_k_helper(csound, &p->h, p->source_handle, (double) *p->axis, 0.0, p->handle, &p->array, CSN_SORT, &p->scratch, &p->k_data, unary_ax_trig(&p->h, p->order, p->trig));
}

int32_t csnarray_argsort(CSOUND *csound, CSN_UNARYOP_AX *p) {
    return csnarray_unary_ax_helper(csound, &p->h, p->source_handle, (double) *p->axis, 0.0, p->handle, &p->array, CSN_ARGSORT, &p->scratch);
}

static int32_t csnarray_argsort_k_init(CSOUND *csound, CSN_UNARYOP_AX *p) {
    return csnarray_unary_ax_k_init_helper(csound, &p->h, p->source_handle, (double) *p->axis, 0.0, p->handle, &p->array, CSN_ARGSORT, &p->scratch, &p->k_data);
}

int32_t csnarray_argsort_k(CSOUND *csound, CSN_UNARYOP_AX *p) {
    return csnarray_unary_ax_k_helper(csound, &p->h, p->source_handle, (double) *p->axis, 0.0, p->handle, &p->array, CSN_ARGSORT, &p->scratch, &p->k_data, unary_ax_trig(&p->h, p->order, p->trig));
}

int32_t csnarray_matmul_scalar(CSOUND *csound, CSN_BINOP_HH_SCALAR *p) {
    return csnarray_scalar_helper(csound, p, CSN_DOT_SCALAR, 0.0);
}

int32_t csnarray_matmul_scalar_k(CSOUND *csound, CSN_BINOP_HH_SCALAR *p) {
    return csnarray_scalar_helper_k(csound, p, CSN_DOT_SCALAR, 0.0, p->arg_a);
}

static int32_t matmul_body(CSOUND *csound, OPDS *perf_h, CSN_REGISTRY *reg, CSN_ARRAY **source_array_a,CSN_ARRAY **source_array_b, uint32_t source_handle_a, uint32_t source_handle_b) {
    CSN_SLOT *source_slot_a = get_slot(reg, source_handle_a);
    if (source_slot_a == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle_a);
    }

    CSN_SLOT *source_slot_b = get_slot(reg, source_handle_b);
    if (source_slot_b == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle_b);
    }

    *source_array_a = source_slot_a->array;
    *source_array_b = source_slot_b->array;
    CSN_ARRAY *source_arr_a = *source_array_a;
    CSN_ARRAY *source_arr_b = *source_array_b;

    if (source_arr_a->itype != source_arr_b->itype) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Element type mismatch: one array is real and the other complex");
    }

    return OK;
}

typedef struct {
    uint32_t a_shape[CSN_MAX_DIMS];
    uint32_t b_shape[CSN_MAX_DIMS];
    size_t a_strides[CSN_MAX_DIMS];
    size_t b_strides[CSN_MAX_DIMS];
    uint32_t batch_shape[CSN_MAX_DIMS];
    uint32_t new_shape[CSN_MAX_DIMS];
    uint32_t a_dim;
    uint32_t b_dim;
    uint32_t a_batch_ndim;
    uint32_t b_batch_ndim;
    uint32_t batch_ndim;
    uint32_t new_dim;
    uint32_t rows;
    uint32_t inner;
    uint32_t cols;
} CSN_MATMUL_LAYOUT;

static int32_t matmul_assign_shape_and_dim(CSOUND *csound, OPDS *perf_h, CSN_ARRAY *source_arr_a, CSN_ARRAY *source_arr_b, CSN_MATMUL_LAYOUT *box) {
    uint32_t source_a_dim = source_arr_a->ndim;
    uint32_t source_b_dim = source_arr_b->ndim;
    bool a_promoted = source_a_dim == 1;
    bool b_promoted = source_b_dim == 1;

    if (a_promoted) {
        box->a_dim = 2U;
        box->a_shape[0] = 1U;
        box->a_shape[1] = source_arr_a->shape[0];
        box->a_strides[0] = 0;
        box->a_strides[1] = source_arr_a->strides[0];
    } else {
        box->a_dim = source_a_dim;
        memcpy(box->a_shape, source_arr_a->shape, sizeof(uint32_t) * box->a_dim);
        memcpy(box->a_strides, source_arr_a->strides, sizeof(size_t) * box->a_dim);
    }

    if (b_promoted) {
        box->b_dim = 2U;
        box->b_shape[0] = source_arr_b->shape[0];
        box->b_shape[1] = 1U;
        box->b_strides[0] = source_arr_b->strides[0];
        box->b_strides[1] = 0;
    } else {
        box->b_dim = source_b_dim;
        memcpy(box->b_shape, source_arr_b->shape, sizeof(uint32_t) * box->b_dim);
        memcpy(box->b_strides, source_arr_b->strides, sizeof(size_t) * box->b_dim);
    }

    box->rows = box->a_shape[box->a_dim - 2];
    box->inner = box->a_shape[box->a_dim - 1];
    box->cols = box->b_shape[box->b_dim - 1];

    if (box->inner != box->b_shape[box->b_dim - 2]) {
        char abuf[CSN_SHAPE_STR_MAX], bbuf[CSN_SHAPE_STR_MAX];
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Shapes %s and %s are not valid for a matrix product: the last axis of the first (%u) must match the second-to-last of the second (%u)", shape_str(abuf, sizeof(abuf), source_arr_a->shape, source_a_dim), shape_str(bbuf, sizeof(bbuf), source_arr_b->shape, source_b_dim), box->inner, box->b_shape[box->b_dim - 2]);
    }

    box->a_batch_ndim = box->a_dim - 2;
    box->b_batch_ndim = box->b_dim - 2;
    box->batch_ndim = box->a_batch_ndim > box->b_batch_ndim ? box->a_batch_ndim : box->b_batch_ndim;

    for (uint32_t i = 0; i < box->batch_ndim; ++i) {
        uint32_t ea = i < box->a_batch_ndim ? box->a_shape[box->a_batch_ndim - 1 - i] : 1U;
        uint32_t eb = i < box->b_batch_ndim ? box->b_shape[box->b_batch_ndim - 1 - i] : 1U;
        if (ea != eb && ea != 1U && eb != 1U) {
            char abuf[CSN_SHAPE_STR_MAX], bbuf[CSN_SHAPE_STR_MAX];
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Batch shapes %s and %s cannot be broadcast together: outside the last two axes, aligned from the right, each pair must match or be 1", shape_str(abuf, sizeof(abuf), source_arr_a->shape, source_a_dim), shape_str(bbuf, sizeof(bbuf), source_arr_b->shape, source_b_dim));
        }
        box->batch_shape[box->batch_ndim - 1 - i] = ea > eb ? ea : eb;
    }

    uint32_t out_ndim = 0;
    for (uint32_t i = 0; i < box->batch_ndim; ++i) {
        box->new_shape[out_ndim++] = box->batch_shape[i];
    }
    if (!a_promoted) box->new_shape[out_ndim++] = box->rows;
    if (!b_promoted) box->new_shape[out_ndim++] = box->cols;

    if (out_ndim > CSN_MAX_DIMS) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] The result would have %u dimensions, the maximum is %d", out_ndim, CSN_MAX_DIMS);
    }
    box->new_dim = out_ndim;
    return OK;
}

static void matmul_assign_value(CSN_ARRAY *source_arr_a, CSN_ARRAY *source_arr_b, CSN_ARRAY *arr, CSN_MATMUL_LAYOUT *box) {
    size_t batch_count = 1;
    for (uint32_t i = 0; i < box->batch_ndim; ++i) {
        batch_count *= box->batch_shape[i];
    }

    size_t a_row_stride = box->a_strides[box->a_dim - 2];
    size_t a_col_stride = box->a_strides[box->a_dim - 1];
    size_t b_row_stride = box->b_strides[box->b_dim - 2];
    size_t b_col_stride = box->b_strides[box->b_dim - 1];

    for (size_t batch = 0; batch < batch_count; ++batch) {
        uint32_t batch_coords[CSN_MAX_DIMS] = {0};
        from_linear_to_coords(batch_coords, box->batch_shape, batch, box->batch_ndim);

        size_t a_base = 0;
        for (uint32_t i = 0; i < box->a_batch_ndim; ++i) {
            uint32_t c = box->a_shape[i] == 1U ? 0U : batch_coords[box->batch_ndim - box->a_batch_ndim + i];
            a_base += (size_t) c * box->a_strides[i];
        }

        size_t b_base = 0;
        for (uint32_t i = 0; i < box->b_batch_ndim; ++i) {
            uint32_t c = box->b_shape[i] == 1U ? 0U : batch_coords[box->batch_ndim - box->b_batch_ndim + i];
            b_base += (size_t) c * box->b_strides[i];
        }

        for (uint32_t r = 0; r < box->rows; ++r) {
            for (uint32_t c = 0; c < box->cols; ++c) {
                /* Bilinear, as numpy.matmul */
                CSN_COMPLEXDAT acc = { 0.0, 0.0 };
                for (uint32_t k = 0; k < box->inner; ++k) {
                    size_t off_a = a_base + (size_t) r * a_row_stride + (size_t) k * a_col_stride;
                    size_t off_b = b_base + (size_t) k * b_row_stride + (size_t) c * b_col_stride;
                    CSN_COMPLEXDAT prod = {0};
                    complex_prod(&prod, item_at(source_arr_a, off_a), item_at(source_arr_b, off_b));
                    complex_add(&acc, acc, prod);
                }
                item_set(arr, (batch * box->rows + r) * box->cols + c, acc);
            }
        }
    }
}

int32_t csnarray_matmul(CSOUND *csound, CSN_BINOP_HH *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle_a = p->source_handle_a->id;
    uint32_t source_handle_b = p->source_handle_b->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr_a = NULL;
    CSN_ARRAY *source_arr_b = NULL;
    res = matmul_body(csound, NULL, reg, &source_arr_a, &source_arr_b, source_handle_a, source_handle_b);
    if (res != OK) goto done;

    uint32_t source_a_dim = source_arr_a->ndim;
    uint32_t source_b_dim = source_arr_b->ndim;
    ITEM_TYPE itype = source_arr_a->itype;

    if (source_a_dim == 1 && source_b_dim == 1) {
        res = csound->InitError(csound, "[csnarray] The matrix product of two 1-D arrays is a scalar; declare the output as i, not as a handle");
        goto done;
    }

    CSN_MATMUL_LAYOUT box = {0};
    res = matmul_assign_shape_and_dim(csound, NULL, source_arr_a, source_arr_b, &box);
    if (res != OK) goto done;

    const uint32_t protect[2] = { source_handle_a, source_handle_b };
    if (create_csnarray_locked(csound, reg, &p->h, box.new_dim, box.new_shape, &p->array, p->handle, protect, 2U, &err, itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;
    matmul_assign_value(source_arr_a, source_arr_b, arr, &box);
    SET_KDATA_BEGIN(p, reg);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_matmul_k(CSOUND *csound, CSN_BINOP_HH *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REG_HANDLE(csound, &p->h, reg, p->k_data.owned_handle);

    uint32_t source_handle_a = p->source_handle_a->id;
    uint32_t source_handle_b = p->source_handle_b->id;

    CHECK_KTRIG(p->trig);

    int32_t res = OK;
    const char *err = NULL;

    res = CHECK_SELF_ALIAS(csound, &p->h, &p->k_data, source_handle_a, source_handle_b);
    if (res != OK) return res;

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr_a = NULL;
    CSN_ARRAY *source_arr_b = NULL;
    res = matmul_body(csound, &p->h, reg, &source_arr_a, &source_arr_b, source_handle_a, source_handle_b);
    if (res != OK) goto done;

    uint32_t source_a_dim = source_arr_a->ndim;
    uint32_t source_b_dim = source_arr_b->ndim;
    ITEM_TYPE itype = source_arr_a->itype;

    if (source_a_dim == 1 && source_b_dim == 1) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] The matrix product of two 1-D arrays is a scalar; declare the output as i, not as a handle");
    }

    CSN_MATMUL_LAYOUT box = {0};
    res = matmul_assign_shape_and_dim(csound, &p->h, source_arr_a, source_arr_b, &box);
    if (res != OK) goto done;

    size_t requested_size = 0;
    if (get_array_size_from_shape(&requested_size, box.new_dim, box.new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSN_SLOT *out_slot = get_slot(reg, p->k_data.owned_handle);
    if (out_slot != NULL && CAN_REUSE_ELEMENTWISE(&p->k_data, source_handle_a, source_arr_a, source_handle_b, source_arr_b, out_slot->array, 0.0, 0.0)) {
        p->handle->id = p->k_data.owned_handle;
        goto done;
    }

    CSN_ARRAY *arr = p->array;
    size_t logical_size = (source_arr_a->size == 0 && source_arr_b->size == 0) ? 0 : requested_size;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, box.new_dim, box.new_shape, logical_size, itype, err);
    if (res != OK) goto done;

    matmul_assign_value(source_arr_a, source_arr_b, arr, &box);
    SET_KDATA_END(p, box.new_shape, box.new_dim, itype);
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle_a, source_arr_a, source_handle_b, source_arr_b, arr, 0.0, 0.0);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_angle_distance(CSOUND *csound, CSN_BINOP_HH_SCALAR *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle_a = (uint32_t) p->source_handle_a->id;
    uint32_t source_handle_b = (uint32_t) p->source_handle_b->id;

    /* Stashed before the zero-length shortcut below, which is a warning and not
       an error: leaving it to the tail would let a note that starts with a zero
       vector run on with a NULL registry, and every later k pass would fail. */
    p->registry = reg;

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

int32_t csnarray_angle_distance_k(CSOUND *csound, CSN_BINOP_HH_SCALAR *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    uint32_t source_handle_a = (uint32_t) p->source_handle_a->id;
    uint32_t source_handle_b = (uint32_t) p->source_handle_b->id;

    CHECK_KTRIG(p->arg_a);

    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot_a = get_slot(reg, source_handle_a);
    if (slot_a == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle_a);
    }

    CSN_SLOT *slot_b = get_slot(reg, source_handle_b);
    if (slot_b == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle_b);
    }

    CSN_ARRAY *source_arr_a = slot_a->array;
    CSN_ARRAY *source_arr_b = slot_b->array;

    if (CAN_REUSE_ELEMENTWISE(&p->k_data, source_handle_a, source_arr_a, source_handle_b, source_arr_b, NULL, 0.0, 0.0)) {
        goto done;
    }

    uint32_t *source_shape_a = source_arr_a->shape;
    uint32_t *source_shape_b = source_arr_b->shape;
    size_t source_dim_a = source_arr_a->ndim;
    size_t source_dim_b = source_arr_b->ndim;

    if (source_dim_a != source_dim_b || memcmp(source_shape_a, source_shape_b, sizeof(uint32_t) * CSN_MAX_DIMS) != 0) {
        char abuf[CSN_SHAPE_STR_MAX], bbuf[CSN_SHAPE_STR_MAX];
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Both arrays must have the same shape, got %s and %s", shape_str(abuf, sizeof(abuf), source_shape_a, (uint32_t) source_dim_a), shape_str(bbuf, sizeof(bbuf), source_shape_b, (uint32_t) source_dim_b));
    }

    if (source_arr_a->itype != source_arr_b->itype) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Element type mismatch: one array is real and the other complex");
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
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle_a, source_arr_a, source_handle_b, source_arr_b, NULL, 0.0, 0.0);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_trace_impl(CSOUND *csound, CSNREF *src_ref, MYFLT *out_value, COMPLEXDAT *out_complex, CSN_REGISTRY **registry) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = (uint32_t) src_ref->id;
    int32_t res = OK;

    *registry = reg;

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

static int32_t csnarray_trace_impl_k(CSOUND *csound, OPDS *perf_h, CSNREF *src_ref, MYFLT *out_value, COMPLEXDAT *out_complex, CSN_REGISTRY **registry, MYFLT *trig, K_DATA *k_data) {
    CSN_REGISTRY *reg = *registry;
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = (uint32_t) src_ref->id;
    int32_t res = OK;

    CHECK_KTRIG(trig);

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot_a = get_slot(reg, source_handle);
    if (slot_a == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
    }

    CSN_ARRAY *source_arr = slot_a->array;
    uint32_t dim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    if (dim != 2) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, perf_h, "[csnarray] Trace needs a 2-D matrix, got %u-D", dim);
    }

    if (CAN_REUSE_ELEMENTWISE(k_data, source_handle, source_arr, 0, NULL, NULL, 0.0, 0.0)) {
        csound->UnlockMutex(reg->mutex);
        return res;
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
            csound->UnlockMutex(reg->mutex);
            return csound->PerfError(csound, perf_h, "[csnarray] Handle holds a complex array; declare the result as :Complex;");
        }
        out_complex->real = (MYFLT) sum.re;
        out_complex->imag = (MYFLT) sum.im;
        out_complex->isPolar = 0;
    } else {
        if (out_value == NULL) {
            csound->UnlockMutex(reg->mutex);
            return csound->PerfError(csound, perf_h, "[csnarray] Handle holds a real array; declare the result as i");
        }
        *out_value = (MYFLT) sum.re;
    }
    PUBLISH_ELEMENTWISE(k_data, source_handle, source_arr, 0, NULL, NULL, 0.0, 0.0);

    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_trace(CSOUND *csound, CSN_UNARYOP_SCALAR *p) {
    return csnarray_trace_impl(csound, p->source_handle, p->value, NULL, &p->registry);
}

int32_t csnarray_trace_k(CSOUND *csound, CSN_UNARYOP_SCALAR *p) {
    return csnarray_trace_impl_k(csound, &p->h, p->source_handle, p->value, NULL, &p->registry, p->trig, &p->k_data);
}

int32_t csnarray_tracecomp(CSOUND *csound, CSN_UNARYOPCOMPLEX_SCALAR *p) {
    return csnarray_trace_impl(csound, p->source_handle, NULL, p->value, &p->registry);
}

int32_t csnarray_tracecomp_k(CSOUND *csound, CSN_UNARYOPCOMPLEX_SCALAR *p) {
    return csnarray_trace_impl_k(csound, &p->h, p->source_handle, NULL, p->value, &p->registry, p->trig, &p->k_data);
}

static int32_t diag_body(CSOUND *csound, OPDS *perf_h, CSN_REGISTRY *reg, CSN_ARRAY **source_array, uint32_t source_handle) {
    CSN_SLOT *slot_a = get_slot(reg, source_handle);
    if (slot_a == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
    }

    *source_array = slot_a->array;
    CSN_ARRAY *source_arr = *source_array;
    uint32_t dim = source_arr->ndim;

    if (dim == 0 || dim > 2) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Diag needs a 1-D or 2-D array, got %u-D", dim);
    }

    return OK;
}

/* new_dim is the rank of the destination, the opposite of the source's: 1 means
   the diagonal of a matrix is being read out, 2 means a vector is being spread
   over the diagonal of a fresh matrix. */
static void diag_assign_value(CSN_ARRAY *source_arr, CSN_ARRAY *arr, uint32_t new_dim, uint32_t *new_shape) {
    uint32_t coords[2] = { 0, 0 };
    for (uint32_t i = 0; i < new_shape[0]; i++) {
        coords[0] = i;
        coords[1] = i;
        if (new_dim == 1) {
            size_t off = from_coords_to_offset(coords, source_arr->strides, 2U);
            item_set(arr, i, item_at(source_arr, off));
        } else {
            size_t off = from_coords_to_offset(coords, arr->strides, 2U);
            item_set(arr, off, item_at(source_arr, i));
        }
    }
}


int32_t csnarray_diag(CSOUND *csound, CSN_UNARYOP *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = (uint32_t) p->source_handle->id;
    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    res = diag_body(csound, NULL, reg, &source_arr, source_handle);
    if (res != OK) goto done;

    uint32_t dim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

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
    diag_assign_value(source_arr, arr, new_dim, new_shape);
    SET_KDATA_BEGIN(p, reg);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_diag_k(CSOUND *csound, CSN_UNARYOP *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REG_HANDLE(csound, &p->h, reg, p->k_data.owned_handle);

    uint32_t source_handle = (uint32_t) p->source_handle->id;

    int32_t res = OK;
    const char *err = NULL;
    res = CHECK_SELF_ALIAS(csound, &p->h, &p->k_data, source_handle, 0);
    if (res != OK) return res;

    CHECK_KTRIG(p->trig);

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    res = diag_body(csound, &p->h, reg, &source_arr, source_handle);
    if (res != OK) goto done;

    uint32_t dim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    uint32_t new_dim = dim == 2U ? 1U : 2U;
    if (dim == 1) {
        new_shape[0] = source_shape[0];
        new_shape[1] = source_shape[0];
    } else {
        new_shape[0] = source_shape[0] < source_shape[1] ? source_shape[0] : source_shape[1];
    }

    size_t requested_size = 0;
    if (get_array_size_from_shape(&requested_size, new_dim, new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSN_ARRAY *arr = p->array;
    size_t logical_size = source_arr->size == 0 ? 0 : requested_size;
    CSN_SLOT *reuse_slot = get_slot(p->k_data.registry, p->k_data.owned_handle);
    if (reuse_slot != NULL && CAN_REUSE_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, reuse_slot->array, 0.0, 0.0)) {
        p->handle->id = p->k_data.owned_handle;
        goto done;
    }

    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, new_dim, new_shape, logical_size, source_arr->itype, err);
    if (res != OK) goto done;

    diag_assign_value(source_arr, arr, new_dim, new_shape);
    SET_KDATA_END(p, new_shape, new_dim, source_arr->itype);
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, arr, 0.0, 0.0);

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

static int32_t movstats_body(CSOUND *csound, OPDS *perf_h, CSN_REGISTRY *reg, CSN_ARRAY **source_array, uint32_t source_handle, CSN_MOVSTATS_MODE mode, const MYFLT *in_axis, int32_t *out_axis) {
    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
    }

    *source_array = source_slot->array;
    CSN_ARRAY *source_arr = *source_array;
    ITEM_TYPE itype = source_arr->itype;

    if (itype == CSN_COMPLEX && (mode == CSN_MOVMIN || mode == CSN_MOVMAX || mode == CSN_MOVMEDIAN)) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Ordering is undefined for complex arrays, so this moving statistic is not available");
    }

    uint32_t source_ndim = source_arr->ndim;
    double axis_value = (double) *in_axis;
    if (axis_value != -1.0 && !IS_VALID_AXIS(axis_value, source_ndim)) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: -1 for all axes, or finite integers 0..%u)", axis_value, source_ndim, source_ndim - 1);
    }

    *out_axis = (int32_t) axis_value;
    return OK;
}

static int32_t movstats_assign_value(CSOUND *csound, OPDS *perf_h, CSN_ARRAY *source_arr, CSN_ARRAY *arr, double *median_buffer, size_t winsize, int32_t axis, ITEM_TYPE itype, CSN_MOVSTATS_MODE mode) {
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
                    return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Division by zero in movstd");
                };
                break;
            case CSN_MOVVAR:
                if (movstdvar_slice(arr->data, source_arr->data, source_arr->size, 1, winsize, RED_VAR, itype) != OK) {
                    return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Division by zero in movvar");
                };
                break;
            case CSN_MOVMIN:
                movminmax_slice(arr->data, source_arr->data, source_arr->size, 1, winsize, RED_MIN);
                break;
            case CSN_MOVMAX:
                movminmax_slice(arr->data, source_arr->data, source_arr->size, 1, winsize, RED_MAX);
                break;
        }
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
                    return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Division by zero in movvar");
                };
                break;
            case CSN_MOVVAR:
                if (movstdvar_slice(arr->data + dst_base, source_arr->data + src_base * itype, source_shape[axis], src_stride, winsize, RED_VAR, itype) != OK) {
                    return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Division by zero in movvar");
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

    return OK;
}

static int32_t csnarray_movstats_helper(CSOUND *csound, CSN_MOVSTATS *p, CSN_MOVSTATS_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    const char *err = NULL;

    if (*p->winsize <= 0.0) {
        return csound->InitError(csound, "[csnarray] Invalid window size");
    }

    size_t winsize = (size_t) *p->winsize;

    double *median_buffer = NULL;
    if (mode == CSN_MOVMEDIAN) {
        median_buffer = csound->Calloc(csound, sizeof(double) * winsize);
        if (median_buffer == NULL) {
            return csound->InitError(csound, "[csnarray] Memory allocation failed");
        }
    }

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    int32_t axis = -1;
    res = movstats_body(csound, NULL, reg, &source_arr, source_handle, mode, p->axis, &axis);
    if (res != OK) goto done;

    uint32_t new_dim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
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
    ITEM_TYPE out_itype = (mode == CSN_MOVSTD || mode == CSN_MOVVAR) ? CSN_REAL : source_arr->itype;
    if (create_csnarray_locked(csound, reg, &p->h, new_dim, new_shape, &p->array, p->handle, protect, 1U, &err, out_itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    ITEM_TYPE itype = source_arr->itype;
    CSN_ARRAY *arr = p->array;
    res = movstats_assign_value(csound, NULL, source_arr, arr, median_buffer, winsize,  axis, itype, mode);

done:
    if (median_buffer != NULL) {
        csound->Free(csound, median_buffer);
    }
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_movstats_k_init_helper(CSOUND *csound, CSN_MOVSTATS *p, CSN_MOVSTATS_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    const char *err = NULL;

    /* No window check here: winsize is a k-argument and normally still reads 0
       during the init pass. The perf pass validates it before every use. */
    size_t winsize = (size_t) *p->winsize;

    if (mode == CSN_MOVMEDIAN) {
        size_t cap = winsize > 0 ? winsize * 2 : 1;
        double *median_buffer = csound->Calloc(csound, sizeof(double) * cap);
        if (median_buffer == NULL) {
            return csound->InitError(csound, "[csnarray] Memory allocation failed");
        }
        p->scratch.scratch = median_buffer;
        p->scratch.scratch_capacity = cap;
    }

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    int32_t axis = -1;
    res = movstats_body(csound, NULL, reg, &source_arr, source_handle, mode, p->axis, &axis);
    if (res != OK) goto done;

    const uint32_t protect[1] = { source_handle };
    ITEM_TYPE out_itype = (mode == CSN_MOVSTD || mode == CSN_MOVVAR) ? CSN_REAL : source_arr->itype;
    if (create_csnarray_locked(csound, reg, &p->h, source_arr->ndim, source_arr->shape, &p->array, p->handle, protect, 1U, &err, out_itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    ITEM_TYPE itype = source_arr->itype;
    CSN_ARRAY *arr = p->array;
    /* With the window still unknown the output keeps the source's layout and
       stays zeroed; the first triggered pass fills it. */
    if (winsize > 0) {
        res = movstats_assign_value(csound, NULL, source_arr, arr, p->scratch.scratch, winsize,  axis, itype, mode);
        if (res != OK) goto done;
    }
    SET_KDATA_BEGIN(p, reg);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_movstats_k_helper(CSOUND *csound, CSN_MOVSTATS *p, CSN_MOVSTATS_MODE mode) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REG_HANDLE(csound, &p->h, reg, p->k_data.owned_handle);

    int32_t res = OK;
    const char *err = NULL;

    uint32_t source_handle = p->source_handle->id;
    res = CHECK_SELF_ALIAS(csound, &p->h, &p->k_data, source_handle, 0);
    if (res != OK) return res;

    if (*p->winsize <= 0.0) {
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid window size");
    }

    CHECK_KTRIG(p->trig);
    size_t winsize = (size_t) *p->winsize;

    if (mode == CSN_MOVMEDIAN) {
        if (winsize > p->scratch.scratch_capacity) {
            size_t new_cap = winsize * 2;
            double *median_buffer = csound->ReAlloc(csound, p->scratch.scratch, sizeof(double) * new_cap);
            if (median_buffer == NULL) {
                return csound->PerfError(csound, &p->h, "[csnarray] Memory allocation failed");
            }
            p->scratch.scratch = median_buffer;
            p->scratch.scratch_capacity = new_cap;
        }
    }

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    int32_t axis = -1;
    res = movstats_body(csound, &p->h, reg, &source_arr, source_handle, mode, p->axis, &axis);
    if (res != OK) goto done;

    uint32_t new_dim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    memcpy(new_shape, source_shape, sizeof(uint32_t) * CSN_MAX_DIMS);

    if (axis == -1) {
        if (winsize == 0 || winsize > source_arr->size) {
            csound->UnlockMutex(reg->mutex);
            return csound->PerfError(csound, &p->h, "[csnarray] Invalid window size");
        }

        memset(new_shape, 0, sizeof(new_shape));
        new_shape[0] = (uint32_t) source_arr->size;
        new_dim = 1U;
    } else {
        if (winsize > source_shape[axis]) {
            csound->UnlockMutex(reg->mutex);
            return csound->PerfError(csound, &p->h, "[csnarray] Invalid window size");
        }
    }

    size_t requested_size = 0;
    if (get_array_size_from_shape(&requested_size, new_dim, new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    /* A moving statistic touches every element of every window, so the cost
       scales with the window as well as the array. Nothing to redo when the
       source, the axis and the window have all held still and the result is
       still the one this opcode left behind. */
    if (p->array != NULL
        && axis == (int32_t) p->k_data.prev_axis
        && (double) winsize == p->k_data.prev_scalar_param
        && CAN_REUSE_LAST_RESULT(&p->k_data, source_handle, source_arr, p->array)) {
        p->handle->id = p->k_data.owned_handle;
        goto done;
    }

    ITEM_TYPE itype = source_arr->itype;
    CSN_ARRAY *arr = p->array;

    size_t logical_size = source_arr->size == 0 ? 0 : requested_size;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, new_dim, new_shape, logical_size, itype, err);
    if (res != OK) goto done;

    res = movstats_assign_value(csound, &p->h, source_arr, arr, p->scratch.scratch, winsize,  axis, itype, mode);
    if (res != OK) goto done;
    SET_KDATA_END(p, new_shape, new_dim, itype);

    p->array = arr;
    p->k_data.prev_axis = (uint32_t) axis;
    p->k_data.prev_scalar_param = (double) winsize;
    PUBLISH_DERIVED_RESULT(&p->k_data, source_handle, source_arr, arr);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_movmean(CSOUND *csound, CSN_MOVSTATS *p) {
    return csnarray_movstats_helper(csound, p, CSN_MOVMEAN);
}

static int32_t csnarray_movmean_k_init(CSOUND *csound, CSN_MOVSTATS *p) {
    return csnarray_movstats_k_init_helper(csound, p, CSN_MOVMEAN);
}

int32_t csnarray_movmean_k(CSOUND *csound, CSN_MOVSTATS *p) {
    return csnarray_movstats_k_helper(csound, p, CSN_MOVMEAN);
}

int32_t csnarray_movmedian(CSOUND *csound, CSN_MOVSTATS *p) {
    return csnarray_movstats_helper(csound, p, CSN_MOVMEDIAN);
}

static int32_t csnarray_movmedian_k_init(CSOUND *csound, CSN_MOVSTATS *p) {
    return csnarray_movstats_k_init_helper(csound, p, CSN_MOVMEDIAN);
}

int32_t csnarray_movmedian_k(CSOUND *csound, CSN_MOVSTATS *p) {
    return csnarray_movstats_k_helper(csound, p, CSN_MOVMEDIAN);
}

int32_t csnarray_movstd(CSOUND *csound, CSN_MOVSTATS *p) {
    return csnarray_movstats_helper(csound, p, CSN_MOVSTD);
}

static int32_t csnarray_movstd_k_init(CSOUND *csound, CSN_MOVSTATS *p) {
    return csnarray_movstats_k_init_helper(csound, p, CSN_MOVSTD);
}

int32_t csnarray_movstd_k(CSOUND *csound, CSN_MOVSTATS *p) {
    return csnarray_movstats_k_helper(csound, p, CSN_MOVSTD);
}

int32_t csnarray_movvar(CSOUND *csound, CSN_MOVSTATS *p) {
    return csnarray_movstats_helper(csound, p, CSN_MOVVAR);
}

static int32_t csnarray_movvar_k_init(CSOUND *csound, CSN_MOVSTATS *p) {
    return csnarray_movstats_k_init_helper(csound, p, CSN_MOVVAR);
}

int32_t csnarray_movvar_k(CSOUND *csound, CSN_MOVSTATS *p) {
    return csnarray_movstats_k_helper(csound, p, CSN_MOVVAR);
}

int32_t csnarray_movmin(CSOUND *csound, CSN_MOVSTATS *p) {
    return csnarray_movstats_helper(csound, p, CSN_MOVMIN);
}

static int32_t csnarray_movmin_k_init(CSOUND *csound, CSN_MOVSTATS *p) {
    return csnarray_movstats_k_init_helper(csound, p, CSN_MOVMIN);
}

int32_t csnarray_movmin_k(CSOUND *csound, CSN_MOVSTATS *p) {
    return csnarray_movstats_k_helper(csound, p, CSN_MOVMIN);
}

int32_t csnarray_movmax(CSOUND *csound, CSN_MOVSTATS *p) {
    return csnarray_movstats_helper(csound, p, CSN_MOVMAX);
}

static int32_t csnarray_movmax_k_init(CSOUND *csound, CSN_MOVSTATS *p) {
    return csnarray_movstats_k_init_helper(csound, p, CSN_MOVMAX);
}

int32_t csnarray_movmax_k(CSOUND *csound, CSN_MOVSTATS *p) {
    return csnarray_movstats_k_helper(csound, p, CSN_MOVMAX);
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

static int32_t ensure_movstats_source_copy(CSOUND *csound, OPDS *perf_h, CSN_SCRATCH *scratch, const CSN_ARRAY *source_arr) {
    size_t required = source_arr->size * (size_t) source_arr->itype;
    if (scratch->scratch != NULL && scratch->scratch_capacity >= required) {
        return OK;
    }

    size_t new_capacity = required > 0 ? required * 2 : 1;
    double *src_copy = csound->ReAlloc(csound, scratch->scratch, sizeof(double) * new_capacity);
    if (src_copy == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Memory allocation failed");
    }

    scratch->scratch = src_copy;
    scratch->scratch_capacity = new_capacity;
    return OK;
}

static int32_t movstats_in_body(CSOUND *csound, OPDS *perf_h, CSN_REGISTRY *reg, uint32_t source_handle, CSN_ARRAY **source_array, MYFLT *in_axis, int32_t *out_axis, size_t winsize, CSN_MOVSTATS_MODE mode) {
    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
    }

    *source_array = source_slot->array;
    CSN_ARRAY *source_arr = *source_array;
    ITEM_TYPE itype = source_arr->itype;

    if (itype == CSN_COMPLEX && (mode == CSN_MOVMIN || mode == CSN_MOVMAX || mode == CSN_MOVMEDIAN)) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Ordering is undefined for complex arrays, so this moving statistic is not available");
    }

    if (itype == CSN_COMPLEX && (mode == CSN_MOVSTD || mode == CSN_MOVVAR)) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Moving std and var of a complex array are real, so they cannot be written back in place");
    }
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    double axis_value = (double) *in_axis;
    if (axis_value != -1.0 && !IS_VALID_AXIS(axis_value, source_ndim)) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: -1 for all axes, or finite integers 0..%u)", axis_value, source_ndim, source_ndim - 1);
    }
    int32_t axis = (int32_t) axis_value;

    if (axis == -1) {
        if (winsize == 0 || winsize > source_arr->size) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Invalid window size");
        }
    } else {
        if (winsize == 0 || winsize > source_shape[axis]) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Invalid window size");
        }
    }

    *out_axis = (int32_t) axis;
    return OK;
}

/* src_copy holds the source as it was before this call, laid out exactly like
   source_arr->data so the same strides and offsets address both. Writing back
   into the source is only safe against that copy: every window reaches over
   elements the pass has already replaced. */
static int32_t movstats_in_assign_value(CSOUND *csound, OPDS *perf_h, CSN_ARRAY *source_arr, double *src_copy, double *median_buffer, int32_t axis, size_t winsize, CSN_MOVSTATS_MODE mode) {
    ITEM_TYPE itype = source_arr->itype;
    memcpy(src_copy, source_arr->data, sizeof(double) * source_arr->size * (size_t) itype);

    if (axis == -1) {
        if (dispatch_movstats(source_arr->data, src_copy, source_arr->size, 1, winsize, median_buffer, mode, itype) != OK) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Division by zero");
        };
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
        if (dispatch_movstats(source_arr->data + src_base * itype, src_copy + src_base * itype, source_shape[axis], src_stride, winsize, median_buffer, mode, itype) != OK) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Division by zero");
        };
    }

    return OK;
}

static int32_t csnarray_movstats_in_helper(CSOUND *csound, CSN_MOVSTATS_IN *p, CSN_MOVSTATS_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    CSN_SCRATCH src_scratch = {0};

    uint32_t source_handle = p->source_handle->id;
    size_t winsize = (size_t) *p->winsize;

    double *median_buffer = NULL;
    if (mode == CSN_MOVMEDIAN) {
        median_buffer = csound->Calloc(csound, sizeof(double) * winsize);
        if (median_buffer == NULL) {
            return csound->InitError(csound, "[csnarray] Memory allocation failed");
        }
    }

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    int32_t axis = -1;
    res = movstats_in_body(csound, NULL, reg, source_handle, &source_arr, p->axis, &axis, winsize, mode);
    if (res != OK) goto done;

    res = ensure_movstats_source_copy(csound, NULL, &src_scratch, source_arr);
    if (res != OK) goto done;

    res = movstats_in_assign_value(csound, NULL, source_arr, src_scratch.scratch, median_buffer, axis, winsize, mode);
    if (res == OK) update_array_data_version(&source_arr->version);

done:
    deinit_scratch(csound, &src_scratch);
    if (median_buffer != NULL) {
        csound->Free(csound, median_buffer);
    }
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_movstats_in_k_deinit(CSOUND *csound, CSN_MOVSTATS_IN *p) {
    deinit_scratch(csound, &p->scratch);
    deinit_scratch(csound, &p->src_scratch);
    return OK;
}

static int32_t csnarray_movstats_in_k_init_helper(CSOUND *csound, CSN_MOVSTATS_IN *p, CSN_MOVSTATS_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    p->registry = reg;
    int32_t res = OK;

    uint32_t source_handle = p->source_handle->id;
    size_t winsize = (size_t) *p->winsize;

    p->scratch.scratch = NULL;
    p->scratch.scratch_capacity = 0;
    p->src_scratch.scratch = NULL;
    p->src_scratch.scratch_capacity = 0;
    if (mode == CSN_MOVMEDIAN) {
        /* The window is a k-argument, so it still reads 0 here; the perf pass
           grows the buffer on demand and only needs a seed it can realloc. */
        size_t cap = winsize > 0 ? winsize * 2 : 1;
        double *median_buffer = csound->Calloc(csound, sizeof(double) * cap);
        if (median_buffer == NULL) {
            return csound->InitError(csound, "[csnarray] Memory allocation failed");
        }
        p->scratch.scratch = median_buffer;
        p->scratch.scratch_capacity = cap;
    }

    /* Same reason as the output form: with a k window this check can only run
       once the first performance pass knows the value. */
    if (winsize == 0) {
        return OK;
    }

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    int32_t axis = -1;
    res = movstats_in_body(csound, NULL, reg, source_handle, &source_arr, p->axis, &axis, winsize, mode);
    if (res != OK) goto done;

    res = ensure_movstats_source_copy(csound, NULL, &p->src_scratch, source_arr);

done:
    csound->UnlockMutex(reg->mutex);
    if (res != OK) {
        deinit_scratch(csound, &p->scratch);
        deinit_scratch(csound, &p->src_scratch);
    }
    return res;
}

static int32_t csnarray_movstats_in_k_helper(CSOUND *csound, CSN_MOVSTATS_IN *p, CSN_MOVSTATS_MODE mode) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    int32_t res = OK;

    /* The in-place form rewrites its own source, so an ungated pass is not a
       wasted recomputation: it filters an already filtered array. */
    CHECK_KTRIG(p->trig);

    uint32_t source_handle = p->source_handle->id;
    size_t winsize = (size_t) *p->winsize;

    if (mode == CSN_MOVMEDIAN) {
        if (winsize > p->scratch.scratch_capacity) {
            size_t new_cap = winsize * 2;
            double *median_buffer = csound->ReAlloc(csound, p->scratch.scratch, sizeof(double) * new_cap);
            if (median_buffer == NULL) {
                return csound->PerfError(csound, &p->h, "[csnarray] Memory allocation failed");
            }
            p->scratch.scratch = median_buffer;
            p->scratch.scratch_capacity = new_cap;
        }
    }

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    int32_t axis = -1;
    res = movstats_in_body(csound, &p->h, reg, source_handle, &source_arr, p->axis, &axis, winsize, mode);
    if (res != OK) goto done;

    if (CAN_REUSE_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, NULL, (double) winsize, (double) axis)) {
        goto done;
    }

    res = ensure_movstats_source_copy(csound, &p->h, &p->src_scratch, source_arr);
    if (res != OK) goto done;

    res = movstats_in_assign_value(csound, &p->h, source_arr, p->src_scratch.scratch, p->scratch.scratch, axis, winsize, mode);
    if (res == OK) {
        update_array_data_version(&source_arr->version);
        PUBLISH_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, NULL, (double) winsize, (double) axis);
    }

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_movmean_in(CSOUND *csound, CSN_MOVSTATS_IN *p) {
    return csnarray_movstats_in_helper(csound, p, CSN_MOVMEAN);
}

static int32_t csnarray_movmean_in_k_init(CSOUND *csound, CSN_MOVSTATS_IN *p) {
    return csnarray_movstats_in_k_init_helper(csound, p, CSN_MOVMEAN);
}

int32_t csnarray_movmean_in_k(CSOUND *csound, CSN_MOVSTATS_IN *p) {
    return csnarray_movstats_in_k_helper(csound, p, CSN_MOVMEAN);
}

int32_t csnarray_movmedian_in(CSOUND *csound, CSN_MOVSTATS_IN *p) {
    return csnarray_movstats_in_helper(csound, p, CSN_MOVMEDIAN);
}

static int32_t csnarray_movmedian_in_k_init(CSOUND *csound, CSN_MOVSTATS_IN *p) {
    return csnarray_movstats_in_k_init_helper(csound, p, CSN_MOVMEDIAN);
}

int32_t csnarray_movmedian_in_k(CSOUND *csound, CSN_MOVSTATS_IN *p) {
    return csnarray_movstats_in_k_helper(csound, p, CSN_MOVMEDIAN);
}

int32_t csnarray_movstd_in(CSOUND *csound, CSN_MOVSTATS_IN *p) {
    return csnarray_movstats_in_helper(csound, p, CSN_MOVSTD);
}

static int32_t csnarray_movstd_in_k_init(CSOUND *csound, CSN_MOVSTATS_IN *p) {
    return csnarray_movstats_in_k_init_helper(csound, p, CSN_MOVSTD);
}

int32_t csnarray_movstd_in_k(CSOUND *csound, CSN_MOVSTATS_IN *p) {
    return csnarray_movstats_in_k_helper(csound, p, CSN_MOVSTD);
}

int32_t csnarray_movvar_in(CSOUND *csound, CSN_MOVSTATS_IN *p) {
    return csnarray_movstats_in_helper(csound, p, CSN_MOVVAR);
}

static int32_t csnarray_movvar_in_k_init(CSOUND *csound, CSN_MOVSTATS_IN *p) {
    return csnarray_movstats_in_k_init_helper(csound, p, CSN_MOVVAR);
}

int32_t csnarray_movvar_in_k(CSOUND *csound, CSN_MOVSTATS_IN *p) {
    return csnarray_movstats_in_k_helper(csound, p, CSN_MOVVAR);
}

int32_t csnarray_movmin_in(CSOUND *csound, CSN_MOVSTATS_IN *p) {
    return csnarray_movstats_in_helper(csound, p, CSN_MOVMIN);
}

static int32_t csnarray_movmin_in_k_init(CSOUND *csound, CSN_MOVSTATS_IN *p) {
    return csnarray_movstats_in_k_init_helper(csound, p, CSN_MOVMIN);
}

int32_t csnarray_movmin_in_k(CSOUND *csound, CSN_MOVSTATS_IN *p) {
    return csnarray_movstats_in_k_helper(csound, p, CSN_MOVMIN);
}

int32_t csnarray_movmax_in(CSOUND *csound, CSN_MOVSTATS_IN *p) {
    return csnarray_movstats_in_helper(csound, p, CSN_MOVMAX);
}

static int32_t csnarray_movmax_in_k_init(CSOUND *csound, CSN_MOVSTATS_IN *p) {
    return csnarray_movstats_in_k_init_helper(csound, p, CSN_MOVMAX);
}

int32_t csnarray_movmax_in_k(CSOUND *csound, CSN_MOVSTATS_IN *p) {
    return csnarray_movstats_in_k_helper(csound, p, CSN_MOVMAX);
}

static void complop_unary_assign_value(CSN_ARRAY *source_arr, CSN_ARRAY *arr, CSN_COMPLEXOP_MODE mode) {
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
}

static int32_t csnarray_complop_unary_helper(CSOUND *csound, CSN_UNARYOP *p, CSN_COMPLEXOP_MODE mode) {
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
    complop_unary_assign_value(source_arr, arr, mode);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_complop_unary_k_init_helper(CSOUND *csound, CSN_UNARYOP *p, CSN_COMPLEXOP_MODE mode) {
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

    const uint32_t protect[1] = { source_handle };
    if (create_csnarray_locked(csound, reg, &p->h, source_ndim, source_shape, &p->array, p->handle, protect, 1U, &err, source_arr->itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;
    if (source_arr->size > 0) {
        memcpy(arr->data, source_arr->data, sizeof(double) * source_arr->size);
        arr->size = source_arr->size;
    }

    SET_KDATA_BEGIN(p, reg);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_complop_unary_k_helper(CSOUND *csound, CSN_UNARYOP *p, CSN_COMPLEXOP_MODE mode) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REG_HANDLE(csound, &p->h, reg, p->k_data.owned_handle);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    const char *err = NULL;
    /* csnconj writes each cell from itself and stays complex. csnreal, csnimag
       and csntocomplex all change the element type, which reallocates the
       destination and drops the source the fill is about to read. */
    if (mode != CSN_CONJ_PART) {
        res = CHECK_SELF_ALIAS(csound, &p->h, &p->k_data, source_handle, 0);
        if (res != OK) return res;
    }

    CHECK_KTRIG(p->trig);

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
    }

    CSN_ARRAY *source_arr = source_slot->array;
    ITEM_TYPE itype = source_arr->itype;

    if (mode == CSN_REAL_TO_COMPLEX) {
        if (itype != CSN_REAL) {
            csound->UnlockMutex(reg->mutex);
            return csound->PerfError(csound, &p->h, "[csnarray] Real-to-complex operation requires real array");
        }
    } else if (itype != CSN_COMPLEX) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Operation requires complex array");
    }

    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    uint32_t new_dim = source_ndim;
    memcpy(new_shape, source_shape, sizeof(uint32_t) * CSN_MAX_DIMS);

    ITEM_TYPE out_itype = (mode == CSN_CONJ_PART || mode == CSN_REAL_TO_COMPLEX) ? CSN_COMPLEX : CSN_REAL;

    size_t req_size = 0;
    if (get_array_size_from_shape(&req_size, new_dim, new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSN_ARRAY *arr = NULL;
    size_t logical_size = source_arr->size == 0 ? 0 : req_size;
    /* out_itype, not the source's: csnreal and csnimag hand back a real array
       from a complex one, csnconj and csntocomplex the other way round. */
    if (mode == CSN_CONJ_PART) {
        res = CHECK_SELF_ALIAS_CELL_LOCAL(csound, &p->h, &p->k_data, source_handle, source_arr, new_dim, new_shape, out_itype);
        if (res != OK) goto done;
    }

    CSN_SLOT *reuse_slot = get_slot(reg, p->k_data.owned_handle);
    if (reuse_slot != NULL && CAN_REUSE_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, reuse_slot->array, 0.0, 0.0)) {
        p->handle->id = p->k_data.owned_handle;
        goto done;
    }

    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, new_dim, new_shape, logical_size, out_itype, err);
    if (res != OK) goto done;
    p->array = arr;

    complop_unary_assign_value(source_arr, arr, mode);
    SET_KDATA_END(p, new_shape, new_dim, out_itype);
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, arr, 0.0, 0.0);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_real(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_complop_unary_helper(csound, p, CSN_REAL_PART);
}

static int32_t csnarray_real_k_init(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_complop_unary_k_init_helper(csound, p, CSN_REAL_PART);
}

int32_t csnarray_real_k(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_complop_unary_k_helper(csound, p, CSN_REAL_PART);
}

int32_t csnarray_imag(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_complop_unary_helper(csound, p, CSN_IMAG_PART);
}

static int32_t csnarray_imag_k_init(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_complop_unary_k_init_helper(csound, p, CSN_IMAG_PART);
}

int32_t csnarray_imag_k(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_complop_unary_k_helper(csound, p, CSN_IMAG_PART);
}

int32_t csnarray_complex_to_real(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_complop_unary_helper(csound, p, CSN_REAL_PART);
}

static int32_t csnarray_complex_to_real_k_init(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_complop_unary_k_init_helper(csound, p, CSN_REAL_PART);
}

int32_t csnarray_complex_to_real_k(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_complop_unary_k_helper(csound, p, CSN_REAL_PART);
}

int32_t csnarray_real_to_complex(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_complop_unary_helper(csound, p, CSN_REAL_TO_COMPLEX);
}

static int32_t csnarray_real_to_complex_k_init(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_complop_unary_k_init_helper(csound, p, CSN_REAL_TO_COMPLEX);
}

int32_t csnarray_real_to_complex_k(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_complop_unary_k_helper(csound, p, CSN_REAL_TO_COMPLEX);
}

int32_t csnarray_conj(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_complop_unary_helper(csound, p, CSN_CONJ_PART);
}

static int32_t csnarray_conj_k_init(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_complop_unary_k_init_helper(csound, p, CSN_CONJ_PART);
}

int32_t csnarray_conj_k(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_complop_unary_k_helper(csound, p, CSN_CONJ_PART);
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

static int32_t angle_body(CSOUND *csound, OPDS *perf_h, CSN_REGISTRY *reg, uint32_t source_handle, CSN_ARRAY **source_array, double in_period, double in_discount, double in_axis, double *out_period, double *out_discount, int32_t *out_axis,  CSN_COMPLEXOP_MODE mode) {
    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
    }

    *source_array = source_slot->array;
    CSN_ARRAY *source_arr = *source_array;
    ITEM_TYPE itype = source_arr->itype;

    if (mode == CSN_COMPLEX_TO_ANGLE) {
        if (itype != CSN_COMPLEX) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Complex to angle requires complex array");
        }
    } else if (itype != CSN_REAL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] wrap/unwrap angle operations requires real array");
    }

    double axis_value = -1.0;
    switch (mode) {
        case CSN_WRAP:
            *out_period = in_period;
            break;
        case CSN_UNWRAP:
            *out_period = in_period;
            *out_discount = in_discount;
            if (*out_discount <= 0.0) *out_discount = *out_period * 0.5;
            axis_value = in_axis;
            break;
        default:
            break;
    }

    uint32_t source_ndim = source_arr->ndim;

    if (axis_value != -1.0 && !IS_VALID_AXIS(axis_value, source_ndim)) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: -1 for all axes, or finite integers 0..%u)", axis_value, source_ndim, source_ndim - 1);
    }

    *out_axis = (int32_t) axis_value;
    return OK;
}

static int32_t angle_assign_value(CSOUND *csound, OPDS *perf_h, CSN_ARRAY *source_arr, CSN_ARRAY *arr, double period, double discount, int32_t axis, CSN_COMPLEXOP_MODE mode) {
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
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unwrap needs at least 2 elements, got %zu", source_arr->size);
        }

        if (axis == -1) {
            unwrap_slice(arr->data, source_arr->data, source_arr->size, 1, period, discount);
        } else {
            uint32_t reduced_shape[CSN_MAX_DIMS] = {0};
            uint32_t reduced_ndim = 0;
            size_t slice_count = 1;
            for (uint32_t i = 0; i < source_arr->ndim; ++i) {
                if (i != (uint32_t) axis) {
                    reduced_shape[reduced_ndim++] = source_arr->shape[i];
                    slice_count *= source_arr->shape[i];
                }
            }

            size_t src_stride = source_arr->strides[axis];
            for (size_t linear = 0; linear < slice_count; ++linear) {
                uint32_t dst_coords[CSN_MAX_DIMS] = {0};
                uint32_t src_coords[CSN_MAX_DIMS] = {0};

                from_linear_to_coords(dst_coords, reduced_shape, linear, reduced_ndim);
                for (uint32_t i = 0, j = 0; i < source_arr->ndim; ++i) {
                    src_coords[i] = (i == (uint32_t) axis) ? 0 : dst_coords[j++];
                }

                size_t src_base = from_coords_to_offset(src_coords, source_arr->strides, source_arr->ndim);
                size_t dst_base = from_coords_to_offset(src_coords, arr->strides, source_arr->ndim);
                unwrap_slice(arr->data + dst_base, source_arr->data + src_base, source_arr->shape[axis], src_stride, period, discount);
            }
        }
    }

    return OK;
}

static int32_t csnarray_angle_helper(CSOUND *csound, CSN_ANGLE *p, CSN_COMPLEXOP_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    double in_period = (mode == CSN_WRAP || mode == CSN_UNWRAP) ? (double) *p->arg_a : 0.0;
    double in_discount = mode == CSN_UNWRAP ? (double) *p->arg_b : 0.0;
    double in_axis = mode == CSN_UNWRAP ? (double) *p->arg_c : -1.0;
    double period = 0.0;
    double discount = 0.0;
    int32_t axis = -1;
    res = angle_body(csound, NULL, reg, source_handle, &source_arr, in_period, in_discount, in_axis, &period, &discount, &axis, mode);
    if (res != OK) goto done;

    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    uint32_t new_dim = source_ndim;
    memcpy(new_shape, source_shape, sizeof(uint32_t) * CSN_MAX_DIMS);

    const uint32_t protect[1] = { source_handle };
    if (create_csnarray_locked(csound, reg, &p->h, new_dim, new_shape, &p->array, p->handle, protect, 1U, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;
    res = angle_assign_value(csound, NULL, source_arr, arr, period, discount, axis, mode);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_angle_k_init_helper(CSOUND *csound, CSN_ANGLE *p, CSN_COMPLEXOP_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    double in_period = (mode == CSN_WRAP || mode == CSN_UNWRAP) ? (double) *p->arg_a : 0.0;
    double in_discount = mode == CSN_UNWRAP ? (double) *p->arg_b : 0.0;
    double in_axis = mode == CSN_UNWRAP ? (double) *p->arg_c : -1.0;
    double period = 0.0;
    double discount = 0.0;
    int32_t axis = -1;
    res = angle_body(csound, NULL, reg, source_handle, &source_arr, in_period, in_discount, in_axis, &period, &discount, &axis, mode);
    if (res != OK) goto done;

    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    const uint32_t protect[1] = { source_handle };
    if (create_csnarray_locked(csound, reg, &p->h, source_ndim, source_shape, &p->array, p->handle, protect, 1U, &err, CSN_REAL) != OK) {
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

static int32_t csnarray_angle_k_helper(CSOUND *csound, CSN_ANGLE *p, CSN_COMPLEXOP_MODE mode) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REG_HANDLE(csound, &p->h, reg, p->k_data.owned_handle);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    const char *err = NULL;
    /* Wrapping folds each angle on its own; csnunwrap walks an axis and reads
       the neighbour it has just written, and csnangle turns complex into real. */
    if (mode != CSN_WRAP) {
        res = CHECK_SELF_ALIAS(csound, &p->h, &p->k_data, source_handle, 0);
        if (res != OK) return res;
    }

    MYFLT *trig = (mode == CSN_WRAP || mode == CSN_UNWRAP) ? ((mode == CSN_WRAP) ? p->arg_b : p->arg_d) : p->arg_a;
    CHECK_KTRIG(trig);

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    double in_period = (mode == CSN_WRAP || mode == CSN_UNWRAP) ? (double) *p->arg_a : 0.0;
    double in_discount = mode == CSN_UNWRAP ? (double) *p->arg_b : 0.0;
    double in_axis = mode == CSN_UNWRAP ? (double) *p->arg_c : -1.0;
    double period = 0.0;
    double discount = 0.0;
    int32_t axis = -1;
    res = angle_body(csound, &p->h, reg, source_handle, &source_arr, in_period, in_discount, in_axis, &period, &discount, &axis, mode);
    if (res != OK) goto done;

    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;
    ITEM_TYPE itype = source_arr->itype;

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    uint32_t new_dim = source_ndim;
    memcpy(new_shape, source_shape, sizeof(uint32_t) * CSN_MAX_DIMS);

    size_t req_size = 0;
    if (get_array_size_from_shape(&req_size, new_dim, new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSN_ARRAY *arr = p->array;
    size_t logical_size = source_arr->size == 0 ? 0 : req_size;
    if (mode == CSN_WRAP) {
        res = CHECK_SELF_ALIAS_CELL_LOCAL(csound, &p->h, &p->k_data, source_handle, source_arr, new_dim, new_shape, itype);
        if (res != OK) goto done;
    }

    CSN_SLOT *out_slot = get_slot(reg, p->k_data.owned_handle);
    if (out_slot != NULL && CAN_REUSE_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, out_slot->array, period, discount) && (int32_t) p->k_data.prev_axis == axis) {
        p->handle->id = p->k_data.owned_handle;
        goto done;
    }

    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, new_dim, new_shape, logical_size, itype, err);
    if (res != OK) goto done;

    res = angle_assign_value(csound, &p->h, source_arr, arr, period, discount, axis, mode);
    if (res != OK) goto done;

    SET_KDATA_END(p, new_shape, new_dim, itype);
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, arr, period, discount);
    p->k_data.prev_axis = (uint32_t) axis;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t angle_in_assign_value(CSOUND *csound, OPDS *perf_h, CSN_ARRAY *source_arr, double period, double discount, int32_t axis, CSN_COMPLEXOP_MODE mode) {
    if (mode == CSN_WRAP) {
        for (size_t i = 0; i < source_arr->size; i ++) {
            double angle_temp = source_arr->data[i];
            double angle = wrap_angle(angle_temp, period);
            source_arr->data[i] = angle;
        }
    } else if (mode == CSN_UNWRAP) {
        if (source_arr->size < 2) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unwrap needs at least 2 elements, got %zu", source_arr->size);
        }

        if (axis == -1) {
            unwrap_slice(source_arr->data, source_arr->data, source_arr->size, 1, period, discount);
        } else {
            uint32_t reduced_shape[CSN_MAX_DIMS] = {0};
            uint32_t reduced_ndim = 0;
            size_t slice_count = 1;
            for (uint32_t i = 0; i < source_arr->ndim; ++i) {
                if (i != (uint32_t) axis) {
                    reduced_shape[reduced_ndim++] = source_arr->shape[i];
                    slice_count *= source_arr->shape[i];
                }
            }

            size_t src_stride = source_arr->strides[axis];
            for (size_t linear = 0; linear < slice_count; ++linear) {
                uint32_t dst_coords[CSN_MAX_DIMS] = {0};
                uint32_t src_coords[CSN_MAX_DIMS] = {0};

                from_linear_to_coords(dst_coords, reduced_shape, linear, reduced_ndim);
                for (uint32_t i = 0, j = 0; i < source_arr->ndim; ++i) {
                    src_coords[i] = (i == (uint32_t) axis) ? 0 : dst_coords[j++];
                }

                size_t src_base = from_coords_to_offset(src_coords, source_arr->strides, source_arr->ndim);
                size_t dst_base = from_coords_to_offset(src_coords, source_arr->strides, source_arr->ndim);
                unwrap_slice(source_arr->data + dst_base, source_arr->data + src_base, source_arr->shape[axis], src_stride, period, discount);
            }
        }
    }

    return OK;
}

static int32_t csnarray_angle_in_helper(CSOUND *csound, CSN_ANGLE_IN *p, CSN_COMPLEXOP_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    p->registry = reg;
    uint32_t source_handle = p->source_handle->id;
    int32_t res = OK;

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    double in_period = (mode == CSN_WRAP || mode == CSN_UNWRAP) ? (double) *p->arg_a : 0.0;
    double in_discount = mode == CSN_UNWRAP ? (double) *p->arg_b : 0.0;
    double in_axis = mode == CSN_UNWRAP ? (double) *p->arg_c : -1.0;
    double period = 0.0;
    double discount = 0.0;
    int32_t axis = -1;
    res = angle_body(csound, &p->h, reg, source_handle, &source_arr, in_period, in_discount, in_axis, &period, &discount, &axis, mode);
    if (res != OK) goto done;


    res = angle_in_assign_value(csound, &p->h, source_arr, period, discount, axis, mode);
    if (res != OK) goto done;
    update_array_data_version(&source_arr->version);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_angle_in_k_helper(CSOUND *csound, CSN_ANGLE_IN *p, CSN_COMPLEXOP_MODE mode) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    p->registry = reg;
    uint32_t source_handle = p->source_handle->id;
    int32_t res = OK;

    MYFLT *trig = (mode == CSN_WRAP || mode == CSN_UNWRAP) ? ((mode == CSN_WRAP) ? p->arg_b : p->arg_d) : p->arg_a;
    CHECK_KTRIG(trig);

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    double in_period = (mode == CSN_WRAP || mode == CSN_UNWRAP) ? (double) *p->arg_a : 0.0;
    double in_discount = mode == CSN_UNWRAP ? (double) *p->arg_b : 0.0;
    double in_axis = mode == CSN_UNWRAP ? (double) *p->arg_c : -1.0;
    double period = 0.0;
    double discount = 0.0;
    int32_t axis = -1;
    res = angle_body(csound, &p->h, reg, source_handle, &source_arr, in_period, in_discount, in_axis, &period, &discount, &axis, mode);
    if (res != OK) goto done;


    if (CAN_REUSE_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, NULL, period, discount)
        && (int32_t) p->k_data.prev_axis == axis) {
        goto done;
    }

    res = angle_in_assign_value(csound, &p->h, source_arr, period, discount, axis, mode);
    if (res != OK) goto done;
    update_array_data_version(&source_arr->version);
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, NULL, period, discount);
    p->k_data.prev_axis = (uint32_t) axis;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}


int32_t csnarray_angle(CSOUND *csound, CSN_ANGLE *p) {
    return csnarray_angle_helper(csound, p, CSN_COMPLEX_TO_ANGLE);
}

static int32_t csnarray_angle_k_init(CSOUND *csound, CSN_ANGLE *p) {
    return csnarray_angle_k_init_helper(csound, p, CSN_COMPLEX_TO_ANGLE);
}

int32_t csnarray_angle_k(CSOUND *csound, CSN_ANGLE *p) {
    return csnarray_angle_k_helper(csound, p, CSN_COMPLEX_TO_ANGLE);
}

int32_t csnarray_wrap_angle(CSOUND *csound, CSN_ANGLE *p) {
    return csnarray_angle_helper(csound, p, CSN_WRAP);
}

static int32_t csnarray_wrap_angle_k_init(CSOUND *csound, CSN_ANGLE *p) {
    return csnarray_angle_k_init_helper(csound, p, CSN_WRAP);
}

int32_t csnarray_wrap_angle_k(CSOUND *csound, CSN_ANGLE *p) {
    return csnarray_angle_k_helper(csound, p, CSN_WRAP);
}

int32_t csnarray_unwrap_angle(CSOUND *csound, CSN_ANGLE *p) {
    return csnarray_angle_helper(csound, p, CSN_UNWRAP);
}

static int32_t csnarray_unwrap_angle_k_init(CSOUND *csound, CSN_ANGLE *p) {
    return csnarray_angle_k_init_helper(csound, p, CSN_UNWRAP);
}

int32_t csnarray_unwrap_angle_k(CSOUND *csound, CSN_ANGLE *p) {
    return csnarray_angle_k_helper(csound, p, CSN_UNWRAP);
}

int32_t csnarray_wrap_angle_in(CSOUND *csound, CSN_ANGLE_IN *p) {
    return csnarray_angle_in_helper(csound, p, CSN_WRAP);
}

int32_t csnarray_wrap_angle_in_k(CSOUND *csound, CSN_ANGLE_IN *p) {
    return csnarray_angle_in_k_helper(csound, p, CSN_WRAP);
}

int32_t csnarray_unwrap_angle_in(CSOUND *csound, CSN_ANGLE_IN *p) {
    return csnarray_angle_in_helper(csound, p, CSN_UNWRAP);
}

int32_t csnarray_unwrap_angle_in_k(CSOUND *csound, CSN_ANGLE_IN *p) {
    return csnarray_angle_in_k_helper(csound, p, CSN_UNWRAP);
}

static int32_t csnarray_type_helper(CSOUND *csound, OPDS *perf_h, CSN_UNARYOP_SCALAR *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, perf_h, reg);

    p->registry = reg;
    uint32_t source_handle = p->source_handle->id;
    int32_t res = OK;

    if (perf_h != NULL) {
        CHECK_KTRIG(p->trig);
    }

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        res = CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = source_slot->array;
    ITEM_TYPE itype = source_arr->itype;
    *p->value = itype == CSN_REAL ? FL(0.0) : FL(1.0);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_type(CSOUND *csound, CSN_UNARYOP_SCALAR *p) {
    return csnarray_type_helper(csound, NULL, p);
}

int32_t csnarray_type_k(CSOUND *csound, CSN_UNARYOP_SCALAR *p) {
    return csnarray_type_helper(csound, &p->h, p);
}

/* Shared by the i-rate form, the k init and the k perf: a straight copy, or the
   same copy read back to front. A complex item moves as a pair. */
static void copy_assign_value(const CSN_ARRAY *source_arr, CSN_ARRAY *arr, bool reverse, ITEM_TYPE itype) {
    if (!reverse) {
        memcpy(arr->data, source_arr->data, sizeof(double) * source_arr->size * (size_t) itype);
        return;
    }

    for (size_t i = 0; i < source_arr->size; i++) {
        size_t src = (source_arr->size - 1U - i) * (size_t) itype;
        size_t dst = i * (size_t) itype;
        arr->data[dst] = source_arr->data[src];
        if (itype == CSN_COMPLEX)
            arr->data[dst + 1U] = source_arr->data[src + 1U];
    }
}

static int32_t csnarray_copy_helper(CSOUND *csound, CSN_UNARYOP *p, bool reverse) {
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
    copy_assign_value(source_arr, arr, reverse, itype);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_copy_k_init_helper(CSOUND *csound, CSN_UNARYOP *p, bool reverse) {
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
    ITEM_TYPE itype = source_arr->itype;
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    const uint32_t protect[1] = { source_handle };
    if (create_csnarray_locked(csound, reg, &p->h, source_ndim, source_shape, &p->array, p->handle, protect, 1U, &err, itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    if (source_arr->size > 0) {
        /* reverse and itype matter here too: a gated csnreverse must already
           read back to front, and a complex item is two doubles wide. */
        copy_assign_value(source_arr, p->array, reverse, itype);
        p->array->size = source_arr->size;
    }

    SET_KDATA_BEGIN(p, reg);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_copy_k_helper(CSOUND *csound, CSN_UNARYOP *p, bool reverse) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REG_HANDLE(csound, &p->h, reg, p->k_data.owned_handle);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    const char *err = NULL;

    /* csncopy leaves every value in its own cell, so a handle may copy onto
       itself; csnreverse reads the cell at the far end of the array and must
       still be kept apart from its own output. The layout test is applied once
       the source is resolved, below. */
    if (reverse) {
        res = CHECK_SELF_ALIAS(csound, &p->h, &p->k_data, source_handle, 0);
        if (res != OK) return res;
    }

    CHECK_KTRIG(p->trig);

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
    }

    CSN_ARRAY *source_arr = source_slot->array;
    ITEM_TYPE itype = source_arr->itype;
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    uint32_t new_dim = source_ndim;
    memcpy(new_shape, source_shape, sizeof(uint32_t) * CSN_MAX_DIMS);

    size_t req_size = 0;
    if (get_array_size_from_shape(&req_size, new_dim, new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSN_ARRAY *arr = p->array;
    size_t logical_size = source_arr->size == 0 ? 0 : req_size;
    if (!reverse) {
        res = CHECK_SELF_ALIAS_CELL_LOCAL(csound, &p->h, &p->k_data, source_handle, source_arr, new_dim, new_shape, itype);
        if (res != OK) goto done;
    }

    CSN_SLOT *reuse_slot = get_slot(reg, p->k_data.owned_handle);
    if (reuse_slot != NULL && CAN_REUSE_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, reuse_slot->array, 0.0, 0.0)) {
        p->handle->id = p->k_data.owned_handle;
        goto done;
    }

    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, new_dim, new_shape, logical_size, itype, err);
    if (res != OK) goto done;

    if (arr->data != source_arr->data) {
        copy_assign_value(source_arr, arr, reverse, itype);
    }
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, arr, 0.0, 0.0);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_copy(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_copy_helper(csound, p, false);
}

static int32_t csnarray_copy_k_init(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_copy_k_init_helper(csound, p, false);
}

int32_t csnarray_copy_k(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_copy_k_helper(csound, p, false);
}

int32_t csnarray_reverse(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_copy_helper(csound, p, true);
}

int32_t csnarray_reverse_k_init(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_copy_k_init_helper(csound, p, true);
}

int32_t csnarray_reverse_k(CSOUND *csound, CSN_UNARYOP *p) {
    return csnarray_copy_k_helper(csound, p, true);
}

static int32_t csnarray_unaryop_in_helper(CSOUND *csound, CSN_UNARYOP_IN *p, CSN_UNARY_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    p->registry = reg;
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

    if (itype == CSN_COMPLEX && (mode == CSN_DEG2RAD || mode == CSN_RAD2DEG)) {
        res = csound->InitError(csound, "[csnarray] Angle conversion requires real arrays");
        goto done;
    }

    switch (mode) {
        case CSN_REVERSE:
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
            break;
        case CSN_RAD2DEG:
        case CSN_DEG2RAD:
            for (size_t i = 0; i < source_arr->size; i++) {
                double value = source_arr->data[i];
                double angle = mode == CSN_DEG2RAD ? value * M_PI / 180.0 : value * 180.0 / M_PI;
                source_arr->data[i] = angle;
            }
            break;
        default:
            break;
    }
    if (source_arr->size >= 1U) update_array_data_version(&source_arr->version);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_reverse_in(CSOUND *csound, CSN_UNARYOP_IN *p) {
    return csnarray_unaryop_in_helper(csound, p, CSN_REVERSE);
}

int32_t csnarray_degtorad_in(CSOUND *csound, CSN_UNARYOP_IN *p) {
    return csnarray_unaryop_in_helper(csound, p, CSN_DEG2RAD);
}

int32_t csnarray_radtodeg_in(CSOUND *csound, CSN_UNARYOP_IN *p) {
    return csnarray_unaryop_in_helper(csound, p, CSN_RAD2DEG);
}

int32_t csnarray_unaryop_in_k_helper(CSOUND *csound, CSN_UNARYOP_IN *p, CSN_UNARY_MODE mode) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    uint32_t source_handle = p->source_handle->id;
    int32_t res = OK;

    CHECK_KTRIG(p->trig);

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
    }

    CSN_ARRAY *source_arr = source_slot->array;
    ITEM_TYPE itype = source_arr->itype;

    if (itype == CSN_COMPLEX && (mode == CSN_DEG2RAD || mode == CSN_RAD2DEG)) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Angle conversion requires real arrays");
    }

    /* Not idempotent: reversing twice undoes it and a second conversion scales
       again, so a pass must run exactly once per write by someone else. */
    if (CAN_REUSE_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, NULL, 0.0, 0.0)) {
        csound->UnlockMutex(reg->mutex);
        return res;
    }

    switch (mode) {
        case CSN_REVERSE:
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
            break;
        case CSN_RAD2DEG:
        case CSN_DEG2RAD:
            for (size_t i = 0; i < source_arr->size; i++) {
                double value = source_arr->data[i];
                double angle = mode == CSN_DEG2RAD ? value * M_PI / 180.0 : value * 180.0 / M_PI;
                source_arr->data[i] = angle;
            }
            break;
        default:
            break;
    }
    if (source_arr->size >= 1U) update_array_data_version(&source_arr->version);
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, NULL, 0.0, 0.0);

    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_unaryop_in_k_init(CSOUND *csound, CSN_UNARYOP_IN *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, &p->h, reg);

    p->registry = reg;
    return OK;
}

int32_t csnarray_reverse_in_k(CSOUND *csound, CSN_UNARYOP_IN *p) {
    return csnarray_unaryop_in_k_helper(csound, p, CSN_REVERSE);
}

int32_t csnarray_degtorad_in_k(CSOUND *csound, CSN_UNARYOP_IN *p) {
    return csnarray_unaryop_in_k_helper(csound, p, CSN_DEG2RAD);
}

int32_t csnarray_radtodeg_in_k(CSOUND *csound, CSN_UNARYOP_IN *p) {
    return csnarray_unaryop_in_k_helper(csound, p, CSN_RAD2DEG);
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
    CHECK_REGISTRY(csound, NULL, reg);

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
        res = csound->InitError(csound, "[csnarray] Percentile and quantile reductions requires real arrays");
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

static int32_t csnarray_perquant_s_k_deinit(CSOUND *csound, CSN_PERCQUANT *p) {
    if (p->scratch.scratch != NULL) {
        csound->Free(csound, p->scratch.scratch);
    }

    return OK;
}

static int32_t csnarray_perquant_k_init(CSOUND *csound, CSN_PERCQUANT_AX *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;

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

    if (source_arr->itype == CSN_COMPLEX) {
        res = csound->InitError(csound, "[csnarray] Percentile and quantile reductions require real arrays");
        goto done;
    }

    const uint32_t protect[1] = { source_handle };

    if (create_csnarray_locked(csound, reg, &p->h, source_ndim, source_shape, &p->array, p->handle, protect, 1U, &err, source_arr->itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    size_t source_size = source_arr->size;
    if (source_arr->size > 0) {
        memcpy(p->array->data, source_arr->data, source_size);
        p->array->size = source_size;
    }

    size_t cap = source_size > 0 ? source_size * 2 : 1;
    buffer = csound->Calloc(csound, sizeof(double) * cap);
    if (buffer == NULL) {
        res = csound->InitError(csound, "Memory allocation failed");
        goto done;
    }

    p->scratch.scratch = buffer;
    p->scratch.scratch_capacity = cap;

    SET_KDATA_BEGIN(p, reg);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_perquant_scalar_k_init(CSOUND *csound, CSN_PERCQUANT *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    p->registry = reg;

    size_t init_capacity = DEFAULT_TEMPORARY_BUFFER_SIZE;
    double *buffer = csound->Calloc(csound, sizeof(double) * init_capacity);
    if (buffer == NULL) {
        return csound->InitError(csound, "[csnarray] Internal memory allocation failed");
    }

    p->scratch.scratch = buffer;
    p->scratch.scratch_capacity = init_capacity;

    return OK;
}

/* The scalar forms own no output slot, so they carry a bare registry instead of
   a K_DATA: everything that depends on a slot is guarded on k_data. */
static int32_t csnarray_perquant_k_reduction(CSOUND *csound, OPDS *h, CSNREF *src_ref, double axis_value, CSNREF *out_handle, CSN_ARRAY **out_array, MYFLT *out_value, bool is_percentile, double q, K_DATA *k_data, CSN_REGISTRY *registry, const MYFLT *trig, CSN_SCRATCH *buffer_ref) {
    /* The scratch lives in the caller's opcode struct; these keep the buffer
       and its capacity moving together. */
    void **buffer = &buffer_ref->scratch;
    size_t *buffer_capacity = &buffer_ref->scratch_capacity;
    CSN_REGISTRY *reg = k_data != NULL ? k_data->registry : registry;
    CHECK_REGISTRY(csound, h, reg);

    if (k_data != NULL) {
        CHECK_REG_HANDLE(csound, h, reg, k_data->owned_handle);
    }

    if (is_percentile) {
        if (q < 0.0 || q > 100) {
            return csound->PerfError(csound, h, "[csnarray] Percentile must be in the range [0, 100]");
        }
    } else {
        if (q < 0.0 || q > 1.0) {
            return csound->PerfError(csound, h, "[csnarray] Quantile must be in the range [0, 1]");
        }
    }

    uint32_t source_handle = src_ref->id;

    int32_t res = OK;
    const char *err = NULL;

    if (k_data != NULL) {
        res = CHECK_SELF_ALIAS(csound, h, k_data, source_handle, 0);
        if (res != OK) return res;
    }

    CHECK_KTRIG(trig);

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
    }

    CSN_ARRAY *source_arr = source_slot->array;
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    if (source_arr->size == 0U) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, h, "[csnarray] Percentile and quantile are undefined for an empty array");
    }

    if (out_handle != NULL && !IS_VALID_AXIS(axis_value, source_ndim)) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, h, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: finite integers 0..%u)", axis_value, source_ndim, source_ndim - 1);
    }
    int32_t axis = (int32_t) axis_value;

    if (source_arr->itype == CSN_COMPLEX) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, h, "[csnarray] Percentile and quantile reductions require real arrays");
    }

    if (axis != -1 && source_ndim == 1U) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, h, "[csnarray] Reducing a 1-D array produces a scalar; omit the axis argument");
    }

    /* Both forms sort a run per output element, so a pass that would produce
       the same numbers is worth skipping. Only the axis form carries a K_DATA
       to remember anything in; the scalar form has no slot of its own and
       recomputes. */
    if (k_data != NULL && axis != -1 && *out_array != NULL
        && axis == (int32_t) k_data->prev_axis
        && q == k_data->prev_scalar_param
        && CAN_REUSE_LAST_RESULT(k_data, source_handle, source_arr, *out_array)) {
        goto done;
    }

    CSN_ARRAY *arr = NULL;
    if (axis != -1) {
        uint32_t new_ndim = source_ndim - 1;
        uint32_t new_shape[CSN_MAX_DIMS] = {0};
        for (uint32_t i = 0, j = 0; i < source_ndim; i++) {
            if (i != (uint32_t) axis) new_shape[j++] = source_shape[i];
        }

        size_t req_size = 0;
        if (get_array_size_from_shape(&req_size, new_ndim, new_shape) != OK) {
            csound->UnlockMutex(reg->mutex);
            return csound->PerfError(csound, h, "[csnarray] Invalid shape or element count exceeds the configured limit");
        }

        size_t logical_size = source_arr->size == 0 ? 0 : req_size;
        res = NEED_TO_UPDATE_SLOT(csound, h, out_array, k_data, NULL, new_ndim, new_shape, logical_size, CSN_REAL, err);
        if (res != OK) goto done;

        arr = *out_array;
    }

    if (arr != NULL) {
        size_t r_size = (size_t) source_shape[axis];
        if (r_size > *buffer_capacity) {
            size_t new_cap = r_size > 0 ? r_size * 2 : 1;
            double *temp = csound->ReAlloc(csound, *buffer, sizeof(double) * new_cap);
            if (temp == NULL) {
                csound->UnlockMutex(reg->mutex);
                return csound->PerfError(csound, h, "[csnarray] Memory allocation failed");
            }
            *buffer = temp;
            *buffer_capacity = new_cap;
        }

        for (size_t linear = 0; linear < arr->size; ++linear) {
            uint32_t dst_coords[CSN_MAX_DIMS] = {0};
            uint32_t src_coords[CSN_MAX_DIMS] = {0};

            from_linear_to_coords(dst_coords, arr->shape, linear, arr->ndim);
            double value = 0.0;
            accumulate_perquant_reduction_axis_helper(&value, q, *buffer, source_arr, src_coords, dst_coords, is_percentile, (uint32_t) axis);
            arr->data[linear] = value;
        }

        if (k_data != NULL) {
            k_data->prev_axis = (uint32_t) axis;
            k_data->prev_scalar_param = q;
            PUBLISH_DERIVED_RESULT(k_data, source_handle, source_arr, arr);
        }
    } else {
        size_t r_size = (size_t) source_arr->size;
        if (r_size > *buffer_capacity) {
            size_t new_cap = r_size > 0 ? r_size * 2 : 1;
            double *temp = csound->ReAlloc(csound, *buffer, sizeof(double) * new_cap);
            if (temp == NULL) {
                csound->UnlockMutex(reg->mutex);
                return csound->PerfError(csound, h, "[csnarray] Memory allocation failed");
            }
            *buffer = temp;
            *buffer_capacity = new_cap;
        }

        double value = 0;
        accumulate_perquant_reduction_scalar_helper(&value, q, *buffer, source_arr, is_percentile);
        *out_value = (MYFLT) value;
    }

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_percentile(CSOUND *csound, CSN_PERCQUANT_AX *p) {
    return csnarray_perquant_reduction(csound, &p->h, p->source_handle, (double) *p->axis, p->handle, &p->array, NULL, true, (double) *p->quantity);
}

int32_t csnarray_percentile_k(CSOUND *csound, CSN_PERCQUANT_AX *p) {
    return csnarray_perquant_k_reduction(csound, &p->h, p->source_handle, (double) *p->axis, p->handle, &p->array, NULL, true, (double) *p->quantity, &p->k_data, NULL, p->trig, &p->scratch);
}

int32_t csnarray_percentile_scalar(CSOUND *csound, CSN_PERCQUANT *p) {
    return csnarray_perquant_reduction(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, true, (double) *p->quantity);
}

int32_t csnarray_percentile_scalar_k(CSOUND *csound, CSN_PERCQUANT *p) {
    return csnarray_perquant_k_reduction(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, true, (double) *p->quantity, NULL, p->registry, p->trig, &p->scratch);
}

int32_t csnarray_quantile(CSOUND *csound, CSN_PERCQUANT_AX *p) {
    return csnarray_perquant_reduction(csound, &p->h, p->source_handle, (double) *p->axis, p->handle, &p->array, NULL, false, (double) *p->quantity);
}

int32_t csnarray_quantile_k(CSOUND *csound, CSN_PERCQUANT_AX *p) {
    return csnarray_perquant_k_reduction(csound, &p->h, p->source_handle, (double) *p->axis, p->handle, &p->array, NULL, false, (double) *p->quantity, &p->k_data, NULL, p->trig, &p->scratch);
}

int32_t csnarray_quantile_scalar(CSOUND *csound, CSN_PERCQUANT *p) {
    return csnarray_perquant_reduction(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, false, (double) *p->quantity);
}

int32_t csnarray_quantile_scalar_k(CSOUND *csound, CSN_PERCQUANT *p) {
    return csnarray_perquant_k_reduction(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, false, (double) *p->quantity, NULL, p->registry, p->trig, &p->scratch);
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

static double bessel_i0(double beta) {
    double ax = fabs(beta);

    if (ax < 3.75) {
        double y = beta / 3.75;
        y *= y;
        return 1.0 +
               y * (3.5156229 +
               y * (3.0899424 +
               y * (1.2067492 +
               y * (0.2659732 +
               y * (0.0360768 +
               y * 0.0045813)))));
    }
    else {
        double y = 3.75 / ax;
        return (exp(ax) / sqrt(ax)) *
               (0.39894228 +
               y * (0.01328592 +
               y * (0.00225319 +
               y * (-0.00157565 +
               y * (0.00916281 +
               y * (-0.02057706 +
               y * (0.02635537 +
               y * (-0.01647633 +
               y * 0.00392377))))))));
    }
}

static int32_t window_function_helper(CSOUND *csound, CSN_WINDOW *p, CSN_WINDOW_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    if (!IS_VALID_LENGTH((double) *p->length)) {
        return csound->InitError(csound, "[csnarray] Invalid window length");
    }
    uint32_t wsize = (uint32_t) *p->length;

    double beta = -1.0;
    if (mode == W_KAISER) {
        beta = (double) *p->beta;
        if (!IS_VALID_VALUE(beta) || beta < 0.0) {
            return csound->InitError(csound, "[csnarray] Invalid beta param in kaiser window");
        }
    }

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);
    uint32_t shape[CSN_MAX_DIMS] = {0};
    shape[0] = wsize;

    if (create_csnarray_locked(csound, reg, &p->h, 1U, shape, &p->array, p->handle, NULL, 0, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;
    if (wsize == 1U) {
        arr->data[0] = 1.0;
    } else if (wsize > 1U) {
        for (size_t n = 0; n < (size_t) wsize; n++) {
            double value = 0.0;
            double fac = (double) n / (double) (wsize - 1);
            switch (mode) {
                case W_HANNING:
                    value = 0.5 * (1.0 - cos(2.0 * M_PI * fac));
                    break;
                case W_HAMMING:
                    value = 0.54 - 0.46 * cos(2.0 * M_PI * fac);
                    break;
                case W_BARTLETT:
                    /* The peak is where fac reaches 0.5, not where n reaches
                       wsize/2: for an even wsize the integer midpoint sits past
                       half of the 0..1 sweep and the rising branch would take
                       it above 1. */
                    value = (fac <= 0.5) ? 2.0 * fac : 2.0 - 2.0 * fac;
                    break;
                case W_BLACKMAN:
                    value = 0.42 - 0.5 * cos(2.0 * M_PI * fac) + 0.08 * cos(4.0 * M_PI * fac);
                    break;
                case W_KAISER: {
                        double denom = bessel_i0(beta);
                        double x = (2.0 * fac) - 1.0;
                        double arg = beta * sqrt(1.0 - x * x);
                        value = bessel_i0(arg) / denom;
                    }
                    break;
            }
            arr->data[n] = value;
        }
    }

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_hanning(CSOUND *csound, CSN_WINDOW *p) {
    return window_function_helper(csound, p, W_HANNING);
}

int32_t csnarray_hamming(CSOUND *csound, CSN_WINDOW *p) {
    return window_function_helper(csound, p, W_HAMMING);
}

int32_t csnarray_bartlett(CSOUND *csound, CSN_WINDOW *p) {
    return window_function_helper(csound, p, W_BARTLETT);
}

int32_t csnarray_blackman(CSOUND *csound, CSN_WINDOW *p) {
    return window_function_helper(csound, p, W_BLACKMAN);
}

int32_t csnarray_kaiser(CSOUND *csound, CSN_WINDOW *p) {
    return window_function_helper(csound, p, W_KAISER);
}

static int32_t csnarray_window_function_k_init(CSOUND *csound, CSN_WINDOW *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t wsize = 1;
    p->is_i_time_length = false;
    if (is_inarg_i_time(&p->h, 0)) {
        if (!IS_VALID_LENGTH((double) *p->length)) {
            return csound->InitError(csound, "[csnarray] Invalid window length");
        }
        wsize = (uint32_t) *p->length;
        p->is_i_time_length = true;
    }

    int32_t res = OK;
    uint32_t shape[CSN_MAX_DIMS] = {0};
    shape[0] = wsize;
    res = create_csnarray_init(csound, &p->h, 1U, shape, &p->array, p->handle, CSN_REAL);
    if (res != OK) return res;

    csound->LockMutex(reg->mutex);
    reset_empty_csnarray(p->array, 1U, shape, CSN_REAL);
    csound->UnlockMutex(reg->mutex);

    SET_KDATA_WITH_ID_BEGIN(p, reg, shape, 1U, CSN_REAL, p->handle->id);
    p->prev_length = p->is_i_time_length ? (int32_t) wsize : -1;
    p->prev_beta = -1.0;
    p->is_published = false;
    return res;
}

static int32_t window_function_k_helper(CSOUND *csound, CSN_WINDOW *p, CSN_WINDOW_MODE mode) {
    CSN_REGISTRY *reg = p->k_data.registry;
    uint32_t source_handle = p->k_data.owned_handle;
    CHECK_REG_HANDLE(csound, &p->h, reg, source_handle);

    uint32_t wsize = p->is_i_time_length ? (uint32_t) *p->length : 0;
    if (!p->is_i_time_length) {
        if (!IS_VALID_LENGTH((double) *p->length)) {
            return csound->PerfError(csound, &p->h, "[csnarray] Invalid window length");
        }
        wsize = (uint32_t) *p->length;
    }

    double beta = -1.0;
    if (mode == W_KAISER) {
        beta = (double) *p->beta;
        if (!IS_VALID_VALUE(beta) || beta < 0.0) {
            return csound->PerfError(csound, &p->h, "[csnarray] Invalid beta param in kaiser window");
        }
    }

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
    }

    bool is_same_size = (int32_t) wsize == p->prev_length && p->is_published;
    bool is_same = mode == W_KAISER ? (is_same_size && (beta == p->prev_beta)) : is_same_size;
    if (is_same) goto done;

    uint32_t shape[CSN_MAX_DIMS] = {0};
    shape[0] = wsize;

    size_t req_size = 0;
    if (get_array_size_from_shape(&req_size, 1U, shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSN_ARRAY *arr = NULL;
    size_t logical_size = wsize == 0 ? 0 : req_size;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, 1U, shape, logical_size, CSN_REAL, err);
    if (res != OK) goto done;
    p->array = arr;

    if (wsize == 1U) {
        arr->data[0] = 1.0;
    } else if (wsize > 1U) {
        for (size_t n = 0; n < (size_t) wsize; n++) {
            double value = 0.0;
            double fac = (double) n / (double) (wsize - 1);
            switch (mode) {
                case W_HANNING:
                    value = 0.5 * (1.0 - cos(2.0 * M_PI * fac));
                    break;
                case W_HAMMING:
                    value = 0.54 - 0.46 * cos(2.0 * M_PI * fac);
                    break;
                case W_BARTLETT:
                    /* The peak is where fac reaches 0.5, not where n reaches
                       wsize/2: for an even wsize the integer midpoint sits past
                       half of the 0..1 sweep and the rising branch would take
                       it above 1. */
                    value = (fac <= 0.5) ? 2.0 * fac : 2.0 - 2.0 * fac;
                    break;
                case W_BLACKMAN:
                    value = 0.42 - 0.5 * cos(2.0 * M_PI * fac) + 0.08 * cos(4.0 * M_PI * fac);
                    break;
                case W_KAISER: {
                        double denom = bessel_i0(beta);
                        double x = (2.0 * fac) - 1.0;
                        double arg = beta * sqrt(1.0 - x * x);
                        value = bessel_i0(arg) / denom;
                    }
                    break;
            }
            arr->data[n] = value;
        }
    }

    SET_KDATA_END(p, shape, 1U, CSN_REAL);
    p->prev_length = (int32_t) wsize;
    p->prev_beta = beta;
    p->is_published = true;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_hanning_k(CSOUND *csound, CSN_WINDOW *p) {
    return window_function_k_helper(csound, p, W_HANNING);
}

int32_t csnarray_hamming_k(CSOUND *csound, CSN_WINDOW *p) {
    return window_function_k_helper(csound, p, W_HAMMING);
}

int32_t csnarray_bartlett_k(CSOUND *csound, CSN_WINDOW *p) {
    return window_function_k_helper(csound, p, W_BARTLETT);
}

int32_t csnarray_blackman_k(CSOUND *csound, CSN_WINDOW *p) {
    return window_function_k_helper(csound, p, W_BLACKMAN);
}

int32_t csnarray_kaiser_k(CSOUND *csound, CSN_WINDOW *p) {
    return window_function_k_helper(csound, p, W_KAISER);
}

static int32_t csnarray_divmod_hh_helper(CSOUND *csound, CSN_DIVMOD_HH *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle_a = (uint32_t) p->source_handle_a->id;
    uint32_t source_handle_b = (uint32_t) p->source_handle_b->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot_a = get_slot(reg, source_handle_a);
    if (slot_a == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle_a);
        goto done;
    }

    CSN_SLOT *slot_b = get_slot(reg, source_handle_b);
    if (slot_b == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle_b);
        goto done;
    }

    CSN_ARRAY *source_arr_a = slot_a->array;
    CSN_ARRAY *source_arr_b = slot_b->array;
    if (source_arr_a->itype == CSN_COMPLEX || source_arr_b->itype == CSN_COMPLEX) {
        res = csound->InitError(csound, "[csnarray] Divmod operation requires real array");
        goto done;
    }

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    uint32_t new_ndim = 0;

    if (broadcast_shape(source_arr_a, source_arr_b, new_shape, &new_ndim) != OK) {
        char abuf[CSN_SHAPE_STR_MAX], bbuf[CSN_SHAPE_STR_MAX];
        res = csound->InitError(csound, "[csnarray] Shapes %s and %s cannot be broadcast together: aligned from the last axis, each pair must match or be 1", shape_str(abuf, sizeof(abuf), source_arr_a->shape, source_arr_a->ndim), shape_str(bbuf, sizeof(bbuf), source_arr_b->shape, source_arr_b->ndim));
        goto done;
    }

    const uint32_t protect[2] = { source_handle_a, source_handle_b };
    if (create_csnarray_locked(csound, reg, &p->h, new_ndim, new_shape, &p->array_a, p->handle_a, protect, 2U, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    if (create_csnarray_locked(csound, reg, &p->h, new_ndim, new_shape, &p->array_b, p->handle_b, protect, 2U, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *q_arr = p->array_a;
    CSN_ARRAY *r_arr = p->array_b;
    size_t size = q_arr->size;

    bool same_shape = source_arr_a->ndim == source_arr_b->ndim
        && memcmp(source_arr_a->shape, source_arr_b->shape, sizeof(uint32_t) * source_arr_a->ndim) == 0;

    for (size_t i = 0; i < size; i++) {
        size_t off_a = i;
        size_t off_b = i;

        if (!same_shape) {
            uint32_t dst_coords[CSN_MAX_DIMS] = {0};
            from_linear_to_coords(dst_coords, new_shape, i, new_ndim);
            off_a = broadcast_offset(source_arr_a, dst_coords, new_ndim);
            off_b = broadcast_offset(source_arr_b, dst_coords, new_ndim);
        }

        double a = source_arr_a->data[off_a];
        double b = source_arr_b->data[off_b];
        if (b == 0.0) {
            res = csound->InitError(csound, "[csnarray] Division by zero");
            goto done;
        }

        double q = floor(a / b);
        double r = a - q * b;

        q_arr->data[i] = q;
        r_arr->data[i] = r;
    }

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_divmod_hs_sh_helper(CSOUND *csound, OPDS *h, CSNREF *handle_a, CSNREF *handle_b, CSNREF *source_handle, CSN_ARRAY **p_array_a, CSN_ARRAY **p_array_b, const MYFLT *scalar, bool is_left) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t shandle = (uint32_t) source_handle->id;

    double scalar_value = (double) *scalar;
    if (!IS_VALID_VALUE(scalar_value)) {
        return csound->InitError(csound, "[csnarray] Invalid scalar value");
    }

    if (scalar_value == 0.0) {
        return csound->InitError(csound, "[csnarray] Division by zero");
    }

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, shandle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", shandle);
        goto done;
    }

    CSN_ARRAY *source_arr = slot->array;
    if (source_arr->itype == CSN_COMPLEX) {
        res = csound->InitError(csound, "[csnarray] Divmod operation requires real array");
        goto done;
    }

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    uint32_t new_ndim = source_arr->ndim;
    memcpy(new_shape, source_arr->shape, sizeof(new_shape));

    const uint32_t protect[1] = { shandle };
    if (create_csnarray_locked(csound, reg, h, new_ndim, new_shape, p_array_a, handle_a, protect, 1U, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    if (create_csnarray_locked(csound, reg, h, new_ndim, new_shape, p_array_b, handle_b, protect, 1U, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *q_arr = *p_array_a;
    CSN_ARRAY *r_arr = *p_array_b;
    size_t size = q_arr->size;

    for (size_t i = 0; i < size; i++) {
        double a = source_arr->data[i];

        double num = is_left ? scalar_value : a;
        double den = is_left ? a : scalar_value;
        if (den == 0.0) {
            return csound->InitError(csound, "[csnarray] Division by zero");
        }
        double q = floor(num / den);
        double r = num - q * den;

        q_arr->data[i] = q;
        r_arr->data[i] = r;
    }

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_divmod_hh(CSOUND *csound, CSN_DIVMOD_HH *p) {
    return csnarray_divmod_hh_helper(csound, p);
}

int32_t csnarray_divmod_hs(CSOUND *csound, CSN_DIVMOD_HS *p) {
    return csnarray_divmod_hs_sh_helper(csound, &p->h, p->handle_a, p->handle_b, p->source_handle, &p->array_a, &p->array_b, p->scalar, false);
}

int32_t csnarray_divmod_sh(CSOUND *csound, CSN_DIVMOD_SH *p) {
    return csnarray_divmod_hs_sh_helper(csound, &p->h, p->handle_a, p->handle_b, p->source_handle, &p->array_a, &p->array_b, p->scalar, true);
}

static int32_t csnarray_divmod_hh_k_init(CSOUND *csound, CSN_DIVMOD_HH *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle_a = (uint32_t) p->source_handle_a->id;
    uint32_t source_handle_b = (uint32_t) p->source_handle_b->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot_a = get_slot(reg, source_handle_a);
    if (slot_a == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle_a);
        goto done;
    }

    CSN_SLOT *slot_b = get_slot(reg, source_handle_b);
    if (slot_b == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle_b);
        goto done;
    }

    CSN_ARRAY *source_arr_a = slot_a->array;
    CSN_ARRAY *source_arr_b = slot_b->array;
    if (source_arr_a->itype == CSN_COMPLEX || source_arr_b->itype == CSN_COMPLEX) {
        res = csound->InitError(csound, "[csnarray] Divmod operation requires real array");
        goto done;
    }

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    uint32_t new_ndim = 0;

    if (broadcast_shape(source_arr_a, source_arr_b, new_shape, &new_ndim) != OK) {
        char abuf[CSN_SHAPE_STR_MAX], bbuf[CSN_SHAPE_STR_MAX];
        res = csound->InitError(csound, "[csnarray] Shapes %s and %s cannot be broadcast together: aligned from the last axis, each pair must match or be 1", shape_str(abuf, sizeof(abuf), source_arr_a->shape, source_arr_a->ndim), shape_str(bbuf, sizeof(bbuf), source_arr_b->shape, source_arr_b->ndim));
        goto done;
    }

    const uint32_t protect[2] = { source_handle_a, source_handle_b };
    if (create_csnarray_locked(csound, reg, &p->h, new_ndim, new_shape, &p->array_a, p->handle_a, protect, 2U, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    if (create_csnarray_locked(csound, reg, &p->h, new_ndim, new_shape, &p->array_b, p->handle_b, protect, 2U, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    reset_empty_csnarray(p->array_a, new_ndim, new_shape, CSN_REAL);
    reset_empty_csnarray(p->array_b, new_ndim, new_shape, CSN_REAL);

    memset(p->k_data.prev_shape, 0, sizeof(uint32_t) * CSN_MAX_DIMS);
    memcpy(p->k_data.prev_shape, new_shape, sizeof(uint32_t) * CSN_MAX_DIMS);
    p->k_data.prev_ndim = new_ndim;
    p->k_data.prev_divmod_state.owned_handle_q = (uint32_t) p->handle_a->id;
    p->k_data.prev_divmod_state.owned_handle_r = (uint32_t) p->handle_b->id;
    p->k_data.registry = reg;
    p->is_published = false;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_divmod_hs_sh_k_init_helper(CSOUND *csound, OPDS *h, CSNREF *handle_a, CSNREF *handle_b, CSN_ARRAY **p_array_a, CSN_ARRAY **p_array_b, CSNREF *source_handle, K_DATA *k_data, bool *is_published) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t shandle = (uint32_t) source_handle->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, shandle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", shandle);
        goto done;
    }

    CSN_ARRAY *source_arr = slot->array;
    if (source_arr->itype == CSN_COMPLEX) {
        res = csound->InitError(csound, "[csnarray] Divmod operation requires real array");
        goto done;
    }

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    uint32_t new_ndim = source_arr->ndim;
    memcpy(new_shape, source_arr->shape, sizeof(new_shape));

    const uint32_t protect[1] = { shandle };
    if (create_csnarray_locked(csound, reg, h, new_ndim, new_shape, p_array_a, handle_a, protect, 1U, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    if (create_csnarray_locked(csound, reg, h, new_ndim, new_shape, p_array_b, handle_b, protect, 1U, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    reset_empty_csnarray(*p_array_a, new_ndim, new_shape, CSN_REAL);
    reset_empty_csnarray(*p_array_b, new_ndim, new_shape, CSN_REAL);

    memset(k_data->prev_shape, 0, sizeof(uint32_t) * CSN_MAX_DIMS);
    memcpy(k_data->prev_shape, new_shape, sizeof(uint32_t) * CSN_MAX_DIMS);
    k_data->prev_ndim = new_ndim;
    k_data->prev_divmod_state.owned_handle_q = handle_a->id;
    k_data->prev_divmod_state.owned_handle_r = handle_b->id;
    k_data->registry = reg;
    *is_published = false;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_divmod_hh_k_helper(CSOUND *csound, CSN_DIVMOD_HH *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    uint32_t handle_a = p->k_data.prev_divmod_state.owned_handle_q;
    uint32_t handle_b = p->k_data.prev_divmod_state.owned_handle_r;
    if (reg == NULL || handle_a == INVALID_HANDLE || handle_b == INVALID_HANDLE) {
        return csound->PerfError(csound, &p->h, "[csnarray] k-rate output slot was not initialized");
    }

    uint32_t source_handle_a = (uint32_t) p->source_handle_a->id;
    uint32_t source_handle_b = (uint32_t) p->source_handle_b->id;

    uint32_t owned_handle_a = p->k_data.prev_divmod_state.owned_handle_q;
    uint32_t owned_handle_b = p->k_data.prev_divmod_state.owned_handle_r;

    CHECK_KTRIG(p->trig);

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot_a = get_slot(reg, source_handle_a);
    if (slot_a == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle_a);
    }

    CSN_SLOT *slot_b = get_slot(reg, source_handle_b);
    if (slot_b == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle_b);
    }

    CSN_ARRAY *source_arr_a = slot_a->array;
    CSN_ARRAY *source_arr_b = slot_b->array;
    if (source_arr_a->itype == CSN_COMPLEX || source_arr_b->itype == CSN_COMPLEX) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Divmod operation requires real array");
    }

    bool is_same_a = is_same_array_version(&source_arr_a->version, &p->k_data.prev_divmod_state.prev_source_a_version);
    bool is_same_b = is_same_array_version(&source_arr_b->version, &p->k_data.prev_divmod_state.prev_source_b_version);

    if (is_same_a && is_same_b && p->is_published) goto done;

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    uint32_t new_ndim = 0;

    if (broadcast_shape(source_arr_a, source_arr_b, new_shape, &new_ndim) != OK) {
        char abuf[CSN_SHAPE_STR_MAX], bbuf[CSN_SHAPE_STR_MAX];
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Shapes %s and %s cannot be broadcast together: aligned from the last axis, each pair must match or be 1", shape_str(abuf, sizeof(abuf), source_arr_a->shape, source_arr_a->ndim), shape_str(bbuf, sizeof(bbuf), source_arr_b->shape, source_arr_b->ndim));
    }

    size_t req_size = 0;
    if (get_array_size_from_shape(&req_size, new_ndim, new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSN_ARRAY *q_arr = NULL;
    size_t logical_size = source_arr_a->size == 0 ? 0 : req_size;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &q_arr, &p->k_data, &owned_handle_a, new_ndim, new_shape, logical_size, CSN_REAL, err);
    if (res != OK) goto done;
    p->array_a = q_arr;

    CSN_ARRAY *r_arr = NULL;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &r_arr, &p->k_data, &owned_handle_b, new_ndim, new_shape, logical_size, CSN_REAL, err);
    if (res != OK) goto done;
    p->array_b = r_arr;

    size_t size = q_arr->size;

    bool same_shape = source_arr_a->ndim == source_arr_b->ndim
        && memcmp(source_arr_a->shape, source_arr_b->shape, sizeof(uint32_t) * source_arr_a->ndim) == 0;

    for (size_t i = 0; i < size; i++) {
        size_t off_a = i;
        size_t off_b = i;

        if (!same_shape) {
            uint32_t dst_coords[CSN_MAX_DIMS] = {0};
            from_linear_to_coords(dst_coords, new_shape, i, new_ndim);
            off_a = broadcast_offset(source_arr_a, dst_coords, new_ndim);
            off_b = broadcast_offset(source_arr_b, dst_coords, new_ndim);
        }

        double a = source_arr_a->data[off_a];
        double b = source_arr_b->data[off_b];
        if (b == 0.0) {
            csound->UnlockMutex(reg->mutex);
            return csound->PerfError(csound, &p->h, "[csnarray] Division by zero");
        }

        double q = floor(a / b);
        double r = a - q * b;

        q_arr->data[i] = q;
        r_arr->data[i] = r;
    }

    memset(p->k_data.prev_shape, 0, sizeof(uint32_t) * CSN_MAX_DIMS);
    memcpy(p->k_data.prev_shape, new_shape, sizeof(uint32_t) * CSN_MAX_DIMS);
    p->k_data.prev_ndim = new_ndim;
    p->handle_a->id = p->k_data.prev_divmod_state.owned_handle_q;
    p->handle_b->id = p->k_data.prev_divmod_state.owned_handle_r;
    p->is_published = true;
    set_array_version(&p->k_data.prev_divmod_state.prev_source_a_version, &source_arr_a->version);
    set_array_version(&p->k_data.prev_divmod_state.prev_source_b_version, &source_arr_b->version);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_divmod_hs_sh_k_helper(CSOUND *csound, OPDS *h, CSNREF *handle_a, CSNREF *handle_b, CSN_ARRAY **p_array_a, CSN_ARRAY **p_array_b, CSNREF *source_handle, const MYFLT *scalar, K_DATA *k_data, bool is_left, bool *is_published) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, h, reg);

    uint32_t shandle = (uint32_t) source_handle->id;
    uint32_t owned_handle_a = k_data->prev_divmod_state.owned_handle_q;
    uint32_t owned_handle_b = k_data->prev_divmod_state.owned_handle_r;

    double scalar_value = (double) *scalar;
    if (!IS_VALID_VALUE(scalar_value)) {
        return csound->PerfError(csound, h, "[csnarray] Invalid scalar value");
    }

    if (scalar_value == 0.0) {
        return csound->PerfError(csound, h, "[csnarray] Division by zero");
    }

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, shandle);
    if (slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", shandle);
    }

    CSN_ARRAY *source_arr = slot->array;
    if (source_arr->itype == CSN_COMPLEX) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, h, "[csnarray] Divmod operation requires real array");
    }

    bool is_same_version = is_same_array_version(&source_arr->version, &k_data->prev_divmod_state.prev_source_a_version);
    if (is_same_version && scalar_value == k_data->prev_scalar_param && *is_published) goto done;

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    uint32_t new_ndim = source_arr->ndim;
    memcpy(new_shape, source_arr->shape, sizeof(new_shape));

    size_t req_size = 0;
    if (get_array_size_from_shape(&req_size, new_ndim, new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSN_ARRAY *q_arr = NULL;
    size_t logical_size = source_arr->size == 0 ? 0 : req_size;
    res = NEED_TO_UPDATE_SLOT(csound, h, &q_arr, k_data, &owned_handle_a, new_ndim, new_shape, logical_size, CSN_REAL, err);
    if (res != OK) goto done;
    *p_array_a = q_arr;

    CSN_ARRAY *r_arr = NULL;
    res = NEED_TO_UPDATE_SLOT(csound, h, &r_arr, k_data, &owned_handle_b, new_ndim, new_shape, logical_size, CSN_REAL, err);
    if (res != OK) goto done;
    *p_array_b = r_arr;

    size_t size = q_arr->size;

    for (size_t i = 0; i < size; i++) {
        double a = source_arr->data[i];

        double num = is_left ? scalar_value : a;
        double den = is_left ? a : scalar_value;
        if (den == 0.0) {
            return csound->InitError(csound, "[csnarray] Division by zero");
        }
        double q = floor(num / den);
        double r = num - q * den;

        q_arr->data[i] = q;
        r_arr->data[i] = r;
    }

    memset(k_data->prev_shape, 0, sizeof(uint32_t) * CSN_MAX_DIMS);
    memcpy(k_data->prev_shape, new_shape, sizeof(uint32_t) * CSN_MAX_DIMS);
    k_data->prev_ndim = new_ndim;
    handle_a->id = k_data->prev_divmod_state.owned_handle_q;
    handle_b->id = k_data->prev_divmod_state.owned_handle_r;
    set_array_version(&k_data->prev_divmod_state.prev_source_a_version, &source_arr->version);
    k_data->prev_scalar_param = scalar_value;
    *is_published = true;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_divmod_hh_k(CSOUND *csound, CSN_DIVMOD_HH *p) {
    return csnarray_divmod_hh_k_helper(csound, p);
}

static int32_t csnarray_divmod_hs_k_init(CSOUND *csound, CSN_DIVMOD_HS *p) {
    return csnarray_divmod_hs_sh_k_init_helper(csound, &p->h, p->handle_a, p->handle_b, &p->array_a, &p->array_b, p->source_handle, &p->k_data, &p->is_published);
}

int32_t csnarray_divmod_hs_k(CSOUND *csound, CSN_DIVMOD_HS *p) {
    return csnarray_divmod_hs_sh_k_helper(csound, &p->h, p->handle_a, p->handle_b, &p->array_a, &p->array_b, p->source_handle, p->scalar, &p->k_data, false, &p->is_published);
}

int32_t csnarray_divmod_sh_k_init(CSOUND *csound, CSN_DIVMOD_SH *p) {
    return csnarray_divmod_hs_sh_k_init_helper(csound, &p->h, p->handle_a, p->handle_b, &p->array_a, &p->array_b, p->source_handle, &p->k_data, &p->is_published);
}

int32_t csnarray_divmod_sh_k(CSOUND *csound, CSN_DIVMOD_SH *p) {
    return csnarray_divmod_hs_sh_k_helper(csound, &p->h, p->handle_a, p->handle_b, &p->array_a, &p->array_b, p->source_handle, p->scalar, &p->k_data, true, &p->is_published);
}

int32_t from_ftable_to_csnarray(CSOUND *csound, CSN_FROM_FTABLE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    /* FTFind names the offending table number itself and counts the init
       error, so reporting again here would delete the note with two errors
       for one mistake. */
    FUNC *ftable = csound->FTFind(csound, p->ftable);
    if (ftable == NULL) {
        return NOTOK;
    }

    /* flen counts the data; the guard point past it is not part of the table. */
    uint32_t size = ftable->flen;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);
    uint32_t shape[CSN_MAX_DIMS] = {0};
    shape[0] = size;

    if (create_csnarray_locked(csound, reg, &p->h, 1U, shape, &p->array, p->handle, NULL, 0, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;
    for (uint32_t i = 0; i < size; i++) {
        arr->data[i] = (double) ftable->ftable[i];
    }

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t from_csnarray_to_ftable(CSOUND *csound, CSN_TO_FTABLE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = (uint32_t) p->source_handle->id;

    if (!IS_VALID_ZERO_ONE((double) *p->resize)) {
        return csound->InitError(csound, "[csnarray] Resize param must be 0 = do not resize or 1 = resize");
    }

    uint32_t resize = (uint32_t) *p->resize;

    /* FTFind maps both 0 and -1 to the global sine table, so an unchecked
       number here would silently overwrite the sine every oscillator reads. */
    int32_t fno = (int32_t) MYFLT2LRND(*p->ftable);
    if (fno <= 0) {
        return csound->InitError(csound, "[csnarray] Invalid ftable number %d: it must be greater than 0", fno);
    }

    FUNC *ftable = NULL;
    int32_t res = OK;

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
        goto done;
    }
    CSN_ARRAY *source_arr = slot->array;
    if (source_arr->itype == CSN_COMPLEX) {
        res = csound->InitError(csound, "[csnarray] csntoftable requires real array");
        goto done;
    }

    size_t size = source_arr->size;
    if (size == 0) {
        res = csound->InitError(csound, "[csnarray] Array %u is empty: there is nothing to write into ftable %d", source_handle, fno);
        goto done;
    }
    if (size > (size_t) INT32_MAX) {
        res = csound->InitError(csound, "[csnarray] Array %u has %zu points: too many for an ftable", source_handle, size);
        goto done;
    }

    if (resize == 0) {
        ftable = csound->FTFind(csound, p->ftable);
        if (ftable == NULL) {
            res = NOTOK;
            goto done;
        }

        if ((size_t) ftable->flen < size) {
            res = csound->InitError(csound, "[csnarray] Ftable %d holds %u points but the array has %zu: the ftable length must be equal or greater", fno, (uint32_t) ftable->flen, size);
            goto done;
        }
    } else {
        if (csound->FTAlloc(csound, fno, (int32_t) size) != 0) {
            res = csound->InitError(csound, "[csnarray] Cannot size ftable %d to %zu points", fno, size);
            goto done;
        }

        ftable = csound->FTFind(csound, p->ftable);
        if (ftable == NULL) {
            res = NOTOK;
            goto done;
        }
        ftable->gen01args.sample_rate = ftable->sr;
    }

    for (size_t i = 0; i < size; i++) {
        ftable->ftable[i] = (MYFLT) source_arr->data[i];
    }

    /* The guard point past flen is allocated but never initialised. Write it
       only when the array reaches the end of the table, wrapping to the first
       point the way the cyclic gens do; a shorter array leaves the tail, and
       the guard that belongs to it, untouched. */
    if ((size_t) ftable->flen == size) {
        ftable->ftable[size] = ftable->ftable[0];
    }

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

/*
 * Largest i in [0, data_size - 2] with xdata[i] <= x.
 *
 * Branchless: the trip count depends only on data_size, so the loop is
 * perfectly predicted and the single data-dependent step compiles to a
 * conditional move. That makes the cost insensitive to the access pattern,
 * unlike a plain left/right binary search whose comparison branch mispredicts
 * on unsorted input.
 * Requires xdata[0] <= x <= xdata[data_size - 1].
 */
static int32_t find_interval(double x, const MYFLT *xdata, int32_t data_size) {
    int32_t base = 0;
    int32_t len = data_size - 1;

    while (len > 1) {
        const int32_t half = len >> 1;
        base += (x >= xdata[base + half]) ? half : 0;
        len -= half;
    }

    return base;
}

static void find_lerp_interval(CSN_LERP_INTERVAL *interval, double x, const MYFLT *xdata, const MYFLT *ydata, int32_t data_size, CSN_INTERP_BOUNDS_MODE bounds) {
    if (x < xdata[0] || x > xdata[data_size - 1]) {
        switch (bounds) {
            case REMAP_ERROR:
                interval->bmode = REMAP_NOT_VALID;
                return;
            case REMAP_CLAMP:
                if (x < xdata[0]) {
                    interval->bmode = REMAP_CLAMP_LEFT;
                } else {
                    interval->bmode = REMAP_CLAMP_RIGHT;
                }
                return;
            case REMAP_FILL:
                interval->bmode = REMAP_FILL_VALUE;
                return;
            case REMAP_EXTRAPOLATE:
                if (x < xdata[0]) {
                    interval->x0 = xdata[0];
                    interval->x1 = xdata[1];
                    interval->y0 = ydata[0];
                    interval->y1 = ydata[1];
                    interval->index = 0;
                    interval->bmode = REMAP_EXTRAPOLATE_LEFT;
                } else {
                    interval->x0 = xdata[data_size - 2];
                    interval->x1 = xdata[data_size - 1];
                    interval->y0 = ydata[data_size - 2];
                    interval->y1 = ydata[data_size - 1];
                    interval->index = data_size - 2;
                    interval->bmode = REMAP_EXTRAPOLATE_RIGHT;
                }
                return;
        }
    }

    const int32_t left = find_interval(x, xdata, data_size);

    interval->x0 = xdata[left];
    interval->x1 = xdata[left + 1];
    interval->y0 = ydata[left];
    interval->y1 = ydata[left + 1];
    interval->index = left;
    interval->bmode = REMAP_VALID;
    return;
}


/*
 * LINEAR INTERPOLATION
 * y = (y0 (x1 - x) + y1 (x - x0)) / (x1 - x0)
 */
static double lerp(double x, double x0, double x1, double y0, double y1) {
    return (y0 * (x1 - x) + y1 * (x - x0)) / (x1 - x0);
}

/*
 * NEAREST INTERPOLATION
 *     | y0 if (x - x0) <= (x1 - x)
 * y = |
 *     | y1 if (x - x0) > (x1 - x)
 */
static double nearest(double x, double x0, double x1, double y0, double y1) {
    return ((x - x0) <= (x1 - x)) ? y0 : y1;
}

/*
 * CUBIC PCHIP
 */
static double pchip_endpoint(double h0, double h1, double d0, double d1) {
    double m = ((2.0 * h0 + h1) * d0 - h0 * d1) / (h0 + h1);
    if ((m > 0.0) != (d0 > 0.0)) {
        return 0.0;
    }

    if (((d0 > 0.0) != (d1 > 0.0)) && fabs(m) > 3.0 * fabs(d0)) {
        return 3.0 * d0;
    }

    return m;
}

/*
 * PCHIP slope at breakpoint i.
 *
 * The Fritsch-Carlson scheme is local: m[i] only depends on the one or two
 * intervals adjacent to i. Evaluating it on demand therefore costs O(1) and
 * removes the need to tabulate (and re-tabulate) the whole curve whenever the
 * breakpoints change at k-rate.
 */
static double pchip_slope(const MYFLT *x, const MYFLT *y, int32_t n, int32_t i) {
    double h0, h1, d0, d1;

    if (n == 2) {
        return ((double) y[1] - (double) y[0]) / ((double) x[1] - (double) x[0]);
    }

    if (i == 0) {
        h0 = (double) x[1] - (double) x[0];
        h1 = (double) x[2] - (double) x[1];
        d0 = ((double) y[1] - (double) y[0]) / h0;
        d1 = ((double) y[2] - (double) y[1]) / h1;
        return pchip_endpoint(h0, h1, d0, d1);
    }

    if (i == n - 1) {
        h0 = (double) x[n - 1] - (double) x[n - 2];
        h1 = (double) x[n - 2] - (double) x[n - 3];
        d0 = ((double) y[n - 1] - (double) y[n - 2]) / h0;
        d1 = ((double) y[n - 2] - (double) y[n - 3]) / h1;
        return pchip_endpoint(h0, h1, d0, d1);
    }

    h0 = (double) x[i] - (double) x[i - 1];
    h1 = (double) x[i + 1] - (double) x[i];
    d0 = ((double) y[i] - (double) y[i - 1]) / h0;
    d1 = ((double) y[i + 1] - (double) y[i]) / h1;
    if (d0 == 0.0 || d1 == 0.0 || ((d0 > 0.0) != (d1 > 0.0))) {
        return 0.0;
    }

    const double w1 = 2.0 * h1 + h0;
    const double w2 = h1 + 2.0 * h0;
    return (w1 + w2) / (w1 / d0 + w2 / d1);
}

/*
 * Cubic Hermite coefficients of segment i, with the PCHIP slopes resolved on
 * the fly. Kept separate from the evaluation so that the vector opcode can
 * reuse them across every input value falling in the same segment.
 */
static void pchip_segment(CSN_PCHIP_SEGMENT *s, const MYFLT *xdata, const MYFLT *ydata, int32_t n, int32_t i) {
    const double x0 = (double) xdata[i];
    const double y0 = (double) ydata[i];
    const double h = (double) xdata[i + 1] - x0;

    s->x0 = x0;
    s->a = y0;

    if (h <= 0.0) {               /* non-increasing x: no usable segment */
        s->b = s->c = s->d = 0.0;
        return;
    }

    const double delta = ((double) ydata[i + 1] - y0) / h;
    const double m0 = pchip_slope(xdata, ydata, n, i);
    const double m1 = pchip_slope(xdata, ydata, n, i + 1);

    s->b = m0;
    s->c = (3.0 * delta - 2.0 * m0 - m1) / h;
    s->d = (m0 + m1 - 2.0 * delta) / (h * h);
}

/*
 * Outside [x[i], x[i+1]] this extrapolates along the same cubic.
 */
static double pchip_segment_eval(const CSN_PCHIP_SEGMENT *s, double x) {
    const double t = x - s->x0;
    return ((s->d * t + s->c) * t + s->b) * t + s->a;
}

static double pchip_eval(double x, const MYFLT *xdata, const MYFLT *ydata, int32_t n, int32_t i) {
    CSN_PCHIP_SEGMENT s;
    pchip_segment(&s, xdata, ydata, n, i);
    return pchip_segment_eval(&s, x);
}

/*
 * The interval search assumes strictly increasing breakpoints, for every
 * interpolation mode. Only checked for i-rate data, where it costs nothing
 * at performance time.
 */
static int32_t check_increasing(CSN_ARRAY *vec) {
    const MYFLT *v = vec->data;
    for (size_t i = 0; i < vec->size - 1; ++i) {
        if (v[i + 1] <= v[i]) {
            return NOTOK;
        }
    }
    return OK;
}


static int32_t remap_assign_value(CSOUND *csound, OPDS *perf_h, const double *x_data, const double *y_data, size_t data_size, const double x, int32_t ibounds, double fill_value, double *y_out, CSN_INTERP_MODE mode) {
    int32_t size = (int32_t) data_size;
    double y = 0.0;
    CSN_LERP_INTERVAL l_interval = {0};
    find_lerp_interval(&l_interval, x, x_data, y_data, size, (CSN_INTERP_BOUNDS_MODE) ibounds);
    switch (l_interval.bmode) {
        case REMAP_NOT_VALID:
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[remap] x value out of bounds");
        case REMAP_CLAMP_LEFT:
            y = y_data[0];
            break;
        case REMAP_CLAMP_RIGHT:
            y = y_data[size - 1];
            break;
        case REMAP_FILL_VALUE:
            y = fill_value;
            break;
        case REMAP_VALID:
        case REMAP_EXTRAPOLATE_LEFT:
        case REMAP_EXTRAPOLATE_RIGHT:
            switch (mode) {
                case REMAP_LINEAR:
                    y = lerp(x, l_interval.x0, l_interval.x1, l_interval.y0, l_interval.y1);
                    break;
                case REMAP_NEAREST:
                    y = nearest(x, l_interval.x0, l_interval.x1, l_interval.y0, l_interval.y1);
                    break;
                case REMAP_PREVIOUS:
                    y = l_interval.y0;
                    break;
                case REMAP_NEXT:
                    y = l_interval.y1;
                    break;
                case REMAP_CUBIC:
                    y = pchip_eval(x, x_data, y_data, size, l_interval.index);
                    break;
            }
            break;
    }

    *y_out = y;
    return OK;
}

int32_t csnarray_remap_scalar(CSOUND *csound, CSN_REMAP_SCALAR *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t imode = (int32_t) *p->mode;
    int32_t ibounds = (int32_t) *p->bounds;
    double fill_value = (double) *p->fill;

    uint32_t x_data_handle = (uint32_t) p->data_handle_x->id;
    uint32_t y_data_handle = (uint32_t) p->data_handle_y->id;

    int32_t is_valid_mode = imode >= REMAP_LINEAR && imode <= REMAP_CUBIC;
    if (!is_valid_mode) {
        return csound->InitError(csound, "[csnarray] Not valid mode value");
    }

    int32_t is_valid_bounds = ibounds >= REMAP_ERROR && ibounds <= REMAP_EXTRAPOLATE;
    if (!is_valid_bounds) {
        return csound->InitError(csound, "[csnarray] Invalid bounds mode: %d", ibounds);
    }

    int32_t res = OK;

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot_x = get_slot(reg, x_data_handle);
    if (slot_x == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", x_data_handle);
        goto done;
    }

    CSN_SLOT *slot_y = get_slot(reg, y_data_handle);
    if (slot_y == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", y_data_handle);
        goto done;
    }

    CSN_ARRAY *x_data = slot_x->array;
    size_t x_data_size = x_data->size;
    uint32_t x_data_ndim = x_data->ndim;

    CSN_ARRAY *y_data = slot_y->array;
    size_t y_data_size = y_data->size;
    uint32_t y_data_ndim = y_data->ndim;

    if (x_data->itype != CSN_REAL || y_data->itype != CSN_REAL) {
        res = csound->InitError(csound, "[csnarray] x and y array must be real array");
        goto done;
    }

    if (x_data_size < 2 || y_data_size < 2 || x_data_ndim != 1U || y_data_ndim != 1U) {
        res = csound->InitError(csound, "[csnarray] x and y array should have length greater or equal to two and dimension equal to 1");
        goto done;
    }

    if (x_data_size != y_data_size) {
        res = csound->InitError(csound, "[csnarray] x and y array should have same length");
        goto done;
    }

    if (check_increasing(x_data) != OK) {
        res = csound->InitError(csound, "[remap] x array must be strictly increasing");
        goto done;
    }

    double x = (double) *p->x;
    double y = 0.0;
    res = remap_assign_value(csound, NULL, x_data->data, y_data->data, x_data->size, x, (CSN_INTERP_BOUNDS_MODE) ibounds, fill_value, &y, (CSN_INTERP_MODE) imode);
    if (res != OK) goto done;

    *p->y = (MYFLT) y;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_remap_scalar_k_init(CSOUND *csound, CSN_REMAP_SCALAR *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    p->registry = reg;

    int32_t imode = (int32_t) *p->mode;
    int32_t ibounds = (int32_t) *p->bounds;
    double fill_value = (double) *p->fill;

    int32_t is_valid_mode = imode >= REMAP_LINEAR && imode <= REMAP_CUBIC;
    if (!is_valid_mode) {
        return csound->InitError(csound, "[csnarray] Not valid mode value");
    }

    int32_t is_valid_bounds = ibounds >= REMAP_ERROR && ibounds <= REMAP_EXTRAPOLATE;
    if (!is_valid_bounds) {
        return csound->InitError(csound, "[csnarray] Invalid bounds mode: %d", ibounds);
    }

    p->imode = (CSN_INTERP_MODE) imode;
    p->ibounds = (CSN_INTERP_BOUNDS_MODE) ibounds;
    p->fill_value = fill_value;

    p->is_published = false;
    return OK;
}

int32_t csnarray_remap_scalar_k(CSOUND *csound, CSN_REMAP_SCALAR *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    uint32_t x_data_handle = (uint32_t) p->data_handle_x->id;
    uint32_t y_data_handle = (uint32_t) p->data_handle_y->id;

    CHECK_KTRIG(p->trig);

    int32_t res = OK;

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot_x = get_slot(reg, x_data_handle);
    if (slot_x == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", x_data_handle);
    }

    CSN_SLOT *slot_y = get_slot(reg, y_data_handle);
    if (slot_y == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", y_data_handle);
    }

    CSN_ARRAY *x_data = slot_x->array;
    size_t x_data_size = x_data->size;
    uint32_t x_data_ndim = x_data->ndim;

    CSN_ARRAY *y_data = slot_y->array;
    size_t y_data_size = y_data->size;
    uint32_t y_data_ndim = y_data->ndim;

    bool is_same_x_data = p->is_published ? is_same_array_version(&x_data->version, &p->prev_x_data_version) : false;
    bool is_same_y_data = p->is_published ? is_same_array_version(&y_data->version, &p->prev_y_data_version) : false;

    double x = (double) *p->x;
    if (is_same_x_data && is_same_y_data && p->prev_x == x) {
        *p->y = (MYFLT) p->prev_y;
        goto done;
    }

    if (x_data->itype != CSN_REAL || y_data->itype != CSN_REAL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] x and y array must be real array");
    }

    if (x_data_size < 2 || y_data_size < 2 || x_data_ndim != 1U || y_data_ndim != 1U) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] x and y array should have length greater or equal to two and dimension equal to 1");
    }

    if (x_data_size != y_data_size) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] x and y array should have same length");
    }

    if (!is_same_x_data) {
        if (check_increasing(x_data) != OK) {
            csound->UnlockMutex(reg->mutex);
            return csound->PerfError(csound, &p->h, "[remap] x array must be strictly increasing");
        }
    }

    double y = 0.0;
    res = remap_assign_value(csound, &p->h, x_data->data, y_data->data, x_data->size, x, p->ibounds, p->fill_value, &y, p->imode);
    if (res != OK) goto done;

    set_array_version(&p->prev_x_data_version, &x_data->version);
    set_array_version(&p->prev_y_data_version, &y_data->version);
    p->prev_x = x;
    p->prev_y = y;
    p->is_published = true;
    *p->y = (MYFLT) y;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_remap_k_init(CSOUND *csound, CSN_REMAP *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t imode = (int32_t) *p->mode;
    int32_t ibounds = (int32_t) *p->bounds;
    double fill_value = (double) *p->fill;

    int32_t is_valid_mode = imode >= REMAP_LINEAR && imode <= REMAP_CUBIC;
    if (!is_valid_mode) {
        return csound->InitError(csound, "[csnarray] Not valid mode value");
    }

    int32_t is_valid_bounds = ibounds >= REMAP_ERROR && ibounds <= REMAP_EXTRAPOLATE;
    if (!is_valid_bounds) {
        return csound->InitError(csound, "[csnarray] Invalid bounds mode: %d", ibounds);
    }

    p->imode = (CSN_INTERP_MODE) imode;
    p->ibounds = (CSN_INTERP_BOUNDS_MODE) ibounds;
    p->fill_value = fill_value;

    const char *err = NULL;

    csound->LockMutex(reg->mutex);
    uint32_t shape[CSN_MAX_DIMS] = {0};
    shape[0] = 1U;

    if (create_csnarray_locked(csound, reg, &p->h, 1U, shape, &p->array, p->handle, NULL, 0, &err, CSN_REAL) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->InitError(csound, "[csnarray] %s", err);
    }

    reset_empty_csnarray(p->array, 1U, shape, CSN_REAL);

    SET_KDATA_BEGIN(p, reg);
    p->is_published = false;

    csound->UnlockMutex(reg->mutex);
    return OK;
}

/*
 * Reads source_size query points out of source (source_stride apart), maps each
 * one through the breakpoints (x_data, y_data, data_size) and writes the result
 * to dst (dst_stride apart). csninterp keeps a value where it found it, so both
 * strides match there; csnresample changes the length of the mapped axis, so it
 * walks a contiguous query grid and writes with the destination stride.
 */
static int32_t remap_slice(CSOUND *csound, OPDS *perf_h, double *dst, size_t dst_stride, const double *x_data, const double *y_data, size_t data_size, const double *source, size_t source_size, size_t source_stride, CSN_INTERP_BOUNDS_MODE ibounds, CSN_INTERP_MODE imode, const double fill_value) {
    int32_t res = OK;
    for (size_t i = 0; i < source_size; ++i) {
        CSN_COMPLEXDAT y = { 0.0, 0.0 };
        CSN_COMPLEXDAT z = slice_get(source, i, source_stride, CSN_REAL);
        res = remap_assign_value(csound, perf_h, x_data, y_data, data_size, z.re, ibounds, fill_value, &y.re, imode);
        if (res != OK) return res;
        slice_put(dst, i, dst_stride, CSN_REAL, y);
    }
    return res;
}


int32_t csnarray_remap_k(CSOUND *csound, CSN_REMAP *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REG_HANDLE(csound, &p->h, reg, p->k_data.owned_handle);

    uint32_t source_handle = (uint32_t) p->source_handle->id;
    uint32_t x_data_handle = (uint32_t) p->data_handle_x->id;
    uint32_t y_data_handle = (uint32_t) p->data_handle_y->id;

    int32_t res = OK;
    const char *err = NULL;

    CHECK_KTRIG(p->trig);

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot_source = get_slot(reg, source_handle);
    if (slot_source == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
    }

    CSN_SLOT *slot_x = get_slot(reg, x_data_handle);
    if (slot_x == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", x_data_handle);
    }

    CSN_SLOT *slot_y = get_slot(reg, y_data_handle);
    if (slot_y == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", y_data_handle);
    }

    CSN_ARRAY *source_arr = slot_source->array;
    size_t source_size = source_arr->size;
    uint32_t *source_shape = source_arr->shape;

    CSN_ARRAY *x_data = slot_x->array;
    size_t x_data_size = x_data->size;
    uint32_t x_data_ndim = x_data->ndim;
    uint32_t *x_data_shape = x_data->shape;

    CSN_ARRAY *y_data = slot_y->array;
    size_t y_data_size = y_data->size;
    uint32_t y_data_ndim = y_data->ndim;
    uint32_t *y_data_shape = y_data->shape;

    if (x_data_size < 2 || y_data_size < 2 || x_data_ndim != 1U || y_data_ndim != 1U) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] x and y array should have length greater or equal to two and dimension equal to 1");
    }

    if (*p->axis != -1.0 && !IS_VALID_AXIS((double) *p->axis, source_arr->ndim)) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "Axis out of bounds");
    }
    int32_t axis = (int32_t) *p->axis;

    bool is_same_source = false;
    bool is_same_x_data = false;
    bool is_same_y_data = false;
    bool is_same_axis = false;
    if (p->is_published) {
        is_same_source = is_same_array_version(&source_arr->version, &p->prev_x_source_version);
        is_same_x_data = is_same_array_version(&x_data->version, &p->prev_x_data_version);
        is_same_y_data = is_same_array_version(&y_data->version, &p->prev_y_data_version);
        is_same_axis = axis == p->k_data.prev_axis;
    }

    if (is_same_x_data && is_same_y_data && is_same_source && is_same_axis) {
        p->handle->id = p->k_data.owned_handle;
        goto done;
    }

    if (x_data->itype != CSN_REAL || y_data->itype != CSN_REAL || source_arr->itype != CSN_REAL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Interp requires real array");
    }

    if (x_data_ndim != y_data_ndim) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] x and y array should have same shape and dim");
    }

    bool is_same_shape = memcmp(x_data_shape, y_data_shape, sizeof(uint32_t) * (size_t) x_data_ndim) != 0;

    if (is_same_shape || x_data_size != y_data_size) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "x and y array should have same shape and dim");
    }

    if (!is_same_x_data) {
        if (check_increasing(x_data) != OK) {
            csound->UnlockMutex(reg->mutex);
            return csound->PerfError(csound, &p->h, "[remap] x array must be strictly increasing");
        }
    }

    uint32_t new_ndim = source_arr->ndim;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    memcpy(new_shape, source_arr->shape, sizeof(uint32_t) * CSN_MAX_DIMS);

    size_t req_size = 0;
    if (get_array_size_from_shape(&req_size, new_ndim, new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSN_ARRAY *arr = NULL;
    size_t logical_size = source_size == 0 ? 0 : req_size;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, new_ndim, new_shape, logical_size, CSN_REAL, err);
    if (res != OK) goto done;
    p->array = arr;

    if (axis == -1) {
        res = remap_slice(csound, &p->h, arr->data, 1U, x_data->data, y_data->data, x_data_size, source_arr->data, source_size, 1U, p->ibounds, p->imode, p->fill_value);
        if (res != OK) goto done;
    } else {
            uint32_t reduced_shape[CSN_MAX_DIMS] = {0};
            uint32_t reduced_ndim = 0;
            size_t slice_count = 1;
            for (uint32_t i = 0; i < source_arr->ndim; ++i) {
                if (i != (uint32_t) axis) {
                    reduced_shape[reduced_ndim++] = source_arr->shape[i];
                    slice_count *= source_arr->shape[i];
                }
            }

            size_t src_stride = source_arr->strides[axis];

            for (size_t linear = 0; linear < slice_count; ++linear) {
                uint32_t dst_coords[CSN_MAX_DIMS] = {0};
                uint32_t src_coords[CSN_MAX_DIMS] = {0};

                from_linear_to_coords(dst_coords, reduced_shape, linear, reduced_ndim);
                for (uint32_t i = 0, j = 0; i < source_arr->ndim; ++i) {
                    src_coords[i] = (i == (uint32_t) axis) ? 0 : dst_coords[j++];
                }

                size_t src_base = from_coords_to_offset(src_coords, source_arr->strides, source_arr->ndim);
                size_t dst_base = from_coords_to_offset(src_coords, arr->strides, source_arr->ndim);
                res = remap_slice(csound, &p->h, arr->data + dst_base, arr->strides[axis], x_data->data, y_data->data, x_data_size, source_arr->data + src_base, source_shape[axis], src_stride, p->ibounds, p->imode, p->fill_value);
                if (res != OK) goto done;
            }
    }


    set_array_version(&p->prev_x_source_version, &source_arr->version);
    set_array_version(&p->prev_x_data_version, &x_data->version);
    set_array_version(&p->prev_y_data_version, &y_data->version);
    SET_KDATA_END(p, new_shape, new_ndim, CSN_REAL);
    p->is_published = true;
    p->k_data.prev_axis = axis;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

/*
 * Query grid of a resample. The breakpoints are the source indexes 0..n-1 and
 * the new_length query points span that whole range, so the first and the last
 * output sample always land exactly on the source endpoints and nothing is
 * asked outside the table (whatever the bounds mode is). A single output point
 * has no span to divide and takes the first sample.
 */
static void fill_resample_grid(double *x_data, size_t data_size, double *x_source, size_t new_length) {
    for (size_t i = 0; i < data_size; ++i) {
        x_data[i] = (double) i;
    }

    if (new_length == 1) {
        x_source[0] = 0.0;
        return;
    }

    const double span = (double) (data_size - 1);
    const double step = span / (double) (new_length - 1);
    for (size_t i = 0; i < new_length; ++i) {
        x_source[i] = (double) i * step;
    }
    /* Rounding must never push the last query past the last breakpoint. */
    x_source[new_length - 1] = span;
}

/*
 * Grows one of the resample scratch buffers. ReAlloc doubles as malloc for a
 * NULL buffer, so the k-rate opcode can start with nothing allocated and only
 * pay for the sizes it actually sees.
 */
static int32_t ensure_resample_buffer(CSOUND *csound, CSN_SCRATCH *scratch, size_t required) {
    if (scratch->scratch != NULL && scratch->scratch_capacity >= required) {
        return OK;
    }

    const size_t new_capacity = required * 2;
    double *data = csound->ReAlloc(csound, scratch->scratch, sizeof(double) * new_capacity);
    if (data == NULL) {
        return NOTOK;
    }

    scratch->scratch = data;
    scratch->scratch_capacity = new_capacity;
    return OK;
}

/*
 * Shared body of the two resample opcodes. axis == -1 resamples the array read
 * as a flat vector; otherwise every slice along axis is resampled on its own.
 * y_scratch is only touched in the second case, where the breakpoints are a
 * strided slice and the interval search wants them contiguous.
 */
static int32_t resample_run(
    CSOUND *csound,
    OPDS *perf_h,
    CSN_ARRAY *arr,
    CSN_ARRAY *source_arr,
    int32_t axis,
    const double *x_data,
    const double *x_source,
    size_t new_length,
    double *y_scratch,
    CSN_INTERP_BOUNDS_MODE ibounds,
    CSN_INTERP_MODE imode,
    const double fill_value
) {
    if (axis == -1) {
        return remap_slice(csound, perf_h, arr->data, 1U, x_data, source_arr->data, source_arr->size, x_source, new_length, 1U, ibounds, imode, fill_value);
    }

    uint32_t reduced_shape[CSN_MAX_DIMS] = {0};
    uint32_t reduced_ndim = 0;
    size_t slice_count = 1;
    for (uint32_t i = 0; i < source_arr->ndim; ++i) {
        if (i != (uint32_t) axis) {
            reduced_shape[reduced_ndim++] = source_arr->shape[i];
            slice_count *= source_arr->shape[i];
        }
    }

    const size_t data_size = (size_t) source_arr->shape[axis];
    const size_t src_stride = source_arr->strides[axis];
    const size_t dst_stride = arr->strides[axis];

    for (size_t linear = 0; linear < slice_count; ++linear) {
        uint32_t dst_coords[CSN_MAX_DIMS] = {0};
        uint32_t src_coords[CSN_MAX_DIMS] = {0};

        from_linear_to_coords(dst_coords, reduced_shape, linear, reduced_ndim);
        for (uint32_t i = 0, j = 0; i < source_arr->ndim; ++i) {
            src_coords[i] = (i == (uint32_t) axis) ? 0 : dst_coords[j++];
        }

        const size_t src_base = from_coords_to_offset(src_coords, source_arr->strides, source_arr->ndim);
        const size_t dst_base = from_coords_to_offset(src_coords, arr->strides, source_arr->ndim);

        for (size_t i = 0; i < data_size; ++i) {
            y_scratch[i] = source_arr->data[src_base + i * src_stride];
        }

        int32_t res = remap_slice(csound, perf_h, arr->data + dst_base, dst_stride, x_data, y_scratch, data_size, x_source, new_length, 1U, ibounds, imode, fill_value);
        if (res != OK) return res;
    }

    return OK;
}

/* The resampled extent, and the shape it produces. Flattening collapses the
   result to a single dimension, since the source layout does not survive it. */
static void resample_layout(const CSN_ARRAY *source_arr, int32_t axis, uint32_t new_length, uint32_t *new_ndim, uint32_t *new_shape, size_t *data_size) {
    if (axis == -1) {
        *new_ndim = 1U;
        new_shape[0] = new_length;
        *data_size = source_arr->size;
        return;
    }

    *new_ndim = source_arr->ndim;
    memcpy(new_shape, source_arr->shape, sizeof(uint32_t) * CSN_MAX_DIMS);
    new_shape[axis] = new_length;
    *data_size = (size_t) source_arr->shape[axis];
}

int32_t csnarray_resample(CSOUND *csound, CSN_RESAMPLE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t imode = (int32_t) *p->mode;
    int32_t ibounds = (int32_t) *p->bounds;
    double fill_value = (double) *p->fill;

    uint32_t source_handle = p->source_handle->id;

    if (!IS_VALID_LENGTH((double) *p->new_length) || (double) *p->new_length < 1.0) {
        return csound->InitError(csound, "[csnarray] Invalid new length");
    }
    uint32_t new_length = (uint32_t) *p->new_length;

    int32_t is_valid_mode = imode >= REMAP_LINEAR && imode <= REMAP_CUBIC;
    if (!is_valid_mode) {
        return csound->InitError(csound, "[csnarray] Not valid mode value");
    }

    int32_t is_valid_bounds = ibounds >= REMAP_ERROR && ibounds <= REMAP_EXTRAPOLATE;
    if (!is_valid_bounds) {
        return csound->InitError(csound, "[csnarray] Invalid bounds mode: %d", ibounds);
    }

    p->imode = (CSN_INTERP_MODE) imode;
    p->ibounds = (CSN_INTERP_BOUNDS_MODE) ibounds;
    p->fill_value = fill_value;

    int32_t res = OK;
    const char *err = NULL;
    /* Declared before the first jump to done, which frees them. */
    double *x_data = NULL;
    double *x_source = NULL;
    double *y_scratch = NULL;

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = slot->array;

    if (source_arr->itype != CSN_REAL) {
        res = csound->InitError(csound, "[csnarray] Source array must be real array");
        goto done;
    }

    if (*p->axis != -1.0 && !IS_VALID_AXIS((double) *p->axis, source_arr->ndim)) {
        res = csound->InitError(csound, "[csnarray] Axis out of bounds");
        goto done;
    }
    int32_t axis = (int32_t) *p->axis;

    uint32_t new_ndim = 1U;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    size_t data_size = 0;
    resample_layout(source_arr, axis, new_length, &new_ndim, new_shape, &data_size);

    if (data_size < 2) {
        res = csound->InitError(csound, "[csnarray] The resampled axis should have length greater or equal to two");
        goto done;
    }

    const uint32_t protect[1] = { source_handle };
    if (create_csnarray_locked(csound, reg, &p->h, new_ndim, new_shape, &p->array, p->handle, protect, 1U, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    x_data = csound->Calloc(csound, sizeof(double) * data_size);
    x_source = csound->Calloc(csound, sizeof(double) * new_length);
    if (axis != -1) {
        y_scratch = csound->Calloc(csound, sizeof(double) * data_size);
    }

    if (x_data == NULL || x_source == NULL || (axis != -1 && y_scratch == NULL)) {
        res = csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
        goto done;
    }

    fill_resample_grid(x_data, data_size, x_source, new_length);

    res = resample_run(csound, NULL, p->array, source_arr, axis, x_data, x_source, new_length, y_scratch, p->ibounds, p->imode, p->fill_value);

done:
    if (x_data != NULL) csound->Free(csound, x_data);
    if (x_source != NULL) csound->Free(csound, x_source);
    if (y_scratch != NULL) csound->Free(csound, y_scratch);
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_resample_k_init(CSOUND *csound, CSN_RESAMPLE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t imode = (int32_t) *p->mode;
    int32_t ibounds = (int32_t) *p->bounds;
    double fill_value = (double) *p->fill;

    int32_t is_valid_mode = imode >= REMAP_LINEAR && imode <= REMAP_CUBIC;
    if (!is_valid_mode) {
        return csound->InitError(csound, "[csnarray] Not valid mode value");
    }

    int32_t is_valid_bounds = ibounds >= REMAP_ERROR && ibounds <= REMAP_EXTRAPOLATE;
    if (!is_valid_bounds) {
        return csound->InitError(csound, "[csnarray] Invalid bounds mode: %d", ibounds);
    }

    p->imode = (CSN_INTERP_MODE) imode;
    p->ibounds = (CSN_INTERP_BOUNDS_MODE) ibounds;
    p->fill_value = fill_value;

    const char *err = NULL;

    csound->LockMutex(reg->mutex);
    uint32_t shape[CSN_MAX_DIMS] = {0};
    shape[0] = 1U;

    if (create_csnarray_locked(csound, reg, &p->h, 1U, shape, &p->array, p->handle, NULL, 0, &err, CSN_REAL) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->InitError(csound, "[csnarray] %s", err);
    }

    reset_empty_csnarray(p->array, 1U, shape, CSN_REAL);

    SET_KDATA_BEGIN(p, reg);
    p->is_published = false;

    /* The scratch buffers are grown by the first pass that needs them: their
       size depends on the source and on the k-rate length. */
    p->x_source_scratch.scratch = NULL;
    p->x_data_scratch.scratch = NULL;
    p->y_data_scratch.scratch = NULL;
    p->x_source_scratch.scratch_capacity = 0;
    p->x_data_scratch.scratch_capacity = 0;
    p->y_data_scratch.scratch_capacity = 0;

    csound->UnlockMutex(reg->mutex);
    return OK;
}

int32_t csnarray_resample_k(CSOUND *csound, CSN_RESAMPLE *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REG_HANDLE(csound, &p->h, reg, p->k_data.owned_handle);

    uint32_t source_handle = p->source_handle->id;

    if (!IS_VALID_LENGTH((double) *p->new_length) || (double) *p->new_length < 1.0) {
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid new length");
    }
    uint32_t new_length = (uint32_t) *p->new_length;

    int32_t res = OK;
    const char *err = NULL;

    CHECK_KTRIG(p->trig);

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
    }

    CSN_ARRAY *source_arr = slot->array;

    if (source_arr->itype != CSN_REAL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Source array must be real array");
    }

    if (*p->axis != -1.0 && !IS_VALID_AXIS((double) *p->axis, source_arr->ndim)) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "Axis out of bounds");
    }
    int32_t axis = (int32_t) *p->axis;

    bool is_same_source = false;
    bool is_same_axis = false;
    bool is_same_length = false;
    if (p->is_published) {
        is_same_source = is_same_array_version(&source_arr->version, &p->prev_x_source_version);
        is_same_axis = axis == (int32_t) p->k_data.prev_axis;
        is_same_length = new_length == p->k_data.prev_size;
    }

    if (is_same_source && is_same_axis && is_same_length) {
        p->handle->id = p->k_data.owned_handle;
        goto done;
    }

    uint32_t new_ndim = 1U;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    size_t data_size = 0;
    resample_layout(source_arr, axis, new_length, &new_ndim, new_shape, &data_size);

    if (data_size < 2) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] The resampled axis should have length greater or equal to two");
    }

    size_t req_size = 0;
    if (get_array_size_from_shape(&req_size, new_ndim, new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSN_ARRAY *arr = NULL;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, new_ndim, new_shape, req_size, CSN_REAL, err);
    if (res != OK) goto done;
    p->array = arr;

    if (ensure_resample_buffer(csound, &p->x_data_scratch, data_size) != OK
        || ensure_resample_buffer(csound, &p->x_source_scratch, (size_t) new_length) != OK
        || (axis != -1 && ensure_resample_buffer(csound, &p->y_data_scratch, data_size) != OK)) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Internal error: memory allocation failed");
        goto done;
    }

    fill_resample_grid(p->x_data_scratch.scratch, data_size, p->x_source_scratch.scratch, (size_t) new_length);

    res = resample_run(csound, &p->h, arr, source_arr, axis, p->x_data_scratch.scratch, p->x_source_scratch.scratch, (size_t) new_length, p->y_data_scratch.scratch, p->ibounds, p->imode, p->fill_value);
    if (res != OK) goto done;

    set_array_version(&p->prev_x_source_version, &source_arr->version);
    SET_KDATA_END(p, new_shape, new_ndim, CSN_REAL);
    p->is_published = true;
    p->k_data.prev_size = new_length;
    p->k_data.prev_axis = axis;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t truncate_assign_shape(CSOUND *csound, OPDS *perf_h, const uint32_t *source_shape, uint32_t *new_shape, uint32_t source_ndim, size_t source_size, size_t new_length, const MYFLT *in_axis, int32_t *out_axis, CSN_RESIZE_MODE mode) {
    if (mode == CSN_HEAD_ARR && source_ndim != 1U) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Dimension greater than one not allowed for head");
    }

    if (mode == CSN_HEAD_ARR) {
        if (new_length > source_size) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Head length %zu must not exceed array size %zu", new_length, source_size);
        }
        new_shape[0] = new_length;
    } else if (mode == CSN_TRUNCATE_ARR) {
        double axis_value = (double) *in_axis;
        if (axis_value != -1.0 && !IS_VALID_AXIS(axis_value, source_ndim)) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: -1 for all axes, or finite integers 0..%u)", axis_value, source_ndim, source_ndim - 1);
        }
        *out_axis = (int32_t) *in_axis;
        int32_t axis = *out_axis;

        if (axis == -1) {
            for (uint32_t i = 0; i < source_ndim; i++) {
                if (new_length > source_shape[i]) {
                    return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] New length %zu must not exceed array axis length %u", new_length, source_shape[i]);
                }
                new_shape[i] = new_length;
            }
        } else {
            if (new_length > source_shape[axis]) {
                return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] New length %zu must not exceed array axis length %u", new_length, source_shape[axis]);
            }
            new_shape[axis] = new_length;
        }
    } else {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Mode not allowed here");
    }

    return OK;
}

static void truncate_assign_value(CSN_ARRAY *arr, CSN_ARRAY *source_arr, uint32_t *new_shape, uint32_t new_ndim, int32_t axis) {
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;
    size_t source_size = source_arr->size;
    /* Every offset and every count below is in elements; a complex array keeps
       two doubles per element, so each one is scaled by itype. */
    ITEM_TYPE itype = source_arr->itype;
    if (axis == -1) {
        uint32_t coords[CSN_MAX_DIMS] = {0};
        for (size_t i = 0; i < arr->size; i++) {
            from_linear_to_coords(coords, new_shape, i, new_ndim);
            size_t src_offset = from_coords_to_offset(coords, source_arr->strides, source_ndim);
            arr->data[i * itype] = source_arr->data[src_offset * itype];
            if (itype == CSN_COMPLEX) {
                arr->data[i * itype + 1] = source_arr->data[src_offset * itype + 1];
            }
        }
    } else if (axis > 0 && (uint32_t) axis < source_ndim) {
        size_t block_size = 1;
        for (uint32_t i = (uint32_t) axis + 1; i < source_ndim; i++) {
            block_size *= source_shape[i];
        }

        uint32_t src_axis_size = source_shape[axis];
        uint32_t dst_axis_size = new_shape[axis];
        size_t outer_count = source_size / (src_axis_size * block_size);
        for (size_t outer = 0; outer < outer_count; outer++) {
            size_t src_base = outer * src_axis_size * block_size;
            size_t dst_base = outer * dst_axis_size * block_size;
            size_t copy_count = block_size * dst_axis_size;
            memcpy(arr->data + dst_base * itype, source_arr->data + src_base * itype, sizeof(double) * copy_count * itype);
        }
    } else {
        /* axis == 0 || CSN_HEAD_ARR: the kept elements are already a prefix */
        memcpy(arr->data, source_arr->data, sizeof(double) * arr->size * itype);
    }
}

static int32_t truncate_assign_value_in(CSOUND *csound, OPDS *perf_h, CSN_ARRAY *source_arr, uint32_t new_ndim, const uint32_t *new_shape, int32_t axis) {
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;
    size_t source_size = source_arr->size;
    size_t new_size = 0;
    if (get_array_size_from_shape(&new_size, new_ndim, new_shape) != OK) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }
    /* Same element/double distinction as truncate_assign_value. */
    ITEM_TYPE itype = source_arr->itype;
    if (axis == -1) {
        uint32_t coords[CSN_MAX_DIMS] = {0};
        for (size_t i = 0; i < new_size; i++) {
            from_linear_to_coords(coords, new_shape, i, new_ndim);
            size_t src_offset = from_coords_to_offset(coords, source_arr->strides, source_ndim);
            source_arr->data[i * itype] = source_arr->data[src_offset * itype];
            if (itype == CSN_COMPLEX) {
                source_arr->data[i * itype + 1] = source_arr->data[src_offset * itype + 1];
            }
        }
    } else if (axis > 0 && (uint32_t) axis < source_ndim) {
        size_t block_size = 1;
        for (uint32_t i = (uint32_t) axis + 1; i < source_ndim; i++) {
            block_size *= source_shape[i];
        }

        uint32_t src_axis_size = source_shape[axis];
        uint32_t dst_axis_size = new_shape[axis];
        size_t outer_count = source_size / (src_axis_size * block_size);
        for (size_t outer = 0; outer < outer_count; outer++) {
            size_t src_base = outer * src_axis_size * block_size;
            size_t dst_base = outer * dst_axis_size * block_size;
            size_t copy_count = block_size * dst_axis_size;
            memmove(source_arr->data + dst_base * itype, source_arr->data + src_base * itype, sizeof(double) * copy_count * itype);
        }
    }
    /* also for axis == 0 || CSN_HEAD_ARR */
    set_csnarray_layout(source_arr, new_ndim, new_shape, new_size, source_arr->itype);
    return OK;
}

static int32_t truncate_helper(CSOUND *csound, CSN_TRUNCATE *p, CSN_RESIZE_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = (uint32_t) p->source_handle->id;

    if (!IS_VALID_LENGTH((double) *p->length)) {
        return csound->InitError(csound, "[csnarray] Invalid new array length");
    }
    uint32_t new_length = (uint32_t) *p->length;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = slot->array;
    size_t source_size = source_arr->size;
    uint32_t *source_shape = source_arr->shape;
    uint32_t source_ndim = source_arr->ndim;
    ITEM_TYPE itype = source_arr->itype;

    uint32_t new_ndim = source_ndim;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    memcpy(new_shape, source_shape, sizeof(uint32_t) * CSN_MAX_DIMS);

    int32_t axis = -1;
    res = truncate_assign_shape(csound, NULL, source_shape, new_shape, source_ndim, source_size, (size_t) new_length, p->arg_a, &axis, mode);
    if (res != OK) goto done;

    uint32_t protect[1] = { source_handle };
    if (create_csnarray_locked(csound, reg, &p->h, new_ndim, new_shape, &p->array, p->handle, protect, 1U, &err, itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    truncate_assign_value(p->array, source_arr, new_shape, new_ndim, axis);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_truncate(CSOUND *csound, CSN_TRUNCATE *p) {
    return truncate_helper(csound, p, CSN_TRUNCATE_ARR);
}

int32_t csnarray_head(CSOUND *csound, CSN_TRUNCATE *p) {
    return truncate_helper(csound, p, CSN_HEAD_ARR);
}

int32_t csnarray_truncate_in(CSOUND *csound, CSN_TRUNCATE_IN *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = (uint32_t) p->source_handle->id;

    if (!IS_VALID_LENGTH((double) *p->length)) {
        return csound->InitError(csound, "[csnarray] Invalid new array length");
    }
    uint32_t new_length = (uint32_t) *p->length;

    int32_t res = OK;

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = slot->array;
    size_t source_size = source_arr->size;
    uint32_t *source_shape = source_arr->shape;
    uint32_t source_ndim = source_arr->ndim;

    uint32_t new_ndim = source_ndim;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    memcpy(new_shape, source_shape, sizeof(uint32_t) * CSN_MAX_DIMS);

    int32_t axis = -1;
    res = truncate_assign_shape(csound, NULL, source_shape, new_shape, source_ndim, source_size, (size_t) new_length, p->axis, &axis, CSN_TRUNCATE_ARR);
    if (res != OK) goto done;

    res = truncate_assign_value_in(csound, NULL, source_arr, new_ndim, new_shape, axis);
    if (res != OK) goto done;

    /* truncate_assign_value_in moves the layout version; the payload moved too. */
    update_array_data_version(&source_arr->version);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t resize_parse_shape_and_size(CSOUND *csound, OPDS *perf_h, uint32_t *ndim, uint32_t *out_shape, const ARRAYDAT *in_shape, size_t *new_size) {
    if (perf_h == NULL) {
        int32_t res_shape = parse_shape_array(csound, in_shape, ndim, out_shape);
        if (res_shape != OK) return res_shape;
    } else {
        int32_t res_shape = parse_shape_array_k(csound, perf_h, in_shape, ndim, out_shape);
        if (res_shape != OK) return res_shape;
    }

    if (get_array_size_from_shape(new_size, *ndim, out_shape) != OK) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] Shape is invalid or its element count exceeds the configured limit");
    }
    return OK;
}

static void resize_assign_value(CSN_ARRAY *arr, CSN_ARRAY *source_arr, size_t new_size) {
    size_t count = source_arr->size < new_size ? source_arr->size : new_size;
    memcpy(arr->data, source_arr->data, sizeof(double) * count * arr->itype);
    if (new_size > source_arr->size) {
        size_t count_diff = new_size - source_arr->size;
        memset(arr->data + source_arr->size * arr->itype, 0, sizeof(double) * count_diff * arr->itype);
    }
}

int32_t csnarray_resize(CSOUND *csound, CSN_RESIZE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;

    uint32_t ndim = 0;
    uint32_t shape[CSN_MAX_DIMS] = {0};
    size_t new_size = 0;

    int32_t res = resize_parse_shape_and_size(csound, NULL, &ndim, shape, p->new_shape, &new_size);
    if (res != OK) return res;

    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = slot->array;
    /* The output is created first: p->array is still NULL on this pass, and the
       item type has to come from the source. */
    if (create_csnarray_locked(csound, reg, &p->h, ndim, shape, &p->array, p->handle, &source_handle, 1U, &err, source_arr->itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    resize_assign_value(p->array, source_arr, new_size);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t resize_in_assign_value(CSOUND *csound, OPDS *perf_h, CSN_ARRAY **source_array, size_t new_size, ITEM_TYPE itype) {
    CSN_ARRAY *source_arr = *source_array;
    const size_t old_size = source_arr->size;

    if (new_size > source_arr->capacity) {
        size_t new_cap = new_size * 2;
        /* ReAlloc keeps the elements already there, so only the grown tail
           below needs writing. */
        double *new_data = csound->ReAlloc(csound, source_arr->data, sizeof(double) * new_cap * itype);
        if (new_data == NULL) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Internal error: memory allocation failed");
        }
        source_arr->data = new_data;
        source_arr->capacity = new_cap;
    }

    /* Growing past the old contents must read as zeros even when the buffer did
       not have to move: the spare capacity still holds whatever a larger
       earlier layout left in it. */
    if (new_size > old_size) {
        memset(source_arr->data + old_size * itype, 0, sizeof(double) * (new_size - old_size) * itype);
    }

    source_arr->size = new_size;
    return OK;
}

int32_t csnarray_resize_in(CSOUND *csound, CSN_RESIZE_IN *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;

    uint32_t ndim = 0;
    uint32_t shape[CSN_MAX_DIMS] = {0};
    size_t new_size = 0;

    int32_t res = resize_parse_shape_and_size(csound, NULL, &ndim, shape, p->new_shape, &new_size);
    if (res != OK) return res;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = slot->array;
    ITEM_TYPE itype = source_arr->itype;

    res = resize_in_assign_value(csound, NULL, &source_arr, new_size, itype);
    if (res != OK) goto done;

    set_csnarray_layout(source_arr, ndim, shape, new_size, itype);
    /* set_csnarray_layout moves the layout version; the payload changed too. */
    update_array_data_version(&source_arr->version);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}


static int32_t truncate_k_init_helper(CSOUND *csound, CSN_TRUNCATE *p, CSN_RESIZE_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = (uint32_t) p->source_handle->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = slot->array;
    size_t source_size = source_arr->size;
    uint32_t *source_shape = source_arr->shape;
    uint32_t source_ndim = source_arr->ndim;
    ITEM_TYPE itype = source_arr->itype;

    uint32_t new_ndim = source_ndim;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    memcpy(new_shape, source_shape, sizeof(uint32_t) * CSN_MAX_DIMS);

    uint32_t protect[1] = { source_handle };
    if (create_csnarray_locked(csound, reg, &p->h, new_ndim, new_shape, &p->array, p->handle, protect, 1U, &err, itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    reset_empty_csnarray(p->array, new_ndim, new_shape, itype);
    if (source_size > 0) {
        memcpy(p->array->data, source_arr->data, sizeof(double) * source_arr->size * itype);
        p->array->size = source_size;
    }

    SET_KDATA_BEGIN(p, reg);
    p->is_published = false;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_truncate_k_init(CSOUND *csound, CSN_TRUNCATE *p) {
    return truncate_k_init_helper(csound, p, CSN_TRUNCATE_ARR);
}

static int32_t csnarray_head_k_init(CSOUND *csound, CSN_TRUNCATE *p) {
    return truncate_k_init_helper(csound, p, CSN_HEAD_ARR);
}

static int32_t truncate_k_helper(CSOUND *csound, CSN_TRUNCATE *p, CSN_RESIZE_MODE mode) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REG_HANDLE(csound, &p->h, reg, p->k_data.owned_handle);

    uint32_t source_handle = (uint32_t) p->source_handle->id;

    if (!IS_VALID_LENGTH((double) *p->length)) {
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid new array length");
    }
    uint32_t new_length = (uint32_t) *p->length;

    int32_t res = OK;
    const char *err = NULL;

    CHECK_KTRIG((mode == CSN_HEAD_ARR) ? p->arg_a : p->arg_b);

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
    }

    CSN_ARRAY *source_arr = slot->array;
    size_t source_size = source_arr->size;
    uint32_t *source_shape = source_arr->shape;
    uint32_t source_ndim = source_arr->ndim;
    ITEM_TYPE itype = source_arr->itype;

    uint32_t new_ndim = source_ndim;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    memcpy(new_shape, source_shape, sizeof(uint32_t) * CSN_MAX_DIMS);


    int32_t axis = -1;
    res = truncate_assign_shape(csound, &p->h, source_shape, new_shape, source_ndim, source_size, (size_t) new_length, p->arg_a, &axis, mode);
    if (res != OK) goto done;

    if (p->is_published) {
        bool is_same_version = is_same_array_version(&p->k_data.prev_source_version, &source_arr->version);
        bool is_same_length = p->k_data.prev_size == new_length;
        bool is_same_axis = (int32_t) p->k_data.prev_axis == axis;
        if (is_same_version && is_same_length && is_same_axis) {
            p->handle->id = p->k_data.owned_handle;
            goto done;
        }
    }

    size_t req_size = 0;
    if (get_array_size_from_shape(&req_size, new_ndim, new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSN_ARRAY *arr = NULL;
    size_t logical_size = source_size == 0 ? 0 : req_size;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, new_ndim, new_shape, logical_size, itype, err);
    if (res != OK) goto done;
    p->array = arr;

    truncate_assign_value(p->array, source_arr, new_shape, new_ndim, axis);
    SET_KDATA_END(p, new_shape, new_ndim, itype);
    set_array_version(&p->k_data.prev_source_version, &source_arr->version);
    p->k_data.prev_axis = axis;
    p->k_data.prev_size = new_length;
    p->is_published = true;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_truncate_k(CSOUND *csound, CSN_TRUNCATE *p) {
    return truncate_k_helper(csound, p, CSN_TRUNCATE_ARR);
}

int32_t csnarray_head_k(CSOUND *csound, CSN_TRUNCATE *p) {
    return truncate_k_helper(csound, p, CSN_HEAD_ARR);
}

static int32_t csnarray_truncate_in_k_init(CSOUND *csound, CSN_TRUNCATE_IN *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    p->k_data.registry = reg;
    p->is_published = false;
    return OK;
}

int32_t csnarray_truncate_in_k(CSOUND *csound, CSN_TRUNCATE_IN *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    uint32_t source_handle = (uint32_t) p->source_handle->id;

    if (!IS_VALID_LENGTH((double) *p->length)) {
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid new array length");
    }
    uint32_t new_length = (uint32_t) *p->length;

    int32_t res = OK;

    CHECK_KTRIG(p->trig);

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
    }

    CSN_ARRAY *source_arr = slot->array;
    size_t source_size = source_arr->size;
    uint32_t *source_shape = source_arr->shape;
    uint32_t source_ndim = source_arr->ndim;

    uint32_t new_ndim = source_ndim;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    memcpy(new_shape, source_shape, sizeof(uint32_t) * CSN_MAX_DIMS);

    int32_t axis = -1;
    res = truncate_assign_shape(csound, &p->h, source_shape, new_shape, source_ndim, source_size, (size_t) new_length, p->axis, &axis, CSN_TRUNCATE_ARR);
    if (res != OK) goto done;

    if (p->is_published) {
        bool is_same_version = is_same_array_version(&p->k_data.prev_source_version, &source_arr->version);
        bool is_same_length = p->k_data.prev_size == new_length;
        bool is_same_axis = (int32_t) p->k_data.prev_axis == axis;
        if (is_same_version && is_same_length && is_same_axis) goto done;
    }

    res = truncate_assign_value_in(csound, &p->h, source_arr, new_ndim, new_shape, axis);
    if (res != OK) goto done;

    /* truncate_assign_value_in already moved the layout version; this adds the
       data generation and records it, so the next pass recognizes its own
       write instead of truncating again. */
    PUBLISH_INPLACE_WRITE(&p->k_data, source_handle, source_arr, false, false, false);
    p->k_data.prev_size = new_length;
    p->k_data.prev_axis = axis;
    p->is_published = true;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}


static int32_t csnarray_resize_k_init(CSOUND *csound, CSN_RESIZE *p) {
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

    CSN_ARRAY *source_arr = slot->array;
    if (create_csnarray_locked(csound, reg, &p->h, source_arr->ndim, source_arr->shape, &p->array, p->handle, &source_handle, 1U, &err, source_arr->itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    reset_empty_csnarray(p->array, source_arr->ndim, source_arr->shape, source_arr->itype);
    SET_KDATA_BEGIN(p, reg);
    p->is_published = false;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}


int32_t csnarray_resize_k(CSOUND *csound, CSN_RESIZE *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REG_HANDLE(csound, &p->h, reg, p->k_data.owned_handle);

    uint32_t source_handle = p->source_handle->id;

    CHECK_KTRIG(p->trig);

    uint32_t ndim = 0;
    uint32_t shape[CSN_MAX_DIMS] = {0};
    size_t new_size = 0;

    int32_t res = resize_parse_shape_and_size(csound, &p->h, &ndim, shape, p->new_shape, &new_size);
    if (res != OK) return res;

    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
    }

    CSN_ARRAY *source_arr = slot->array;

    if (p->is_published) {
        bool is_same_version = is_same_array_version(&p->k_data.prev_source_version, &source_arr->version);
        bool is_same_shape = memcmp(p->k_data.prev_shape, shape, sizeof(uint32_t) * CSN_MAX_DIMS) == 0;
        if (is_same_version && is_same_shape) goto done;
    }

    CSN_ARRAY *arr = NULL;
    size_t logical_size = source_arr->size == 0 ? 0 : new_size;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, ndim, shape, logical_size, source_arr->itype, err);
    if (res != OK) goto done;
    p->array = arr;

    resize_assign_value(arr, source_arr, new_size);
    SET_KDATA_END(p, shape, ndim, source_arr->itype);
    p->is_published = true;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_resize_in_k_init(CSOUND *csound, CSN_RESIZE_IN *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    p->k_data.registry = reg;
    p->is_published = false;
    return OK;
}

int32_t csnarray_resize_in_k(CSOUND *csound, CSN_RESIZE_IN *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    uint32_t source_handle = p->source_handle->id;

    uint32_t ndim = 0;
    uint32_t shape[CSN_MAX_DIMS] = {0};
    size_t new_size = 0;

    CHECK_KTRIG(p->trig);

    int32_t res = resize_parse_shape_and_size(csound, &p->h, &ndim, shape, p->new_shape, &new_size);
    if (res != OK) return res;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
    }

    CSN_ARRAY *source_arr = slot->array;
    ITEM_TYPE itype = source_arr->itype;

    if (p->is_published) {
        bool is_same_version = is_same_array_version(&p->k_data.prev_source_version, &source_arr->version);
        bool is_same_shape = memcmp(p->k_data.prev_shape, shape, sizeof(uint32_t) * CSN_MAX_DIMS) == 0;
        if (is_same_version && is_same_shape) goto done;
    }

    res = resize_in_assign_value(csound, &p->h, &source_arr, new_size, itype);
    if (res != OK) goto done;

    set_csnarray_layout(source_arr, ndim, shape, new_size, itype);
    memset(p->k_data.prev_shape, 0, sizeof(uint32_t) * CSN_MAX_DIMS);
    memcpy(p->k_data.prev_shape, shape, sizeof(uint32_t) * CSN_MAX_DIMS);
    PUBLISH_INPLACE_WRITE(&p->k_data, source_handle, source_arr, false, false, false);
    p->is_published = true;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_save(CSOUND *csound, CSN_SAVE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = (uint32_t) p->source_handle->id;

    if (p->path == NULL || p->path->data == NULL || p->path->size <= 0 || p->path->data[0] == '\0') {
        return csound->InitError(csound, "[csnarray] File path cannot be empty");
    }

    const char *dot = strrchr(p->path->data, '.');
    if (dot == NULL || strcmp(dot, CSN_FILE_EXT) != 0) {
        return csound->InitError(csound, "[csnarray] Invalid file extension: should be [%s]", CSN_FILE_EXT);
    }

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        return csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
    }
    CSN_ARRAY *arr = slot->array;

    const char *path = p->path->data;
    CSN_FILE_ERROR_CODE err_code = csnfile_save_array_to_file(arr, path);
    if (err_code != CSN_FILE_NO_ERROR) {
        const char *err_message = NULL;
        csnfile_dispatch_error(&err_message, err_code);
        return csound->InitError(csound, "[csnarray] %s", err_message);
    }

    return OK;
}

int32_t csnarray_load(CSOUND *csound, CSN_LOAD *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    if (p->path == NULL || p->path->data == NULL || p->path->size <= 0 || p->path->data[0] == '\0') {
        return csound->InitError(csound, "[csnarray] File path cannot be empty");
    }

    const char *dot = strrchr(p->path->data, '.');
    if (dot == NULL || strcmp(dot, CSN_FILE_EXT) != 0) {
        return csound->InitError(csound, "[csnarray] Invalid file extension: should be [%s]", CSN_FILE_EXT);
    }

    int32_t res = OK;
    const char *err = NULL;
    const char *path = p->path->data;

    double *buffer = csound->Calloc(csound, sizeof(double) * DEFAULT_TEMPORARY_BUFFER_SIZE);
    if (buffer == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
    }
    size_t buffer_capacity = DEFAULT_TEMPORARY_BUFFER_SIZE;

    CSN_FILE_HEADER header = {0};
    CSN_FILE_ERROR_CODE err_code = csnfile_load_array_from_file(csound, &header, &buffer, &buffer_capacity, path);
    if (err_code != CSN_FILE_NO_ERROR) {
        csound->Free(csound, buffer);
        const char *err_message = NULL;
        csnfile_dispatch_error(&err_message, err_code);
        return csound->InitError(csound, "[csnarray] %s", err_message);
    };

    csound->LockMutex(reg->mutex);
    if (create_csnarray_locked(csound, reg, &p->h, header.dim, header.shape, &p->array, p->handle, NULL, 0, &err, (ITEM_TYPE) header.dtype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;
    if (header.size > 0) {
        memcpy(arr->data, buffer, (size_t) header.data_bytes);
        update_array_data_version(&arr->version);
    }

done:
    csound->Free(csound, buffer);
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_save_k_deinit(CSOUND *csound, CSN_SAVE *p) {
    deinit_scratch(csound, &p->scratch);
    return OK;
}

static int32_t csnarray_save_k_init(CSOUND *csound, CSN_SAVE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = (uint32_t) p->source_handle->id;
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        return csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
    }
    CSN_ARRAY *arr = slot->array;

    char *path_buffer = csound->Malloc(csound, DEFAULT_TEMPORARY_BUFFER_SIZE);
    if (path_buffer == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
    }
    p->scratch.scratch = path_buffer;
    p->scratch.scratch_capacity = DEFAULT_TEMPORARY_BUFFER_SIZE;
    p->registry = reg;
    p->prev_source_version = arr->version;
    p->prev_array_id = 0;
    p->is_published = false;
    return OK;
}

int32_t csnarray_save_k(CSOUND *csound, CSN_SAVE *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    uint32_t source_handle = (uint32_t) p->source_handle->id;

    if (p->path == NULL || p->path->data == NULL || p->path->size <= 0 || p->path->data[0] == '\0') {
        return csound->PerfError(csound, &p->h, "[csnarray] File path cannot be empty");
    }

    const char *dot = strrchr(p->path->data, '.');
    if (dot == NULL || strcmp(dot, CSN_FILE_EXT) != 0) {
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid file extension: should be [%s]", CSN_FILE_EXT);
    }

    CHECK_KTRIG(p->trig);

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
    }

    const char *path = p->path->data;
    CSN_ARRAY *arr = slot->array;

    if (p->is_published) {
        bool is_same_array = is_same_array_version(&arr->version, &p->prev_source_version);
        bool is_same_path = strcmp(path, p->scratch.scratch) == 0;
        bool is_same_array_id = source_handle == p->prev_array_id;
        if (is_same_array && is_same_path && is_same_array_id) return OK;
    }

    CSN_FILE_ERROR_CODE err_code = csnfile_save_array_to_file(arr, path);
    if (err_code != CSN_FILE_NO_ERROR) {
        const char *err_message = NULL;
        csnfile_dispatch_error(&err_message, err_code);
        return csound->PerfError(csound, &p->h, "[csnarray] %s", err_message);
    }

    size_t path_len = strlen(path);
    if (path_len + 1 > p->scratch.scratch_capacity) {
        size_t new_path_cap = (path_len + 1) * 2;
        char *path_buffer = csound->ReAlloc(csound, p->scratch.scratch, sizeof(char) * new_path_cap);
        if (path_buffer == NULL) {
            return csound->PerfError(csound, &p->h, "[csnarray] Internal error: memory allocation failed");
        }
        p->scratch.scratch = path_buffer;
        p->scratch.scratch_capacity = new_path_cap;
    }

    snprintf((char *) p->scratch.scratch, path_len + 1, "%s", path);
    p->prev_array_id = source_handle;
    set_array_version(&p->prev_source_version, &arr->version);
    p->is_published = true;

    return OK;
}

static int32_t csnarray_load_k_init(CSOUND *csound, CSN_LOAD *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    const char *err = NULL;

    double *buffer = csound->Calloc(csound, sizeof(double) * DEFAULT_TEMPORARY_BUFFER_SIZE);
    if (buffer == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
    }
    p->buffer_scratch.scratch = buffer;
    p->buffer_scratch.scratch_capacity = DEFAULT_TEMPORARY_BUFFER_SIZE;

    char *path_buffer = csound->Malloc(csound, DEFAULT_TEMPORARY_BUFFER_SIZE);
    if (path_buffer == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
    }
    p->path_scratch.scratch = path_buffer;
    p->path_scratch.scratch_capacity = DEFAULT_TEMPORARY_BUFFER_SIZE;

    csound->LockMutex(reg->mutex);

    uint32_t shape[CSN_MAX_DIMS] = {0};
    shape[0] = 1U;

    if (create_csnarray_locked(csound, reg, &p->h, 1U, shape, &p->array, p->handle, NULL, 0, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    reset_empty_csnarray(p->array, 1U, shape, CSN_REAL);
    SET_KDATA_BEGIN(p, reg);
    p->is_published = false;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_load_k(CSOUND *csound, CSN_LOAD *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REG_HANDLE(csound, &p->h, reg, p->k_data.owned_handle);

    if (p->path == NULL || p->path->data == NULL || p->path->size <= 0 || p->path->data[0] == '\0') {
        return csound->PerfError(csound, &p->h, "[csnarray] File path cannot be empty");
    }

    const char *dot = strrchr(p->path->data, '.');
    if (dot == NULL || strcmp(dot, CSN_FILE_EXT) != 0) {
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid file extension: should be [%s]", CSN_FILE_EXT);
    }

    int32_t res = OK;
    const char *err = NULL;
    const char *path = p->path->data;

    /* No reload cache on purpose. csnload reads a file it does not own, so the
       path is not a usable key, and a stat stamp only narrows the window: on
       HFS+, SMB/NFS and FAT the mtime granularity is one to two seconds, wide
       enough for a same-size rewrite to hide in. The trigger is the contract
       and the only authority: it fires, we read. */
    CHECK_KTRIG(p->trig);

    double *buffer = (double *) p->buffer_scratch.scratch;
    size_t capacity = p->buffer_scratch.scratch_capacity;

    CSN_FILE_HEADER header = {0};
    CSN_FILE_ERROR_CODE err_code = csnfile_load_array_from_file(csound, &header, &buffer, &capacity, path);
    p->buffer_scratch.scratch = buffer;
    p->buffer_scratch.scratch_capacity = capacity;
    if (err_code != CSN_FILE_NO_ERROR) {
        const char *err_message = NULL;
        csnfile_dispatch_error(&err_message, err_code);
        return csound->PerfError(csound, &p->h, "[csnarray] %s", err_message);
    }

    csound->LockMutex(reg->mutex);
    size_t req_size = 0;
    if (get_array_size_from_shape(&req_size, header.dim, header.shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }
    ITEM_TYPE itype = (ITEM_TYPE) header.dtype;
    CSN_ARRAY *arr = NULL;
    size_t logical_size = req_size;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, header.dim, header.shape, logical_size, itype, err);
    if (res != OK) goto done;
    p->array = arr;

    if (header.size > 0) {
        memcpy(arr->data, buffer, (size_t) header.data_bytes);
        update_array_data_version(&arr->version);
    }

    size_t path_len = strlen(path);
    if (path_len + 1 > p->path_scratch.scratch_capacity) {
        size_t new_path_cap = (path_len + 1) * 2;
        char *path_buffer = csound->ReAlloc(csound, p->path_scratch.scratch, sizeof(char) * new_path_cap);
        if (path_buffer == NULL) {
            csound->UnlockMutex(reg->mutex);
            return csound->PerfError(csound, &p->h, "[csnarray] Internal error: memory allocation failed");
        }
        p->path_scratch.scratch = path_buffer;
        p->path_scratch.scratch_capacity = new_path_cap;
    }
    snprintf((char *) p->path_scratch.scratch, path_len + 1, "%s", path);

    SET_KDATA_END(p, arr->shape, arr->ndim, arr->itype);
    p->is_published = true;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_show(CSOUND *csound, CSN_SHOW *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = (uint32_t) p->source_handle->id;
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        return csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
    }
    CSN_ARRAY *arr = slot->array;
    CSN_PRINT_BUFFER pbuffer = {0};
    char *data = csound->Malloc(csound, CSN_PRINT_BUFFER_INITIAL_CAPACITY);
    if (data == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
    }
    pbuffer.data = data;
    pbuffer.length = 0;
    pbuffer.capacity = CSN_PRINT_BUFFER_INITIAL_CAPACITY;

    int32_t res = csnfile_show_array(csound, &pbuffer, arr);
    if (res == OK) {
        csound->Message(csound, "%s", pbuffer.data);
    }
    csound->Free(csound, pbuffer.data);

    if (res != OK) {
        return csound->InitError(csound, "[csnarray] Internal error: wrong print buffer allocation");
    }
    return res;
}

static int32_t csnarray_show_k_deinit(CSOUND *csound, CSN_SHOW *p) {
    if (p->pbuffer.data != NULL) {
        csound->Free(csound, p->pbuffer.data);
        p->pbuffer.data = NULL;
        p->pbuffer.length = 0;
        p->pbuffer.capacity = 0;
    }

    return OK;
}

static int32_t csnarray_show_k_init(CSOUND *csound, CSN_SHOW *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    p->pbuffer.data = NULL;
    p->pbuffer.length = 0;
    p->pbuffer.capacity = 0;

    char *data = csound->Malloc(csound, CSN_PRINT_BUFFER_INITIAL_CAPACITY);
    if (data == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
    }

    p->pbuffer.data = data;
    p->pbuffer.length = 0;
    p->pbuffer.capacity = CSN_PRINT_BUFFER_INITIAL_CAPACITY;
    p->registry = reg;

    return OK;
}

int32_t csnarray_show_k(CSOUND *csound, CSN_SHOW *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    CHECK_KTRIG(p->trig);

    uint32_t source_handle = (uint32_t) p->source_handle->id;
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
    }

    CSN_ARRAY *arr = slot->array;

    if (csnfile_show_array(csound, &p->pbuffer, arr) != OK) {
        return csound->PerfError(csound, &p->h, "[csnarray] Internal error: wrong print buffer allocation");
    }

    csound->Message(csound, "%s", p->pbuffer.data);
    return OK;
}

static int32_t csnarray_from_audio_init(CSOUND *csound, CSN_FROM_AUDIO *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    if (!IS_VALID_ZERO_ONE((double) *p->rt_lock)) {
        return csound->InitError(csound, "[csnarray] Resize param must be 0 = realloc at perf-time or 1 = do not realloc at perf-time");
    }

    uint32_t shape[CSN_MAX_DIMS] = {0};
    shape[0] = CS_KSMPS;

    int32_t res = create_csnarray_init(csound, &p->h, 1U, shape, &p->array, p->handle, CSN_REAL);
    if (res != OK) return res;

    csound->LockMutex(reg->mutex);
    bool is_locked = (*p->rt_lock != 0.0);
    CSN_SLOT *slot = get_slot(reg, p->handle->id);
    if (slot != NULL) slot->rt_locked = is_locked;
    fill_csnarray(p->array, 0.0);
    SET_KDATA_WITH_ID_BEGIN(p, reg, shape, 1U, CSN_REAL, p->handle->id);
    csound->UnlockMutex(reg->mutex);
    return OK;
}

int32_t csnarray_from_audio(CSOUND *csound, CSN_FROM_AUDIO *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    uint32_t source_handle = p->k_data.owned_handle;
    CHECK_REG_HANDLE(csound, &p->h, reg, source_handle);

    uint32_t nsamples = CS_KSMPS;
    uint32_t offset = p->h.insdshead->ksmps_offset;
    uint32_t early = p->h.insdshead->ksmps_no_end;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
    }

    p->array = slot->array;
    CSN_ARRAY *buffer = p->array;
    if (buffer->size != CS_KSMPS || buffer->ndim != 1U || buffer->itype != CSN_REAL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid slot: array size and dimension mismatch");
    }

    if (offset) memset(buffer->data, 0, sizeof(double) * offset);
    if (early) {
        nsamples -= early;
        memset(buffer->data + nsamples, 0, sizeof(double) * early);
    }

    for (uint32_t i = offset; i < nsamples; i++) {
        buffer->data[i] = (double) p->source_sig[i];
    }

    update_array_data_version(&buffer->version);
    SET_KDATA_END(p, buffer->shape, 1U, CSN_REAL);
    csound->UnlockMutex(reg->mutex);

    return OK;
}

static int32_t csnarray_to_audio_init(CSOUND *csound, CSN_TO_AUDIO *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = (uint32_t) p->source_handle->id;
    uint32_t nsamples = CS_KSMPS;
    uint32_t shape[CSN_MAX_DIMS] = {0};
    shape[0] = nsamples;

    int32_t res = OK;

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
        goto done;
    }

    CSN_ARRAY *buffer = slot->array;
    if (buffer->size != CS_KSMPS || buffer->itype != CSN_REAL) {
        res = csound->InitError(csound, "[csnarray] Invalid csn-audio buffer: size/dim/dtype mismatch");
        goto done;
    }

    p->registry = reg;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_to_audio(CSOUND *csound, CSN_TO_AUDIO *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    uint32_t source_handle = (uint32_t) p->source_handle->id;

    uint32_t nsamples = CS_KSMPS;
    uint32_t offset = p->h.insdshead->ksmps_offset;
    uint32_t early = p->h.insdshead->ksmps_no_end;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
    }

    CSN_ARRAY *buffer = slot->array;
    if (buffer->size != CS_KSMPS || buffer->itype != CSN_REAL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid csn-audio buffer: size/dim/dtype mismatch");
    }

    if (offset) memset(p->sig, 0, sizeof(MYFLT) * offset);
    if (early) {
        nsamples -= early;
        memset(p->sig + nsamples, 0, sizeof(MYFLT) * early);
    }

    for (uint32_t i = offset; i < nsamples; i++) {
        p->sig[i] = (MYFLT) buffer->data[i];
    }

    csound->UnlockMutex(reg->mutex);
    return OK;
}

int32_t csnarray_pack_audio_init(CSOUND *csound, CSN_PACK_AUDIO *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    if (p->source_sig == NULL || p->source_sig->sizes == NULL || p->source_sig->sizes[0] == 0) {
        return csound->InitError(csound, "[csnarray] Empty audio array");
    }

    if (!IS_VALID_ZERO_ONE((double) *p->rt_lock)) {
        return csound->InitError(csound, "[csnarray] Resize param must be 0 = realloc at perf-time or 1 = do not realloc at perf-time");
    }

    uint32_t nchnls = (uint32_t) p->source_sig->sizes[0];

    uint32_t shape[CSN_MAX_DIMS] = {0};
    shape[0] = nchnls;
    shape[1] = CS_KSMPS;

    int32_t res = create_csnarray_init(csound, &p->h, 2U, shape, &p->array, p->handle, CSN_REAL);
    if (res != OK) return res;

    csound->LockMutex(reg->mutex);
    bool is_locked = (*p->rt_lock != 0.0);
    CSN_SLOT *slot = get_slot(reg, p->handle->id);
    if (slot != NULL) slot->rt_locked = is_locked;
    fill_csnarray(p->array, 0.0);
    SET_KDATA_WITH_ID_BEGIN(p, reg, shape, 2U, CSN_REAL, p->handle->id);
    p->prev_nchnls = nchnls;
    csound->UnlockMutex(reg->mutex);
    return OK;
}

int32_t csnarray_pack_audio(CSOUND *csound, CSN_PACK_AUDIO *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    uint32_t source_handle = p->k_data.owned_handle;
    CHECK_REG_HANDLE(csound, &p->h, reg, source_handle);

    uint32_t nsamples = CS_KSMPS;
    uint32_t offset = p->h.insdshead->ksmps_offset;
    uint32_t early = p->h.insdshead->ksmps_no_end;
    uint32_t end_offset = nsamples - early;

    if (p->source_sig == NULL || p->source_sig->sizes == NULL || p->source_sig->sizes[0] == 0) {
        return csound->PerfError(csound, &p->h, "[csnarray] Empty audio array");
    }

    uint32_t nchnls = (uint32_t) p->source_sig->sizes[0];
    if (nchnls != p->prev_nchnls) {
        return csound->PerfError(csound, &p->h, "[csnarray] Shape change not allowed on audio path");
    }

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
    }

    CSN_ARRAY *buffer = slot->array;
    for (size_t i = 0; i < (size_t) nchnls; i++) {
        MYFLT *channel = p->source_sig->data + i * CS_KSMPS;
        double *row = buffer->data + i * CS_KSMPS;
        for (size_t j = 0; j < nsamples; j++) {
            row[j] = (j < offset || j >= end_offset) ? 0.0 : (double) channel[j];
        }
    }

    update_array_data_version(&buffer->version);
    SET_KDATA_END(p, buffer->shape, 2U, CSN_REAL);

    csound->UnlockMutex(reg->mutex);
    return OK;
}


int32_t csnarray_unpack_audio_init(CSOUND *csound, CSN_UNPACK_AUDIO *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = (uint32_t) p->source_handle->id;
    int32_t res = OK;

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
        goto done;
    }

    CSN_ARRAY *buffer = slot->array;
    if (buffer->ndim != 2U || buffer->itype != CSN_REAL || buffer->shape[1] != CS_KSMPS) {
        res = csound->InitError(csound, "[csnarray] Invalid csn-audio buffer: size/dim/dtype mismatch");
        goto done;
    }

    int32_t nchnls = (int32_t) buffer->shape[0];
    if (tabinit(csound, p->sig, nchnls, p->h.insdshead) != OK) {
        res = csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
        goto done;
    }

    p->registry = reg;
    p->prev_nchnls = buffer->shape[0];

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_unpack_audio(CSOUND *csound, CSN_UNPACK_AUDIO *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    uint32_t source_handle = (uint32_t) p->source_handle->id;

    uint32_t nsamples = CS_KSMPS;
    uint32_t offset = p->h.insdshead->ksmps_offset;
    uint32_t early = p->h.insdshead->ksmps_no_end;
    uint32_t end_offset = nsamples - early;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
    }

    CSN_ARRAY *buffer = slot->array;
    if (buffer->ndim != 2U || buffer->itype != CSN_REAL || buffer->shape[1] != CS_KSMPS) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid csn-audio buffer: size/dim/dtype mismatch");
    }

    uint32_t nchnls = buffer->shape[0];
    if (nchnls != p->prev_nchnls) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Shape change not allowed on audio path");
    }

    for (uint32_t i = 0; i < nchnls; i++) {
        MYFLT *channel_out = p->sig->data + i * CS_KSMPS;
        double *channel_in = buffer->data + i * CS_KSMPS;
        for (uint32_t j = 0; j < nsamples; j++) {
            channel_out[j] = (j < offset || j >= end_offset) ? FL(0.0) : (MYFLT) channel_in[j];
        }
    }

    csound->UnlockMutex(reg->mutex);
    return OK;
}

static int32_t csnarray_frame_audio_init(CSOUND *csound, CSN_FRAME_AUDIO *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    if (!IS_VALID_ZERO_ONE((double) *p->rt_lock)) {
        return csound->InitError(csound, "[csnarray] Resize param must be 0 = realloc at perf-time or 1 = do not realloc at perf-time");
    }

    int32_t res = OK;
    const char *err = NULL;

    double frame_size_temp = (double) *p->frame_size;
    if (!IS_VALID_LENGTH(frame_size_temp)) {
        return csound->InitError(csound, "[csnarray] Invalid frame size");
    }
    if (frame_size_temp == 0.0) {
        return csound->InitError(csound, "[csnarray] Zero-length frame size");
    }

    double hop_size_temp = (double) *p->hop_size;
    if (!IS_VALID_LENGTH(hop_size_temp)) {
        return csound->InitError(csound, "[csnarray] Invalid hop size");
    }
    hop_size_temp = hop_size_temp == 0.0 ? frame_size_temp : hop_size_temp;

    if (hop_size_temp < CS_KSMPS) {
        return csound->InitError(csound, "[csnarray] Hop size must be greater or equal to ksmps");
    }

    if (hop_size_temp > frame_size_temp) {
        return csound->InitError(csound, "[csnarray] Hop size must be less or equal to frame size");
    }


    p->fsize = (size_t) frame_size_temp;
    p->hsize = (size_t) hop_size_temp;

    size_t buffer_cap = p->fsize * 2;
    double *buffer_temp = csound->Calloc(csound, sizeof(double) * buffer_cap);
    if (buffer_temp == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
    }

    p->buffer.scratch = buffer_temp;
    p->buffer.scratch_capacity = buffer_cap;
    p->buffer.current_size = 0;
    p->buffer.reader = 0;
    p->buffer.writer = 0;
    p->buffer.sample_count = 0;

    *p->is_ready = FL(0.0);

    csound->LockMutex(reg->mutex);
    uint32_t shape[CSN_MAX_DIMS] = {0};
    shape[0] = (uint32_t) p->fsize;
    if (create_csnarray_locked(csound, reg, &p->h, 1U, shape, &p->array, p->handle, NULL, 0, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    bool is_locked = (*p->rt_lock != 0.0);
    CSN_SLOT *slot = get_slot(reg, p->handle->id);
    if (slot != NULL) slot->rt_locked = is_locked;
    fill_csnarray(p->array, 0.0);
    SET_KDATA_WITH_ID_BEGIN(p, reg, shape, 1U, CSN_REAL, p->handle->id);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_frame_audio(CSOUND *csound, CSN_FRAME_AUDIO *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    uint32_t source_handle = p->k_data.owned_handle;
    CHECK_REG_HANDLE(csound, &p->h, reg, source_handle);

    uint32_t nsamples = CS_KSMPS;
    uint32_t offset = p->h.insdshead->ksmps_offset;
    uint32_t early = p->h.insdshead->ksmps_no_end;
    uint32_t end_offset = nsamples - early;

    size_t fsize = p->fsize;
    size_t hsize = p->hsize;
    CSN_SCRATCH *buffer = &p->buffer;
    double *cbuffer = (double *) buffer->scratch;

    // writer
    for (uint32_t i = 0; i < nsamples; i++) {
        size_t index = buffer->writer;
        cbuffer[index] = (i < offset || i >= end_offset) ? 0.0 : (double) p->source_sig[i];
        buffer->writer = (buffer->writer + 1U) % buffer->scratch_capacity;
        if (buffer->current_size < buffer->scratch_capacity) {
            buffer->current_size++;
        }
    }

    if (buffer->current_size >= fsize) {
        csound->LockMutex(reg->mutex);
        CSN_SLOT *slot = get_slot(reg, source_handle);
        if (slot == NULL) {
            csound->UnlockMutex(reg->mutex);
            return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
        }

        CSN_ARRAY *frame = p->array;
        // reader
        for (size_t i = 0; i < fsize; i++) {
            size_t index = (buffer->reader + i) % buffer->scratch_capacity;
            frame->data[i] = cbuffer[index];
        }
        buffer->reader = (buffer->reader + hsize) % buffer->scratch_capacity;
        buffer->current_size -= hsize;

        update_array_data_version(&frame->version);
        *p->is_ready = FL(1.0);
        csound->UnlockMutex(reg->mutex);
    } else {
        *p->is_ready = FL(0.0);
    }

    p->handle->id = p->k_data.owned_handle;
    return OK;
}

static int32_t csnarray_ola_audio_deinit(CSOUND *csound, CSN_OLA_AUDIO *p) {
    deinit_scratch(csound, &p->buffer);
    return OK;
}

static int32_t csnarray_ola_audio_init(CSOUND *csound, CSN_OLA_AUDIO *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    double hop_size_temp = (double) *p->hop_size;
    if (!IS_VALID_LENGTH(hop_size_temp)) {
        return csound->InitError(csound, "[csnarray] Invalid hop size");
    }

    if (hop_size_temp < CS_KSMPS) {
        return csound->InitError(csound, "[csnarray] Hop size %d must be greater or equal to ksmps %d", (int32_t) hop_size_temp, (int32_t) CS_KSMPS);
    }

    int32_t res = OK;
    uint32_t source_handle = (uint32_t) p->source_handle->id;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
        goto done;
    }

    CSN_ARRAY *frame = slot->array;
    if (frame->ndim != 1U || frame->itype != CSN_REAL || frame->size == 0) {
        res = csound->InitError(csound, "[csnarray] Invalid csn-audio frame: expected a non-empty 1-D real array");
        goto done;
    }

    p->fsize = frame->size;
    p->hsize = (size_t) hop_size_temp;

    if (p->hsize > p->fsize) {
        res = csound->InitError(csound, "[csnarray] Hop size %zu must be less or equal to the frame size %zu", p->hsize, p->fsize);
        goto done;
    }

    size_t buffer_cap = p->fsize * 2;
    double *buffer_temp = csound->Calloc(csound, sizeof(double) * buffer_cap);
    if (buffer_temp == NULL) {
        res = csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
        goto done;
    }

    p->buffer.scratch = buffer_temp;
    p->buffer.scratch_capacity = buffer_cap;
    p->buffer.current_size = 0;
    p->buffer.reader = 0;
    p->buffer.writer = 0;
    p->buffer.sample_count = 0;

    p->prev_handle = source_handle;
    set_array_version(&frame->version, &p->prev_version);

    p->phase = 0;
    *p->is_ready = FL(0.0);
    p->registry = reg;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_ola_audio(CSOUND *csound, CSN_OLA_AUDIO *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    uint32_t source_handle = (uint32_t) p->source_handle->id;

    uint32_t nsamples = CS_KSMPS;
    uint32_t offset = p->h.insdshead->ksmps_offset;
    uint32_t early = p->h.insdshead->ksmps_no_end;
    uint32_t end_offset = nsamples - early;

    size_t fsize = p->fsize;
    size_t hsize = p->hsize;
    CSN_SCRATCH *buffer = &p->buffer;
    double *acc = (double *) buffer->scratch;
    size_t capacity = buffer->scratch_capacity;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
    }

    CSN_ARRAY *frame = slot->array;
    if (frame->ndim != 1U || frame->itype != CSN_REAL || frame->size != fsize) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Frame layout changed since init: expected a 1-D real array of %zu elements", fsize);
    }

    bool is_same_version = is_same_array_data_version(&frame->version, &p->prev_version);
    bool is_same = p->prev_handle == source_handle && is_same_version;

    p->phase += nsamples;
    bool is_consumed = p->phase >= hsize;
    if (is_consumed) p->phase -= hsize;

    if (is_consumed && !is_same) {
        for (size_t i = 0; i < fsize; i++) {
            acc[(buffer->writer + i) % capacity] += frame->data[i];
        }
        buffer->writer = (buffer->writer + hsize) % capacity;
        buffer->current_size += hsize;

        p->prev_handle = source_handle;
        set_array_version(&p->prev_version, &frame->version);
    }

    csound->UnlockMutex(reg->mutex);

    if (buffer->current_size >= (size_t) nsamples) {
        for (uint32_t j = 0; j < nsamples; j++) {
            size_t index = (buffer->reader + j) % capacity;
            p->sig[j] = (j < offset || j >= end_offset) ? FL(0.0) : (MYFLT) acc[index];
            acc[index] = 0.0;
        }
        buffer->reader = (buffer->reader + nsamples) % capacity;
        buffer->current_size -= nsamples;
        *p->is_ready = FL(1.0);
    } else {
        memset(p->sig, 0, sizeof(MYFLT) * nsamples);
        *p->is_ready = FL(0.0);
    }

    return OK;
}

int32_t csnarray_set_rtlock(CSOUND *csound, CSN_RTLOCK *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;

    if (!IS_VALID_ZERO_ONE((double) *p->rt_lock)) {
        return csound->InitError(csound, "[csnarray] rtlock param must be 0/1");
    }

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
    }
    slot->rt_locked = (*p->rt_lock) != 0.0;

    csound->UnlockMutex(reg->mutex);
    return OK;
}

static int32_t where_body(CSOUND *csound, OPDS *perf_h, CSN_REGISTRY *reg, CSN_ARRAY **source_array, CSN_ARRAY **true_array, CSN_ARRAY **false_array, double *false_scalar, CSNREF *shandle, CSNREF *thandle, CSNREF *fhandle) {
    uint32_t source_handle = shandle->id;
    uint32_t true_handle = thandle->id;

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
    }
    CSN_ARRAY *source_arr = source_slot->array;

    CSN_SLOT *true_slot = get_slot(reg, true_handle);
    if (true_slot == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) true_handle);
    }
    CSN_ARRAY *true_arr = true_slot->array;
    bool is_same_first_shape = memcmp(source_arr->shape, true_arr->shape, sizeof(true_arr->shape)) == 0;
    if (!is_same_first_shape || source_arr->ndim != true_arr->ndim) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] source array and true array replacement must have same shape and dim");
    }

    CSN_ARRAY *false_arr = NULL;
    if (fhandle != NULL) {
        uint32_t false_handle = fhandle->id;
        CSN_SLOT *false_slot = get_slot(reg, false_handle);
        if (false_slot == NULL) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", false_handle);
        }
        false_arr = false_slot->array;
        bool is_same_shape = memcmp(true_arr->shape, false_arr->shape, sizeof(true_arr->shape)) == 0;
        if (!is_same_shape || true_arr->ndim != false_arr->ndim) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] true array and false array replacement must have same shape and dim");
        }
    }

    if (source_arr->itype != CSN_REAL || true_arr->itype != CSN_REAL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] This operation is not implemented for complex arrays");
    }

    if (false_arr != NULL && false_arr->itype != CSN_REAL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] This operation is not implemented for complex arrays");
    }

    if (false_scalar != NULL && !IS_VALID_VALUE(*false_scalar)) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Invalid false scalar to put");
    }

    *source_array = source_slot->array;
    *true_array = true_slot->array;
    if (false_arr != NULL) *false_array = false_arr;
    return OK;
}

static int32_t csnarray_where_helper(CSOUND *csound, OPDS *h, CSNREF *source_handle, CSNREF *true_handle, CSNREF *false_handle, CSN_ARRAY **p_array, CSNREF *handle, double *false_scalar) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);
    CSN_ARRAY *source_arr = NULL;
    CSN_ARRAY *true_arr = NULL;
    CSN_ARRAY *false_arr = NULL;
    res = where_body(csound, NULL, reg, &source_arr, &true_arr, &false_arr, false_scalar, source_handle, true_handle, false_handle);
    if (res != OK) goto done;

    uint32_t new_dim = source_arr->ndim;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    memcpy(new_shape, source_arr->shape, sizeof(uint32_t) * CSN_MAX_DIMS);

    uint32_t fhandle = false_handle == NULL ? 0 : false_handle->id;
    uint32_t n_protect = false_handle == NULL ? 2 : 3;
    uint32_t protect[3] = { source_handle->id, true_handle->id, fhandle };
    if (create_csnarray_locked(csound, reg, h, new_dim, new_shape, p_array, handle, protect, n_protect, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = *p_array;
    size_t size = arr->size;
    for (size_t i = 0; i < size; i++) {
        double mask_value = source_arr->data[i];
        if (false_scalar != NULL) {
            arr->data[i] = mask_value == 0.0 ? *false_scalar : true_arr->data[i];
        } else {
            arr->data[i] = mask_value == 0.0 ? false_arr->data[i] : true_arr->data[i];
        }
    }
    *p_array = arr;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_where_hh(CSOUND *csound, CSN_WHERE_HH *p) {
    return csnarray_where_helper(csound, &p->h, p->source_handle, p->source_handle_true, p->source_handle_false, &p->array, p->handle, NULL);
}

int32_t csnarray_where_hs(CSOUND *csound, CSN_WHERE_HS *p) {
    double scalar_false = (double) *p->source_scalar_false;
    return csnarray_where_helper(csound, &p->h, p->source_handle, p->source_handle_true, NULL, &p->array, p->handle, &scalar_false);
}

static int32_t csnarray_where_k_init_helper(CSOUND *csound, OPDS *h, CSNREF *source_handle, CSNREF *true_handle, CSNREF *false_handle, CSN_ARRAY **p_array, CSNREF *handle, double *false_scalar, K_DATA *k_data, bool *is_published) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);
    CSN_ARRAY *source_arr = NULL;
    CSN_ARRAY *true_arr = NULL;
    CSN_ARRAY *false_arr = NULL;
    res = where_body(csound, NULL, reg, &source_arr, &true_arr, &false_arr, false_scalar, source_handle, true_handle, false_handle);
    if (res != OK) goto done;

    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    uint32_t fhandle = false_handle == NULL ? 0 : false_handle->id;
    uint32_t n_protect = false_handle == NULL ? 2 : 3;
    uint32_t protect[3] = { source_handle->id, true_handle->id, fhandle };
    if (create_csnarray_locked(csound, reg, h, source_ndim, source_shape, p_array, handle, protect, n_protect, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    reset_empty_csnarray(*p_array, source_ndim, source_shape, CSN_REAL);

    memset(k_data->prev_shape, 0, sizeof(k_data->prev_shape));
    memcpy(k_data->prev_shape, source_shape, sizeof(k_data->prev_shape));
    k_data->prev_ndim = source_ndim;
    k_data->prev_itype = CSN_REAL;
    k_data->owned_handle = handle->id;
    k_data->registry = reg;
    set_array_version(&k_data->prev_output_version, &(*p_array)->version);
    *is_published = false;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_where_k_helper(CSOUND *csound, OPDS *h, CSNREF *source_handle, CSNREF *true_handle, CSNREF *false_handle, CSN_ARRAY **p_array, CSNREF *handle, double *false_scalar, K_DATA *k_data, bool *is_published, CSN_WHERE_VERSION_K_STATE *versions, const MYFLT *trig) {
    CSN_REGISTRY *reg = k_data->registry;
    CHECK_REG_HANDLE(csound, h, reg, k_data->owned_handle);

    int32_t res = OK;
    const char *err = NULL;

    res = CHECK_SELF_ALIAS(csound, h, k_data, source_handle->id, true_handle->id);
    if (res != OK) return res;
    if (false_handle != NULL) {
        res = CHECK_SELF_ALIAS(csound, h, k_data, false_handle->id, 0);
        if (res != OK) return res;
    }

    CHECK_KTRIG(trig);

    csound->LockMutex(reg->mutex);
    CSN_ARRAY *source_arr = NULL;
    CSN_ARRAY *true_arr = NULL;
    CSN_ARRAY *false_arr = NULL;
    res = where_body(csound, h, reg, &source_arr, &true_arr, &false_arr, false_scalar, source_handle, true_handle, false_handle);
    if (res != OK) goto done;

    if (*is_published) {
        bool is_same_source_version = is_same_array_version(&versions->prev_a_version, &source_arr->version);
        bool is_same_true_version = is_same_array_version(&versions->prev_b_version, &true_arr->version);
        bool is_same_false = false;
        if (false_handle == NULL && false_scalar != NULL) {
            is_same_false = k_data->prev_scalar_param == *false_scalar;
        } else {
            is_same_false = is_same_array_version(&versions->prev_c_version, &false_arr->version);
        }

        CSN_SLOT *out_slot = get_slot(reg, k_data->owned_handle);
        bool is_same_result = out_slot != NULL && is_same_array_version(&k_data->prev_output_version, &out_slot->array->version);

        if (is_same_source_version && is_same_true_version && is_same_false && is_same_result) {
            handle->id = k_data->owned_handle;
            goto done;
        }
    }

    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    size_t req_size = 0;
    if (get_array_size_from_shape(&req_size, source_ndim, source_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSN_ARRAY *arr = NULL;
    size_t logical_size = source_arr->size == 0 ? 0 : req_size;
    res = NEED_TO_UPDATE_SLOT(csound, h, &arr, k_data, NULL, source_ndim, source_shape, logical_size, CSN_REAL, err);
    if (res != OK) {
        csound->UnlockMutex(reg->mutex);
        return res;
    }

    size_t size = arr->size;
    for (size_t i = 0; i < size; i++) {
        double mask_value = source_arr->data[i];
        if (false_scalar != NULL) {
            arr->data[i] = mask_value == 0.0 ? *false_scalar : true_arr->data[i];
        } else {
            arr->data[i] = mask_value == 0.0 ? false_arr->data[i] : true_arr->data[i];
        }
    }
    update_array_data_version(&arr->version);
    *p_array = arr;

    memset(k_data->prev_shape, 0, sizeof(k_data->prev_shape));
    memcpy(k_data->prev_shape, source_shape, sizeof(k_data->prev_shape));
    k_data->prev_ndim = source_ndim;
    handle->id = k_data->owned_handle;
    set_array_version(&k_data->prev_output_version, &(*p_array)->version);
    set_array_version(&versions->prev_a_version, &source_arr->version);
    set_array_version(&versions->prev_b_version, &true_arr->version);
    if (false_handle != NULL) {
        set_array_version(&versions->prev_c_version, &false_arr->version);
    } else {
        k_data->prev_scalar_param = *false_scalar;
    }
    *is_published = true;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_where_hh_k_init(CSOUND *csound, CSN_WHERE_HH *p) {
    return csnarray_where_k_init_helper(csound, &p->h, p->source_handle, p->source_handle_true, p->source_handle_false, &p->array, p->handle, NULL, &p->k_data, &p->is_published);
}

static int32_t csnarray_where_hs_k_init(CSOUND *csound, CSN_WHERE_HS *p) {
    double scalar_false = (double) *p->source_scalar_false;
    return csnarray_where_k_init_helper(csound, &p->h, p->source_handle, p->source_handle_true, NULL, &p->array, p->handle, &scalar_false, &p->k_data, &p->is_published);
}

int32_t csnarray_where_hh_k(CSOUND *csound, CSN_WHERE_HH *p) {
    return csnarray_where_k_helper(csound, &p->h, p->source_handle, p->source_handle_true, p->source_handle_false, &p->array, p->handle, NULL, &p->k_data, &p->is_published, &p->versions, p->trig);
}

int32_t csnarray_where_hs_k(CSOUND *csound, CSN_WHERE_HS *p) {
    double scalar_false = (double) *p->source_scalar_false;
    return csnarray_where_k_helper(csound, &p->h, p->source_handle, p->source_handle_true, NULL, &p->array, p->handle, &scalar_false, &p->k_data, &p->is_published, &p->versions, p->trig);
}

static int32_t csnarray_where_in_helper(CSOUND *csound, CSNREF *source_handle, CSNREF *true_handle, CSNREF *false_handle, double *false_scalar) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    csound->LockMutex(reg->mutex);
    CSN_ARRAY *source_arr = NULL;
    CSN_ARRAY *true_arr = NULL;
    CSN_ARRAY *false_arr = NULL;
    int32_t res = where_body(csound, NULL, reg, &source_arr, &true_arr, &false_arr, false_scalar, source_handle, true_handle, false_handle);
    if (res != OK) {
        csound->UnlockMutex(reg->mutex);
        return res;
    }

    size_t size = source_arr->size;
    for (size_t i = 0; i < size; i++) {
        double mask_value = source_arr->data[i];
        if (false_scalar != NULL) {
            source_arr->data[i] = mask_value == 0.0 ? *false_scalar : true_arr->data[i];
        } else {
            source_arr->data[i] = mask_value == 0.0 ? false_arr->data[i] : true_arr->data[i];
        }
    }
    update_array_data_version(&source_arr->version);

    csound->UnlockMutex(reg->mutex);
    return OK;
}

int32_t csnarray_where_in_hh(CSOUND *csound, CSN_WHERE_HH_IN *p) {
    return csnarray_where_in_helper(csound, p->source_handle, p->source_handle_true, p->source_handle_false, NULL);
}

int32_t csnarray_where_in_hs(CSOUND *csound, CSN_WHERE_HS_IN *p) {
    double scalar_false = (double) *p->source_scalar_false;
    return csnarray_where_in_helper(csound, p->source_handle, p->source_handle_true, NULL, &scalar_false);
}

static int32_t csnarray_where_k_in_init_helper(CSOUND *csound, CSNREF *source_handle, CSNREF *true_handle, CSNREF *false_handle, double *false_scalar, CSN_REGISTRY **registry, bool *is_published) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    CSN_ARRAY *true_arr = NULL;
    CSN_ARRAY *false_arr = NULL;
    int32_t res = where_body(csound, NULL, reg, &source_arr, &true_arr, &false_arr, false_scalar, source_handle, true_handle, false_handle);
    if (res != OK) {
        csound->UnlockMutex(reg->mutex);
        return res;
    }
    *registry = reg;
    *is_published = false;

    csound->UnlockMutex(reg->mutex);
    return OK;
}

static int32_t csnarray_where_k_in_helper(CSOUND *csound, OPDS *h, CSNREF *source_handle, CSNREF *true_handle, CSNREF *false_handle, double *false_scalar, CSN_REGISTRY *registry, bool *is_published, CSN_WHERE_VERSION_K_STATE *versions, const MYFLT *trig, double *prev_scalar_false) {
    CSN_REGISTRY *reg = registry;
    CHECK_REGISTRY(csound, h, reg);

    int32_t res = OK;
    CHECK_KTRIG(trig);

    csound->LockMutex(reg->mutex);
    CSN_ARRAY *source_arr = NULL;
    CSN_ARRAY *true_arr = NULL;
    CSN_ARRAY *false_arr = NULL;
    res = where_body(csound, h, reg, &source_arr, &true_arr, &false_arr, false_scalar, source_handle, true_handle, false_handle);
    if (res != OK) goto done;

    if (*is_published) {
        bool is_same_source_version = is_same_array_version(&versions->prev_a_version, &source_arr->version);
        bool is_same_true_version = is_same_array_version(&versions->prev_b_version, &true_arr->version);
        bool is_same_false = false;
        if (false_handle == NULL && false_scalar != NULL) {
            is_same_false = *prev_scalar_false == *false_scalar;
        } else {
            is_same_false = is_same_array_version(&versions->prev_c_version, &false_arr->version);
        }

        if (is_same_source_version && is_same_true_version && is_same_false) {
            goto done;
        }
    }

    size_t size = source_arr->size;
    for (size_t i = 0; i < size; i++) {
        double mask_value = source_arr->data[i];
        if (false_scalar != NULL) {
            source_arr->data[i] = mask_value == 0.0 ? *false_scalar : true_arr->data[i];
        } else {
            source_arr->data[i] = mask_value == 0.0 ? false_arr->data[i] : true_arr->data[i];
        }
    }
    update_array_data_version(&source_arr->version);

    set_array_version(&versions->prev_a_version, &source_arr->version);
    set_array_version(&versions->prev_b_version, &true_arr->version);
    if (false_handle != NULL) {
        set_array_version(&versions->prev_c_version, &false_arr->version);
    } else {
        *prev_scalar_false = *false_scalar;
    }
    *is_published = true;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_where_in_hh_k_init(CSOUND *csound, CSN_WHERE_HH_IN *p) {
    return csnarray_where_k_in_init_helper(csound, p->source_handle, p->source_handle_true, p->source_handle_false, NULL, &p->registry, &p->is_published);
}

int32_t csnarray_where_in_hs_k_init(CSOUND *csound, CSN_WHERE_HS_IN *p) {
    double scalar_false = (double) *p->source_scalar_false;
    return csnarray_where_k_in_init_helper(csound, p->source_handle, p->source_handle_true, NULL, &scalar_false, &p->registry, &p->is_published);
}

int32_t csnarray_where_in_hh_k(CSOUND *csound, CSN_WHERE_HH_IN *p) {
    return csnarray_where_k_in_helper(csound, &p->h, p->source_handle, p->source_handle_true, p->source_handle_false, NULL, p->registry, &p->is_published, &p->versions, p->trig, &p->prev_scalar_false);
}

int32_t csnarray_where_in_hs_k(CSOUND *csound, CSN_WHERE_HS_IN *p) {
    double scalar_false = (double) *p->source_scalar_false;
    return csnarray_where_k_in_helper(csound, &p->h, p->source_handle, p->source_handle_true, NULL, &scalar_false, p->registry, &p->is_published, &p->versions, p->trig, &p->prev_scalar_false);
}

static int32_t compress_body(CSOUND *csound, OPDS *perf_h, CSN_REGISTRY *reg, CSN_ARRAY **source_array, CSN_ARRAY **mask, CSNREF *shandle, CSNREF *mhandle, const MYFLT *axis_in, int32_t *axis_out) {
    uint32_t source_handle = shandle->id;
    uint32_t mask_handle = mhandle->id;

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
    }
    CSN_ARRAY *source_arr = source_slot->array;
    uint32_t *source_shape = source_arr->shape;
    uint32_t source_ndim = source_arr->ndim;

    CSN_SLOT *mask_slot = get_slot(reg, mask_handle);
    if (mask_slot == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", mask_handle);
    }
    CSN_ARRAY *mask_arr = mask_slot->array;
    if (mask_arr->itype != CSN_REAL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Mask must be real-array");
    }

    double axis_value = (double) *axis_in;
    if (axis_value != -1.0 && !IS_VALID_AXIS(axis_value, source_ndim)) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Invalid axis value");
    }
    int32_t axis = (int32_t) axis_value;

    uint32_t axis_length = axis == -1 ? (uint32_t) source_arr->size : source_arr->shape[axis];
    if (mask_arr->ndim != 1U || mask_arr->shape[0] > axis_length) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Mask must be 1-D and no longer than the selected axis");
    }
    *axis_out = axis;

    *source_array = source_slot->array;
    *mask = mask_slot->array;
    return OK;
}

static void compress_assign_value(CSN_ARRAY *arr, CSN_ARRAY *source_arr, size_t count_true, size_t dst_size, uint32_t *indexes, int32_t axis_out, uint32_t ndim, ITEM_TYPE itype) {
    if (axis_out == -1) {
        for (size_t i = 0; i < count_true; i++) {
            size_t index = indexes[i];
            if (itype == CSN_REAL) {
                arr->data[i] = source_arr->data[index];
            } else {
                arr->data[i * 2] = source_arr->data[index * 2];
                arr->data[i * 2 + 1] = source_arr->data[index * 2 + 1];
            }
        }
    } else {
        uint32_t dst_coords[CSN_MAX_DIMS] = {0};
        uint32_t src_coords[CSN_MAX_DIMS] = {0};
        for (size_t i = 0; i < dst_size; i++) {
            from_linear_to_coords(dst_coords, arr->shape, i, ndim);
            memcpy(src_coords, dst_coords, sizeof(dst_coords));
            src_coords[axis_out] = indexes[dst_coords[axis_out]];
            size_t src_off = from_coords_to_offset(src_coords, source_arr->strides, source_arr->ndim);
            if (itype == CSN_REAL) {
                arr->data[i] = source_arr->data[src_off];
            } else {
                arr->data[i * 2] = source_arr->data[src_off * 2];
                arr->data[i * 2 + 1] = source_arr->data[src_off * 2 + 1];
            }
        }
    }
}

int32_t csnarray_compress(CSOUND *csound, CSN_WHERE_HS *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    CSNREF *source_handle = p->source_handle;
    CSNREF *mask_handle = p->source_handle_true;
    const MYFLT *axis_in = p->source_scalar_false;

    int32_t res = OK;
    const char *err = NULL;
    uint32_t *indexes = NULL;

    csound->LockMutex(reg->mutex);
    CSN_ARRAY *source_arr = NULL;
    CSN_ARRAY *mask_arr = NULL;
    int32_t axis_out = -1;
    res = compress_body(csound, NULL, reg, &source_arr, &mask_arr, source_handle, mask_handle, axis_in, &axis_out);
    if (res != OK) goto done;

    indexes = csound->Calloc(csound, sizeof(uint32_t) * mask_arr->size);
    if (indexes == NULL) {
        res = csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
        goto done;
    }
    uint32_t count_true = 0;
    for (uint32_t i = 0; i < mask_arr->size; i++) {
        if (mask_arr->data[i] != 0.0) indexes[count_true++] = i;
    }

    ITEM_TYPE itype = source_arr->itype;
    uint32_t new_dim = source_arr->ndim;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};

    if (axis_out == -1) {
        new_dim = 1U;
        new_shape[0] = count_true;
    } else {
        memcpy(new_shape, source_arr->shape, sizeof(uint32_t) * CSN_MAX_DIMS);
        new_shape[axis_out] = count_true;
    }

    uint32_t protect[2] = { source_handle->id, mask_handle->id };
    if (create_csnarray_locked(csound, reg, &p->h, new_dim, new_shape, &p->array, p->handle, protect, 2U, &err, itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    size_t size = p->array->size;
    compress_assign_value(p->array, source_arr, count_true, size, indexes, axis_out, new_dim, itype);
    update_array_data_version(&p->array->version);

done:
    if (indexes != NULL) csound->Free(csound, indexes);
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_compress_k_init(CSOUND *csound, CSN_WHERE_HS *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    const char *err = NULL;

    uint32_t *indexes = csound->Calloc(csound, sizeof(uint32_t) * DEFAULT_TEMPORARY_BUFFER_SIZE);
    if (indexes == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
    }
    p->scratch.scratch = indexes;
    p->scratch.scratch_capacity = DEFAULT_TEMPORARY_BUFFER_SIZE;

    csound->LockMutex(reg->mutex);
    uint32_t new_dim = 1U;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    new_shape[0] = 1U;
    if (create_csnarray_locked(csound, reg, &p->h, new_dim, new_shape, &p->array, p->handle, NULL, 0, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    reset_empty_csnarray(p->array, new_dim, new_shape, CSN_REAL);
    SET_KDATA_BEGIN(p, reg);
    p->is_published = false;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_compress_k(CSOUND *csound, CSN_WHERE_HS *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    uint32_t owned_handle = p->k_data.owned_handle;
    CHECK_REG_HANDLE(csound, &p->h, reg, owned_handle);

    CSNREF *source_handle = p->source_handle;
    CSNREF *mask_handle = p->source_handle_true;
    const MYFLT *axis_in = p->source_scalar_false;

    int32_t res = OK;
    const char *err = NULL;

    res = CHECK_SELF_ALIAS(csound, &p->h, &p->k_data, source_handle->id, mask_handle->id);
    if (res != OK) return res;

    CHECK_KTRIG(p->trig);

    csound->LockMutex(reg->mutex);
    CSN_ARRAY *source_arr = NULL;
    CSN_ARRAY *mask_arr = NULL;
    int32_t axis_out = -1;
    res = compress_body(csound, &p->h, reg, &source_arr, &mask_arr, source_handle, mask_handle, axis_in, &axis_out);
    if (res != OK) goto done;

    if (p->is_published) {
        bool is_same_source = is_same_array_version(&p->versions.prev_a_version, &source_arr->version);
        bool is_same_mask = is_same_array_version(&p->versions.prev_b_version, &mask_arr->version);
        bool is_same_axis = p->k_data.prev_axis == axis_out;

        bool is_same_result = false;
        CSN_SLOT *slot = get_slot(reg, owned_handle);
        if (slot != NULL) {
            is_same_result = is_same_array_version(&p->k_data.prev_output_version, &slot->array->version);
        }

        if (is_same_source && is_same_mask && is_same_axis && is_same_result) {
            p->handle->id = p->k_data.owned_handle;
            goto done;
        }
    }

    if (mask_arr->size > p->scratch.scratch_capacity) {
        size_t cap = mask_arr->size == 0 ? 1 : mask_arr->size * 2;
        uint32_t *indexes = csound->ReAlloc(csound, p->scratch.scratch, sizeof(uint32_t) * cap);
        if (indexes == NULL) {
            csound->UnlockMutex(reg->mutex);
            return csound->PerfError(csound, &p->h, "[csnarray] Internal error: memory allocation failed");
        }

        p->scratch.scratch = indexes;
        p->scratch.scratch_capacity = cap;
    }

    uint32_t *indexes_temp = (uint32_t *) p->scratch.scratch;
    uint32_t count_true = 0;
    for (uint32_t i = 0; i < mask_arr->size; i++) {
        if (mask_arr->data[i] != 0.0) indexes_temp[count_true++] = i;
    }

    ITEM_TYPE itype = source_arr->itype;
    uint32_t new_dim = source_arr->ndim;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};

    if (axis_out == -1) {
        new_dim = 1U;
        new_shape[0] = count_true;
    } else {
        memcpy(new_shape, source_arr->shape, sizeof(uint32_t) * CSN_MAX_DIMS);
        new_shape[axis_out] = count_true;
    }

    size_t req_size = 0;
    if (get_array_size_from_shape(&req_size, new_dim, new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSN_ARRAY *arr = NULL;
    size_t logical_size = (source_arr->size == 0 || mask_arr->size == 0) ? 0 : req_size;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, new_dim, new_shape, logical_size, itype, err);
    if (res != OK) goto done;
    p->array = arr;

    compress_assign_value(p->array, source_arr, count_true, p->array->size, indexes_temp, axis_out, new_dim, itype);
    SET_KDATA_END(p, new_shape, new_dim, itype);
    p->k_data.prev_axis = axis_out;
    set_array_version(&p->k_data.prev_output_version, &p->array->version);
    set_array_version(&p->versions.prev_a_version, &source_arr->version);
    set_array_version(&p->versions.prev_b_version, &mask_arr->version);
    p->scratch.scratch = indexes_temp;
    p->is_published = true;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}


static int32_t select_body(CSOUND *csound, OPDS *perf_h, CSN_ARRAY **source_array, CSN_ARRAY **mask_array, CSN_REGISTRY *reg, uint32_t source_handle, uint32_t mask_handle) {
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
    }
    CSN_ARRAY *source_arr = slot->array;

    CSN_SLOT *mask_slot = get_slot(reg, mask_handle);
    if (mask_slot == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", mask_handle);
    }
    CSN_ARRAY *mask_arr = mask_slot->array;

    if (mask_arr->ndim != source_arr->ndim || memcmp(mask_arr->shape, source_arr->shape, sizeof(uint32_t) * CSN_MAX_DIMS) != 0) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] select operation requires source and mask array with same dimension and shape");
    }

    if (mask_arr->itype == CSN_COMPLEX) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] select operation requires real-array mask");
    }

    *source_array = source_arr;
    *mask_array = mask_arr;
    return OK;
}

static void select_assign_value(CSN_ARRAY *out_arr, CSN_ARRAY *source_arr, CSN_ARRAY *mask_arr) {
    for (size_t i = 0, j = 0; i < source_arr->size; i++) {
        if (mask_arr->data[i] != 0.0) {
            if (source_arr->itype == CSN_REAL) {
                out_arr->data[j] = source_arr->data[i];
            } else {
                out_arr->data[j * 2] = source_arr->data[i * 2];
                out_arr->data[j * 2 + 1] = source_arr->data[i * 2 + 1];
            }
            j++;
        }
    }
}

int32_t csnarray_select(CSOUND *csound, CSN_ARGWHERE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;
    uint32_t mask_handle = p->data_handle->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);
    CSN_ARRAY *source_arr = NULL;
    CSN_ARRAY *mask_arr = NULL;
    res = select_body(csound, NULL, &source_arr, &mask_arr, reg, source_handle, mask_handle);
    if (res != OK) goto done;

    uint32_t count_true = 0;
    for (uint32_t i = 0; i < mask_arr->size; i++) {
        if (mask_arr->data[i] != 0.0) count_true++;
    }

    ITEM_TYPE itype = source_arr->itype;
    uint32_t new_dim = 1U;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    new_shape[0] = count_true;

    uint32_t protect[2] = { source_handle, mask_handle };
    if (create_csnarray_locked(csound, reg, &p->h, new_dim, new_shape, &p->array, p->handle, protect, 2U, &err, itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    select_assign_value(p->array, source_arr, mask_arr);
    update_array_data_version(&p->array->version);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_select_k_init(CSOUND *csound, CSN_ARGWHERE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    new_shape[0] = 1U;
    int32_t res = create_csnarray_init(csound, &p->h, 1U, new_shape, &p->array, p->handle, CSN_REAL);
    if (res != OK) return res;

    csound->LockMutex(reg->mutex);
    reset_empty_csnarray(p->array, 1U, new_shape, CSN_REAL);
    csound->UnlockMutex(reg->mutex);

    SET_KDATA_BEGIN(p, reg);
    p->is_published = false;

    return OK;
}

int32_t csnarray_select_k(CSOUND *csound, CSN_ARGWHERE *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    uint32_t owned_handle = p->k_data.owned_handle;
    CHECK_REG_HANDLE(csound, &p->h, reg, owned_handle);

    uint32_t source_handle = p->source_handle->id;
    uint32_t mask_handle = p->data_handle->id;

    int32_t res = OK;
    const char *err = NULL;

    CHECK_KTRIG(p->trig);

    csound->LockMutex(reg->mutex);
    CSN_ARRAY *source_arr = NULL;
    CSN_ARRAY *mask_arr = NULL;
    res = select_body(csound, &p->h, &source_arr, &mask_arr, reg, source_handle, mask_handle);
    if (res != OK) goto done;

    if (p->is_published) {
        bool is_same_result = false;
        CSN_SLOT *res_slot = get_slot(reg, p->k_data.owned_handle);
        if (res_slot != NULL) {
            is_same_result = is_same_array_data_version(&p->k_data.prev_output_version, &res_slot->array->version);
        }
        bool is_same_source = is_same_array_version(&p->versions.prev_a_version, &source_arr->version);
        bool is_same_mask = is_same_array_version(&p->versions.prev_b_version, &mask_arr->version);
        if (is_same_source && is_same_mask && is_same_result) {
            p->handle->id = p->k_data.owned_handle;
            goto done;
        }
    }

    uint32_t count_true = 0;
    for (uint32_t i = 0; i < mask_arr->size; i++) {
        if (mask_arr->data[i] != 0.0) count_true++;
    }

    ITEM_TYPE itype = source_arr->itype;
    uint32_t new_dim = 1U;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    new_shape[0] = count_true;

    size_t req_size = 0;
    if (get_array_size_from_shape(&req_size, new_dim, new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSN_ARRAY *arr = NULL;
    size_t logical_size = (mask_arr->size == 0 || source_arr->size == 0) ? 0 : req_size;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, new_dim, new_shape, logical_size, itype, err);
    if (res != OK) goto done;
    p->array = arr;

    select_assign_value(p->array, source_arr, mask_arr);
    update_array_data_version(&p->array->version);

    SET_KDATA_END(p, new_shape, new_dim, itype);
    p->is_published = true;
    set_array_version(&p->versions.prev_a_version, &source_arr->version);
    set_array_version(&p->versions.prev_b_version, &mask_arr->version);
    set_array_version(&p->k_data.prev_output_version, &p->array->version);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

// --- OENTRY ---

#define S(x) sizeof(x)

static OENTRY localops[] = {
    { "csnseed",               S(CSN_SEED),                   0, "",                    "i",                              (SUBR) csnarray_set_seed,                    NULL,                                   NULL,                                   NULL, 0 },
    { "csnsave",               S(CSN_SAVE),                   0, "",                    ":CsnArr;S",                      (SUBR) csnarray_save,                        NULL,                                   NULL,                                   NULL, 0 },
    { "csnload",               S(CSN_LOAD),                   0, ":CsnArr;",            "S",                              (SUBR) csnarray_load,                        NULL,                                   (SUBR) csnarray_load_deinit,            NULL, 0 },
    { "csnsave.k",             S(CSN_SAVE),                   0, "",                    ":CsnArr;Sk",                     (SUBR) csnarray_save_k_init,                 (SUBR) csnarray_save_k,                 (SUBR) csnarray_save_k_deinit,          NULL, 0 },
    { "csnload.k",             S(CSN_LOAD),                   0, ":CsnArr;",            "Sk",                             (SUBR) csnarray_load_k_init,                 (SUBR) csnarray_load_k,                 (SUBR) csnarray_load_deinit,            NULL, 0 },
    { "csnrtlock",             S(CSN_RTLOCK),                 0, "",                    ":CsnArr;i",                      (SUBR) csnarray_set_rtlock,                  NULL,                                   NULL,                                   NULL, 0 },
    // REAL-ONLY
    { "csnfromaudio",          S(CSN_FROM_AUDIO),             0, ":CsnArr;",            "ap",                             (SUBR) csnarray_from_audio_init,             (SUBR) csnarray_from_audio,             (SUBR) csnarray_from_audio_deinit,      NULL, 0 },
    { "csntoaudio",            S(CSN_TO_AUDIO),               0, "a",                   ":CsnArr;",                       (SUBR) csnarray_to_audio_init,               (SUBR) csnarray_to_audio,               NULL,                                   NULL, 0 },
    { "csnpack",               S(CSN_PACK_AUDIO),             0, ":CsnArr;",            "a[]p",                           (SUBR) csnarray_pack_audio_init,             (SUBR) csnarray_pack_audio,             (SUBR) csnarray_from_audio_deinit,      NULL, 0 },
    { "csnunpack",             S(CSN_UNPACK_AUDIO),           0, "a[]",                 ":CsnArr;",                       (SUBR) csnarray_unpack_audio_init,           (SUBR) csnarray_unpack_audio,           NULL,                                   NULL, 0 },
    { "csnsnap",               S(CSN_FRAME_AUDIO),            0, ":CsnArr;k",           "aiop",                           (SUBR) csnarray_frame_audio_init,            (SUBR) csnarray_frame_audio,            (SUBR) csnarray_frame_audio_deinit,     NULL, 0 },
    { "csnstream",             S(CSN_OLA_AUDIO),              0, "ak",                  ":CsnArr;i",                      (SUBR) csnarray_ola_audio_init,              (SUBR) csnarray_ola_audio,              (SUBR) csnarray_ola_audio_deinit,       NULL, 0 },
    { "csnrand",               S(CSN_ARR_RND_INIT),           0, ":CsnArr;",            "i[]ii",                          (SUBR) create_random_csnarray,               NULL,                                   (SUBR) create_csnarray_random_deinit,   NULL, 0 },
    { "csnrand.k",             S(CSN_ARR_RND_INIT),           0, ":CsnArr;",            "k[]kkP",                         (SUBR) create_random_csnarray_k_init,        (SUBR) create_random_csnarray_k,        (SUBR) create_csnarray_random_deinit,   NULL, 0 },
    { "csnarange",             S(CSN_SPACED_SPACE),           0, ":CsnArr;",            "iii",                            (SUBR) csnarray_arange,                      NULL,                                   (SUBR) csnarray_space_spaced_deinit,    NULL, 0 },
    { "csnlinspace",           S(CSN_SPACED_SPACE),           0, ":CsnArr;",            "iii",                            (SUBR) csnarray_linspace,                    NULL,                                   (SUBR) csnarray_space_spaced_deinit,    NULL, 0 },
    { "csnlogspace",           S(CSN_SPACED_SPACE),           0, ":CsnArr;",            "iiii",                           (SUBR) csnarray_logspace,                    NULL,                                   (SUBR) csnarray_space_spaced_deinit,    NULL, 0 },
    { "csngeomspace",          S(CSN_SPACED_SPACE),           0, ":CsnArr;",            "iii",                            (SUBR) csnarray_geomspace,                   NULL,                                   (SUBR) csnarray_space_spaced_deinit,    NULL, 0 },
    { "csnarange.k",           S(CSN_SPACED_SPACE),           0, ":CsnArr;",            "kkkk",                           (SUBR) csnarray_spaced_space_k_init,         (SUBR) csnarray_arange_k,               (SUBR) csnarray_space_spaced_deinit,    NULL, 0 },
    { "csnlinspace.k",         S(CSN_SPACED_SPACE),           0, ":CsnArr;",            "kkkk",                           (SUBR) csnarray_spaced_space_k_init,         (SUBR) csnarray_linspace_k,             (SUBR) csnarray_space_spaced_deinit,    NULL, 0 },
    { "csnlogspace.k",         S(CSN_SPACED_SPACE),           0, ":CsnArr;",            "kkkkk",                          (SUBR) csnarray_spaced_space_k_init,         (SUBR) csnarray_logspace_k,             (SUBR) csnarray_space_spaced_deinit,    NULL, 0 },
    { "csngeomspace.k",        S(CSN_SPACED_SPACE),           0, ":CsnArr;",            "kkkk",                           (SUBR) csnarray_spaced_space_k_init,         (SUBR) csnarray_geomspace_k,            (SUBR) csnarray_space_spaced_deinit,    NULL, 0 },
    { "csnclip",               S(CSN_CLIP),                   0, ":CsnArr;",            ":CsnArr;ii",                     (SUBR) csnarray_clip,                        NULL,                                   (SUBR) csnarray_clip_deinit,            NULL, 0 },
    { "csnclip.k",             S(CSN_CLIP),                   0, ":CsnArr;",            ":CsnArr;kkP",                    (SUBR) csnarray_clip_k_init,                 (SUBR) csnarray_clip_k,                 (SUBR) csnarray_clip_deinit,            NULL, 0 },
    { "csnclip.in",            S(CSN_CLIP_IN),                0, "",                    ":CsnArr;ii",                     (SUBR) csnarray_clip_in,                     NULL,                                   NULL,                                   NULL, 0 },
    { "csnclip.in.k",          S(CSN_CLIP_IN),                0, "",                    ":CsnArr;kkP",                    (SUBR) csnarray_clip_in_k_init,              (SUBR) csnarray_clip_in_k,              NULL,                                   NULL, 0 },
    { "csnargwhere",           S(CSN_ARGWHERE),               0, ":CsnArr;",            ":CsnArr;:CsnArr;",               (SUBR) csnarray_argwhere,                    NULL,                                   (SUBR) csnarray_argwhere_deinit,        NULL, 0 },
    { "csnargwhere.k",         S(CSN_ARGWHERE),               0, ":CsnArr;",            ":CsnArr;:CsnArr;k",              (SUBR) csnarray_argwhere_k_init,             (SUBR) csnarray_argwhere_k,             (SUBR) csnarray_argwhere_deinit,        NULL, 0 },
    { "csnargnonzero",         S(CSN_ARGWHERE),               0, ":CsnArr;",            ":CsnArr;",                       (SUBR) csnarray_argnonzero,                  NULL,                                   (SUBR) csnarray_argwhere_deinit,        NULL, 0 },
    { "csnargnonzero.k",       S(CSN_ARGWHERE),               0, ":CsnArr;",            ":CsnArr;k",                      (SUBR) csnarray_argselect_k_init,            (SUBR) csnarray_argnonzero_k,           (SUBR) csnarray_argwhere_deinit,        NULL, 0 },
    { "csnargisnan",           S(CSN_ARGWHERE),               0, ":CsnArr;",            ":CsnArr;",                       (SUBR) csnarray_argisnan,                    NULL,                                   (SUBR) csnarray_argwhere_deinit,        NULL, 0 },
    { "csnargisnan.k",         S(CSN_ARGWHERE),               0, ":CsnArr;",            ":CsnArr;k",                      (SUBR) csnarray_argselect_k_init,            (SUBR) csnarray_argisnan_k,             (SUBR) csnarray_argwhere_deinit,        NULL, 0 },
    { "csnargunique",          S(CSN_ARGWHERE),               0, ":CsnArr;",            ":CsnArr;",                       (SUBR) csnarray_argunique,                   NULL,                                   (SUBR) csnarray_argwhere_deinit,        NULL, 0 },
    { "csnargunique.k",        S(CSN_ARGWHERE),               0, ":CsnArr;",            ":CsnArr;k",                      (SUBR) csnarray_argunique_k_init,            (SUBR) csnarray_argunique_k,            (SUBR) csnarray_argwhere_deinit,        NULL, 0 },
    { "csnunique",             S(CSN_COMPARE),                0, ":CsnArr;",            ":CsnArr;",                       (SUBR) csnarray_unique,                      NULL,                                   (SUBR) csnarray_compare_deinit,         NULL, 0 },
    { "csnunique.k",           S(CSN_COMPARE),                0, ":CsnArr;",            ":CsnArr;k",                      (SUBR) csnarray_unique_k_init,               (SUBR) csnarray_unique_k,               (SUBR) csnarray_compare_deinit,         NULL, 0 },
    { "csngt",                 S(CSN_COMPARE),                0, ":CsnArr;",            ":CsnArr;i",                      (SUBR) csnarray_greater_than,                NULL,                                   (SUBR) csnarray_compare_deinit,         NULL, 0 },
    { "csnlt",                 S(CSN_COMPARE),                0, ":CsnArr;",            ":CsnArr;i",                      (SUBR) csnarray_less_than,                   NULL,                                   (SUBR) csnarray_compare_deinit,         NULL, 0 },
    { "csnne",                 S(CSN_COMPARE),                0, ":CsnArr;",            ":CsnArr;i",                      (SUBR) csnarray_not_equal,                   NULL,                                   (SUBR) csnarray_compare_deinit,         NULL, 0 },
    { "csnge",                 S(CSN_COMPARE),                0, ":CsnArr;",            ":CsnArr;i",                      (SUBR) csnarray_greater_equal,               NULL,                                   (SUBR) csnarray_compare_deinit,         NULL, 0 },
    { "csnle",                 S(CSN_COMPARE),                0, ":CsnArr;",            ":CsnArr;i",                      (SUBR) csnarray_less_equal,                  NULL,                                   (SUBR) csnarray_compare_deinit,         NULL, 0 },
    { "csneq",                 S(CSN_COMPARE),                0, ":CsnArr;",            ":CsnArr;i",                      (SUBR) csnarray_equal,                       NULL,                                   (SUBR) csnarray_compare_deinit,         NULL, 0 },
    { "csngt.k",               S(CSN_COMPARE),                0, ":CsnArr;",            ":CsnArr;kP",                     (SUBR) csnarray_compare_k_init,              (SUBR) csnarray_greater_than_k,         (SUBR) csnarray_compare_deinit,         NULL, 0 },
    { "csnlt.k",               S(CSN_COMPARE),                0, ":CsnArr;",            ":CsnArr;kP",                     (SUBR) csnarray_compare_k_init,              (SUBR) csnarray_less_than_k,            (SUBR) csnarray_compare_deinit,         NULL, 0 },
    { "csnne.k",               S(CSN_COMPARE),                0, ":CsnArr;",            ":CsnArr;kP",                     (SUBR) csnarray_compare_k_init,              (SUBR) csnarray_not_equal_k,            (SUBR) csnarray_compare_deinit,         NULL, 0 },
    { "csnge.k",               S(CSN_COMPARE),                0, ":CsnArr;",            ":CsnArr;kP",                     (SUBR) csnarray_compare_k_init,              (SUBR) csnarray_greater_equal_k,        (SUBR) csnarray_compare_deinit,         NULL, 0 },
    { "csnle.k",               S(CSN_COMPARE),                0, ":CsnArr;",            ":CsnArr;kP",                     (SUBR) csnarray_compare_k_init,              (SUBR) csnarray_less_equal_k,           (SUBR) csnarray_compare_deinit,         NULL, 0 },
    { "csneq.k",               S(CSN_COMPARE),                0, ":CsnArr;",            ":CsnArr;kP",                     (SUBR) csnarray_compare_k_init,              (SUBR) csnarray_equal_k,                (SUBR) csnarray_compare_deinit,         NULL, 0 },
    { "csncnteq",              S(CSN_COUNT),                  0, "i",                   ":CsnArr;i",                      (SUBR) csnarray_count_equal,                 NULL,                                   NULL,                                   NULL, 0 },
    { "csncntnz",              S(CSN_COUNT),                  0, "i",                   ":CsnArr;",                       (SUBR) csnarray_count_nonzero,               NULL,                                   NULL,                                   NULL, 0 },
    { "csncntnan",             S(CSN_COUNT),                  0, "i",                   ":CsnArr;",                       (SUBR) csnarray_count_nan,                   NULL,                                   NULL,                                   NULL, 0 },
    { "csncnteq.k",            S(CSN_COUNT),                  0, "k",                   ":CsnArr;kk",                     (SUBR) csnarray_compare_count_k_init,        (SUBR) csnarray_count_equal_k,          NULL,                                   NULL, 0 },
    { "csncntnz.k",            S(CSN_COUNT),                  0, "k",                   ":CsnArr;k",                      (SUBR) csnarray_compare_count_k_init,        (SUBR) csnarray_count_nonzero_k,        NULL,                                   NULL, 0 },
    { "csncntnan.k",           S(CSN_COUNT),                  0, "k",                   ":CsnArr;k",                      (SUBR) csnarray_compare_count_k_init,        (SUBR) csnarray_count_nan_k,            NULL,                                   NULL, 0 },
    { "csnmin",                S(CSN_REDUCTION_SCALAR),       0, "i",                   ":CsnArr;",                       (SUBR) csnarray_min_all,                     NULL,                                   NULL,                                   NULL, 0 },
    { "csnmin.k",              S(CSN_REDUCTION_SCALAR),       0, "k",                   ":CsnArr;P",                      (SUBR) csnarray_min_all_k_init,              (SUBR) csnarray_min_all_k,              NULL,                                   NULL, 0 },
    { "csnmax",                S(CSN_REDUCTION_SCALAR),       0, "i",                   ":CsnArr;",                       (SUBR) csnarray_max_all,                     NULL,                                   NULL,                                   NULL, 0 },
    { "csnmax.k",              S(CSN_REDUCTION_SCALAR),       0, "k",                   ":CsnArr;P",                      (SUBR) csnarray_max_all_k_init,              (SUBR) csnarray_max_all_k,              NULL,                                   NULL, 0 },
    { "csnmedian",             S(CSN_REDUCTION_SCALAR),       0, "i",                   ":CsnArr;",                       (SUBR) csnarray_median_all,                  NULL,                                   NULL,                                   NULL, 0 },
    { "csnmedian.k",           S(CSN_REDUCTION_SCALAR),       0, "k",                   ":CsnArr;P",                      (SUBR) csnarray_median_all_k_init,           (SUBR) csnarray_median_all_k,           (SUBR) csnarray_median_scalar_k_deinit, NULL, 0 },
    { "csnmin.ax",             S(CSN_REDUCTION),              0, ":CsnArr;",            ":CsnArr;i",                      (SUBR) csnarray_min,                         NULL,                                   (SUBR) csnarray_reduction_deinit,       NULL, 0 },
    { "csnmin.ax.k",           S(CSN_REDUCTION),              0, ":CsnArr;",            ":CsnArr;kP",                     (SUBR) csnarray_min_k_init,                  (SUBR) csnarray_min_k,                  (SUBR) csnarray_reduction_deinit,       NULL, 0 },
    { "csnmax.ax",             S(CSN_REDUCTION),              0, ":CsnArr;",            ":CsnArr;i",                      (SUBR) csnarray_max,                         NULL,                                   (SUBR) csnarray_reduction_deinit,       NULL, 0 },
    { "csnmax.ax.k",           S(CSN_REDUCTION),              0, ":CsnArr;",            ":CsnArr;kP",                     (SUBR) csnarray_max_k_init,                  (SUBR) csnarray_max_k,                  (SUBR) csnarray_reduction_deinit,       NULL, 0 },
    { "csnmedian.ax",          S(CSN_REDUCTION),              0, ":CsnArr;",            ":CsnArr;i",                      (SUBR) csnarray_median,                      NULL,                                   (SUBR) csnarray_reduction_deinit,       NULL, 0 },
    { "csnmedian.ax.k",        S(CSN_REDUCTION),              0, ":CsnArr;",            ":CsnArr;kP",                     (SUBR) csnarray_median_k_init,               (SUBR) csnarray_median_k,               (SUBR) csnarray_reduction_deinit,       NULL, 0 },
    { "csnargmin",             S(CSN_REDUCTION),              0, ":CsnArr;",            ":CsnArr;j",                      (SUBR) csnarray_argmin,                      NULL,                                   (SUBR) csnarray_reduction_deinit,       NULL, 0 },
    { "csnargmin.k",           S(CSN_REDUCTION),              0, ":CsnArr;",            ":CsnArr;kP",                     (SUBR) csnarray_argmin_k_init,               (SUBR) csnarray_argmin_k,               (SUBR) csnarray_reduction_deinit,       NULL, 0 },
    { "csnargmax",             S(CSN_REDUCTION),              0, ":CsnArr;",            ":CsnArr;j",                      (SUBR) csnarray_argmax,                      NULL,                                   (SUBR) csnarray_reduction_deinit,       NULL, 0 },
    { "csnargmax.k",           S(CSN_REDUCTION),              0, ":CsnArr;",            ":CsnArr;kP",                     (SUBR) csnarray_argmax_k_init,               (SUBR) csnarray_argmax_k,               (SUBR) csnarray_reduction_deinit,       NULL, 0 },
    { "csnfloor",              S(CSN_UNARYOP),                0, ":CsnArr;",            ":CsnArr;",                       (SUBR) csnarray_floor,                       NULL,                                   (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csnceil",               S(CSN_UNARYOP),                0, ":CsnArr;",            ":CsnArr;",                       (SUBR) csnarray_ceil,                        NULL,                                   (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csnround",              S(CSN_UNARYOP),                0, ":CsnArr;",            ":CsnArr;",                       (SUBR) csnarray_round,                       NULL,                                   (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csnfloor.k",            S(CSN_UNARYOP),                0, ":CsnArr;",            ":CsnArr;k",                      (SUBR) csnarray_floor,                       (SUBR) csnarray_floor_k,                (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csnceil.k",             S(CSN_UNARYOP),                0, ":CsnArr;",            ":CsnArr;k",                      (SUBR) csnarray_ceil,                        (SUBR) csnarray_ceil_k,                 (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csnround.k",            S(CSN_UNARYOP),                0, ":CsnArr;",            ":CsnArr;k",                      (SUBR) csnarray_round,                       (SUBR) csnarray_round_k,                (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csnproject",            S(CSN_BINOP_HH),               0, ":CsnArr;",            ":CsnArr;:CsnArr;",               (SUBR) csnarray_project,                     NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnproject.k",          S(CSN_BINOP_HH),               0, ":CsnArr;",            ":CsnArr;:CsnArr;k",              (SUBR) csnarray_project_k_init,              (SUBR) csnarray_project_k,              (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnreject",             S(CSN_BINOP_HH),               0, ":CsnArr;",            ":CsnArr;:CsnArr;",               (SUBR) csnarray_reject,                      NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnreject.k",           S(CSN_BINOP_HH),               0, ":CsnArr;",            ":CsnArr;:CsnArr;k",              (SUBR) csnarray_reject_k_init,               (SUBR) csnarray_reject_k,               (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csncross",              S(CSN_BINOP_HH),               0, ":CsnArr;",            ":CsnArr;:CsnArr;",               (SUBR) csnarray_cross,                       NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csncross.k",            S(CSN_BINOP_HH),               0, ":CsnArr;",            ":CsnArr;:CsnArr;k",              (SUBR) csnarray_cross_k_init,                (SUBR) csnarray_cross_k,                (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csngrad",               S(CSN_UNARYOP_AX),             0, ":CsnArr;",            ":CsnArr;j",                      (SUBR) csnarray_gradient,                    NULL,                                   (SUBR) csnarray_opunary_ax_deinit,      NULL, 0 },
    { "csngrad.k",             S(CSN_UNARYOP_AX),             0, ":CsnArr;",            ":CsnArr;kP",                     (SUBR) csnarray_gradient_k_init,             (SUBR) csnarray_gradient_k,             (SUBR) csnarray_opunary_ax_deinit,      NULL, 0 },
    { "csnmovmedian",          S(CSN_MOVSTATS),               0, ":CsnArr;",            ":CsnArr;ij",                     (SUBR) csnarray_movmedian,                   NULL,                                   (SUBR) csnarray_movstats_deinit,        NULL, 0 },
    { "csnmovmedian.k",        S(CSN_MOVSTATS),               0, ":CsnArr;",            ":CsnArr;kkP",                    (SUBR) csnarray_movmedian_k_init,            (SUBR) csnarray_movmedian_k,            (SUBR) csnarray_movstats_deinit,        NULL, 0 },
    { "csnmovmedian.in",       S(CSN_MOVSTATS_IN),            0, "",                    ":CsnArr;ij",                     (SUBR) csnarray_movmedian_in,                NULL,                                   NULL,                                   NULL, 0 },
    { "csnmovmedian.in.k",     S(CSN_MOVSTATS_IN),            0, "",                    ":CsnArr;kkP",                    (SUBR) csnarray_movmedian_in_k_init,         (SUBR) csnarray_movmedian_in_k,         (SUBR) csnarray_movstats_in_k_deinit,   NULL, 0 },
    { "csnmovmin",             S(CSN_MOVSTATS),               0, ":CsnArr;",            ":CsnArr;ij",                     (SUBR) csnarray_movmin,                      NULL,                                   (SUBR) csnarray_movstats_deinit,        NULL, 0 },
    { "csnmovmin.k",           S(CSN_MOVSTATS),               0, ":CsnArr;",            ":CsnArr;kkP",                    (SUBR) csnarray_movmin_k_init,               (SUBR) csnarray_movmin_k,               (SUBR) csnarray_movstats_deinit,        NULL, 0 },
    { "csnmovmin.in",          S(CSN_MOVSTATS_IN),            0, "",                    ":CsnArr;ij",                     (SUBR) csnarray_movmin_in,                   NULL,                                   NULL,                                   NULL, 0 },
    { "csnmovmin.in.k",        S(CSN_MOVSTATS_IN),            0, "",                    ":CsnArr;kkP",                    (SUBR) csnarray_movmin_in_k_init,            (SUBR) csnarray_movmin_in_k,            (SUBR) csnarray_movstats_in_k_deinit,   NULL, 0 },
    { "csnmovmax",             S(CSN_MOVSTATS),               0, ":CsnArr;",            ":CsnArr;ij",                     (SUBR) csnarray_movmax,                      NULL,                                   (SUBR) csnarray_movstats_deinit,        NULL, 0 },
    { "csnmovmax.k",           S(CSN_MOVSTATS),               0, ":CsnArr;",            ":CsnArr;kkP",                    (SUBR) csnarray_movmax_k_init,               (SUBR) csnarray_movmax_k,               (SUBR) csnarray_movstats_deinit,        NULL, 0 },
    { "csnmovmax.in",          S(CSN_MOVSTATS_IN),            0, "",                    ":CsnArr;ij",                     (SUBR) csnarray_movmax_in,                   NULL,                                   NULL,                                   NULL, 0 },
    { "csnmovmax.in.k",        S(CSN_MOVSTATS_IN),            0, "",                    ":CsnArr;kkP",                    (SUBR) csnarray_movmax_in_k_init,            (SUBR) csnarray_movmax_in_k,            (SUBR) csnarray_movstats_in_k_deinit,   NULL, 0 },
    { "csnsort",               S(CSN_UNARYOP_AX),             0, ":CsnArr;",            ":CsnArr;j",                      (SUBR) csnarray_sort,                        NULL,                                   (SUBR) csnarray_opunary_ax_deinit,      NULL, 0 },
    { "csnsort.k",             S(CSN_UNARYOP_AX),             0, ":CsnArr;",            ":CsnArr;kP",                     (SUBR) csnarray_sort_k_init,                 (SUBR) csnarray_sort_k,                 (SUBR) csnarray_opunary_ax_deinit,      NULL, 0 },
    { "csnsort.in",            S(CSN_UNARYOP_AX_IN),          0, "",                    ":CsnArr;j",                      (SUBR) csnarray_sort_in,                     NULL,                                   NULL,                                   NULL, 0 },
    { "csnsort.in.k",          S(CSN_UNARYOP_AX_IN),          0, "",                    ":CsnArr;kP",                     (SUBR) csnarray_sort_in_k_init,              (SUBR) csnarray_sort_in_k,              (SUBR) opunary_ax_in_k_deinit,          NULL, 0 },
    { "csnargsort",            S(CSN_UNARYOP_AX),             0, ":CsnArr;",            ":CsnArr;j",                      (SUBR) csnarray_argsort,                     NULL,                                   (SUBR) csnarray_opunary_ax_deinit,      NULL, 0 },
    { "csnargsort.k",          S(CSN_UNARYOP_AX),             0, ":CsnArr;",            ":CsnArr;kP",                     (SUBR) csnarray_argsort_k_init,              (SUBR) csnarray_argsort_k,              (SUBR) csnarray_opunary_ax_deinit,      NULL, 0 },
    { "csnpercentile",         S(CSN_PERCQUANT),              0, "i",                   ":CsnArr;i",                      (SUBR) csnarray_percentile_scalar,           NULL,                                   NULL,                                   NULL, 0 },
    { "csnpercentile.ax",      S(CSN_PERCQUANT_AX),           0, ":CsnArr;",            ":CsnArr;ii",                     (SUBR) csnarray_percentile,                  NULL,                                   (SUBR) csnarray_perquant_deinit,        NULL, 0 },
    { "csnquantile",           S(CSN_PERCQUANT),              0, "i",                   ":CsnArr;i",                      (SUBR) csnarray_quantile_scalar,             NULL,                                   NULL,                                   NULL, 0 },
    { "csnquantile.ax",        S(CSN_PERCQUANT_AX),           0, ":CsnArr;",            ":CsnArr;ii",                     (SUBR) csnarray_quantile,                    NULL,                                   (SUBR) csnarray_perquant_deinit,        NULL, 0 },
    { "csnpercentile.k",       S(CSN_PERCQUANT),              0, "k",                   ":CsnArr;kP",                     (SUBR) csnarray_perquant_scalar_k_init,      (SUBR) csnarray_percentile_scalar_k,    (SUBR) csnarray_perquant_s_k_deinit,    NULL, 0 },
    { "csnpercentile.ax.k",    S(CSN_PERCQUANT_AX),           0, ":CsnArr;",            ":CsnArr;kkP",                    (SUBR) csnarray_perquant_k_init,             (SUBR) csnarray_percentile_k,           (SUBR) csnarray_perquant_deinit,        NULL, 0 },
    { "csnquantile.k",         S(CSN_PERCQUANT),              0, "k",                   ":CsnArr;kP",                     (SUBR) csnarray_perquant_scalar_k_init,      (SUBR) csnarray_quantile_scalar_k,      (SUBR) csnarray_perquant_s_k_deinit,    NULL, 0 },
    { "csnquantile.ax.k",      S(CSN_PERCQUANT_AX),           0, ":CsnArr;",            ":CsnArr;kkP",                    (SUBR) csnarray_perquant_k_init,             (SUBR) csnarray_quantile_k,             (SUBR) csnarray_perquant_deinit,        NULL, 0 },
    { "csnlogicand.hh",        S(CSN_BINOP_HH),               0, ":CsnArr;",            ":CsnArr;:CsnArr;",               (SUBR) csnarray_logical_and_hh,              NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnlogicand.hh.k",      S(CSN_BINOP_HH),               0, ":CsnArr;",            ":CsnArr;:CsnArr;k",              (SUBR) csnarray_logical_and_hh_k_init,       (SUBR) csnarray_logical_and_hh_k,       (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnlogicor.hh",         S(CSN_BINOP_HH),               0, ":CsnArr;",            ":CsnArr;:CsnArr;",               (SUBR) csnarray_logical_or_hh,               NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnlogicor.hh.k",       S(CSN_BINOP_HH),               0, ":CsnArr;",            ":CsnArr;:CsnArr;k",              (SUBR) csnarray_logical_or_hh_k_init,        (SUBR) csnarray_logical_or_hh_k,        (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnlogicand.hs",        S(CSN_BINOP_HS),               0, ":CsnArr;",            ":CsnArr;i",                      (SUBR) csnarray_logical_and_hs,              NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnlogicor.hs",         S(CSN_BINOP_HS),               0, ":CsnArr;",            ":CsnArr;i",                      (SUBR) csnarray_logical_or_hs,               NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnlogicand.sh",        S(CSN_BINOP_SH),               0, ":CsnArr;",            "i:CsnArr;",                      (SUBR) csnarray_logical_and_sh,              NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnlogicor.sh",         S(CSN_BINOP_SH),               0, ":CsnArr;",            "i:CsnArr;",                      (SUBR) csnarray_logical_or_sh,               NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnlogicand.hs.k",      S(CSN_BINOP_HS),               0, ":CsnArr;",            ":CsnArr;kP",                     (SUBR) csnarray_logical_and_hs_k_init,       (SUBR) csnarray_logical_and_hs_k,       (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnlogicor.hs.k",       S(CSN_BINOP_HS),               0, ":CsnArr;",            ":CsnArr;kP",                     (SUBR) csnarray_logical_or_hs_k_init,        (SUBR) csnarray_logical_or_hs_k,        (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnlogicand.sh.k",      S(CSN_BINOP_SH),               0, ":CsnArr;",            "k:CsnArr;P",                     (SUBR) csnarray_logical_and_sh_k_init,       (SUBR) csnarray_logical_and_sh_k,       (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnlogicor.sh.k",       S(CSN_BINOP_SH),               0, ":CsnArr;",            "k:CsnArr;P",                     (SUBR) csnarray_logical_or_sh_k_init,        (SUBR) csnarray_logical_or_sh_k,        (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnlogicnot",           S(CSN_UNARYOP),                0, ":CsnArr;",            ":CsnArr;",                       (SUBR) csnarray_logical_not,                 NULL,                                   (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csnlogicnot.k",         S(CSN_UNARYOP),                0, ":CsnArr;",            ":CsnArr;k",                      (SUBR) csnarray_logical_not,                 (SUBR) csnarray_logical_not_k,          (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csnhypot",              S(CSN_BINOP_HH),               0, ":CsnArr;",            ":CsnArr;:CsnArr;",               (SUBR) csnarray_hypot_hh,                    NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnhypot.k",            S(CSN_BINOP_HH),               0, ":CsnArr;",            ":CsnArr;:CsnArr;k",              (SUBR) csnarray_hypot_hh_k_init,             (SUBR) csnarray_hypot_hh_k,             (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnhypot.hs",           S(CSN_BINOP_HS),               0, ":CsnArr;",            ":CsnArr;i",                      (SUBR) csnarray_hypot_hs,                    NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnhypot.hs.k",         S(CSN_BINOP_HS),               0, ":CsnArr;",            ":CsnArr;kP",                     (SUBR) csnarray_hypot_hs_k_init,             (SUBR) csnarray_hypot_hs_k,             (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csndegtorad",           S(CSN_UNARYOP),                0, ":CsnArr;",            ":CsnArr;",                       (SUBR) csnarray_degtorad,                    NULL,                                   (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csndegtorad.k",         S(CSN_UNARYOP),                0, ":CsnArr;",            ":CsnArr;k",                      (SUBR) csnarray_degtorad,                    (SUBR) csnarray_degtorad_k,             (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csndegtorad.in",        S(CSN_UNARYOP_IN),             0, "",                    ":CsnArr;",                       (SUBR) csnarray_degtorad_in,                 NULL,                                   NULL,                                   NULL, 0 },
    { "csndegtorad.in.k",      S(CSN_UNARYOP_IN),             0, "",                    ":CsnArr;k",                      (SUBR) csnarray_unaryop_in_k_init,           (SUBR) csnarray_degtorad_in_k,          NULL,                                   NULL, 0 },
    { "csnradtodeg",           S(CSN_UNARYOP),                0, ":CsnArr;",            ":CsnArr;",                       (SUBR) csnarray_radtodeg,                    NULL,                                   (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csnradtodeg.k",         S(CSN_UNARYOP),                0, ":CsnArr;",            ":CsnArr;k",                      (SUBR) csnarray_radtodeg,                    (SUBR) csnarray_radtodeg_k,             (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csnradtodeg.in",        S(CSN_UNARYOP_IN),             0, "",                    ":CsnArr;",                       (SUBR) csnarray_radtodeg_in,                 NULL,                                   NULL,                                   NULL, 0 },
    { "csnradtodeg.in.k",      S(CSN_UNARYOP_IN),             0, "",                    ":CsnArr;k",                      (SUBR) csnarray_unaryop_in_k_init,           (SUBR) csnarray_radtodeg_in_k,          NULL,                                   NULL, 0 },
    { "csnhanning",            S(CSN_WINDOW),                 0, ":CsnArr;",            "i",                              (SUBR) csnarray_hanning,                     NULL,                                   (SUBR) csnarray_window_deinit,          NULL, 0 },
    { "csnhanning.k",          S(CSN_WINDOW),                 0, ":CsnArr;",            "k",                              (SUBR) csnarray_window_function_k_init,      (SUBR) csnarray_hanning_k,              (SUBR) csnarray_window_deinit,          NULL, 0 },
    { "csnhamming",            S(CSN_WINDOW),                 0, ":CsnArr;",            "i",                              (SUBR) csnarray_hamming,                     NULL,                                   (SUBR) csnarray_window_deinit,          NULL, 0 },
    { "csnhamming.k",          S(CSN_WINDOW),                 0, ":CsnArr;",            "k",                              (SUBR) csnarray_window_function_k_init,      (SUBR) csnarray_hamming_k,              (SUBR) csnarray_window_deinit,          NULL, 0 },
    { "csnbartlett",           S(CSN_WINDOW),                 0, ":CsnArr;",            "i",                              (SUBR) csnarray_bartlett,                    NULL,                                   (SUBR) csnarray_window_deinit,          NULL, 0 },
    { "csnbartlett.k",         S(CSN_WINDOW),                 0, ":CsnArr;",            "k",                              (SUBR) csnarray_window_function_k_init,      (SUBR) csnarray_bartlett_k,             (SUBR) csnarray_window_deinit,          NULL, 0 },
    { "csnblackman",           S(CSN_WINDOW),                 0, ":CsnArr;",            "i",                              (SUBR) csnarray_blackman,                    NULL,                                   (SUBR) csnarray_window_deinit,          NULL, 0 },
    { "csnblackman.k",         S(CSN_WINDOW),                 0, ":CsnArr;",            "k",                              (SUBR) csnarray_window_function_k_init,      (SUBR) csnarray_blackman_k,             (SUBR) csnarray_window_deinit,          NULL, 0 },
    { "csnkaiser",             S(CSN_WINDOW),                 0, ":CsnArr;",            "ii",                             (SUBR) csnarray_kaiser,                      NULL,                                   (SUBR) csnarray_window_deinit,          NULL, 0 },
    { "csnkaiser.k",           S(CSN_WINDOW),                 0, ":CsnArr;",            "kk",                             (SUBR) csnarray_window_function_k_init,      (SUBR) csnarray_kaiser_k,               (SUBR) csnarray_window_deinit,          NULL, 0 },
    { "csnkaiser.ik",          S(CSN_WINDOW),                 0, ":CsnArr;",            "ik",                             (SUBR) csnarray_window_function_k_init,      (SUBR) csnarray_kaiser_k,               (SUBR) csnarray_window_deinit,          NULL, 0 },
    { "csndivmod.hh",          S(CSN_DIVMOD_HH),              0, ":CsnArr;:CsnArr;",    ":CsnArr;:CsnArr;",               (SUBR) csnarray_divmod_hh,                   NULL,                                   (SUBR) csnarray_divmod_deinit,          NULL, 0 },
    { "csndivmod.hs",          S(CSN_DIVMOD_HS),              0, ":CsnArr;:CsnArr;",    ":CsnArr;i",                      (SUBR) csnarray_divmod_hs,                   NULL,                                   (SUBR) csnarray_divmod_deinit,          NULL, 0 },
    { "csndivmod.sh",          S(CSN_DIVMOD_SH),              0, ":CsnArr;:CsnArr;",    "i:CsnArr;",                      (SUBR) csnarray_divmod_sh,                   NULL,                                   (SUBR) csnarray_divmod_deinit,          NULL, 0 },
    { "csndivmod.hh.k",        S(CSN_DIVMOD_HH),              0, ":CsnArr;:CsnArr;",    ":CsnArr;:CsnArr;P",              (SUBR) csnarray_divmod_hh_k_init,            (SUBR) csnarray_divmod_hh_k,            (SUBR) csnarray_divmod_deinit,          NULL, 0 },
    { "csndivmod.hs.k",        S(CSN_DIVMOD_HS),              0, ":CsnArr;:CsnArr;",    ":CsnArr;k",                      (SUBR) csnarray_divmod_hs_k_init,            (SUBR) csnarray_divmod_hs_k,            (SUBR) csnarray_divmod_deinit,          NULL, 0 },
    { "csndivmod.sh.k",        S(CSN_DIVMOD_SH),              0, ":CsnArr;:CsnArr;",    "k:CsnArr;",                      (SUBR) csnarray_divmod_sh_k_init,            (SUBR) csnarray_divmod_sh_k,            (SUBR) csnarray_divmod_deinit,          NULL, 0 },
    { "csnfromftable",         S(CSN_FROM_FTABLE),            0, ":CsnArr;",             "i",                             (SUBR) from_ftable_to_csnarray,              NULL,                                   (SUBR) csnarray_from_ftable_deinit,     NULL, 0 },
    { "csntoftable",           S(CSN_TO_FTABLE),              0, "",                     ":CsnArr;io",                    (SUBR) from_csnarray_to_ftable,              NULL,                                   NULL,                                   NULL, 0 },
    { "csninterp",             S(CSN_REMAP),                  0, ":CsnArr;",             ":CsnArr;:CsnArr;:CsnArr;iioJP", (SUBR) csnarray_remap_k_init,                (SUBR) csnarray_remap_k,                (SUBR) csnarray_remap_deinit,           NULL, 0 },
    { "csninterp.s",           S(CSN_REMAP_SCALAR),           0, "i",                    "k:CsnArr;:CsnArr;iio",          (SUBR) csnarray_remap_scalar,                NULL,                                   NULL,                                   NULL, 0 },
    { "csninterp.s.k",         S(CSN_REMAP_SCALAR),           0, "k",                    "k:CsnArr;:CsnArr;iioP",         (SUBR) csnarray_remap_scalar_k_init,         (SUBR) csnarray_remap_scalar_k,         NULL,                                   NULL, 0 },
    { "csnresample",           S(CSN_RESAMPLE),               0, ":CsnArr;",             ":CsnArr;iiioj",                 (SUBR) csnarray_resample,                    NULL,                                   (SUBR) csnarray_resample_deinit,        NULL, 0 },
    { "csnresample.k",         S(CSN_RESAMPLE),               0, ":CsnArr;",             ":CsnArr;kiioJP",                (SUBR) csnarray_resample_k_init,             (SUBR) csnarray_resample_k,             (SUBR) csnarray_resample_deinit,        NULL, 0 },
    { "csnwhere.hh",           S(CSN_WHERE_HH),               0, ":CsnArr;",             ":CsnArr;:CsnArr;:CsnArr;",      (SUBR) csnarray_where_hh,                    NULL,                                   (SUBR) csnarray_where_deinit,           NULL, 0 },
    { "csnwhere.hs",           S(CSN_WHERE_HS),               0, ":CsnArr;",             ":CsnArr;:CsnArr;i",             (SUBR) csnarray_where_hs,                    NULL,                                   (SUBR) csnarray_where_deinit,           NULL, 0 },
    { "csnwhere.hh.k",         S(CSN_WHERE_HH),               0, ":CsnArr;",             ":CsnArr;:CsnArr;:CsnArr;P",     (SUBR) csnarray_where_hh_k_init,             (SUBR) csnarray_where_hh_k,             (SUBR) csnarray_where_deinit,           NULL, 0 },
    { "csnwhere.hs.k",         S(CSN_WHERE_HS),               0, ":CsnArr;",             ":CsnArr;:CsnArr;kP",            (SUBR) csnarray_where_hs_k_init,             (SUBR) csnarray_where_hs_k,             (SUBR) csnarray_where_deinit,           NULL, 0 },
    { "csnputmask.hh",         S(CSN_WHERE_HH_IN),            0, "",                     ":CsnArr;:CsnArr;:CsnArr;",      (SUBR) csnarray_where_in_hh,                 NULL,                                   NULL,                                   NULL, 0 },
    { "csnputmask.hs",         S(CSN_WHERE_HS_IN),            0, "",                     ":CsnArr;:CsnArr;i",             (SUBR) csnarray_where_in_hs,                 NULL,                                   NULL,                                   NULL, 0 },
    { "csnputmask.hh.k",       S(CSN_WHERE_HH_IN),            0, "",                     ":CsnArr;:CsnArr;:CsnArr;P",     (SUBR) csnarray_where_in_hh_k_init,          (SUBR) csnarray_where_in_hh_k,          NULL,                                   NULL, 0 },
    { "csnputmask.hs.k",       S(CSN_WHERE_HS_IN),            0, "",                     ":CsnArr;:CsnArr;kP",            (SUBR) csnarray_where_in_hs_k_init,          (SUBR) csnarray_where_in_hs_k,          NULL,                                   NULL, 0 },
    { "csnminimum.hh",         S(CSN_BINOP_HH),               0, ":CsnArr;",             ":CsnArr;:CsnArr;",              (SUBR) csnarray_minimum_hh,                  NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnminimum.hh.k",       S(CSN_BINOP_HH),               0, ":CsnArr;",             ":CsnArr;:CsnArr;P",             (SUBR) csnarray_minimum_hh_k_init,           (SUBR) csnarray_minimum_hh_k,           (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnminimum.hs",         S(CSN_BINOP_HS),               0, ":CsnArr;",             ":CsnArr;i",                     (SUBR) csnarray_minimum_hs,                  NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnminimum.hs.k",       S(CSN_BINOP_HS),               0, ":CsnArr;",             ":CsnArr;kP",                    (SUBR) csnarray_minimum_hs_k_init,           (SUBR) csnarray_minimum_hs_k,           (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnmaximum.hh",         S(CSN_BINOP_HH),               0, ":CsnArr;",             ":CsnArr;:CsnArr;",              (SUBR) csnarray_maximum_hh,                  NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnmaximum.hh.k",       S(CSN_BINOP_HH),               0, ":CsnArr;",             ":CsnArr;:CsnArr;P",             (SUBR) csnarray_maximum_hh_k_init,           (SUBR) csnarray_maximum_hh_k,           (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnmaximum.hs",         S(CSN_BINOP_HS),               0, ":CsnArr;",             ":CsnArr;i",                     (SUBR) csnarray_maximum_hs,                  NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnmaximum.hs.k",       S(CSN_BINOP_HS),               0, ":CsnArr;",             ":CsnArr;kP",                    (SUBR) csnarray_maximum_hs_k_init,           (SUBR) csnarray_maximum_hs_k,           (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnatan2.hh",           S(CSN_BINOP_HH),               0, ":CsnArr;",             ":CsnArr;:CsnArr;",              (SUBR) csnarray_atan2_hh,                    NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnatan2.hh.k",         S(CSN_BINOP_HH),               0, ":CsnArr;",             ":CsnArr;:CsnArr;P",             (SUBR) csnarray_atan2_hh_k_init,             (SUBR) csnarray_atan2_hh_k,             (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnatan2.hs",           S(CSN_BINOP_HS),               0, ":CsnArr;",             ":CsnArr;i",                     (SUBR) csnarray_atan2_hs,                    NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnatan2.hs.k",         S(CSN_BINOP_HS),               0, ":CsnArr;",             ":CsnArr;kP",                    (SUBR) csnarray_atan2_hs_k_init,             (SUBR) csnarray_atan2_hs_k,             (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnatan2.sh",           S(CSN_BINOP_SH),               0, ":CsnArr;",             "i:CsnArr;",                     (SUBR) csnarray_atan2_sh,                    NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnatan2.sh.k",         S(CSN_BINOP_SH),               0, ":CsnArr;",             "k:CsnArr;P",                    (SUBR) csnarray_atan2_sh_k_init,             (SUBR) csnarray_atan2_sh_k,             (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnrms",                S(CSN_REDUCTION_SCALAR),       0, "i",                    ":CsnArr;",                      (SUBR) csnarray_rms_all,                     NULL,                                   NULL,                                   NULL, 0 },
    { "csnrms.ax",             S(CSN_REDUCTION),              0, ":CsnArr;",             ":CsnArr;i",                     (SUBR) csnarray_rms,                         NULL,                                   (SUBR) csnarray_reduction_deinit,       NULL, 0 },
    { "csnrms.k",              S(CSN_REDUCTION_SCALAR),       0, "k",                    ":CsnArr;P",                     (SUBR) csnarray_rms_all_k_init,              (SUBR) csnarray_rms_all_k,              NULL,                                   NULL, 0 },
    { "csnrms.ax.k",           S(CSN_REDUCTION),              0, ":CsnArr;",             ":CsnArr;kP",                    (SUBR) csnarray_rms_k_init,                  (SUBR) csnarray_rms_k,                  (SUBR) csnarray_reduction_deinit,       NULL, 0 },
    { "csnisnan",              S(CSN_UNARYOP),                0, ":CsnArr;",             ":CsnArr;",                      (SUBR) csnarray_isnan,                       NULL,                                   (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csnisnan.k",            S(CSN_UNARYOP),                0, ":CsnArr;",             ":CsnArr;P",                     (SUBR) csnarray_isnan,                       (SUBR) csnarray_isnan_k,                (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csnisinf",              S(CSN_UNARYOP),                0, ":CsnArr;",             ":CsnArr;",                      (SUBR) csnarray_isinf,                       NULL,                                   (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csnisinf.k",            S(CSN_UNARYOP),                0, ":CsnArr;",             ":CsnArr;P",                     (SUBR) csnarray_isinf,                       (SUBR) csnarray_isinf_k,                (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csnisfin",              S(CSN_UNARYOP),                0, ":CsnArr;",             ":CsnArr;",                      (SUBR) csnarray_isfin,                       NULL,                                   (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csnisfin.k",            S(CSN_UNARYOP),                0, ":CsnArr;",             ":CsnArr;P",                     (SUBR) csnarray_isfin,                       (SUBR) csnarray_isfin_k,                (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    // ---
    // REAL AND COMPLEX
    { "csnempty",              S(CSN_ARR_INIT),               0, ":CsnArr;",             "i[]o",                          (SUBR) create_empty_csnarray,                NULL,                                   (SUBR) create_csnarray_deinit,          NULL, 0 },
    { "csnempty.k",            S(CSN_ARR_INIT),               0, ":CsnArr;",             "k[]o",                          (SUBR) create_empty_csnarray_k_init,         (SUBR) create_empty_csnarray_k,         (SUBR) create_csnarray_deinit,          NULL, 0 },
    { "csnzeros",              S(CSN_ARR_INIT),               0, ":CsnArr;",             "i[]o",                          (SUBR) create_zeros_csnarray,                NULL,                                   (SUBR) create_csnarray_deinit,          NULL, 0 },
    { "csnzeros.k",            S(CSN_ARR_INIT),               0, ":CsnArr;",             "k[]o",                          (SUBR) create_zeros_csnarray_k_init,         (SUBR) create_zeros_csnarray_k,         (SUBR) create_csnarray_deinit,          NULL, 0 },
    { "csnones",               S(CSN_ARR_INIT),               0, ":CsnArr;",             "i[]o",                          (SUBR) create_ones_csnarray,                 NULL,                                   (SUBR) create_csnarray_deinit,          NULL, 0 },
    { "csnones.k",             S(CSN_ARR_INIT),               0, ":CsnArr;",             "k[]o",                          (SUBR) create_ones_csnarray_k_init,          (SUBR) create_ones_csnarray_k,          (SUBR) create_csnarray_deinit,          NULL, 0 },
    { "csnfull",               S(CSN_FULL),                   0, ":CsnArr;",             "i[]io",                         (SUBR) create_full_csnarray,                 NULL,                                   (SUBR) create_csnarray_full_deinit,     NULL, 0 },
    { "csnfull.c",             S(CSN_FULLCOMPLEX),            0, ":CsnArr;",             "i[]:Complex;" ,                 (SUBR) create_fullcomp_csnarray,             NULL,                                   (SUBR) create_csnarray_fullcomp_deinit, NULL, 0 },
    { "csnfull.k",             S(CSN_FULL),                   0, ":CsnArr;",             "k[]ko",                         (SUBR) create_full_csnarray_k_init,          (SUBR) create_full_csnarray_k,          (SUBR) create_csnarray_full_deinit,     NULL, 0 },
    { "csnfull.c.k",           S(CSN_FULLCOMPLEX),            0, ":CsnArr;",             "k[]:Complex;" ,                 (SUBR) create_fullcomp_csnarray_k_init,      (SUBR) create_fullcomp_csnarray_k,      (SUBR) create_csnarray_fullcomp_deinit, NULL, 0 },
    { "csnlike",               S(CSN_ARR_INIT_LIKE),          0, ":CsnArr;",             ":CsnArr;i",                     (SUBR) create_like_csnarray,                 NULL,                                   (SUBR) create_csnarray_like_deinit,     NULL, 0 },
    { "csnlike.k",             S(CSN_ARR_INIT_LIKE),          0, ":CsnArr;",             ":CsnArr;k",                     (SUBR) create_like_csnarray_k_init,          (SUBR) create_like_csnarray_k,          (SUBR) create_csnarray_like_deinit,     NULL, 0 },
    { "csnfromarray",          S(CSN_FROM_ARRAY),             0, ":CsnArr;",             "i[]",                           (SUBR) from_array_to_csnarray,               NULL,                                   (SUBR) from_array_to_csnarray_deinit,   NULL, 0 },
    { "csnfromarray.c",        S(CSN_FROM_ARRAY),             0, ":CsnArr;",             ":Complex;[]",                   (SUBR) from_complexarray_to_csnarray_k_init, (SUBR) from_complexarray_to_csnarray_k, (SUBR) from_array_to_csnarray_deinit,   NULL, 0 },
    { "csnfromarray.k",        S(CSN_FROM_ARRAY),             0, ":CsnArr;",             "k[]",                           (SUBR) from_array_to_csnarray_k_init,        (SUBR) from_array_to_csnarray_k,        (SUBR) from_array_to_csnarray_deinit,   NULL, 0 },
    { "csntoarray",            S(CSN_TO_ARRAY),               0, "i[]",                  ":CsnArr;",                      (SUBR) from_csnarray_to_array,               NULL,                                   NULL,                                   NULL, 0 },
    { "csntoarray.k",          S(CSN_TO_ARRAY),               0, "k[]",                  ":CsnArr;",                      (SUBR) from_csnarray_to_array,               (SUBR) from_csnarray_to_array_k,        NULL,                                   NULL, 0 },
    { "csntoarray.c",          S(CSN_TO_ARRAY),               0, ":Complex;[]",          ":CsnArr;",                      (SUBR) from_csnarray_to_complexarray,        (SUBR) from_csnarray_to_complexarray_k, NULL,                                   NULL, 0 },
    { "csnfree",               S(CSN_FREE),                   0, "",                     ":CsnArr;",                      (SUBR) free_csnarray,                        NULL,                                   NULL,                                   NULL, 0 },
    { "csndims",               S(CSN_SIZE_DIMS),              0, "i",                    ":CsnArr;",                      (SUBR) csnarray_dims,                        NULL,                                   NULL,                                   NULL, 0 },
    { "csnsize",               S(CSN_SIZE_DIMS),              0, "i",                    ":CsnArr;",                      (SUBR) csnarray_size,                        NULL,                                   NULL,                                   NULL, 0 },
    { "csnisempty",            S(CSN_SIZE_DIMS),              0, "i",                    ":CsnArr;",                      (SUBR) csnarray_is_empty,                    NULL,                                   NULL,                                   NULL, 0 },
    { "csnshape",              S(CSN_SHAPE),                  0, "i[]",                  ":CsnArr;",                      (SUBR) csnarray_shape,                       NULL,                                   NULL,                                   NULL, 0 },
    { "csndims.k",             S(CSN_SIZE_DIMS),              0, "k",                    ":CsnArr;",                      NULL,                                        (SUBR) csnarray_dims_k,                 NULL,                                   NULL, 0 },
    { "csnsize.k",             S(CSN_SIZE_DIMS),              0, "k",                    ":CsnArr;",                      NULL,                                        (SUBR) csnarray_size_k,                 NULL,                                   NULL, 0 },
    { "csnisempty.k",          S(CSN_SIZE_DIMS),              0, "k",                    ":CsnArr;",                      NULL,                                        (SUBR) csnarray_is_empty_k,             NULL,                                   NULL, 0 },
    { "csnshape.k",            S(CSN_SHAPE),                  0, "k[]",                  ":CsnArr;",                      (SUBR) csnarray_shape,                       (SUBR) csnarray_shape_k,                NULL,                                   NULL, 0 },
    { "csnidentity",           S(CSN_IDENTITY),               0, ":CsnArr;",             "io",                            (SUBR) csnarray_identity,                    NULL,                                   (SUBR) csnarray_identity_deinit,        NULL, 0 },
    { "csnidentity.k",         S(CSN_IDENTITY),               0, ":CsnArr;",             "ko",                            (SUBR) csnarray_identity_k_init,             (SUBR) csnarray_identity_k,             (SUBR) csnarray_identity_deinit,        NULL, 0 },
    { "csnreshape",            S(CSN_RESHAPE),                0, ":CsnArr;",             ":CsnArr;i[]",                   (SUBR) csnarray_reshape,                     NULL,                                   (SUBR) csnarray_shape_deinit,           NULL, 0 },
    { "csnreshape.in",         S(CSN_RESHAPE_IN),             0, "",                     ":CsnArr;i[]",                   (SUBR) csnarray_reshape_in,                  NULL,                                   NULL,                                   NULL, 0 },
    { "csnreshape.k",          S(CSN_RESHAPE),                0, ":CsnArr;",             ":CsnArr;k[]",                   (SUBR) csnarray_reshape_k_init,              (SUBR) csnarray_reshape_k,              (SUBR) csnarray_shape_deinit,           NULL, 0 },
    { "csnreshape.in.k",       S(CSN_RESHAPE_IN),             0, "",                     ":CsnArr;k[]",                   (SUBR) csnarray_reshape_in_k_init,           (SUBR) csnarray_reshape_in_k,           NULL,                                   NULL, 0 },
    { "csnflatten",            S(CSN_RESHAPE),                0, ":CsnArr;",             ":CsnArr;",                      (SUBR) csnarray_flatten,                     (SUBR) csnarray_flatten_k,              (SUBR) csnarray_shape_deinit,           NULL, 0 },
    { "csnflatten.in",         S(CSN_RESHAPE_IN),             0, "",                     ":CsnArr;",                      (SUBR) csnarray_flatten_in,                  (SUBR) csnarray_flatten_in_k,           NULL,                                   NULL, 0 },
    { "csntranspose",          S(CSN_RESHAPE),                0, ":CsnArr;",             ":CsnArr;",                      (SUBR) csnarray_transpose,                   (SUBR) csnarray_transpose_k,            (SUBR) csnarray_shape_deinit,           NULL, 0 },
    { "csntranspose.ax",       S(CSN_RESHAPE),                0, ":CsnArr;",             ":CsnArr;i[]",                   (SUBR) csnarray_transpose,                   (SUBR) csnarray_transpose_k,            (SUBR) csnarray_shape_deinit,           NULL, 0 },
    { "csntranspose.ax.k",     S(CSN_RESHAPE),                0, ":CsnArr;",             ":CsnArr;k[]",                   (SUBR) csnarray_transpose,                   (SUBR) csnarray_transpose_k,            (SUBR) csnarray_shape_deinit,           NULL, 0 },
    { "csntranspose.in",       S(CSN_RESHAPE_IN),             0, "",                     ":CsnArr;",                      (SUBR) csnarray_transpose_in_k_init,         (SUBR) csnarray_transpose_in_k,         (SUBR) csnarray_transpose_in_k_deinit,  NULL, 0 },
    { "csntranspose.ax.in",    S(CSN_RESHAPE_IN),             0, "",                     ":CsnArr;i[]",                   (SUBR) csnarray_transpose_in,                NULL,                                   NULL,                                   NULL, 0 },
    { "csntranspose.ax.in.k",  S(CSN_RESHAPE_IN),             0, "",                     ":CsnArr;k[]",                   (SUBR) csnarray_transpose_in_k_init,         (SUBR) csnarray_transpose_in_k,         (SUBR) csnarray_transpose_in_k_deinit,  NULL, 0 },
    { "csnflip",               S(CSN_FLIP_ROLL),              0, ":CsnArr;",             ":CsnArr;j",                     (SUBR) csnarray_flip,                        NULL,                                   (SUBR) csnarray_flip_deinit,            NULL, 0 },
    { "csnflip.in",            S(CSN_FLIP_ROLL_IN),           0, "",                     ":CsnArr;j",                     (SUBR) csnarray_flip_in,                     NULL,                                   NULL,                                   NULL, 0 },
    { "csnflip.k",             S(CSN_FLIP_ROLL),              0, ":CsnArr;",             ":CsnArr;J",                     (SUBR) csnarray_flip,                        (SUBR) csnarray_flip_k,                 (SUBR) csnarray_flip_deinit,            NULL, 0 },
    { "csnflip.in.k",          S(CSN_FLIP_ROLL_IN),           0, "",                     ":CsnArr;J",                     (SUBR) csnarray_flip_in_k_init,              (SUBR) csnarray_flip_in_k,              (SUBR) csnarray_flip_in_k_deinit,       NULL, 0 },
    { "csnroll",               S(CSN_FLIP_ROLL),              0, ":CsnArr;",             ":CsnArr;i",                     (SUBR) csnarray_roll,                        NULL,                                   (SUBR) csnarray_flip_deinit,            NULL, 0 },
    { "csnroll.in",            S(CSN_FLIP_ROLL_IN),           0, "",                     ":CsnArr;i",                     (SUBR) csnarray_roll_in,                     NULL,                                   NULL,                                   NULL, 0 },
    { "csnroll.ax",            S(CSN_FLIP_ROLL),              0, ":CsnArr;",             ":CsnArr;ij",                    (SUBR) csnarray_rollaxis,                    NULL,                                   (SUBR) csnarray_flip_deinit,            NULL, 0 },
    { "csnroll.ax.in",         S(CSN_FLIP_ROLL_IN),           0, "",                     ":CsnArr;ij",                    (SUBR) csnarray_rollaxis_in,                 NULL,                                   NULL,                                   NULL, 0 },
    { "csnroll.k",             S(CSN_FLIP_ROLL),              0, ":CsnArr;",             ":CsnArr;k",                     (SUBR) csnarray_roll,                        (SUBR) csnarray_roll_k,                 (SUBR) csnarray_flip_deinit,            NULL, 0 },
    { "csnroll.in.k",          S(CSN_FLIP_ROLL_IN),           0, "",                     ":CsnArr;k",                     (SUBR) csnarray_roll_in_k_init,              (SUBR) csnarray_roll_in_k,              (SUBR) csnarray_flip_in_k_deinit,       NULL, 0 },
    { "csnroll.ax.k",          S(CSN_FLIP_ROLL),              0, ":CsnArr;",             ":CsnArr;kJ",                    (SUBR) csnarray_rollaxis,                    (SUBR) csnarray_rollaxis_k,             (SUBR) csnarray_flip_deinit,            NULL, 0 },
    { "csnroll.ax.in.k",       S(CSN_FLIP_ROLL_IN),           0, "",                     ":CsnArr;kJ",                    (SUBR) csnarray_rollaxis_in_k_init,          (SUBR) csnarray_rollaxis_in_k,          (SUBR) csnarray_flip_in_k_deinit,       NULL, 0 },
    { "csnget",                S(CSN_GET),                    0, "i",                    ":CsnArr;i[]",                   (SUBR) csnarray_get,                         NULL,                                   NULL,                                   NULL, 0 },
    { "csnget.c",              S(CSN_GETCOMPLEX),             0, ":Complex;",            ":CsnArr;i[]",                   (SUBR) csnarray_get_complex,                 NULL,                                   NULL,                                   NULL, 0 },
    { "csnget.k",              S(CSN_GET),                    0, "k",                    ":CsnArr;k[]",                   (SUBR) csnarray_get,                         (SUBR) csnarray_get_k,                  NULL,                                   NULL, 0 },
    { "csnget.c.k",            S(CSN_GETCOMPLEX),             0, ":Complex;",            ":CsnArr;k[]",                   (SUBR) csnarray_get_complex,                 (SUBR) csnarray_get_complex_k,          NULL,                                   NULL, 0 },
    { "csnset",                S(CSN_SET),                    0, "",                     ":CsnArr;i[]i",                  (SUBR) csnarray_set,                         NULL,                                   NULL,                                   NULL, 0 },
    { "csnset.c",              S(CSN_SETCOMPLEX),             0, "",                     ":CsnArr;i[]:Complex;",          (SUBR) csnarray_set_complex,                 NULL,                                   NULL,                                   NULL, 0 },
    { "csnset.kk",             S(CSN_SET),                    0, "",                     ":CsnArr;k[]k",                  (SUBR) csnarray_set,                         (SUBR) csnarray_set_k,                  NULL,                                   NULL, 0 },
    { "csnset.c.k",            S(CSN_SETCOMPLEX),             0, "",                     ":CsnArr;k[]:Complex;",          (SUBR) csnarray_set_complex,                 (SUBR) csnarray_set_complex_k,          NULL,                                   NULL, 0 },
    { "csnset.ik",             S(CSN_SET),                    0, "",                     ":CsnArr;i[]k",                  (SUBR) csnarray_set,                         (SUBR) csnarray_set_k,                  NULL,                                   NULL, 0 },
    { "csnset.ki",             S(CSN_SET),                    0, "",                     ":CsnArr;k[]i",                  (SUBR) csnarray_set,                         (SUBR) csnarray_set_k,                  NULL,                                   NULL, 0 },
    { "csntake",               S(CSN_TAKE),                   0, ":CsnArr;",             ":CsnArr;ii",                    (SUBR) csnarray_take,                        NULL,                                   (SUBR) csnarray_take_deinit,            NULL, 0 },
    { "csntake.kk",            S(CSN_TAKE),                   0, ":CsnArr;",             ":CsnArr;kk",                    (SUBR) csnarray_take,                        (SUBR) csnarray_take_k,                 (SUBR) csnarray_take_deinit,            NULL, 0 },
    { "csntake.ik",            S(CSN_TAKE),                   0, ":CsnArr;",             ":CsnArr;ik",                    (SUBR) csnarray_take,                        (SUBR) csnarray_take_k,                 (SUBR) csnarray_take_deinit,            NULL, 0 },
    { "csntake.ki",            S(CSN_TAKE),                   0, ":CsnArr;",             ":CsnArr;ki",                    (SUBR) csnarray_take,                        (SUBR) csnarray_take_k,                 (SUBR) csnarray_take_deinit,            NULL, 0 },
    { "csntake.flat",          S(CSN_TAKE_FLAT),              0, "i",                    ":CsnArr;i",                     (SUBR) csnarray_take_flat,                   NULL,                                   NULL,                                   NULL, 0 },
    { "csntake.flat.c",        S(CSN_TAKECOMPLEX_FLAT),       0, ":Complex;",            ":CsnArr;i",                     (SUBR) csnarray_takecomp_flat,               NULL,                                   NULL,                                   NULL, 0 },
    { "csntake.flat.k",        S(CSN_TAKE_FLAT),              0, "k",                    ":CsnArr;k",                     (SUBR) csnarray_take_flat,                   (SUBR) csnarray_take_flat_k,            NULL,                                   NULL, 0 },
    { "csntake.flat.c.k",      S(CSN_TAKECOMPLEX_FLAT),       0, ":Complex;",            ":CsnArr;k",                     (SUBR) csnarray_takecomp_flat,               (SUBR) csnarray_takecomp_flat_k,        NULL,                                   NULL, 0 },
    { "csngetslice",           S(CSN_GET_SLICE),              0, ":CsnArr;",             ":CsnArr;iiii",                  (SUBR) csnarray_get_slice,                   NULL,                                   (SUBR) csnarray_slice_deinit,           NULL, 0 },
    { "csngetslice.k",         S(CSN_GET_SLICE),              0, ":CsnArr;",             ":CsnArr;kkkk",                  (SUBR) csnarray_get_slice_k_init,                   (SUBR) csnarray_get_slice_k,            (SUBR) csnarray_slice_deinit,           NULL, 0 },
    { "csnsetslice",           S(CSN_SET_SLICE),              0, "",                     ":CsnArr;:CsnArr;iiii",          (SUBR) csnarray_set_slice,                   NULL,                                   NULL,                                   NULL, 0 },
    { "csnsetslice.k",         S(CSN_SET_SLICE),              0, "",                     ":CsnArr;:CsnArr;kkkk",          (SUBR) csnarray_set_slice,                   (SUBR) csnarray_set_slice_k,            NULL,                                   NULL, 0 },
    { "csnpush",               S(CSN_PUSH),                   0, "",                     ":CsnArr;i",                     (SUBR) csnarray_push,                        NULL,                                   NULL,                                   NULL, 0 },
    { "csnpush.c",             S(CSN_PUSHCOMPLEX),            0, "",                     ":CsnArr;:Complex;",             (SUBR) csnarray_pushcomp,                    NULL,                                   NULL,                                   NULL, 0 },
    { "csnpush.k",             S(CSN_PUSH_K),                 0, "",                     ":CsnArr;kk",                    (SUBR) csnarray_push_k_init,                 (SUBR) csnarray_push_k,                 NULL,                                   NULL, 0 },
    { "csnpush.c.k",           S(CSN_PUSHCOMPLEX_K),          0, "",                     ":CsnArr;:Complex;k",            (SUBR) csnarray_pushcomp_k_init,             (SUBR) csnarray_pushcomp_k,             NULL,                                   NULL, 0 },
    { "csnpop",                S(CSN_POP),                    0, "i",                    ":CsnArr;",                      (SUBR) csnarray_pop,                         NULL,                                   NULL,                                   NULL, 0 },
    { "csnpop.c",              S(CSN_POPCOMPLEX),             0, ":Complex;",            ":CsnArr;",                      (SUBR) csnarray_popcomp,                     NULL,                                   NULL,                                   NULL, 0 },
    { "csnpop.k",              S(CSN_POP_K),                  0, "k",                    ":CsnArr;k",                     (SUBR) csnarray_pop_k_init,                  (SUBR) csnarray_pop_k,                  NULL,                                   NULL, 0 },
    { "csnpop.c.k",            S(CSN_POPCOMPLEX_K),           0, ":Complex;",            ":CsnArr;k",                     (SUBR) csnarray_popcomp_k_init,              (SUBR) csnarray_popcomp_k,              NULL,                                   NULL, 0 },
    { "csninsert.flat",        S(CSN_PUSH),                   0, "",                     ":CsnArr;ii",                    (SUBR) csnarray_insert,                      NULL,                                   NULL,                                   NULL, 0 },
    { "csninsert.flat.c",      S(CSN_PUSHCOMPLEX),            0, "",                     ":CsnArr;:Complex;i",            (SUBR) csnarray_insertcomp,                  NULL,                                   NULL,                                   NULL, 0 },
    { "csninsert.flat.k",      S(CSN_PUSH_K),                 0, "",                     ":CsnArr;kkk",                   (SUBR) csnarray_push_k_init,                 (SUBR) csnarray_insert_k,               NULL,                                   NULL, 0 },
    { "csninsert.flat.c.k",    S(CSN_PUSHCOMPLEX_K),          0, "",                     ":CsnArr;:Complex;kk",           (SUBR) csnarray_pushcomp_k_init,             (SUBR) csnarray_insertcomp_k,           NULL,                                   NULL, 0 },
    { "csnremove.flat",        S(CSN_POP),                    0, "i",                    ":CsnArr;i",                     (SUBR) csnarray_remove,                      NULL,                                   NULL,                                   NULL, 0 },
    { "csnremove.flat.c",      S(CSN_POPCOMPLEX),             0, ":Complex;",            ":CsnArr;i",                     (SUBR) csnarray_removecomp,                  NULL,                                   NULL,                                   NULL, 0 },
    { "csnremove.flat.k",      S(CSN_POP_K),                  0, "k",                    ":CsnArr;kk",                    (SUBR) csnarray_pop_k_init,                  (SUBR) csnarray_remove_k,               NULL,                                   NULL, 0 },
    { "csnremove.flat.c.k",    S(CSN_POPCOMPLEX_K),           0, ":Complex;",            ":CsnArr;kk",                    (SUBR) csnarray_popcomp_k_init,              (SUBR) csnarray_removecomp_k,           NULL,                                   NULL, 0 },
    { "csninsert.block",       S(CSN_INSERT_BLOCK),           0, "",                     ":CsnArr;:CsnArr;ii",            (SUBR) csnarray_insert_block,                NULL,                                   (SUBR) csnarray_insert_block_deinit,    NULL, 0 },
    { "csninsert.block.k",     S(CSN_INSERT_BLOCK),           0, "",                     ":CsnArr;:CsnArr;kkP",           (SUBR) csnarray_insert_block_k_init,         (SUBR) csnarray_insert_block_k,         (SUBR) csnarray_insert_block_deinit,    NULL, 0 },
    { "csnremove.block",       S(CSN_TAKE),                   0, ":CsnArr;",             ":CsnArr;ii",                    (SUBR) csnarray_remove_block,                NULL,                                   (SUBR) csnarray_take_deinit,            NULL, 0 },
    { "csnremove.block.k",     S(CSN_TAKE),                   0, ":CsnArr;",             ":CsnArr;kkP",                   (SUBR) csnarray_remove_block_k_init,         (SUBR) csnarray_remove_block_k,         (SUBR) csnarray_take_deinit,            NULL, 0 },
    { "csnconcat.block",       S(CSN_CONCAT),                 0, ":CsnArr;",             ":CsnArr;:CsnArr;i",             (SUBR) csnarray_concat_block,                NULL,                                   (SUBR) csnarray_concat_deinit,          NULL, 0 },
    { "csnconcat.block.k",     S(CSN_CONCAT),                 0, ":CsnArr;",             ":CsnArr;:CsnArr;kk",            (SUBR) csnarray_concat_block_k_init,         (SUBR) csnarray_concat_block_k,         (SUBR) csnarray_concat_deinit,          NULL, 0 },
    { "csnconcat.flat",        S(CSN_CONCAT),                 0, ":CsnArr;",             ":CsnArr;:CsnArr;",              (SUBR) csnarray_concat_flat,                 NULL,                                   (SUBR) csnarray_concat_deinit,          NULL, 0 },
    { "csnconcat.flat.k",      S(CSN_CONCAT),                 0, ":CsnArr;",             ":CsnArr;:CsnArr;k",             (SUBR) csnarray_concat_flat,                 (SUBR) csnarray_concat_flat_k,          (SUBR) csnarray_concat_deinit,          NULL, 0 },
    { "csnpad",                S(CSN_PAD),                    0, ":CsnArr;",             ":CsnArr;iio",                   (SUBR) csnarray_pad,                         NULL,                                   (SUBR) csnarray_pad_deinit,             NULL, 0 },
    { "csnpad.ax",             S(CSN_PAD),                    0, ":CsnArr;",             ":CsnArr;iiii",                  (SUBR) csnarray_pad,                         NULL,                                   (SUBR) csnarray_pad_deinit,             NULL, 0 },
    { "csnpad.in",             S(CSN_PAD_IN),                 0, "",                     ":CsnArr;iio",                   (SUBR) csnarray_pad_in,                      NULL,                                   NULL,                                   NULL, 0 },
    { "csnpad.ax.in",          S(CSN_PAD_IN),                 0, "",                     ":CsnArr;iiii",                  (SUBR) csnarray_pad_in,                      NULL,                                   NULL,                                   NULL, 0 },
    { "csnpad.c",              S(CSN_PADCOMPLEX),             0, ":CsnArr;",             ":CsnArr;ii:Complex;",           (SUBR) csnarray_padcomp,                     NULL,                                   (SUBR) csnarray_padcomp_deinit,         NULL, 0 },
    { "csnpad.ax.c",           S(CSN_PADCOMPLEX),             0, ":CsnArr;",             ":CsnArr;ii:Complex;i",          (SUBR) csnarray_padcomp,                     NULL,                                   (SUBR) csnarray_padcomp_deinit,         NULL, 0 },
    { "csnpad.in.c",           S(CSN_PADCOMPLEX_IN),          0, "",                     ":CsnArr;ii:Complex;",           (SUBR) csnarray_padcomp_in,                  NULL,                                   NULL,                                   NULL, 0 },
    { "csnpad.ax.in.c",        S(CSN_PADCOMPLEX_IN),          0, "",                     ":CsnArr;ii:Complex;i",          (SUBR) csnarray_padcomp_in,                  NULL,                                   NULL,                                   NULL, 0 },
    { "csnpad.k",              S(CSN_PAD),                    0, ":CsnArr;",             ":CsnArr;kkkk",                  (SUBR) csnarray_pad_k_init,                  (SUBR) csnarray_pad_k,                  (SUBR) csnarray_pad_deinit,             NULL, 0 },
    { "csnpad.ax.k",           S(CSN_PAD),                    0, ":CsnArr;",             ":CsnArr;kkkkk",                 (SUBR) csnarray_pad_k_init,                  (SUBR) csnarray_pad_k,                  (SUBR) csnarray_pad_deinit,             NULL, 0 },
    { "csnpad.in.k",           S(CSN_PAD_IN),                 0, "",                     ":CsnArr;kkkk",                  (SUBR) csnarray_pad_in_k_init,               (SUBR) csnarray_pad_in_k,               (SUBR) csnarray_pad_in_k_deinit,        NULL, 0 },
    { "csnpad.ax.in.k",        S(CSN_PAD_IN),                 0, "",                     ":CsnArr;kkkkk",                 (SUBR) csnarray_pad_in_k_init,               (SUBR) csnarray_pad_in_k,               (SUBR) csnarray_pad_in_k_deinit,        NULL, 0 },
    { "csnpad.c.k",            S(CSN_PADCOMPLEX),             0, ":CsnArr;",             ":CsnArr;kk:Complex;k",          (SUBR) csnarray_padcomp_k_init,              (SUBR) csnarray_padcomp_k,              (SUBR) csnarray_padcomp_deinit,         NULL, 0 },
    { "csnpad.ax.c.k",         S(CSN_PADCOMPLEX),             0, ":CsnArr;",             ":CsnArr;kk:Complex;kk",         (SUBR) csnarray_padcomp_k_init,              (SUBR) csnarray_padcomp_k,              (SUBR) csnarray_padcomp_deinit,         NULL, 0 },
    { "csnpad.in.c.k",         S(CSN_PADCOMPLEX_IN),          0, "",                     ":CsnArr;kk:Complex;k",          (SUBR) csnarray_padcomp_in_k_init,           (SUBR) csnarray_padcomp_in_k,           (SUBR) csnarray_padcomp_in_k_deinit,    NULL, 0 },
    { "csnpad.ax.in.c.k",      S(CSN_PADCOMPLEX_IN),          0, "",                     ":CsnArr;kk:Complex;kk",         (SUBR) csnarray_padcomp_in_k_init,           (SUBR) csnarray_padcomp_in_k,           (SUBR) csnarray_padcomp_in_k_deinit,    NULL, 0 },
    { "csnsum",                S(CSN_REDUCTION_SCALAR),       0, "i",                    ":CsnArr;",                      (SUBR) csnarray_sum_all,                     NULL,                                   NULL,                                   NULL, 0 },
    { "csnsum.k",              S(CSN_REDUCTION_SCALAR),       0, "k",                    ":CsnArr;P",                     (SUBR) csnarray_sum_all_k_init,              (SUBR) csnarray_sum_all_k,              NULL,                                   NULL, 0 },
    { "csnprod",               S(CSN_REDUCTION_SCALAR),       0, "i",                    ":CsnArr;",                      (SUBR) csnarray_prod_all,                    NULL,                                   NULL,                                   NULL, 0 },
    { "csnprod.k",             S(CSN_REDUCTION_SCALAR),       0, "k",                    ":CsnArr;P",                     (SUBR) csnarray_prod_all_k_init,             (SUBR) csnarray_prod_all_k,             NULL,                                   NULL, 0 },
    { "csnsub",                S(CSN_REDUCTION_SCALAR),       0, "i",                    ":CsnArr;",                      (SUBR) csnarray_sub_all,                     NULL,                                   NULL,                                   NULL, 0 },
    { "csnsub.k",              S(CSN_REDUCTION_SCALAR),       0, "k",                    ":CsnArr;P",                     (SUBR) csnarray_sub_all_k_init,              (SUBR) csnarray_sub_all_k,              NULL,                                   NULL, 0 },
    { "csnmean",               S(CSN_REDUCTION_SCALAR),       0, "i",                    ":CsnArr;",                      (SUBR) csnarray_mean_all,                    NULL,                                   NULL,                                   NULL, 0 },
    { "csnmean.k",             S(CSN_REDUCTION_SCALAR),       0, "k",                    ":CsnArr;P",                     (SUBR) csnarray_mean_all_k_init,             (SUBR) csnarray_mean_all_k,             NULL,                                   NULL, 0 },
    { "csnall",                S(CSN_REDUCTION_SCALAR),       0, "i",                    ":CsnArr;",                      (SUBR) csnarray_all_all,                     NULL,                                   NULL,                                   NULL, 0 },
    { "csnall.k",              S(CSN_REDUCTION_SCALAR),       0, "k",                    ":CsnArr;P",                     (SUBR) csnarray_all_all_k_init,              (SUBR) csnarray_all_all_k,              NULL,                                   NULL, 0 },
    { "csnany",                S(CSN_REDUCTION_SCALAR),       0, "i",                    ":CsnArr;",                      (SUBR) csnarray_any_all,                     NULL,                                   NULL,                                   NULL, 0 },
    { "csnany.k",              S(CSN_REDUCTION_SCALAR),       0, "k",                    ":CsnArr;P",                     (SUBR) csnarray_any_all_k_init,              (SUBR) csnarray_any_all_k,              NULL,                                   NULL, 0 },
    { "csnstd",                S(CSN_REDUCTION_SCALAR),       0, "i",                    ":CsnArr;",                      (SUBR) csnarray_std_all,                     NULL,                                   NULL,                                   NULL, 0 },
    { "csnstd.k",              S(CSN_REDUCTION_SCALAR),       0, "k",                    ":CsnArr;P",                     (SUBR) csnarray_std_all_k_init,              (SUBR) csnarray_std_all_k,              NULL,                                   NULL, 0 },
    { "csnvar",                S(CSN_REDUCTION_SCALAR),       0, "i",                    ":CsnArr;",                      (SUBR) csnarray_var_all,                     NULL,                                   NULL,                                   NULL, 0 },
    { "csnvar.k",              S(CSN_REDUCTION_SCALAR),       0, "k",                    ":CsnArr;P",                     (SUBR) csnarray_var_all_k_init,              (SUBR) csnarray_var_all_k,              NULL,                                   NULL, 0 },
    { "csnsum.c",              S(CSN_REDUCTION_COMPLEX_S),    0, ":Complex;",            ":CsnArr;",                      (SUBR) csnarray_sumcomp_all,                 NULL,                                   NULL,                                   NULL, 0 },
    { "csnprod.c",             S(CSN_REDUCTION_COMPLEX_S),    0, ":Complex;",            ":CsnArr;",                      (SUBR) csnarray_prodcomp_all,                NULL,                                   NULL,                                   NULL, 0 },
    { "csnsub.c",              S(CSN_REDUCTION_COMPLEX_S),    0, ":Complex;",            ":CsnArr;",                      (SUBR) csnarray_subcomp_all,                 NULL,                                   NULL,                                   NULL, 0 },
    { "csnmean.c",             S(CSN_REDUCTION_COMPLEX_S),    0, ":Complex;",            ":CsnArr;",                      (SUBR) csnarray_meancomp_all,                NULL,                                   NULL,                                   NULL, 0 },
    { "csnsum.c.k",            S(CSN_REDUCTION_COMPLEX_S),    0, ":Complex;",            ":CsnArr;k",                     (SUBR) csnarray_sumcomp_all_k_init,          (SUBR) csnarray_sumcomp_all_k,          NULL,                                   NULL, 0 },
    { "csnprod.c.k",           S(CSN_REDUCTION_COMPLEX_S),    0, ":Complex;",            ":CsnArr;k",                     (SUBR) csnarray_prodcomp_all_k_init,         (SUBR) csnarray_prodcomp_all_k,         NULL,                                   NULL, 0 },
    { "csnsub.c.k",            S(CSN_REDUCTION_COMPLEX_S),    0, ":Complex;",            ":CsnArr;k",                     (SUBR) csnarray_subcomp_all_k_init,          (SUBR) csnarray_subcomp_all_k,          NULL,                                   NULL, 0 },
    { "csnmean.c.k",           S(CSN_REDUCTION_COMPLEX_S),    0, ":Complex;",            ":CsnArr;k",                     (SUBR) csnarray_meancomp_all_k_init,         (SUBR) csnarray_meancomp_all_k,         NULL,                                   NULL, 0 },
    { "csnsum.ax",             S(CSN_REDUCTION),              0, ":CsnArr;",             ":CsnArr;i",                     (SUBR) csnarray_sum,                         NULL,                                   (SUBR) csnarray_reduction_deinit,       NULL, 0 },
    { "csnsum.ax.k",           S(CSN_REDUCTION),              0, ":CsnArr;",             ":CsnArr;kP",                    (SUBR) csnarray_sum_k_init,                  (SUBR) csnarray_sum_k,                  (SUBR) csnarray_reduction_deinit,       NULL, 0 },
    { "csnprod.ax",            S(CSN_REDUCTION),              0, ":CsnArr;",             ":CsnArr;i",                     (SUBR) csnarray_prod,                        NULL,                                   (SUBR) csnarray_reduction_deinit,       NULL, 0 },
    { "csnprod.ax.k",          S(CSN_REDUCTION),              0, ":CsnArr;",             ":CsnArr;kP",                    (SUBR) csnarray_prod_k_init,                 (SUBR) csnarray_prod_k,                 (SUBR) csnarray_reduction_deinit,       NULL, 0 },
    { "csnsub.ax",             S(CSN_REDUCTION),              0, ":CsnArr;",             ":CsnArr;i",                     (SUBR) csnarray_sub,                         NULL,                                   (SUBR) csnarray_reduction_deinit,       NULL, 0 },
    { "csnsub.ax.k",           S(CSN_REDUCTION),              0, ":CsnArr;",             ":CsnArr;kP",                    (SUBR) csnarray_sub_k_init,                  (SUBR) csnarray_sub_k,                  (SUBR) csnarray_reduction_deinit,       NULL, 0 },
    { "csnmean.ax",            S(CSN_REDUCTION),              0, ":CsnArr;",             ":CsnArr;i",                     (SUBR) csnarray_mean,                        NULL,                                   (SUBR) csnarray_reduction_deinit,       NULL, 0 },
    { "csnmean.ax.k",          S(CSN_REDUCTION),              0, ":CsnArr;",             ":CsnArr;kP",                    (SUBR) csnarray_mean_k_init,                 (SUBR) csnarray_mean_k,                 (SUBR) csnarray_reduction_deinit,       NULL, 0 },
    { "csnany.ax",             S(CSN_REDUCTION),              0, ":CsnArr;",             ":CsnArr;i",                     (SUBR) csnarray_any,                         NULL,                                   (SUBR) csnarray_reduction_deinit,       NULL, 0 },
    { "csnany.ax.k",           S(CSN_REDUCTION),              0, ":CsnArr;",             ":CsnArr;kP",                    (SUBR) csnarray_any_k_init,                  (SUBR) csnarray_any_k,                  (SUBR) csnarray_reduction_deinit,       NULL, 0 },
    { "csnall.ax",             S(CSN_REDUCTION),              0, ":CsnArr;",             ":CsnArr;i",                     (SUBR) csnarray_all,                         NULL,                                   (SUBR) csnarray_reduction_deinit,       NULL, 0 },
    { "csnall.ax.k",           S(CSN_REDUCTION),              0, ":CsnArr;",             ":CsnArr;kP",                    (SUBR) csnarray_all_k_init,                  (SUBR) csnarray_all_k,                  (SUBR) csnarray_reduction_deinit,       NULL, 0 },
    { "csnstd.ax",             S(CSN_REDUCTION),              0, ":CsnArr;",             ":CsnArr;i",                     (SUBR) csnarray_std,                         NULL,                                   (SUBR) csnarray_reduction_deinit,       NULL, 0 },
    { "csnstd.ax.k",           S(CSN_REDUCTION),              0, ":CsnArr;",             ":CsnArr;kP",                    (SUBR) csnarray_std_k_init,                  (SUBR) csnarray_std_k,                  (SUBR) csnarray_reduction_deinit,       NULL, 0 },
    { "csnvar.ax",             S(CSN_REDUCTION),              0, ":CsnArr;",             ":CsnArr;i",                     (SUBR) csnarray_var,                         NULL,                                   (SUBR) csnarray_reduction_deinit,       NULL, 0 },
    { "csnvar.ax.k",           S(CSN_REDUCTION),              0, ":CsnArr;",             ":CsnArr;kP",                    (SUBR) csnarray_var_k_init,                  (SUBR) csnarray_var_k,                  (SUBR) csnarray_reduction_deinit,       NULL, 0 },
    { "csnadd",                S(CSN_BINOP_HH),               0, ":CsnArr;",             ":CsnArr;:CsnArr;",              (SUBR) csnarray_add_hh,                      NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnadd.k",              S(CSN_BINOP_HH),               0, ":CsnArr;",             ":CsnArr;:CsnArr;k",             (SUBR) csnarray_add_hh_k_init,               (SUBR) csnarray_add_hh_k,               (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnadd.hs",             S(CSN_BINOP_HS),               0, ":CsnArr;",             ":CsnArr;i",                     (SUBR) csnarray_add_hs,                      NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnadd.hs.c",           S(CSN_BINOPCOMPLEX_HS),        0, ":CsnArr;",             ":CsnArr;:Complex;",             (SUBR) csnarray_addcomp_hs,                  NULL,                                   (SUBR) csnarray_opbincomp_deinit,       NULL, 0 },
    { "csnadd.hs.k",           S(CSN_BINOP_HS),               0, ":CsnArr;",             ":CsnArr;kP",                    (SUBR) csnarray_add_hs_k_init,               (SUBR) csnarray_add_hs_k,               (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnadd.hs.c.k",         S(CSN_BINOPCOMPLEX_HS),        0, ":CsnArr;",             ":CsnArr;:Complex;k",            (SUBR) csnarray_addcomp_hs_k_init,           (SUBR) csnarray_addcomp_hs_k,           (SUBR) csnarray_opbincomp_deinit,       NULL, 0 },
    { "csnsubtract.hh",        S(CSN_BINOP_HH),               0, ":CsnArr;",             ":CsnArr;:CsnArr;",              (SUBR) csnarray_subtract_hh,                 NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnsubtract.hh.k",      S(CSN_BINOP_HH),               0, ":CsnArr;",             ":CsnArr;:CsnArr;k",             (SUBR) csnarray_subtract_hh_k_init,          (SUBR) csnarray_subtract_hh_k,          (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnsubtract.hs",        S(CSN_BINOP_HS),               0, ":CsnArr;",             ":CsnArr;i",                     (SUBR) csnarray_subtract_hs,                 NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnsubtract.hs.c",      S(CSN_BINOPCOMPLEX_HS),        0, ":CsnArr;",             ":CsnArr;:Complex;",             (SUBR) csnarray_subtractcomp_hs,             NULL,                                   (SUBR) csnarray_opbincomp_deinit,       NULL, 0 },
    { "csnsubtract.sh",        S(CSN_BINOP_SH),               0, ":CsnArr;",             "i:CsnArr;",                     (SUBR) csnarray_subtract_sh,                 NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnsubtract.sh.c",      S(CSN_BINOPCOMPLEX_SH),        0, ":CsnArr;",             ":Complex;:CsnArr;",             (SUBR) csnarray_subtractcomp_sh,             NULL,                                   (SUBR) csnarray_opbincomp_deinit,       NULL, 0 },
    { "csnsubtract.hs.k",      S(CSN_BINOP_HS),               0, ":CsnArr;",             ":CsnArr;kP",                    (SUBR) csnarray_subtract_hs_k_init,          (SUBR) csnarray_subtract_hs_k,          (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnsubtract.hs.c.k",    S(CSN_BINOPCOMPLEX_HS),        0, ":CsnArr;",             ":CsnArr;:Complex;k",            (SUBR) csnarray_subtractcomp_hs_k_init,      (SUBR) csnarray_subtractcomp_hs_k,      (SUBR) csnarray_opbincomp_deinit,       NULL, 0 },
    { "csnsubtract.sh.k",      S(CSN_BINOP_SH),               0, ":CsnArr;",             "k:CsnArr;P",                    (SUBR) csnarray_subtract_sh_k_init,          (SUBR) csnarray_subtract_sh_k,          (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnsubtract.sh.c.k",    S(CSN_BINOPCOMPLEX_SH),        0, ":CsnArr;",             ":Complex;:CsnArr;k",            (SUBR) csnarray_subtractcomp_sh_k_init,      (SUBR) csnarray_subtractcomp_sh_k,      (SUBR) csnarray_opbincomp_deinit,       NULL, 0 },
    { "csnmul.hh",             S(CSN_BINOP_HH),               0, ":CsnArr;",             ":CsnArr;:CsnArr;",              (SUBR) csnarray_mul_hh,                      NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnmul.hh.k",           S(CSN_BINOP_HH),               0, ":CsnArr;",             ":CsnArr;:CsnArr;k",             (SUBR) csnarray_mul_hh_k_init,               (SUBR) csnarray_mul_hh_k,               (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnmul.hs",             S(CSN_BINOP_HS),               0, ":CsnArr;",             ":CsnArr;i",                     (SUBR) csnarray_mul_hs,                      NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnmul.hs.c",           S(CSN_BINOPCOMPLEX_HS),        0, ":CsnArr;",             ":CsnArr;:Complex;",             (SUBR) csnarray_mulcomp_hs,                  NULL,                                   (SUBR) csnarray_opbincomp_deinit,       NULL, 0 },
    { "csnmul.hs.k",           S(CSN_BINOP_HS),               0, ":CsnArr;",             ":CsnArr;kP",                    (SUBR) csnarray_mul_hs_k_init,               (SUBR) csnarray_mul_hs_k,               (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnmul.hs.c.k",         S(CSN_BINOPCOMPLEX_HS),        0, ":CsnArr;",             ":CsnArr;:Complex;k",            (SUBR) csnarray_mulcomp_hs_k_init,           (SUBR) csnarray_mulcomp_hs_k,           (SUBR) csnarray_opbincomp_deinit,       NULL, 0 },
    { "csndiv.hh",             S(CSN_BINOP_HH),               0, ":CsnArr;",             ":CsnArr;:CsnArr;",              (SUBR) csnarray_div_hh,                      NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csndiv.hh.k",           S(CSN_BINOP_HH),               0, ":CsnArr;",             ":CsnArr;:CsnArr;k",             (SUBR) csnarray_div_hh_k_init,               (SUBR) csnarray_div_hh_k,               (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csndiv.hs",             S(CSN_BINOP_HS),               0, ":CsnArr;",             ":CsnArr;i",                     (SUBR) csnarray_div_hs,                      NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csndiv.sh",             S(CSN_BINOP_SH),               0, ":CsnArr;",             "i:CsnArr;",                     (SUBR) csnarray_div_sh,                      NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csndiv.hs.c",           S(CSN_BINOPCOMPLEX_HS),        0, ":CsnArr;",             ":CsnArr;:Complex;",             (SUBR) csnarray_divcomp_hs,                  NULL,                                   (SUBR) csnarray_opbincomp_deinit,       NULL, 0 },
    { "csndiv.sh.c",           S(CSN_BINOPCOMPLEX_SH),        0, ":CsnArr;",             ":Complex;:CsnArr;",             (SUBR) csnarray_divcomp_sh,                  NULL,                                   (SUBR) csnarray_opbincomp_deinit,       NULL, 0 },
    { "csndiv.hs.k",           S(CSN_BINOP_HS),               0, ":CsnArr;",             ":CsnArr;kP",                    (SUBR) csnarray_div_hs_k_init,               (SUBR) csnarray_div_hs_k,               (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csndiv.sh.k",           S(CSN_BINOP_SH),               0, ":CsnArr;",             "k:CsnArr;P",                    (SUBR) csnarray_div_sh_k_init,               (SUBR) csnarray_div_sh_k,               (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csndiv.hs.c.k",         S(CSN_BINOPCOMPLEX_HS),        0, ":CsnArr;",             ":CsnArr;:Complex;k",            (SUBR) csnarray_divcomp_hs_k_init,           (SUBR) csnarray_divcomp_hs_k,           (SUBR) csnarray_opbincomp_deinit,       NULL, 0 },
    { "csndiv.sh.c.k",         S(CSN_BINOPCOMPLEX_SH),        0, ":CsnArr;",             ":Complex;:CsnArr;k",            (SUBR) csnarray_divcomp_sh_k_init,           (SUBR) csnarray_divcomp_sh_k,           (SUBR) csnarray_opbincomp_deinit,       NULL, 0 },
    { "csnpow.hh",             S(CSN_BINOP_HH),               0, ":CsnArr;",             ":CsnArr;:CsnArr;",              (SUBR) csnarray_pow_hh,                      NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnpow.hh.k",           S(CSN_BINOP_HH),               0, ":CsnArr;",             ":CsnArr;:CsnArr;k",             (SUBR) csnarray_pow_hh_k_init,               (SUBR) csnarray_pow_hh_k,               (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnpow.hs",             S(CSN_BINOP_HS),               0, ":CsnArr;",             ":CsnArr;i",                     (SUBR) csnarray_pow_hs,                      NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnpow.sh",             S(CSN_BINOP_SH),               0, ":CsnArr;",             "i:CsnArr;",                     (SUBR) csnarray_pow_sh,                      NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnpow.hs.c",           S(CSN_BINOPCOMPLEX_HS),        0, ":CsnArr;",             ":CsnArr;:Complex;",             (SUBR) csnarray_powcomp_hs,                  NULL,                                   (SUBR) csnarray_opbincomp_deinit,       NULL, 0 },
    { "csnpow.sh.c",           S(CSN_BINOPCOMPLEX_SH),        0, ":CsnArr;",             ":Complex;:CsnArr;",             (SUBR) csnarray_powcomp_sh,                  NULL,                                   (SUBR) csnarray_opbincomp_deinit,       NULL, 0 },
    { "csnpow.hs.k",           S(CSN_BINOP_HS),               0, ":CsnArr;",             ":CsnArr;kP",                    (SUBR) csnarray_pow_hs_k_init,               (SUBR) csnarray_pow_hs_k,               (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnpow.sh.k",           S(CSN_BINOP_SH),               0, ":CsnArr;",             "k:CsnArr;P",                    (SUBR) csnarray_pow_sh_k_init,               (SUBR) csnarray_pow_sh_k,               (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnpow.hs.c.k",         S(CSN_BINOPCOMPLEX_HS),        0, ":CsnArr;",             ":CsnArr;:Complex;k",            (SUBR) csnarray_powcomp_hs_k_init,           (SUBR) csnarray_powcomp_hs_k,           (SUBR) csnarray_opbincomp_deinit,       NULL, 0 },
    { "csnpow.sh.c.k",         S(CSN_BINOPCOMPLEX_SH),        0, ":CsnArr;",             ":Complex;:CsnArr;k",            (SUBR) csnarray_powcomp_sh_k_init,           (SUBR) csnarray_powcomp_sh_k,           (SUBR) csnarray_opbincomp_deinit,       NULL, 0 },
    { "csnlog.hh",             S(CSN_BINOP_HH),               0, ":CsnArr;",             ":CsnArr;:CsnArr;",              (SUBR) csnarray_log_hh,                      NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnlog.hh.k",           S(CSN_BINOP_HH),               0, ":CsnArr;",             ":CsnArr;:CsnArr;k",             (SUBR) csnarray_log_hh_k_init,               (SUBR) csnarray_log_hh_k,               (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnlog.hs",             S(CSN_BINOP_HS),               0, ":CsnArr;",             ":CsnArr;i",                     (SUBR) csnarray_log_hs,                      NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnlog.sh",             S(CSN_BINOP_SH),               0, ":CsnArr;",             "i:CsnArr;",                     (SUBR) csnarray_log_sh,                      NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnlog.hs.c",           S(CSN_BINOPCOMPLEX_HS),        0, ":CsnArr;",             ":CsnArr;:Complex;",             (SUBR) csnarray_logcomp_hs,                  NULL,                                   (SUBR) csnarray_opbincomp_deinit,       NULL, 0 },
    { "csnlog.sh.c",           S(CSN_BINOPCOMPLEX_SH),        0, ":CsnArr;",             ":Complex;:CsnArr;",             (SUBR) csnarray_logcomp_sh,                  NULL,                                   (SUBR) csnarray_opbincomp_deinit,       NULL, 0 },
    { "csnlog.hs.k",           S(CSN_BINOP_HS),               0, ":CsnArr;",             ":CsnArr;kP",                    (SUBR) csnarray_log_hs_k_init,               (SUBR) csnarray_log_hs_k,               (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnlog.sh.k",           S(CSN_BINOP_SH),               0, ":CsnArr;",             "k:CsnArr;P",                    (SUBR) csnarray_log_sh_k_init,               (SUBR) csnarray_log_sh_k,               (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnlog.hs.c.k",         S(CSN_BINOPCOMPLEX_HS),        0, ":CsnArr;",             ":CsnArr;:Complex;k",            (SUBR) csnarray_logcomp_hs_k_init,           (SUBR) csnarray_logcomp_hs_k,           (SUBR) csnarray_opbincomp_deinit,       NULL, 0 },
    { "csnlog.sh.c.k",         S(CSN_BINOPCOMPLEX_SH),        0, ":CsnArr;",             ":Complex;:CsnArr;k",            (SUBR) csnarray_logcomp_sh_k_init,           (SUBR) csnarray_logcomp_sh_k,           (SUBR) csnarray_opbincomp_deinit,       NULL, 0 },
    { "csnabs",                S(CSN_UNARYOP),                0, ":CsnArr;",             ":CsnArr;",                      (SUBR) csnarray_abs,                         NULL,                                   (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csnexp",                S(CSN_UNARYOP),                0, ":CsnArr;",             ":CsnArr;",                      (SUBR) csnarray_exp,                         NULL,                                   (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csnsqrt",               S(CSN_UNARYOP),                0, ":CsnArr;",             ":CsnArr;",                      (SUBR) csnarray_sqrt,                        NULL,                                   (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csncbrt",               S(CSN_UNARYOP),                0, ":CsnArr;",             ":CsnArr;",                      (SUBR) csnarray_cbrt,                        NULL,                                   (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csnsin",                S(CSN_UNARYOP),                0, ":CsnArr;",             ":CsnArr;",                      (SUBR) csnarray_sin,                         NULL,                                   (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csncos",                S(CSN_UNARYOP),                0, ":CsnArr;",             ":CsnArr;",                      (SUBR) csnarray_cos,                         NULL,                                   (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csntan",                S(CSN_UNARYOP),                0, ":CsnArr;",             ":CsnArr;",                      (SUBR) csnarray_tan,                         NULL,                                   (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csnasin",               S(CSN_UNARYOP),                0, ":CsnArr;",             ":CsnArr;",                      (SUBR) csnarray_asin,                        NULL,                                   (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csnacos",               S(CSN_UNARYOP),                0, ":CsnArr;",             ":CsnArr;",                      (SUBR) csnarray_acos,                        NULL,                                   (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csnatan",               S(CSN_UNARYOP),                0, ":CsnArr;",             ":CsnArr;",                      (SUBR) csnarray_atan,                        NULL,                                   (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csnsinh",               S(CSN_UNARYOP),                0, ":CsnArr;",             ":CsnArr;",                      (SUBR) csnarray_sinh,                        NULL,                                   (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csncosh",               S(CSN_UNARYOP),                0, ":CsnArr;",             ":CsnArr;",                      (SUBR) csnarray_cosh,                        NULL,                                   (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csntanh",               S(CSN_UNARYOP),                0, ":CsnArr;",             ":CsnArr;",                      (SUBR) csnarray_tanh,                        NULL,                                   (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csnasinh",              S(CSN_UNARYOP),                0, ":CsnArr;",             ":CsnArr;",                      (SUBR) csnarray_asinh,                       NULL,                                   (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csnacosh",              S(CSN_UNARYOP),                0, ":CsnArr;",             ":CsnArr;",                      (SUBR) csnarray_acosh,                       NULL,                                   (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csnatanh",              S(CSN_UNARYOP),                0, ":CsnArr;",             ":CsnArr;",                      (SUBR) csnarray_atanh,                       NULL,                                   (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csnsign",               S(CSN_UNARYOP),                0, ":CsnArr;",             ":CsnArr;",                      (SUBR) csnarray_sign,                        NULL,                                   (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csnabs.k",              S(CSN_UNARYOP),                0, ":CsnArr;",             ":CsnArr;k",                     (SUBR) csnarray_abs,                         (SUBR) csnarray_abs_k,                  (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csnexp.k",              S(CSN_UNARYOP),                0, ":CsnArr;",             ":CsnArr;k",                     (SUBR) csnarray_exp,                         (SUBR) csnarray_exp_k,                  (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csnsqrt.k",             S(CSN_UNARYOP),                0, ":CsnArr;",             ":CsnArr;k",                     (SUBR) csnarray_sqrt,                        (SUBR) csnarray_sqrt_k,                 (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csncbrt.k",             S(CSN_UNARYOP),                0, ":CsnArr;",             ":CsnArr;k",                     (SUBR) csnarray_cbrt,                        (SUBR) csnarray_cbrt_k,                 (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csnsin.k",              S(CSN_UNARYOP),                0, ":CsnArr;",             ":CsnArr;k",                     (SUBR) csnarray_sin,                         (SUBR) csnarray_sin_k,                  (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csncos.k",              S(CSN_UNARYOP),                0, ":CsnArr;",             ":CsnArr;k",                     (SUBR) csnarray_cos,                         (SUBR) csnarray_cos_k,                  (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csntan.k",              S(CSN_UNARYOP),                0, ":CsnArr;",             ":CsnArr;k",                     (SUBR) csnarray_tan,                         (SUBR) csnarray_tan_k,                  (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csnasin.k",             S(CSN_UNARYOP),                0, ":CsnArr;",             ":CsnArr;k",                     (SUBR) csnarray_asin,                        (SUBR) csnarray_asin_k,                 (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csnacos.k",             S(CSN_UNARYOP),                0, ":CsnArr;",             ":CsnArr;k",                     (SUBR) csnarray_acos,                        (SUBR) csnarray_acos_k,                 (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csnatan.k",             S(CSN_UNARYOP),                0, ":CsnArr;",             ":CsnArr;k",                     (SUBR) csnarray_atan,                        (SUBR) csnarray_atan_k,                 (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csnsinh.k",             S(CSN_UNARYOP),                0, ":CsnArr;",             ":CsnArr;k",                     (SUBR) csnarray_sinh,                        (SUBR) csnarray_sinh_k,                 (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csncosh.k",             S(CSN_UNARYOP),                0, ":CsnArr;",             ":CsnArr;k",                     (SUBR) csnarray_cosh,                        (SUBR) csnarray_cosh_k,                 (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csntanh.k",             S(CSN_UNARYOP),                0, ":CsnArr;",             ":CsnArr;k",                     (SUBR) csnarray_tanh,                        (SUBR) csnarray_tanh_k,                 (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csnasinh.k",            S(CSN_UNARYOP),                0, ":CsnArr;",             ":CsnArr;k",                     (SUBR) csnarray_asinh,                       (SUBR) csnarray_asinh_k,                (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csnacosh.k",            S(CSN_UNARYOP),                0, ":CsnArr;",             ":CsnArr;k",                     (SUBR) csnarray_acosh,                       (SUBR) csnarray_acosh_k,                (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csnatanh.k",            S(CSN_UNARYOP),                0, ":CsnArr;",             ":CsnArr;k",                     (SUBR) csnarray_atanh,                       (SUBR) csnarray_atanh_k,                (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csnsign.k",             S(CSN_UNARYOP),                0, ":CsnArr;",             ":CsnArr;k",                     (SUBR) csnarray_sign,                        (SUBR) csnarray_sign_k,                 (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csndot",                S(CSN_BINOP_HH),               0, ":CsnArr;",             ":CsnArr;:CsnArr;",              (SUBR) csnarray_dot,                         NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csndot.k",              S(CSN_BINOP_HH),               0, ":CsnArr;",             ":CsnArr;:CsnArr;k",             (SUBR) csnarray_dot_k_init,                  (SUBR) csnarray_dot_k,                  (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csndot.s",              S(CSN_BINOP_HH_SCALAR),        0, "i",                    ":CsnArr;:CsnArr;",              (SUBR) csnarray_dot_scalar,                  NULL,                                   NULL,                                   NULL, 0 },
    { "csndot.s.k",            S(CSN_BINOP_HH_SCALAR),        0, "k",                    ":CsnArr;:CsnArr;k",             (SUBR) csnarray_dot_scalar,                  (SUBR) csnarray_dot_scalar_k,           NULL,                                   NULL, 0 },
    { "csndot.s.c",            S(CSN_BINOPCOMPLEX_HH_SCALAR), 0, ":Complex;",            ":CsnArr;:CsnArr;",              (SUBR) csnarray_dotcomp_scalar,              NULL,                                   NULL,                                   NULL, 0 },
    { "csndot.s.c.k",          S(CSN_BINOPCOMPLEX_HH_SCALAR), 0, ":Complex;",            ":CsnArr;:CsnArr;k",             (SUBR) csnarray_dotcomp_scalar,              (SUBR) csnarray_dotcomp_scalar_k,       NULL,                                   NULL, 0 },
    { "csninner",              S(CSN_BINOP_HH),               0, ":CsnArr;",             ":CsnArr;:CsnArr;",              (SUBR) csnarray_inner,                       NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csninner.k",            S(CSN_BINOP_HH),               0, ":CsnArr;",             ":CsnArr;:CsnArr;k",             (SUBR) csnarray_inner_k_init,                (SUBR) csnarray_inner_k,                (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csninner.s",            S(CSN_BINOP_HH_SCALAR),        0, "i",                    ":CsnArr;:CsnArr;",              (SUBR) csnarray_inner_scalar,                NULL,                                   NULL,                                   NULL, 0 },
    { "csninner.s.k",          S(CSN_BINOP_HH_SCALAR),        0, "k",                    ":CsnArr;:CsnArr;k",             (SUBR) csnarray_inner_scalar,                (SUBR) csnarray_inner_scalar_k,         NULL,                                   NULL, 0 },
    { "csninner.s.c",          S(CSN_BINOPCOMPLEX_HH_SCALAR), 0, ":Complex;",            ":CsnArr;:CsnArr;",              (SUBR) csnarray_innercomp_scalar,            NULL,                                   NULL,                                   NULL, 0 },
    { "csninner.s.c.k",        S(CSN_BINOPCOMPLEX_HH_SCALAR), 0, ":Complex;",            ":CsnArr;:CsnArr;k",             (SUBR) csnarray_innercomp_scalar,            (SUBR) csnarray_innercomp_scalar_k,     NULL,                                   NULL, 0 },
    { "csnouter",              S(CSN_BINOP_HH),               0, ":CsnArr;",             ":CsnArr;:CsnArr;",              (SUBR) csnarray_outer,                       NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnouter.k",            S(CSN_BINOP_HH),               0, ":CsnArr;",             ":CsnArr;:CsnArr;k",             (SUBR) csnarray_outer_k_init,                (SUBR) csnarray_outer_k,                (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnnorm",               S(CSN_NORM_REDUCTION),         0, ":CsnArr;",             ":CsnArr;ip",                    (SUBR) csnarray_norm,                        NULL,                                   (SUBR) csnarray_norm_deinit,            NULL, 0 },
    { "csnnorm.k",             S(CSN_NORM_REDUCTION),         0, ":CsnArr;",             ":CsnArr;kkP",                   (SUBR) csnarray_norm_k_init,                 (SUBR) csnarray_norm_k,                 (SUBR) csnarray_norm_deinit,            NULL, 0 },
    { "csnnorm.s",             S(CSN_NORM_REDUCTION_SCALAR),  0, "i",                    ":CsnArr;p",                     (SUBR) csnarray_norm_scalar,                 NULL,                                   NULL,                                   NULL, 0 },
    { "csnnorm.s.k",           S(CSN_NORM_REDUCTION_SCALAR),  0, "k",                    ":CsnArr;kP",                    (SUBR) csnarray_norm_scalar_k_init,                 (SUBR) csnarray_norm_scalar_k,          NULL,                                   NULL, 0 },
    { "csnnormalize",          S(CSN_UNARYOP_AX),             0, ":CsnArr;",             ":CsnArr;jp",                    (SUBR) csnarray_normalize,                   NULL,                                   (SUBR) csnarray_opunary_ax_deinit,      NULL, 0 },
    { "csnnormalize.in",       S(CSN_UNARYOP_AX_IN),          0, "",                     ":CsnArr;jp",                    (SUBR) csnarray_normalize_in,                NULL,                                   NULL,                                   NULL, 0 },
    { "csnnormalize.k",        S(CSN_UNARYOP_AX),             0, ":CsnArr;",             ":CsnArr;kkP",                   (SUBR) csnarray_normalize_k_init,            (SUBR) csnarray_normalize_k,            (SUBR) csnarray_opunary_ax_deinit,      NULL, 0 },
    { "csnnormalize.in.k",     S(CSN_UNARYOP_AX_IN),          0, "",                     ":CsnArr;kkP",                   (SUBR) csnarray_normalize_in_k_init,         (SUBR) csnarray_normalize_in_k,         (SUBR) opunary_ax_in_k_deinit,          NULL, 0 },
    { "csnpairdist",           S(CSN_BINOP_HH),               0, ":CsnArr;",             ":CsnArr;:CsnArr;",              (SUBR) csnarray_pair_distance,               NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnpairdist.k",         S(CSN_BINOP_HH),               0, ":CsnArr;",             ":CsnArr;:CsnArr;k",             (SUBR) csnarray_pair_distance_k_init,        (SUBR) csnarray_pair_distance_k,        (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csndist",               S(CSN_BINOP_HH_SCALAR),        0, "i",                    ":CsnArr;:CsnArr;p",             (SUBR) csnarray_distance,                    NULL,                                   NULL,                                   NULL, 0 },
    { "csndist.k",             S(CSN_BINOP_HH_SCALAR),        0, "k",                    ":CsnArr;:CsnArr;kk",            (SUBR) csnarray_distance_k_init,                    (SUBR) csnarray_distance_k,             NULL,                                   NULL, 0 },
    { "csnangledist",          S(CSN_BINOP_HH_SCALAR),        0, "i",                    ":CsnArr;:CsnArr;",              (SUBR) csnarray_angle_distance,              NULL,                                   NULL,                                   NULL, 0 },
    { "csnangledist.k",        S(CSN_BINOP_HH_SCALAR),        0, "k",                    ":CsnArr;:CsnArr;k",             (SUBR) csnarray_angle_distance,              (SUBR) csnarray_angle_distance_k,       NULL,                                   NULL, 0 },
    { "csnreflect",            S(CSN_BINOP_HH),               0, ":CsnArr;",             ":CsnArr;:CsnArr;",              (SUBR) csnarray_reflect,                     NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnreflect.k",          S(CSN_BINOP_HH),               0, ":CsnArr;",             ":CsnArr;:CsnArr;k",             (SUBR) csnarray_reflect_k_init,              (SUBR) csnarray_reflect_k,              (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csndiff",               S(CSN_UNARYOP_AX),             0, ":CsnArr;",             ":CsnArr;j",                     (SUBR) csnarray_diff,                        NULL,                                   (SUBR) csnarray_opunary_ax_deinit,      NULL, 0 },
    { "csndiff.k",             S(CSN_UNARYOP_AX),             0, ":CsnArr;",             ":CsnArr;kP",                    (SUBR) csnarray_diff_k_init,                 (SUBR) csnarray_diff_k,                 (SUBR) csnarray_opunary_ax_deinit,      NULL, 0 },
    { "csncumsum",             S(CSN_UNARYOP_AX),             0, ":CsnArr;",             ":CsnArr;j",                     (SUBR) csnarray_cumsum,                      NULL,                                   (SUBR) csnarray_opunary_ax_deinit,      NULL, 0 },
    { "csncumsum.k",           S(CSN_UNARYOP_AX),             0, ":CsnArr;",             ":CsnArr;kP",                    (SUBR) csnarray_cumsum_k_init,               (SUBR) csnarray_cumsum_k,               (SUBR) csnarray_opunary_ax_deinit,      NULL, 0 },
    { "csncumprod",            S(CSN_UNARYOP_AX),             0, ":CsnArr;",             ":CsnArr;j",                     (SUBR) csnarray_cumprod,                     NULL,                                   (SUBR) csnarray_opunary_ax_deinit,      NULL, 0 },
    { "csncumprod.k",          S(CSN_UNARYOP_AX),             0, ":CsnArr;",             ":CsnArr;kP",                    (SUBR) csnarray_cumprod_k_init,              (SUBR) csnarray_cumprod_k,              (SUBR) csnarray_opunary_ax_deinit,      NULL, 0 },
    { "csnmatmul",             S(CSN_BINOP_HH),               0, ":CsnArr;",             ":CsnArr;:CsnArr;",              (SUBR) csnarray_matmul,                      NULL,                                   (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnmatmul.k",           S(CSN_BINOP_HH),               0, ":CsnArr;",             ":CsnArr;:CsnArr;k",             (SUBR) csnarray_matmul,                      (SUBR) csnarray_matmul_k,               (SUBR) csnarray_opbin_deinit,           NULL, 0 },
    { "csnmatmul.s",           S(CSN_BINOP_HH_SCALAR),        0, "i",                    ":CsnArr;:CsnArr;",              (SUBR) csnarray_matmul_scalar,               NULL,                                   NULL,                                   NULL, 0 },
    { "csnmatmul.s.k",         S(CSN_BINOP_HH_SCALAR),        0, "k",                    ":CsnArr;:CsnArr;k",             (SUBR) csnarray_matmul_scalar,               (SUBR) csnarray_matmul_scalar_k,        NULL,                                   NULL, 0 },
    { "csntrace",              S(CSN_UNARYOP_SCALAR),         0, "i",                    ":CsnArr;",                      (SUBR) csnarray_trace,                       NULL,                                   NULL,                                   NULL, 0 },
    { "csntrace.c",            S(CSN_UNARYOPCOMPLEX_SCALAR),  0, ":Complex;",            ":CsnArr;",                      (SUBR) csnarray_tracecomp,                   NULL,                                   NULL,                                   NULL, 0 },
    { "csntrace.k",            S(CSN_UNARYOP_SCALAR),         0, "k",                    ":CsnArr;P",                     (SUBR) csnarray_trace,                       (SUBR) csnarray_trace_k,                NULL,                                   NULL, 0 },
    { "csntrace.c.k",          S(CSN_UNARYOPCOMPLEX_SCALAR),  0, ":Complex;",            ":CsnArr;k",                     (SUBR) csnarray_tracecomp,                   (SUBR) csnarray_tracecomp_k,            NULL,                                   NULL, 0 },
    { "csndiag",               S(CSN_UNARYOP),                0, ":CsnArr;",             ":CsnArr;",                      (SUBR) csnarray_diag,                        NULL,                                   (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csndiag.k",             S(CSN_UNARYOP),                0, ":CsnArr;",             ":CsnArr;k",                     (SUBR) csnarray_diag,                        (SUBR) csnarray_diag_k,                 (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csnmovmean",            S(CSN_MOVSTATS),               0, ":CsnArr;",             ":CsnArr;ij",                    (SUBR) csnarray_movmean,                     NULL,                                   (SUBR) csnarray_movstats_deinit,        NULL, 0 },
    { "csnmovmean.k",          S(CSN_MOVSTATS),               0, ":CsnArr;",             ":CsnArr;kkP",                   (SUBR) csnarray_movmean_k_init,              (SUBR) csnarray_movmean_k,              (SUBR) csnarray_movstats_deinit,        NULL, 0 },
    { "csnmovmean.in",         S(CSN_MOVSTATS_IN),            0, "",                     ":CsnArr;ij",                    (SUBR) csnarray_movmean_in,                  NULL,                                   NULL,                                   NULL, 0 },
    { "csnmovmean.in.k",       S(CSN_MOVSTATS_IN),            0, "",                     ":CsnArr;kkP",                   (SUBR) csnarray_movmean_in_k_init,           (SUBR) csnarray_movmean_in_k,           (SUBR) csnarray_movstats_in_k_deinit,   NULL, 0 },
    { "csnmovstd",             S(CSN_MOVSTATS),               0, ":CsnArr;",             ":CsnArr;ij",                    (SUBR) csnarray_movstd,                      NULL,                                   (SUBR) csnarray_movstats_deinit,        NULL, 0 },
    { "csnmovstd.k",           S(CSN_MOVSTATS),               0, ":CsnArr;",             ":CsnArr;kkP",                   (SUBR) csnarray_movstd_k_init,               (SUBR) csnarray_movstd_k,               (SUBR) csnarray_movstats_deinit,        NULL, 0 },
    { "csnmovstd.in",          S(CSN_MOVSTATS_IN),            0, "",                     ":CsnArr;ij",                    (SUBR) csnarray_movstd_in,                   NULL,                                   NULL,                                   NULL, 0 },
    { "csnmovstd.in.k",        S(CSN_MOVSTATS_IN),            0, "",                     ":CsnArr;kkP",                   (SUBR) csnarray_movstd_in_k_init,            (SUBR) csnarray_movstd_in_k,            (SUBR) csnarray_movstats_in_k_deinit,   NULL, 0 },
    { "csnmovvar",             S(CSN_MOVSTATS),               0, ":CsnArr;",             ":CsnArr;ij",                    (SUBR) csnarray_movvar,                      NULL,                                   (SUBR) csnarray_movstats_deinit,        NULL, 0 },
    { "csnmovvar.k",           S(CSN_MOVSTATS),               0, ":CsnArr;",             ":CsnArr;kkP",                   (SUBR) csnarray_movvar_k_init,               (SUBR) csnarray_movvar_k,               (SUBR) csnarray_movstats_deinit,        NULL, 0 },
    { "csnmovvar.in",          S(CSN_MOVSTATS_IN),            0, "",                     ":CsnArr;ij",                    (SUBR) csnarray_movvar_in,                   NULL,                                   NULL,                                   NULL, 0 },
    { "csnmovvar.in.k",        S(CSN_MOVSTATS_IN),            0, "",                     ":CsnArr;kkP",                   (SUBR) csnarray_movvar_in_k_init,            (SUBR) csnarray_movvar_in_k,            (SUBR) csnarray_movstats_in_k_deinit,   NULL, 0 },
    { "csnreal",               S(CSN_UNARYOP),                0, ":CsnArr;",             ":CsnArr;",                      (SUBR) csnarray_real,                        NULL,                                   (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csnreal.k",             S(CSN_UNARYOP),                0, ":CsnArr;",             ":CsnArr;k",                     (SUBR) csnarray_real_k_init,                 (SUBR) csnarray_real_k,                 (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csnimag",               S(CSN_UNARYOP),                0, ":CsnArr;",             ":CsnArr;",                      (SUBR) csnarray_imag,                        NULL,                                   (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csnimag.k",             S(CSN_UNARYOP),                0, ":CsnArr;",             ":CsnArr;k",                     (SUBR) csnarray_imag_k_init,                 (SUBR) csnarray_imag_k,                 (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csntoreal",             S(CSN_UNARYOP),                0, ":CsnArr;",             ":CsnArr;",                      (SUBR) csnarray_complex_to_real,             NULL,                                   (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csntoreal.k",           S(CSN_UNARYOP),                0, ":CsnArr;",             ":CsnArr;k",                     (SUBR) csnarray_complex_to_real_k_init,      (SUBR) csnarray_complex_to_real_k,      (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csntocomplex",          S(CSN_UNARYOP),                0, ":CsnArr;",             ":CsnArr;",                      (SUBR) csnarray_real_to_complex,             NULL,                                   (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csntocomplex.k",        S(CSN_UNARYOP),                0, ":CsnArr;",             ":CsnArr;k",                     (SUBR) csnarray_real_to_complex_k_init,      (SUBR) csnarray_real_to_complex_k,      (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csnconj",               S(CSN_UNARYOP),                0, ":CsnArr;",             ":CsnArr;",                      (SUBR) csnarray_conj,                        NULL,                                   (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csnconj.k",             S(CSN_UNARYOP),                0, ":CsnArr;",             ":CsnArr;k",                     (SUBR) csnarray_conj_k_init,                 (SUBR) csnarray_conj_k,                 (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csnangle",              S(CSN_ANGLE),                  0, ":CsnArr;",             ":CsnArr;",                      (SUBR) csnarray_angle,                       NULL,                                   (SUBR) csnarray_angle_deinit,           NULL, 0 },
    { "csnangle.k",            S(CSN_ANGLE),                  0, ":CsnArr;",             ":CsnArr;k",                     (SUBR) csnarray_angle_k_init,                (SUBR) csnarray_angle_k,                (SUBR) csnarray_angle_deinit,           NULL, 0 },
    { "csnwrap",               S(CSN_ANGLE),                  0, ":CsnArr;",             ":CsnArr;i",                     (SUBR) csnarray_wrap_angle,                  NULL,                                   (SUBR) csnarray_angle_deinit,           NULL, 0 },
    { "csnwrap.k",             S(CSN_ANGLE),                  0, ":CsnArr;",             ":CsnArr;kk",                    (SUBR) csnarray_wrap_angle_k_init,           (SUBR) csnarray_wrap_angle_k,           (SUBR) csnarray_angle_deinit,           NULL, 0 },
    { "csnwrap.in",            S(CSN_ANGLE),                  0, "",                     ":CsnArr;i",                     (SUBR) csnarray_wrap_angle_in,               NULL,                                   NULL,                                   NULL, 0 },
    { "csnwrap.in.k",          S(CSN_ANGLE),                  0, "",                     ":CsnArr;kk",                    (SUBR) csnarray_wrap_angle_in,               (SUBR) csnarray_wrap_angle_in_k,        NULL,                                   NULL, 0 },
    { "csnunwrap",             S(CSN_ANGLE),                  0, ":CsnArr;",             ":CsnArr;iij",                   (SUBR) csnarray_unwrap_angle,                NULL,                                   (SUBR) csnarray_angle_deinit,           NULL, 0 },
    { "csnunwrap.k",           S(CSN_ANGLE),                  0, ":CsnArr;",             ":CsnArr;kkkk",                  (SUBR) csnarray_unwrap_angle_k_init,         (SUBR) csnarray_unwrap_angle_k,         (SUBR) csnarray_angle_deinit,           NULL, 0 },
    { "csnunwrap.in",          S(CSN_ANGLE),                  0, "",                     ":CsnArr;iij",                   (SUBR) csnarray_unwrap_angle_in,             NULL,                                   NULL,                                   NULL, 0 },
    { "csnunwrap.in.k",        S(CSN_ANGLE),                  0, "",                     ":CsnArr;kkkk",                  (SUBR) csnarray_unwrap_angle_in,             (SUBR) csnarray_unwrap_angle_in_k,      NULL,                                   NULL, 0 },
    { "csntype",               S(CSN_UNARYOP_SCALAR),         0, "i",                    ":CsnArr;",                      (SUBR) csnarray_type,                        NULL,                                   NULL,                                   NULL, 0 },
    { "csntype.k",             S(CSN_UNARYOP_SCALAR),         0, "k",                    ":CsnArr;P",                     (SUBR) csnarray_type,                        (SUBR) csnarray_type_k,                 NULL,                                   NULL, 0 },
    { "csncopy",               S(CSN_UNARYOP),                0, ":CsnArr;",             ":CsnArr;",                      (SUBR) csnarray_copy,                        NULL,                                   (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csncopy.k",             S(CSN_UNARYOP),                0, ":CsnArr;",             ":CsnArr;k",                     (SUBR) csnarray_copy_k_init,                 (SUBR) csnarray_copy_k,                 (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csnreverse",            S(CSN_UNARYOP),                0, ":CsnArr;",             ":CsnArr;",                      (SUBR) csnarray_reverse,                     NULL,                                   (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csnreverse.k",          S(CSN_UNARYOP),                0, ":CsnArr;",             ":CsnArr;k",                     (SUBR) csnarray_reverse_k_init,              (SUBR) csnarray_reverse_k,              (SUBR) csnarray_opunary_deinit,         NULL, 0 },
    { "csnreverse.in",         S(CSN_UNARYOP_IN),             0, "",                     ":CsnArr;",                      (SUBR) csnarray_reverse_in,                  NULL,                                   NULL,                                   NULL, 0 },
    { "csnreverse.in.k",       S(CSN_UNARYOP_IN),             0, "",                     ":CsnArr;k",                     (SUBR) csnarray_unaryop_in_k_init,           (SUBR) csnarray_reverse_in_k,           NULL,                                   NULL, 0 },
    { "csntruncate",           S(CSN_TRUNCATE),               0, ":CsnArr;",             ":CsnArr;ij",                    (SUBR) csnarray_truncate,                    NULL,                                   (SUBR) csnarray_truncate_deinit,        NULL, 0 },
    { "csntruncate.k",         S(CSN_TRUNCATE),               0, ":CsnArr;",             ":CsnArr;kJP",                   (SUBR) csnarray_truncate_k_init,             (SUBR) csnarray_truncate_k,             (SUBR) csnarray_truncate_deinit,        NULL, 0 },
    { "csntruncate.in",        S(CSN_TRUNCATE_IN),            0, "",                     ":CsnArr;ij",                    (SUBR) csnarray_truncate_in,                 NULL,                                   NULL,                                   NULL, 0 },
    { "csntruncate.in.k",      S(CSN_TRUNCATE_IN),            0, "",                     ":CsnArr;kJP",                   (SUBR) csnarray_truncate_in_k_init,          (SUBR) csnarray_truncate_in_k,          NULL,                                   NULL, 0 },
    { "csnresize",             S(CSN_RESIZE),                 0, ":CsnArr;",             ":CsnArr;i[]",                   (SUBR) csnarray_resize,                      NULL,                                   (SUBR) csnarray_resize_deinit,          NULL, 0 },
    { "csnresize.k",           S(CSN_RESIZE),                 0, ":CsnArr;",             ":CsnArr;k[]J",                  (SUBR) csnarray_resize_k_init,               (SUBR) csnarray_resize_k,               (SUBR) csnarray_resize_deinit,          NULL, 0 },
    { "csnresize.in",          S(CSN_RESIZE_IN),              0, "",                     ":CsnArr;i[]",                   (SUBR) csnarray_resize_in,                   NULL,                                   NULL,                                   NULL, 0 },
    { "csnresize.in.k",        S(CSN_RESIZE_IN),              0, "",                     ":CsnArr;k[]J",                  (SUBR) csnarray_resize_in_k_init,            (SUBR) csnarray_resize_in_k,            NULL,                                   NULL, 0 },
    { "csnhead",               S(CSN_TRUNCATE),               0, ":CsnArr;",             ":CsnArr;i",                     (SUBR) csnarray_head,                        NULL,                                   (SUBR) csnarray_truncate_deinit,        NULL, 0 },
    { "csnhead.k",             S(CSN_TRUNCATE),               0, ":CsnArr;",             ":CsnArr;kP",                    (SUBR) csnarray_head_k_init,                 (SUBR) csnarray_head_k,                 (SUBR) csnarray_truncate_deinit,        NULL, 0 },
    { "csnprint",              S(CSN_SHOW),                   0, "",                     ":CsnArr;",                      (SUBR) csnarray_show,                        NULL,                                   NULL,                                   NULL, 0 },
    { "csnprint.k",            S(CSN_SHOW),                   0, "",                     ":CsnArr;k",                     (SUBR) csnarray_show_k_init,                 (SUBR) csnarray_show_k,                 (SUBR) csnarray_show_k_deinit,          NULL, 0 },
    { "csncompress",           S(CSN_WHERE_HS),               0, ":CsnArr;",             ":CsnArr;:CsnArr;j",             (SUBR) csnarray_compress,                    NULL,                                   (SUBR) csnarray_where_deinit,           NULL, 0 },
    { "csncompress.k",         S(CSN_WHERE_HS),               0, ":CsnArr;",             ":CsnArr;:CsnArr;JP",            (SUBR) csnarray_compress_k_init,             (SUBR) csnarray_compress_k,             (SUBR) csnarray_where_deinit,           NULL, 0 },
    { "csnselect",             S(CSN_ARGWHERE),               0, ":CsnArr;",             ":CsnArr;:CsnArr;",              (SUBR) csnarray_select,                      NULL,                                   (SUBR) csnarray_argwhere_deinit,        NULL, 0 },
    { "csnselect.k",           S(CSN_ARGWHERE),               0, ":CsnArr;",             ":CsnArr;:CsnArr;P",             (SUBR) csnarray_select_k_init,               (SUBR) csnarray_select_k,               (SUBR) csnarray_argwhere_deinit,        NULL, 0 },
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
