/* Opcode implementations for the signal family.
   The public opcode inventory remains centralized in csnum.c. */
#include "csnum_internal.h"
#include "csnregistry.h"
#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "arrays.h"

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

void get_window_function(double *win, uint32_t wsize, CSN_WINDOW_MODE mode, double beta) {
    if (wsize == 1U) {
        win[0] = 1.0;
    } else if (wsize > 1U) {
        for (size_t n = 0; n < (size_t) wsize; n++) {
            double value = 0.0;
            double fac = (double) n / (double) (wsize - 1);
            switch (mode) {
                case W_RECT:
                    value = 1.0;
                    break;
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
            win[n] = value;
        }
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
    get_window_function(arr->data, wsize,  mode, beta);

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

int32_t csnarray_window_function_k_init(CSOUND *csound, CSN_WINDOW *p) {
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
                case W_RECT:
                    value = 1.0;
                    break;
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

