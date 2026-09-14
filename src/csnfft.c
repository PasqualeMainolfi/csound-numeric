#include "csnregistry.h"
#include "csnfft.h"
#include "csnum.h"
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>


static inline bool IS_POWER_OF_TWO(size_t n) {
    return n != 0 && (n & (n - 1)) == 0;
}

static inline bool IS_VALID_FFT_SIZE(double value) {
    return isfinite(value) && !isnan(value) && trunc(value) == value && value > 0.0 && value <= (double) CSN_MAX_ELEMS;
}

static inline bool IS_VALID_NMFCC(double value) {
    return IS_VALID_FFT_SIZE(value);
}

static inline bool IS_VALID_SR(double value) {
    return isfinite(value) && !isnan(value) && trunc(value) == value && value > 0.0 && value <= (double) UINT32_MAX;
}

static inline bool IS_VALID_STFT_WIN(double value) {
    return isfinite(value) && !isnan(value) && trunc(value) == value && value >= 0.0 && value <= (double) NUMBER_OF_STFT_WINDOWS;
}

static bool IS_VALID_VALUE_GT_ZERO(double value) {
    return isfinite(value) && !isnan(value) && value > 0.0;
}

static bool IS_VALID_DCT_MODE(double value) {
    return isfinite(value) && !isnan(value) && trunc(value) == value && value >= 1.0 && value <= 2.0;
}

/* Csound hands a deallocated instrument's opcode memory to the next note
   without zeroing it, and an opcode inside a branch the new note does not take
   is deinitialized all the same. A pointer left behind here would therefore be
   freed a second time, so every buffer is cleared as it is released. */
static void FREE_CSNARRDATA(CSOUND *csound, CSN_ARRAY *array) {
    if (array->data != NULL) csound->Free(csound, array->data);
    memset(array, 0, sizeof(CSN_ARRAY));
}

static inline size_t NEXT_POWER_OF_TWO(size_t n) {
    if (n <= 1) return 1;
    n--;
    for (size_t shift = 1; shift < sizeof(size_t) * 8; shift <<= 1) n |= n >> shift;
    return n + 1;
}

static int32_t fft_body(CSOUND *csound, OPDS *perf_h, CSN_REGISTRY *reg, CSN_ARRAY **source_array, const MYFLT *axis_in, uint32_t *axis_out, uint32_t source_handle, CSN_FFT_MODE mode) {
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
    }

    CSN_ARRAY *source_arr = slot->array;
    uint32_t source_ndim = source_arr->ndim;
    ITEM_TYPE itype = source_arr->itype;

    if (itype == CSN_COMPLEX && mode == CSNRFFT) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Real-valued FFT requires real array");
    }

    if (axis_in != NULL && axis_out != NULL) {
        double axis_value = (double) *axis_in;
        if (axis_value != -1.0 && !IS_VALID_AXIS(axis_value, source_ndim)) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: -1 for last axes, or finite integers 0..%u)", axis_value, source_ndim, source_ndim - 1);
        }
        *axis_out = axis_value != -1.0 ? (uint32_t) axis_value : source_ndim - 1U;
    }

    *source_array = source_arr;
    return OK;
}

static void fft_assign_value(CSOUND *csound, void *fft_setup, CSN_ARRAY *fft_buffer, CSN_ARRAY *source_arr, MYFLT *temp_buffer, uint32_t nfft, size_t work_size, size_t out_size, uint32_t axis, CSN_FFT_MODE mode) {
    uint32_t reduced_shape[CSN_MAX_DIMS] = {0};
    uint32_t reduced_ndim = 0;
    size_t slice_count = 1;
    for (uint32_t i = 0; i < source_arr->ndim; ++i) {
        if (i != (uint32_t) axis) {
            reduced_shape[reduced_ndim++] = source_arr->shape[i];
            slice_count *= source_arr->shape[i];
        }
    }

    uint32_t *source_shape = source_arr->shape;
    uint32_t nfft_copy = source_shape[axis] < nfft ? source_shape[axis] : nfft;

    size_t src_stride = source_arr->strides[axis];
    size_t dst_stride = fft_buffer->strides[axis];
    for (size_t linear = 0; linear < slice_count; ++linear) {
        uint32_t dst_coords[CSN_MAX_DIMS] = {0};
        uint32_t src_coords[CSN_MAX_DIMS] = {0};

        memset(temp_buffer, 0, sizeof(MYFLT) * work_size);

        from_linear_to_coords(dst_coords, reduced_shape, linear, reduced_ndim);
        for (uint32_t i = 0, j = 0; i < source_arr->ndim; ++i) {
            src_coords[i] = (i == (uint32_t) axis) ? 0 : dst_coords[j++];
        }

        size_t src_base = from_coords_to_offset(src_coords, source_arr->strides, source_arr->ndim);
        size_t dst_base = from_coords_to_offset(src_coords, fft_buffer->strides, source_arr->ndim);
        for (uint32_t i = 0; i < nfft_copy; i++) {
            CSN_COMPLEXDAT z = slice_get(source_arr->data + src_base * source_arr->itype, i, src_stride, source_arr->itype);
            if (mode == CSNRFFT) {
                temp_buffer[i] = (MYFLT) z.re;
            } else {
                temp_buffer[i * 2] = (MYFLT) z.re;
                temp_buffer[i * 2 + 1] = mode == CSNFFT ? (MYFLT) z.im : FL(0.0);
            }
        }

        if (mode == CSNRFFT) {
            csound->RealFFT(csound, fft_setup, temp_buffer);
        } else {
            csound->ComplexFFT(csound, temp_buffer, (int32_t) nfft);
        }

        for (uint32_t i = 0; i < out_size; i++) {
            CSN_COMPLEXDAT y;
            if (mode == CSNRFFT) {
                if (i == 0) {
                    y.re = (double) temp_buffer[0];
                    y.im = 0.0;
                } else if (i == (nfft / 2U)) {
                    y.re = (double) temp_buffer[1];
                    y.im = 0.0;
                } else {
                    y.re = (double) temp_buffer[i * 2];
                    y.im = (double) temp_buffer[i * 2 + 1];
                }
            } else {
                    y.re = (double) temp_buffer[i * 2];
                    y.im = (double) temp_buffer[i * 2 + 1];
            }
            slice_put(fft_buffer->data + dst_base * CSN_COMPLEX, i, dst_stride, CSN_COMPLEX, y);
        }
    }
}

static void ifft_assign_value(CSOUND *csound, void *fft_setup, CSN_ARRAY *fft_buffer, CSN_ARRAY *source_arr, MYFLT *temp_buffer, uint32_t nfft, size_t work_size, size_t out_size, uint32_t axis, CSN_FFT_MODE mode) {
    uint32_t reduced_shape[CSN_MAX_DIMS] = {0};
    uint32_t reduced_ndim = 0;
    size_t slice_count = 1;
    for (uint32_t i = 0; i < source_arr->ndim; ++i) {
        if (i != (uint32_t) axis) {
            reduced_shape[reduced_ndim++] = source_arr->shape[i];
            slice_count *= source_arr->shape[i];
        }
    }

    uint32_t *source_shape = source_arr->shape;
    uint32_t nfft_copy = nfft;
    if (mode == CSNIFFT) {
        nfft_copy = source_shape[axis] < nfft ? source_shape[axis] : nfft;
    } else {
        nfft_copy = source_shape[axis] < nfft / 2U + 1U ? source_shape[axis] : nfft / 2U + 1U;
    }

    size_t src_stride = source_arr->strides[axis];
    size_t dst_stride = fft_buffer->strides[axis];
    for (size_t linear = 0; linear < slice_count; ++linear) {
        uint32_t dst_coords[CSN_MAX_DIMS] = {0};
        uint32_t src_coords[CSN_MAX_DIMS] = {0};

        memset(temp_buffer, 0, sizeof(MYFLT) * work_size);

        from_linear_to_coords(dst_coords, reduced_shape, linear, reduced_ndim);
        for (uint32_t i = 0, j = 0; i < source_arr->ndim; ++i) {
            src_coords[i] = (i == (uint32_t) axis) ? 0 : dst_coords[j++];
        }

        size_t src_base = from_coords_to_offset(src_coords, source_arr->strides, source_arr->ndim);
        size_t dst_base = from_coords_to_offset(src_coords, fft_buffer->strides, source_arr->ndim);
        for (uint32_t i = 0; i < nfft_copy; i++) {
            CSN_COMPLEXDAT z = slice_get(source_arr->data + src_base * source_arr->itype, i, src_stride, source_arr->itype);
            if (mode == CSNIRFFT) {
                if (i == 0) {
                    temp_buffer[0] = (MYFLT) z.re;
                } else if (i == (nfft / 2U)) {
                    temp_buffer[1] = (MYFLT) z.re;
                } else {
                    temp_buffer[i * 2] = (MYFLT) z.re;
                    temp_buffer[i * 2 + 1] = (MYFLT) z.im;
                }
            } else {
                temp_buffer[i * 2] = (MYFLT) z.re;
                temp_buffer[i * 2 + 1] = (MYFLT) z.im;
            }
        }

        if (mode == CSNIRFFT) {
            csound->RealFFT(csound, fft_setup, temp_buffer);
        } else {
            csound->InverseComplexFFT(csound, temp_buffer, (int32_t) nfft);
        }

        if (mode == CSNIRFFT) {
            double *dst = fft_buffer->data + dst_base * CSN_REAL;
            for (uint32_t i = 0; i < out_size; ++i) {
                CSN_COMPLEXDAT y = {
                    .re = (double) temp_buffer[i],
                    .im = 0.0
                };
                slice_put(dst, i, dst_stride, CSN_REAL, y);
            }
        } else {
            double *dst = fft_buffer->data + dst_base * CSN_COMPLEX;
            for (uint32_t i = 0; i < out_size; ++i) {
                CSN_COMPLEXDAT y = {
                    .re = (double) temp_buffer[2U * i],
                    .im = (double) temp_buffer[2U * i + 1U]
                };
                slice_put(dst, i, dst_stride, CSN_COMPLEX, y);
            }
        }
    }
}

static void fft_assign_layout(size_t *out_size, size_t *work_size, uint32_t *new_ndim, uint32_t *new_shape, CSN_ARRAY *source_arr, uint32_t nfft, CSN_FFT_MODE mode, uint32_t axis) {
    uint32_t *source_shape = source_arr->shape;
    *new_ndim = source_arr->ndim;
    memcpy(new_shape, source_shape, sizeof(uint32_t) * CSN_MAX_DIMS);
    if (work_size != NULL) {
        switch (mode) {
            case CSNFFT:
                *out_size = (size_t) nfft;
                *work_size = (size_t) nfft * 2;
                break;
            case CSNRFFT:
                *out_size = (size_t) nfft / 2 + 1;
                *work_size = (size_t) nfft;
                break;
            case CSNIFFT:
                *out_size = (size_t) nfft;
                *work_size = (size_t) nfft * 2U;
                break;
            case CSNIRFFT:
                *out_size = (size_t) nfft;
                *work_size = (size_t) nfft;
                break;
            default:
                break;
        }
    }
    new_shape[axis] = (uint32_t) *out_size;
}

static void fft_assign_layout_at(size_t *out_size, size_t *work_size, uint32_t *new_shape, size_t *nfft, CSN_FFT_MODE mode, uint32_t axis) {
    size_t temp_out_size = 0;
    size_t temp_work_size = 0;
    switch (mode) {
        case CSNFFT:
            temp_out_size = (size_t) nfft[axis];
            temp_work_size = (size_t) nfft[axis] * 2;
            break;
        case CSNRFFT:
            temp_out_size = (size_t) nfft[axis] / 2 + 1;
            temp_work_size = (size_t) nfft[axis];
            break;
        case CSNIFFT:
            temp_out_size = (size_t) nfft[axis];
            temp_work_size = (size_t) nfft[axis] * 2U;
            break;
        case CSNIRFFT:
            temp_out_size = (size_t) nfft[axis];
            temp_work_size = (size_t) nfft[axis];
            break;
        default:
            break;
    }
    out_size[axis] = temp_out_size;
    work_size[axis] = temp_work_size;
    new_shape[axis] = (uint32_t) temp_out_size;
}

static void fft_assign_flatten_layout(size_t *out_size, size_t *work_size, uint32_t *new_ndim, uint32_t *new_shape, CSN_ARRAY *source_arr, uint32_t nfft, CSN_FFT_MODE mode) {
    *new_ndim = 1U;
    if (work_size != NULL) {
        switch (mode) {
            case CSNFFT:
                *out_size = (size_t) nfft;
                *work_size = (size_t) nfft * 2;
                break;
            case CSNRFFT:
                *out_size = (size_t) nfft / 2 + 1;
                *work_size = (size_t) nfft;
                break;
            case CSNIFFT:
                *out_size = (size_t) nfft;
                *work_size = (size_t) nfft * 2U;
                break;
            case CSNIRFFT:
                *out_size = (size_t) nfft;
                *work_size = (size_t) nfft;
                break;
            default:
                break;
        }
    }
    new_shape[0] = (uint32_t) *out_size;
}

static int32_t csnarray_fft_helper(CSOUND *csound, CSN_FFT *p, CSN_FFT_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;
    int32_t res = OK;
    const char *err = NULL;
    MYFLT *temp_buffer = NULL;

    double fftsize_temp = (double) *p->fft_size;
    if (!IS_VALID_FFT_SIZE(fftsize_temp) || !IS_POWER_OF_TWO((uint32_t) fftsize_temp)) {
        return csound->InitError(csound, "[csnarray] FFT size must be a valid power of two value");
    }
    int32_t fft_size = (int32_t) fftsize_temp;

    void *fft_setup = NULL;

    csound->LockMutex(reg->mutex);
    CSN_ARRAY *source_arr = NULL;
    uint32_t axis = 0;
    res = fft_body(csound, NULL, reg, &source_arr, p->axis, &axis, source_handle, mode);
    if (res != OK) goto done;

    if (mode == CSNRFFT) {
        fft_setup = csound->RealFFTSetup(csound, fft_size, FFT_FWD);
    }

    uint32_t new_ndim;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    size_t out_size = 0;
    size_t work_size = 0;
    fft_assign_layout(&out_size, &work_size, &new_ndim, new_shape, source_arr, (uint32_t) fft_size, mode, axis);

    temp_buffer = csound->Calloc(csound, sizeof(MYFLT) * work_size);
    if (temp_buffer == NULL) {
        res = csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
        goto done;
    }

    if (create_csnarray_locked(csound, reg, &p->h, new_ndim, new_shape, &p->array, p->handle, &source_handle, 1U, &err, CSN_COMPLEX) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }
    CSN_ARRAY *fft_buffer = p->array;
    fft_assign_value(csound, fft_setup, fft_buffer, source_arr, temp_buffer, (uint32_t) fft_size, work_size, out_size, axis, mode);

    p->buffer.scratch = temp_buffer;
    p->buffer.scratch_capacity = work_size;
    p->k_data_fft.fft_setup = fft_setup;
    p->k_data_fft.nfft = (size_t) fft_size;
    p->k_data_fft.buffer_out_size = out_size;
    p->k_data_fft.buffer_work_size = work_size;
    p->k_data.prev_axis_u = axis;
    p->is_published = false;
    set_array_version(&p->k_data.prev_output_version, &p->array->version);
    set_array_version(&p->k_data.prev_source_version, &source_arr->version);
    SET_KDATA_BEGIN(p, reg);

done:
    if (res != OK && temp_buffer != NULL) csound->Free(csound, temp_buffer);
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_fft(CSOUND *csound, CSN_FFT *p) {
    return csnarray_fft_helper(csound, p, CSNFFT);
}

int32_t csnarray_rfft(CSOUND *csound, CSN_FFT *p) {
    return csnarray_fft_helper(csound, p, CSNRFFT);
}

static int32_t csnarray_fft_k_helper(CSOUND *csound, CSN_FFT *p, CSN_FFT_MODE mode) {
    CSN_REGISTRY *reg = p->k_data.registry;
    uint32_t owned_handle = p->k_data.owned_handle;
    CHECK_REG_HANDLE(csound, &p->h, reg, owned_handle);

    uint32_t source_handle = p->source_handle->id;
    int32_t res = OK;
    const char *err = NULL;

    CHECK_KTRIG(p->trig);

    size_t fft_size = p->k_data_fft.nfft;
    void *fft_setup = p->k_data_fft.fft_setup;

    csound->LockMutex(reg->mutex);
    CSN_ARRAY *source_arr = NULL;
    uint32_t axis = 0;
    res = fft_body(csound, &p->h, reg, &source_arr, p->axis, &axis, source_handle, mode);
    if (res != OK) goto done;

    if (p->is_published) {
        bool is_same_source = is_same_array_version(&p->k_data.prev_source_version, &source_arr->version);
        bool is_same_result = false;
        CSN_SLOT *slot = get_slot(reg, owned_handle);
        if (slot != NULL) {
            is_same_result = is_same_array_version(&p->k_data.prev_output_version, &slot->array->version);
        }

        if (is_same_source && is_same_result) {
            p->handle->id = owned_handle;
            goto done;
        }
    }

    uint32_t new_ndim = 0;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    size_t out_size = 0;
    size_t work_size = 0;
    fft_assign_layout(&out_size, &work_size, &new_ndim, new_shape, source_arr, (uint32_t) fft_size, mode, axis);

    size_t output_size = 0;
    if (get_array_size_from_shape(&output_size, new_ndim, new_shape) != OK) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] FFT output exceeds the maximum element count");
        goto done;
    }

    CSN_ARRAY *fft_buffer = NULL;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &fft_buffer, &p->k_data, NULL, new_ndim, new_shape, output_size, CSN_COMPLEX, err);
    if (res != OK) goto done;
    p->array = fft_buffer;

    MYFLT *temp_buffer = (MYFLT *) p->buffer.scratch;
    fft_assign_value(csound, fft_setup, fft_buffer, source_arr, temp_buffer, (uint32_t) fft_size, work_size, out_size, axis, mode);

    p->k_data.prev_axis_u = axis;
    set_array_version(&p->k_data.prev_source_version, &source_arr->version);
    set_array_version(&p->k_data.prev_output_version, &fft_buffer->version);
    SET_KDATA_END(p, fft_buffer->shape, fft_buffer->ndim, CSN_COMPLEX);
    p->is_published = true;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_fft_k(CSOUND *csound, CSN_FFT *p) {
    return csnarray_fft_k_helper(csound, p, CSNFFT);
}

int32_t csnarray_rfft_k(CSOUND *csound, CSN_FFT *p) {
    return csnarray_fft_k_helper(csound, p, CSNRFFT);
}

static int32_t csnarray_ifft_helper(CSOUND *csound, CSN_FFT *p, CSN_FFT_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;
    int32_t res = OK;
    const char *err = NULL;
    MYFLT *temp_buffer = NULL;
    void *fft_setup = NULL;

    double fftsize_temp = (double) *p->fft_size;
    if (!IS_VALID_FFT_SIZE(fftsize_temp) || !IS_POWER_OF_TWO((uint32_t) fftsize_temp)) {
        return csound->InitError(csound, "[csnarray] FFT size must be a valid power of two value");
    }
    int32_t fft_size = (int32_t) fftsize_temp;

    csound->LockMutex(reg->mutex);
    CSN_ARRAY *source_arr = NULL;
    uint32_t axis = 0;
    res = fft_body(csound, NULL, reg, &source_arr, p->axis, &axis, source_handle, mode);
    if (res != OK) goto done;

    if (mode == CSNIRFFT) {
        fft_setup = csound->RealFFTSetup(csound, fft_size, FFT_INV);
    }

    uint32_t new_ndim;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    size_t out_size = 0;
    size_t work_size = 0;
    fft_assign_layout(&out_size, &work_size, &new_ndim, new_shape, source_arr, (uint32_t) fft_size, mode, axis);

    temp_buffer = csound->Calloc(csound, sizeof(MYFLT) * work_size);
    if (temp_buffer == NULL) {
        res = csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
        goto done;
    }

    ITEM_TYPE otype = mode == CSNIRFFT ? CSN_REAL : CSN_COMPLEX;
    if (create_csnarray_locked(csound, reg, &p->h, new_ndim, new_shape, &p->array, p->handle, &source_handle, 1U, &err, otype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *fft_buffer = p->array;
    ifft_assign_value(csound, fft_setup, fft_buffer, source_arr, temp_buffer, (uint32_t) fft_size, work_size, out_size, axis, mode);

    p->buffer.scratch = temp_buffer;
    p->buffer.scratch_capacity = work_size;
    p->k_data_fft.fft_setup = fft_setup;
    p->k_data_fft.nfft = (size_t) fft_size;
    p->k_data_fft.buffer_out_size = out_size;
    p->k_data_fft.buffer_work_size = work_size;
    p->k_data.prev_axis_u = axis;
    p->is_published = false;
    set_array_version(&p->k_data.prev_output_version, &p->array->version);
    set_array_version(&p->k_data.prev_source_version, &source_arr->version);
    SET_KDATA_BEGIN(p, reg);

done:
    if (res != OK && temp_buffer != NULL) csound->Free(csound, temp_buffer);
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_fft_deinit(CSOUND *csound, CSN_FFT *p) {
    deinit_scratch(csound, &p->buffer);
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

int32_t csnarray_ifft(CSOUND *csound, CSN_FFT *p) {
    return csnarray_ifft_helper(csound, p, CSNIFFT);
}

int32_t csnarray_irfft(CSOUND *csound, CSN_FFT *p) {
    return csnarray_ifft_helper(csound, p, CSNIRFFT);
}

static int32_t csnarray_ifft_k_helper(CSOUND *csound, CSN_FFT *p, CSN_FFT_MODE mode) {
    CSN_REGISTRY *reg = p->k_data.registry;
    uint32_t owned_handle = p->k_data.owned_handle;
    CHECK_REG_HANDLE(csound, &p->h, reg, owned_handle);

    uint32_t source_handle = p->source_handle->id;
    int32_t res = OK;
    const char *err = NULL;

    CHECK_KTRIG(p->trig);

    size_t fft_size = p->k_data_fft.nfft;
    void *fft_setup = p->k_data_fft.fft_setup;

    csound->LockMutex(reg->mutex);
    CSN_ARRAY *source_arr = NULL;
    uint32_t axis = 0;
    res = fft_body(csound, &p->h, reg, &source_arr, p->axis, &axis, source_handle, mode);
    if (res != OK) goto done;

    if (p->is_published) {
        bool is_same_source = is_same_array_version(&p->k_data.prev_source_version, &source_arr->version);
        bool is_same_result = false;
        CSN_SLOT *slot = get_slot(reg, owned_handle);
        if (slot != NULL) {
            is_same_result = is_same_array_version(&p->k_data.prev_output_version, &slot->array->version);
        }

        if (is_same_source && is_same_result) {
            p->handle->id = owned_handle;
            goto done;
        }
    }

    uint32_t new_ndim = 0;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    size_t out_size = 0;
    size_t work_size = 0;
    fft_assign_layout(&out_size, &work_size, &new_ndim, new_shape, source_arr, (uint32_t) fft_size, mode, axis);

    size_t output_size = 0;
    if (get_array_size_from_shape(&output_size, new_ndim, new_shape) != OK) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] inverse FFT output exceeds the maximum element count");
        goto done;
    }

    ITEM_TYPE otype = mode == CSNIRFFT ? CSN_REAL : CSN_COMPLEX;
    CSN_ARRAY *fft_buffer = NULL;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &fft_buffer, &p->k_data, NULL, new_ndim, new_shape, output_size, otype, err);
    if (res != OK) goto done;
    p->array = fft_buffer;

    MYFLT *temp_buffer = (MYFLT *) p->buffer.scratch;
    ifft_assign_value(csound, fft_setup, fft_buffer, source_arr, temp_buffer, (uint32_t) fft_size, work_size, out_size, axis, mode);

    p->k_data.prev_axis_u = axis;
    set_array_version(&p->k_data.prev_source_version, &source_arr->version);
    set_array_version(&p->k_data.prev_output_version, &fft_buffer->version);
    SET_KDATA_END(p, fft_buffer->shape, fft_buffer->ndim, fft_buffer->itype);
    p->is_published = true;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_ifft_k(CSOUND *csound, CSN_FFT *p) {
    return csnarray_ifft_k_helper(csound, p, CSNIFFT);
}

int32_t csnarray_irfft_k(CSOUND *csound, CSN_FFT *p) {
    return csnarray_ifft_k_helper(csound, p, CSNIRFFT);
}

static int32_t stft_body(CSOUND *csound, OPDS *perf_h, CSN_REGISTRY *reg, CSN_ARRAY **source_array, uint32_t source_handle, CSN_FFT_MODE *fft_mode) {
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
    }

    CSN_ARRAY *source_arr = slot->array;
    uint32_t source_ndim = source_arr->ndim;
    if (source_ndim != 1U) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] STFT requires 1-D array");
    }

    ITEM_TYPE itype = source_arr->itype;
    *fft_mode = itype == CSN_COMPLEX ? CSNFFT : CSNRFFT;

    *source_array = source_arr;
    return OK;
}

static int32_t istft_body(CSOUND *csound, OPDS *perf_h, CSN_REGISTRY *reg, CSN_ARRAY **source_array, uint32_t source_handle, uint32_t nfft) {
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
    }

    CSN_ARRAY *source_arr = slot->array;
    uint32_t source_ndim = source_arr->ndim;
    if (source_ndim != 2U) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] ISTFT requires 2-D array");
    }

    if (source_arr->itype != CSN_COMPLEX) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] ISTFT requires complex array");
    }

    uint32_t nbins = source_arr->shape[0];
    if (nbins != nfft && nbins != nfft / 2U + 1U) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Number of row must be equal to fft size");
    }

    uint32_t nframes = source_arr->shape[1];
    if (nframes == 0) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Zero-length of shape[1] (number of frames)");
    }

    if (source_arr->size > CSN_MAX_ELEMS) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Array length exceed the maximum element count");
    }

    *source_array = source_arr;
    return OK;
}

static void stft_assign_layout(size_t *fft_out_size, size_t *fft_work_size, uint32_t *new_shape_f, uint32_t *new_shape_t, uint32_t *new_shape_z, CSN_ARRAY *source_arr, uint32_t nfft, uint32_t hopsize, CSN_FFT_MODE mode) {
    size_t s_size = source_arr->size;
    uint32_t nrows = mode == CSNFFT ? nfft : nfft / 2U + 1U;
    uint32_t ncols = s_size < (size_t) nfft ? 1U : 1U + (uint32_t) ((s_size - nfft) / hopsize);
    if (new_shape_f != NULL) new_shape_f[0] = nrows;
    if (new_shape_t != NULL) new_shape_t[0] = ncols;
    new_shape_z[0] = nrows;
    new_shape_z[1] = ncols;
    switch (mode) {
        case CSNFFT:
            *fft_out_size = (size_t) nfft;
            *fft_work_size = (size_t) nfft * 2;
            break;
        case CSNRFFT:
            *fft_out_size = (size_t) nfft / 2 + 1;
            *fft_work_size = (size_t) nfft;
            break;
        case CSNIFFT:
            *fft_out_size = (size_t) nfft;
            *fft_work_size = (size_t) nfft * 2U;
            break;
        case CSNIRFFT:
            *fft_out_size = (size_t) nfft;
            *fft_work_size = (size_t) nfft;
            break;
        default:
            break;
    }
}

static int32_t istft_assign_layout(CSOUND *csound, OPDS *perf_h, size_t *ifft_out_size, size_t *ifft_work_size, uint32_t *new_shape_t, uint32_t *new_shape_x, CSN_ARRAY *source_arr, uint32_t nfft, uint32_t hopsize, CSN_FFT_MODE mode) {
    size_t extra_frames = (size_t) source_arr->shape[1] - 1U;
    if (extra_frames > (CSN_MAX_ELEMS - (size_t) nfft) / (size_t) hopsize) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] ISTFT output would exceed the maximum element count");
    }

    size_t s_size = (size_t) nfft + extra_frames * (size_t) hopsize;
    new_shape_t[0] = (uint32_t) s_size;
    new_shape_x[0] = (uint32_t) s_size;
    switch (mode) {
        case CSNFFT:
            *ifft_out_size = (size_t) nfft;
            *ifft_work_size = (size_t) nfft * 2;
            break;
        case CSNRFFT:
            *ifft_out_size = (size_t) nfft / 2 + 1;
            *ifft_work_size = (size_t) nfft;
            break;
        case CSNIFFT:
            *ifft_out_size = (size_t) nfft;
            *ifft_work_size = (size_t) nfft * 2U;
            break;
        case CSNIRFFT:
            *ifft_out_size = (size_t) nfft;
            *ifft_work_size = (size_t) nfft;
            break;
        default:
            break;
    }
    return OK;
}

static void stft_assign_value(CSOUND *csound, void *fft_setup, CSN_ARRAY *fft_buffer, CSN_ARRAY *source_arr, MYFLT *temp_buffer, const double *win, uint32_t nfft, uint32_t hopsize, size_t work_size, size_t out_size, CSN_FFT_MODE mode) {
    size_t source_size = source_arr->size;
    ITEM_TYPE itype = source_arr->itype;
    uint32_t nframes = fft_buffer->shape[1];
    for (uint32_t i = 0; i < nframes; i++) {
        memset(temp_buffer, 0, sizeof(MYFLT) * work_size);
        size_t start = (size_t) i * hopsize;
        size_t remaining = source_size - start;
        uint32_t current_size = remaining < nfft ? (uint32_t) remaining : nfft;
        for (uint32_t j = 0; j < current_size; j++) {
            CSN_COMPLEXDAT z = slice_get(source_arr->data + start * itype, j, 1U, itype);
            if (mode == CSNRFFT) {
                temp_buffer[j] = (MYFLT) (z.re * win[j]);
            } else {
                temp_buffer[j * 2] = (MYFLT) (z.re * win[j]);
                temp_buffer[j * 2 + 1] = mode == CSNFFT ? (MYFLT) (z.im * win[j]) : FL(0.0);
            }
        }

        if (mode == CSNRFFT) {
            csound->RealFFT(csound, fft_setup, temp_buffer);
        } else {
            csound->ComplexFFT(csound, temp_buffer, (int32_t) nfft);
        }

        for (uint32_t j = 0; j < out_size; j++) {
            CSN_COMPLEXDAT y;
            if (mode == CSNRFFT) {
                if (j == 0) {
                    y.re = (double) temp_buffer[0];
                    y.im = 0.0;
                } else if (j == (nfft / 2U)) {
                    y.re = (double) temp_buffer[1];
                    y.im = 0.0;
                } else {
                    y.re = (double) temp_buffer[j * 2];
                    y.im = (double) temp_buffer[j * 2 + 1];
                }
            } else {
                y.re = (double) temp_buffer[j * 2];
                y.im = (double) temp_buffer[j * 2 + 1];
            }
            double *frame_col = fft_buffer->data + (size_t) i * CSN_COMPLEX;
            slice_put(frame_col, j, fft_buffer->strides[0], CSN_COMPLEX, y);
        }
    }
}

static void istft_assign_value(CSOUND *csound, void *fft_setup, CSN_ARRAY *fft_buffer, CSN_ARRAY *source_arr, MYFLT *temp_buffer, const double *win, const double *win_sum, uint32_t nfft, uint32_t hopsize, size_t work_size, size_t out_size, CSN_FFT_MODE mode) {
    uint32_t *source_shape = source_arr->shape;
    uint32_t nbins = source_shape[0];
    uint32_t nframes = source_shape[1];

    for (uint32_t i = 0; i < nframes; i++) {
        memset(temp_buffer, 0, sizeof(MYFLT) * work_size);
        double *frame_col = source_arr->data + (size_t) i * CSN_COMPLEX;
        for (uint32_t j = 0; j < nbins; j++) {
            CSN_COMPLEXDAT z = slice_get(frame_col, j, source_arr->strides[0], CSN_COMPLEX);
            if (mode == CSNIRFFT) {
                if (j == 0U) {
                    temp_buffer[0] = (MYFLT) z.re;
                } else if (j == (nfft / 2U)) {
                    temp_buffer[1] = (MYFLT) z.re;
                } else {
                    temp_buffer[j * 2] = (MYFLT) z.re;
                    temp_buffer[j * 2 + 1] = (MYFLT) z.im;
                }
            } else {
                temp_buffer[j * 2] = (MYFLT) z.re;
                temp_buffer[j * 2 + 1] = (MYFLT) z.im;
            }
        }

        if (mode == CSNIRFFT) {
            csound->RealFFT(csound, fft_setup, temp_buffer);
        } else {
            csound->InverseComplexFFT(csound, temp_buffer, (int32_t) nfft);
        }

        for (uint32_t j = 0; j < out_size; j++) {
            size_t k = (size_t) i * hopsize + j;
            double den = win_sum[k];
            if (den > 1e-12) {
                double scale = win[j] / den;
                if (mode == CSNIRFFT) {
                    fft_buffer->data[k] += (double) temp_buffer[j] * scale;
                } else {
                    fft_buffer->data[k * 2] += (double) temp_buffer[j * 2] * scale;
                    fft_buffer->data[k * 2 + 1] += (double) temp_buffer[j * 2 + 1] * scale;
                }
            }
        }
    }
}

static void get_fftfreqs(CSN_ARRAY *freqs, double sr, uint32_t nfft, CSN_FFT_MODE mode) {
    size_t first_negative = ((size_t) nfft + 1U) / 2U;
    bool cond_one = mode == CSNRFFT || mode == CSNRFFTFREQ;
    bool cond_two = mode == CSNFFT || mode == CSNFFTFREQ;
    for (size_t i = 0; i < freqs->size; i++) {
        if (cond_one || i < first_negative) {
            freqs->data[i] = (double) i * sr / (double) nfft;
        } else if (cond_two) {
            freqs->data[i] = ((double) i - (double) nfft) * sr / (double) nfft;
        }
    }
}

static void get_timevec(CSN_ARRAY *times, double sr, double nfft, double hopsize) {
    for (size_t i = 0; i < times->size; i++) {
        times->data[i] = ((double) i * hopsize + 0.5 * nfft) / sr; // as scipy -> center of window
    }
}

static void get_timesamples(CSN_ARRAY *times, double sr) {
    for (size_t i = 0; i < times->size; i++) {
        times->data[i] = (double) i / sr;
    }
}

static void get_window_function_sum(double *w, const double *win, uint32_t nframes, uint32_t fft_size, uint32_t hopsize) {
    for (uint32_t i = 0; i < nframes; i++) {
        for (uint32_t j = 0; j < fft_size; j++) {
            size_t k = (size_t) i * hopsize + j;
            w[k] += win[j] * win[j];
        }
    }
}

static int32_t stft_validate_params(CSOUND *csound, OPDS *perf_h, const MYFLT *winsize, const MYFLT *hopsize, const MYFLT *sr, const MYFLT *window_type) {
    double fftsize_temp = (double) *winsize;
    if (!IS_VALID_FFT_SIZE(fftsize_temp) || !IS_POWER_OF_TWO((uint32_t) fftsize_temp)) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] FFT size must be a valid power of two value");
    }
    double hopsize_temp = (double) *hopsize;
    if (!IS_VALID_FFT_SIZE(hopsize_temp)) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Hop size must be a positive finite integer");
    }
    if (sr != NULL) {
        double sr_temp = (double) *sr;
        if (!IS_VALID_SR(sr_temp)) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Invalid sr value");
        }
    }
    double win = (double) *window_type;
    if (!IS_VALID_STFT_WIN(win)) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown window type");
    }
    return OK;
}

static int32_t csnarray_stft_helper(CSOUND *csound, CSN_STFT *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;
    int32_t res = OK;
    const char *err = NULL;
    MYFLT *temp_buffer = NULL;
    double *win_buffer = NULL;

    res = stft_validate_params(csound, NULL, p->winsize, p->hopsize, p->sr, p->window_type);
    if (res != OK) return res;

    int32_t fft_size = (int32_t) *p->winsize;
    int32_t overlap_size = (int32_t) *p->hopsize;
    double sr = (double) *p->sr;
    uint32_t wtype = (uint32_t) *p->window_type;
    void *fft_setup = NULL;

    csound->LockMutex(reg->mutex);
    CSN_ARRAY *source_arr = NULL;
    CSN_FFT_MODE fft_mode;
    res = stft_body(csound, NULL, reg, &source_arr, source_handle, &fft_mode);
    if (res != OK) goto done;

    /* Keep the real setup available even if a k-rate source later switches
       between real and complex storage. ComplexFFT simply ignores it. */
    fft_setup = csound->RealFFTSetup(csound, fft_size, FFT_FWD);

    uint32_t new_dim_f_t = 1U;
    uint32_t new_dim_z = 2U;
    uint32_t new_shape_f[CSN_MAX_DIMS] = {0};
    uint32_t new_shape_t[CSN_MAX_DIMS] = {0};
    uint32_t new_shape_z[CSN_MAX_DIMS] = {0};
    size_t out_size = 0;
    size_t work_size = 0;
    stft_assign_layout(&out_size, &work_size, new_shape_f, new_shape_t, new_shape_z, source_arr, (uint32_t) fft_size, (uint32_t) overlap_size, fft_mode);

    size_t scratch_work_size = (size_t) fft_size * 2U;
    temp_buffer = csound->Calloc(csound, sizeof(MYFLT) * scratch_work_size);
    if (temp_buffer == NULL) {
        res = csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
        goto done;
    }

    win_buffer = csound->Calloc(csound, sizeof(double) * (size_t) fft_size);
    if (win_buffer == NULL) {
        res = csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
        goto done;
    }

    if (create_csnarray_locked(csound, reg, &p->h, new_dim_f_t, new_shape_f, &p->array_f, p->handle_f, &source_handle, 1U, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }
    CSN_ARRAY *f_buffer = p->array_f;

    if (create_csnarray_locked(csound, reg, &p->h, new_dim_f_t, new_shape_t, &p->array_t, p->handle_t, &source_handle, 1U, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }
    CSN_ARRAY *t_buffer = p->array_t;

    if (create_csnarray_locked(csound, reg, &p->h, new_dim_z, new_shape_z, &p->array_z, p->handle_z, &source_handle, 1U, &err, CSN_COMPLEX) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }
    CSN_ARRAY *fft_buffer = p->array_z;

    get_fftfreqs(f_buffer, sr, (uint32_t) fft_size, fft_mode);
    get_timevec(t_buffer, sr, (double) fft_size, (double) overlap_size);
    get_window_function(win_buffer, (uint32_t) fft_size, wtype, 0.0);
    stft_assign_value(csound, fft_setup, fft_buffer, source_arr, temp_buffer, win_buffer, (uint32_t) fft_size, (uint32_t) overlap_size, work_size, out_size, fft_mode);

    SET_FROM_KDATA_WITH_ID_BEGIN(p->k_data_f, reg, new_shape_f, new_dim_f_t, CSN_REAL, p->handle_f->id);
    SET_FROM_KDATA_WITH_ID_BEGIN(p->k_data_t, reg, new_shape_t, new_dim_f_t, CSN_REAL, p->handle_t->id);
    SET_FROM_KDATA_WITH_ID_BEGIN(p->k_data_z, reg, new_shape_z, new_dim_z, CSN_COMPLEX, p->handle_z->id);
    set_array_version(&p->k_data_z.prev_source_version, &source_arr->version);
    p->buffer.scratch = temp_buffer;
    p->buffer.scratch_capacity = scratch_work_size;
    p->window.scratch = win_buffer;
    p->window.scratch_capacity = (size_t) fft_size;
    p->k_data_fft.nfft = (size_t) fft_size;
    p->k_data_fft.hopsize = (size_t) overlap_size;
    p->k_data_fft.fft_setup = fft_setup;
    p->k_data_fft.sr = sr;
    p->k_data_fft.mode = fft_mode;
    p->k_data_fft.buffer_out_size = out_size;
    p->k_data_fft.buffer_work_size = work_size;
    p->is_published = false;

done:
    if (res != OK) {
        if (temp_buffer != NULL) csound->Free(csound, temp_buffer);
        if (win_buffer != NULL) csound->Free(csound, win_buffer);
    }
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t GET_STFT_INIT(CSOUND *csound, OPDS *perf_h, CSN_ARRAY *source_arr, CSN_RAW_STFT *p) {
    int32_t res = OK;
    res = stft_validate_params(csound, perf_h, &p->winsize, &p->hopsize, NULL, &p->window_type);
    if (res != OK) return res;

    int32_t fft_size = (int32_t) p->winsize;
    int32_t overlap_size = (int32_t) p->hopsize;
    uint32_t wtype = (uint32_t) p->window_type;

    uint32_t source_ndim = source_arr->ndim;
    if (source_ndim != 1U) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] STFT requires 1-D array");
    }

    ITEM_TYPE itype = source_arr->itype;
    CSN_FFT_MODE fft_mode = itype == CSN_COMPLEX ? CSNFFT : CSNRFFT;

    p->fft_setup = csound->RealFFTSetup(csound, fft_size, FFT_FWD);

    uint32_t new_dim_z = 2U;
    uint32_t new_shape_z[CSN_MAX_DIMS] = {0};
    size_t out_size = 0;
    size_t work_size = 0;
    stft_assign_layout(&out_size, &work_size, NULL, NULL, new_shape_z, source_arr, (uint32_t) fft_size, (uint32_t) overlap_size, fft_mode);

    size_t scratch_work_size = (size_t) fft_size * 2U;
    MYFLT *temp_buffer = csound->Calloc(csound, sizeof(MYFLT) * scratch_work_size);
    if (temp_buffer == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Internal error: memory allocation failed");
    }

    double *win_buffer = csound->Calloc(csound, sizeof(double) * (size_t) fft_size);
    if (win_buffer == NULL) {
        csound->Free(csound, temp_buffer);
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Internal error: memory allocation failed");
    }

    FREE_CSNARRDATA(csound, &p->stft);
    if (allocate_array(csound, &p->stft, new_dim_z, new_shape_z, 0, CSN_COMPLEX) != OK) {
        csound->Free(csound, temp_buffer);
        csound->Free(csound, win_buffer);
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Internal error: memory allocation failed");
    }
    get_window_function(win_buffer, fft_size, wtype, 0.0);

    deinit_scratch(csound, &p->buffer);
    deinit_scratch(csound, &p->window);
    p->buffer.scratch = temp_buffer;
    p->buffer.scratch_capacity = scratch_work_size;
    p->window.scratch = win_buffer;
    p->window.scratch_capacity = (size_t) fft_size;
    p->fft_out_size = out_size;
    p->fft_work_size = work_size;

    return res;
}

static int32_t GET_STFT(CSOUND *csound, CSN_ARRAY *source_arr, CSN_RAW_STFT *p) {
    int32_t fft_size = (int32_t) p->winsize;
    int32_t overlap_size = (int32_t) p->hopsize;

    MYFLT *temp_buffer = (MYFLT *) p->buffer.scratch;
    double *win = (double *) p->window.scratch;
    CSN_FFT_MODE fft_mode = source_arr->itype == CSN_COMPLEX ? CSNFFT : CSNRFFT;
    stft_assign_value(csound, p->fft_setup, &p->stft, source_arr, temp_buffer, win, fft_size, overlap_size, p->fft_work_size, p->fft_out_size, fft_mode);
    return OK;
}

static int32_t csnarray_istft_helper(CSOUND *csound, CSN_ISTFT *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;
    int32_t res = OK;
    const char *err = NULL;
    MYFLT *temp_buffer = NULL;
    double *win_buffer = NULL;
    double *winsum_buffer = NULL;

    res = stft_validate_params(csound, NULL, p->winsize, p->hopsize, p->sr, p->window_type);
    if (res != OK) return res;

    int32_t fft_size = (int32_t) *p->winsize;
    int32_t overlap_size = (int32_t) *p->hopsize;
    double sr = (double) *p->sr;
    uint32_t wtype = (uint32_t) *p->window_type;

    void *ifft_setup = NULL;

    csound->LockMutex(reg->mutex);
    CSN_ARRAY *source_arr = NULL;
    res = istft_body(csound, NULL, reg, &source_arr, source_handle, (uint32_t) fft_size);
    if (res != OK) goto done;

    CSN_FFT_MODE ifft_mode = source_arr->shape[0] == (uint32_t) fft_size / 2U + 1U ? CSNIRFFT : CSNIFFT;
    /* As for STFT, retain both inverse paths for a k-rate source whose bin
       layout changes between full and one-sided spectra. */
    ifft_setup = csound->RealFFTSetup(csound, (int32_t) fft_size, FFT_INV);

    ITEM_TYPE itype = ifft_mode == CSNIRFFT ? CSN_REAL : CSN_COMPLEX;

    uint32_t new_dim_t = 1U;
    uint32_t new_dim_x = 1U;
    uint32_t new_shape_t[CSN_MAX_DIMS] = {0};
    uint32_t new_shape_x[CSN_MAX_DIMS] = {0};
    size_t out_size = 0;
    size_t work_size = 0;
    res = istft_assign_layout(csound, NULL, &out_size, &work_size, new_shape_t, new_shape_x, source_arr, (uint32_t) fft_size, (uint32_t) overlap_size, ifft_mode);
    if (res != OK) goto done;

    if (work_size > CSN_MAX_ELEMS) {
        res = csound->InitError(csound, "[csnarray] Array length exceed the maximum element count");
        goto done;
    }

    size_t scratch_work_size = (size_t) fft_size * 2U;
    temp_buffer = csound->Calloc(csound, sizeof(MYFLT) * scratch_work_size);
    if (temp_buffer == NULL) {
        res = csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
        goto done;
    }

    win_buffer = csound->Calloc(csound, sizeof(double) * (size_t) fft_size);
    if (win_buffer == NULL) {
        res = csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
        goto done;
    }

    if (create_csnarray_locked(csound, reg, &p->h, new_dim_t, new_shape_t, &p->array_t, p->handle_t, &source_handle, 1U, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }
    CSN_ARRAY *t_buffer = p->array_t;

    if (create_csnarray_locked(csound, reg, &p->h, new_dim_x, new_shape_x, &p->array_x, p->handle_x, &source_handle, 1U, &err, itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }
    CSN_ARRAY *fft_buffer = p->array_x;

    winsum_buffer = csound->Calloc(csound, sizeof(double) * p->array_x->size);
    if (winsum_buffer == NULL) {
        res = csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
        goto done;
    }

    get_window_function(win_buffer, (uint32_t) fft_size, wtype, 0.0);
    get_window_function_sum(winsum_buffer, win_buffer, source_arr->shape[1], (uint32_t) fft_size, (uint32_t) overlap_size);
    get_timesamples(t_buffer, sr);
    istft_assign_value(csound, ifft_setup, fft_buffer, source_arr, temp_buffer, win_buffer, winsum_buffer, (uint32_t) fft_size, (uint32_t) overlap_size, work_size, out_size, ifft_mode);

    SET_FROM_KDATA_WITH_ID_BEGIN(p->k_data_t, reg, new_shape_t, new_dim_t, CSN_REAL, p->handle_t->id);
    SET_FROM_KDATA_WITH_ID_BEGIN(p->k_data_x, reg, new_shape_x, new_dim_x, itype, p->handle_x->id);
    set_array_version(&p->k_data_x.prev_source_version, &source_arr->version);
    p->buffer.scratch = temp_buffer;
    p->buffer.scratch_capacity = scratch_work_size;
    p->window.scratch = win_buffer;
    p->window.scratch_capacity = (size_t) fft_size;
    p->window_sum.scratch = winsum_buffer;
    p->window_sum.scratch_capacity = p->array_x->size;
    p->k_data_fft.nfft = (size_t) fft_size;
    p->k_data_fft.hopsize = (size_t) overlap_size;
    p->k_data_fft.fft_setup = ifft_setup;
    p->k_data_fft.sr = sr;
    p->k_data_fft.mode = ifft_mode;
    p->k_data_fft.buffer_out_size = out_size;
    p->k_data_fft.buffer_work_size = work_size;
    p->is_published = false;

done:
    if (res != OK) {
        if (temp_buffer != NULL) csound->Free(csound, temp_buffer);
        if (win_buffer != NULL) csound->Free(csound, win_buffer);
        if (winsum_buffer != NULL) csound->Free(csound, winsum_buffer);
    }
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_stft_deinit(CSOUND *csound, CSN_STFT *p) {
    int32_t res = OK;
    deinit_scratch(csound, &p->buffer);
    deinit_scratch(csound, &p->window);
    res = csnarray_deinit_by_handle(csound, &p->handle_f->id, &p->array_f, &p->h);
    if (res != OK) return res;
    res = csnarray_deinit_by_handle(csound, &p->handle_t->id, &p->array_t, &p->h);
    if (res != OK) return res;
    res = csnarray_deinit_by_handle(csound, &p->handle_z->id, &p->array_z, &p->h);
    return res;
}

int32_t csnarray_istft_deinit(CSOUND *csound, CSN_ISTFT *p) {
    int32_t res = OK;
    deinit_scratch(csound, &p->buffer);
    deinit_scratch(csound, &p->window);
    deinit_scratch(csound, &p->window_sum);
    res = csnarray_deinit_by_handle(csound, &p->handle_t->id, &p->array_t, &p->h);
    if (res != OK) return res;
    res = csnarray_deinit_by_handle(csound, &p->handle_x->id, &p->array_x, &p->h);
    return res;
}

int32_t csnarray_stft(CSOUND *csound, CSN_STFT *p) {
    return csnarray_stft_helper(csound, p);
}

int32_t csnarray_istft(CSOUND *csound, CSN_ISTFT *p) {
    return csnarray_istft_helper(csound, p);
}

static int32_t csnarray_stft_k_helper(CSOUND *csound, CSN_STFT *p) {
    CSN_REGISTRY *reg = p->k_data_z.registry;
    uint32_t owned_handle_f = p->k_data_f.owned_handle;
    uint32_t owned_handle_t = p->k_data_t.owned_handle;
    uint32_t owned_handle_z = p->k_data_z.owned_handle;
    CHECK_REGISTRY(csound, &p->h, reg);
    CHECK_HANDLE(csound, &p->h, owned_handle_f);
    CHECK_HANDLE(csound, &p->h, owned_handle_t);
    CHECK_HANDLE(csound, &p->h, owned_handle_z);

    uint32_t source_handle = p->source_handle->id;
    int32_t res = OK;
    const char *err = NULL;

    CHECK_KTRIG(p->trig);

    size_t fft_size = p->k_data_fft.nfft;
    size_t overlap_size = p->k_data_fft.hopsize;
    double sr = p->k_data_fft.sr;
    void *fft_setup = p->k_data_fft.fft_setup;

    csound->LockMutex(reg->mutex);
    CSN_ARRAY *source_arr = NULL;
    CSN_FFT_MODE fft_mode;
    res = stft_body(csound, &p->h, reg, &source_arr, source_handle, &fft_mode);
    if (res != OK) goto done;

    if (p->is_published) {
        bool is_same_source = is_same_array_version(&p->k_data_z.prev_source_version, &source_arr->version);
        bool is_same_f = false;
        bool is_same_t = false;
        bool is_same_z = false;
        CSN_SLOT *slot_f = get_slot(reg, owned_handle_f);
        CSN_SLOT *slot_t = get_slot(reg, owned_handle_t);
        CSN_SLOT *slot_z = get_slot(reg, owned_handle_z);
        if (slot_f != NULL && slot_t != NULL && slot_z != NULL) {
            is_same_f = is_same_array_version(&p->k_data_f.prev_output_version, &slot_f->array->version);
            is_same_t = is_same_array_version(&p->k_data_t.prev_output_version, &slot_t->array->version);
            is_same_z = is_same_array_version(&p->k_data_z.prev_output_version, &slot_z->array->version);
        }

        if (is_same_source && is_same_f && is_same_t && is_same_z) {
            p->handle_f->id = owned_handle_f;
            p->handle_t->id = owned_handle_t;
            p->handle_z->id = owned_handle_z;
            goto done;
        }
    }

    uint32_t shape_f[CSN_MAX_DIMS] = {0};
    uint32_t shape_t[CSN_MAX_DIMS] = {0};
    uint32_t shape_z[CSN_MAX_DIMS] = {0};
    size_t out_size = 0;
    size_t work_size = 0;
    stft_assign_layout(&out_size, &work_size, shape_f, shape_t, shape_z, source_arr, (uint32_t) fft_size, (uint32_t) overlap_size, fft_mode);

    size_t size_z = 0;
    if (get_array_size_from_shape(&size_z, 2U, shape_z) != OK) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] STFT output exceeds the maximum element count");
        goto done;
    }

    CSN_ARRAY *array_f = NULL;
    CSN_ARRAY *array_t = NULL;
    CSN_ARRAY *array_z = NULL;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &array_f, &p->k_data_f, NULL, 1U, shape_f, shape_f[0], CSN_REAL, err);
    if (res != OK) goto done;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &array_t, &p->k_data_t, NULL, 1U, shape_t, shape_t[0], CSN_REAL, err);
    if (res != OK) goto done;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &array_z, &p->k_data_z, NULL, 2U, shape_z, size_z, CSN_COMPLEX, err);
    if (res != OK) goto done;
    p->array_f = array_f;
    p->array_t = array_t;
    p->array_z = array_z;

    MYFLT *temp_buffer = (MYFLT *) p->buffer.scratch;
    const double *win_buffer = (const double *) p->window.scratch;
    get_fftfreqs(array_f, sr, (uint32_t) fft_size, fft_mode);
    get_timevec(array_t, sr, (double) fft_size, (double) overlap_size);
    stft_assign_value(csound, fft_setup, array_z, source_arr, temp_buffer, win_buffer, (uint32_t) fft_size, (uint32_t) overlap_size, work_size, out_size, fft_mode);

    SET_FROM_KDATA_END_WITH_ID(p->k_data_f, p->handle_f, p->array_f->shape, p->array_f->ndim, CSN_REAL);
    SET_FROM_KDATA_END_WITH_ID(p->k_data_t, p->handle_t, p->array_t->shape, p->array_t->ndim, CSN_REAL);
    SET_FROM_KDATA_END_WITH_ID(p->k_data_z, p->handle_z, p->array_z->shape, p->array_z->ndim, p->array_z->itype);
    set_array_version(&p->k_data_f.prev_output_version, &p->array_f->version);
    set_array_version(&p->k_data_t.prev_output_version, &p->array_t->version);
    set_array_version(&p->k_data_z.prev_output_version, &p->array_z->version);
    set_array_version(&p->k_data_z.prev_source_version, &source_arr->version);
    p->k_data_fft.mode = fft_mode;
    p->k_data_fft.buffer_out_size = out_size;
    p->k_data_fft.buffer_work_size = work_size;
    p->is_published = true;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_istft_k_helper(CSOUND *csound, CSN_ISTFT *p) {
    CSN_REGISTRY *reg = p->k_data_x.registry;
    uint32_t owned_handle_t = p->k_data_t.owned_handle;
    uint32_t owned_handle_x = p->k_data_x.owned_handle;
    CHECK_REGISTRY(csound, &p->h, reg);
    CHECK_HANDLE(csound, &p->h, owned_handle_t);
    CHECK_HANDLE(csound, &p->h, owned_handle_x);

    uint32_t source_handle = p->source_handle->id;
    int32_t res = OK;
    const char *err = NULL;

    CHECK_KTRIG(p->trig) ;

    size_t fft_size = p->k_data_fft.nfft;
    size_t overlap_size = p->k_data_fft.hopsize;
    double sr = p->k_data_fft.sr;
    void *ifft_setup = p->k_data_fft.fft_setup;

    csound->LockMutex(reg->mutex);
    CSN_ARRAY *source_arr = NULL;
    res = istft_body(csound, &p->h, reg, &source_arr, source_handle, (uint32_t) fft_size);
    if (res != OK) goto done;

    if (p->is_published) {
        bool is_same_source = is_same_array_version(&p->k_data_x.prev_source_version, &source_arr->version);
        bool is_same_t = false;
        bool is_same_x = false;
        CSN_SLOT *slot_t = get_slot(reg, owned_handle_t);
        CSN_SLOT *slot_x = get_slot(reg, owned_handle_x);
        if (slot_t != NULL && slot_x != NULL) {
            is_same_t = is_same_array_version(&p->k_data_t.prev_output_version, &slot_t->array->version);
            is_same_x = is_same_array_version(&p->k_data_x.prev_output_version, &slot_x->array->version);
        }

        if (is_same_source && is_same_t && is_same_x) {
            p->handle_t->id = owned_handle_t;
            p->handle_x->id = owned_handle_x;
            goto done;
        }
    }

    CSN_FFT_MODE ifft_mode = source_arr->shape[0] == (uint32_t) fft_size / 2U + 1U ? CSNIRFFT : CSNIFFT;
    ITEM_TYPE otype = ifft_mode == CSNIRFFT ? CSN_REAL : CSN_COMPLEX;
    uint32_t shape_t[CSN_MAX_DIMS] = {0};
    uint32_t shape_x[CSN_MAX_DIMS] = {0};
    size_t out_size = 0;
    size_t work_size = 0;
    res = istft_assign_layout(csound, &p->h, &out_size, &work_size, shape_t, shape_x, source_arr, (uint32_t) fft_size, (uint32_t) overlap_size, ifft_mode);
    if (res != OK) goto done;

    CSN_ARRAY *array_t = NULL;
    CSN_ARRAY *array_x = NULL;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &array_t, &p->k_data_t, NULL, 1U, shape_t, shape_t[0], CSN_REAL, err);
    if (res != OK) goto done;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &array_x, &p->k_data_x, NULL, 1U, shape_x, shape_x[0], otype, err);
    if (res != OK) goto done;
    p->array_t = array_t;
    p->array_x = array_x;

    /* Sized to the signal output, so on a marked output it can only grow
       where that slot has already been refused a new shape above. */
    res = csn_scratch_reserve(csound, &p->h, csn_slot_rt_locked(reg, p->k_data_x.owned_handle), &p->window_sum, array_x->size, sizeof(double));
    if (res != OK) goto done;

    MYFLT *temp_buffer = (MYFLT *) p->buffer.scratch;
    const double *win_buffer = (const double *) p->window.scratch;
    double *winsum_buffer = (double *) p->window_sum.scratch;
    memset(array_x->data, 0, sizeof(double) * array_x->size * (size_t) array_x->itype);
    memset(winsum_buffer, 0, sizeof(double) * array_x->size);
    get_window_function_sum(winsum_buffer, win_buffer, source_arr->shape[1], (uint32_t) fft_size, (uint32_t) overlap_size);
    get_timesamples(array_t, sr);
    istft_assign_value(csound, ifft_setup, array_x, source_arr, temp_buffer, win_buffer, winsum_buffer, (uint32_t) fft_size, (uint32_t) overlap_size, work_size, out_size, ifft_mode);

    SET_FROM_KDATA_END_WITH_ID(p->k_data_t, p->handle_t, p->array_t->shape, p->array_t->ndim, CSN_REAL);
    SET_FROM_KDATA_END_WITH_ID(p->k_data_x, p->handle_x, p->array_x->shape, p->array_x->ndim, p->array_x->itype);
    set_array_version(&p->k_data_t.prev_output_version, &p->array_t->version);
    set_array_version(&p->k_data_x.prev_output_version, &p->array_x->version);
    set_array_version(&p->k_data_x.prev_source_version, &source_arr->version);
    p->k_data_fft.mode = ifft_mode;
    p->k_data_fft.buffer_out_size = out_size;
    p->k_data_fft.buffer_work_size = work_size;
    p->is_published = true;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_stft_k(CSOUND *csound, CSN_STFT *p) {
    return csnarray_stft_k_helper(csound, p);
}

int32_t csnarray_istft_k(CSOUND *csound, CSN_ISTFT *p) {
    return csnarray_istft_k_helper(csound, p);
}

static int32_t csnarray_fftfreq_helper(CSOUND *csound, CSN_FFTFREQ *p, CSN_FFT_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    const char *err = NULL;

    double size_temp = (double) *p->size;
    if (!IS_VALID_FFT_SIZE(size_temp)) {
        return csound->InitError(csound, "[csnarray] Invalid size");
    }
    uint32_t size = (uint32_t) size_temp;
    if (!IS_VALID_VALUE_GT_ZERO((double) *p->d)) {
        return csound->InitError(csound, "[csnarray] Invalid sample spacing value");
    }
    double sample_spacing = 1.0 / (double) *p->d;

    csound->LockMutex(reg->mutex);
    uint32_t ndim = 1U;
    uint32_t shape[CSN_MAX_DIMS] = {0};
    shape[0] = mode == CSNRFFTFREQ ? size / 2U + 1U : size;

    if (create_csnarray_locked(csound, reg, &p->h, ndim, shape, &p->array, p->handle, NULL, 0, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    get_fftfreqs(p->array, sample_spacing, size, mode);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_fftfreq_k_init_helper(CSOUND *csound, CSN_FFTFREQ *p, CSN_FFT_MODE mode) {
    (void) mode;
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);
    uint32_t ndim = 1U;
    uint32_t shape[CSN_MAX_DIMS] = {0};
    shape[0] = DEFAULT_TEMPORARY_BUFFER_SIZE;

    if (create_csnarray_locked(csound, reg, &p->h, ndim, shape, &p->array, p->handle, NULL, 0, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    reset_empty_csnarray(p->array, ndim, shape, CSN_REAL);

    SET_KDATA_BEGIN(p, reg);
    p->k_data.prev_output_version = p->array->version;
    p->is_published = false;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_fftfreq_deinit(CSOUND *csound, CSN_FFTFREQ *p) {
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

int32_t csnarray_fftfreq(CSOUND *csound, CSN_FFTFREQ *p) {
    return csnarray_fftfreq_helper(csound, p, CSNFFTFREQ);
}

int32_t csnarray_rfftfreq(CSOUND *csound, CSN_FFTFREQ *p) {
    return csnarray_fftfreq_helper(csound, p, CSNRFFTFREQ);
}

static int32_t csnarray_fftfreq_k_helper(CSOUND *csound, CSN_FFTFREQ *p, CSN_FFT_MODE mode) {
    CSN_REGISTRY *reg = p->k_data.registry;
    uint32_t owned_handle = p->k_data.owned_handle;
    CHECK_REG_HANDLE(csound, &p->h, reg, owned_handle);

    CHECK_KTRIG(p->trig);
    int32_t res = OK;
    const char *err = NULL;

    double size_temp = (double) *p->size;
    if (!IS_VALID_FFT_SIZE(size_temp)) {
        return csound->InitError(csound, "[csnarray] Invalid size");
    }
    uint32_t size = (uint32_t) size_temp;

    if (!IS_VALID_VALUE_GT_ZERO((double) *p->d)) {
        return csound->InitError(csound, "[csnarray] Invalid sample spacing value");
    }
    double sample_spacing = 1.0 / (double) *p->d;

    csound->LockMutex(reg->mutex);
    if (p->is_published) {
        bool is_same_size = p->k_data.prev_size == size;
        bool is_same_d = p->k_data.prev_scalar_param == sample_spacing;
        bool is_same_result = false;
        CSN_SLOT *slot = get_slot(reg, owned_handle);
        if (slot != NULL) {
            is_same_result = is_same_array_version(&p->k_data.prev_output_version, &slot->array->version);
        }

        if (is_same_size && is_same_d && is_same_result) {
            p->handle->id = owned_handle;
            goto done;
        }
    }

    uint32_t ndim = 1U;
    uint32_t shape[CSN_MAX_DIMS] = {0};
    shape[0] = mode == CSNRFFTFREQ ? size / 2U + 1U : size;

    size_t req_size = 0;
    if (get_array_size_from_shape(&req_size, ndim, shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSN_ARRAY *arr = NULL;
    size_t logical_size = req_size;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, ndim, shape, logical_size, CSN_REAL, err);
    if (res != OK) goto done;
    p->array = arr;

    get_fftfreqs(p->array, sample_spacing, size, mode);

    SET_KDATA_END(p, p->array->shape, p->array->ndim, CSN_REAL);
    set_array_version(&p->k_data.prev_output_version, &p->array->version);
    p->k_data.prev_size = size;
    p->k_data.prev_scalar_param = sample_spacing;
    p->is_published = true;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_fftfreq_k(CSOUND *csound, CSN_FFTFREQ *p) {
    return csnarray_fftfreq_k_helper(csound, p, CSNFFTFREQ);
}

int32_t csnarray_rfftfreq_k(CSOUND *csound, CSN_FFTFREQ *p) {
    return csnarray_fftfreq_k_helper(csound, p, CSNRFFTFREQ);
}

int32_t csnarray_fftfreq_k_init(CSOUND *csound, CSN_FFTFREQ *p) {
    return csnarray_fftfreq_k_init_helper(csound, p, CSNFFTFREQ);
}

int32_t csnarray_rfftfreq_k_init(CSOUND *csound, CSN_FFTFREQ *p) {
    return csnarray_fftfreq_k_init_helper(csound, p, CSNRFFTFREQ);
}

int32_t csnarray_fftshift_deinit(CSOUND *csound, CSN_FFTSHIFT *p) {
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

static void fftshift_assign_value(CSN_ARRAY *arr, CSN_ARRAY *source_arr, uint32_t axis, CSN_FFT_MODE mode) {
    uint32_t reduced_shape[CSN_MAX_DIMS] = {0};
    uint32_t reduced_ndim = 0;
    size_t slice_count = 1;
    for (uint32_t i = 0; i < source_arr->ndim; ++i) {
        if (i != axis) {
            reduced_shape[reduced_ndim++] = source_arr->shape[i];
            slice_count *= source_arr->shape[i];
        }
    }

    size_t src_stride = source_arr->strides[axis];
    size_t dst_stride = arr->strides[axis];
    for (size_t linear = 0; linear < slice_count; ++linear) {
        uint32_t dst_coords[CSN_MAX_DIMS] = {0};
        uint32_t src_coords[CSN_MAX_DIMS] = {0};
        uint32_t slice_size = source_arr->shape[axis];
        from_linear_to_coords(dst_coords, reduced_shape, linear, reduced_ndim);
        for (uint32_t i = 0, j = 0; i < source_arr->ndim; ++i) {
            src_coords[i] = (i == (uint32_t) axis) ? 0 : dst_coords[j++];
        }

        size_t src_base = from_coords_to_offset(src_coords, source_arr->strides, source_arr->ndim);
        size_t dst_base = from_coords_to_offset(src_coords, arr->strides, source_arr->ndim);
        uint32_t shift_offset = mode == CSNFFTSHIFT ? (slice_size + 1) / 2 : (slice_size / 2);
        for (uint32_t i = 0; i < slice_size; i++) {
            size_t src_index = (i + shift_offset) % slice_size;
            CSN_COMPLEXDAT z = slice_get(source_arr->data + src_base * source_arr->itype, src_index, src_stride, source_arr->itype);
            slice_put(arr->data + dst_base * source_arr->itype, i, dst_stride, source_arr->itype, z);
        }
    }
}

static int32_t csnarray_fftshift_helper(CSOUND *csound, CSN_FFTSHIFT *p, CSN_FFT_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    const char *err = NULL;

    double axis_value = (double) *p->axis;

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = slot->array;
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    if (axis_value != -1.0 && !IS_VALID_AXIS(axis_value, source_ndim)) {
        res = csound->InitError(csound, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: -1 for last axes, or finite integers 0..%u)", axis_value, source_ndim, source_ndim - 1);
        goto done;
    }
    uint32_t axis = axis_value == -1.0 ? source_ndim - 1 : (uint32_t) axis_value;

    uint32_t new_ndim = source_ndim;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    memcpy(new_shape, source_shape, sizeof(uint32_t) * CSN_MAX_DIMS);

    if (create_csnarray_locked(csound, reg, &p->h, new_ndim, new_shape, &p->array, p->handle, &source_handle, 1U, &err, source_arr->itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    fftshift_assign_value(p->array, source_arr, axis,  mode);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_fftshift(CSOUND *csound, CSN_FFTSHIFT *p) {
    return csnarray_fftshift_helper(csound, p, CSNFFTSHIFT);
}

int32_t csnarray_ifftshift(CSOUND *csound, CSN_FFTSHIFT *p) {
    return csnarray_fftshift_helper(csound, p, CSNIFFTSHIFT);
}

static int32_t csnarray_fftshift_k_init_helper(CSOUND *csound, CSN_FFTSHIFT *p, CSN_FFT_MODE mode) {
    (void) mode;
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
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    uint32_t new_ndim = source_ndim;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    memcpy(new_shape, source_shape, sizeof(uint32_t) * CSN_MAX_DIMS);

    if (create_csnarray_locked(csound, reg, &p->h, new_ndim, new_shape, &p->array, p->handle, &source_handle, 1U, &err, source_arr->itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    reset_empty_csnarray(p->array, new_ndim, new_shape, source_arr->itype);

    SET_KDATA_BEGIN(p, reg);
    set_array_version(&p->k_data.prev_output_version, &p->array->version);
    set_array_version(&p->k_data.prev_source_version, &source_arr->version);
    p->is_published = false;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_fftshift_k_helper(CSOUND *csound, CSN_FFTSHIFT *p, CSN_FFT_MODE mode) {
    CSN_REGISTRY *reg = p->k_data.registry;
    uint32_t owned_handle = p->k_data.owned_handle;
    CHECK_REG_HANDLE(csound, &p->h, reg, owned_handle);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    const char *err = NULL;

    CHECK_KTRIG(p->trig);

    double axis_value = (double) *p->axis;

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
    }

    CSN_ARRAY *source_arr = slot->array;
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    if (axis_value != -1.0 && !IS_VALID_AXIS(axis_value, source_ndim)) {
        csound->UnlockMutex(reg->mutex);
        return csound->InitError(csound, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: -1 for last axes, or finite integers 0..%u)", axis_value, source_ndim, source_ndim - 1);
    }
    uint32_t axis = axis_value == -1.0 ? source_ndim - 1 : (uint32_t) axis_value;

    if (p->is_published) {
        bool is_same_source = is_same_array_version(&p->k_data.prev_source_version, &source_arr->version);
        bool is_axis = p->k_data.prev_axis_u == axis;
        bool is_same_result = false;
        CSN_SLOT *res_slot = get_slot(reg, owned_handle);
        if (res_slot != NULL) {
            is_same_result = is_same_array_version(&p->k_data.prev_output_version, &res_slot->array->version);
        }

        if (is_same_source && is_axis && is_same_result) {
            p->handle->id = owned_handle;
            goto done;
        }
    }

    uint32_t new_ndim = source_ndim;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    memcpy(new_shape, source_shape, sizeof(uint32_t) * CSN_MAX_DIMS);

    size_t req_size = 0;
    if (get_array_size_from_shape(&req_size, new_ndim, new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSN_ARRAY *arr = NULL;
    size_t logical_size = req_size;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, new_ndim, new_shape, logical_size, source_arr->itype, err);
    if (res != OK) goto done;
    p->array = arr;

    fftshift_assign_value(p->array, source_arr, axis,  mode);

    SET_KDATA_END(p, new_shape, new_ndim, source_arr->itype);
    set_array_version(&p->k_data.prev_output_version, &p->array->version);
    set_array_version(&p->k_data.prev_source_version, &source_arr->version);
    p->k_data.prev_axis_u = axis;
    p->is_published = true;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_fftshift_k_init(CSOUND *csound, CSN_FFTSHIFT *p) {
    return csnarray_fftshift_k_init_helper(csound, p, CSNFFTSHIFT);
}

int32_t csnarray_ifftshift_k_init(CSOUND *csound, CSN_FFTSHIFT *p) {
    return csnarray_fftshift_k_init_helper(csound, p, CSNIFFTSHIFT);
}

int32_t csnarray_fftshift_k(CSOUND *csound, CSN_FFTSHIFT *p) {
    return csnarray_fftshift_k_helper(csound, p, CSNFFTSHIFT);
}

int32_t csnarray_ifftshift_k(CSOUND *csound, CSN_FFTSHIFT *p) {
    return csnarray_fftshift_k_helper(csound, p, CSNIFFTSHIFT);
}

static int32_t fft2_body(CSOUND *csound, OPDS *perf_h, CSN_REGISTRY *reg, CSN_ARRAY **source_array, uint32_t source_handle, CSN_FFT_MODE mode) {
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
    }

    CSN_ARRAY *source_arr = slot->array;
    uint32_t source_ndim = source_arr->ndim;
    ITEM_TYPE itype = source_arr->itype;

    if (itype == CSN_COMPLEX && mode == CSNRFFT) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Real-valued FFT requires real array");
    }

    if (source_ndim != 2U) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] fft2 is allowed for 2-D array only");
    }

    *source_array = source_arr;
    return OK;
}

static void fft2_assign_size(size_t *out_size, size_t *work_size, size_t nfft, CSN_FFT_MODE mode) {
    switch (mode) {
        case CSNFFT:
            *out_size = (size_t) nfft;
            *work_size = (size_t) nfft * 2;
            break;
        case CSNRFFT:
            *out_size = (size_t) nfft / 2 + 1;
            *work_size = (size_t) nfft;
            break;
        case CSNIFFT:
            *out_size = (size_t) nfft;
            *work_size = (size_t) nfft * 2U;
            break;
        case CSNIRFFT:
            *out_size = (size_t) nfft;
            *work_size = (size_t) nfft;
            break;
        default:
            break;
    }
}

static int32_t fft2_init_intermediate(CSOUND *csound, CSN_ARRAY *array, const uint32_t *shape) {
    size_t size = 0;
    if (get_array_size_from_shape(&size, 2U, shape) != OK) {
        return csound->InitError(csound, "[csnarray] FFT2 intermediate array exceeds the maximum element count");
    }

    size_t capacity = size > 0U ? size : 1U;
    array->data = csound->Calloc(csound, sizeof(double) * capacity * CSN_COMPLEX);
    if (array->data == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
    }

    array->capacity = capacity;
    set_csnarray_layout(array, 2U, shape, size, CSN_COMPLEX);
    return OK;
}

static void fft2_assign_value(CSOUND *csound, void *fft_setup, CSN_ARRAY *fft_buffer, CSN_ARRAY *source_arr, MYFLT *temp_buffer, uint32_t nfft, size_t work_size, size_t out_size, uint32_t axis, CSN_FFT_MODE mode) {
    uint32_t *source_shape = source_arr->shape;
    uint32_t nfft_copy = source_shape[axis] < nfft ? source_shape[axis] : nfft;
    size_t rows = source_shape[0];
    size_t cols = source_shape[1];
    size_t slice_count = axis == 0U ? cols : rows;
    size_t destination_slices = axis == 0U ? fft_buffer->shape[1] : fft_buffer->shape[0];
    if (slice_count > destination_slices) slice_count = destination_slices;

    size_t src_stride = source_arr->strides[axis];
    size_t dst_stride = fft_buffer->strides[axis];
    for (size_t s = 0; s < slice_count; ++s) {
        memset(temp_buffer, 0, sizeof(MYFLT) * work_size);

        size_t src_base = axis == 0U ? s * source_arr->strides[1] : s * source_arr->strides[0];
        size_t dst_base = axis == 0U ? s * fft_buffer->strides[1] : s * fft_buffer->strides[0];
        for (uint32_t i = 0; i < nfft_copy; i++) {
            CSN_COMPLEXDAT z = slice_get(source_arr->data + src_base * source_arr->itype, i, src_stride, source_arr->itype);
            if (mode == CSNRFFT) {
                temp_buffer[i] = (MYFLT) z.re;
            } else {
                temp_buffer[i * 2] = (MYFLT) z.re;
                temp_buffer[i * 2 + 1] = mode == CSNFFT ? (MYFLT) z.im : FL(0.0);
            }
        }

        if (mode == CSNRFFT) {
            csound->RealFFT(csound, fft_setup, temp_buffer);
        } else {
            csound->ComplexFFT(csound, temp_buffer, (int32_t) nfft);
        }

        for (uint32_t i = 0; i < out_size; i++) {
            CSN_COMPLEXDAT y;
            if (mode == CSNRFFT) {
                if (i == 0) {
                    y.re = (double) temp_buffer[0];
                    y.im = 0.0;
                } else if (i == (nfft / 2U)) {
                    y.re = (double) temp_buffer[1];
                    y.im = 0.0;
                } else {
                    y.re = (double) temp_buffer[i * 2];
                    y.im = (double) temp_buffer[i * 2 + 1];
                }
            } else {
                    y.re = (double) temp_buffer[i * 2];
                    y.im = (double) temp_buffer[i * 2 + 1];
            }
            slice_put(fft_buffer->data + dst_base * CSN_COMPLEX, i, dst_stride, CSN_COMPLEX, y);
        }
    }
}

static int32_t csnarray_fft2_helper(CSOUND *csound, CSN_FFT2 *p, CSN_FFT_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;
    int32_t res = OK;
    const char *err = NULL;
    MYFLT *row_temp_buffer = NULL;
    MYFLT *col_temp_buffer = NULL;
    CSN_ARRAY intermediate = {0};

    double rows_fftsize_temp = (double) *p->rows_fft_size;
    if (!IS_VALID_FFT_SIZE(rows_fftsize_temp) || !IS_POWER_OF_TWO((uint32_t) rows_fftsize_temp)) {
        return csound->InitError(csound, "[csnarray] FFT size must be a valid power of two value");
    }
    int32_t rows_fft_size = (int32_t) rows_fftsize_temp;

    double cols_fftsize_temp = (double) *p->cols_fft_size;
    if (!IS_VALID_FFT_SIZE(cols_fftsize_temp) || !IS_POWER_OF_TWO((uint32_t) cols_fftsize_temp)) {
        return csound->InitError(csound, "[csnarray] FFT size must be a valid power of two value");
    }
    int32_t cols_fft_size = (int32_t) cols_fftsize_temp;

    void *cols_fft_setup = NULL;

    csound->LockMutex(reg->mutex);
    CSN_ARRAY *source_arr = NULL;
    res = fft2_body(csound, NULL, reg, &source_arr, source_handle, mode);
    if (res != OK) goto done;

    if (mode == CSNRFFT) {
        cols_fft_setup = csound->RealFFTSetup(csound, cols_fft_size, FFT_FWD);
    }

    size_t rows_out_size = 0;
    size_t rows_work_size = 0;
    size_t cols_out_size = 0;
    size_t cols_work_size = 0;
    fft2_assign_size(&rows_out_size, &rows_work_size, (size_t) rows_fft_size, CSNFFT);
    fft2_assign_size(&cols_out_size, &cols_work_size, (size_t) cols_fft_size, mode);
    uint32_t new_ndim = 2U;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    new_shape[0] = (uint32_t) rows_out_size;
    new_shape[1] = (uint32_t) cols_out_size;
    size_t output_size = 0;
    if (get_array_size_from_shape(&output_size, new_ndim, new_shape) != OK) {
        res = csound->InitError(csound, "[csnarray] FFT2 output exceeds the maximum element count");
        goto done;
    }

    uint32_t intermediate_shape[CSN_MAX_DIMS] = {0};
    /* The intermediate is fixed by the requested transform size, not by the
       source layout. This lets k-rate sources grow or shrink safely: the first
       pass truncates excess slices and the cleared tail provides zero-padding. */
    intermediate_shape[0] = (uint32_t) rows_out_size;
    intermediate_shape[1] = (uint32_t) cols_out_size;
    res = fft2_init_intermediate(csound, &intermediate, intermediate_shape);
    if (res != OK) goto done;

    row_temp_buffer = csound->Calloc(csound, sizeof(MYFLT) * rows_work_size);
    if (row_temp_buffer == NULL) {
        res = csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
        goto done;
    }
    col_temp_buffer = csound->Calloc(csound, sizeof(MYFLT) * cols_work_size);
    if (col_temp_buffer == NULL) {
        res = csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
        goto done;
    }

    if (create_csnarray_locked(csound, reg, &p->h, new_ndim, new_shape, &p->array, p->handle, &source_handle, 1U, &err, CSN_COMPLEX) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *fft_buffer = p->array;
    fft2_assign_value(csound, cols_fft_setup, &intermediate, source_arr, col_temp_buffer, (uint32_t) cols_fft_size, cols_work_size, cols_out_size, 1U, mode);
    fft2_assign_value(csound, NULL, fft_buffer, &intermediate, row_temp_buffer, (uint32_t) rows_fft_size, rows_work_size, rows_out_size, 0U, CSNFFT);

    SET_KDATA_BEGIN(p, reg);
    set_array_version(&p->k_data.prev_output_version, &fft_buffer->version);
    set_array_version(&p->k_data.prev_source_version, &source_arr->version);
    p->k_data_row_fft.nfft = (size_t) rows_fft_size;
    p->k_data_row_fft.buffer_out_size = rows_out_size;
    p->k_data_row_fft.buffer_work_size = rows_work_size;
    p->k_data_row_fft.fft_setup = NULL;
    p->k_data_col_fft.nfft = (size_t) cols_fft_size;
    p->k_data_col_fft.buffer_out_size = cols_out_size;
    p->k_data_col_fft.buffer_work_size = cols_work_size;
    p->k_data_col_fft.fft_setup = cols_fft_setup;
    p->intermediate = intermediate;
    p->row_buffer.scratch = row_temp_buffer;
    p->row_buffer.scratch_capacity = rows_work_size;
    p->col_buffer.scratch = col_temp_buffer;
    p->col_buffer.scratch_capacity = cols_work_size;
    p->is_published = false;

done:
    if (res != OK) {
        if (row_temp_buffer != NULL) csound->Free(csound, row_temp_buffer);
        if (col_temp_buffer != NULL) csound->Free(csound, col_temp_buffer);
        if (intermediate.data != NULL) csound->Free(csound, intermediate.data);
    }
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_fft2(CSOUND *csound, CSN_FFT2 *p) {
    return csnarray_fft2_helper(csound, p, CSNFFT);
}

int32_t csnarray_rfft2(CSOUND *csound, CSN_FFT2 *p) {
    return csnarray_fft2_helper(csound, p, CSNRFFT);
}

static void ifft2_assign_value(CSOUND *csound, void *fft_setup, CSN_ARRAY *fft_buffer, CSN_ARRAY *source_arr, MYFLT *temp_buffer, uint32_t nfft, size_t work_size, size_t out_size, uint32_t axis, CSN_FFT_MODE mode) {
    uint32_t *source_shape = source_arr->shape;
    size_t rows = source_shape[0];
    size_t cols = source_shape[1];
    size_t slice_count = axis == 0U ? cols : rows;
    size_t destination_slices = axis == 0U ? fft_buffer->shape[1] : fft_buffer->shape[0];
    if (slice_count > destination_slices) slice_count = destination_slices;

    uint32_t axis_size = source_shape[axis];
    uint32_t nfft_copy = nfft;
    if (mode == CSNIFFT) {
        nfft_copy = axis_size < nfft ? axis_size : nfft;
    } else {
        uint32_t spectrum_size = nfft / 2U + 1U;
        nfft_copy = axis_size < spectrum_size ? axis_size : spectrum_size;
    }

    size_t src_stride = source_arr->strides[axis];
    size_t dst_stride = fft_buffer->strides[axis];
    for (size_t s = 0; s < slice_count; ++s) {
        memset(temp_buffer, 0, sizeof(MYFLT) * work_size);

        size_t src_base = axis == 0U ? s * source_arr->strides[1] : s * source_arr->strides[0];
        size_t dst_base = axis == 0U ? s * fft_buffer->strides[1] : s * fft_buffer->strides[0];
        for (uint32_t i = 0; i < nfft_copy; i++) {
            CSN_COMPLEXDAT z = slice_get(source_arr->data + src_base * source_arr->itype, i, src_stride, source_arr->itype);
            if (mode == CSNIRFFT) {
                if (i == 0) {
                    temp_buffer[0] = (MYFLT) z.re;
                } else if (i == (nfft / 2U)) {
                    temp_buffer[1] = (MYFLT) z.re;
                } else {
                    temp_buffer[i * 2] = (MYFLT) z.re;
                    temp_buffer[i * 2 + 1] = (MYFLT) z.im;
                }
            } else {
                temp_buffer[i * 2] = (MYFLT) z.re;
                temp_buffer[i * 2 + 1] = (MYFLT) z.im;
            }
        }

        if (mode == CSNIRFFT) {
            csound->RealFFT(csound, fft_setup, temp_buffer);
        } else {
            csound->InverseComplexFFT(csound, temp_buffer, (int32_t) nfft);
        }

        if (mode == CSNIRFFT) {
            double *dst = fft_buffer->data + dst_base * CSN_REAL;
            for (uint32_t i = 0; i < out_size; ++i) {
                CSN_COMPLEXDAT y = {
                    .re = (double) temp_buffer[i],
                    .im = 0.0
                };
                slice_put(dst, i, dst_stride, CSN_REAL, y);
            }
        } else {
            double *dst = fft_buffer->data + dst_base * CSN_COMPLEX;
            for (uint32_t i = 0; i < out_size; ++i) {
                CSN_COMPLEXDAT y = {
                    .re = (double) temp_buffer[2U * i],
                    .im = (double) temp_buffer[2U * i + 1U]
                };
                slice_put(dst, i, dst_stride, CSN_COMPLEX, y);
            }
        }
    }
}

static int32_t csnarray_ifft2_helper(CSOUND *csound, CSN_FFT2 *p, CSN_FFT_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;
    int32_t res = OK;
    const char *err = NULL;
    MYFLT *row_temp_buffer = NULL;
    MYFLT *col_temp_buffer = NULL;
    CSN_ARRAY intermediate = {0};

    double rows_fftsize_temp = (double) *p->rows_fft_size;
    if (!IS_VALID_FFT_SIZE(rows_fftsize_temp) || !IS_POWER_OF_TWO((uint32_t) rows_fftsize_temp)) {
        return csound->InitError(csound, "[csnarray] FFT size must be a valid power of two value");
    }
    int32_t rows_fft_size = (int32_t) rows_fftsize_temp;

    double cols_fftsize_temp = (double) *p->cols_fft_size;
    if (!IS_VALID_FFT_SIZE(cols_fftsize_temp) || !IS_POWER_OF_TWO((uint32_t) cols_fftsize_temp)) {
        return csound->InitError(csound, "[csnarray] FFT size must be a valid power of two value");
    }
    int32_t cols_fft_size = (int32_t) cols_fftsize_temp;

    void *cols_fft_setup = NULL;

    csound->LockMutex(reg->mutex);
    CSN_ARRAY *source_arr = NULL;
    res = fft2_body(csound, NULL, reg, &source_arr, source_handle, mode);
    if (res != OK) goto done;

    if (mode == CSNIRFFT) {
        cols_fft_setup = csound->RealFFTSetup(csound, cols_fft_size, FFT_INV);
    }

    size_t rows_out_size = 0;
    size_t rows_work_size = 0;
    size_t cols_out_size = 0;
    size_t cols_work_size = 0;
    fft2_assign_size(&rows_out_size, &rows_work_size, (size_t) rows_fft_size, CSNIFFT);
    fft2_assign_size(&cols_out_size, &cols_work_size, (size_t) cols_fft_size, mode);
    uint32_t new_ndim = 2U;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    new_shape[0] = (uint32_t) rows_out_size;
    new_shape[1] = (uint32_t) cols_out_size;
    size_t output_size = 0;
    if (get_array_size_from_shape(&output_size, new_ndim, new_shape) != OK) {
        res = csound->InitError(csound, "[csnarray] FFT2 output exceeds the maximum element count");
        goto done;
    }

    uint32_t intermediate_shape[CSN_MAX_DIMS] = {0};
    intermediate_shape[0] = (uint32_t) rows_out_size;
    intermediate_shape[1] = mode == CSNIRFFT
        ? (uint32_t) (cols_fft_size / 2 + 1)
        : (uint32_t) cols_out_size;
    res = fft2_init_intermediate(csound, &intermediate, intermediate_shape);
    if (res != OK) goto done;

    row_temp_buffer = csound->Calloc(csound, sizeof(MYFLT) * rows_work_size);
    if (row_temp_buffer == NULL) {
        res = csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
        goto done;
    }
    col_temp_buffer = csound->Calloc(csound, sizeof(MYFLT) * cols_work_size);
    if (col_temp_buffer == NULL) {
        res = csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
        goto done;
    }

    ITEM_TYPE otype = mode == CSNIRFFT ? CSN_REAL : CSN_COMPLEX;
    if (create_csnarray_locked(csound, reg, &p->h, new_ndim, new_shape, &p->array, p->handle, &source_handle, 1U, &err, otype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *fft_buffer = p->array;
    ifft2_assign_value(csound, NULL, &intermediate, source_arr, row_temp_buffer, (uint32_t) rows_fft_size, rows_work_size, rows_out_size, 0U, CSNIFFT);
    ifft2_assign_value(csound, cols_fft_setup, fft_buffer, &intermediate, col_temp_buffer, (uint32_t) cols_fft_size, cols_work_size, cols_out_size, 1U, mode);

    SET_KDATA_BEGIN(p, reg);
    set_array_version(&p->k_data.prev_output_version, &fft_buffer->version);
    set_array_version(&p->k_data.prev_source_version, &source_arr->version);
    p->k_data_row_fft.nfft = (size_t) rows_fft_size;
    p->k_data_row_fft.buffer_out_size = rows_out_size;
    p->k_data_row_fft.buffer_work_size = rows_work_size;
    p->k_data_row_fft.fft_setup = NULL;
    p->k_data_col_fft.nfft = (size_t) cols_fft_size;
    p->k_data_col_fft.buffer_out_size = cols_out_size;
    p->k_data_col_fft.buffer_work_size = cols_work_size;
    p->k_data_col_fft.fft_setup = cols_fft_setup;
    p->intermediate = intermediate;
    p->row_buffer.scratch = row_temp_buffer;
    p->row_buffer.scratch_capacity = rows_work_size;
    p->col_buffer.scratch = col_temp_buffer;
    p->col_buffer.scratch_capacity = cols_work_size;
    p->is_published = false;

done:
    if (res != OK) {
        if (row_temp_buffer != NULL) csound->Free(csound, row_temp_buffer);
        if (col_temp_buffer != NULL) csound->Free(csound, col_temp_buffer);
        if (intermediate.data != NULL) csound->Free(csound, intermediate.data);
    }
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_fft2_deinit(CSOUND *csound, CSN_FFT2 *p) {
    deinit_scratch(csound, &p->row_buffer);
    deinit_scratch(csound, &p->col_buffer);
    if (p->intermediate.data != NULL) csound->Free(csound, p->intermediate.data);
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

int32_t csnarray_ifft2(CSOUND *csound, CSN_FFT2 *p) {
    return csnarray_ifft2_helper(csound, p, CSNIFFT);
}

int32_t csnarray_irfft2(CSOUND *csound, CSN_FFT2 *p) {
    return csnarray_ifft2_helper(csound, p, CSNIRFFT);
}

static int32_t csnarray_fft2_k_helper(CSOUND *csound, CSN_FFT2 *p, CSN_FFT_MODE mode) {
    CSN_REGISTRY *reg = p->k_data.registry;
    uint32_t owned_handle = p->k_data.owned_handle;
    CHECK_REG_HANDLE(csound, &p->h, reg, owned_handle);

    int32_t res = OK;
    CHECK_KTRIG(p->trig);

    uint32_t source_handle = p->source_handle->id;
    size_t rows_fft_size = p->k_data_row_fft.nfft;
    size_t cols_fft_size = p->k_data_col_fft.nfft;
    void *cols_fft_setup = p->k_data_col_fft.fft_setup;
    size_t rows_out_size = p->k_data_row_fft.buffer_out_size;
    size_t rows_work_size = p->k_data_row_fft.buffer_work_size;
    size_t cols_out_size = p->k_data_col_fft.buffer_out_size;
    size_t cols_work_size = p->k_data_col_fft.buffer_work_size;

    csound->LockMutex(reg->mutex);
    CSN_ARRAY *source_arr = NULL;
    res = fft2_body(csound, &p->h, reg, &source_arr, source_handle, mode);
    if (res != OK) goto done;

    if (p->is_published) {
        bool is_same_source = is_same_array_version(&p->k_data.prev_source_version, &source_arr->version);
        bool is_same_result = false;
        CSN_SLOT *res_slot = get_slot(reg, owned_handle);
        if (res_slot != NULL) {
            is_same_result = is_same_array_version(&p->k_data.prev_output_version, &res_slot->array->version);
        }

        if (is_same_source && is_same_result) {
            p->handle->id = owned_handle;
            goto done;
        }
    }

    uint32_t new_shape[CSN_MAX_DIMS] = {
        (uint32_t) rows_out_size,
        (uint32_t) cols_out_size
    };
    size_t output_size = 0;
    if (get_array_size_from_shape(&output_size, 2U, new_shape) != OK) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] FFT2 output exceeds the maximum element count");
        goto done;
    }

    CSN_ARRAY *fft_buffer = NULL;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &fft_buffer, &p->k_data, NULL, 2U, new_shape, output_size, CSN_COMPLEX, NULL);
    if (res != OK) goto done;
    p->array = fft_buffer;

    MYFLT *col_temp_buffer = (MYFLT *) p->col_buffer.scratch;
    MYFLT *row_temp_buffer = (MYFLT *) p->row_buffer.scratch;
    memset(p->intermediate.data, 0, sizeof(double) * p->intermediate.size * (size_t) CSN_COMPLEX);
    fft2_assign_value(csound, cols_fft_setup, &p->intermediate, source_arr, col_temp_buffer, (uint32_t) cols_fft_size, cols_work_size, cols_out_size, 1U, mode);
    fft2_assign_value(csound, NULL, fft_buffer, &p->intermediate, row_temp_buffer, (uint32_t) rows_fft_size, rows_work_size, rows_out_size, 0U, CSNFFT);

    SET_KDATA_END(p, fft_buffer->shape, fft_buffer->ndim, fft_buffer->itype);
    set_array_version(&p->k_data.prev_output_version, &fft_buffer->version);
    set_array_version(&p->k_data.prev_source_version, &source_arr->version);
    p->is_published = true;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_ifft2_k_helper(CSOUND *csound, CSN_FFT2 *p, CSN_FFT_MODE mode) {
    CSN_REGISTRY *reg = p->k_data.registry;
    uint32_t owned_handle = p->k_data.owned_handle;
    CHECK_REG_HANDLE(csound, &p->h, reg, owned_handle);

    int32_t res = OK;
    CHECK_KTRIG(p->trig);

    uint32_t source_handle = p->source_handle->id;
    size_t rows_fft_size = p->k_data_row_fft.nfft;
    size_t cols_fft_size = p->k_data_col_fft.nfft;
    void *cols_fft_setup = p->k_data_col_fft.fft_setup;
    size_t rows_out_size = p->k_data_row_fft.buffer_out_size;
    size_t rows_work_size = p->k_data_row_fft.buffer_work_size;
    size_t cols_out_size = p->k_data_col_fft.buffer_out_size;
    size_t cols_work_size = p->k_data_col_fft.buffer_work_size;

    csound->LockMutex(reg->mutex);
    CSN_ARRAY *source_arr = NULL;
    res = fft2_body(csound, &p->h, reg, &source_arr, source_handle, mode);
    if (res != OK) goto done;

    if (p->is_published) {
        bool is_same_source = is_same_array_version(&p->k_data.prev_source_version, &source_arr->version);
        bool is_same_result = false;
        CSN_SLOT *res_slot = get_slot(reg, owned_handle);
        if (res_slot != NULL) {
            is_same_result = is_same_array_version(&p->k_data.prev_output_version, &res_slot->array->version);
        }

        if (is_same_source && is_same_result) {
            p->handle->id = owned_handle;
            goto done;
        }
    }

    uint32_t new_shape[CSN_MAX_DIMS] = {
        (uint32_t) rows_out_size,
        (uint32_t) cols_out_size
    };
    size_t output_size = 0;
    if (get_array_size_from_shape(&output_size, 2U, new_shape) != OK) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] inverse FFT2 output exceeds the maximum element count");
        goto done;
    }

    ITEM_TYPE otype = mode == CSNIRFFT ? CSN_REAL : CSN_COMPLEX;
    CSN_ARRAY *fft_buffer = NULL;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &fft_buffer, &p->k_data, NULL, 2U, new_shape, output_size, otype, NULL);
    if (res != OK) goto done;
    p->array = fft_buffer;

    MYFLT *col_temp_buffer = (MYFLT *) p->col_buffer.scratch;
    MYFLT *row_temp_buffer = (MYFLT *) p->row_buffer.scratch;
    memset(p->intermediate.data, 0, sizeof(double) * p->intermediate.size * (size_t) CSN_COMPLEX);
    ifft2_assign_value(csound, NULL, &p->intermediate, source_arr, row_temp_buffer, (uint32_t) rows_fft_size, rows_work_size, rows_out_size, 0U, CSNIFFT);
    ifft2_assign_value(csound, cols_fft_setup, fft_buffer, &p->intermediate, col_temp_buffer, (uint32_t) cols_fft_size, cols_work_size, cols_out_size, 1U, mode);

    SET_KDATA_END(p, fft_buffer->shape, fft_buffer->ndim, fft_buffer->itype);
    set_array_version(&p->k_data.prev_output_version, &fft_buffer->version);
    set_array_version(&p->k_data.prev_source_version, &source_arr->version);
    p->is_published = true;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_fft2_k(CSOUND *csound, CSN_FFT2 *p) {
    return csnarray_fft2_k_helper(csound, p, CSNFFT);
}

int32_t csnarray_rfft2_k(CSOUND *csound, CSN_FFT2 *p) {
    return csnarray_fft2_k_helper(csound, p, CSNRFFT);
}

int32_t csnarray_ifft2_k(CSOUND *csound, CSN_FFT2 *p) {
    return csnarray_ifft2_k_helper(csound, p, CSNIFFT);
}

int32_t csnarray_irfft2_k(CSOUND *csound, CSN_FFT2 *p) {
    return csnarray_ifft2_k_helper(csound, p, CSNIRFFT);
}


// CONVOLVE AND CORRELATE

static int32_t IS_VALID_EDGES(double value) {
    return isfinite(value) && !isnan(value) && trunc(value) == value && value >= 0.0 && value < (double) NUMBER_OF_EDGES_MODE;
}

static void corrconv_get_size_and_edges_offset(size_t *size_result, int64_t *start_offset, CSN_ARRAY *x, CSN_ARRAY *h, int32_t axis, CSN_EDGES_MODE mode) {
    switch (mode) {
        case EDGES_FULL:
            *start_offset = 0;
            *size_result = axis == -1 ? x->size + h->size - 1 : x->shape[axis] + h->size - 1;
            break;
        case EDGES_SAME:
            *start_offset = (int64_t) ((h->size - 1) / 2);
            *size_result = axis == -1 ? x->size : x->shape[axis];
            break;
        case EDGES_VALID:
            *start_offset = (int64_t) (h->size - 1);
            *size_result = axis == -1 ? x->size - h->size + 1 : x->shape[axis] - h->size + 1;
            break;
    }
}

static void fftcorrconv_get_edges_offset(int64_t *start_offset, size_t h_size, size_t fft_size, CSN_EDGES_MODE edges_mode, CSN_CORRCONV_MODE corrconv_mode) {
    int64_t offset;
    switch (edges_mode) {
        case EDGES_FULL:
            offset = 0;
            break;
        case EDGES_SAME:
            offset = (int64_t) ((h_size - 1) / 2);
            break;
        case EDGES_VALID:
            offset = (int64_t) (h_size - 1);
            break;
    }

    *start_offset = corrconv_mode == CSN_CONVOLUTION ? offset : (int64_t) (((size_t) offset + fft_size - (h_size - 1)) % fft_size);
}

static void corrconv_get_shape_and_edges_offset_ndims(uint32_t *new_shape, int64_t *start_offset, CSN_ARRAY *x, CSN_ARRAY *h, CSN_EDGES_MODE mode) {
    uint32_t ndim = x->ndim;
    for (uint32_t axis = 0; axis < ndim; axis++) {
        switch (mode) {
            case EDGES_FULL:
                start_offset[axis] = 0;
                new_shape[axis] = x->shape[axis] + h->shape[axis] - 1;
                break;
            case EDGES_SAME:
                start_offset[axis] = (int64_t) ((h->shape[axis] - 1) / 2);
                new_shape[axis] = x->shape[axis];
                break;
            case EDGES_VALID:
                start_offset[axis] = (int64_t) (h->shape[axis] - 1);
                new_shape[axis] = x->shape[axis] - h->shape[axis] + 1;
                break;
        }
    }
}

static void fftcorrconv_get_shape_and_edges_offset_ndims(uint32_t *new_shape, int64_t *start_offset, CSN_ARRAY *x, CSN_ARRAY *h, CSN_EDGES_MODE edges_mode, CSN_CORRCONV_MODE corrconv_mode) {
    uint32_t ndim = x->ndim;
    for (uint32_t i = 0; i < ndim; i++) {
        size_t fft_size_check = x->shape[i] + h->shape[i] - 1;
        size_t fft_size = IS_POWER_OF_TWO(fft_size_check) ? fft_size_check : NEXT_POWER_OF_TWO(fft_size_check);
        new_shape[i] = fft_size;
    }

    for (uint32_t axis = 0; axis < ndim; axis++) {
        int64_t offset = 0;
        switch (edges_mode) {
            case EDGES_FULL:
                offset = 0;
                break;
            case EDGES_SAME:
                offset = (int64_t) ((h->shape[axis] - 1) / 2);
                break;
            case EDGES_VALID:
                offset = (int64_t) (h->shape[axis] - 1);
                break;
        }
        start_offset[axis] = corrconv_mode == CSN_CONVOLUTION ? offset : (int64_t) (((size_t) offset + new_shape[axis] - (h->shape[axis] - 1)) % new_shape[axis]);
    }
}

/* Everything the operands have to satisfy for the 1-D forms, in one place: the
   init pass calls it with a NULL perf handle, the performance pass with its
   own. Neither the axis nor the edges mode can move between the two, they are
   i-rate, but the arrays behind the handles can: a k-rate producer may drop the
   rank the axis was chosen for, shrink the source under the kernel, or grow the
   kernel past the source. Re-checking every pass costs a handful of
   comparisons and keeps the two paths from drifting apart. */
static int32_t corrconv1d_validate(CSOUND *csound, OPDS *perf_h, CSN_ARRAY *x, CSN_ARRAY *h, int32_t axis, uint32_t edges_mode) {
    if (h->ndim != 1U) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] convolve1d and correlate1d requires 1-D kernel");
    }

    if (axis != -1 && (uint32_t) axis >= x->ndim) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Axis %d is invalid for a %u-D array (valid axes: -1 for all axes (flatten), or finite integers 0..%u)", axis, x->ndim, x->ndim - 1);
    }

    /* A kernel with no taps is not an identity, it is an operation with nothing
       to apply: SAME and VALID would both read their offset from h->size - 1
       and wrap it, and VALID would answer longer than it was asked. */
    if (h->size == 0) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Kernel is empty: convolve1d and correlate1d need at least one tap");
    }

    size_t x_length = axis == -1 ? x->size : (size_t) x->shape[axis];
    if (edges_mode == EDGES_VALID && x_length < h->size) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] VALID edges requires x size to be at least the kernel size");
    }

    return OK;
}

/* The same contract for the N-D forms, where the kernel is shaped like the
   source rather than laid along one axis. */
static int32_t corrconv_validate(CSOUND *csound, OPDS *perf_h, CSN_ARRAY *x, CSN_ARRAY *h) {
    if (h->ndim != x->ndim) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] convolve and correlate requires arrays with same dimension");
    }

    if (h->size == 0) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Kernel is empty: convolve and correlate need at least one tap");
    }

    for (uint32_t i = 0; i < x->ndim; i++) {
        if (x->shape[i] < h->shape[i]) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] convolve and correlate N-D requires every x axis length to be at least the kernel axis length");
        }
    }

    return OK;
}

static void corrconv_loop(CSN_ARRAY *y, CSN_ARRAY *x, CSN_ARRAY *h, size_t src_base, size_t dst_base, size_t src_stride, size_t dst_stride, uint32_t out_size, int64_t start_offset, int32_t axis, CSN_CORRCONV_MODE mode) {
    int64_t x_length = axis == -1 ? x->size : x->shape[axis];
    for (uint32_t i = 0; i < out_size; i++) {
        // direct -> output-side
        CSN_COMPLEXDAT y_value = { .re = 0.0, .im = 0.0 };
        for (uint32_t j = 0; j < h->size; j++) {
            int64_t src_index = (int64_t) i + start_offset - (int64_t) j;
            if (src_index >= 0 && src_index < x_length) {
                size_t kernel_index = mode == CSN_CONVOLUTION ? (size_t) j : h->size - 1 - (size_t) j;
                CSN_COMPLEXDAT x_value = slice_get(x->data + src_base * x->itype, (size_t) src_index, src_stride, x->itype);
                CSN_COMPLEXDAT h_value = slice_get(h->data, kernel_index, 1U, h->itype);
                h_value.im = mode == CSN_CONVOLUTION ? h_value.im : -h_value.im;
                if (y->itype == CSN_COMPLEX) {
                    CSN_COMPLEXDAT temp = { .re = 0.0, .im = 0.0 };
                    complex_prod(&temp, x_value, h_value);
                    complex_add(&y_value, y_value, temp);
                } else {
                    y_value.re += x_value.re * h_value.re;
                }
            }
        }
        slice_put(y->data + dst_base * y->itype, i, dst_stride, y->itype, y_value);
    }
}

static void corrconv1d_assig_value(CSN_ARRAY *y, CSN_ARRAY *x, CSN_ARRAY *h, uint32_t out_size, int64_t start_offset, int32_t axis, CSN_CORRCONV_MODE mode) {
    if (axis == -1) {
        corrconv_loop(y, x, h, 0, 0, 1U, 1U, out_size, start_offset, axis, mode);
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
    size_t dst_stride = y->strides[axis];
    for (size_t linear = 0; linear < slice_count; ++linear) {
        uint32_t dst_coords[CSN_MAX_DIMS] = {0};
        uint32_t src_coords[CSN_MAX_DIMS] = {0};

        from_linear_to_coords(dst_coords, reduced_shape, linear, reduced_ndim);
        for (uint32_t i = 0, j = 0; i < x->ndim; ++i) {
            src_coords[i] = (i == (uint32_t) axis) ? 0 : dst_coords[j++];
        }

        size_t src_base = from_coords_to_offset(src_coords, x->strides, x->ndim);
        size_t dst_base = from_coords_to_offset(src_coords, y->strides, y->ndim);
        corrconv_loop(y, x, h, src_base, dst_base, src_stride, dst_stride, out_size, start_offset, axis, mode);
    }
}

static void corrconv_assig_value(CSN_ARRAY *y, CSN_ARRAY *x, CSN_ARRAY *h, int64_t *start_offset, CSN_CORRCONV_MODE mode) {
    uint32_t dst_coords[CSN_MAX_DIMS] = {0};
    uint32_t knl_coords[CSN_MAX_DIMS] = {0};
    uint32_t src_coords[CSN_MAX_DIMS] = {0};
    for (size_t linear = 0; linear < y->size; ++linear) {
        from_linear_to_coords(dst_coords, y->shape, linear, y->ndim);
        CSN_COMPLEXDAT y_value = { .re = 0.0, .im = 0.0 };
        for (size_t i = 0; i < h->size; i++) {
            from_linear_to_coords(knl_coords, h->shape, i, h->ndim);
            bool valid = true;
            for (uint32_t d = 0; d < h->ndim; d++) {
                int64_t coord = (int64_t) dst_coords[d] + start_offset[d] - (int64_t) knl_coords[d];
                if (coord < 0 || coord >= (int64_t) x->shape[d]) {
                    valid = false;
                    break;
                }
                src_coords[d] = (uint32_t) coord;
            }

            if (!valid) continue;

            size_t src_offset = from_coords_to_offset(src_coords, x->strides, x->ndim);
            size_t knl_offset = i;

            size_t knl_compute = mode == CSN_CONVOLUTION ? (size_t) knl_offset : h->size - 1 - knl_offset;
            CSN_COMPLEXDAT x_value = slice_get(x->data, src_offset, 1U, x->itype);
            CSN_COMPLEXDAT h_value = slice_get(h->data, knl_compute, 1U, h->itype);
            h_value.im = mode == CSN_CONVOLUTION ? h_value.im : -h_value.im;
            if (y->itype == CSN_COMPLEX) {
                CSN_COMPLEXDAT temp = { .re = 0.0, .im = 0.0 };
                complex_prod(&temp, x_value, h_value);
                complex_add(&y_value, y_value, temp);
            } else {
                y_value.re += x_value.re * h_value.re;
            }
        }
        slice_put(y->data, linear, 1U, y->itype, y_value);
    }
}

static int32_t csnarray_corrconv1d_helper(CSOUND *csound, CSN_CORRCONV *p, CSN_CORRCONV_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle_a = p->source_handle_a->id;
    uint32_t source_handle_b = p->source_handle_b->id;

    double edges_temp = (double) *p->arg_a;
    double axis_value = (double) *p->arg_b;

    if (!IS_VALID_EDGES(edges_temp)) {
        return csound->InitError(csound, "[csnarray] Invalid edges mode: should be 0, 1, or 2 (see documentation)");
    }
    uint32_t edges_mode = (uint32_t) edges_temp;

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
    uint32_t source_ndim_a = source_arr_a->ndim;
    uint32_t *source_shape_a = source_arr_a->shape;

    if (axis_value != -1.0 && !IS_VALID_AXIS(axis_value, source_ndim_a)) {
        res = csound->InitError(csound, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: -1 for all axes (flatten), or finite integers 0..%u)", axis_value, source_ndim_a, source_ndim_a - 1);
        goto done;
    }
    int32_t axis = (int32_t) axis_value;

    res = corrconv1d_validate(csound, NULL, source_arr_a, source_arr_b, axis, edges_mode);
    if (res != OK) goto done;

    size_t size_result = 0;
    int64_t start_offset = 0;
    corrconv_get_size_and_edges_offset(&size_result, &start_offset, source_arr_a, source_arr_b, axis, edges_mode);

    uint32_t new_ndim = axis == -1 ? 1U : source_ndim_a;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    if (axis == -1) {
        new_shape[0] = size_result;
    } else {
        memcpy(new_shape, source_shape_a, sizeof(uint32_t) * CSN_MAX_DIMS);
        new_shape[axis] = size_result;
    }

    ITEM_TYPE itype = source_arr_a->itype == CSN_COMPLEX || source_arr_b->itype == CSN_COMPLEX ? CSN_COMPLEX : CSN_REAL;
    uint32_t protect[2] = { source_handle_a, source_handle_b };
    if (create_csnarray_locked(csound, reg, &p->h, new_ndim, new_shape, &p->array, p->handle, protect, 2U, &err, itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    corrconv1d_assig_value(p->array, source_arr_a, source_arr_b, size_result, start_offset, axis, mode);

    SET_KDATA_BEGIN(p, reg);
    set_array_version(&p->k_data.prev_output_version, &p->array->version);
    p->k_data.prev_axis_i = axis;
    p->k_data.prev_index = edges_mode;
    p->is_published = false;

done:
    csound->UnlockMutex(reg->mutex);
    return res;

}

int32_t csnarray_corrconv_deinit(CSOUND *csound, CSN_CORRCONV *p) {
     deinit_scratch(csound, &p->buffer_x);
     deinit_scratch(csound, &p->buffer_h);
     deinit_scratch(csound, &p->buffer_ifft);
     FREE_CSNARRDATA(csound, &p->fft_buffer_x);
     FREE_CSNARRDATA(csound, &p->fft_buffer_h);
     FREE_CSNARRDATA(csound, &p->ifft_buffer);
     FREE_CSNARRDATA(csound, &p->ifft_out);
     FREE_CSNARRDATA(csound, &p->x_padded);
     FREE_CSNARRDATA(csound, &p->h_padded);
     return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

int32_t csnarray_convolve1d(CSOUND *csound, CSN_CORRCONV *p) {
    return csnarray_corrconv1d_helper(csound, p, CSN_CONVOLUTION);
}

int32_t csnarray_correlate1d(CSOUND *csound, CSN_CORRCONV *p) {
    return csnarray_corrconv1d_helper(csound, p, CSN_CORRELATION);
}

static int32_t csnarray_corrconv1d_k_helper(CSOUND *csound, CSN_CORRCONV *p, CSN_CORRCONV_MODE mode) {
    CSN_REGISTRY *reg = p->k_data.registry;
    uint32_t owned_handle = p->k_data.owned_handle;
    CHECK_REG_HANDLE(csound, &p->h, reg, owned_handle);

    uint32_t source_handle_a = p->source_handle_a->id;
    uint32_t source_handle_b = p->source_handle_b->id;

    uint32_t edges_mode = p->k_data.prev_index;
    int32_t axis = p->k_data.prev_axis_i;

    int32_t res = OK;
    const char *err = NULL;

    res = CHECK_SELF_ALIAS(csound, &p->h, &p->k_data, source_handle_a, source_handle_b);
    if (res != OK) return res;

    CHECK_KTRIG(p->arg_c);

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot_a = get_slot(reg, source_handle_a);
    if (slot_a == NULL) {
        res = CSN_ACCESSOR_ERROR_LOCKED(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle_a);
        goto done;
    }
    CSN_SLOT *slot_b = get_slot(reg, source_handle_b);
    if (slot_b == NULL) {
        res = CSN_ACCESSOR_ERROR_LOCKED(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle_b);
        goto done;
    }

    CSN_ARRAY *source_arr_a = slot_a->array;
    CSN_ARRAY *source_arr_b = slot_b->array;
    uint32_t source_ndim_a = source_arr_a->ndim;
    uint32_t *source_shape_a = source_arr_a->shape;

    res = corrconv1d_validate(csound, &p->h, source_arr_a, source_arr_b, axis, edges_mode);
    if (res != OK) goto done;

    if (p->is_published) {
        bool is_same_x = is_same_array_version(&p->k_data.prev_source_version, &source_arr_a->version);
        bool is_same_h = is_same_array_version(&p->k_data.prev_source_version_b, &source_arr_b->version);
        bool is_same_y = false;
        CSN_SLOT *slot_y = get_slot(reg, owned_handle);
        if (slot_y != NULL) {
            is_same_y = is_same_array_version(&p->k_data.prev_output_version, &slot_y->array->version);
        }

        if (is_same_x && is_same_h && is_same_y) {
            p->handle->id = owned_handle;
            goto done;
        }
    }

    size_t size_result = 0;
    int64_t start_offset = 0;
    corrconv_get_size_and_edges_offset(&size_result, &start_offset, source_arr_a, source_arr_b, axis, edges_mode);

    uint32_t new_ndim = axis == -1 ? 1U : source_ndim_a;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    if (axis == -1) {
        new_shape[0] = size_result;
    } else {
        memcpy(new_shape, source_shape_a, sizeof(uint32_t) * CSN_MAX_DIMS);
        new_shape[axis] = size_result;
    }

    size_t req_size = 0;
    if (get_array_size_from_shape(&req_size, new_ndim, new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSN_ARRAY *arr = NULL;
    size_t logical_size = (source_arr_a->size == 0 || source_arr_b->size == 0) ? 0 : req_size;
    ITEM_TYPE itype = source_arr_a->itype == CSN_COMPLEX || source_arr_b->itype == CSN_COMPLEX ? CSN_COMPLEX : CSN_REAL;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, new_ndim, new_shape, logical_size, itype, err);
    if (res != OK) goto done;
    p->array = arr;

    corrconv1d_assig_value(p->array, source_arr_a, source_arr_b, size_result, start_offset, axis, mode);
    SET_KDATA_END(p, new_shape, new_ndim, itype);
    set_array_version(&p->k_data.prev_output_version, &p->array->version);
    set_array_version(&p->k_data.prev_source_version, &source_arr_a->version);
    set_array_version(&p->k_data.prev_source_version_b, &source_arr_b->version);
    p->is_published = true;

done:
    csound->UnlockMutex(reg->mutex);
    return res;

}

int32_t csnarray_convolve1d_k(CSOUND *csound, CSN_CORRCONV *p) {
    return csnarray_corrconv1d_k_helper(csound, p, CSN_CONVOLUTION);
}

int32_t csnarray_correlate1d_k(CSOUND *csound, CSN_CORRCONV *p) {
    return csnarray_corrconv1d_k_helper(csound, p, CSN_CORRELATION);
}

static int32_t csnarray_corrconv_helper(CSOUND *csound, CSN_CORRCONV *p, CSN_CORRCONV_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle_a = p->source_handle_a->id;
    uint32_t source_handle_b = p->source_handle_b->id;

    double edges_temp = (double) *p->arg_a;

    if (!IS_VALID_EDGES(edges_temp)) {
        return csound->InitError(csound, "[csnarray] Invalid edges mode: should be 0, 1, or 2 (see documentation)");
    }
    uint32_t edges_mode = (uint32_t) edges_temp;

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

    res = corrconv_validate(csound, NULL, source_arr_a, source_arr_b);
    if (res != OK) goto done;

    uint32_t new_ndim = source_arr_a->ndim;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    int64_t start_offset[CSN_MAX_DIMS] = {0};
    corrconv_get_shape_and_edges_offset_ndims(new_shape, start_offset, source_arr_a, source_arr_b, edges_mode);

    ITEM_TYPE itype = source_arr_a->itype == CSN_COMPLEX || source_arr_b->itype == CSN_COMPLEX ? CSN_COMPLEX : CSN_REAL;
    uint32_t protect[2] = { source_handle_a, source_handle_b };
    if (create_csnarray_locked(csound, reg, &p->h, new_ndim, new_shape, &p->array, p->handle, protect, 2U, &err, itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    corrconv_assig_value(p->array, source_arr_a, source_arr_b, start_offset, mode);

    SET_KDATA_BEGIN(p, reg);
    set_array_version(&p->k_data.prev_output_version, &p->array->version);
    p->k_data.prev_index = edges_mode;
    p->is_published = false;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_convolve(CSOUND *csound, CSN_CORRCONV *p) {
    return csnarray_corrconv_helper(csound, p, CSN_CONVOLUTION);
}

int32_t csnarray_correlate(CSOUND *csound, CSN_CORRCONV *p) {
    return csnarray_corrconv_helper(csound, p, CSN_CORRELATION);
}

static int32_t csnarray_corrconv_k_helper(CSOUND *csound, CSN_CORRCONV *p, CSN_CORRCONV_MODE mode) {
    CSN_REGISTRY *reg = p->k_data.registry;
    uint32_t owned_handle = p->k_data.owned_handle;
    CHECK_REG_HANDLE(csound, &p->h, reg, owned_handle);

    uint32_t source_handle_a = p->source_handle_a->id;
    uint32_t source_handle_b = p->source_handle_b->id;

    uint32_t edges_mode = p->k_data.prev_index;

    int32_t res = OK;
    const char *err = NULL;

    res = CHECK_SELF_ALIAS(csound, &p->h, &p->k_data, source_handle_a, source_handle_b);
    if (res != OK) return res;

    CHECK_KTRIG(p->arg_b);

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot_a = get_slot(reg, source_handle_a);
    if (slot_a == NULL) {
        res = CSN_ACCESSOR_ERROR_LOCKED(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle_a);
        goto done;
    }
    CSN_SLOT *slot_b = get_slot(reg, source_handle_b);
    if (slot_b == NULL) {
        res = CSN_ACCESSOR_ERROR_LOCKED(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle_b);
        goto done;
    }

    CSN_ARRAY *source_arr_a = slot_a->array;
    CSN_ARRAY *source_arr_b = slot_b->array;

    res = corrconv_validate(csound, &p->h, source_arr_a, source_arr_b);
    if (res != OK) goto done;

    if (p->is_published) {
        bool is_same_x = is_same_array_version(&p->k_data.prev_source_version, &source_arr_a->version);
        bool is_same_h = is_same_array_version(&p->k_data.prev_source_version_b, &source_arr_b->version);
        bool is_same_y = false;
        CSN_SLOT *slot_y = get_slot(reg, owned_handle);
        if (slot_y != NULL) {
            is_same_y = is_same_array_version(&p->k_data.prev_output_version, &slot_y->array->version);
        }

        if (is_same_x && is_same_h && is_same_y) {
            p->handle->id = owned_handle;
            goto done;
        }
    }

    uint32_t new_ndim = source_arr_a->ndim;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    int64_t start_offset[CSN_MAX_DIMS] = {0};
    corrconv_get_shape_and_edges_offset_ndims(new_shape, start_offset, source_arr_a, source_arr_b, edges_mode);

    ITEM_TYPE itype = source_arr_a->itype == CSN_COMPLEX || source_arr_b->itype == CSN_COMPLEX ? CSN_COMPLEX : CSN_REAL;
    size_t req_size = 0;
    if (get_array_size_from_shape(&req_size, new_ndim, new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSN_ARRAY *arr = NULL;
    size_t logical_size = (source_arr_a->size == 0 || source_arr_b->size == 0) ? 0 : req_size;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, new_ndim, new_shape, logical_size, itype, err);
    if (res != OK) goto done;
    p->array = arr;

    corrconv_assig_value(p->array, source_arr_a, source_arr_b, start_offset, mode);
    SET_KDATA_END(p, new_shape, new_ndim, itype);
    set_array_version(&p->k_data.prev_output_version, &p->array->version);
    set_array_version(&p->k_data.prev_source_version, &source_arr_a->version);
    set_array_version(&p->k_data.prev_source_version_b, &source_arr_b->version);
    p->is_published = true;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_convolve_k(CSOUND *csound, CSN_CORRCONV *p) {
    return csnarray_corrconv_k_helper(csound, p, CSN_CONVOLUTION);
}

int32_t csnarray_correlate_k(CSOUND *csound, CSN_CORRCONV *p) {
    return csnarray_corrconv_k_helper(csound, p, CSN_CORRELATION);
}

static int32_t allocate_and_zero_pad_before(CSOUND *csound, OPDS *h, bool is_perf, CSN_ARRAY *dest, CSN_ARRAY *source, uint32_t new_ndim, uint32_t *new_shape, uint32_t axis, bool flat) {
    if (is_perf) {
        size_t req_size = 0;
        if (get_array_size_from_shape(&req_size, new_ndim, new_shape) != OK) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, h, "[csnarray] The transform this convolution needs exceeds the maximum element count");
        }
        int32_t res = ensure_mutation_capacity(csound, h, dest, req_size, false);
        if (res != OK) return res;
        set_csnarray_layout(dest, new_ndim, new_shape, req_size, dest->itype);

        if (flat) {
            memcpy(dest->data, source->data, sizeof(double) * source->size * source->itype);
            memset(dest->data + source->size * source->itype, 0, sizeof(double) * (req_size - source->size) * source->itype);
            return OK;
        }
    } else {
        int32_t res = allocate_array(csound, dest, new_ndim, new_shape, 0, source->itype);
        if (res != OK) return res;

        if (flat) {
            memcpy(dest->data, source->data, sizeof(double) * source->size * source->itype);
            return OK;
        }
    }

    if (source->itype == CSN_COMPLEX) {
        COMPLEXDAT value = { .real = 0.0, .imag = 0.0, .isPolar = 0 };
        pad_assign_value(source, dest, 0.0, &value, axis, 0);
    } else {
        pad_assign_value(source, dest, 0.0, NULL, axis, 0);
    }
    return OK;
}

static int32_t allocate_and_zero_pad_before_ndims(CSOUND *csound, OPDS *h, bool is_perf, CSN_ARRAY *dest, CSN_ARRAY *source, uint32_t new_ndim, uint32_t *new_shape) {
    if (is_perf) {
        size_t req_size = 0;
        if (get_array_size_from_shape(&req_size, new_ndim, new_shape) != OK) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, h, "[csnarray] The transform this convolution needs exceeds the maximum element count");
        }
        int32_t res = ensure_mutation_capacity(csound, h, dest, req_size, false);
        if (res != OK) return res;
        set_csnarray_layout(dest, new_ndim, new_shape, req_size, dest->itype);
    } else {
        int32_t res = allocate_array(csound, dest, new_ndim, new_shape, 0, source->itype);
        if (res != OK) return res;
    }

    /* Every axis grows here, so every axis has to be padded in the same pass:
       pad_assign_value bounds-checks the axis it is given and takes the others
       straight from the destination coordinates, which on a destination larger
       than the source reads past its extent. -1 pads them all. */
    if (source->itype == CSN_COMPLEX) {
        COMPLEXDAT value = { .real = 0.0, .imag = 0.0, .isPolar = 0 };
        pad_assign_value(source, dest, 0.0, &value, -1, 0);
    } else {
        pad_assign_value(source, dest, 0.0, NULL, -1, 0);
    }
    return OK;
}

static void fftcoorconv1d_prod(CSN_ARRAY *y, CSN_ARRAY *source_arr_a, CSN_ARRAY *source_arr_b, int32_t axis, CSN_CORRCONV_MODE mode) {
    uint32_t *source_shape = source_arr_a->shape;

    if (axis == -1) {
        for (uint32_t i = 0; i < source_shape[0]; i++) {
            CSN_COMPLEXDAT a = { .re = source_arr_a->data[i * 2], .im = source_arr_a->data[i * 2 + 1] };
            CSN_COMPLEXDAT b = { .re = source_arr_b->data[i * 2], .im = source_arr_b->data[i * 2 + 1] };
            if (mode == CSN_CORRELATION) b.im = -b.im;
            CSN_COMPLEXDAT result = {0};
            complex_prod(&result, a, b);
            y->data[i * 2] = result.re;
            y->data[i * 2 + 1] = result.im;
        }
        return;
    }

    uint32_t reduced_shape[CSN_MAX_DIMS] = {0};
    uint32_t reduced_ndim = 0;
    size_t slice_count = 1;
    for (uint32_t i = 0; i < source_arr_a->ndim; ++i) {
        if (i != (uint32_t) axis) {
            reduced_shape[reduced_ndim++] = source_arr_a->shape[i];
            slice_count *= source_arr_a->shape[i];
        }
    }

    size_t src_stride = source_arr_a->strides[axis];
    size_t dst_stride = y->strides[axis];
    for (size_t linear = 0; linear < slice_count; ++linear) {
        uint32_t dst_coords[CSN_MAX_DIMS] = {0};
        uint32_t src_coords[CSN_MAX_DIMS] = {0};

        from_linear_to_coords(dst_coords, reduced_shape, linear, reduced_ndim);
        for (uint32_t i = 0, j = 0; i < source_arr_a->ndim; ++i) {
            src_coords[i] = (i == (uint32_t) axis) ? 0 : dst_coords[j++];
        }

        size_t src_base = from_coords_to_offset(src_coords, source_arr_a->strides, source_arr_a->ndim);
        size_t dst_base = from_coords_to_offset(src_coords, y->strides, source_arr_a->ndim);
        for (uint32_t i = 0; i < source_shape[axis]; i++) {
            CSN_COMPLEXDAT a = slice_get(source_arr_a->data + src_base * CSN_COMPLEX, i, src_stride, CSN_COMPLEX);
            CSN_COMPLEXDAT b = { .re = source_arr_b->data[i * 2], .im = source_arr_b->data[i * 2 + 1] };
            if (mode == CSN_CORRELATION) b.im = -b.im;
            CSN_COMPLEXDAT result = {0};
            complex_prod(&result, a, b);
            slice_put(y->data + dst_base * CSN_COMPLEX, i, dst_stride, CSN_COMPLEX, result);
        }
    }
}

/* Cuts the published result out of the padded inverse transform.

   The transform is computed over fft_size points, the next power of two at or
   above x + h - 1, so what comes back is the FULL answer followed by padding.
   The edges mode is a window on that: a start offset, from
   fftcorrconv_get_edges_offset, and a length, the one the direct forms
   publish. The read wraps, because a correlation's negative lags sit at the
   end of a circular buffer rather than before its start; for a convolution the
   window never reaches the wrap and the modulo costs nothing. */
static void fftcorrconv_crop(CSN_ARRAY *y, CSN_ARRAY *source_arr, size_t out_size, int64_t start_offset, size_t fft_size, int32_t axis) {
    size_t start = (size_t) start_offset;

    if (axis == -1) {
        for (size_t i = 0; i < out_size; i++) {
            size_t at = (start + i) % fft_size;
            slice_put(y->data, i, 1U, y->itype, slice_get(source_arr->data, at, 1U, source_arr->itype));
        }
        return;
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
    size_t dst_stride = y->strides[axis];
    for (size_t linear = 0; linear < slice_count; ++linear) {
        uint32_t dst_coords[CSN_MAX_DIMS] = {0};
        uint32_t src_coords[CSN_MAX_DIMS] = {0};

        from_linear_to_coords(dst_coords, reduced_shape, linear, reduced_ndim);
        for (uint32_t i = 0, j = 0; i < source_arr->ndim; ++i) {
            src_coords[i] = (i == (uint32_t) axis) ? 0 : dst_coords[j++];
        }

        size_t src_base = from_coords_to_offset(src_coords, source_arr->strides, source_arr->ndim);
        size_t dst_base = from_coords_to_offset(src_coords, y->strides, y->ndim);
        for (size_t i = 0; i < out_size; i++) {
            size_t at = (start + i) % fft_size;
            CSN_COMPLEXDAT value = slice_get(source_arr->data + src_base * source_arr->itype, at, src_stride, source_arr->itype);
            slice_put(y->data + dst_base * y->itype, i, dst_stride, y->itype, value);
        }
    }
}

/* The N-D spectra have the same shape and are both complex and contiguous, so
   their product is one flat pass. Conjugating the kernel is what separates a
   correlation from a convolution, exactly as in the 1-D form. */
static void fftcorrconv_prod_ndims(CSN_ARRAY *y, CSN_ARRAY *source_arr_a, CSN_ARRAY *source_arr_b, CSN_CORRCONV_MODE mode) {
    for (size_t i = 0; i < y->size; i++) {
        CSN_COMPLEXDAT a = { .re = source_arr_a->data[i * 2], .im = source_arr_a->data[i * 2 + 1] };
        CSN_COMPLEXDAT b = { .re = source_arr_b->data[i * 2], .im = source_arr_b->data[i * 2 + 1] };
        if (mode == CSN_CORRELATION) b.im = -b.im;
        CSN_COMPLEXDAT result = {0};
        complex_prod(&result, a, b);
        y->data[i * 2] = result.re;
        y->data[i * 2 + 1] = result.im;
    }
}

/* The N-D crop: the same window the 1-D form applies, one offset and one
   modulus per axis. Every axis wraps for a correlation and none of them does
   for a convolution, so the modulo is written once and costs nothing in the
   case that never reaches it. */
static void fftcorrconv_crop_ndims(CSN_ARRAY *y, CSN_ARRAY *source_arr, const size_t *fft_sizes, const int64_t *start_offset) {
    for (size_t linear = 0; linear < y->size; linear++) {
        uint32_t dst_coords[CSN_MAX_DIMS] = {0};
        uint32_t src_coords[CSN_MAX_DIMS] = {0};

        from_linear_to_coords(dst_coords, y->shape, linear, y->ndim);
        for (uint32_t d = 0; d < y->ndim; d++) {
            src_coords[d] = (uint32_t) (((size_t) dst_coords[d] + (size_t) start_offset[d]) % fft_sizes[d]);
        }

        size_t src_offset = from_coords_to_offset(src_coords, source_arr->strides, source_arr->ndim);
        slice_put(y->data, linear, 1U, y->itype, slice_get(source_arr->data, src_offset, 1U, source_arr->itype));
    }
}

/* The transform buffers are private arrays, so they carry the mark as their own
   external_lock. They serve the output and are sized from both operands, so
   any of the three marks forbids growing them at perf time. */
static void fftcorrconv_set_external_lock(CSN_CORRCONV *p, bool locked) {
    p->fft_buffer_x.external_lock = locked;
    p->fft_buffer_h.external_lock = locked;
    p->ifft_buffer.external_lock = locked;
    p->ifft_out.external_lock = locked;
    p->x_padded.external_lock = locked;
    p->h_padded.external_lock = locked;
}

static int32_t fftcorrconv_allocate(CSOUND *csound, OPDS *perf_h, CSN_CORRCONV *p, CSN_ARRAY *source_x, CSN_ARRAY *source_h, size_t fft_size, CSN_FFT_MODE fft_mode, CSN_FFT_MODE ifft_mode, int32_t axis) {
    if (axis != -1) {
        fft_assign_layout(&p->fc.x_out_size, &p->fc.x_work_size, &p->fc.x_ndim, p->fc.x_shape_fft, source_x, fft_size, fft_mode, (uint32_t) axis);
    } else {
        fft_assign_flatten_layout(&p->fc.x_out_size, &p->fc.x_work_size, &p->fc.x_ndim, p->fc.x_shape_fft, source_x, fft_size, fft_mode);
    }
    fft_assign_layout(&p->fc.h_out_size, &p->fc.h_work_size, &p->fc.h_ndim, p->fc.h_shape_fft, source_h, fft_size, fft_mode, 0U);

    uint32_t converted_axis = axis == -1 ? 0 : (uint32_t) axis;
    p->fc.padded_ndim = axis == -1 ? 1U : source_x->ndim;
    if (axis != -1) {
        memcpy(p->fc.shape_padded_x, source_x->shape, sizeof(uint32_t) * CSN_MAX_DIMS);
        p->fc.shape_padded_x[axis] = (uint32_t) fft_size;
    } else {
        p->fc.shape_padded_x[0] = (uint32_t) fft_size;
    }
    p->fc.shape_padded_h[0] = (uint32_t) fft_size;

    bool is_perf = perf_h != NULL;
    int32_t res;
    size_t req_size = 0;
    if (is_perf) {
        if (get_array_size_from_shape(&req_size, p->fc.x_ndim, p->fc.x_shape_fft) != OK) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] The transform this convolution needs exceeds the maximum element count");
        }
        res = ensure_mutation_capacity(csound, perf_h, &p->fft_buffer_x, req_size, false);
        if (res != OK) return res;
        set_csnarray_layout(&p->fft_buffer_x, p->fc.x_ndim, p->fc.x_shape_fft, req_size, p->fft_buffer_x.itype);

        if (get_array_size_from_shape(&req_size, p->fc.h_ndim, p->fc.h_shape_fft) != OK) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] The transform this convolution needs exceeds the maximum element count");
        }
        res = ensure_mutation_capacity(csound, perf_h, &p->fft_buffer_h, req_size, false);
        if (res != OK) return res;
        set_csnarray_layout(&p->fft_buffer_h, p->fc.h_ndim, p->fc.h_shape_fft, req_size, p->fft_buffer_h.itype);

        if (get_array_size_from_shape(&req_size, p->fc.x_ndim, p->fc.x_shape_fft) != OK) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] The transform this convolution needs exceeds the maximum element count");
        }
        res = ensure_mutation_capacity(csound, perf_h, &p->ifft_buffer, req_size, false);
        if (res != OK) return res;
        set_csnarray_layout(&p->ifft_buffer, p->fc.x_ndim, p->fc.x_shape_fft, req_size, p->ifft_buffer.itype);

        if (allocate_and_zero_pad_before(csound, &p->h, is_perf, &p->x_padded, source_x, p->fc.padded_ndim, p->fc.shape_padded_x, converted_axis, axis == -1) != OK
            || allocate_and_zero_pad_before(csound, &p->h, is_perf, &p->h_padded, source_h, 1U, p->fc.shape_padded_h, 0, true) != OK) {
                return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Internal error: memory allocation failed");
            }
    } else {
        if (allocate_and_zero_pad_before(csound, &p->h, is_perf, &p->x_padded, source_x, p->fc.padded_ndim, p->fc.shape_padded_x, converted_axis, axis == -1) != OK
            || allocate_and_zero_pad_before(csound, &p->h, is_perf, &p->h_padded, source_h, 1U, p->fc.shape_padded_h, 0, true) != OK
            || allocate_array(csound, &p->fft_buffer_x, p->fc.x_ndim, p->fc.x_shape_fft, 0, CSN_COMPLEX) != OK
            || allocate_array(csound, &p->fft_buffer_h, p->fc.h_ndim, p->fc.h_shape_fft, 0, CSN_COMPLEX) != OK
            || allocate_array(csound, &p->ifft_buffer, p->fc.x_ndim, p->fc.x_shape_fft, 0, CSN_COMPLEX) != OK) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Internal error: memory allocation failed");
        }
    }

    if (axis != -1) {
        fft_assign_layout(&p->fc.ifft_out_size, &p->fc.ifft_work_size, &p->fc.ifft_ndim, p->fc.ifft_shape, &p->ifft_buffer, (uint32_t) fft_size, ifft_mode, (uint32_t) axis);
    } else {
        fft_assign_flatten_layout(&p->fc.ifft_out_size, &p->fc.ifft_work_size, &p->fc.ifft_ndim, p->fc.ifft_shape, &p->ifft_buffer, (uint32_t) fft_size, ifft_mode);
    }

    /* The inverse transform is written at its full padded length and the
       published handle is a window on it, so it needs a destination of its
       own rather than the output array. */

    if (is_perf) {
        if (get_array_size_from_shape(&req_size, p->fc.ifft_ndim, p->fc.ifft_shape) != OK) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] The transform this convolution needs exceeds the maximum element count");
        }
        res = ensure_mutation_capacity(csound, perf_h, &p->ifft_out, req_size, false);
        if (res != OK) return res;
        set_csnarray_layout(&p->ifft_out, p->fc.ifft_ndim, p->fc.ifft_shape, req_size, p->ifft_out.itype);
    } else {
        ITEM_TYPE otype = ifft_mode == CSNIRFFT ? CSN_REAL : CSN_COMPLEX;
        if (allocate_array(csound, &p->ifft_out, p->fc.ifft_ndim, p->fc.ifft_shape, 0, otype) != OK) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Internal error: memory allocation failed");
        }
    }

    MYFLT *temp_buffer_x = NULL;
    MYFLT *temp_buffer_h = NULL;
    MYFLT *temp_buffer_ifft = NULL;
    if (is_perf) {
        /* The spectra above already carry the mark as external_lock; the work
           buffers follow the same one. Each is committed to p only once it
           exists, so a failure leaves nothing freed behind a live pointer. */
        bool rt_locked = p->fft_buffer_x.external_lock;
        res = csn_scratch_reserve(csound, perf_h, rt_locked, &p->buffer_x, p->fc.x_work_size, sizeof(MYFLT));
        if (res == OK) res = csn_scratch_reserve(csound, perf_h, rt_locked, &p->buffer_h, p->fc.h_work_size, sizeof(MYFLT));
        if (res == OK) res = csn_scratch_reserve(csound, perf_h, rt_locked, &p->buffer_ifft, p->fc.ifft_work_size, sizeof(MYFLT));
        if (res != OK) return res;
    } else {
        temp_buffer_x = csound->Calloc(csound, sizeof(MYFLT) * p->fc.x_work_size * 2);
        if (temp_buffer_x == NULL) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Internal error: memory allocation failed");
        }
        temp_buffer_h = csound->Calloc(csound, sizeof(MYFLT) * p->fc.h_work_size * 2);
        if (temp_buffer_h == NULL) {
            csound->Free(csound, temp_buffer_x);
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Internal error: memory allocation failed");
        }
        temp_buffer_ifft = csound->Calloc(csound, sizeof(MYFLT) * p->fc.ifft_work_size * 2);
        if (temp_buffer_ifft == NULL) {
            csound->Free(csound, temp_buffer_x);
            csound->Free(csound, temp_buffer_h);
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Internal error: memory allocation failed");
        }
        p->buffer_x.scratch = temp_buffer_x;
        p->buffer_x.scratch_capacity = p->fc.x_work_size * 2;
        p->buffer_h.scratch = temp_buffer_h;
        p->buffer_h.scratch_capacity = p->fc.h_work_size * 2;
        p->buffer_ifft.scratch = temp_buffer_ifft;
        p->buffer_ifft.scratch_capacity = p->fc.ifft_work_size * 2;
    }

    return OK;
}

/* Lays out and allocates everything the N-D transform needs.

   An N-D FFT is separable: one pass per axis, each consuming and producing the
   same grid, so a single spectrum buffer per operand carries the whole thing
   however many dimensions there are. What is per-axis is not the buffer but
   the length, and with it the shape of the spectrum: only the axis transformed
   first is one-sided, and only when the operands are real. Every pass after it
   is a full complex transform that leaves its axis at the padded length. The
   scratch is one lane's worth of workspace, so it is sized to the widest axis
   and reused. */
static int32_t fftcorrconv_allocate_ndims(CSOUND *csound, OPDS *perf_h, CSN_CORRCONV *p, CSN_ARRAY *source_x, CSN_ARRAY *source_h, CSN_FFT_MODE fft_mode, CSN_FFT_MODE ifft_mode) {
    uint32_t ndim = source_x->ndim;
    uint32_t last_axis = ndim - 1U;

    p->fc.padded_ndim = ndim;
    p->fc.x_ndim = ndim;
    p->fc.h_ndim = ndim;
    p->fc.ifft_ndim = ndim;

    size_t max_work_size = 0;
    /* The inverse walks the same lengths back and its shape is the padded grid
       the crop reads from, so the shape this fills in is not needed. */
    uint32_t ifft_layout_shape[CSN_MAX_DIMS] = {0};
    for (uint32_t axis = 0; axis < ndim; axis++) {
        p->fc.shape_padded_x[axis] = (uint32_t) p->fc.fft_sizes[axis];
        p->fc.shape_padded_h[axis] = (uint32_t) p->fc.fft_sizes[axis];
        p->fc.ifft_shape[axis] = (uint32_t) p->fc.fft_sizes[axis];

        CSN_FFT_MODE axis_mode = axis == last_axis ? fft_mode : CSNFFT;
        CSN_FFT_MODE axis_ifft_mode = axis == last_axis ? ifft_mode : CSNIFFT;
        fft_assign_layout_at(p->fc.axis_out_size, p->fc.axis_work_size, p->fc.x_shape_fft, p->fc.fft_sizes, axis_mode, axis);
        fft_assign_layout_at(p->fc.ifft_axis_out_size, p->fc.ifft_axis_work_size, ifft_layout_shape, p->fc.fft_sizes, axis_ifft_mode, axis);

        max_work_size = p->fc.axis_work_size[axis] > max_work_size ? p->fc.axis_work_size[axis] : max_work_size;
        max_work_size = p->fc.ifft_axis_work_size[axis] > max_work_size ? p->fc.ifft_axis_work_size[axis] : max_work_size;
    }
    /* Both operands are padded onto the same grid, so they share a spectrum. */
    memcpy(p->fc.h_shape_fft, p->fc.x_shape_fft, sizeof(uint32_t) * CSN_MAX_DIMS);

    size_t padded_size = 0;
    size_t spectrum_size = 0;
    if (get_array_size_from_shape(&padded_size, ndim, p->fc.shape_padded_x) != OK
        || get_array_size_from_shape(&spectrum_size, ndim, p->fc.x_shape_fft) != OK) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] The transform this convolution needs exceeds the maximum element count: every axis is padded to a power of two at or above x + h - 1");
    }

    bool is_perf = perf_h != NULL;
    int32_t res;
    MYFLT *temp_buffer_x = NULL;
    MYFLT *temp_buffer_h = NULL;
    MYFLT *temp_buffer_ifft = NULL;
    size_t req_size = 0;
    if (is_perf) {
        if (get_array_size_from_shape(&req_size, ndim, p->fc.x_shape_fft) != OK) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] The transform this convolution needs exceeds the maximum element count");
        }
        res = ensure_mutation_capacity(csound, perf_h, &p->fft_buffer_x, req_size, false);
        if (res != OK) return res;
        set_csnarray_layout(&p->fft_buffer_x, ndim, p->fc.x_shape_fft, req_size, CSN_COMPLEX);

        if (get_array_size_from_shape(&req_size, ndim, p->fc.h_shape_fft) != OK) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] The transform this convolution needs exceeds the maximum element count");
        }
        res = ensure_mutation_capacity(csound, perf_h, &p->fft_buffer_h, req_size, false);
        if (res != OK) return res;
        set_csnarray_layout(&p->fft_buffer_h, ndim, p->fc.h_shape_fft, req_size, CSN_COMPLEX);

        if (get_array_size_from_shape(&req_size, ndim, p->fc.x_shape_fft) != OK) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] The transform this convolution needs exceeds the maximum element count");
        }
        res = ensure_mutation_capacity(csound, perf_h, &p->ifft_buffer, req_size, false);
        if (res != OK) return res;
        set_csnarray_layout(&p->ifft_buffer, ndim, p->fc.x_shape_fft, req_size, CSN_COMPLEX);

        if (get_array_size_from_shape(&req_size, ndim, p->fc.ifft_shape) != OK) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] The transform this convolution needs exceeds the maximum element count");
        }
        res = ensure_mutation_capacity(csound, perf_h, &p->ifft_out, req_size, false);
        if (res != OK) return res;
        ITEM_TYPE otype = ifft_mode == CSNIRFFT ? CSN_REAL : CSN_COMPLEX;
        set_csnarray_layout(&p->ifft_out, ndim, p->fc.ifft_shape, req_size, otype);

        if (allocate_and_zero_pad_before_ndims(csound, &p->h, is_perf, &p->x_padded, source_x, ndim, p->fc.shape_padded_x) != OK
            || allocate_and_zero_pad_before_ndims(csound, &p->h, is_perf, &p->h_padded, source_h, ndim, p->fc.shape_padded_h) != OK) {
                return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Internal error: memory allocation failed");
            }

        /* Same rule as the 1-D form: the work buffers follow the spectra's
           mark and are committed only once they exist. */
        bool rt_locked = p->fft_buffer_x.external_lock;
        res = csn_scratch_reserve(csound, perf_h, rt_locked, &p->buffer_x, max_work_size, sizeof(MYFLT));
        if (res == OK) res = csn_scratch_reserve(csound, perf_h, rt_locked, &p->buffer_h, max_work_size, sizeof(MYFLT));
        if (res == OK) res = csn_scratch_reserve(csound, perf_h, rt_locked, &p->buffer_ifft, max_work_size, sizeof(MYFLT));
        if (res != OK) return res;
    } else {
        ITEM_TYPE otype = ifft_mode == CSNIRFFT ? CSN_REAL : CSN_COMPLEX;
        if (allocate_and_zero_pad_before_ndims(csound, &p->h, is_perf, &p->x_padded, source_x, ndim, p->fc.shape_padded_x) != OK
            || allocate_and_zero_pad_before_ndims(csound, &p->h, is_perf, &p->h_padded, source_h, ndim, p->fc.shape_padded_h) != OK
            || allocate_array(csound, &p->fft_buffer_x, ndim, p->fc.x_shape_fft, 0, CSN_COMPLEX) != OK
            || allocate_array(csound, &p->fft_buffer_h, ndim, p->fc.h_shape_fft, 0, CSN_COMPLEX) != OK
            || allocate_array(csound, &p->ifft_buffer, ndim, p->fc.x_shape_fft, 0, CSN_COMPLEX) != OK
            || allocate_array(csound, &p->ifft_out, ndim, p->fc.ifft_shape, 0, otype) != OK) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Internal error: memory allocation failed");
        }

        size_t b_cap = max_work_size * 2;
        temp_buffer_x = csound->Calloc(csound, sizeof(MYFLT) * b_cap);
        if (temp_buffer_x == NULL) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Internal error: memory allocation failed");
        }
        temp_buffer_h = csound->Calloc(csound, sizeof(MYFLT) * b_cap);
        if (temp_buffer_h == NULL) {
            csound->Free(csound, temp_buffer_x);
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Internal error: memory allocation failed");
        }
        temp_buffer_ifft = csound->Calloc(csound, sizeof(MYFLT) * b_cap);
        if (temp_buffer_ifft == NULL) {
            csound->Free(csound, temp_buffer_x);
            csound->Free(csound, temp_buffer_h);
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Internal error: memory allocation failed");
        }

        p->buffer_x.scratch = temp_buffer_x;
        p->buffer_x.scratch_capacity = b_cap;
        p->buffer_h.scratch = temp_buffer_h;
        p->buffer_h.scratch_capacity = b_cap;
        p->buffer_ifft.scratch = temp_buffer_ifft;
        p->buffer_ifft.scratch_capacity = b_cap;
    }

    return OK;
}

static int32_t fftcorrconv1d_helper(CSOUND *csound, CSN_CORRCONV *p, CSN_CORRCONV_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle_a = p->source_handle_a->id;
    uint32_t source_handle_b = p->source_handle_b->id;

    double edges_temp = (double) *p->arg_a;
    double axis_value = (double) *p->arg_b;

    if (!IS_VALID_EDGES(edges_temp)) {
        return csound->InitError(csound, "[csnarray] Invalid edges mode: should be 0, 1, or 2 (see documentation)");
    }
    uint32_t edges_mode = (uint32_t) edges_temp;

    int32_t res = OK;
    const char *err = NULL;

    void *fft_setup = NULL;
    void *ifft_setup = NULL;

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot_x = get_slot(reg, source_handle_a);
    if (slot_x == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle_a);
        goto done;
    }
    CSN_SLOT *slot_h = get_slot(reg, source_handle_b);
    if (slot_h == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle_b);
        goto done;
    }
    CSN_ARRAY *source_x = slot_x->array;
    CSN_ARRAY *source_h = slot_h->array;
    uint32_t source_ndim_x = source_x->ndim;

    if (axis_value != -1.0 && !IS_VALID_AXIS(axis_value, source_ndim_x)) {
        res = csound->InitError(csound, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: -1 for all axes (flatten), or finite integers 0..%u)", axis_value, source_ndim_x, source_ndim_x - 1);
        goto done;
    }
    int32_t axis = (int32_t) axis_value;

    res = corrconv1d_validate(csound, NULL, source_x, source_h, axis, edges_mode);
    if (res != OK) goto done;

    size_t xfft_length = axis == -1 ? source_x->size : (size_t) source_x->shape[axis];
    size_t fft_size_check = xfft_length + source_h->size - 1;
    size_t fft_size = IS_POWER_OF_TWO(fft_size_check) ? fft_size_check : NEXT_POWER_OF_TWO(fft_size_check);
    fftcorrconv_get_edges_offset(&p->fc.crop_offset[0], source_h->size, fft_size, edges_mode, mode);

    ITEM_TYPE itype = (source_x->itype == CSN_COMPLEX || source_h->itype == CSN_COMPLEX) ? CSN_COMPLEX : CSN_REAL;
    CSN_FFT_MODE fft_mode = itype == CSN_COMPLEX ? CSNFFT : CSNRFFT;
    CSN_FFT_MODE ifft_mode = fft_mode == CSNFFT ? CSNIFFT : CSNIRFFT;

    if (fft_mode == CSNRFFT) {
        fft_setup = csound->RealFFTSetup(csound, fft_size, FFT_FWD);
        ifft_setup = csound->RealFFTSetup(csound, fft_size, FFT_INV);
    }

    /* The published shape is the one the direct forms publish: the transform
       runs over a padded power of two, the result does not. */
    size_t size_result = 0;
    int64_t direct_offset = 0;
    corrconv_get_size_and_edges_offset(&size_result, &direct_offset, source_x, source_h, axis, edges_mode);

    uint32_t new_ndim = axis == -1 ? 1U : source_ndim_x;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    if (axis == -1) {
        new_shape[0] = (uint32_t) size_result;
    } else {
        memcpy(new_shape, source_x->shape, sizeof(uint32_t) * CSN_MAX_DIMS);
        new_shape[axis] = (uint32_t) size_result;
    }

    ITEM_TYPE otype = ifft_mode == CSNIRFFT ? CSN_REAL : CSN_COMPLEX;
    uint32_t protect[2] = { source_handle_a, source_handle_b };
    if (create_csnarray_locked(csound, reg, &p->h, new_ndim, new_shape, &p->array, p->handle, protect, 2U, &err, otype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    res = fftcorrconv_allocate(csound, NULL, p, source_x, source_h, fft_size, fft_mode, ifft_mode, axis);
    if (res != OK) goto done;

    double *temp_buffer_x = (double *) p->buffer_x.scratch;
    double *temp_buffer_h = (double *) p->buffer_h.scratch;
    double *temp_buffer_ifft = (double *) p->buffer_ifft.scratch;
    uint32_t converted_axis = axis == -1 ? 0 : (uint32_t) axis;
    fft_assign_value(csound, fft_setup, &p->fft_buffer_x, &p->x_padded, temp_buffer_x, fft_size, p->fc.x_work_size, p->fc.x_out_size, converted_axis, fft_mode);
    fft_assign_value(csound, fft_setup, &p->fft_buffer_h, &p->h_padded, temp_buffer_h, fft_size, p->fc.h_work_size, p->fc.h_out_size, 0U, fft_mode);
    fftcoorconv1d_prod(&p->ifft_buffer, &p->fft_buffer_x, &p->fft_buffer_h, axis, mode);
    ifft_assign_value(csound, ifft_setup, &p->ifft_out, &p->ifft_buffer, temp_buffer_ifft, fft_size, p->fc.ifft_work_size, p->fc.ifft_out_size, converted_axis, ifft_mode);
    fftcorrconv_crop(p->array, &p->ifft_out, size_result, p->fc.crop_offset[0], fft_size, axis);

    SET_KDATA_BEGIN(p, reg);
    set_array_version(&p->k_data.prev_output_version, &p->array->version);
    p->k_data.prev_axis_i = axis;
    p->k_data.prev_index = edges_mode;

    p->k_data_fft_x.mode = fft_mode;
    p->k_data_fft_x.nfft = fft_size;
    p->k_data_fft_x.fft_setup = fft_setup;
    p->k_data_fft_x.buffer_out_size = p->fc.x_out_size;
    p->k_data_fft_x.buffer_work_size = p->fc.x_work_size;
    p->k_data_fft_h.mode = fft_mode;
    p->k_data_fft_h.nfft = fft_size;
    p->k_data_fft_h.fft_setup = fft_setup;
    p->k_data_fft_h.buffer_out_size = p->fc.h_out_size;
    p->k_data_fft_h.buffer_work_size = p->fc.h_work_size;
    p->k_data_ifft.mode = ifft_mode;
    p->k_data_ifft.nfft = fft_size;
    p->k_data_ifft.fft_setup = ifft_setup;
    p->k_data_ifft.buffer_out_size = p->fc.ifft_out_size;
    p->k_data_ifft.buffer_work_size = p->fc.ifft_work_size;

    fftcorrconv_set_external_lock(p, csn_slot_rt_locked(reg, p->k_data.owned_handle) || slot_x->rt_locked || slot_h->rt_locked);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_fftconvolve1d(CSOUND *csound, CSN_CORRCONV *p) {
    return fftcorrconv1d_helper(csound, p, CSN_CONVOLUTION);
}

int32_t csnarray_fftcorrelate1d(CSOUND *csound, CSN_CORRCONV *p) {
    return fftcorrconv1d_helper(csound, p, CSN_CORRELATION);
}

static int32_t fftcorrconv_helper(CSOUND *csound, CSN_CORRCONV *p, CSN_CORRCONV_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle_a = p->source_handle_a->id;
    uint32_t source_handle_b = p->source_handle_b->id;

    double edges_temp = (double) *p->arg_a;

    if (!IS_VALID_EDGES(edges_temp)) {
        return csound->InitError(csound, "[csnarray] Invalid edges mode: should be 0, 1, or 2 (see documentation)");
    }
    uint32_t edges_mode = (uint32_t) edges_temp;

    int32_t res = OK;
    const char *err = NULL;

    void *fft_setup = NULL;
    void *ifft_setup = NULL;

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot_x = get_slot(reg, source_handle_a);
    if (slot_x == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle_a);
        goto done;
    }
    CSN_SLOT *slot_h = get_slot(reg, source_handle_b);
    if (slot_h == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle_b);
        goto done;
    }
    CSN_ARRAY *source_x = slot_x->array;
    CSN_ARRAY *source_h = slot_h->array;
    uint32_t ndim = source_x->ndim;

    res = corrconv_validate(csound, NULL, source_x, source_h);
    if (res != OK) goto done;

    uint32_t last_axis = ndim - 1U;
    uint32_t fft_shape[CSN_MAX_DIMS] = {0};
    fftcorrconv_get_shape_and_edges_offset_ndims(fft_shape, p->fc.crop_offset, source_x, source_h, edges_mode, mode);
    for (uint32_t axis = 0; axis < ndim; axis++) {
        p->fc.fft_sizes[axis] = (size_t) fft_shape[axis];
    }

    ITEM_TYPE itype = (source_x->itype == CSN_COMPLEX || source_h->itype == CSN_COMPLEX) ? CSN_COMPLEX : CSN_REAL;
    CSN_FFT_MODE fft_mode = itype == CSN_COMPLEX ? CSNFFT : CSNRFFT;
    CSN_FFT_MODE ifft_mode = fft_mode == CSNFFT ? CSNIFFT : CSNIRFFT;

    /* Only the one-sided pass needs a setup, and it runs on the last axis, so
       that axis alone decides the length it is built for. */
    if (fft_mode == CSNRFFT) {
        fft_setup = csound->RealFFTSetup(csound, (int32_t) p->fc.fft_sizes[last_axis], FFT_FWD);
        ifft_setup = csound->RealFFTSetup(csound, (int32_t) p->fc.fft_sizes[last_axis], FFT_INV);
    }

    /* The published shape is the one the direct forms publish: the transform
       runs over a padded power of two on every axis, the result does not. */
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    int64_t direct_offset[CSN_MAX_DIMS] = {0};
    corrconv_get_shape_and_edges_offset_ndims(new_shape, direct_offset, source_x, source_h, edges_mode);

    ITEM_TYPE otype = ifft_mode == CSNIRFFT ? CSN_REAL : CSN_COMPLEX;
    uint32_t protect[2] = { source_handle_a, source_handle_b };
    if (create_csnarray_locked(csound, reg, &p->h, ndim, new_shape, &p->array, p->handle, protect, 2U, &err, otype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    res = fftcorrconv_allocate_ndims(csound, NULL, p, source_x, source_h, fft_mode, ifft_mode);
    if (res != OK) goto done;

    MYFLT *temp_buffer_x = (MYFLT *) p->buffer_x.scratch;
    MYFLT *temp_buffer_h = (MYFLT *) p->buffer_h.scratch;
    MYFLT *temp_buffer_ifft = (MYFLT *) p->buffer_ifft.scratch;

    /* Forward: the last axis reads the padded operand and is the one-sided
       pass when the operands are real; the axes before it run in place on the
       spectrum, each at its own length. */
    fft_assign_value(csound, fft_setup, &p->fft_buffer_x, &p->x_padded, temp_buffer_x, (uint32_t) p->fc.fft_sizes[last_axis], p->fc.axis_work_size[last_axis], p->fc.axis_out_size[last_axis], last_axis, fft_mode);
    fft_assign_value(csound, fft_setup, &p->fft_buffer_h, &p->h_padded, temp_buffer_h, (uint32_t) p->fc.fft_sizes[last_axis], p->fc.axis_work_size[last_axis], p->fc.axis_out_size[last_axis], last_axis, fft_mode);
    for (int32_t axis = (int32_t) last_axis - 1; axis >= 0; axis--) {
        fft_assign_value(csound, NULL, &p->fft_buffer_x, &p->fft_buffer_x, temp_buffer_x, (uint32_t) p->fc.fft_sizes[axis], p->fc.axis_work_size[axis], p->fc.axis_out_size[axis], (uint32_t) axis, CSNFFT);
        fft_assign_value(csound, NULL, &p->fft_buffer_h, &p->fft_buffer_h, temp_buffer_h, (uint32_t) p->fc.fft_sizes[axis], p->fc.axis_work_size[axis], p->fc.axis_out_size[axis], (uint32_t) axis, CSNFFT);
    }

    fftcorrconv_prod_ndims(&p->ifft_buffer, &p->fft_buffer_x, &p->fft_buffer_h, mode);

    /* Inverse: mirror order, the full complex axes first and the one-sided one
       last, so the pass that widens the spectrum back is the pass that writes
       the padded result. */
    for (uint32_t axis = 0; axis < last_axis; axis++) {
        ifft_assign_value(csound, NULL, &p->ifft_buffer, &p->ifft_buffer, temp_buffer_ifft, (uint32_t) p->fc.fft_sizes[axis], p->fc.ifft_axis_work_size[axis], p->fc.ifft_axis_out_size[axis], axis, CSNIFFT);
    }
    ifft_assign_value(csound, ifft_setup, &p->ifft_out, &p->ifft_buffer, temp_buffer_ifft, (uint32_t) p->fc.fft_sizes[last_axis], p->fc.ifft_axis_work_size[last_axis], p->fc.ifft_axis_out_size[last_axis], last_axis, ifft_mode);

    fftcorrconv_crop_ndims(p->array, &p->ifft_out, p->fc.fft_sizes, p->fc.crop_offset);

    SET_KDATA_BEGIN(p, reg);
    set_array_version(&p->k_data.prev_output_version, &p->array->version);
    p->k_data.prev_index = edges_mode;

    p->k_data_fft_x.mode = fft_mode;
    p->k_data_fft_x.nfft = p->fc.fft_sizes[last_axis];
    p->k_data_fft_x.fft_setup = fft_setup;
    p->k_data_fft_h.mode = fft_mode;
    p->k_data_fft_h.nfft = p->fc.fft_sizes[last_axis];
    p->k_data_fft_h.fft_setup = fft_setup;
    p->k_data_ifft.mode = ifft_mode;
    p->k_data_ifft.nfft = p->fc.fft_sizes[last_axis];
    p->k_data_ifft.fft_setup = ifft_setup;
    p->is_published = false;

    fftcorrconv_set_external_lock(p, csn_slot_rt_locked(reg, p->k_data.owned_handle) || slot_x->rt_locked || slot_h->rt_locked);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_fftconvolve(CSOUND *csound, CSN_CORRCONV *p) {
    return fftcorrconv_helper(csound, p, CSN_CONVOLUTION);
}

int32_t csnarray_fftcorrelate(CSOUND *csound, CSN_CORRCONV *p) {
    return fftcorrconv_helper(csound, p, CSN_CORRELATION);
}

static int32_t fftcorrconv1d_k_helper(CSOUND *csound, CSN_CORRCONV *p, CSN_CORRCONV_MODE mode) {
    CSN_REGISTRY *reg = p->k_data.registry;
    uint32_t owned_handle = p->k_data.owned_handle;
    CHECK_REG_HANDLE(csound, &p->h, reg, owned_handle);

    uint32_t source_handle_a = p->source_handle_a->id;
    uint32_t source_handle_b = p->source_handle_b->id;

    uint32_t edges_mode = p->k_data.prev_index;
    int32_t axis = p->k_data.prev_axis_i;

    int32_t res = OK;
    const char *err = NULL;

    res = CHECK_SELF_ALIAS(csound, &p->h, &p->k_data, source_handle_a, source_handle_b);
    if (res != OK) return res;

    CHECK_KTRIG(p->arg_c);

    void *fft_setup = p->k_data_fft_x.fft_setup;
    void *ifft_setup = p->k_data_ifft.fft_setup;
    size_t fft_size = p->k_data_fft_x.nfft;
    size_t fft_mode = p->k_data_fft_x.mode;
    size_t ifft_mode = p->k_data_ifft.mode;

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot_x = get_slot(reg, source_handle_a);
    if (slot_x == NULL) {
        res = CSN_ACCESSOR_ERROR_LOCKED(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle_a);
        goto done;
    }
    CSN_SLOT *slot_h = get_slot(reg, source_handle_b);
    if (slot_h == NULL) {
        res = CSN_ACCESSOR_ERROR_LOCKED(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle_b);
        goto done;
    }
    CSN_ARRAY *source_x = slot_x->array;
    CSN_ARRAY *source_h = slot_h->array;
    uint32_t source_ndim_x = source_x->ndim;

    res = corrconv1d_validate(csound, &p->h, source_x, source_h, axis, edges_mode);
    if (res != OK) goto done;

    /* Decided before anything below can allocate: the transform setups, the
       spectra and the work buffers all serve the output, and a mark on it or
       on either operand forbids growing them at perf time. */
    bool rt_locked = csn_slot_rt_locked(reg, owned_handle) || slot_x->rt_locked || slot_h->rt_locked;
    fftcorrconv_set_external_lock(p, rt_locked);

    if (p->is_published) {
        bool is_same_source = is_same_array_version(&p->k_data.prev_source_version, &source_x->version);
        bool is_same_kernel = is_same_array_version(&p->k_data.prev_source_version_b, &source_h->version);
        bool is_same_result = false;
        CSN_SLOT *slot_res = get_slot(reg, owned_handle);
        if (slot_res != NULL) {
            is_same_result = is_same_array_version(&p->k_data.prev_output_version, &slot_res->array->version);
        }

        if (is_same_source && is_same_kernel && is_same_result) {
            p->handle->id = owned_handle;
            goto done;
        }
    }

    size_t xfft_length = axis == -1 ? source_x->size : (size_t) source_x->shape[axis];
    size_t fft_size_check = xfft_length + source_h->size - 1;
    size_t requested_fft_size = IS_POWER_OF_TWO(fft_size_check) ? fft_size_check : NEXT_POWER_OF_TWO(fft_size_check);
    if (requested_fft_size != fft_size) {
        if (rt_locked) {
            res = csn_locked_perf_error(csound, &p->h, "[csnarray] A real-time path would need a new %zu-point transform at perf time; clear the mark with csnrtunlock, or pass irt=0 at the audio source it descends from", (size_t) requested_fft_size);
            goto done;
        }
        fft_size = requested_fft_size;
        if (fft_mode == CSNRFFT) {
            fft_setup = csound->RealFFTSetup(csound, (int32_t) fft_size, FFT_FWD);
            ifft_setup = csound->RealFFTSetup(csound, (int32_t) fft_size, FFT_INV);
            p->k_data_fft_x.fft_setup = fft_setup;
            p->k_data_fft_h.fft_setup = fft_setup;
            p->k_data_ifft.fft_setup = ifft_setup;
        }
        p->k_data_fft_x.nfft = fft_size;
        p->k_data_fft_h.nfft = fft_size;
        p->k_data_ifft.nfft = fft_size;
    }

    /* The crop window is read off the current kernel and the current length,
       since a correlation's start is rotated by both. */
    fftcorrconv_get_edges_offset(&p->fc.crop_offset[0], source_h->size, fft_size, edges_mode, mode);

    size_t size_result = 0;
    int64_t direct_offset = 0;
    corrconv_get_size_and_edges_offset(&size_result, &direct_offset, source_x, source_h, axis, edges_mode);

    uint32_t new_ndim = axis == -1 ? 1U : source_ndim_x;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    if (axis == -1) {
        new_shape[0] = (uint32_t) size_result;
    } else {
        memcpy(new_shape, source_x->shape, sizeof(uint32_t) * CSN_MAX_DIMS);
        new_shape[axis] = (uint32_t) size_result;
    }

    size_t req_size = 0;
    if (get_array_size_from_shape(&req_size, new_ndim, new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    ITEM_TYPE otype = ifft_mode == CSNIRFFT ? CSN_REAL : CSN_COMPLEX;
    CSN_ARRAY *arr = NULL;
    size_t logical_size = source_x->size == 0 ? 0 : req_size;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, new_ndim, new_shape, logical_size, otype, err);
    if (res != OK) goto done;
    p->array = arr;

    res = fftcorrconv_allocate(csound, &p->h, p, source_x, source_h, fft_size, fft_mode, ifft_mode, axis);
    if (res != OK) goto done;

    double *temp_buffer_x = (double *) p->buffer_x.scratch;
    double *temp_buffer_h = (double *) p->buffer_h.scratch;
    double *temp_buffer_ifft = (double *) p->buffer_ifft.scratch;
    uint32_t converted_axis = axis == -1 ? 0 : (uint32_t) axis;
    fft_assign_value(csound, fft_setup, &p->fft_buffer_x, &p->x_padded, temp_buffer_x, fft_size, p->fc.x_work_size, p->fc.x_out_size, converted_axis, fft_mode);
    fft_assign_value(csound, fft_setup, &p->fft_buffer_h, &p->h_padded, temp_buffer_h, fft_size, p->fc.h_work_size, p->fc.h_out_size, 0U, fft_mode);
    fftcoorconv1d_prod(&p->ifft_buffer, &p->fft_buffer_x, &p->fft_buffer_h, axis, mode);
    ifft_assign_value(csound, ifft_setup, &p->ifft_out, &p->ifft_buffer, temp_buffer_ifft, fft_size, p->fc.ifft_work_size, p->fc.ifft_out_size, converted_axis, ifft_mode);
    fftcorrconv_crop(p->array, &p->ifft_out, size_result, p->fc.crop_offset[0], fft_size, axis);

    SET_KDATA_END(p, new_shape, new_ndim, p->array->itype);
    set_array_version(&p->k_data.prev_output_version, &p->array->version);
    set_array_version(&p->k_data.prev_source_version, &source_x->version);
    set_array_version(&p->k_data.prev_source_version_b, &source_h->version);

    p->k_data_fft_x.buffer_out_size = p->fc.x_out_size;
    p->k_data_fft_x.buffer_work_size = p->fc.x_work_size;
    p->k_data_fft_h.buffer_out_size = p->fc.h_out_size;
    p->k_data_fft_h.buffer_work_size = p->fc.h_work_size;
    p->k_data_ifft.buffer_out_size = p->fc.ifft_out_size;
    p->k_data_ifft.buffer_work_size = p->fc.ifft_work_size;
    p->is_published = true;


done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t fftcorrconv_k_helper(CSOUND *csound, CSN_CORRCONV *p, CSN_CORRCONV_MODE mode) {
    CSN_REGISTRY *reg = p->k_data.registry;
    uint32_t owned_handle = p->k_data.owned_handle;
    CHECK_REG_HANDLE(csound, &p->h, reg, owned_handle);

    uint32_t source_handle_a = p->source_handle_a->id;
    uint32_t source_handle_b = p->source_handle_b->id;

    uint32_t edges_mode = p->k_data.prev_index;

    int32_t res = OK;
    const char *err = NULL;

    CHECK_KTRIG(p->arg_b);

    void *fft_setup = p->k_data_fft_x.fft_setup;
    void *ifft_setup = p->k_data_ifft.fft_setup;
    size_t fft_size = p->k_data_fft_x.nfft;
    size_t fft_mode = p->k_data_fft_x.mode;
    size_t ifft_mode = p->k_data_ifft.mode;

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot_x = get_slot(reg, source_handle_a);
    if (slot_x == NULL) {
        res = CSN_ACCESSOR_ERROR_LOCKED(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle_a);
        goto done;
    }
    CSN_SLOT *slot_h = get_slot(reg, source_handle_b);
    if (slot_h == NULL) {
        res = CSN_ACCESSOR_ERROR_LOCKED(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle_b);
        goto done;
    }
    CSN_ARRAY *source_x = slot_x->array;
    CSN_ARRAY *source_h = slot_h->array;
    uint32_t ndim = source_x->ndim;

    if (p->is_published) {
        bool is_same_source = is_same_array_version(&p->k_data.prev_source_version, &source_x->version);
        bool is_same_kernel = is_same_array_version(&p->k_data.prev_source_version_b, &source_h->version);
        bool is_same_result = false;
        CSN_SLOT *slot_res = get_slot(reg, owned_handle);
        if (slot_res != NULL) {
            is_same_result = is_same_array_version(&p->k_data.prev_output_version, &slot_res->array->version);
        }

        if (is_same_source && is_same_kernel && is_same_result) {
            p->handle->id = owned_handle;
            goto done;
        }
    }

    res = corrconv_validate(csound, &p->h, source_x, source_h);
    if (res != OK) goto done;

    /* Decided before anything below can allocate: the transform setups, the
       spectra and the work buffers all serve the output, and a mark on it or
       on either operand forbids growing them at perf time. */
    bool rt_locked = csn_slot_rt_locked(reg, owned_handle) || slot_x->rt_locked || slot_h->rt_locked;
    fftcorrconv_set_external_lock(p, rt_locked);

    uint32_t last_axis = ndim - 1U;
    uint32_t fft_shape[CSN_MAX_DIMS] = {0};
    fftcorrconv_get_shape_and_edges_offset_ndims(fft_shape, p->fc.crop_offset, source_x, source_h, edges_mode, mode);
    for (uint32_t axis = 0; axis < ndim; axis++) {
        p->fc.fft_sizes[axis] = (size_t) fft_shape[axis];
    }

    if (p->fc.fft_sizes[last_axis] != fft_size) {
        if (rt_locked) {
            res = csn_locked_perf_error(csound, &p->h, "[csnarray] A real-time path would need a new %zu-point transform at perf time; clear the mark with csnrtunlock, or pass irt=0 at the audio source it descends from", (size_t) p->fc.fft_sizes[last_axis]);
            goto done;
        }
        if (fft_mode == CSNRFFT) {
            fft_setup = csound->RealFFTSetup(csound, (int32_t) p->fc.fft_sizes[last_axis], FFT_FWD);
            ifft_setup = csound->RealFFTSetup(csound, (int32_t) p->fc.fft_sizes[last_axis], FFT_INV);
            p->k_data_fft_x.fft_setup = fft_setup;
            p->k_data_fft_h.fft_setup = fft_setup;
            p->k_data_ifft.fft_setup = ifft_setup;
        }
    }

    /* The published shape is the one the direct forms publish: the transform
       runs over a padded power of two on every axis, the result does not. */
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    int64_t direct_offset[CSN_MAX_DIMS] = {0};
    corrconv_get_shape_and_edges_offset_ndims(new_shape, direct_offset, source_x, source_h, edges_mode);

    size_t req_size = 0;
    if (get_array_size_from_shape(&req_size, ndim, new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    ITEM_TYPE otype = ifft_mode == CSNIRFFT ? CSN_REAL : CSN_COMPLEX;
    CSN_ARRAY *arr = NULL;
    size_t logical_size = source_x->size == 0 ? 0 : req_size;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, ndim, new_shape, logical_size, otype, err);
    if (res != OK) goto done;
    p->array = arr;

    res = fftcorrconv_allocate_ndims(csound, &p->h, p, source_x, source_h, fft_mode, ifft_mode);
    if (res != OK) goto done;

    MYFLT *temp_buffer_x = (MYFLT *) p->buffer_x.scratch;
    MYFLT *temp_buffer_h = (MYFLT *) p->buffer_h.scratch;
    MYFLT *temp_buffer_ifft = (MYFLT *) p->buffer_ifft.scratch;

    /* Forward: the last axis reads the padded operand and is the one-sided
       pass when the operands are real; the axes before it run in place on the
       spectrum, each at its own length. */
    fft_assign_value(csound, fft_setup, &p->fft_buffer_x, &p->x_padded, temp_buffer_x, (uint32_t) p->fc.fft_sizes[last_axis], p->fc.axis_work_size[last_axis], p->fc.axis_out_size[last_axis], last_axis, fft_mode);
    fft_assign_value(csound, fft_setup, &p->fft_buffer_h, &p->h_padded, temp_buffer_h, (uint32_t) p->fc.fft_sizes[last_axis], p->fc.axis_work_size[last_axis], p->fc.axis_out_size[last_axis], last_axis, fft_mode);
    for (int32_t axis = (int32_t) last_axis - 1; axis >= 0; axis--) {
        fft_assign_value(csound, &p->h, &p->fft_buffer_x, &p->fft_buffer_x, temp_buffer_x, (uint32_t) p->fc.fft_sizes[axis], p->fc.axis_work_size[axis], p->fc.axis_out_size[axis], (uint32_t) axis, CSNFFT);
        fft_assign_value(csound, &p->h, &p->fft_buffer_h, &p->fft_buffer_h, temp_buffer_h, (uint32_t) p->fc.fft_sizes[axis], p->fc.axis_work_size[axis], p->fc.axis_out_size[axis], (uint32_t) axis, CSNFFT);
    }

    fftcorrconv_prod_ndims(&p->ifft_buffer, &p->fft_buffer_x, &p->fft_buffer_h, mode);

    /* Inverse: mirror order, the full complex axes first and the one-sided one
       last, so the pass that widens the spectrum back is the pass that writes
       the padded result. */
    for (uint32_t axis = 0; axis < last_axis; axis++) {
        ifft_assign_value(csound, &p->h, &p->ifft_buffer, &p->ifft_buffer, temp_buffer_ifft, (uint32_t) p->fc.fft_sizes[axis], p->fc.ifft_axis_work_size[axis], p->fc.ifft_axis_out_size[axis], axis, CSNIFFT);
    }
    ifft_assign_value(csound, ifft_setup, &p->ifft_out, &p->ifft_buffer, temp_buffer_ifft, (uint32_t) p->fc.fft_sizes[last_axis], p->fc.ifft_axis_work_size[last_axis], p->fc.ifft_axis_out_size[last_axis], last_axis, ifft_mode);

    fftcorrconv_crop_ndims(p->array, &p->ifft_out, p->fc.fft_sizes, p->fc.crop_offset);

    SET_KDATA_END(p, new_shape, ndim, p->array->itype);
    set_array_version(&p->k_data.prev_output_version, &p->array->version);
    set_array_version(&p->k_data.prev_source_version, &source_x->version);
    set_array_version(&p->k_data.prev_source_version_b, &source_h->version);

    p->k_data_fft_x.nfft = p->fc.fft_sizes[last_axis];
    p->k_data_fft_h.nfft = p->fc.fft_sizes[last_axis];
    p->k_data_ifft.nfft = p->fc.fft_sizes[last_axis];
    p->is_published = true;


done:
    csound->UnlockMutex(reg->mutex);
    return res;
}


int32_t csnarray_fftconvolve1d_k(CSOUND *csound, CSN_CORRCONV *p) {
    return fftcorrconv1d_k_helper(csound, p, CSN_CONVOLUTION);
}

int32_t csnarray_fftcorrelate1d_k(CSOUND *csound, CSN_CORRCONV *p) {
    return fftcorrconv1d_k_helper(csound, p, CSN_CORRELATION);
}

int32_t csnarray_fftconvolve_k(CSOUND *csound, CSN_CORRCONV *p) {
    return fftcorrconv_k_helper(csound, p, CSN_CONVOLUTION);
}

int32_t csnarray_fftcorrelate_k(CSOUND *csound, CSN_CORRCONV *p) {
    return fftcorrconv_k_helper(csound, p, CSN_CORRELATION);
}

// DCT I - II


static int32_t get_dct_size(int32_t start_size, CSN_DCST_MODE dcst_mode) {
    int32_t size = start_size;
    switch (dcst_mode) {
        case CSN_DCT_I:
            size = 2 * (size - 1);
            break;
        case CSN_DST_I:
            size = 2 * (size + 1);
            break;
        case CSN_DCT_II:
        case CSN_DST_II:
        case CSN_DST_III:
        case CSN_DCT_III:
            size = 2 * size;
            break;
        case CSN_DCT_IV:
        case CSN_DST_IV:
            size = 4 * size;
            break;
    }
    return size;
}

static int32_t dcst1d_from_fft_assign_value(CSN_ARRAY *dcst_buffer, CSN_ARRAY *fft_buffer, CSN_DCST_MODE mode, int32_t axis) {
    uint32_t reduced_shape[CSN_MAX_DIMS] = {0};
    uint32_t reduced_ndim = 0;
    size_t slice_count = 1;
    /* Driven by the array being filled, not by the spectrum: the two carry the
       same off-axis extents, but only this one bounds the writes. */
    for (uint32_t i = 0; i < dcst_buffer->ndim; ++i) {
        if (i != (uint32_t) axis) {
            reduced_shape[reduced_ndim++] = dcst_buffer->shape[i];
            slice_count *= dcst_buffer->shape[i];
        }
    }

    size_t src_stride = fft_buffer->strides[axis];
    size_t dst_stride = dcst_buffer->strides[axis];
    for (size_t linear = 0; linear < slice_count; ++linear) {
        uint32_t dst_coords[CSN_MAX_DIMS] = {0};
        uint32_t src_coords[CSN_MAX_DIMS] = {0};

        from_linear_to_coords(dst_coords, reduced_shape, linear, reduced_ndim);
        for (uint32_t i = 0, j = 0; i < dcst_buffer->ndim; ++i) {
            src_coords[i] = (i == (uint32_t) axis) ? 0 : dst_coords[j++];
        }

        size_t src_base = from_coords_to_offset(src_coords, fft_buffer->strides, dcst_buffer->ndim);
        size_t dst_base = from_coords_to_offset(src_coords, dcst_buffer->strides, dcst_buffer->ndim);
        uint32_t n = dcst_buffer->shape[axis];
        for (uint32_t i = 0; i < n; i++) {
            CSN_COMPLEXDAT y = {0};
            switch (mode) {
                case CSN_DCT_I: {
                        CSN_COMPLEXDAT z = slice_get(fft_buffer->data + src_base * fft_buffer->itype, i, src_stride, fft_buffer->itype);
                        y.re = z.re;
                    };
                    break;
                case CSN_DST_I: {
                        CSN_COMPLEXDAT z = slice_get(fft_buffer->data + src_base * fft_buffer->itype, i + 1, src_stride, fft_buffer->itype);
                        y.re = -z.im;
                    }
                    break;
                case CSN_DCT_II: {
                        CSN_COMPLEXDAT z = slice_get(fft_buffer->data + src_base * fft_buffer->itype, i, src_stride, fft_buffer->itype);
                        double angle = M_PI * (double) i / (2.0 * (double) n);
                        y.re = z.re * cos(angle) + z.im * sin(angle);
                    };
                    break;
                case CSN_DST_II: {
                        size_t q = i + 1;
                        CSN_COMPLEXDAT z = slice_get(fft_buffer->data + src_base * fft_buffer->itype, q, src_stride, fft_buffer->itype);
                        double angle = M_PI * (double) q / (2.0 * (double) n);
                        double c = cos(angle);
                        double s = sin(angle);
                        double rotated_im =z.im * c - z.re * s;
                        y.re = -rotated_im;
                    }
                    break;
                case CSN_DST_III:
                case CSN_DCT_III:
                case CSN_DCT_IV:
                case CSN_DST_IV:
                    return NOTOK;
            }
            slice_put(dcst_buffer->data + dst_base * dcst_buffer->itype, i, dst_stride, dcst_buffer->itype, y);
        }
    }

    return OK;
}

static int32_t dcst1d_extend_source(CSN_ARRAY *dcst_buffer, CSN_ARRAY *source_arr, int32_t axis, CSN_DCST_MODE mode) {
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
    size_t dst_stride = dcst_buffer->strides[axis];
    for (size_t linear = 0; linear < slice_count; ++linear) {
        uint32_t dst_coords[CSN_MAX_DIMS] = {0};
        uint32_t src_coords[CSN_MAX_DIMS] = {0};

        from_linear_to_coords(dst_coords, reduced_shape, linear, reduced_ndim);
        for (uint32_t i = 0, j = 0; i < source_arr->ndim; ++i) {
            src_coords[i] = (i == (uint32_t) axis) ? 0 : dst_coords[j++];
        }

        size_t src_base = from_coords_to_offset(src_coords, source_arr->strides, source_arr->ndim);
        size_t dst_base = from_coords_to_offset(src_coords, dcst_buffer->strides, source_arr->ndim);
        uint32_t n_embedded = dcst_buffer->shape[axis];
        uint32_t n_source = source_arr->shape[axis];
        for (uint32_t i = 0; i < n_source; i++) {
            CSN_COMPLEXDAT z = slice_get(source_arr->data + src_base * source_arr->itype, i, src_stride, source_arr->itype);
            switch (mode) {
                case CSN_DCT_I: {
                        slice_put(dcst_buffer->data + dst_base * dcst_buffer->itype, i, dst_stride, dcst_buffer->itype, z);
                        if (i > 0 && i < n_source - 1) {
                            slice_put(dcst_buffer->data + dst_base * dcst_buffer->itype, n_embedded - i, dst_stride, dcst_buffer->itype, z);
                        }
                    }
                    break;
                case CSN_DST_I: {
                        size_t p = i + 1;
                        slice_put(dcst_buffer->data + dst_base * dcst_buffer->itype, p, dst_stride, dcst_buffer->itype, z);
                        z.re = -z.re;
                        z.im = -z.im;
                        slice_put(dcst_buffer->data + dst_base * dcst_buffer->itype, n_embedded - p, dst_stride, dcst_buffer->itype, z);
                    }
                    break;
                case CSN_DCT_II: {
                        slice_put(dcst_buffer->data + dst_base * dcst_buffer->itype, i, dst_stride, dcst_buffer->itype, z);
                        slice_put(dcst_buffer->data + dst_base * dcst_buffer->itype, 2 * n_source - 1 - i, dst_stride, dcst_buffer->itype, z);
                    }
                    break;
                case CSN_DST_II: {
                        slice_put(dcst_buffer->data + dst_base * dcst_buffer->itype, i, dst_stride, dcst_buffer->itype, z);
                        z.re = -z.re;
                        z.im = -z.im;
                        slice_put(dcst_buffer->data + dst_base * dcst_buffer->itype, n_embedded - 1 - i, dst_stride, dcst_buffer->itype, z);
                    }
                    break;
                case CSN_DST_III:
                case CSN_DCT_III:
                case CSN_DCT_IV:
                case CSN_DST_IV:
                    return NOTOK;
            }
        }
    }

    return OK;
}

static int32_t dcst1d_helper(CSOUND *csound, CSN_DCST *p, CSN_DCST_MODE dcst_mode, CSN_FFT_MODE fft_mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;

    if (dcst_mode == CSN_DCT_III || dcst_mode == CSN_DCT_IV || dcst_mode == CSN_DST_III || dcst_mode == CSN_DST_IV) {
        return csound->InitError(csound, "[csnarray] DCT/DST mode not yet implemented");
    }

    ITEM_TYPE itype = CSN_REAL;

    int32_t res = OK;
    const char *err = NULL;
    MYFLT *temp_buffer = NULL;
    void *fft_setup = NULL;

    csound->LockMutex(reg->mutex);
    CSN_ARRAY *source_arr = NULL;
    uint32_t axis = 0;
    res = fft_body(csound, NULL, reg, &source_arr, p->axis, &axis, source_handle, fft_mode);
    if (res != OK) goto done;

    size_t source_size = source_arr->shape[axis];
    int32_t dcst_fft_size = get_dct_size(source_size, dcst_mode);
    /* Any length goes, as scipy allows. The extension is transformed by
       csound->RealFFT, whose non-power-of-two branch is exact but wants an
       even size -- which every one of the four extensions yields by
       construction: 2N for the type-II pair, 2(N-1) for DCT-I, 2(N+1) for
       DST-I. A ragged factorization costs time, not accuracy, and the branch
       allocates its own scratch per call, so a k-rate transform over a length
       that is not a power of two does touch the allocator. */
    if (!IS_VALID_FFT_SIZE((double) dcst_fft_size) || dcst_fft_size < 2 || (dcst_fft_size & 1)) {
        res = csound->InitError(csound, "[csnarray] Length %zu is too short for this transform", source_size);
        goto done;
    }

    uint32_t dcst_shape[CSN_MAX_DIMS] = {0};
    memcpy(dcst_shape, source_arr->shape, sizeof(dcst_shape));
    dcst_shape[axis] = (uint32_t) dcst_fft_size;
    /* A second init pass would otherwise drop the previous payload on the
       floor: allocate_array overwrites ->data without looking at it. */
    FREE_CSNARRDATA(csound, &p->dcst_extended);
    if (allocate_array(csound, &p->dcst_extended, source_arr->ndim, dcst_shape, 0, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
        goto done;
    }

    if (dcst1d_extend_source(&p->dcst_extended, source_arr, axis, dcst_mode) != OK) {
        res = csound->InitError(csound, "[csnarray] DCT/DST mode not yet implemented");
        goto done;
    }

    uint32_t fft_ndim;
    uint32_t fft_shape[CSN_MAX_DIMS] = {0};
    size_t out_size = 0;
    size_t work_size = 0;
    fft_assign_layout(&out_size, &work_size, &fft_ndim, fft_shape, &p->dcst_extended, (uint32_t) dcst_fft_size, fft_mode, axis);

    temp_buffer = csound->Calloc(csound, sizeof(MYFLT) * work_size);
    if (temp_buffer == NULL) {
        res = csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
        goto done;
    }

    if (fft_mode == CSNRFFT) {
        fft_setup = csound->RealFFTSetup(csound, dcst_fft_size, FFT_FWD);
    }

    FREE_CSNARRDATA(csound, &p->fft_buffer);
    if (allocate_array(csound, &p->fft_buffer, fft_ndim, fft_shape, 0, CSN_COMPLEX) != OK) {
        res = csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
        goto done;
    }

    CSN_ARRAY *fft_buffer = &p->fft_buffer;
    fft_assign_value(csound, fft_setup, fft_buffer, &p->dcst_extended, temp_buffer, (uint32_t) dcst_fft_size, work_size, out_size, axis, fft_mode);

    uint32_t new_ndim = source_arr->ndim;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    memcpy(new_shape, source_arr->shape, sizeof(new_shape));
    if (create_csnarray_locked(csound, reg, &p->h, new_ndim, new_shape, &p->array, p->handle, &source_handle, 1U, &err, itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    if (dcst1d_from_fft_assign_value(p->array, fft_buffer, dcst_mode, axis) != OK) {
        res = csound->InitError(csound, "[csnarray] DCT/DST mode not yet implemented");
        goto done;
    }

    p->buffer.scratch = temp_buffer;
    p->buffer.scratch_capacity = work_size;
    p->k_data_fft.fft_setup = fft_setup;
    p->k_data_fft.nfft = (size_t) dcst_fft_size;
    p->k_data_fft.buffer_out_size = out_size;
    p->k_data_fft.buffer_work_size = work_size;
    p->k_data.prev_axis_u = axis;
    p->source_axis_len = source_arr->shape[axis];
    p->is_published = false;
    set_array_version(&p->k_data.prev_output_version, &p->array->version);
    set_array_version(&p->k_data.prev_source_version, &source_arr->version);
    SET_KDATA_BEGIN(p, reg);

done:
    if (res != OK) if (temp_buffer != NULL) csound->Free(csound, temp_buffer);
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t dcst1d_k_helper(CSOUND *csound, CSN_DCST *p, CSN_DCST_MODE dcst_mode, CSN_FFT_MODE fft_mode) {
    CSN_REGISTRY *reg = p->k_data.registry;
    uint32_t owned_handle = p->k_data.owned_handle;
    CHECK_REG_HANDLE(csound, &p->h, reg, owned_handle);

    uint32_t source_handle = p->source_handle->id;

    ITEM_TYPE itype = CSN_REAL;

    int32_t res = OK;
    const char *err = NULL;
    CHECK_KTRIG(p->trig);

    size_t fft_size = p->k_data_fft.nfft;
    void *fft_setup = p->k_data_fft.fft_setup;
    size_t dcst_fft_size = fft_size;

    csound->LockMutex(reg->mutex);
    CSN_ARRAY *source_arr = NULL;
    uint32_t axis = 0;
    res = fft_body(csound, &p->h, reg, &source_arr, p->axis, &axis, source_handle, fft_mode);
    if (res != OK) goto done;

    if (p->is_published) {
        bool is_same_source = is_same_array_version(&p->k_data.prev_source_version, &source_arr->version);
        bool is_same_result = false;
        CSN_SLOT *slot = get_slot(reg, owned_handle);
        if (slot != NULL) {
            is_same_result = is_same_array_version(&p->k_data.prev_output_version, &slot->array->version);
        }

        if (is_same_source && is_same_result) {
            p->handle->id = owned_handle;
            goto done;
        }
    }

    /* dcst_extended and fft_buffer were sized at init for one source layout and
       cannot be grown here: reallocating on the performance path is what the
       rt-lock model forbids. Every extent has to match, not just the
       transformed one -- a second axis that grows walks off the end of
       dcst_extended just as surely. */
    if (source_arr->ndim != p->dcst_extended.ndim || axis != p->k_data.prev_axis_u) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Source is now %u-dimensional on axis %u, but the transform was set up for %u-dimensional on axis %u", source_arr->ndim, axis, p->dcst_extended.ndim, p->k_data.prev_axis_u);
        goto done;
    }
    for (uint32_t i = 0; i < source_arr->ndim; i++) {
        uint32_t expected = (i == axis) ? p->source_axis_len : p->dcst_extended.shape[i];
        if (source_arr->shape[i] != expected) {
            res = csn_locked_perf_error(csound, &p->h, "[csnarray] Extent %u of the source is %u, but the transform was set up for %u", i, source_arr->shape[i], expected);
            goto done;
        }
    }

    if (dcst1d_extend_source(&p->dcst_extended, source_arr, axis, dcst_mode) != OK) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] DCT/DST mode not yet implemented");
        goto done;
    }

    uint32_t fft_ndim;
    uint32_t fft_shape[CSN_MAX_DIMS] = {0};
    size_t out_size = 0;
    size_t work_size = 0;
    fft_assign_layout(&out_size, &work_size, &fft_ndim, fft_shape, &p->dcst_extended, (uint32_t) dcst_fft_size, fft_mode, axis);

    CSN_ARRAY *fft_buffer = &p->fft_buffer;
    MYFLT *temp_buffer = (MYFLT *) p->buffer.scratch;
    fft_assign_value(csound, fft_setup, fft_buffer, &p->dcst_extended, temp_buffer, (uint32_t) dcst_fft_size, work_size, out_size, axis, fft_mode);

    uint32_t new_ndim = source_arr->ndim;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    memcpy(new_shape, source_arr->shape, sizeof(new_shape));
    size_t output_size = 0;
    if (get_array_size_from_shape(&output_size, new_ndim, new_shape) != OK) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] FFT output exceeds the maximum element count");
        goto done;
    }

    CSN_ARRAY *arr = NULL;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, new_ndim, new_shape, output_size, CSN_REAL, err);
    if (res != OK) goto done;
    p->array = arr;

    if (dcst1d_from_fft_assign_value(p->array, fft_buffer, dcst_mode, axis) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] DCT/DST mode not yet implemented");
    }

    p->k_data.prev_axis_u = axis;
    set_array_version(&p->k_data.prev_source_version, &source_arr->version);
    set_array_version(&p->k_data.prev_output_version, &p->array->version);
    SET_KDATA_END(p, p->array->shape, p->array->ndim, itype);
    p->is_published = true;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_dct_one(CSOUND *csound, CSN_DCST *p) {
    return dcst1d_helper(csound, p, CSN_DCT_I, CSNRFFT);
}

int32_t csnarray_dct_two(CSOUND *csound, CSN_DCST *p) {
    return dcst1d_helper(csound, p, CSN_DCT_II, CSNRFFT);
}

int32_t csnarray_dst_one(CSOUND *csound, CSN_DCST *p) {
    return dcst1d_helper(csound, p, CSN_DST_I, CSNRFFT);
}

int32_t csnarray_dst_two(CSOUND *csound, CSN_DCST *p) {
    return dcst1d_helper(csound, p, CSN_DST_II, CSNRFFT);
}

int32_t csnarray_dct_one_k(CSOUND *csound, CSN_DCST *p) {
    return dcst1d_k_helper(csound, p, CSN_DCT_I, CSNRFFT);
}

int32_t csnarray_dct_two_k(CSOUND *csound, CSN_DCST *p) {
    return dcst1d_k_helper(csound, p, CSN_DCT_II, CSNRFFT);
}

int32_t csnarray_dst_one_k(CSOUND *csound, CSN_DCST *p) {
    return dcst1d_k_helper(csound, p, CSN_DST_I, CSNRFFT);
}

int32_t csnarray_dst_two_k(CSOUND *csound, CSN_DCST *p) {
    return dcst1d_k_helper(csound, p, CSN_DST_II, CSNRFFT);
}

int32_t csnarray_dcst_deinit(CSOUND *csound, CSN_DCST *p) {
    deinit_scratch(csound, &p->buffer);
    FREE_CSNARRDATA(csound, &p->dcst_extended);
    FREE_CSNARRDATA(csound, &p->fft_buffer);
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}


// MFCC

#define HZ_TO_MEL(hz) (2595.0 * log10(1.0 + (hz) / 700.0))
#define MEL_TO_HZ(mel) (700.0 * (pow(10.0, (mel) / 2595.0) - 1.0))
/* Centre frequency of one-sided FFT bin k. Bin nfft/2 is Nyquist. */
#define BIN_TO_HZ(bin, nfft, sr) ((double) (bin) * (sr) / (double) (nfft))

/* Owns every weight vector as well as the band array, and clears the caller's
   pointer so a second deinit pass is a no-op. The bank is built once at init
   and read on every k-pass, so releasing it is the deinit's job. */
static void mel_filterbank_free(CSOUND *csound, CSN_MEL_FILTER **fbank, uint32_t nmels) {
    CSN_MEL_FILTER *fb = *fbank;
    if (fb == NULL) return;
    for (uint32_t i = 0; i < nmels; i++) {
        if (fb[i].weights != NULL) csound->Free(csound, fb[i].weights);
    }
    csound->Free(csound, fb);
    *fbank = NULL;
}

/* Triangular mel filterbank over the bins of a one-sided spectrum.

   The triangles are evaluated at each bin's own centre frequency rather than
   built from rounded bin edges: a band narrower than the bin spacing still
   picks up fractional weight from its neighbours instead of collapsing to
   nothing, which is what quantising the edges to integers does to the low
   bands whenever nmels is large relative to nfft.

   slaney_norm scales each band to unit area in frequency. Without it sum(w)
   grows with the band width -- roughly 20x from the lowest band to the
   highest at nmels=40, nfft=1024 -- which shows up as a fixed tilt across the
   log-mel spectrum and lands in the first MFCC coefficients. */
static int32_t mel_filterbank(CSOUND *csound, OPDS *perf_h, CSN_MEL_FILTER **fbank, uint32_t nmels, double fmin, double fmax, double sr, uint32_t nfft, bool slaney_norm) {
    if (nmels == 0) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] mel filterbank needs at least one band");
    }
    if (!IS_VALID_VALUE_GT_ZERO(sr)) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] mel filterbank needs a positive sample rate");
    }
    if (nfft < 2U) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] mel filterbank needs an FFT size of at least 2");
    }
    if (!isfinite(fmin) || !isfinite(fmax) || fmin < 0.0 || fmax <= fmin) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] mel filterbank needs 0 <= fmin < fmax");
    }
    if (fmax > sr / 2.0) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] mel filterbank fmax %g is above the Nyquist frequency %g", fmax, sr / 2.0);
    }

    uint32_t nbins = nfft / 2U;
    size_t npoints = (size_t) nmels + 2U;

    double *edges = csound->Calloc(csound, sizeof(double) * npoints);
    if (edges == NULL) return NOTOK;

    double mmin = HZ_TO_MEL(fmin);
    double mmax = HZ_TO_MEL(fmax);
    double step = (mmax - mmin) / (double) (nmels + 1);
    for (size_t i = 0; i < npoints; i++) {
        edges[i] = MEL_TO_HZ(mmin + (double) i * step);
    }

    CSN_MEL_FILTER *fb = csound->Calloc(csound, sizeof(CSN_MEL_FILTER) * (size_t) nmels);
    if (fb == NULL) {
        csound->Free(csound, edges);
        return NOTOK;
    }

    uint32_t empty = 0;
    for (uint32_t i = 0; i < nmels; i++) {
        double lo = edges[i];
        double mid = edges[i + 1];
        double hi = edges[i + 2];
        if (!(lo < mid && mid < hi)) {
            empty++;
            continue;
        }

        /* Bins strictly inside (lo, hi): the triangle is zero at both edges,
           so a bin sitting exactly on one contributes nothing. */
        double first_f = floor(lo * (double) nfft / sr) + 1.0;
        double last_f = ceil(hi * (double) nfft / sr) - 1.0;
        if (first_f < 0.0) first_f = 0.0;
        if (last_f > (double) nbins) last_f = (double) nbins;
        if (last_f < first_f) {
            empty++;
            continue;
        }

        uint32_t first = (uint32_t) first_f;
        uint32_t count = (uint32_t) (last_f - first_f) + 1U;

        double *weights = csound->Calloc(csound, sizeof(double) * (size_t) count);
        if (weights == NULL) {
            mel_filterbank_free(csound, &fb, nmels);
            csound->Free(csound, edges);
            return NOTOK;
        }

        double scale = slaney_norm ? 2.0 / (hi - lo) : 1.0;
        uint32_t nonzero = 0;
        for (uint32_t j = 0; j < count; j++) {
            double freq = BIN_TO_HZ(first + j, nfft, sr);
            double rise = (freq - lo) / (mid - lo);
            double fall = (hi - freq) / (hi - mid);
            double w = rise < fall ? rise : fall;
            if (w <= 0.0) {
                weights[j] = 0.0;
                continue;
            }
            weights[j] = w * scale;
            nonzero++;
        }

        if (nonzero == 0) {
            csound->Free(csound, weights);
            empty++;
            continue;
        }

        fb[i].first_bin = first;
        fb[i].count = count;
        fb[i].weights = weights;
    }

    csound->Free(csound, edges);

    if (empty > 0) {
        /* Left in place rather than refused: the band reads as the log floor,
           which is legible once it is announced. */
        csound->Message(csound, "[csnarray] WARNING: %u of %u mel bands are empty at nfft %u and sr %g: raise the FFT size or lower the band count\n", empty, nmels, nfft, sr);
    }

    *fbank = fb;
    return OK;
}

static int32_t mel_apply_filter_bank(CSOUND *csound, OPDS *perf_h, CSN_ARRAY *out_log_mel, CSN_ARRAY *stft, CSN_MEL_FILTER *fbank, uint32_t nmels) {
    if (stft->ndim != 2U || stft->itype != CSN_COMPLEX) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] mel filterbank needs a two-dimensional complex spectrogram");
    }
    if (out_log_mel->ndim != 2U || out_log_mel->itype != CSN_REAL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] mel filterbank writes a two-dimensional real array");
    }

    uint32_t nbins = stft->shape[0];
    uint32_t nframes = stft->shape[1];
    if (out_log_mel->shape[0] != nmels || out_log_mel->shape[1] != nframes) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] mel output is %ux%u but %u bands over %u frames were requested", out_log_mel->shape[0], out_log_mel->shape[1], nmels, nframes);
    }

    /* Nothing else ties the FFT size the bank was built for to the one the
       spectrogram was produced with, and a mismatch reads past the last bin. */
    for (uint32_t mel = 0; mel < nmels; mel++) {
        if ((size_t) fbank[mel].first_bin + fbank[mel].count > (size_t) nbins) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] mel band %u reaches bin %zu but the spectrogram holds %u: the filterbank was built for a different FFT size", mel, (size_t) fbank[mel].first_bin + fbank[mel].count - 1U, nbins);
        }
    }

    size_t src_stride = stft->strides[0];
    size_t dst_stride = out_log_mel->strides[0];
    for (uint32_t frame = 0; frame < nframes; frame++) {
        uint32_t src_coords[2] = { 0U, frame };
        uint32_t dst_coords[2] = { 0U, frame };
        size_t src_base = from_coords_to_offset(src_coords, stft->strides, 2U);
        size_t dst_base = from_coords_to_offset(dst_coords, out_log_mel->strides, 2U);

        for (uint32_t mel = 0; mel < nmels; mel++) {
            const CSN_MEL_FILTER *filter = &fbank[mel];
            double sum = 0.0;
            for (uint32_t j = 0; j < filter->count; j++) {
                CSN_COMPLEXDAT z = slice_get(stft->data + src_base * stft->itype, (size_t) filter->first_bin + j, src_stride, stft->itype);
                sum += (z.re * z.re + z.im * z.im) * filter->weights[j];
            }
            CSN_COMPLEXDAT y = { log(fmax(sum, 1.0e-12)), 0.0 };
            slice_put(out_log_mel->data + dst_base * out_log_mel->itype, mel, dst_stride, out_log_mel->itype, y);
        }
    }

    return OK;
}

/* Cepstral stage: a DCT over the mel axis, held as an explicit
   mfcc_count-by-mel_count table rather than routed through csndct.

   At these sizes the direct product is the cheaper transform -- 0.14 us
   against 1.2 us for the FFT route at 40 bands, measured -- it needs no
   scratch and no allocator on the per-frame path, and it places no
   arithmetic condition on the band count. The FFT route only starts winning
   somewhere past a hundred bands, which no filterbank reaches.

   Rows follow the scipy definitions with norm=None, so a column of this
   times a log-mel frame equals scipy.fft.dct of that frame. */
static int32_t mfcc_dct_matrix(CSOUND *csound, double **matrix, uint32_t ncoef, uint32_t nmels, uint32_t dct_type) {
    if (dct_type == 1U && nmels < 2U) {
        return csound->InitError(csound, "[csnarray] DCT-I over the mel axis needs at least two bands");
    }

    double *table = csound->Calloc(csound, sizeof(double) * (size_t) ncoef * (size_t) nmels);
    if (table == NULL) return NOTOK;

    for (uint32_t k = 0; k < ncoef; k++) {
        double *row = table + (size_t) k * (size_t) nmels;
        if (dct_type == 1U) {
            row[0] = 1.0;
            row[nmels - 1U] = (k & 1U) ? -1.0 : 1.0;
            for (uint32_t n = 1U; n + 1U < nmels; n++) {
                row[n] = 2.0 * cos(M_PI * (double) k * (double) n / (double) (nmels - 1U));
            }
        } else {
            for (uint32_t n = 0; n < nmels; n++) {
                row[n] = 2.0 * cos(M_PI * (double) k * (2.0 * (double) n + 1.0) / (2.0 * (double) nmels));
            }
        }
    }

    *matrix = table;
    return OK;
}

static int32_t mfcc_apply_dct(CSOUND *csound, OPDS *perf_h, CSN_ARRAY *out, const CSN_ARRAY *log_mel, const double *matrix, uint32_t ncoef, uint32_t nmels) {
    if (log_mel->shape[0] != nmels || out->shape[0] != ncoef || out->shape[1] != log_mel->shape[1]) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] cepstral stage expects %u bands and %u coefficients over %u frames", nmels, ncoef, log_mel->shape[1]);
    }

    uint32_t nframes = log_mel->shape[1];
    size_t src_stride = log_mel->strides[0];
    size_t dst_stride = out->strides[0];

    for (uint32_t frame = 0; frame < nframes; frame++) {
        uint32_t src_coords[2] = { 0U, frame };
        uint32_t dst_coords[2] = { 0U, frame };
        size_t src_base = from_coords_to_offset(src_coords, log_mel->strides, 2U);
        size_t dst_base = from_coords_to_offset(dst_coords, out->strides, 2U);

        for (uint32_t k = 0; k < ncoef; k++) {
            const double *row = matrix + (size_t) k * (size_t) nmels;
            double sum = 0.0;
            for (uint32_t n = 0; n < nmels; n++) {
                CSN_COMPLEXDAT z = slice_get(log_mel->data + src_base * log_mel->itype, n, src_stride, log_mel->itype);
                sum += row[n] * z.re;
            }
            CSN_COMPLEXDAT y = { sum, 0.0 };
            slice_put(out->data + dst_base * out->itype, k, dst_stride, out->itype, y);
        }
    }

    return OK;
}

int32_t csnarray_mfcc_deinit(CSOUND *csound, CSN_MFCC *p) {
    mel_filterbank_free(csound, &p->fbank, p->mel_count);
    if (p->dct_matrix != NULL) {
        csound->Free(csound, p->dct_matrix);
        p->dct_matrix = NULL;
    }
    FREE_CSNARRDATA(csound, &p->log_mel);
    deinit_scratch(csound, &p->stft_buffer.buffer);
    deinit_scratch(csound, &p->stft_buffer.window);
    FREE_CSNARRDATA(csound, &p->stft_buffer.stft);
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

int32_t csnarray_mfcc(CSOUND *csound, CSN_MFCC *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    const char *err = NULL;

    p->stft_buffer.winsize = *p->winsize;
    p->stft_buffer.hopsize = *p->hopsize;
    p->stft_buffer.window_type = *p->wintype;

    double nmfcc_value = (double) *p->nmfcc;
    if (!IS_VALID_NMFCC(nmfcc_value)) {
        return csound->InitError(csound, "[csnarray] Invalid number of mfcc coefficients");
    }
    p->mfcc_count = (uint32_t) nmfcc_value;
    p->mel_count = p->mfcc_count;

    double lowf = (double) *p->lowf;
    double highf = (double) *p->highf;
    if (!IS_VALID_VALUE(lowf) || !IS_VALID_VALUE(highf) || lowf >= highf) {
        return csound->InitError(csound, "[csnarray] flow and fhigh should be a valid values with flow less than fhigh");
    }
    double dct_type_value = (double) *p->dct_type;
    if (!IS_VALID_DCT_MODE(dct_type_value)) {
        return csound->InitError(csound, "[csnarray] DCT mode should be 1 or 2");
    }
    uint32_t dct_type = (uint32_t) dct_type_value;

    double sr = (double) *p->sr;
    if (!IS_VALID_SR(sr)) {
        return csound->InitError(csound, "[csnarray] Invalid sample rate");
    }

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = slot->array;
    if (source_arr->ndim != 1U) {
        res = csound->InitError(csound, "[csnarray] MFCC requires 1-D array");
        goto done;
    }
    res = GET_STFT_INIT(csound, NULL, source_arr, &p->stft_buffer);
    if (res != OK) goto done;
    res = GET_STFT(csound, source_arr, &p->stft_buffer);
    if (res != OK) goto done;

    uint32_t nfft = (uint32_t) p->stft_buffer.winsize;
    res = mel_filterbank(csound, NULL, &p->fbank, p->mel_count, lowf, highf, sr, nfft, true);
    if (res != OK) goto done;

    uint32_t new_ndim = 2U;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    new_shape[0] = p->mfcc_count;
    new_shape[1] = p->stft_buffer.stft.shape[1];
    if (create_csnarray_locked(csound, reg, &p->h, new_ndim, new_shape, &p->array, p->handle, &source_handle, 1U, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    uint32_t log_mel_shape[CSN_MAX_DIMS] = {0};
    log_mel_shape[0] = p->mel_count;
    log_mel_shape[1] = p->stft_buffer.stft.shape[1];
    FREE_CSNARRDATA(csound, &p->log_mel);
    if (allocate_array(csound, &p->log_mel, 2U, log_mel_shape, 0, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
        goto done;
    }

    res = mel_apply_filter_bank(csound, NULL, &p->log_mel, &p->stft_buffer.stft, p->fbank, p->mel_count);
    if (res != OK) goto done;

    if (p->dct_matrix != NULL) {
        csound->Free(csound, p->dct_matrix);
        p->dct_matrix = NULL;
    }
    res = mfcc_dct_matrix(csound, &p->dct_matrix, p->mfcc_count, p->mel_count, dct_type);
    if (res != OK) goto done;

    res = mfcc_apply_dct(csound, NULL, p->array, &p->log_mel, p->dct_matrix, p->mfcc_count, p->mel_count);
    if (res != OK) goto done;

    SET_KDATA_BEGIN(p, reg);
    set_array_version(&p->k_data.prev_output_version, &p->array->version);
    set_array_version(&p->k_data.prev_source_version, &source_arr->version);
    p->k_data.prev_size = source_arr->size;
    p->is_published = false;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_mfcc_k(CSOUND *csound, CSN_MFCC *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    uint32_t owned_handle = p->k_data.owned_handle;
    CHECK_REG_HANDLE(csound, &p->h, reg, owned_handle);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    const char *err = NULL;

    CHECK_KTRIG(p->trig);

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = slot->array;
    if (source_arr->ndim != 1U) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] MFCC requires 1-D array");
        goto done;
    }

    if (source_arr->shape[0] != p->k_data.prev_size) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Extent %u of the source is %u, but the transform was set up for %zu", 0, source_arr->shape[0], p->k_data.prev_size);
        goto done;
    }

    if (p->is_published) {
        bool is_same_source = is_same_array_version(&p->k_data.prev_source_version, &source_arr->version);
        bool is_same_result = false;
        CSN_SLOT *res_slot = get_slot(reg, owned_handle);
        if (res_slot != NULL) {
            is_same_result = is_same_array_version(&p->k_data.prev_output_version, &res_slot->array->version);
        }

        if (is_same_source && is_same_result) {
            p->handle->id = owned_handle;
            goto done;
        }
    }

    res = GET_STFT(csound, source_arr, &p->stft_buffer);
    if (res != OK) goto done;

    uint32_t new_ndim = 2U;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    new_shape[0] = p->mfcc_count;
    new_shape[1] = p->stft_buffer.stft.shape[1];
    size_t req_size = 0;
    if (get_array_size_from_shape(&req_size, new_ndim, new_shape) != OK) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
        goto done;
    }

    CSN_ARRAY *arr = NULL;
    size_t logical_size = req_size;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, new_ndim, new_shape, logical_size, CSN_REAL, err);
    if (res != OK) goto done;
    p->array = arr;

    res = mel_apply_filter_bank(csound, &p->h, &p->log_mel, &p->stft_buffer.stft, p->fbank, p->mel_count);
    if (res != OK) goto done;

    res = mfcc_apply_dct(csound, &p->h, p->array, &p->log_mel, p->dct_matrix, p->mfcc_count, p->mel_count);
    if (res != OK) goto done;

    SET_KDATA_END(p, new_shape, new_ndim, CSN_REAL);
    set_array_version(&p->k_data.prev_output_version, &p->array->version);
    set_array_version(&p->k_data.prev_source_version, &source_arr->version);
    p->is_published = true;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_mfbank_helper(CSOUND *csound, CSN_MFCC_FBANK *p, bool is_log) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    const char *err = NULL;

    /* Power of two, matching what stft_validate_params accepts: a bank built
       for any other size cannot be applied to a spectrogram csnum produces. */
    double nfft_value = (double) *p->nfft;
    if (!IS_VALID_FFT_SIZE(nfft_value) || !IS_POWER_OF_TWO((uint32_t) nfft_value)) {
        return csound->InitError(csound, "[csnarray] FFT size must be a valid power of two value");
    }
    uint32_t nfft = (uint32_t) nfft_value;

    double nmfcc_value = (double) *p->nmfcc;
    if (!IS_VALID_NMFCC(nmfcc_value)) {
        return csound->InitError(csound, "[csnarray] Invalid number of mfcc coefficients");
    }
    uint32_t nmfcc = (uint32_t) *p->nmfcc;

    double lowf = (double) *p->lowf;
    double highf = (double) *p->highf;
    if (!IS_VALID_VALUE(lowf) || !IS_VALID_VALUE(highf) || lowf >= highf) {
        return csound->InitError(csound, "[csnarray] flow and fhigh should be a valid values with flow less than fhigh");
    }

    double sr = (double) *p->sr;
    if (!IS_VALID_SR(sr)) {
        return csound->InitError(csound, "[csnarray] Invalid sample rate");
    }

    double has_norm = (double) *p->slaney_norm;
    if (!IS_VALID_ZERO_ONE(has_norm)) {
        return csound->InitError(csound, "[csnarray] Slaney normalisation flag must be 0 or 1");
    }
    bool slaney_norm = (bool) has_norm;

    csound->LockMutex(reg->mutex);

    /* One row per band over the bins of a one-sided spectrum, which is the
       shape librosa.filters.mel returns. Adjacent bands always overlap -- band
       i spans the edges (i, i+2) and band i+1 the edges (i+1, i+3) -- so they
       cannot share a single row without erasing each other. */
    uint32_t nbins = nfft / 2U + 1U;
    uint32_t new_ndim = 2U;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    new_shape[0] = nmfcc;
    new_shape[1] = nbins;
    if (create_csnarray_locked(csound, reg, &p->h, new_ndim, new_shape, &p->array, p->handle, NULL, 0, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_MEL_FILTER *fbank = NULL;
    res = mel_filterbank(csound, NULL, &fbank, nmfcc, lowf, highf, sr, nfft, slaney_norm);
    if (res != OK) goto done;

    size_t bin_stride = p->array->strides[1];
    for (uint32_t i = 0; i < nmfcc; i++) {
        const CSN_MEL_FILTER *filter = &fbank[i];
        uint32_t row_coords[2] = { i, 0U };
        size_t row_base = from_coords_to_offset(row_coords, p->array->strides, 2U);
        double *row = p->array->data + row_base * p->array->itype;

        if (is_log) {
            for (uint32_t b = 0; b < nbins; b++) {
                double w = 0.0;
                if (b >= filter->first_bin && b - filter->first_bin < filter->count) {
                    w = filter->weights[b - filter->first_bin];
                }
                CSN_COMPLEXDAT y = { log(fmax(w, 1.0e-12)), 0.0 };
                slice_put(row, b, bin_stride, p->array->itype, y);
            }
        } else {
            for (uint32_t j = 0; j < filter->count; j++) {
                CSN_COMPLEXDAT y = { filter->weights[j], 0.0 };
                slice_put(row, (size_t) filter->first_bin + j, bin_stride, p->array->itype, y);
            }
        }
    }

    mel_filterbank_free(csound, &fbank, nmfcc);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_mfbank_deinit(CSOUND *csound, CSN_MFCC_FBANK *p) {
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

int32_t csnarray_mfbank(CSOUND *csound, CSN_MFCC_FBANK *p) {
    return csnarray_mfbank_helper(csound, p, false);
}

int32_t csnarray_mlogfbank(CSOUND *csound, CSN_MFCC_FBANK *p) {
    return csnarray_mfbank_helper(csound, p, true);
}
