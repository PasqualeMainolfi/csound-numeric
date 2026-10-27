#include "csnframe.h"
#include "csnregistry.h"
#include "csnstrings.h"
#include "csnum.h"
#include "csnum_internal.h"
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static int32_t framing_assign_value(CSOUND *csound, OPDS *perf_h, CSN_ARRAY *source_arr, CSN_ARRAY *dest_arr, uint32_t axis, uint32_t nrows, uint32_t frame_size, uint32_t hop_size) {
    CSN_AXIS_SLICE_ITER it;
    if (AXIS_ITER_SLICE_INIT(&it, source_arr, dest_arr, axis) != OK) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Incompatible remap array shapes/ranks");
    }

    ITEM_TYPE itype = dest_arr->itype;

    CSN_STR_WRITER w;
    if (itype == CSN_STRING) w = csn_str_writer(csound, perf_h, dest_arr);

    const size_t frame_stride = dest_arr->strides[axis];
    while (AXIS_SLICE_ITER_NEXT(&it)) {
        for (uint32_t f = 0; f < nrows; f++) {
            size_t s = it.src_base + (size_t) f * hop_size * it.src_axis_stride;
            size_t d = it.dst_base + (size_t) f * frame_stride;
            for (uint32_t k = 0; k < frame_size; k++) {
                size_t d_index = d + k * it.dst_axis_stride;
                size_t s_index = s + k * it.src_axis_stride;
                switch (itype) {
                    case CSN_REAL:
                    case CSN_COMPLEX:
                        dest_arr->data[d_index * itype] = source_arr->data[s_index * itype];
                        if (itype == CSN_COMPLEX) {
                            dest_arr->data[d_index * itype + 1] = source_arr->data[s_index * itype + 1];
                        }
                        break;
                    case CSN_STRING: {
                            CSN_STRDATA *slot = &dest_arr->sdata[d_index];
                            const char *source_string = slot_string(&source_arr->sdata[s_index]);
                            int32_t res = csn_str_write(&w, slot, source_string, GET_STRINGDAT_LEN(source_string));
                            if (res != OK) return res;
                        }
                        break;
                }
            }
        }
    }

    return OK;
}

static int32_t framing_helper(CSOUND *csound, CSN_FRAME *p, bool is_perf) {
    OPDS *perf_h = is_perf ? &p->h : NULL;
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, perf_h, reg);

    bool is_k_form = csn_is_k_form(&p->h);
    cs_float *axis_arg = is_k_form ? p->arg_b : p->arg_a;
    if (is_perf) {
        p->k_data.registry = reg;
        CHECK_KTRIG(p->arg_a);
    } else if (is_k_form) {
        memset(&p->k_data, 0, sizeof(p->k_data));
        p->k_data.registry = reg;
    }

    double fsize = (double) *p->frame_size;
    double hsize = (double) *p->hop_size;
    if (!IS_VALID_LENGTH(fsize) || fsize < 1.0 || !IS_VALID_LENGTH(hsize) || hsize < 1.0) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] Frame and hop sizes must be whole numbers of at least 1");
    }

    uint32_t frame_size = (uint32_t) fsize;
    uint32_t hop_size = (uint32_t) hsize;

    int32_t res = OK;
    if (!is_perf) stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle }, 2);

    CSN_ARRAY *source_arr = p->source_handle;
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;
    ITEM_TYPE itype = source_arr->itype;

    if (source_ndim + 1 > CSN_MAX_DIMS) {
        res = CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Source must have fewer than %d dimensions", CSN_MAX_DIMS);
        goto done;
    }

    CSN_AXIS_SPEC axis_spec = csn_normalize_axis(axis_arg, source_ndim, CSN_AXIS_DEFAULT_LAST);
    if (axis_spec.kind == CSN_AXIS_INVALID) {
        res = CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Invalid axis value");
        goto done;
    }

    /* A k source nothing has computed yet is an empty placeholder at init:
       the output starts empty, with no frames, and the first pass frames. */
    uint32_t s_size = source_shape[axis_spec.index];
    uint32_t nrows = 0;
    if (s_size >= frame_size) {
        nrows = ((s_size - frame_size) / hop_size) + 1U;
    } else if (is_perf || source_arr->size != 0) {
        res = CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Frame size %u is larger than the source extent %u along the axis", frame_size, s_size);
        goto done;
    }

    uint32_t new_ndim = source_ndim + 1;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    for (uint32_t i = 0; i < new_ndim; i++) {
        if (i < axis_spec.index || i > axis_spec.index + 1) {
            new_shape[i] = source_shape[i < axis_spec.index ? i : i - 1];
        } else{
            if (i == axis_spec.index) new_shape[i] = nrows;
            if (i == axis_spec.index + 1) new_shape[i] = frame_size;
        }
    }

    const CSN_ARRAY *ops[1] = { p->source_handle };
    CSNARR_RES out_res = prepare_output(csound, &p->h, perf_h, reg, p->handle, new_ndim, new_shape, itype, ops, 1, CSN_ALIAS_REFUSE);
    res = out_res.err;
    if (res != OK) goto done;
    pres.locked |= out_res.locked;

    if (is_perf && CAN_REUSE_LAST_RESULT(&p->k_data, p->source_handle, p->handle)) goto done;

    res = framing_assign_value(csound, perf_h, source_arr, p->handle, axis_spec.index, nrows, frame_size, hop_size);
    if (res != OK) goto done;

    if (is_perf) {
        PUBLISH_DERIVED_RESULT(&p->k_data, p->source_handle, p->handle);
    }

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
 }

int32_t csnframe_framing(CSOUND *csound, CSN_FRAME *p) {
    return framing_helper(csound, p, false);
}

int32_t csnframe_framing_k(CSOUND *csound, CSN_FRAME *p) {
    return framing_helper(csound, p, true);
}


typedef enum { CSN_ZEROCROSSING, CSN_AVERAGEMAGNITUDO } CSN_TFEATURE_MODE;

static inline bool zcr_is_negative(double x) {
    return fabs(x) > 1e-10 && signbit(x);
}

static int32_t tfeatures_assign_value(CSOUND *csound, OPDS *perf_h, CSN_ARRAY *source_arr, CSN_ARRAY *dest_arr, double *out_value, uint32_t axis, CSN_TFEATURE_MODE mode, uint32_t lag) {
    if (dest_arr != NULL) {
        CSN_AXIS_SLICE_ITER it;
        if (AXIS_ITER_SLICE_INIT(&it, source_arr, dest_arr, axis) != OK) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Invalid reduction iterator layout");
        }

        while (AXIS_SLICE_ITER_NEXT(&it)) {
            const double *source_slice = source_arr->data + it.src_base;
            switch (mode) {
                case CSN_ZEROCROSSING: {
                        size_t crossings = 0;
                        bool prev = zcr_is_negative(source_slice[0]);
                        for (size_t i = 0; i < it.axis_size; i++) {
                            bool curr = zcr_is_negative(source_slice[i * it.src_axis_stride]);
                            crossings += curr != prev;
                            prev = curr;
                        }
                        dest_arr->data[it.dst_base] = (double) crossings / (double) it.axis_size;
                    }
                    break;
                case CSN_AVERAGEMAGNITUDO: {
                        /* D(tau) = sum |x[i] - x[i + tau]| / (N - tau), tau 0..maxlag:
                           the mean over the terms there are, so a longer lag
                           is not penalised for having fewer. */
                        double *dest_slice = dest_arr->data + it.dst_base;
                        for (size_t lag_curr = 0; lag_curr <= lag; lag_curr++) {
                            double acc = 0.0;
                            for (size_t i = 0; i + lag_curr < it.axis_size; i++) {
                                acc += fabs(source_slice[i * it.src_axis_stride] - source_slice[(i + lag_curr) * it.src_axis_stride]);
                            }
                            dest_slice[lag_curr * it.dst_axis_stride] = acc / (double) (it.axis_size - lag_curr);
                        }
                    }
                    break;
            }

        }
    } else {
        /* Only zcr has a number output: an empty source, a k placeholder at
           init among them, has no crossings. */
        if (source_arr->size == 0) {
            *out_value = 0.0;
            return OK;
        }

        size_t crossings = 0;
        bool prev = zcr_is_negative(source_arr->data[0]);
        for (size_t i = 1; i < source_arr->size; i++) {
            bool curr = zcr_is_negative(source_arr->data[i]);
            crossings += curr != prev;
            prev = curr;
        }
        *out_value = (double) crossings / (double) source_arr->size;
    }

    return OK;
}

static int32_t tfeature_helper(CSOUND *csound, CSN_TFEATURE *p, bool is_perf, CSN_TFEATURE_MODE mode) {
    OPDS *perf_h = is_perf ? &p->h : NULL;
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, perf_h, reg);

    /* After the source:
         csnzcr  (axis)             csnzcr.k  (trig, axis)
         csnamdf (maxlag, axis)     csnamdf.k (maxlag, trig, axis) */
    bool is_k_form = csn_is_k_form(&p->h);
    cs_float *trig_arg = NULL;
    cs_float *axis_arg = NULL;
    if (mode == CSN_ZEROCROSSING) {
        trig_arg = is_k_form ? p->arg_a : NULL;
        axis_arg = is_k_form ? p->arg_b : p->arg_a;
    } else {
        trig_arg = is_k_form ? p->arg_b : NULL;
        axis_arg = is_k_form ? p->arg_c : p->arg_b;
    }

    uint32_t amdf_lag = 0U;
    if (mode == CSN_AVERAGEMAGNITUDO) {
        double lag = (double) *p->arg_a;
        if (!IS_VALID_LENGTH(lag)) {
            return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] The AMDF maximum lag must be a whole number of at least 0");
        }
        amdf_lag = (uint32_t) lag;
    }

    if (is_perf) {
        p->k_data.registry = reg;
        CHECK_KTRIG(trig_arg);
    } else if (is_k_form) {
        memset(&p->k_data, 0, sizeof(p->k_data));
        p->k_data.registry = reg;
    }

    int32_t res = OK;
    if (!is_perf) stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle }, 2);

    CSN_ARRAY *source_arr = p->source_handle;
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;
    ITEM_TYPE itype = source_arr->itype;

    /* zcr reduces the axis away, so a 1-D source goes to the number form;
       amdf replaces it with the lags, so any rank keeps its own. */
    if (mode == CSN_ZEROCROSSING && source_ndim <= 1) {
        res = CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Source must have at least 2 dimensions; a 1-D source gives a number, assign it to an i or k variable");
        goto done;
    }

    CSN_AXIS_SPEC axis_spec = csn_normalize_axis(axis_arg, source_ndim, CSN_AXIS_DEFAULT_LAST);
    if (axis_spec.kind == CSN_AXIS_INVALID) {
        res = CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Invalid axis value");
        goto done;
    }

    if (itype != CSN_REAL) {
        res = CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, mode == CSN_ZEROCROSSING ? "[csnarray] Zero crossing rate requires a real array" : "[csnarray] AMDF requires a real array");
        goto done;
    }

    /* A k source nothing has computed yet is an empty placeholder at init:
       its extent says nothing, and the first pass checks the lag. */
    if (mode == CSN_AVERAGEMAGNITUDO && amdf_lag >= source_shape[axis_spec.index] && (is_perf || source_arr->size != 0)) {
        res = CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] AMDF maximum lag %u must be less than the source extent %u along the axis", amdf_lag, source_shape[axis_spec.index]);
        goto done;
    }

    uint32_t new_ndim = mode == CSN_ZEROCROSSING ? source_ndim - 1 : source_ndim;
    uint32_t shape[CSN_MAX_DIMS] = {0};
    if (mode == CSN_ZEROCROSSING) {
        for (uint32_t i = 0, j = 0; i < source_ndim; i++) {
            if (i != (uint32_t) axis_spec.index) shape[j++] = source_shape[i];
        }
    } else {
        memcpy(shape, source_shape, source_ndim * sizeof(uint32_t));
        shape[axis_spec.index] = amdf_lag + 1U;
    }

    const CSN_ARRAY *ops[1] = { p->source_handle };
    CSNARR_RES out_res = prepare_output(csound, &p->h, perf_h, reg, p->handle, new_ndim, shape, CSN_REAL, ops, 1, CSN_ALIAS_REFUSE);
    res = out_res.err;
    if (res != OK) goto done;
    pres.locked |= out_res.locked;

    if (is_perf && CAN_REUSE_LAST_RESULT(&p->k_data, p->source_handle, p->handle)) goto done;

    res = tfeatures_assign_value(csound, perf_h, source_arr, p->handle, NULL, axis_spec.index, mode, amdf_lag);
    if (res != OK) goto done;

    if (is_perf) {
        PUBLISH_DERIVED_RESULT(&p->k_data, p->source_handle, p->handle);
    }

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

static int32_t tfeature_scalar_helper(CSOUND *csound, CSN_TFEATURE_SCALAR *p, bool is_perf) {
    OPDS *perf_h = is_perf ? &p->h : NULL;
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, perf_h, reg);

    bool is_k_form = csn_is_k_form(&p->h);

    if (is_perf) {
        p->k_data.registry = reg;
        CHECK_KTRIG(p->trig);
    } else if (is_k_form) {
        memset(&p->k_data, 0, sizeof(p->k_data));
        p->k_data.registry = reg;
    }

    int32_t res = OK;
    if (!is_perf) stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);

    CSN_ARRAY *source_arr = p->source_handle;
    uint32_t source_ndim = source_arr->ndim;
    ITEM_TYPE itype = source_arr->itype;

    if (source_ndim != 1) {
        res = CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] A number output needs a 1-D source; assign to a CsnArr to reduce along an axis");
        goto done;
    }

    if (itype != CSN_REAL) {
        res = CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Zero crossing rate operation requires real array");
        goto done;
    }

    if (is_perf && CAN_REUSE_LAST_RESULT(&p->k_data, p->source_handle, NULL)) goto done;

    double value = 0.0;
    res = tfeatures_assign_value(csound, perf_h, source_arr, NULL, &value, 0, CSN_ZEROCROSSING, 0U);
    if (res != OK) goto done;
    *p->value = (cs_float) value;

    if (is_perf) {
        PUBLISH_DERIVED_RESULT(&p->k_data, p->source_handle, NULL);
    }

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}


int32_t csnframe_zcr(CSOUND *csound, CSN_TFEATURE *p) {
    return tfeature_helper(csound, p, false, CSN_ZEROCROSSING);
}

int32_t csnframe_zcr_k(CSOUND *csound, CSN_TFEATURE *p) {
    return tfeature_helper(csound, p, true, CSN_ZEROCROSSING);
}

int32_t csnframe_zcr_scalar(CSOUND *csound, CSN_TFEATURE_SCALAR *p) {
    return tfeature_scalar_helper(csound, p, false);
}

int32_t csnframe_zcr_scalar_k(CSOUND *csound, CSN_TFEATURE_SCALAR *p) {
    return tfeature_scalar_helper(csound, p, true);
}

int32_t csnframe_amdf(CSOUND *csound, CSN_TFEATURE *p) {
    return tfeature_helper(csound, p, false, CSN_AVERAGEMAGNITUDO);
}

int32_t csnframe_amdf_k(CSOUND *csound, CSN_TFEATURE *p) {
    return tfeature_helper(csound, p, true, CSN_AVERAGEMAGNITUDO);
}

/* Heap's algorithm: each permutation differs from the one before by a single
   swap. Writes one permutation per row of dest, len wide, and stops once
   rows have been written. */
static void heap_perm(double *buffer, double *dest, size_t n, size_t len, size_t *row, size_t rows) {
    if (*row >= rows) return;
    if (n <= 1) {
        memcpy(dest + *row * len, buffer, len * sizeof(double));
        (*row)++;
        return;
    }

    heap_perm(buffer, dest, n - 1, len, row, rows);
    for (size_t i = 0; i < n - 1 && *row < rows; i++) {
        size_t j = (n & 1) ? 0 : i;
        double temp = buffer[j];
        buffer[j] = buffer[n - 1];
        buffer[n - 1] = temp;
        heap_perm(buffer, dest, n - 1, len, row, rows);
    }
}

static int32_t heap_perm_helper(CSOUND *csound, CSN_PERM *p, bool is_perf) {
    OPDS *perf_h = is_perf ? &p->h : NULL;
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, perf_h, reg);

    bool is_k_form = csn_is_k_form(&p->h);
    cs_float *trig_arg = is_k_form ? p->trig : NULL;

    /* -1 asks for every permutation, a whole number for at most that many */
    double count_temp = (double) *p->n_perm;
    if (count_temp != -1.0 && !IS_VALID_LENGTH(count_temp)) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] The number of permutations must be -1, for all of them, or a whole number of at least 0");
    }

    if (is_perf) {
        p->k_data.registry = reg;
        CHECK_KTRIG(trig_arg);
    } else if (is_k_form) {
        memset(&p->k_data, 0, sizeof(p->k_data));
        p->k_data.registry = reg;
    }

    int32_t res = OK;
    if (!is_perf) stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle, p->source_handle }, 2);

    CSN_ARRAY *source_arr = p->source_handle;
    uint32_t source_ndim = source_arr->ndim;
    ITEM_TYPE itype = source_arr->itype;

    if (source_ndim != 1) {
        res = CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Source must be 1-D");
        goto done;
    }

    if (itype != CSN_REAL) {
        res = CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Permutations require a real array");
        goto done;
    }

    /* n! rows of n, as np.array(list(itertools.permutations(x))) but in
       Heap's order; an empty source has the one empty permutation. */
    size_t n = source_arr->size;
    size_t total = csn_factorial(n);
    size_t rows = count_temp == -1.0 || (size_t) count_temp > total ? total : (size_t) count_temp;
    if (rows > UINT32_MAX) {
        res = CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] %zu elements have too many permutations to hold; ask for at most %u", n, UINT32_MAX);
        goto done;
    }

    res = csn_scratch_reserve(csound, perf_h, source_arr->rt_locked, &p->buffer, n, sizeof(double));
    if (res != OK) goto done;

    uint32_t shape[CSN_MAX_DIMS] = { (uint32_t) rows, (uint32_t) n };
    const CSN_ARRAY *ops[1] = { p->source_handle };
    CSNARR_RES out_res = prepare_output(csound, &p->h, perf_h, reg, p->handle, 2, shape, CSN_REAL, ops, 1, CSN_ALIAS_REFUSE);
    res = out_res.err;
    if (res != OK) goto done;
    pres.locked |= out_res.locked;

    if (is_perf && CAN_REUSE_LAST_RESULT(&p->k_data, p->source_handle, p->handle)) goto done;

    if (n > 0) memcpy(p->buffer.scratch, source_arr->data, n * sizeof(double));
    /* an output kept empty after an empty source has no rows to fill */
    size_t row = 0;
    size_t out_rows = p->handle->size == rows * n ? rows : 0;
    heap_perm(p->buffer.scratch, p->handle->data, n, n, &row, out_rows);

    if (is_perf) {
        PUBLISH_DERIVED_RESULT(&p->k_data, p->source_handle, p->handle);
    }

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnframe_perm_deinit(CSOUND *csound, CSN_PERM *p) {
    deinit_scratch(csound, &p->buffer);
    return OK;
}

int32_t csnframe_perm(CSOUND *csound, CSN_PERM *p) {
    return heap_perm_helper(csound, p, false);
}

int32_t csnframe_perm_k(CSOUND *csound, CSN_PERM *p) {
    return heap_perm_helper(csound, p, true);
}
