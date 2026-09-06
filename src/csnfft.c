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

int32_t csnarray_stft(CSOUND *csound, CSN_STFT *p);
int32_t csnarray_istft(CSOUND *csound, CSN_ISTFT *p);
int32_t csnarray_fftfreq(CSOUND *csound, CSN_FFTTOOL *p);
int32_t csnarray_rfftfreq(CSOUND *csound, CSN_FFTTOOL *p);
int32_t csnarray_fftshift(CSOUND *csound, CSN_FFTTOOL *p);
int32_t csnarray_ifftshift(CSOUND *csound, CSN_FFTTOOL *p);
