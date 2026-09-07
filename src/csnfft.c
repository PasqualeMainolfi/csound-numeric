#include "csnregistry.h"
#include "csnum.h"
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>


static inline bool IS_POWER_OF_TWO(uint32_t n) {
    return n != 0U && (n & (n - 1U)) == 0U;
}

static inline bool IS_VALID_FFT_SIZE(double value) {
    return isfinite(value) && !isnan(value) && trunc(value) == value && value > 0.0 && value <= (double) CSN_MAX_ELEMS;
}

static inline bool IS_VALID_SR(double value) {
    return isfinite(value) && !isnan(value) && trunc(value) == value && value > 0.0 && value <= (double) UINT32_MAX;
}

static inline bool IS_VALID_STFT_WIN(double value) {
    return isfinite(value) && !isnan(value) && trunc(value) == value && value >= 0.0 && value <= (double) NUMBER_OF_STFT_WINDWS;
}

static bool IS_VALID_VALUE_GT_ZERO(double value) {
    return isfinite(value) && !isnan(value) && value > 0.0;
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

    double axis_value = (double) *axis_in;
    if (axis_value != -1.0 && !IS_VALID_AXIS(axis_value, source_ndim)) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: -1 for last axes, or finite integers 0..%u)", axis_value, source_ndim, source_ndim - 1);
    }

    *axis_out = axis_value != -1.0 ? (uint32_t) axis_value : source_ndim - 1U;
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
    new_shape[axis] = (uint32_t) *out_size;
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

done:
    if (temp_buffer != NULL) csound->Free(csound, temp_buffer);
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_fft(CSOUND *csound, CSN_FFT *p) {
    return csnarray_fft_helper(csound, p, CSNFFT);
}

int32_t csnarray_rfft(CSOUND *csound, CSN_FFT *p) {
    return csnarray_fft_helper(csound, p, CSNRFFT);
}

static int32_t csnarray_ifft_helper(CSOUND *csound, CSN_FFT *p, CSN_FFT_MODE mode) {
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

done:
    if (temp_buffer != NULL) csound->Free(csound, temp_buffer);
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
    new_shape_f[0] = nrows;
    new_shape_t[0] = ncols;
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
    double sr_temp = (double) *sr;
    if (!IS_VALID_SR(sr_temp)) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Invalid sr value");
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

    if (fft_mode == CSNRFFT) {
        fft_setup = csound->RealFFTSetup(csound, fft_size, FFT_FWD);
    }

    uint32_t new_dim_f_t = 1U;
    uint32_t new_dim_z = 2U;
    uint32_t new_shape_f[CSN_MAX_DIMS] = {0};
    uint32_t new_shape_t[CSN_MAX_DIMS] = {0};
    uint32_t new_shape_z[CSN_MAX_DIMS] = {0};
    size_t out_size = 0;
    size_t work_size = 0;
    stft_assign_layout(&out_size, &work_size, new_shape_f, new_shape_t, new_shape_z, source_arr, (uint32_t) fft_size, (uint32_t) overlap_size, fft_mode);

    temp_buffer = csound->Calloc(csound, sizeof(MYFLT) * work_size);
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

done:
    if (temp_buffer != NULL) csound->Free(csound, temp_buffer);
    if (win_buffer != NULL) csound->Free(csound, win_buffer);
    csound->UnlockMutex(reg->mutex);
    return res;
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
    if (ifft_mode == CSNIRFFT) {
        ifft_setup = csound->RealFFTSetup(csound, (int32_t) fft_size, FFT_INV);
    }

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

    temp_buffer = csound->Calloc(csound, sizeof(MYFLT) * work_size);
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

done:
    if (temp_buffer != NULL) csound->Free(csound, temp_buffer);
    if (win_buffer != NULL) csound->Free(csound, win_buffer);
    if (winsum_buffer != NULL) csound->Free(csound, winsum_buffer);
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

int32_t csnarray_fftfreq_deinit(CSOUND *csound, CSN_FFTFREQ *p) {
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

int32_t csnarray_fftfreq(CSOUND *csound, CSN_FFTFREQ *p) {
    return csnarray_fftfreq_helper(csound, p, CSNFFTFREQ);
}

int32_t csnarray_rfftfreq(CSOUND *csound, CSN_FFTFREQ *p) {
    return csnarray_fftfreq_helper(csound, p, CSNRFFTFREQ);
}

int32_t csnarray_fftshift_deinit(CSOUND *csound, CSN_FFTSHIFT *p) {
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
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

    CSN_ARRAY *arr = p->array;

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
    for (size_t linear = 0; linear < slice_count; ++linear) {
        uint32_t dst_coords[CSN_MAX_DIMS] = {0};
        uint32_t src_coords[CSN_MAX_DIMS] = {0};
        uint32_t slice_size = source_shape[axis];
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
