#include "csnregistry.h"
#include "csnum.h"
#include <math.h>
#include <stdbool.h>
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
    return isfinite(value) && !isnan(value) && trunc(value) == value && value >= 0.0 && value <= (double) NUMBER_OF_STFT_WINDOWS;
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
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &fft_buffer, &p->k_data, NULL,
                              new_ndim, new_shape, output_size, otype, err);
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

    if (p->window_sum.scratch_capacity < array_x->size) {
        double *grown = csound->ReAlloc(csound, p->window_sum.scratch, sizeof(double) * array_x->size);
        if (grown == NULL) {
            res = csn_locked_perf_error(csound, &p->h, "[csnarray] Internal error: memory allocation failed");
            goto done;
        }
        p->window_sum.scratch = grown;
        p->window_sum.scratch_capacity = array_x->size;
    }

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

static void convolve_get_size_and_edges_offset(size_t *size_result, int64_t *start_offset, CSN_ARRAY *x, CSN_ARRAY *h, int32_t axis, CSN_EDGES_MODE mode) {
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

static void convolve_get_shape_and_edges_offset_ndims(uint32_t *new_shape, int64_t *start_offset, CSN_ARRAY *x, CSN_ARRAY *h, CSN_EDGES_MODE mode) {
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
    convolve_get_size_and_edges_offset(&size_result, &start_offset, source_arr_a, source_arr_b, axis, edges_mode);

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
        res = csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle_a);
        goto done;
    }
    CSN_SLOT *slot_b = get_slot(reg, source_handle_b);
    if (slot_b == NULL) {
        res = csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle_b);
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
    convolve_get_size_and_edges_offset(&size_result, &start_offset, source_arr_a, source_arr_b, axis, edges_mode);

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
    convolve_get_shape_and_edges_offset_ndims(new_shape, start_offset, source_arr_a, source_arr_b, edges_mode);

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
        res = csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle_a);
        goto done;
    }
    CSN_SLOT *slot_b = get_slot(reg, source_handle_b);
    if (slot_b == NULL) {
        res = csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle_b);
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
    convolve_get_shape_and_edges_offset_ndims(new_shape, start_offset, source_arr_a, source_arr_b, edges_mode);

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
