/* Opcode implementations for the signal family.
   The public opcode inventory remains centralized in csnum.c. */
#include "csnsignal.h"
#include "csnum.h"
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
                    value = 0.5 * (1.0 - cos(2.0 * CSN_PI * fac));
                    break;
                case W_HAMMING:
                    value = 0.54 - 0.46 * cos(2.0 * CSN_PI * fac);
                    break;
                case W_BARTLETT:
                    /* The peak is where fac reaches 0.5, not where n reaches
                       wsize/2: for an even wsize the integer midpoint sits past
                       half of the 0..1 sweep and the rising branch would take
                       it above 1. */
                    value = (fac <= 0.5) ? 2.0 * fac : 2.0 - 2.0 * fac;
                    break;
                case W_BLACKMAN:
                    value = 0.42 - 0.5 * cos(2.0 * CSN_PI * fac) + 0.08 * cos(4.0 * CSN_PI * fac);
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

int32_t csnarray_window_deinit(CSOUND *csound, CSN_WINDOW *p) {
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
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
                    value = 0.5 * (1.0 - cos(2.0 * CSN_PI * fac));
                    break;
                case W_HAMMING:
                    value = 0.54 - 0.46 * cos(2.0 * CSN_PI * fac);
                    break;
                case W_BARTLETT:
                    /* The peak is where fac reaches 0.5, not where n reaches
                       wsize/2: for an even wsize the integer midpoint sits past
                       half of the 0..1 sweep and the rising branch would take
                       it above 1. */
                    value = (fac <= 0.5) ? 2.0 * fac : 2.0 - 2.0 * fac;
                    break;
                case W_BLACKMAN:
                    value = 0.42 - 0.5 * cos(2.0 * CSN_PI * fac) + 0.08 * cos(4.0 * CSN_PI * fac);
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


// FILTER

static CSN_POLYDEV polyeval_and_derivative(const double *coeffs, size_t stride, CSN_COMPLEXDAT z_complex, size_t degree, ITEM_TYPE ctype) {
    CSN_POLYDEV p = {0};

    p.value = slice_get(coeffs, 0, stride, ctype);
    for (size_t i = 1; i <= degree; i++) {
        complex_prod(&p.derivative, p.derivative, z_complex);
        complex_add(&p.derivative, p.derivative, p.value);

        CSN_COMPLEXDAT c = slice_get(coeffs, i, stride, ctype);
        complex_prod(&p.value, p.value, z_complex);
        complex_add(&p.value, p.value, c);
    }

    return p;
}

int32_t csnsig_roots_deinit(CSOUND *csound, CSN_ROOTS *p) {
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

static int32_t get_roots(CSOUND *csound, CSN_ARRAY *roots, CSN_ARRAY *source_arr, CSN_AXIS_SPEC axis, uint32_t degree, CSN_COMPLEXDAT *z_buffer, CSN_COMPLEXDAT *z_new_buffer) {
    CSN_AXIS_SLICE_ITER it;
    if (AXIS_ITER_SLICE_INIT(&it, source_arr, roots, axis.index) != OK) {
        return csound->InitError(csound, "[csnarray] Internal error: incompatible roots array shapes/ranks");
    }

    size_t max_iter = CSN_ROOTS_MAX_ITER;
    while (AXIS_SLICE_ITER_NEXT(&it)) {
        const double *src_slice = source_arr->data + it.src_base * source_arr->itype;
        bool converged = false;

        CSN_COMPLEXDAT a0 = slice_get(src_slice, 0, it.src_axis_stride, source_arr->itype);
        double a0_abs = hypot(a0.re, a0.im);
        if (a0_abs == 0.0) {
            return csound->InitError(csound, "[csnarray] Leading polynomial coefficient is zero");
        }

        double r = 0.0;
        for (uint32_t i = 1; i <= degree; i++) {
            CSN_COMPLEXDAT ai = slice_get(src_slice, i, it.src_axis_stride, source_arr->itype);
            double ai_abs = hypot(ai.re, ai.im);
            r = fmax(r, fabs(ai_abs / a0_abs));
        }
        r += 1;

        for (uint32_t i = 0; i < degree; i++) {
            double phase = 2.0 * CSN_PI * ((double) i + 0.25);
            z_buffer[i].re = r * cos(phase / (double) degree);
            z_buffer[i].im = r * sin(phase / (double) degree);
        }

        for (size_t iter = 0; iter < max_iter; iter++) {

            double max_delta = 0.0;

            for (size_t i = 0; i < degree; i++) {
                CSN_POLYDEV p = polyeval_and_derivative(src_slice, it.src_axis_stride, z_buffer[i], degree, source_arr->itype);
                CSN_COMPLEXDAT q = {0};
                if (p.value.re == 0.0 && p.value.im == 0.0) {
                    z_new_buffer[i] = z_buffer[i];
                    continue;
                }
                if (complex_div(&q, p.value, p.derivative) != OK) {
                    // stationary point: nudge off it
                    z_new_buffer[i].re = z_buffer[i].re + 1.0e-6 * r;
                    z_new_buffer[i].im = z_buffer[i].im + 1.0e-6 * r;
                    max_delta = INFINITY;
                    continue;
                }
                CSN_COMPLEXDAT s = {0};

                for (size_t j = 0; j < degree; j++) {
                    if (j == i) continue;
                    CSN_COMPLEXDAT diff = {0};
                    complex_sub(&diff, z_buffer[i], z_buffer[j]);
                    CSN_COMPLEXDAT one = { .re = 1.0, .im = 0.0 };
                    CSN_COMPLEXDAT inv = {0};

                    if (complex_div(&inv, one, diff) != OK) continue;
                    complex_add(&s, s, inv);
                }

                CSN_COMPLEXDAT qs = {0};
                complex_prod(&qs, q, s);
                CSN_COMPLEXDAT den = { .re = 1.0 - qs.re, .im = -qs.im };
                CSN_COMPLEXDAT delta = q;
                if (complex_div(&delta, q, den) != OK) delta = q;
                complex_sub(&z_new_buffer[i], z_buffer[i], delta);
                double rel_delta = hypot(delta.re, delta.im) / fmax(1.0, hypot(z_new_buffer[i].re, z_new_buffer[i].im));
                if (!(rel_delta <= max_delta)) max_delta = rel_delta;
            }
            memcpy(z_buffer, z_new_buffer, sizeof(CSN_COMPLEXDAT) * degree);
            if (max_delta < 1.0e-9) {
                converged = true;
                break;
            }
        }

        if (!converged) {
            return csound->InitError(csound, "[csnarray] Roots did not converge");
        }

        for (size_t n = 0; n < degree; n++) {
            slice_put(roots->data + it.dst_base * CSN_COMPLEX, n, it.dst_axis_stride, CSN_COMPLEX, z_buffer[n]);
        }
    }

    return OK;
}

int32_t csnsig_roots(CSOUND *csound, CSN_ROOTS *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    const char *err = NULL;
    int32_t is_axis_omitted = p->INOCOUNT == 1;
    CSN_COMPLEXDAT *z_buffer = NULL;
    CSN_COMPLEXDAT *z_new_buffer = NULL;

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = slot->array;
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;
    ITEM_TYPE itype = source_arr->itype;
    CSN_AXIS_SPEC axis = csn_normalize_axis(is_axis_omitted ? NULL : p->axis, source_ndim, CSN_AXIS_DEFAULT_LAST);
    if (axis.kind == CSN_AXIS_INVALID) {
        double value = is_axis_omitted ? 0.0 : (double) *p->axis;
        res = csound->InitError(csound, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: finite integers %d..%u)", value, source_ndim, -(int32_t) source_ndim, source_ndim - 1U);
        goto done;
    }

    if (source_shape[axis.index] < 2U) {
        res = csound->InitError(csound, "[csnarray] Constant polynomial, no roots");
        goto done;
    }

    uint32_t degree = source_shape[axis.index] - 1U;

    uint32_t new_ndim = source_ndim;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    memcpy(new_shape, source_shape, sizeof(new_shape));
    new_shape[axis.index] = degree;

    z_buffer = csound->Calloc(csound, sizeof(CSN_COMPLEXDAT) * degree);
    z_new_buffer = csound->Calloc(csound, sizeof(CSN_COMPLEXDAT) * degree);
    if (z_buffer == NULL || z_new_buffer == NULL) {
        res = csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
        goto done;
    }

    if (create_csnarray_locked(csound, reg, &p->h, new_ndim, new_shape, &p->array, p->handle, &source_handle, 1U, &err, CSN_COMPLEX) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *roots = p->array;
    res = get_roots(csound, roots, source_arr, axis, degree, z_buffer, z_new_buffer);

done:
    if (z_buffer != NULL) csound->Free(csound, z_buffer);
    if (z_new_buffer != NULL) csound->Free(csound, z_new_buffer);
    csound->UnlockMutex(reg->mutex);
    return res;

}

static CSN_COMPLEXDAT zpk_get(const CSN_ARRAY *arr, size_t i) {
    return slice_get(arr->data, i, 1, arr->itype);
}

static void zpk_put(CSN_ARRAY *out, size_t *index, CSN_COMPLEXDAT z) {
    slice_put(out->data, *index, 1, CSN_COMPLEX, z);
    (*index)++;
}

// lp2bp / lp2bs: the two band roots x +- sqrt(x^2 - w0^2) of one prototype root
static void zpk_put_band_pair(CSN_ARRAY *out, size_t *index, CSN_COMPLEXDAT x, double w0) {
    CSN_COMPLEXDAT temp;
    complex_prod(&temp, x, x);
    temp.re -= w0 * w0;
    CSN_COMPLEXDAT root;
    complex_sqrt(&root, temp);
    CSN_COMPLEXDAT r1;
    CSN_COMPLEXDAT r2;
    complex_add(&r1, x, root);
    complex_sub(&r2, x, root);
    zpk_put(out, index, r1);
    zpk_put(out, index, r2);
}

/* Maps every zero (or pole) of the prototype. The map is the same for zeros and
   poles in every mode; only the zeros are padded afterwards (fill_extra_zeros). */
static int32_t map_zpk_roots(CSOUND *csound, CSN_ARRAY *out, const CSN_ARRAY *in, double w0fs, double bw, CSN_PF_DESIGN_MODE mode, const char *what) {
    size_t out_index = 0;
    for (size_t i = 0; i < in->size; i++) {
        CSN_COMPLEXDAT r = zpk_get(in, i);
        CSN_COMPLEXDAT mapped = {0};
        switch (mode) {
            case BILINEAR_ZPK: {
                    double fs2 = 2.0 * w0fs;
                    CSN_COMPLEXDAT num = { .re = fs2 + r.re, .im = r.im };
                    CSN_COMPLEXDAT den = { .re = fs2 - r.re, .im = -r.im };
                    if (complex_div(&mapped, num, den) != OK) {
                        return csound->InitError(csound, "[csnarray] A %s at 2*fs (%g) has no bilinear image", what, fs2);
                    }
                    zpk_put(out, &out_index, mapped);
                }
                break;
            case LP2LP_ZPK:
                complex_scalar_prod(&mapped, r, w0fs);
                zpk_put(out, &out_index, mapped);
                break;
            case LP2HP_ZPK: {
                    CSN_COMPLEXDAT num = { .re = w0fs, .im = 0.0 };
                    if (complex_div(&mapped, num, r) != OK) {
                        return csound->InitError(csound, "[csnarray] A prototype %s at the origin has no highpass image", what);
                    }
                    zpk_put(out, &out_index, mapped);
                }
                break;
            case LP2BP_ZPK:
                complex_scalar_prod(&mapped, r, bw * 0.5);
                zpk_put_band_pair(out, &out_index, mapped, w0fs);
                break;
            case LP2BS_ZPK: {
                    CSN_COMPLEXDAT num = { .re = bw * 0.5, .im = 0.0 };
                    if (complex_div(&mapped, num, r) != OK) {
                        return csound->InitError(csound, "[csnarray] A prototype %s at the origin has no bandstop image", what);
                    }
                    zpk_put_band_pair(out, &out_index, mapped, w0fs);
                }
                break;
            default:
                break;
        }
    }
    return OK;
}

// the zeros each transform adds for the excess of poles over zeros, after the mapped ones
static void fill_extra_zeros(CSN_ARRAY *zeros_out, size_t mapped_count, double w0fs, size_t degree, CSN_PF_DESIGN_MODE mode) {
    size_t out_index = mapped_count;
    CSN_COMPLEXDAT origin = { .re = 0.0, .im = 0.0 };
    CSN_COMPLEXDAT nyquist = { .re = -1.0, .im = 0.0 };
    CSN_COMPLEXDAT up = { .re = 0.0, .im = w0fs };
    CSN_COMPLEXDAT down = { .re = 0.0, .im = -w0fs };
    switch (mode) {
        case LP2HP_ZPK:
        case LP2BP_ZPK:
            for (size_t i = 0; i < degree; i++) zpk_put(zeros_out, &out_index, origin);
            break;
        case LP2BS_ZPK:
            for (size_t i = 0; i < degree; i++) zpk_put(zeros_out, &out_index, up);
            for (size_t i = 0; i < degree; i++) zpk_put(zeros_out, &out_index, down);
            break;
        case BILINEAR_ZPK:
            for (size_t i = 0; i < degree; i++) zpk_put(zeros_out, &out_index, nyquist);
            break;
        default:
            break;
    }
}

// real(prod(shift - z) / prod(shift - p)) over the prototype zeros and poles
static double zpk_gain_ratio(const CSN_ARRAY *zeros, const CSN_ARRAY *poles, double shift) {
    CSN_COMPLEXDAT num = { .re = 1.0, .im = 0.0 };
    for (size_t i = 0; i < zeros->size; i++) {
        CSN_COMPLEXDAT z = zpk_get(zeros, i);
        CSN_COMPLEXDAT d = { .re = shift - z.re, .im = -z.im };
        complex_prod(&num, num, d);
    }
    CSN_COMPLEXDAT den = { .re = 1.0, .im = 0.0 };
    for (size_t i = 0; i < poles->size; i++) {
        CSN_COMPLEXDAT pz = zpk_get(poles, i);
        CSN_COMPLEXDAT d = { .re = shift - pz.re, .im = -pz.im };
        complex_prod(&den, den, d);
    }
    CSN_COMPLEXDAT g = {0};
    // den is non-zero: map_zpk_roots has already refused a pole at `shift`
    complex_div(&g, num, den);
    return g.re;
}

static double get_zpk_gain(const CSN_ARRAY *zeros_arr, const CSN_ARRAY *poles_arr, double w0fs, double bw, double gain_in, size_t degree, CSN_PF_DESIGN_MODE mode) {
    switch (mode) {
        case LP2LP_ZPK:
            return gain_in * pow(w0fs, (double) degree);
        case LP2BP_ZPK:
            return gain_in * pow(bw, (double) degree);
        case LP2HP_ZPK:
        case LP2BS_ZPK:
            return gain_in * zpk_gain_ratio(zeros_arr, poles_arr, 0.0);
        case BILINEAR_ZPK:
            return gain_in * zpk_gain_ratio(zeros_arr, poles_arr, 2.0 * w0fs);
        default:
            return gain_in;
    }
}


static int32_t fdesign3_helper(CSOUND *csound, CSN_FDESIGN3 *p, CSN_PF_DESIGN_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle_a = p->source_handle_a->id;
    uint32_t source_handle_b = p->source_handle_b->id;

    double gain = (double) *p->arg_a;
    if (!IS_VALID_VALUE(gain)) {
        return csound->InitError(csound, "[csnarray] Invalid gain value");
    }

    double arg_b = (double) *p->arg_b;
    if (!IS_VALID_VALUE(arg_b) || arg_b <= 0.0) {
        if (mode == BILINEAR_ZPK) {
            return csound->InitError(csound, "[csnarray] Invalid sampling rate %g: it must be finite and greater than zero", arg_b);
        }
        return csound->InitError(csound, "[csnarray] Invalid w0 %g: it must be finite and greater than zero", arg_b);
    }

    double bw = 0.0;
    if (mode == LP2BP_ZPK || mode == LP2BS_ZPK) {
        bw = (double) *p->arg_c;
        if (!IS_VALID_VALUE(bw) || bw <= 0.0) {
            return csound->InitError(csound, "[csnarray] Invalid bandwidth %g: it must be finite and greater than zero", bw);
        }
    }

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot_a = get_slot(reg, source_handle_a);
    CSN_SLOT *slot_b = get_slot(reg, source_handle_b);
    if (slot_a == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle_a);
        goto done;
    }
    if (slot_b == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle_b);
        goto done;
    }

    CSN_ARRAY *zeros_arr = slot_a->array;
    uint32_t zeros_ndim = zeros_arr->ndim;
    uint32_t zeros_size = zeros_arr->size;

    CSN_ARRAY *poles_arr = slot_b->array;
    uint32_t poles_ndim = poles_arr->ndim;
    uint32_t poles_size = poles_arr->size;

    if (zeros_ndim != 1U || poles_ndim != 1U) {
        res = csound->InitError(csound, "[csnarray] zeros and poles must be 1-D arrays");
        goto done;
    }

    if (zeros_size > poles_size) {
        res = csound->InitError(csound, "[csnarray] There must be at least as many poles as zeros (%u zeros, %u poles)", zeros_size, poles_size);
        goto done;
    }

    size_t rel_degree = poles_size - zeros_size;

    uint32_t zeros_new_shape[CSN_MAX_DIMS] = {0};
    uint32_t poles_new_shape[CSN_MAX_DIMS] = {0};
    switch (mode) {
        case BILINEAR_ZPK:
        case LP2HP_ZPK:
            zeros_new_shape[0] = (uint32_t) poles_size;
            poles_new_shape[0] = (uint32_t) poles_size;
            break;
        case LP2LP_ZPK:
            zeros_new_shape[0] = (uint32_t) zeros_size;
            poles_new_shape[0] = (uint32_t) poles_size;
            break;
        case LP2BP_ZPK:
            zeros_new_shape[0] = 2U * (uint32_t) zeros_size + (uint32_t) rel_degree;
            poles_new_shape[0] = 2U * (uint32_t) poles_size;
            break;
        case LP2BS_ZPK:
            zeros_new_shape[0] = 2U * (uint32_t) poles_size;
            poles_new_shape[0] = 2U * (uint32_t) poles_size;
            break;
        default:
            break;
    }

    uint32_t protect[2] = { source_handle_a, source_handle_b };
    if (create_csnarray_locked(csound, reg, &p->h, zeros_ndim, zeros_new_shape, &p->array_a, p->handle_a, protect, 2U, &err, CSN_COMPLEX) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    if (create_csnarray_locked(csound, reg, &p->h, poles_ndim, poles_new_shape, &p->array_b, p->handle_b, protect, 2U, &err, CSN_COMPLEX) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *zeros_out = p->array_a;
    CSN_ARRAY *poles_out = p->array_b;

    res = map_zpk_roots(csound, zeros_out, zeros_arr, arg_b, bw, mode, "zero");
    if (res != OK) goto done;

    res = map_zpk_roots(csound, poles_out, poles_arr, arg_b, bw, mode, "pole");
    if (res != OK) goto done;

    size_t mapped_zeros = (mode == LP2BP_ZPK || mode == LP2BS_ZPK) ? 2U * zeros_size : zeros_size;
    fill_extra_zeros(zeros_out, mapped_zeros, arg_b, rel_degree, mode);

    *p->out_gain = (MYFLT) get_zpk_gain(zeros_arr, poles_arr, arg_b, bw, gain, rel_degree, mode);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnsig_fdesign3_deinit(CSOUND *csound, CSN_FDESIGN3 *p) {
    int32_t res = csnarray_deinit_by_handle(csound, &p->handle_a->id, &p->array_a, &p->h);
    if (res != OK) return res;
    return csnarray_deinit_by_handle(csound, &p->handle_b->id, &p->array_b, &p->h);
}

int32_t csnsig_lp2lp_zpk(CSOUND *csound, CSN_FDESIGN3 *p) {
    return fdesign3_helper(csound, p, LP2LP_ZPK);
}

int32_t csnsig_lp2hp_zpk(CSOUND *csound, CSN_FDESIGN3 *p) {
    return fdesign3_helper(csound, p, LP2HP_ZPK);
}

int32_t csnsig_lp2bp_zpk(CSOUND *csound, CSN_FDESIGN3 *p) {
    return fdesign3_helper(csound, p, LP2BP_ZPK);
}

int32_t csnsig_lp2bs_zpk(CSOUND *csound, CSN_FDESIGN3 *p) {
    return fdesign3_helper(csound, p, LP2BS_ZPK);
}

int32_t csnsig_bilinear_zpk(CSOUND *csound, CSN_FDESIGN3 *p) {
    return fdesign3_helper(csound, p, BILINEAR_ZPK);
}

// index of the first non-zero coefficient, or size when every one is zero
static size_t first_nonzero(const CSN_ARRAY *arr) {
    size_t i = 0;
    while (i < arr->size && arr->data[i] == 0.0) i++;
    return i;
}

// the coefficients from `lead` on, as a 1-D complex array get_roots can read
static int32_t load_trimmed(CSOUND *csound, CSN_ARRAY *dst, const CSN_ARRAY *src, size_t lead) {
    uint32_t shape[CSN_MAX_DIMS] = {0};
    shape[0] = (uint32_t) (src->size - lead);
    if (allocate_array(csound, dst, 1U, shape, 0, CSN_COMPLEX) != OK) return NOTOK;
    for (size_t i = lead; i < src->size; i++) {
        CSN_COMPLEXDAT c = { .re = src->data[i], .im = 0.0 };
        slice_put(dst->data, i - lead, 1, CSN_COMPLEX, c);
    }
    return OK;
}

int32_t csnsig_tf2zpk(CSOUND *csound, CSN_FDESIGN3 *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle_a = p->source_handle_a->id;
    uint32_t source_handle_b = p->source_handle_b->id;

    int32_t res = OK;
    const char *err = NULL;
    CSN_ARRAY b_trim = {0};
    CSN_ARRAY a_trim = {0};
    CSN_COMPLEXDAT *z_buffer = NULL;
    CSN_COMPLEXDAT *z_new_buffer = NULL;

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot_a = get_slot(reg, source_handle_a);
    CSN_SLOT *slot_b = get_slot(reg, source_handle_b);
    if (slot_a == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle_a);
        goto done;
    }
    if (slot_b == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle_b);
        goto done;
    }

    CSN_ARRAY *b_arr = slot_a->array;
    CSN_ARRAY *a_arr = slot_b->array;

    if (b_arr->ndim != 1U || a_arr->ndim != 1U) {
        res = csound->InitError(csound, "[csnarray] b and a must be 1-D arrays");
        goto done;
    }
    if (b_arr->itype != CSN_REAL || a_arr->itype != CSN_REAL) {
        res = csound->InitError(csound, "[csnarray] b and a must be real arrays: the gain of a complex transfer function is not real");
        goto done;
    }

    // leading zeros only lower the degree, as scipy.signal.normalize strips them
    size_t b_lead = first_nonzero(b_arr);
    size_t a_lead = first_nonzero(a_arr);
    if (a_lead == a_arr->size) {
        res = csound->InitError(csound, "[csnarray] The denominator a is empty or all zeros");
        goto done;
    }
    if (b_lead == b_arr->size) {
        res = csound->InitError(csound, "[csnarray] The numerator b is empty or all zeros");
        goto done;
    }

    uint32_t zeros_degree = (uint32_t) (b_arr->size - b_lead - 1U);
    uint32_t poles_degree = (uint32_t) (a_arr->size - a_lead - 1U);
    double gain = b_arr->data[b_lead] / a_arr->data[a_lead];

    if (load_trimmed(csound, &b_trim, b_arr, b_lead) != OK || load_trimmed(csound, &a_trim, a_arr, a_lead) != OK) {
        res = csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
        goto done;
    }

    uint32_t max_degree = zeros_degree > poles_degree ? zeros_degree : poles_degree;
    if (max_degree > 0U) {
        z_buffer = csound->Calloc(csound, sizeof(CSN_COMPLEXDAT) * max_degree);
        z_new_buffer = csound->Calloc(csound, sizeof(CSN_COMPLEXDAT) * max_degree);
        if (z_buffer == NULL || z_new_buffer == NULL) {
            res = csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
            goto done;
        }
    }

    uint32_t zeros_shape[CSN_MAX_DIMS] = {0};
    zeros_shape[0] = zeros_degree;
    uint32_t poles_shape[CSN_MAX_DIMS] = {0};
    poles_shape[0] = poles_degree;

    uint32_t protect[2] = { source_handle_a, source_handle_b };
    if (create_csnarray_locked(csound, reg, &p->h, 1U, zeros_shape, &p->array_a, p->handle_a, protect, 2U, &err, CSN_COMPLEX) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }
    if (create_csnarray_locked(csound, reg, &p->h, 1U, poles_shape, &p->array_b, p->handle_b, protect, 2U, &err, CSN_COMPLEX) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_AXIS_SPEC axis = csn_normalize_axis(NULL, 1U, CSN_AXIS_DEFAULT_LAST);
    if (zeros_degree > 0U) {
        res = get_roots(csound, p->array_a, &b_trim, axis, zeros_degree, z_buffer, z_new_buffer);
        if (res != OK) goto done;
    }
    if (poles_degree > 0U) {
        res = get_roots(csound, p->array_b, &a_trim, axis, poles_degree, z_buffer, z_new_buffer);
        if (res != OK) goto done;
    }

    *p->out_gain = (MYFLT) gain;

done:
    if (b_trim.data != NULL) csound->Free(csound, b_trim.data);
    if (a_trim.data != NULL) csound->Free(csound, a_trim.data);
    if (z_buffer != NULL) csound->Free(csound, z_buffer);
    if (z_new_buffer != NULL) csound->Free(csound, z_new_buffer);
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnsig_fdesign2_deinit(CSOUND *csound, CSN_FDESIGN2 *p) {
    int32_t res = csnarray_deinit_by_handle(csound, &p->handle_a->id, &p->array_a, &p->h);
    if (res != OK) return res;
    return csnarray_deinit_by_handle(csound, &p->handle_b->id, &p->array_b, &p->h);
}

static void poly_from_root(CSN_COMPLEXDAT *poly, CSN_COMPLEXDAT *tmp_poly, double *roots, size_t nroots, ITEM_TYPE itype) {
    size_t ncoeff = nroots + 1;
    poly[0].re = 1.0;
    poly[0].im = 0.0;

    size_t current_len = 1;
    for (size_t i = 0; i < nroots; ++i) {

        CSN_COMPLEXDAT r;
        r.re = roots[i * itype];
        r.im = itype == CSN_COMPLEX ? roots[i * 2 + 1] : 0.0;

        memset(tmp_poly, 0, ncoeff * sizeof(CSN_COMPLEXDAT));
        for (size_t j = 0; j < current_len; ++j) {
            /* tmp_poly[j] += poly[j] */
            complex_add(&tmp_poly[j], tmp_poly[j], poly[j]);
            /* tmp = r * poly[j] */
            CSN_COMPLEXDAT tmp = {0};
            complex_prod(&tmp, r, poly[j]);
            /* tmp_poly[j + 1] -= tmp */
            complex_sub(&tmp_poly[j + 1], tmp_poly[j + 1], tmp);
        }

        memcpy(poly, tmp_poly, ncoeff * sizeof(CSN_COMPLEXDAT));
        current_len++;
    }
}

/* Whether every imaginary part is negligible next to the largest coefficient:
   conjugate pairs expand to real coefficients up to rounding. */
static bool poly_is_real(const CSN_COMPLEXDAT *poly, size_t n) {
    double max_abs = 0.0;
    double max_im = 0.0;
    for (size_t i = 0; i < n; i++) {
        max_abs = fmax(max_abs, hypot(poly[i].re, poly[i].im));
        max_im = fmax(max_im, fabs(poly[i].im));
    }
    return max_im <= 1.0e-12 * max_abs;
}

static void store_poly(CSN_ARRAY *out, const CSN_COMPLEXDAT *poly, double scale) {
    for (size_t i = 0; i < out->size; i++) {
        CSN_COMPLEXDAT c = { .re = scale * poly[i].re, .im = scale * poly[i].im };
        slice_put(out->data, i, 1, out->itype, c);
    }
}

int32_t csnsig_zpk2tf(CSOUND *csound, CSN_FDESIGN2 *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle_a = p->source_handle_a->id;
    uint32_t source_handle_b = p->source_handle_b->id;

    int32_t res = OK;
    const char *err = NULL;
    CSN_COMPLEXDAT *b_poly = NULL;
    CSN_COMPLEXDAT *a_poly = NULL;
    CSN_COMPLEXDAT *temp_poly = NULL;
    double gain = (double) *p->arg_a;
    if (!IS_VALID_VALUE(gain)) {
        return csound->InitError(csound, "[csnarray] Invalid gain %g: it must be finite", gain);
    }

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot_a = get_slot(reg, source_handle_a);
    CSN_SLOT *slot_b = get_slot(reg, source_handle_b);
    if (slot_a == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle_a);
        goto done;
    }
    if (slot_b == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle_b);
        goto done;
    }

    CSN_ARRAY *zeros_arr = slot_a->array;
    CSN_ARRAY *poles_arr = slot_b->array;

    if (zeros_arr->ndim != 1U || poles_arr->ndim != 1U) {
        res = csound->InitError(csound, "[csnarray] zeros and poles must be 1-D arrays");
        goto done;
    }

    size_t b_len = zeros_arr->size + 1;
    size_t a_len = poles_arr->size + 1;
    size_t max_len = b_len > a_len ? b_len : a_len;

    b_poly = csound->Calloc(csound, sizeof(CSN_COMPLEXDAT) * b_len);
    a_poly = csound->Calloc(csound, sizeof(CSN_COMPLEXDAT) * a_len);
    temp_poly = csound->Calloc(csound, sizeof(CSN_COMPLEXDAT) * max_len);
    if (b_poly == NULL || a_poly == NULL || temp_poly == NULL) {
        res = csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
        goto done;
    }

    poly_from_root(b_poly, temp_poly, zeros_arr->data, zeros_arr->size, zeros_arr->itype);
    poly_from_root(a_poly, temp_poly, poles_arr->data, poles_arr->size, poles_arr->itype);

    // complex only when the roots are not in conjugate pairs, as scipy.signal.zpk2tf
    ITEM_TYPE b_itype = poly_is_real(b_poly, b_len) ? CSN_REAL : CSN_COMPLEX;
    ITEM_TYPE a_itype = poly_is_real(a_poly, a_len) ? CSN_REAL : CSN_COMPLEX;

    uint32_t b_shape[CSN_MAX_DIMS] = {0};
    b_shape[0] = (uint32_t) b_len;
    uint32_t a_shape[CSN_MAX_DIMS] = {0};
    a_shape[0] = (uint32_t) a_len;

    uint32_t protect[2] = { source_handle_a, source_handle_b };
    if (create_csnarray_locked(csound, reg, &p->h, 1U, b_shape, &p->array_a, p->handle_a, protect, 2U, &err, b_itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }
    if (create_csnarray_locked(csound, reg, &p->h, 1U, a_shape, &p->array_b, p->handle_b, protect, 2U, &err, a_itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    store_poly(p->array_a, b_poly, gain);
    store_poly(p->array_b, a_poly, 1.0);

done:
    if (b_poly != NULL) csound->Free(csound, b_poly);
    if (a_poly != NULL) csound->Free(csound, a_poly);
    if (temp_poly != NULL) csound->Free(csound, temp_poly);
    csound->UnlockMutex(reg->mutex);
    return res;
}

static double binomial(size_t n, size_t k) {
    double c = 1.0;
    for (size_t i = 1; i <= k; i++) c = c * (double) (n - k + i) / (double) i;
    return c;
}

static CSN_COMPLEXDAT cscale(CSN_COMPLEXDAT z, double s) {
    CSN_COMPLEXDAT out;
    complex_scalar_prod(&out, z, s);
    return out;
}

/* s -> s / w0. b and a are scaled so that the higher of the two degrees keeps
   its coefficient; normalize_tf divides it out afterwards. */
static void tf_lp2lp(CSN_COMPLEXDAT *ob, CSN_COMPLEXDAT *oa, const CSN_COMPLEXDAT *b, size_t n, const CSN_COMPLEXDAT *a, size_t d, double w0) {
    size_t m = n > d ? n : d;
    size_t start1 = n > d ? n - d : 0;
    size_t start2 = d > n ? d - n : 0;
    // pwo[i] = w0^(m - 1 - i)
    double top = pow(w0, (double) (m - 1 - start1));
    for (size_t i = 0; i < n; i++) ob[i] = cscale(b[i], top / pow(w0, (double) (m - 1 - (start2 + i))));
    for (size_t i = 0; i < d; i++) oa[i] = cscale(a[i], top / pow(w0, (double) (m - 1 - (start1 + i))));
}

/* s -> w0 / s: both polynomials reversed and padded to the same length. */
static void tf_lp2hp(CSN_COMPLEXDAT *ob, CSN_COMPLEXDAT *oa, const CSN_COMPLEXDAT *b, size_t n, const CSN_COMPLEXDAT *a, size_t d, double w0) {
    size_t m = n > d ? n : d;
    for (size_t i = 0; i < m; i++) {
        double pwo = pow(w0, (double) i);
        CSN_COMPLEXDAT zero = {0};
        ob[i] = i < n ? cscale(b[n - 1 - i], pwo) : zero;
        oa[i] = i < d ? cscale(a[d - 1 - i], pwo) : zero;
    }
}

/* s -> (s^2 + w0^2) / (s bw), both sides multiplied by (s bw)^ma. The power
   s^i of a degree-deg polynomial becomes sum_k C(i, k) s^(2k) w0^(2(i-k)) (s bw)^(ma-i),
   the power j = ma - i + 2k. */
static void tf_lp2bp_one(CSN_COMPLEXDAT *out, size_t out_deg, const CSN_COMPLEXDAT *c, size_t deg, size_t ma, double w0, double bw) {
    double wosq = w0 * w0;
    for (size_t j = 0; j <= out_deg; j++) out[j] = (CSN_COMPLEXDAT) {0};
    for (size_t i = 0; i <= deg; i++) {
        for (size_t k = 0; k <= i; k++) {
            size_t j = ma - i + 2 * k;
            double f = binomial(i, k) * pow(wosq, (double) (i - k)) * pow(bw, (double) (ma - i));
            CSN_COMPLEXDAT term = cscale(c[deg - i], f);
            complex_add(&out[out_deg - j], out[out_deg - j], term);
        }
    }
}

/* s -> s bw / (s^2 + w0^2), both sides multiplied by (s^2 + w0^2)^ma. The power
   s^i becomes sum_k C(ma-i, k) (s bw)^i s^(2k) w0^(2(ma-i-k)), the power j = i + 2k. */
static void tf_lp2bs_one(CSN_COMPLEXDAT *out, size_t out_deg, const CSN_COMPLEXDAT *c, size_t deg, size_t ma, double w0, double bw) {
    double wosq = w0 * w0;
    for (size_t j = 0; j <= out_deg; j++) out[j] = (CSN_COMPLEXDAT) {0};
    for (size_t i = 0; i <= deg; i++) {
        for (size_t k = 0; k <= ma - i; k++) {
            size_t j = i + 2 * k;
            double f = binomial(ma - i, k) * pow(wosq, (double) (ma - i - k)) * pow(bw, (double) i);
            CSN_COMPLEXDAT term = cscale(c[deg - i], f);
            complex_add(&out[out_deg - j], out[out_deg - j], term);
        }
    }
}

static bool cis_zero(CSN_COMPLEXDAT z) {
    return z.re == 0.0 && z.im == 0.0;
}

/* scipy.signal.normalize: strip the leading zeros of a, divide both by a[0],
   then strip the leading coefficients of b below 1e-14, keeping at least one.
   Returns false when a is all zeros. */
static bool normalize_tf(CSN_COMPLEXDAT *b, size_t *b_off, size_t nb, CSN_COMPLEXDAT *a, size_t *a_off, size_t na) {
    size_t lead = 0;
    while (lead < na && cis_zero(a[lead])) lead++;
    if (lead == na) return false;
    CSN_COMPLEXDAT a0 = a[lead];
    for (size_t i = lead; i < na; i++) complex_div(&a[i], a[i], a0);
    for (size_t i = 0; i < nb; i++) complex_div(&b[i], b[i], a0);
    *a_off = lead;

    size_t blead = 0;
    while (blead + 1 < nb && hypot(b[blead].re, b[blead].im) <= 1.0e-14) blead++;
    *b_off = blead;
    return true;
}

static int32_t tf_transform_helper(CSOUND *csound, CSN_FDESIGN2 *p, CSN_PF_DESIGN_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle_b = p->source_handle_a->id;
    uint32_t source_handle_a = p->source_handle_b->id;

    double w0 = (double) *p->arg_a;
    if (!IS_VALID_VALUE(w0) || w0 <= 0.0) {
        return csound->InitError(csound, "[csnarray] Invalid w0 %g: it must be finite and greater than zero", w0);
    }
    double bw = 0.0;
    if (mode == LP2BP || mode == LP2BS) {
        bw = (double) *p->arg_b;
        if (!IS_VALID_VALUE(bw) || bw <= 0.0) {
            return csound->InitError(csound, "[csnarray] Invalid bandwidth %g: it must be finite and greater than zero", bw);
        }
    }

    int32_t res = OK;
    const char *err = NULL;

    CSN_COMPLEXDAT *b = NULL;
    CSN_COMPLEXDAT *a = NULL;
    CSN_COMPLEXDAT *ob = NULL;
    CSN_COMPLEXDAT *oa = NULL;

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot_b = get_slot(reg, source_handle_b);
    CSN_SLOT *slot_a = get_slot(reg, source_handle_a);
    if (slot_b == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle_b);
        goto done;
    }
    if (slot_a == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle_a);
        goto done;
    }

    CSN_ARRAY *b_arr = slot_b->array;
    CSN_ARRAY *a_arr = slot_a->array;
    if (b_arr->ndim != 1U || a_arr->ndim != 1U) {
        res = csound->InitError(csound, "[csnarray] b and a must be 1-D arrays");
        goto done;
    }
    if (b_arr->size == 0 || a_arr->size == 0) {
        res = csound->InitError(csound, "[csnarray] b and a must hold at least one coefficient");
        goto done;
    }

    size_t n = b_arr->size;
    size_t d = a_arr->size;
    size_t ma = (n > d ? n : d) - 1;
    size_t nb_out = 0;
    size_t na_out = 0;
    switch (mode) {
        case LP2LP:
            nb_out = n;
            na_out = d;
            break;
        case LP2HP:
            nb_out = ma + 1;
            na_out = ma + 1;
            break;
        case LP2BP:
            nb_out = n + ma;
            na_out = d + ma;
            break;
        case LP2BS:
            nb_out = 2 * ma + 1;
            na_out = 2 * ma + 1;
            break;
        default: break;
    }

    b = csound->Calloc(csound, sizeof(CSN_COMPLEXDAT) * n);
    a = csound->Calloc(csound, sizeof(CSN_COMPLEXDAT) * d);
    ob = csound->Calloc(csound, sizeof(CSN_COMPLEXDAT) * nb_out);
    oa = csound->Calloc(csound, sizeof(CSN_COMPLEXDAT) * na_out);
    if (b == NULL || a == NULL || ob == NULL || oa == NULL) {
        res = csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
        goto done;
    }

    for (size_t i = 0; i < n; i++) {
        b[i] = slice_get(b_arr->data, i, 1, b_arr->itype);
    }

    for (size_t i = 0; i < d; i++) {
        a[i] = slice_get(a_arr->data, i, 1, a_arr->itype);
    }

    switch (mode) {
        case LP2LP:
            tf_lp2lp(ob, oa, b, n, a, d, w0);
            break;
        case LP2HP:
            tf_lp2hp(ob, oa, b, n, a, d, w0);
            break;
        case LP2BP:
            tf_lp2bp_one(ob, nb_out - 1, b, n - 1, ma, w0, bw);
            tf_lp2bp_one(oa, na_out - 1, a, d - 1, ma, w0, bw);
            break;
        case LP2BS:
            tf_lp2bs_one(ob, nb_out - 1, b, n - 1, ma, w0, bw);
            tf_lp2bs_one(oa, na_out - 1, a, d - 1, ma, w0, bw);
            break;
        default: break;
    }

    size_t b_off = 0;
    size_t a_off = 0;
    if (!normalize_tf(ob, &b_off, nb_out, oa, &a_off, na_out)) {
        res = csound->InitError(csound, "[csnarray] The denominator a is all zeros");
        goto done;
    }

    ITEM_TYPE out_itype = (b_arr->itype == CSN_COMPLEX || a_arr->itype == CSN_COMPLEX) ? CSN_COMPLEX : CSN_REAL;
    uint32_t b_shape[CSN_MAX_DIMS] = {0};
    b_shape[0] = (uint32_t) (nb_out - b_off);
    uint32_t a_shape[CSN_MAX_DIMS] = {0};
    a_shape[0] = (uint32_t) (na_out - a_off);

    uint32_t protect[2] = { source_handle_b, source_handle_a };
    if (create_csnarray_locked(csound, reg, &p->h, 1U, b_shape, &p->array_a, p->handle_a, protect, 2U, &err, out_itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }
    if (create_csnarray_locked(csound, reg, &p->h, 1U, a_shape, &p->array_b, p->handle_b, protect, 2U, &err, out_itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }
    for (size_t i = 0; i < b_shape[0]; i++) {
        slice_put(p->array_a->data, i, 1, out_itype, ob[b_off + i]);
    }

    for (size_t i = 0; i < a_shape[0]; i++) {
        slice_put(p->array_b->data, i, 1, out_itype, oa[a_off + i]);
    }

done:
    if (b != NULL) csound->Free(csound, b);
    if (a != NULL) csound->Free(csound, a);
    if (ob != NULL) csound->Free(csound, ob);
    if (oa != NULL) csound->Free(csound, oa);
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnsig_lp2lp(CSOUND *csound, CSN_FDESIGN2 *p) {
    return tf_transform_helper(csound, p, LP2LP);
}

int32_t csnsig_lp2hp(CSOUND *csound, CSN_FDESIGN2 *p) {
    return tf_transform_helper(csound, p, LP2HP);
}

int32_t csnsig_lp2bp(CSOUND *csound, CSN_FDESIGN2 *p) {
    return tf_transform_helper(csound, p, LP2BP);
}

int32_t csnsig_lp2bs(CSOUND *csound, CSN_FDESIGN2 *p) {
    return tf_transform_helper(csound, p, LP2BS);
}

int32_t csnsig_zpk2sos_deinit(CSOUND *csound, CSN_ZPK2SOS *p) {
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

static int compare_root_order(const void *x, const void *y) {
    const CSN_COMPLEXDAT *a = (const CSN_COMPLEXDAT *) x;
    const CSN_COMPLEXDAT *b = (const CSN_COMPLEXDAT *) y;
    if (a->re != b->re) return a->re < b->re ? -1 : 1;
    double ia = fabs(a->im);
    double ib = fabs(b->im);
    if (ia != ib) return ia < ib ? -1 : 1;
    return 0;
}

/* scipy's _cplxreal: sorted by real part then |imag|, a root within 100 eps of
   the real axis is real, and every other one must meet its conjugate. Each
   conjugate pair is kept once, as the mean of the upper root and the conjugate
   of the lower, upper half plane first, reals after. Sorts `in` in place. */
static int32_t split_conjugates(CSOUND *csound, CSN_COMPLEXDAT *in, size_t n, CSN_ROOTSLOT *out, size_t *nout, const char *what) {
    const double tol = 100.0 * DBL_EPSILON;
    qsort(in, n, sizeof(CSN_COMPLEXDAT), compare_root_order);

    size_t count = 0;
    size_t upper = 0;
    size_t lower = 0;
    for (size_t i = 0; i < n; i++) {
        if (fabs(in[i].im) <= tol * hypot(in[i].re, in[i].im)) continue;
        if (in[i].im > 0.0) upper++; else lower++;
    }
    if (upper != lower) {
        return csound->InitError(csound, "[csnarray] The %s contain a complex value with no matching conjugate", what);
    }

    bool *taken = csound->Calloc(csound, sizeof(bool) * (n > 0 ? n : 1));
    if (taken == NULL) return csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");

    for (size_t i = 0; i < n; i++) {
        double mag = hypot(in[i].re, in[i].im);
        if (fabs(in[i].im) <= tol * mag || in[i].im < 0.0) continue;
        // the lower-half root closest to this one's conjugate
        ptrdiff_t best = -1;
        double best_dist = INFINITY;
        for (size_t j = 0; j < n; j++) {
            if (taken[j] || in[j].im >= 0.0 || fabs(in[j].im) <= tol * hypot(in[j].re, in[j].im)) continue;
            double dist = hypot(in[i].re - in[j].re, in[i].im + in[j].im);
            if (dist < best_dist) {
                best_dist = dist;
                best = (ptrdiff_t) j;
            }
        }
        if (best < 0 || best_dist > tol * hypot(in[best].re, in[best].im)) {
            csound->Free(csound, taken);
            return csound->InitError(csound, "[csnarray] The %s contain a complex value with no matching conjugate", what);
        }
        taken[best] = true;
        out[count].v.re = (in[i].re + in[best].re) * 0.5;
        out[count].v.im = (in[i].im - in[best].im) * 0.5;
        out[count].is_real = false;
        out[count].used = false;
        count++;
    }
    for (size_t i = 0; i < n; i++) {
        if (fabs(in[i].im) > tol * hypot(in[i].re, in[i].im)) continue;
        out[count].v.re = in[i].re;
        out[count].v.im = 0.0;
        out[count].is_real = true;
        out[count].used = false;
        count++;
    }
    csound->Free(csound, taken);
    *nout = count;
    return OK;
}

static size_t count_unused(const CSN_ROOTSLOT *r, size_t n, bool real_only) {
    size_t c = 0;
    for (size_t i = 0; i < n; i++) {
        if (!r[i].used && (!real_only || r[i].is_real)) c++;
    }
    return c;
}

// the unused pole closest to the unit circle, optionally among the real ones
static ptrdiff_t worst_pole(const CSN_ROOTSLOT *r, size_t n, bool real_only) {
    ptrdiff_t best = -1;
    double best_dist = INFINITY;
    for (size_t i = 0; i < n; i++) {
        if (r[i].used || (real_only && !r[i].is_real)) continue;
        double dist = fabs(1.0 - hypot(r[i].v.re, r[i].v.im));
        if (dist < best_dist) {
            best_dist = dist;
            best = (ptrdiff_t) i;
        }
    }
    return best;
}

// scipy's _nearest_real_complex_idx: the unused root of the given kind closest to `to`
static ptrdiff_t nearest_root(const CSN_ROOTSLOT *r, size_t n, CSN_COMPLEXDAT to, CSN_ROOT_KIND kind) {
    ptrdiff_t best = -1;
    double best_dist = INFINITY;
    for (size_t i = 0; i < n; i++) {
        if (r[i].used) continue;
        if (kind == ROOT_REAL && !r[i].is_real) continue;
        if (kind == ROOT_COMPLEX && r[i].is_real) continue;
        double dist = hypot(r[i].v.re - to.re, r[i].v.im - to.im);
        if (dist < best_dist) {
            best_dist = dist;
            best = (ptrdiff_t) i;
        }
    }
    return best;
}

static CSN_COMPLEXDAT cconj(CSN_COMPLEXDAT z) {
    CSN_COMPLEXDAT c = { .re = z.re, .im = -z.im };
    return c;
}

/* scipy's _single_zpksos: the monic polynomial of up to two roots, its real
   part written right-aligned into three slots. */
static void put_section_poly(double *dst, const CSN_COMPLEXDAT *roots, size_t n) {
    CSN_COMPLEXDAT poly[3] = { { 1.0, 0.0 }, { 0.0, 0.0 }, { 0.0, 0.0 } };
    CSN_COMPLEXDAT tmp[3];
    poly_from_root(poly, tmp, (double *) roots, n, CSN_COMPLEX);
    for (size_t i = 0; i < 3; i++) dst[i] = 0.0;
    for (size_t i = 0; i <= n; i++) dst[3 - (n + 1) + i] = poly[i].re;
}

static void put_section(double *row, const CSN_COMPLEXDAT *z, size_t nz, const CSN_COMPLEXDAT *p, size_t np) {
    put_section_poly(row, z, nz);
    put_section_poly(row + 3, p, np);
}

static int32_t zpk2sos_helper(CSOUND *csound, CSN_ARRAY *zeros_arr, CSN_ARRAY *poles_arr, double **sos_out, size_t *nsec,  double gain) {
    size_t nz = zeros_arr->size;
    size_t np = poles_arr->size;
    size_t n_sections = 0;
    size_t n = 0;

    CSN_COMPLEXDAT *z_in = NULL;
    CSN_COMPLEXDAT *p_in = NULL;
    CSN_ROOTSLOT *zs = NULL;
    CSN_ROOTSLOT *ps = NULL;
    double *sos = NULL;

    int32_t res = OK;

    if (nz == 0 && np == 0) {
        n_sections = 1;
        sos = csound->Calloc(csound, sizeof(double) * 6);
        if (sos == NULL) {
            res = csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
            goto done;
        }
        sos[0] = gain;
        sos[3] = 1.0;
    } else {
        // pad the shorter side with roots at the origin, then to an even count
        n = nz > np ? nz : np;
        n_sections = (n + 1) / 2;
        size_t n_even = 2 * n_sections;

        z_in = csound->Calloc(csound, sizeof(CSN_COMPLEXDAT) * n_even);
        p_in = csound->Calloc(csound, sizeof(CSN_COMPLEXDAT) * n_even);
        zs = csound->Calloc(csound, sizeof(CSN_ROOTSLOT) * n_even);
        ps = csound->Calloc(csound, sizeof(CSN_ROOTSLOT) * n_even);
        sos = csound->Calloc(csound, sizeof(double) * 6 * n_sections);
        if (z_in == NULL || p_in == NULL || zs == NULL || ps == NULL || sos == NULL) {
            res = csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
            goto done;
        }
        for (size_t i = 0; i < nz; i++) z_in[i] = slice_get(zeros_arr->data, i, 1, zeros_arr->itype);
        for (size_t i = 0; i < np; i++) p_in[i] = slice_get(poles_arr->data, i, 1, poles_arr->itype);

        size_t zn = 0;
        size_t pn = 0;
        res = split_conjugates(csound, z_in, n_even, zs, &zn, "zeros");
        if (res != OK) goto done;
        res = split_conjugates(csound, p_in, n_even, ps, &pn, "poles");
        if (res != OK) goto done;

        for (size_t si = 0; si < n_sections; si++) {
            double *row = sos + 6 * si;
            ptrdiff_t p1_idx = worst_pole(ps, pn, false);
            ps[p1_idx].used = true;
            CSN_ROOTSLOT p1 = ps[p1_idx];

            if (p1.is_real && count_unused(ps, pn, true) == 0) {
                // the last real pole, paired with the nearest real zero
                ptrdiff_t z1_idx = nearest_root(zs, zn, p1.v, ROOT_REAL);
                zs[z1_idx].used = true;
                CSN_COMPLEXDAT zr[2] = { zs[z1_idx].v, { 0.0, 0.0 } };
                CSN_COMPLEXDAT pr[2] = { p1.v, { 0.0, 0.0 } };
                put_section(row, zr, 2, pr, 2);
            } else if (count_unused(ps, pn, false) + 1 == count_unused(zs, zn, false) && !p1.is_real && count_unused(ps, pn, true) == 1 && count_unused(zs, zn, true) == 1) {
                // one real pole and one real zero are left for the last
                // section: this complex pole must take a complex zero
                ptrdiff_t z1_idx = nearest_root(zs, zn, p1.v, ROOT_COMPLEX);
                zs[z1_idx].used = true;
                CSN_COMPLEXDAT zr[2] = { zs[z1_idx].v, cconj(zs[z1_idx].v) };
                CSN_COMPLEXDAT pr[2] = { p1.v, cconj(p1.v) };
                put_section(row, zr, 2, pr, 2);
            } else {
                CSN_COMPLEXDAT p2;
                if (p1.is_real) {
                    ptrdiff_t p2_idx = worst_pole(ps, pn, true);
                    ps[p2_idx].used = true;
                    p2 = ps[p2_idx].v;
                } else {
                    p2 = cconj(p1.v);
                }
                CSN_COMPLEXDAT pr[2] = { p1.v, p2 };

                if (count_unused(zs, zn, false) > 0) {
                    ptrdiff_t z1_idx = nearest_root(zs, zn, p1.v, ROOT_ANY);
                    zs[z1_idx].used = true;
                    CSN_ROOTSLOT z1 = zs[z1_idx];
                    if (!z1.is_real) {
                        CSN_COMPLEXDAT zr[2] = { z1.v, cconj(z1.v) };
                        put_section(row, zr, 2, pr, 2);
                    } else if (count_unused(zs, zn, false) > 0) {
                        ptrdiff_t z2_idx = nearest_root(zs, zn, p1.v, ROOT_REAL);
                        zs[z2_idx].used = true;
                        CSN_COMPLEXDAT zr[2] = { z1.v, zs[z2_idx].v };
                        put_section(row, zr, 2, pr, 2);
                    } else {
                        put_section(row, &z1.v, 1, pr, 2);
                    }
                } else {
                    put_section(row, NULL, 0, pr, 2);
                }
            }
        }

        // the sections closest to the unit circle go last
        for (size_t i = 0; i < n_sections / 2; i++) {
            double tmp[6];
            memcpy(tmp, sos + 6 * i, sizeof(tmp));
            memcpy(sos + 6 * i, sos + 6 * (n_sections - 1 - i), sizeof(tmp));
            memcpy(sos + 6 * (n_sections - 1 - i), tmp, sizeof(tmp));
        }
        for (size_t i = 0; i < 3; i++) sos[i] *= gain;
    }

    *nsec = n_sections;
    *sos_out = sos;

done:
    if (res != OK && sos != NULL) csound->Free(csound, sos);
    if (z_in != NULL) csound->Free(csound, z_in);
    if (p_in != NULL) csound->Free(csound, p_in);
    if (zs != NULL) csound->Free(csound, zs);
    if (ps != NULL) csound->Free(csound, ps);
    return res;
}

int32_t csnsig_zpk2sos(CSOUND *csound, CSN_ZPK2SOS *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t zeros_handle = p->source_handle_a->id;
    uint32_t poles_handle = p->source_handle_b->id;

    double gain = (double) *p->gain;
    if (!IS_VALID_VALUE(gain)) {
        return csound->InitError(csound, "[csnarray] Invalid gain %g: it must be finite", gain);
    }

    int32_t res = OK;
    const char *err = NULL;
    double *sos = NULL;

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot_z = get_slot(reg, zeros_handle);
    CSN_SLOT *slot_p = get_slot(reg, poles_handle);
    if (slot_z == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", zeros_handle);
        goto done;
    }
    if (slot_p == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", poles_handle);
        goto done;
    }

    CSN_ARRAY *zeros_arr = slot_z->array;
    CSN_ARRAY *poles_arr = slot_p->array;
    if (zeros_arr->ndim != 1U || poles_arr->ndim != 1U) {
        res = csound->InitError(csound, "[csnarray] zeros and poles must be 1-D arrays");
        goto done;
    }

    size_t n_sections = 0;
    res = zpk2sos_helper(csound, zeros_arr, poles_arr, &sos, &n_sections, gain);
    if (res != OK) goto done;

    uint32_t shape[CSN_MAX_DIMS] = {0};
    shape[0] = (uint32_t) n_sections;
    shape[1] = 6U;
    uint32_t protect[2] = { zeros_handle, poles_handle };
    if (create_csnarray_locked(csound, reg, &p->h, 2U, shape, &p->array, p->handle, protect, 2U, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }
    memcpy(p->array->data, sos, sizeof(double) * 6 * n_sections);

done:
    if (sos != NULL) csound->Free(csound, sos);
    csound->UnlockMutex(reg->mutex);
    return res;
}


int32_t csnsig_buttap_deinit(CSOUND *csound, CSN_BUTTAP *p) {
    int32_t res = csnarray_deinit_by_handle(csound, &p->handle_a->id, &p->array_a, &p->h);
    if (res != OK) return res;
    return csnarray_deinit_by_handle(csound, &p->handle_b->id, &p->array_b, &p->h);
}

static void buttap_assign(double *data, size_t order) {
    /* scipy's -exp(j pi m / 2N), m = -N+1, -N+3, ..., N-1: symmetric in m, so
       the pairs are exact conjugates and the real pole of an odd order is
       exactly -1 */
    for (size_t k = 0; k < order; ++k) {
        double m = 2.0 * (double) k - (double) order + 1.0;
        double theta = CSN_PI * m / (2.0 * (double) order);
        data[k * 2] = -cos(theta);
        data[k * 2 + 1] = -sin(theta);
    }
}

int32_t csnsig_buttap(CSOUND *csound, CSN_BUTTAP *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    const char *err = NULL;

    double ord_value = (double) *p->order;
    if (!IS_VALID_LENGTH(ord_value)) {
        return csound->InitError(csound, "[csnarray] Invalid Butterworth order %g: it must be a non-negative integer", ord_value);
    }
    size_t order = (size_t) ord_value;

    csound->LockMutex(reg->mutex);

    uint32_t zeros_shape[CSN_MAX_DIMS] = {0};
    if (create_csnarray_locked(csound, reg, &p->h, 1U, zeros_shape, &p->array_a, p->handle_a, NULL, 0U, &err, CSN_COMPLEX) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }
    reset_empty_csnarray(p->array_a, 1U, zeros_shape, CSN_COMPLEX);

    uint32_t poles_shape[CSN_MAX_DIMS] = {0};
    poles_shape[0] = (uint32_t) order;
    if (create_csnarray_locked(csound, reg, &p->h, 1U, poles_shape, &p->array_b, p->handle_b, NULL, 0U, &err, CSN_COMPLEX) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    buttap_assign(p->array_b->data, order);
    *p->gain = FL(1.0);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}


int32_t csnsig_fdesign1f2_deinit(CSOUND *csound, CSN_FDESIGN1F2 *p) {
    int32_t res = csnarray_deinit_by_handle(csound, &p->handle_a->id, &p->array_a, &p->h);
    if (res != OK) return res;
    return csnarray_deinit_by_handle(csound, &p->handle_b->id, &p->array_b, &p->h);
}

int32_t csnsig_fdesign1f1_deinit(CSOUND *csound, CSN_FDESIGN1F1 *p) {
    int32_t res = csnarray_deinit_by_handle(csound, &p->handle_a->id, &p->array_a, &p->h);
    if (res != OK) return res;
    return csnarray_deinit_by_handle(csound, &p->handle_b->id, &p->array_b, &p->h);
}

/* The part every IIR design shares once its analog lowpass prototype exists:
   prototype -> lowpass / highpass / bandpass / bandstop at w0 (and bw) ->
   bilinear at fs. Only the prototype differs between Butterworth, Chebyshev
   and elliptic designs. The digital zeros and poles land in bil_zeros and
   bil_poles, which the caller frees whatever the outcome. */
static int32_t iir_digital_zpk(CSOUND *csound, const CSN_ARRAY *ztap, const CSN_ARRAY *ptap, double ktap,
                               CSN_PF_DESIGN_MODE pf, double w0, double bw, double fs,
                               CSN_ARRAY *bil_zeros, CSN_ARRAY *bil_poles, double *bil_gain) {
    int32_t res = OK;
    CSN_ARRAY zeros = {0};
    CSN_ARRAY poles = {0};

    // prototype -> analog filter of the requested type
    size_t degree = ptap->size - ztap->size;
    uint32_t zeros_shape[CSN_MAX_DIMS] = {0};
    uint32_t poles_shape[CSN_MAX_DIMS] = {0};
    switch (pf) {
        case LP2LP_ZPK:
            zeros_shape[0] = (uint32_t) ztap->size;
            poles_shape[0] = (uint32_t) ptap->size;
            break;
        case LP2HP_ZPK:
            zeros_shape[0] = (uint32_t) ptap->size;
            poles_shape[0] = (uint32_t) ptap->size;
            break;
        case LP2BP_ZPK:
            zeros_shape[0] = 2U * (uint32_t) ztap->size + (uint32_t) degree;
            poles_shape[0] = 2U * (uint32_t) ptap->size;
            break;
        case LP2BS_ZPK:
            zeros_shape[0] = 2U * (uint32_t) ptap->size;
            poles_shape[0] = 2U * (uint32_t) ptap->size;
            break;
        default:
            return csound->InitError(csound, "[csnarray] Internal error: invalid filter transform");
    }
    if (allocate_array(csound, &zeros, 1U, zeros_shape, 0, CSN_COMPLEX) != OK || allocate_array(csound, &poles, 1U, poles_shape, 0, CSN_COMPLEX) != OK) {
        res = csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
        goto done;
    }

    res = map_zpk_roots(csound, &zeros, ztap, w0, bw, pf, "zero");
    if (res != OK) goto done;
    res = map_zpk_roots(csound, &poles, ptap, w0, bw, pf, "pole");
    if (res != OK) goto done;
    size_t mapped_zeros = (pf == LP2BP_ZPK || pf == LP2BS_ZPK) ? 2U * ztap->size : ztap->size;
    fill_extra_zeros(&zeros, mapped_zeros, w0, degree, pf);
    double gain = get_zpk_gain(ztap, ptap, w0, bw, ktap, degree, pf);

    // analog -> digital: the zeros the bilinear transform adds sit at -1
    size_t bil_degree = poles.size - zeros.size;
    uint32_t bil_shape[CSN_MAX_DIMS] = {0};
    bil_shape[0] = (uint32_t) poles.size;
    if (allocate_array(csound, bil_zeros, 1U, bil_shape, 0, CSN_COMPLEX) != OK || allocate_array(csound, bil_poles, 1U, bil_shape, 0, CSN_COMPLEX) != OK) {
        res = csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
        goto done;
    }

    res = map_zpk_roots(csound, bil_zeros, &zeros, fs, 0.0, BILINEAR_ZPK, "zero");
    if (res != OK) goto done;
    res = map_zpk_roots(csound, bil_poles, &poles, fs, 0.0, BILINEAR_ZPK, "pole");
    if (res != OK) goto done;
    fill_extra_zeros(bil_zeros, zeros.size, fs, bil_degree, BILINEAR_ZPK);
    *bil_gain = get_zpk_gain(&zeros, &poles, fs, 0.0, gain, bil_degree, BILINEAR_ZPK);

done:
    if (zeros.data != NULL) csound->Free(csound, zeros.data);
    if (poles.data != NULL) csound->Free(csound, poles.data);
    return res;
}

// the digital filter as b, a published on two handles
static int32_t iir_zpk_to_ba(CSOUND *csound, OPDS *h, CSNREF *handle_b, CSNREF *handle_a, CSN_ARRAY **array_b, CSN_ARRAY **array_a, const CSN_ARRAY *ztap, const CSN_ARRAY *ptap, double ktap, CSN_PF_DESIGN_MODE pf, double w0, double bw, double fs) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    const char *err = NULL;
    CSN_ARRAY bil_zeros = {0};
    CSN_ARRAY bil_poles = {0};
    CSN_COMPLEXDAT *b_poly = NULL;
    CSN_COMPLEXDAT *a_poly = NULL;
    CSN_COMPLEXDAT *temp_poly = NULL;
    bool locked = false;

    double bil_gain = 0.0;
    res = iir_digital_zpk(csound, ztap, ptap, ktap, pf, w0, bw, fs, &bil_zeros, &bil_poles, &bil_gain);
    if (res != OK) goto done;

    // zpk -> b, a: conjugate pairs, so the coefficients are real
    size_t b_len = bil_zeros.size + 1;
    size_t a_len = bil_poles.size + 1;
    size_t max_len = b_len > a_len ? b_len : a_len;

    b_poly = csound->Calloc(csound, sizeof(CSN_COMPLEXDAT) * b_len);
    a_poly = csound->Calloc(csound, sizeof(CSN_COMPLEXDAT) * a_len);
    temp_poly = csound->Calloc(csound, sizeof(CSN_COMPLEXDAT) * max_len);
    if (b_poly == NULL || a_poly == NULL || temp_poly == NULL) {
        res = csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
        goto done;
    }

    poly_from_root(b_poly, temp_poly, bil_zeros.data, bil_zeros.size, bil_zeros.itype);
    poly_from_root(a_poly, temp_poly, bil_poles.data, bil_poles.size, bil_poles.itype);

    uint32_t b_shape[CSN_MAX_DIMS] = {0};
    b_shape[0] = (uint32_t) b_len;
    uint32_t a_shape[CSN_MAX_DIMS] = {0};
    a_shape[0] = (uint32_t) a_len;

    csound->LockMutex(reg->mutex);
    locked = true;
    if (create_csnarray_locked(csound, reg, h, 1U, b_shape, array_b, handle_b, NULL, 0U, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }
    if (create_csnarray_locked(csound, reg, h, 1U, a_shape, array_a, handle_a, NULL, 0U, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    store_poly(*array_b, b_poly, bil_gain);
    store_poly(*array_a, a_poly, 1.0);

done:
    if (bil_zeros.data != NULL) csound->Free(csound, bil_zeros.data);
    if (bil_poles.data != NULL) csound->Free(csound, bil_poles.data);
    if (b_poly != NULL) csound->Free(csound, b_poly);
    if (a_poly != NULL) csound->Free(csound, a_poly);
    if (temp_poly != NULL) csound->Free(csound, temp_poly);
    if (locked) csound->UnlockMutex(reg->mutex);
    return res;
}

// the digital filter as an (n, 6) array of second-order sections
static int32_t iir_zpk_to_sos(CSOUND *csound, OPDS *h, CSNREF *handle, CSN_ARRAY **array, const CSN_ARRAY *ztap, const CSN_ARRAY *ptap, double ktap, CSN_PF_DESIGN_MODE pf, double w0, double bw, double fs) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    const char *err = NULL;
    CSN_ARRAY bil_zeros = {0};
    CSN_ARRAY bil_poles = {0};
    double *sos = NULL;
    bool locked = false;

    double bil_gain = 0.0;
    res = iir_digital_zpk(csound, ztap, ptap, ktap, pf, w0, bw, fs, &bil_zeros, &bil_poles, &bil_gain);
    if (res != OK) goto done;

    size_t n_sections = 0;
    res = zpk2sos_helper(csound, &bil_zeros, &bil_poles, &sos, &n_sections, bil_gain);
    if (res != OK) goto done;

    uint32_t sos_shape[CSN_MAX_DIMS] = {0};
    sos_shape[0] = (uint32_t) n_sections;
    sos_shape[1] = 6U;

    csound->LockMutex(reg->mutex);
    locked = true;
    if (create_csnarray_locked(csound, reg, h, 2U, sos_shape, array, handle, NULL, 0U, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }
    memcpy((*array)->data, sos, sizeof(double) * 6 * n_sections);

done:
    if (bil_zeros.data != NULL) csound->Free(csound, bil_zeros.data);
    if (bil_poles.data != NULL) csound->Free(csound, bil_poles.data);
    if (sos != NULL) csound->Free(csound, sos);
    if (locked) csound->UnlockMutex(reg->mutex);
    return res;
}

/* ---------------------------------------------------------------------------
   Elliptic-function helpers for the elliptic prototype, parameter convention
   (m = k^2) as scipy.special uses it.
   ------------------------------------------------------------------------- */

// arithmetic-geometric mean
static double agm(double a, double b) {
    for (int i = 0; i < 64 && fabs(a - b) > DBL_EPSILON * a; i++) {
        double an = 0.5 * (a + b);
        b = sqrt(a * b);
        a = an;
    }
    return 0.5 * (a + b);
}

// complete elliptic integral of the first kind K(m), from its complement m1 = 1 - m
static double ellipk_from_m1(double m1) {
    return CSN_PI / (2.0 * agm(1.0, sqrt(m1)));
}

// scipy.special.ellipk(m) and ellipkm1(p) = K(1 - p)
static double csn_ellipk(double m) { return ellipk_from_m1(1.0 - m); }
static double csn_ellipkm1(double p) { return ellipk_from_m1(p); }

/* Jacobi elliptic functions sn, cn, dn of u at parameter m, by the descending
   Landen (AGM) scheme of Abramowitz & Stegun 16.4, as cephes' ellpj. */
static void csn_ellipj(double u, double m, double *sn, double *cn, double *dn) {
    if (m < 1.0e-9) {
        double t = sin(u);
        double b = cos(u);
        double ai = 0.25 * m * (u - t * b);
        *sn = t - ai * b;
        *cn = b + ai * t;
        *dn = 1.0 - 0.5 * m * t * t;
        return;
    }
    if (m >= 0.9999999999) {
        double ai = 0.25 * (1.0 - m);
        double b = cosh(u);
        double t = tanh(u);
        double phi = 1.0 / b;
        double twon = b * sinh(u);
        *sn = t + ai * (twon - u) / (b * b);
        ai *= t * phi;
        *cn = phi - ai * (twon - u);
        *dn = phi + ai * (twon + u);
        return;
    }
    double a[10];
    double c[10];
    a[0] = 1.0;
    double b = sqrt(1.0 - m);
    c[0] = sqrt(m);
    double twon = 1.0;
    int i = 0;
    while (fabs(c[i] / a[i]) > DBL_EPSILON && i < 8) {
        double ai = a[i];
        ++i;
        c[i] = 0.5 * (ai - b);
        double t = sqrt(ai * b);
        a[i] = 0.5 * (ai + b);
        b = t;
        twon *= 2.0;
    }
    double phi = twon * a[i] * u;
    double prev = phi;
    do {
        double t = c[i] * sin(phi) / a[i];
        prev = phi;
        phi = 0.5 * (asin(t) + phi);
    } while (--i);
    *sn = sin(phi);
    *cn = cos(phi);
    *dn = *cn / cos(phi - prev);
}

// Carlson's symmetric integral R_F(x, y, z), by duplication
static double carlson_rf(double x, double y, double z) {
    for (int i = 0; i < 100; i++) {
        double mu = (x + y + z) / 3.0;
        double dx = 1.0 - x / mu;
        double dy = 1.0 - y / mu;
        double dz = 1.0 - z / mu;
        double eps = fmax(fabs(dx), fmax(fabs(dy), fabs(dz)));
        if (eps < 1.0e-4) {
            double e2 = dx * dy - dz * dz;
            double e3 = dx * dy * dz;
            return (1.0 - e2 / 10.0 + e3 / 14.0 + e2 * e2 / 24.0 - 3.0 * e2 * e3 / 44.0) / sqrt(mu);
        }
        double sx = sqrt(x);
        double sy = sqrt(y);
        double sz = sqrt(z);
        double lambda = sx * sy + sy * sz + sz * sx;
        x = 0.25 * (x + lambda);
        y = 0.25 * (y + lambda);
        z = 0.25 * (z + lambda);
    }
    return NAN;
}

/* scipy's _arc_jac_sc1(w, m): the real u with sc(u | 1 - m) = w, that is the
   incomplete integral F(atan(w) | 1 - m). The 1 - (1 - m) sin^2 term is written
   as cos^2 + m sin^2 so a tiny m keeps its digits. */
static double arc_jac_sc1(double w, double m) {
    double phi = atan(w);
    double s = sin(phi);
    double c = cos(phi);
    return s * carlson_rf(c * c, c * c + m * s * s, 1.0);
}

// scipy's _ellipdeg: the degree equation solved through nomes
static double ellipdeg(size_t n, double m1) {
    double k1 = csn_ellipk(m1);
    double k1p = csn_ellipkm1(m1);
    double q1 = exp(-CSN_PI * k1p / k1);
    double q = pow(q1, 1.0 / (double) n);
    double num = 0.0;
    for (int m = 0; m <= 7; m++) num += pow(q, (double) (m * (m + 1)));
    double den = 0.0;
    for (int m = 1; m <= 8; m++) den += pow(q, (double) (m * m));
    den = 1.0 + 2.0 * den;
    double r = num / den;
    return 16.0 * q * r * r * r * r;
}

/* ---------------------------------------------------------------------------
   Analog lowpass prototypes, cutoff 1 rad/s, as scipy's buttap, cheb1ap,
   cheb2ap and ellipap. Zeros and poles go into ztap and ptap, allocated here
   and freed by the caller; the gain into *ktap.
   ------------------------------------------------------------------------- */

static const char *iir_family_name(CSN_IIR_TYPE family) {
    switch (family) {
        case CSN_BUTTER: return "Butterworth";
        case CSN_CHEBY1: return "Chebyshev type I";
        case CSN_CHEBY2: return "Chebyshev type II";
        case CSN_ELLIP: return "elliptic";
        default: return "IIR";
    }
}

static CSN_COMPLEXDAT cmake(double re, double im) {
    CSN_COMPLEXDAT z = { .re = re, .im = im };
    return z;
}

// real(prod(-p) / prod(-z))
static double proto_gain(const CSN_COMPLEXDAT *z, size_t nz, const CSN_COMPLEXDAT *p, size_t np) {
    CSN_COMPLEXDAT num = cmake(1.0, 0.0);
    for (size_t i = 0; i < np; i++) complex_prod(&num, num, cmake(-p[i].re, -p[i].im));
    CSN_COMPLEXDAT den = cmake(1.0, 0.0);
    for (size_t i = 0; i < nz; i++) complex_prod(&den, den, cmake(-z[i].re, -z[i].im));
    CSN_COMPLEXDAT g = {0};
    complex_div(&g, num, den);
    return g.re;
}

static int32_t iir_prototype(CSOUND *csound, const CSN_IIR_SPEC *spec, size_t order, CSN_ARRAY *ztap, CSN_ARRAY *ptap, double *ktap) {
    size_t n = order;
    double nd = (double) n;
    size_t nz = 0;
    // elliptic: N even has N zeros, N odd N - 1; Chebyshev II the same; the others none
    if (spec->family == CSN_CHEBY2 || spec->family == CSN_ELLIP) nz = n - (n % 2);
    if (spec->family == CSN_ELLIP && n == 1) nz = 0;

    uint32_t zshape[CSN_MAX_DIMS] = {0};
    uint32_t pshape[CSN_MAX_DIMS] = {0};
    zshape[0] = (uint32_t) nz;
    pshape[0] = (uint32_t) n;
    if (allocate_array(csound, ztap, 1U, zshape, 0, CSN_COMPLEX) != OK || allocate_array(csound, ptap, 1U, pshape, 0, CSN_COMPLEX) != OK) {
        return csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
    }
    CSN_COMPLEXDAT *z = (CSN_COMPLEXDAT *) ztap->data;
    CSN_COMPLEXDAT *p = (CSN_COMPLEXDAT *) ptap->data;

    switch (spec->family) {
        case CSN_BUTTER:
            buttap_assign(ptap->data, n);
            *ktap = 1.0;
            return OK;

        case CSN_CHEBY1: {
            double eps = sqrt(pow(10.0, 0.1 * spec->rp) - 1.0);
            double mu = asinh(1.0 / eps) / nd;
            for (size_t k = 0; k < n; k++) {
                double theta = CSN_PI * (2.0 * (double) k - nd + 1.0) / (2.0 * nd);
                // -sinh(mu + j theta)
                p[k] = cmake(-sinh(mu) * cos(theta), -cosh(mu) * sin(theta));
            }
            *ktap = proto_gain(z, 0, p, n);
            if (n % 2 == 0) *ktap /= sqrt(1.0 + eps * eps);
            return OK;
        }

        case CSN_CHEBY2: {
            double de = 1.0 / sqrt(pow(10.0, 0.1 * spec->rs) - 1.0);
            double mu = asinh(1.0 / de) / nd;
            // zeros j / sin(m pi / 2N), m = -N+1, -N+3, ..., N-1 without 0
            size_t zi = 0;
            for (size_t k = 0; k < n; k++) {
                double m = 2.0 * (double) k - nd + 1.0;
                if (m == 0.0) continue;
                z[zi++] = cmake(0.0, 1.0 / sin(m * CSN_PI / (2.0 * nd)));
            }
            // Butterworth poles warped into Chebyshev II, then inverted
            for (size_t k = 0; k < n; k++) {
                double theta = CSN_PI * (2.0 * (double) k - nd + 1.0) / (2.0 * nd);
                CSN_COMPLEXDAT w = cmake(sinh(mu) * -cos(theta), cosh(mu) * -sin(theta));
                complex_div(&p[k], cmake(1.0, 0.0), w);
            }
            *ktap = proto_gain(z, nz, p, n);
            return OK;
        }

        case CSN_ELLIP: {
            double eps_sq = expm1(log(10.0) * 0.1 * spec->rp);
            if (n == 1) {
                p[0] = cmake(-sqrt(1.0 / eps_sq), 0.0);
                *ktap = -p[0].re;
                return OK;
            }
            double eps = sqrt(eps_sq);
            double ck1_sq = eps_sq / expm1(log(10.0) * 0.1 * spec->rs);
            if (ck1_sq == 0.0) {
                return csound->InitError(csound, "[csnarray] Cannot design an elliptic filter with rp = %g dB and rs = %g dB", spec->rp, spec->rs);
            }
            double val0 = csn_ellipk(ck1_sq);
            double m = ellipdeg(n, ck1_sq);
            double capk = csn_ellipk(m);

            // j = 1 - N%2, 3 - N%2, ... < N
            size_t jj = 0;
            double s[CSN_MAX_ELLIP_ORDER];
            double c[CSN_MAX_ELLIP_ORDER];
            double d[CSN_MAX_ELLIP_ORDER];
            for (size_t j = 1 - (n % 2); j < n; j += 2) {
                csn_ellipj((double) j * capk / nd, m, &s[jj], &c[jj], &d[jj]);
                jj++;
            }
            // zeros j / (sqrt(m) sn) and their conjugates, dropping sn = 0
            size_t zi = 0;
            for (size_t i = 0; i < jj; i++) {
                if (fabs(s[i]) <= 2.0e-16) continue;
                z[zi++] = cmake(0.0, 1.0 / (sqrt(m) * s[i]));
            }
            size_t half = zi;
            for (size_t i = 0; i < half; i++) z[zi++] = cmake(0.0, -z[i].im);

            double r = arc_jac_sc1(1.0 / eps, ck1_sq);
            double v0 = capk * r / (nd * val0);
            double sv, cv, dv;
            csn_ellipj(v0, 1.0 - m, &sv, &cv, &dv);

            CSN_COMPLEXDAT pj[CSN_MAX_ELLIP_ORDER];
            double norm = 0.0;
            for (size_t i = 0; i < jj; i++) {
                double den = 1.0 - (d[i] * sv) * (d[i] * sv);
                pj[i] = cmake(-(c[i] * d[i] * sv * cv) / den, -(s[i] * dv) / den);
                norm += pj[i].re * pj[i].re + pj[i].im * pj[i].im;
            }
            norm = sqrt(norm);
            size_t pi = 0;
            for (size_t i = 0; i < jj; i++) p[pi++] = pj[i];
            for (size_t i = 0; i < jj; i++) {
                // odd N: the real pole has no conjugate to add
                if (n % 2 == 1 && fabs(pj[i].im) <= 2.0e-16 * norm) continue;
                p[pi++] = cmake(pj[i].re, -pj[i].im);
            }
            *ktap = proto_gain(z, nz, p, n);
            if (n % 2 == 0) *ktap /= sqrt(1.0 + eps_sq);
            return OK;
        }

        default:
            return csound->InitError(csound, "[csnarray] Internal error: unknown filter family");
    }
}

/* ---------------------------------------------------------------------------
   The opcode side shared by the four families: argument checks, the
   pre-warped cutoffs, and the choice between b, a and second-order sections.
   ------------------------------------------------------------------------- */

// order, type and fs; the type must be one of the two a cutoff count allows
static int32_t iir_check_common(CSOUND *csound, const CSN_IIR_SPEC *spec, double ord_value, double type_value, double fs, bool band, size_t *order) {
    if (!IS_VALID_LENGTH(ord_value) || ord_value < 1.0) {
        return csound->InitError(csound, "[csnarray] Invalid %s order %g: it must be an integer greater or equal to one", iir_family_name(spec->family), ord_value);
    }
    if (spec->family == CSN_ELLIP && ord_value > (double) CSN_MAX_ELLIP_ORDER) {
        return csound->InitError(csound, "[csnarray] Invalid elliptic order %g: it must not exceed %d", ord_value, CSN_MAX_ELLIP_ORDER);
    }
    if ((spec->family == CSN_CHEBY1 || spec->family == CSN_ELLIP) && (!isfinite(spec->rp) || spec->rp <= 0.0)) {
        return csound->InitError(csound, "[csnarray] Invalid passband ripple %g dB: it must be finite and greater than zero", spec->rp);
    }
    if ((spec->family == CSN_CHEBY2 || spec->family == CSN_ELLIP) && (!isfinite(spec->rs) || spec->rs <= 0.0)) {
        return csound->InitError(csound, "[csnarray] Invalid stopband attenuation %g dB: it must be finite and greater than zero", spec->rs);
    }
    if (spec->family == CSN_ELLIP && spec->rs <= spec->rp) {
        return csound->InitError(csound, "[csnarray] Invalid elliptic specification: the stopband attenuation (%g dB) must exceed the passband ripple (%g dB)", spec->rs, spec->rp);
    }
    bool is_band_type = type_value == 2.0 || type_value == 3.0;
    bool is_single_type = type_value == 0.0 || type_value == 1.0;
    if (!is_band_type && !is_single_type) {
        return csound->InitError(csound, "[csnarray] Invalid filter type %g: it must be 0 = lowpass, 1 = highpass, 2 = bandpass, 3 = bandstop", type_value);
    }
    if (band && !is_band_type) {
        return csound->InitError(csound, "[csnarray] Two cutoff frequencies are only for a bandpass (2) or a bandstop (3)");
    }
    if (!band && !is_single_type) {
        return csound->InitError(csound, "[csnarray] A single cutoff frequency is only for a lowpass (0) or a highpass (1)");
    }
    if (!IS_VALID_SR(fs)) {
        return csound->InitError(csound, "[csnarray] Invalid sample rate %g: it must be a positive integer", fs);
    }
    *order = (size_t) ord_value;
    return OK;
}

int32_t csnsig_fdesign1f1_sos_deinit(CSOUND *csound, CSN_FDESIGN1F1_SOS *p) {
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

int32_t csnsig_fdesign1f2_sos_deinit(CSOUND *csound, CSN_FDESIGN1F2_SOS *p) {
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

static int32_t iir_design_and_publish(CSOUND *csound, OPDS *h, CSNREF *handle_a, CSNREF *handle_b, CSN_ARRAY **array_a, CSN_ARRAY **array_b, const CSN_IIR_SPEC *spec, size_t order, CSN_PF_DESIGN_MODE pf, double w0, double bw, double fs, bool is_sos) {
    CSN_ARRAY ztap = {0};
    CSN_ARRAY ptap = {0};
    double ktap = 1.0;
    int32_t res = iir_prototype(csound, spec, order, &ztap, &ptap, &ktap);
    if (res == OK) {
        if (is_sos) {
            res = iir_zpk_to_sos(csound, h, handle_a, array_a, &ztap, &ptap, ktap, pf, w0, bw, fs);
        } else {
            res = iir_zpk_to_ba(csound, h, handle_a, handle_b, array_a, array_b, &ztap, &ptap, ktap, pf, w0, bw, fs);
        }
    }
    if (ztap.data != NULL) csound->Free(csound, ztap.data);
    if (ptap.data != NULL) csound->Free(csound, ptap.data);
    return res;
}

static int32_t iir_band_helper(CSOUND *csound, OPDS *h, CSNREF *handle_a, CSNREF *handle_b, CSN_ARRAY **array_a, CSN_ARRAY **array_b, const CSN_IIR_SPEC *spec, const MYFLT *arg_order, const ARRAYDAT *fcut, const MYFLT *arg_type, const MYFLT *arg_fs, bool is_sos) {
    size_t order = 0;
    double fs = (double) *arg_fs;
    if (iir_check_common(csound, spec, (double) *arg_order, (double) *arg_type, fs, true, &order) != OK) return NOTOK;
    CSN_FILTER_TYPE ftype = (CSN_FILTER_TYPE) *arg_type;

    if (fcut == NULL || fcut->data == NULL || fcut->sizes == NULL || fcut->dimensions != 1 || fcut->sizes[0] != 2) {
        return csound->InitError(csound, "[csnarray] The cutoff must be a 1-D array of two frequencies, low and high (an inline array at global scope arrives empty: bind it to a named i-array first)");
    }
    double f1 = (double) fcut->data[0];
    double f2 = (double) fcut->data[1];
    if (!isfinite(f1) || !isfinite(f2) || f1 <= 0.0 || f2 <= f1 || f2 >= fs * 0.5) {
        return csound->InitError(csound, "[csnarray] Invalid band [%g, %g]: it must satisfy 0 < low < high < fs/2 (%g)", f1, f2, fs * 0.5);
    }

    // pre-warp the band edges so the digital filter hits them exactly
    double w1 = 2.0 * fs * tan(CSN_PI * f1 / fs);
    double w2 = 2.0 * fs * tan(CSN_PI * f2 / fs);
    CSN_PF_DESIGN_MODE pf = ftype == CSN_BP ? LP2BP_ZPK : LP2BS_ZPK;
    return iir_design_and_publish(csound, h, handle_a, handle_b, array_a, array_b, spec, order, pf, sqrt(w1 * w2), w2 - w1, fs, is_sos);
}

static int32_t iir_noband_helper(CSOUND *csound, OPDS *h, CSNREF *handle_a, CSNREF *handle_b, CSN_ARRAY **array_a, CSN_ARRAY **array_b, const CSN_IIR_SPEC *spec, const MYFLT *arg_order, const MYFLT *fcut, const MYFLT *arg_type, const MYFLT *arg_fs, bool is_sos) {
    size_t order = 0;
    double fs = (double) *arg_fs;
    if (iir_check_common(csound, spec, (double) *arg_order, (double) *arg_type, fs, false, &order) != OK) return NOTOK;
    CSN_FILTER_TYPE ftype = (CSN_FILTER_TYPE) *arg_type;

    double fc = (double) *fcut;
    if (!isfinite(fc) || fc <= 0.0 || fc >= fs * 0.5) {
        return csound->InitError(csound, "[csnarray] Invalid cutoff %g: it must satisfy 0 < fc < fs/2 (%g)", fc, fs * 0.5);
    }

    // pre-warp the cutoff so the digital filter hits it exactly
    double w0 = 2.0 * fs * tan(CSN_PI * fc / fs);
    CSN_PF_DESIGN_MODE pf = ftype == CSN_LP ? LP2LP_ZPK : LP2HP_ZPK;
    return iir_design_and_publish(csound, h, handle_a, handle_b, array_a, array_b, spec, order, pf, w0, 0.0, fs, is_sos);
}

/* Entry points. Butterworth: order, cutoff, type, fs. Chebyshev: order,
   cutoff, rp or rs, type, fs. Elliptic: order, cutoff, rp, rs, type, fs. */

int32_t csnsig_butter_band(CSOUND *csound, CSN_FDESIGN1F2 *p) {
    IIR_SPEC(CSN_BUTTER, 0.0, 0.0);
    return iir_band_helper(csound, &p->h, p->handle_a, p->handle_b, &p->array_a, &p->array_b, &spec, p->arg_a, p->fcut, p->arg_b, p->arg_c, false);
}

int32_t csnsig_butter_noband(CSOUND *csound, CSN_FDESIGN1F1 *p) {
    IIR_SPEC(CSN_BUTTER, 0.0, 0.0);
    return iir_noband_helper(csound, &p->h, p->handle_a, p->handle_b, &p->array_a, &p->array_b, &spec, p->arg_a, p->fcut, p->arg_b, p->arg_c, false);
}

int32_t csnsig_butter_band_sos(CSOUND *csound, CSN_FDESIGN1F2_SOS *p) {
    IIR_SPEC(CSN_BUTTER, 0.0, 0.0);
    return iir_band_helper(csound, &p->h, p->handle, NULL, &p->array, NULL, &spec, p->arg_a, p->fcut, p->arg_b, p->arg_c, true);
}

int32_t csnsig_butter_noband_sos(CSOUND *csound, CSN_FDESIGN1F1_SOS *p) {
    IIR_SPEC(CSN_BUTTER, 0.0, 0.0);
    return iir_noband_helper(csound, &p->h, p->handle, NULL, &p->array, NULL, &spec, p->arg_a, p->fcut, p->arg_b, p->arg_c, true);
}

int32_t csnsig_cheby1_band(CSOUND *csound, CSN_FDESIGN1F2 *p) {
    IIR_SPEC(CSN_CHEBY1, (double) *p->arg_b, 0.0);
    return iir_band_helper(csound, &p->h, p->handle_a, p->handle_b, &p->array_a, &p->array_b, &spec, p->arg_a, p->fcut, p->arg_c, p->arg_d, false);
}

int32_t csnsig_cheby1_noband(CSOUND *csound, CSN_FDESIGN1F1 *p) {
    IIR_SPEC(CSN_CHEBY1, (double) *p->arg_b, 0.0);
    return iir_noband_helper(csound, &p->h, p->handle_a, p->handle_b, &p->array_a, &p->array_b, &spec, p->arg_a, p->fcut, p->arg_c, p->arg_d, false);
}

int32_t csnsig_cheby1_band_sos(CSOUND *csound, CSN_FDESIGN1F2_SOS *p) {
    IIR_SPEC(CSN_CHEBY1, (double) *p->arg_b, 0.0);
    return iir_band_helper(csound, &p->h, p->handle, NULL, &p->array, NULL, &spec, p->arg_a, p->fcut, p->arg_c, p->arg_d, true);
}

int32_t csnsig_cheby1_noband_sos(CSOUND *csound, CSN_FDESIGN1F1_SOS *p) {
    IIR_SPEC(CSN_CHEBY1, (double) *p->arg_b, 0.0);
    return iir_noband_helper(csound, &p->h, p->handle, NULL, &p->array, NULL, &spec, p->arg_a, p->fcut, p->arg_c, p->arg_d, true);
}

int32_t csnsig_cheby2_band(CSOUND *csound, CSN_FDESIGN1F2 *p) {
    IIR_SPEC(CSN_CHEBY2, 0.0, (double) *p->arg_b);
    return iir_band_helper(csound, &p->h, p->handle_a, p->handle_b, &p->array_a, &p->array_b, &spec, p->arg_a, p->fcut, p->arg_c, p->arg_d, false);
}

int32_t csnsig_cheby2_noband(CSOUND *csound, CSN_FDESIGN1F1 *p) {
    IIR_SPEC(CSN_CHEBY2, 0.0, (double) *p->arg_b);
    return iir_noband_helper(csound, &p->h, p->handle_a, p->handle_b, &p->array_a, &p->array_b, &spec, p->arg_a, p->fcut, p->arg_c, p->arg_d, false);
}

int32_t csnsig_cheby2_band_sos(CSOUND *csound, CSN_FDESIGN1F2_SOS *p) {
    IIR_SPEC(CSN_CHEBY2, 0.0, (double) *p->arg_b);
    return iir_band_helper(csound, &p->h, p->handle, NULL, &p->array, NULL, &spec, p->arg_a, p->fcut, p->arg_c, p->arg_d, true);
}

int32_t csnsig_cheby2_noband_sos(CSOUND *csound, CSN_FDESIGN1F1_SOS *p) {
    IIR_SPEC(CSN_CHEBY2, 0.0, (double) *p->arg_b);
    return iir_noband_helper(csound, &p->h, p->handle, NULL, &p->array, NULL, &spec, p->arg_a, p->fcut, p->arg_c, p->arg_d, true);
}

int32_t csnsig_ellip_band(CSOUND *csound, CSN_FDESIGN1F2 *p) {
    IIR_SPEC(CSN_ELLIP, (double) *p->arg_b, (double) *p->arg_c);
    return iir_band_helper(csound, &p->h, p->handle_a, p->handle_b, &p->array_a, &p->array_b, &spec, p->arg_a, p->fcut, p->arg_d, p->arg_e, false);
}

int32_t csnsig_ellip_noband(CSOUND *csound, CSN_FDESIGN1F1 *p) {
    IIR_SPEC(CSN_ELLIP, (double) *p->arg_b, (double) *p->arg_c);
    return iir_noband_helper(csound, &p->h, p->handle_a, p->handle_b, &p->array_a, &p->array_b, &spec, p->arg_a, p->fcut, p->arg_d, p->arg_e, false);
}

int32_t csnsig_ellip_band_sos(CSOUND *csound, CSN_FDESIGN1F2_SOS *p) {
    IIR_SPEC(CSN_ELLIP, (double) *p->arg_b, (double) *p->arg_c);
    return iir_band_helper(csound, &p->h, p->handle, NULL, &p->array, NULL, &spec, p->arg_a, p->fcut, p->arg_d, p->arg_e, true);
}

int32_t csnsig_ellip_noband_sos(CSOUND *csound, CSN_FDESIGN1F1_SOS *p) {
    IIR_SPEC(CSN_ELLIP, (double) *p->arg_b, (double) *p->arg_c);
    return iir_noband_helper(csound, &p->h, p->handle, NULL, &p->array, NULL, &spec, p->arg_a, p->fcut, p->arg_d, p->arg_e, true);
}

/* ---------------------------------------------------------------------------
   lfilter / sosfilter: direct form II transposed, as scipy.signal.lfilter and
   sosfilt. Coefficients are copied at init, normalised and widened to complex,
   so one set of loops serves real and complex data alike; the audio forms keep
   a real-only loop.
   ------------------------------------------------------------------------- */

/* b and a divided by a[0], both zero-padded to order + 1, as complex arrays.
   order = max(len b, len a) - 1. */
static int32_t load_tf_coeffs(CSOUND *csound, const CSN_ARRAY *b_arr, const CSN_ARRAY *a_arr, CSN_ARRAY *b_buf, CSN_ARRAY *a_buf, size_t *order) {
    if (b_arr->ndim != 1U || a_arr->ndim != 1U) {
        return csound->InitError(csound, "[csnarray] b and a must be 1-D arrays");
    }
    if (b_arr->size == 0 || a_arr->size == 0) {
        return csound->InitError(csound, "[csnarray] b and a must hold at least one coefficient");
    }
    CSN_COMPLEXDAT a0 = slice_get(a_arr->data, 0, 1, a_arr->itype);
    if (a0.re == 0.0 && a0.im == 0.0) {
        return csound->InitError(csound, "[csnarray] The first denominator coefficient a[0] must be non-zero");
    }
    size_t n = (b_arr->size > a_arr->size ? b_arr->size : a_arr->size);
    uint32_t shape[CSN_MAX_DIMS] = {0};
    shape[0] = (uint32_t) n;
    if (allocate_array(csound, b_buf, 1U, shape, 0, CSN_COMPLEX) != OK || allocate_array(csound, a_buf, 1U, shape, 0, CSN_COMPLEX) != OK) {
        return csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
    }
    CSN_COMPLEXDAT *b = (CSN_COMPLEXDAT *) b_buf->data;
    CSN_COMPLEXDAT *a = (CSN_COMPLEXDAT *) a_buf->data;
    for (size_t i = 0; i < n; i++) {
        CSN_COMPLEXDAT zero = {0};
        CSN_COMPLEXDAT bi = i < b_arr->size ? slice_get(b_arr->data, i, 1, b_arr->itype) : zero;
        CSN_COMPLEXDAT ai = i < a_arr->size ? slice_get(a_arr->data, i, 1, a_arr->itype) : zero;
        complex_div(&b[i], bi, a0);
        complex_div(&a[i], ai, a0);
    }
    *order = n - 1;
    return OK;
}

/* An (n, 6) sos array copied as complex; every a0 must be exactly 1, as
   scipy.signal.sosfilt requires. */
static int32_t load_sos(CSOUND *csound, const CSN_ARRAY *sos_arr, CSN_ARRAY *sos_buf, size_t *n_sections) {
    if (sos_arr->ndim != 2U || sos_arr->shape[1] != 6U || sos_arr->shape[0] == 0U) {
        return csound->InitError(csound, "[csnarray] sos must be a 2-D array of at least one section of six coefficients [b0 b1 b2 a0 a1 a2]");
    }
    size_t n = sos_arr->shape[0];
    for (size_t s = 0; s < n; s++) {
        CSN_COMPLEXDAT a0 = slice_get(sos_arr->data, s * 6 + 3, 1, sos_arr->itype);
        if (a0.re != 1.0 || a0.im != 0.0) {
            return csound->InitError(csound, "[csnarray] Section %zu has a0 = %g: every sos[:, 3] must be 1", s, a0.re);
        }
    }
    if (allocate_array(csound, sos_buf, 2U, sos_arr->shape, 0, CSN_COMPLEX) != OK) {
        return csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
    }
    for (size_t i = 0; i < n * 6; i++) {
        slice_put(sos_buf->data, i, 1, CSN_COMPLEX, slice_get(sos_arr->data, i, 1, sos_arr->itype));
    }
    *n_sections = n;
    return OK;
}

/* One sample through the transposed direct form II of order N, with b and a
   already divided by a[0]:

       y      = b[0] x + z[0]
       z[j]   = b[j+1] x - a[j+1] y + z[j+1]      j = 0 .. N-2
       z[N-1] = b[N] x - a[N] y

   z holds the N delayed partial sums and is updated in place. */
static CSN_COMPLEXDAT df2t_step(const CSN_COMPLEXDAT *b, const CSN_COMPLEXDAT *a, size_t order, CSN_COMPLEXDAT *z, CSN_COMPLEXDAT x) {
    CSN_COMPLEXDAT y;
    complex_prod(&y, b[0], x);
    if (order == 0) return y;
    complex_add(&y, y, z[0]);

    for (size_t j = 0; j < order; j++) {
        CSN_COMPLEXDAT bx, ay;
        complex_prod(&bx, b[j + 1], x);
        complex_prod(&ay, a[j + 1], y);
        complex_sub(&z[j], bx, ay);
        if (j + 1 < order) complex_add(&z[j], z[j], z[j + 1]);
    }
    return y;
}

// the positions of the six coefficients in a section row [b0 b1 b2 a0 a1 a2]
enum { SOS_B0, SOS_B1, SOS_B2, SOS_A0, SOS_A1, SOS_A2 };

/* One sample through one second-order section: the same form with N = 2
   and a0 = 1, its two states in z[0], z[1]. */
static CSN_COMPLEXDAT sos_section_step(const CSN_COMPLEXDAT *sec, CSN_COMPLEXDAT *z, CSN_COMPLEXDAT x) {
    CSN_COMPLEXDAT y, bx, ay;

    // y = b0 x + z[0]
    complex_prod(&y, sec[SOS_B0], x);
    complex_add(&y, y, z[0]);

    // z[0] = b1 x - a1 y + z[1]
    complex_prod(&bx, sec[SOS_B1], x);
    complex_prod(&ay, sec[SOS_A1], y);
    complex_sub(&z[0], bx, ay);
    complex_add(&z[0], z[0], z[1]);

    // z[1] = b2 x - a2 y
    complex_prod(&bx, sec[SOS_B2], x);
    complex_prod(&ay, sec[SOS_A2], y);
    complex_sub(&z[1], bx, ay);

    return y;
}

/* One slice of n samples, read from x with stride sx and written to y with
   stride sy, through the transposed direct form II. */
static void df2t_slice(const CSN_COMPLEXDAT *b, const CSN_COMPLEXDAT *a, size_t order, CSN_COMPLEXDAT *z,
                       const double *x, size_t sx, ITEM_TYPE xt, double *y, size_t sy, ITEM_TYPE yt, size_t n) {
    for (size_t i = 0; i < n; i++) {
        CSN_COMPLEXDAT out = df2t_step(b, a, order, z, slice_get(x, i, sx, xt));
        slice_put(y, i, sy, yt, out);
    }
}

/* The same slice through the cascade: each section feeds the next, and each
   keeps its two states at z + 2 * section. */
static void sos_slice(const CSN_COMPLEXDAT *sos, size_t n_sections, CSN_COMPLEXDAT *z,
                      const double *x, size_t sx, ITEM_TYPE xt, double *y, size_t sy, ITEM_TYPE yt, size_t n) {
    for (size_t i = 0; i < n; i++) {
        CSN_COMPLEXDAT v = slice_get(x, i, sx, xt);
        for (size_t s = 0; s < n_sections; s++) {
            v = sos_section_step(sos + 6 * s, z + 2 * s, v);
        }
        slice_put(y, i, sy, yt, v);
    }
}

/* Runs every slice along the axis; the states of slice k start at
   state + k * per_slice. The caller sets them first: zeros when the filter
   starts from rest, zi in the explicit-state forms. */
static int32_t lfilter_slices(CSOUND *csound, OPDS *perf_h, const CSN_ARRAY *source_arr, CSN_ARRAY *arr, uint32_t axis, const CSN_COMPLEXDAT *b, const CSN_COMPLEXDAT *a, size_t order, CSN_COMPLEXDAT *state, size_t per_slice) {
    if (source_arr->size == 0) return OK;
    CSN_AXIS_SLICE_ITER it = {0};
    if (AXIS_ITER_SLICE_INIT(&it, source_arr, arr, axis) != OK) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Internal error: incompatible filter array shapes/ranks");
    }
    size_t k = 0;
    while (AXIS_SLICE_ITER_NEXT(&it)) {
        const double *src = source_arr->data + it.src_base * source_arr->itype;
        double *dst = arr->data + it.dst_base * arr->itype;
        CSN_COMPLEXDAT *z = state + k * per_slice;
        df2t_slice(b, a, order, z, src, it.src_axis_stride, source_arr->itype, dst, it.dst_axis_stride, arr->itype, it.axis_size);
        k++;
    }
    return OK;
}

static int32_t sosfilter_slices(CSOUND *csound, OPDS *perf_h, const CSN_ARRAY *source_arr, CSN_ARRAY *arr, uint32_t axis, const CSN_COMPLEXDAT *sos, size_t n_sections, CSN_COMPLEXDAT *state, size_t per_slice) {
    if (source_arr->size == 0) return OK;
    CSN_AXIS_SLICE_ITER it = {0};
    if (AXIS_ITER_SLICE_INIT(&it, source_arr, arr, axis) != OK) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Internal error: incompatible filter array shapes/ranks");
    }
    size_t k = 0;
    while (AXIS_SLICE_ITER_NEXT(&it)) {
        const double *src = source_arr->data + it.src_base * source_arr->itype;
        double *dst = arr->data + it.dst_base * arr->itype;
        CSN_COMPLEXDAT *z = state + k * per_slice;
        sos_slice(sos, n_sections, z, src, it.src_axis_stride, source_arr->itype, dst, it.dst_axis_stride, arr->itype, it.axis_size);
        k++;
    }
    return OK;
}

static size_t slice_count_of(const CSN_ARRAY *arr, uint32_t axis) {
    if (arr->size == 0 || axis >= arr->ndim || arr->shape[axis] == 0) return 0;
    return arr->size / arr->shape[axis];
}

static bool coef_array_is_complex(const CSN_ARRAY *buf) {
    const CSN_COMPLEXDAT *c = (const CSN_COMPLEXDAT *) buf->data;
    for (size_t i = 0; i < buf->size; i++) if (c[i].im != 0.0) return true;
    return false;
}

/* i-rate, k-rate and audio share the axis handling: omitted means the last
   axis, anything else must name an axis of the source */
static int32_t filter_axis(CSOUND *csound, const MYFLT *axis_in, uint32_t ndim, CSN_AXIS_SPEC *spec) {
    *spec = csn_normalize_axis(axis_in, ndim, CSN_AXIS_DEFAULT_LAST);
    if (spec->kind == CSN_AXIS_INVALID) {
        double value = axis_in == NULL ? 0.0 : (double) *axis_in;
        return csound->InitError(csound, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: finite integers %d..%u)", value, ndim, -(int32_t) ndim, ndim - 1U);
    }
    return OK;
}

/* ---- audio ---------------------------------------------------------------- */

int32_t csnsig_lfilter_audio_deinit(CSOUND *csound, CSN_LFILTER_AUDIO *p) {
    deinit_scratch(csound, &p->filter_state);
    FREE_CSNARRDATA(csound, &p->b_buffer);
    FREE_CSNARRDATA(csound, &p->a_buffer);
    return OK;
}

int32_t csnsig_sosfilter_audio_deinit(CSOUND *csound, CSN_SOSFILTER_AUDIO *p) {
    deinit_scratch(csound, &p->filter_state);
    FREE_CSNARRDATA(csound, &p->sos_buffer);
    return OK;
}

int32_t csnsig_lfilter_audio_init(CSOUND *csound, CSN_LFILTER_AUDIO *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle_b = p->b->id;
    uint32_t source_handle_a = p->a->id;
    int32_t res = OK;

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot_b = get_slot(reg, source_handle_b);
    CSN_SLOT *slot_a = get_slot(reg, source_handle_a);
    if (slot_b == NULL || slot_a == NULL) {
        uint32_t missing = slot_b == NULL ? source_handle_b : source_handle_a;
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", missing);
        goto done;
    }
    CSN_ARRAY *b_arr = slot_b->array;
    CSN_ARRAY *a_arr = slot_a->array;
    if (b_arr->itype == CSN_COMPLEX || a_arr->itype == CSN_COMPLEX) {
        res = csound->InitError(csound, "[csnarray] The audio lfilter needs real b and a");
        goto done;
    }

    FREE_CSNARRDATA(csound, &p->b_buffer);
    FREE_CSNARRDATA(csound, &p->a_buffer);
    res = load_tf_coeffs(csound, b_arr, a_arr, &p->b_buffer, &p->a_buffer, &p->order);
    if (res != OK) goto done;

    // real states only: `order` doubles, zeroed
    size_t n_states = p->order > 0 ? p->order : 1;
    res = csn_scratch_reserve(csound, NULL, false, &p->filter_state, n_states, sizeof(double));
    if (res != OK) goto done;
    memset(p->filter_state.scratch, 0, sizeof(double) * n_states);

    set_array_version(&p->b_version, &b_arr->version);
    set_array_version(&p->a_version, &a_arr->version);
    p->registry = reg;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnsig_lfilter_audio(CSOUND *csound, CSN_LFILTER_AUDIO *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    uint32_t source_handle_b = p->b->id;
    uint32_t source_handle_a = p->a->id;

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot_b = get_slot(reg, source_handle_b);
    CSN_SLOT *slot_a = get_slot(reg, source_handle_a);
    int32_t res = OK;
    if (slot_b == NULL || slot_a == NULL) {
        uint32_t missing = slot_b == NULL ? source_handle_b : source_handle_a;
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", missing);
    } else if (!is_same_array_version(&p->b_version, &slot_b->array->version) || !is_same_array_version(&p->a_version, &slot_a->array->version)) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] b or a changed after the filter was initialised");
    }
    csound->UnlockMutex(reg->mutex);
    if (res != OK) return res;

    uint32_t offset = p->h.insdshead->ksmps_offset;
    uint32_t early = p->h.insdshead->ksmps_no_end;
    uint32_t nsmps = CS_KSMPS;
    if (UNLIKELY(offset)) memset(p->sig_out, 0, offset * sizeof(MYFLT));
    if (UNLIKELY(early)) {
        nsmps -= early;
        memset(&p->sig_out[nsmps], 0, early * sizeof(MYFLT));
    }

    const CSN_COMPLEXDAT *b = (const CSN_COMPLEXDAT *) p->b_buffer.data;
    const CSN_COMPLEXDAT *a = (const CSN_COMPLEXDAT *) p->a_buffer.data;
    double *z = (double *) p->filter_state.scratch;
    size_t order = p->order;
    for (uint32_t i = offset; i < nsmps; i++) {
        double xn = (double) p->sig_in[i];
        double yn = b[0].re * xn + (order > 0 ? z[0] : 0.0);
        for (size_t j = 0; j < order; j++) {
            z[j] = b[j + 1].re * xn - a[j + 1].re * yn + (j + 1 < order ? z[j + 1] : 0.0);
        }
        p->sig_out[i] = (MYFLT) yn;
    }
    return OK;
}

int32_t csnsig_sosfilter_audio_init(CSOUND *csound, CSN_SOSFILTER_AUDIO *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->sos->id;
    int32_t res = OK;

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
        goto done;
    }
    CSN_ARRAY *sos_arr = slot->array;
    if (sos_arr->itype == CSN_COMPLEX) {
        res = csound->InitError(csound, "[csnarray] The audio sosfilter needs real sections");
        goto done;
    }

    FREE_CSNARRDATA(csound, &p->sos_buffer);
    res = load_sos(csound, sos_arr, &p->sos_buffer, &p->n_sections);
    if (res != OK) goto done;

    size_t n_states = 2 * p->n_sections;
    res = csn_scratch_reserve(csound, NULL, false, &p->filter_state, n_states, sizeof(double));
    if (res != OK) goto done;
    memset(p->filter_state.scratch, 0, sizeof(double) * n_states);

    set_array_version(&p->sos_version, &sos_arr->version);
    p->registry = reg;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnsig_sosfilter_audio(CSOUND *csound, CSN_SOSFILTER_AUDIO *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    uint32_t source_handle = p->sos->id;

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot = get_slot(reg, source_handle);
    int32_t res = OK;
    if (slot == NULL) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
    } else if (!is_same_array_version(&p->sos_version, &slot->array->version)) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] sos changed after the filter was initialised");
    }
    csound->UnlockMutex(reg->mutex);
    if (res != OK) return res;

    uint32_t offset = p->h.insdshead->ksmps_offset;
    uint32_t early = p->h.insdshead->ksmps_no_end;
    uint32_t nsmps = CS_KSMPS;
    if (UNLIKELY(offset)) memset(p->sig_out, 0, offset * sizeof(MYFLT));
    if (UNLIKELY(early)) {
        nsmps -= early;
        memset(&p->sig_out[nsmps], 0, early * sizeof(MYFLT));
    }

    const CSN_COMPLEXDAT *sos = (const CSN_COMPLEXDAT *) p->sos_buffer.data;
    double *z = (double *) p->filter_state.scratch;
    size_t n_sections = p->n_sections;
    for (uint32_t i = offset; i < nsmps; i++) {
        double v = (double) p->sig_in[i];
        for (size_t s = 0; s < n_sections; s++) {
            const CSN_COMPLEXDAT *sec = sos + 6 * s;
            double *st = z + 2 * s;
            double y = sec[SOS_B0].re * v + st[0];
            st[0] = sec[SOS_B1].re * v - sec[SOS_A1].re * y + st[1];
            st[1] = sec[SOS_B2].re * v - sec[SOS_A2].re * y;
            v = y;
        }
        p->sig_out[i] = (MYFLT) v;
    }
    return OK;
}

/* ---- arrays --------------------------------------------------------------- */

int32_t csnsig_lfilter_arr_deinit(CSOUND *csound, CSN_LFILTER *p) {
    deinit_scratch(csound, &p->filter_state);
    FREE_CSNARRDATA(csound, &p->b_buffer);
    FREE_CSNARRDATA(csound, &p->a_buffer);
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

int32_t csnsig_sosfilter_arr_deinit(CSOUND *csound, CSN_SOSFILTER *p) {
    deinit_scratch(csound, &p->filter_state);
    FREE_CSNARRDATA(csound, &p->sos_buffer);
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

/* Init of the array forms, i-rate and k-rate: coefficients, axis, output
   array, and a zeroed state for every slice. */
static int32_t lfilter_arr_init(CSOUND *csound, CSN_REGISTRY *reg, CSN_LFILTER *p, const MYFLT *axis_in, CSN_ARRAY **source_out, CSN_ARRAY **b_out, CSN_ARRAY **a_out) {
    const char *err = NULL;
    CSN_SLOT *slot_b = get_slot(reg, p->b->id);
    CSN_SLOT *slot_a = get_slot(reg, p->a->id);
    CSN_SLOT *slot = get_slot(reg, p->source_handle->id);
    uint32_t missing = slot_b == NULL ? p->b->id : slot_a == NULL ? p->a->id : slot == NULL ? p->source_handle->id : 0;
    if (slot_b == NULL || slot_a == NULL || slot == NULL) {
        return csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", missing);
    }
    CSN_ARRAY *source_arr = slot->array;

    int32_t res = filter_axis(csound, axis_in, source_arr->ndim, &p->axis_spec);
    if (res != OK) return res;

    size_t per_slice = 0;
    FREE_CSNARRDATA(csound, &p->b_buffer);
    FREE_CSNARRDATA(csound, &p->a_buffer);
    res = load_tf_coeffs(csound, slot_b->array, slot_a->array, &p->b_buffer, &p->a_buffer, &p->order);
    if (res != OK) return res;
    p->coef_is_complex = coef_array_is_complex(&p->b_buffer) || coef_array_is_complex(&p->a_buffer);
    per_slice = p->order;

    size_t slices = slice_count_of(source_arr, p->axis_spec.index);
    size_t need = per_slice * slices;
    res = csn_scratch_reserve(csound, NULL, false, &p->filter_state, need > 0 ? need : 1, sizeof(CSN_COMPLEXDAT));
    if (res != OK) return res;
    memset(p->filter_state.scratch, 0, sizeof(CSN_COMPLEXDAT) * (need > 0 ? need : 1));
    p->filter_state.current_size = slices;

    ITEM_TYPE out_itype = (source_arr->itype == CSN_COMPLEX || p->coef_is_complex) ? CSN_COMPLEX : CSN_REAL;
    uint32_t protect[3] = { p->b->id, p->a->id, p->source_handle->id };
    if (create_csnarray_locked(csound, reg, &p->h, source_arr->ndim, source_arr->shape, &p->array, p->handle, protect, 3U, &err, out_itype) != OK) {
        return csound->InitError(csound, "[csnarray] %s", err);
    }

    *source_out = source_arr;
    *b_out = slot_b->array;
    *a_out = slot_a->array;
    return OK;
}

static int32_t sosfilter_arr_init(CSOUND *csound, CSN_REGISTRY *reg, CSN_SOSFILTER *p, const MYFLT *axis_in, CSN_ARRAY **source_out, CSN_ARRAY **sos_out) {
    const char *err = NULL;
    CSN_SLOT *slot_sos = get_slot(reg, p->sos->id);
    CSN_SLOT *slot = get_slot(reg, p->source_handle->id);
    uint32_t missing = slot_sos == NULL ? p->sos->id : slot == NULL ? p->source_handle->id : 0;
    if (missing != 0) {
        return csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", missing);
    }
    CSN_ARRAY *source_arr = slot->array;

    int32_t res = filter_axis(csound, axis_in, source_arr->ndim, &p->axis_spec);
    if (res != OK) return res;

    size_t per_slice = 0;
    FREE_CSNARRDATA(csound, &p->sos_buffer);
    res = load_sos(csound, slot_sos->array, &p->sos_buffer, &p->n_sections);
    if (res != OK) return res;
    p->coef_is_complex = coef_array_is_complex(&p->sos_buffer);
    per_slice = 2 * p->n_sections;

    size_t slices = slice_count_of(source_arr, p->axis_spec.index);
    size_t need = per_slice * slices;
    res = csn_scratch_reserve(csound, NULL, false, &p->filter_state, need > 0 ? need : 1, sizeof(CSN_COMPLEXDAT));
    if (res != OK) return res;
    memset(p->filter_state.scratch, 0, sizeof(CSN_COMPLEXDAT) * (need > 0 ? need : 1));
    p->filter_state.current_size = slices;

    ITEM_TYPE out_itype = (source_arr->itype == CSN_COMPLEX || p->coef_is_complex) ? CSN_COMPLEX : CSN_REAL;
    uint32_t protect[2] = { p->sos->id, p->source_handle->id };
    if (create_csnarray_locked(csound, reg, &p->h, source_arr->ndim, source_arr->shape, &p->array, p->handle, protect, 2U, &err, out_itype) != OK) {
        return csound->InitError(csound, "[csnarray] %s", err);
    }

    *source_out = source_arr;
    *sos_out = slot_sos->array;
    return OK;
}

int32_t csnsig_lfilter_arr(CSOUND *csound, CSN_LFILTER *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    const MYFLT *axis_in = p->INOCOUNT == 4 ? p->opt_a : NULL;
    CSN_ARRAY *source_arr = NULL;
    CSN_ARRAY *b_arr = NULL;
    CSN_ARRAY *a_arr = NULL;

    csound->LockMutex(reg->mutex);
    int32_t res = lfilter_arr_init(csound, reg, p, axis_in, &source_arr, &b_arr, &a_arr);
    if (res == OK) {
        const CSN_COMPLEXDAT *b_comp = (const CSN_COMPLEXDAT *) p->b_buffer.data;
        const CSN_COMPLEXDAT *a_comp = (const CSN_COMPLEXDAT *) p->a_buffer.data;
        CSN_COMPLEXDAT *state = (CSN_COMPLEXDAT *) p->filter_state.scratch;
        res = lfilter_slices(csound, NULL, source_arr, p->array, p->axis_spec.index, b_comp, a_comp, p->order, state, p->order);
    }
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnsig_sosfilter_arr(CSOUND *csound, CSN_SOSFILTER *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    const MYFLT *axis_in = p->INOCOUNT == 3 ? p->opt_a : NULL;
    CSN_ARRAY *source_arr = NULL;
    CSN_ARRAY *sos_arr = NULL;

    csound->LockMutex(reg->mutex);
    int32_t res = sosfilter_arr_init(csound, reg, p, axis_in, &source_arr, &sos_arr);
    if (res == OK) {
        const CSN_COMPLEXDAT *sos_comp = (const CSN_COMPLEXDAT *) p->sos_buffer.data;
        CSN_COMPLEXDAT *state = (CSN_COMPLEXDAT *) p->filter_state.scratch;
        res = sosfilter_slices(csound, NULL, source_arr, p->array, p->axis_spec.index, sos_comp, p->n_sections, state, 2 * p->n_sections);
    }
    csound->UnlockMutex(reg->mutex);
    return res;
}

/* k-rate: the state of every slice carries over from one pass to the next, so
   successive blocks filter as one continuous signal, like lfilter with zi set
   to the previous zf. A change in the number of slices restarts the filter. */

int32_t csnsig_lfilter_arr_k_init(CSOUND *csound, CSN_LFILTER *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    const MYFLT *axis_in = p->INOCOUNT == 5 ? p->opt_b : NULL;
    CSN_ARRAY *source_arr = NULL;
    CSN_ARRAY *b_arr = NULL;
    CSN_ARRAY *a_arr = NULL;

    csound->LockMutex(reg->mutex);
    int32_t res = lfilter_arr_init(csound, reg, p, axis_in, &source_arr, &b_arr, &a_arr);
    if (res == OK) {
        SET_KDATA_BEGIN(p, reg);
        set_array_version(&p->b_version, &b_arr->version);
        set_array_version(&p->a_version, &a_arr->version);
        set_array_version(&p->k_data.prev_output_version, &p->array->version);
        set_array_version(&p->k_data.prev_source_version, &source_arr->version);
        p->is_published = false;
    }
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnsig_sosfilter_arr_k_init(CSOUND *csound, CSN_SOSFILTER *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    const MYFLT *axis_in = p->INOCOUNT == 4 ? p->opt_b : NULL;
    CSN_ARRAY *source_arr = NULL;
    CSN_ARRAY *sos_arr = NULL;

    csound->LockMutex(reg->mutex);
    int32_t res = sosfilter_arr_init(csound, reg, p, axis_in, &source_arr, &sos_arr);
    if (res == OK) {
        SET_KDATA_BEGIN(p, reg);
        set_array_version(&p->sos_version, &sos_arr->version);
        set_array_version(&p->k_data.prev_output_version, &p->array->version);
        set_array_version(&p->k_data.prev_source_version, &source_arr->version);
        p->is_published = false;
    }
    csound->UnlockMutex(reg->mutex);
    return res;
}

/* Shared performance pass: republish when nothing moved, otherwise make sure
   the state fits the current slices (restarting it if their count changed)
   and run the next block through. */
static int32_t lfilter_arr_perf(CSOUND *csound, CSN_REGISTRY *reg, CSN_LFILTER *p, CSN_SLOT *slot, const CSN_COMPLEXDAT *b, const CSN_COMPLEXDAT *a) {
    const char *err = NULL;
    uint32_t owned_handle = p->k_data.owned_handle;
    CSN_ARRAY *source_arr = slot->array;

    if (p->is_published) {
        bool is_same_source = is_same_array_version(&p->k_data.prev_source_version, &source_arr->version);
        CSN_SLOT *slot_res = get_slot(reg, owned_handle);
        bool is_same_result = slot_res != NULL && is_same_array_version(&p->k_data.prev_output_version, &slot_res->array->version);
        if (is_same_source && is_same_result) {
            p->handle->id = owned_handle;
            return OK;
        }
    }

    if (p->axis_spec.index >= source_arr->ndim) {
        return csn_locked_perf_error(csound, &p->h, "[csnarray] The source is now %u-D, which has no axis %u", source_arr->ndim, p->axis_spec.index);
    }

    size_t slices = slice_count_of(source_arr, p->axis_spec.index);
    if (slices != p->filter_state.current_size) {
        size_t need = p->order * slices;
        int32_t res = csn_scratch_reserve(csound, &p->h, slot->rt_locked, &p->filter_state, need > 0 ? need : 1, sizeof(CSN_COMPLEXDAT));
        if (res != OK) return res;
        memset(p->filter_state.scratch, 0, sizeof(CSN_COMPLEXDAT) * (need > 0 ? need : 1));
        p->filter_state.current_size = slices;
    }

    ITEM_TYPE out_itype = (source_arr->itype == CSN_COMPLEX || p->coef_is_complex) ? CSN_COMPLEX : CSN_REAL;
    int32_t res = NEED_TO_UPDATE_SLOT(csound, &p->h, &p->array, &p->k_data, NULL, source_arr->ndim, source_arr->shape, source_arr->size, out_itype, err);
    if (res != OK) return res;

    CSN_ARRAY *arr = p->array;
    res = lfilter_slices(csound, &p->h, source_arr, arr, p->axis_spec.index, b, a, p->order, (CSN_COMPLEXDAT *) p->filter_state.scratch, p->order);
    if (res != OK) return res;

    SET_FROM_KDATA_END_WITH_ID(p->k_data, p->handle, source_arr->shape, source_arr->ndim, out_itype);
    set_array_version(&p->k_data.prev_output_version, &arr->version);
    set_array_version(&p->k_data.prev_source_version, &source_arr->version);
    p->is_published = true;
    return OK;
}

static int32_t sosfilter_arr_perf(CSOUND *csound, CSN_REGISTRY *reg, CSN_SOSFILTER *p, CSN_SLOT *slot, const CSN_COMPLEXDAT *sos) {
    const char *err = NULL;
    uint32_t owned_handle = p->k_data.owned_handle;
    CSN_ARRAY *source_arr = slot->array;

    if (p->is_published) {
        bool is_same_source = is_same_array_version(&p->k_data.prev_source_version, &source_arr->version);
        CSN_SLOT *slot_res = get_slot(reg, owned_handle);
        bool is_same_result = slot_res != NULL && is_same_array_version(&p->k_data.prev_output_version, &slot_res->array->version);
        if (is_same_source && is_same_result) {
            p->handle->id = owned_handle;
            return OK;
        }
    }

    if (p->axis_spec.index >= source_arr->ndim) {
        return csn_locked_perf_error(csound, &p->h, "[csnarray] The source is now %u-D, which has no axis %u", source_arr->ndim, p->axis_spec.index);
    }

    size_t per_slice = 2 * p->n_sections;
    size_t slices = slice_count_of(source_arr, p->axis_spec.index);
    if (slices != p->filter_state.current_size) {
        size_t need = per_slice * slices;
        int32_t res = csn_scratch_reserve(csound, &p->h, slot->rt_locked, &p->filter_state, need > 0 ? need : 1, sizeof(CSN_COMPLEXDAT));
        if (res != OK) return res;
        memset(p->filter_state.scratch, 0, sizeof(CSN_COMPLEXDAT) * (need > 0 ? need : 1));
        p->filter_state.current_size = slices;
    }

    ITEM_TYPE out_itype = (source_arr->itype == CSN_COMPLEX || p->coef_is_complex) ? CSN_COMPLEX : CSN_REAL;
    int32_t res = NEED_TO_UPDATE_SLOT(csound, &p->h, &p->array, &p->k_data, NULL, source_arr->ndim, source_arr->shape, source_arr->size, out_itype, err);
    if (res != OK) return res;

    CSN_ARRAY *arr = p->array;
    res = sosfilter_slices(csound, &p->h, source_arr, arr, p->axis_spec.index, sos, p->n_sections, (CSN_COMPLEXDAT *) p->filter_state.scratch, per_slice);
    if (res != OK) return res;

    SET_FROM_KDATA_END_WITH_ID(p->k_data, p->handle, source_arr->shape, source_arr->ndim, out_itype);
    set_array_version(&p->k_data.prev_output_version, &arr->version);
    set_array_version(&p->k_data.prev_source_version, &source_arr->version);
    p->is_published = true;
    return OK;
}

int32_t csnsig_lfilter_arr_k(CSOUND *csound, CSN_LFILTER *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REG_HANDLE(csound, &p->h, reg, p->k_data.owned_handle);

    uint32_t source_handle = p->source_handle->id;
    if (CHECK_SELF_ALIAS(csound, &p->h, &p->k_data, source_handle, 0) != OK) return NOTOK;
    CHECK_KTRIG(p->opt_a);

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot_b = get_slot(reg, p->b->id);
    CSN_SLOT *slot_a = get_slot(reg, p->a->id);
    CSN_SLOT *slot = get_slot(reg, source_handle);
    int32_t res = OK;
    if (slot_b == NULL || slot_a == NULL || slot == NULL) {
        uint32_t missing = slot_b == NULL ? p->b->id : slot_a == NULL ? p->a->id : source_handle;
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", missing);
    } else if (!is_same_array_version(&p->b_version, &slot_b->array->version) || !is_same_array_version(&p->a_version, &slot_a->array->version)) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] b or a changed after the filter was initialised");
    } else {
        res = lfilter_arr_perf(csound, reg, p, slot, (const CSN_COMPLEXDAT *) p->b_buffer.data, (const CSN_COMPLEXDAT *) p->a_buffer.data);
    }
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnsig_sosfilter_arr_k(CSOUND *csound, CSN_SOSFILTER *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REG_HANDLE(csound, &p->h, reg, p->k_data.owned_handle);

    uint32_t source_handle = p->source_handle->id;
    if (CHECK_SELF_ALIAS(csound, &p->h, &p->k_data, source_handle, 0) != OK) return NOTOK;
    CHECK_KTRIG(p->opt_a);

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot_sos = get_slot(reg, p->sos->id);
    CSN_SLOT *slot = get_slot(reg, source_handle);
    int32_t res = OK;
    if (slot_sos == NULL || slot == NULL) {
        uint32_t missing = slot_sos == NULL ? p->sos->id : source_handle;
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", missing);
    } else if (!is_same_array_version(&p->sos_version, &slot_sos->array->version)) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] sos changed after the filter was initialised");
    } else {
        res = sosfilter_arr_perf(csound, reg, p, slot, (const CSN_COMPLEXDAT *) p->sos_buffer.data);
    }
    csound->UnlockMutex(reg->mutex);
    return res;
}

/* ---------------------------------------------------------------------------
   Explicit state, as scipy's lfilter(b, a, x, zi=zi) -> (y, zf) and
   sosfilt(sos, x, zi=zi) -> (y, zf). Nothing is kept between calls: every
   call, and every pass at k-rate and audio rate, starts from the zi it is
   given and hands the final state back as zf, which the caller feeds in as
   the next zi.

   zi has the layout scipy gives it:
     lfilter  the shape of x with the filtered axis replaced by the order;
     sosfilt  (n_sections, ...) with the filtered axis of x replaced by 2.
   Internally the states of slice k sit at state + k * per_slice, section s at
   + 2 * s, which is the order filter_slices reads them in.
   ------------------------------------------------------------------------- */

// the shape zi and zf must have for this source; false if it would exceed CSN_MAX_DIMS
static bool zi_shape_of(const CSN_ARRAY *source, uint32_t axis, bool is_sos, size_t order, size_t n_sections, uint32_t *ndim, uint32_t *shape) {
    memset(shape, 0, sizeof(uint32_t) * CSN_MAX_DIMS);
    if (!is_sos) {
        *ndim = source->ndim;
        memcpy(shape, source->shape, sizeof(uint32_t) * source->ndim);
        shape[axis] = (uint32_t) order;
        return true;
    }
    if (source->ndim + 1U > CSN_MAX_DIMS) return false;
    *ndim = source->ndim + 1U;
    shape[0] = (uint32_t) n_sections;
    for (uint32_t d = 0; d < source->ndim; d++) shape[d + 1] = source->shape[d];
    shape[axis + 1] = 2U;
    return true;
}

static bool same_shape(const CSN_ARRAY *arr, uint32_t ndim, const uint32_t *shape) {
    if (arr->ndim != ndim) return false;
    for (uint32_t d = 0; d < ndim; d++) if (arr->shape[d] != shape[d]) return false;
    return true;
}

// a shape as numpy prints it: (3,) or (2, 3)
static void shape_to_text(char *buf, size_t len, uint32_t ndim, const uint32_t *shape) {
    size_t at = (size_t) snprintf(buf, len, "(");
    for (uint32_t d = 0; d < ndim && at < len; d++) {
        const char *sep = d + 1 < ndim ? ", " : (ndim == 1 ? "," : "");
        at += (size_t) snprintf(buf + at, len - at, "%u%s", shape[d], sep);
    }
    if (at < len) snprintf(buf + at, len - at, ")");
}

/* Copies zi into the state buffer (to_state) or the state buffer into zf.
   zaxis is the axis of z holding the states of one slice: the filtered axis
   for lfilter, one further for sosfilt, whose leading axis is the section. */
static int32_t zi_transfer(CSOUND *csound, OPDS *perf_h, CSN_ARRAY *z, uint32_t zaxis, size_t n_slices, size_t per_slice, CSN_COMPLEXDAT *state, bool to_state) {
    if (z->size == 0 || n_slices == 0) return OK;
    CSN_AXIS_SLICE_ITER it = {0};
    if (AXIS_ITER_SLICE_INIT(&it, z, z, zaxis) != OK) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Internal error: incompatible filter state shapes/ranks");
    }
    size_t m = 0;
    while (AXIS_SLICE_ITER_NEXT(&it)) {
        // slice m of z is slice (m % n_slices) of the source, in group (m / n_slices): the section for sosfilt, 0 for lfilter
        size_t k = m % n_slices;
        size_t group = m / n_slices;
        CSN_COMPLEXDAT *zs = state + k * per_slice + group * it.axis_size;
        double *base = z->data + it.src_base * z->itype;
        for (size_t j = 0; j < it.axis_size; j++) {
            if (to_state) {
                zs[j] = slice_get(base, j, it.src_axis_stride, z->itype);
            } else {
                slice_put(base, j, it.src_axis_stride, z->itype, zs[j]);
            }
        }
        m++;
    }
    return OK;
}

// checks zi against the shape this source needs, and names that shape if it does not match
static int32_t zi_check(CSOUND *csound, OPDS *perf_h, const CSN_ARRAY *zi, const CSN_ARRAY *source, uint32_t axis, bool is_sos, size_t order, size_t n_sections, uint32_t *ndim, uint32_t *shape) {
    if (!zi_shape_of(source, axis, is_sos, order, n_sections, ndim, shape)) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] The sosfilt state of a %u-D source would exceed %d dimensions", source->ndim, CSN_MAX_DIMS);
    }
    if (!same_shape(zi, *ndim, shape)) {
        char want[128];
        shape_to_text(want, sizeof(want), *ndim, shape);
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] zi must have shape %s for this source and filter", want);
    }
    return OK;
}

/* ---- arrays, zi -> zf ------------------------------------------------------ */

int32_t csnsig_zlfilter_arr_deinit(CSOUND *csound, CSN_LFILTER_Z *p) {
    deinit_scratch(csound, &p->filter_state);
    FREE_CSNARRDATA(csound, &p->b_buffer);
    FREE_CSNARRDATA(csound, &p->a_buffer);
    int32_t res = csnarray_deinit_by_handle(csound, &p->handle_a->id, &p->array, &p->h);
    if (res != OK) return res;
    return csnarray_deinit_by_handle(csound, &p->handle_b->id, &p->array_state, &p->h);
}

int32_t csnsig_zsosfilter_arr_deinit(CSOUND *csound, CSN_SOSFILTER_Z *p) {
    deinit_scratch(csound, &p->filter_state);
    FREE_CSNARRDATA(csound, &p->sos_buffer);
    int32_t res = csnarray_deinit_by_handle(csound, &p->handle_a->id, &p->array, &p->h);
    if (res != OK) return res;
    return csnarray_deinit_by_handle(csound, &p->handle_b->id, &p->array_state, &p->h);
}

/* Shared init of the zi forms: coefficients, axis, zi checked and loaded into
   the state buffer, y and zf created. Nothing is filtered here. */
static int32_t zlfilter_arr_init(CSOUND *csound, CSN_REGISTRY *reg, CSN_LFILTER_Z *p, const MYFLT *axis_in, CSN_ARRAY **source_out, CSN_ARRAY **zi_out, CSN_ARRAY **b_out, CSN_ARRAY **a_out) {
    const char *err = NULL;
    uint32_t hb = p->b->id, ha = p->a->id, hx = p->source_handle_a->id, hz = p->source_handle_b->id;
    CSN_SLOT *slot_b = get_slot(reg, hb);
    CSN_SLOT *slot_a = get_slot(reg, ha);
    CSN_SLOT *slot = get_slot(reg, hx);
    CSN_SLOT *slot_zi = get_slot(reg, hz);
    if (slot_b == NULL || slot_a == NULL || slot == NULL || slot_zi == NULL) {
        uint32_t missing = slot_b == NULL ? hb : slot_a == NULL ? ha : slot == NULL ? hx : hz;
        return csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", missing);
    }
    CSN_ARRAY *source_arr = slot->array;
    CSN_ARRAY *zi_arr = slot_zi->array;

    int32_t res = filter_axis(csound, axis_in, source_arr->ndim, &p->axis_spec);
    if (res != OK) return res;

    FREE_CSNARRDATA(csound, &p->b_buffer);
    FREE_CSNARRDATA(csound, &p->a_buffer);
    res = load_tf_coeffs(csound, slot_b->array, slot_a->array, &p->b_buffer, &p->a_buffer, &p->order);
    if (res != OK) return res;
    p->coef_is_complex = coef_array_is_complex(&p->b_buffer) || coef_array_is_complex(&p->a_buffer);

    uint32_t z_ndim = 0;
    uint32_t z_shape[CSN_MAX_DIMS];
    res = zi_check(csound, NULL, zi_arr, source_arr, p->axis_spec.index, false, p->order, 0, &z_ndim, z_shape);
    if (res != OK) return res;

    size_t slices = slice_count_of(source_arr, p->axis_spec.index);
    size_t need = p->order * slices;
    res = csn_scratch_reserve(csound, NULL, false, &p->filter_state, need > 0 ? need : 1, sizeof(CSN_COMPLEXDAT));
    if (res != OK) return res;
    p->filter_state.current_size = slices;
    res = zi_transfer(csound, NULL, zi_arr, p->axis_spec.index, slices, p->order, (CSN_COMPLEXDAT *) p->filter_state.scratch, true);
    if (res != OK) return res;

    ITEM_TYPE out_itype = (source_arr->itype == CSN_COMPLEX || zi_arr->itype == CSN_COMPLEX || p->coef_is_complex) ? CSN_COMPLEX : CSN_REAL;
    uint32_t protect[4] = { hb, ha, hx, hz };
    if (create_csnarray_locked(csound, reg, &p->h, source_arr->ndim, source_arr->shape, &p->array, p->handle_a, protect, 4U, &err, out_itype) != OK) {
        return csound->InitError(csound, "[csnarray] %s", err);
    }
    if (create_csnarray_locked(csound, reg, &p->h, z_ndim, z_shape, &p->array_state, p->handle_b, protect, 4U, &err, out_itype) != OK) {
        return csound->InitError(csound, "[csnarray] %s", err);
    }

    *source_out = source_arr;
    *zi_out = zi_arr;
    *b_out = slot_b->array;
    *a_out = slot_a->array;
    return OK;
}

static int32_t zsosfilter_arr_init(CSOUND *csound, CSN_REGISTRY *reg, CSN_SOSFILTER_Z *p, const MYFLT *axis_in, CSN_ARRAY **source_out, CSN_ARRAY **zi_out, CSN_ARRAY **sos_out) {
    const char *err = NULL;
    uint32_t hs = p->sos->id, hx = p->source_handle_a->id, hz = p->source_handle_b->id;
    CSN_SLOT *slot_sos = get_slot(reg, hs);
    CSN_SLOT *slot = get_slot(reg, hx);
    CSN_SLOT *slot_zi = get_slot(reg, hz);
    if (slot_sos == NULL || slot == NULL || slot_zi == NULL) {
        uint32_t missing = slot_sos == NULL ? hs : slot == NULL ? hx : hz;
        return csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", missing);
    }
    CSN_ARRAY *source_arr = slot->array;
    CSN_ARRAY *zi_arr = slot_zi->array;

    int32_t res = filter_axis(csound, axis_in, source_arr->ndim, &p->axis_spec);
    if (res != OK) return res;

    FREE_CSNARRDATA(csound, &p->sos_buffer);
    res = load_sos(csound, slot_sos->array, &p->sos_buffer, &p->n_sections);
    if (res != OK) return res;
    p->coef_is_complex = coef_array_is_complex(&p->sos_buffer);

    uint32_t z_ndim = 0;
    uint32_t z_shape[CSN_MAX_DIMS];
    res = zi_check(csound, NULL, zi_arr, source_arr, p->axis_spec.index, true, 0, p->n_sections, &z_ndim, z_shape);
    if (res != OK) return res;

    size_t per_slice = 2 * p->n_sections;
    size_t slices = slice_count_of(source_arr, p->axis_spec.index);
    size_t need = per_slice * slices;
    res = csn_scratch_reserve(csound, NULL, false, &p->filter_state, need > 0 ? need : 1, sizeof(CSN_COMPLEXDAT));
    if (res != OK) return res;
    p->filter_state.current_size = slices;
    res = zi_transfer(csound, NULL, zi_arr, p->axis_spec.index + 1U, slices, per_slice, (CSN_COMPLEXDAT *) p->filter_state.scratch, true);
    if (res != OK) return res;

    ITEM_TYPE out_itype = (source_arr->itype == CSN_COMPLEX || zi_arr->itype == CSN_COMPLEX || p->coef_is_complex) ? CSN_COMPLEX : CSN_REAL;
    uint32_t protect[3] = { hs, hx, hz };
    if (create_csnarray_locked(csound, reg, &p->h, source_arr->ndim, source_arr->shape, &p->array, p->handle_a, protect, 3U, &err, out_itype) != OK) {
        return csound->InitError(csound, "[csnarray] %s", err);
    }
    if (create_csnarray_locked(csound, reg, &p->h, z_ndim, z_shape, &p->array_state, p->handle_b, protect, 3U, &err, out_itype) != OK) {
        return csound->InitError(csound, "[csnarray] %s", err);
    }

    *source_out = source_arr;
    *zi_out = zi_arr;
    *sos_out = slot_sos->array;
    return OK;
}

int32_t csnsig_zlfilter_arr(CSOUND *csound, CSN_LFILTER_Z *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    const MYFLT *axis_in = p->INOCOUNT == 5 ? p->opt_a : NULL;
    CSN_ARRAY *source_arr = NULL, *zi_arr = NULL, *b_arr = NULL, *a_arr = NULL;

    csound->LockMutex(reg->mutex);
    int32_t res = zlfilter_arr_init(csound, reg, p, axis_in, &source_arr, &zi_arr, &b_arr, &a_arr);
    if (res == OK) {
        CSN_COMPLEXDAT *state = (CSN_COMPLEXDAT *) p->filter_state.scratch;
        res = lfilter_slices(csound, NULL, source_arr, p->array, p->axis_spec.index, (const CSN_COMPLEXDAT *) p->b_buffer.data, (const CSN_COMPLEXDAT *) p->a_buffer.data, p->order, state, p->order);
    }
    if (res == OK) {
        res = zi_transfer(csound, NULL, p->array_state, p->axis_spec.index, p->filter_state.current_size, p->order, (CSN_COMPLEXDAT *) p->filter_state.scratch, false);
    }
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnsig_zsosfilter_arr(CSOUND *csound, CSN_SOSFILTER_Z *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    const MYFLT *axis_in = p->INOCOUNT == 4 ? p->opt_a : NULL;
    CSN_ARRAY *source_arr = NULL, *zi_arr = NULL, *sos_arr = NULL;

    csound->LockMutex(reg->mutex);
    int32_t res = zsosfilter_arr_init(csound, reg, p, axis_in, &source_arr, &zi_arr, &sos_arr);
    size_t per_slice = 2 * p->n_sections;
    if (res == OK) {
        res = sosfilter_slices(csound, NULL, source_arr, p->array, p->axis_spec.index, (const CSN_COMPLEXDAT *) p->sos_buffer.data, p->n_sections, (CSN_COMPLEXDAT *) p->filter_state.scratch, per_slice);
    }
    if (res == OK) {
        res = zi_transfer(csound, NULL, p->array_state, p->axis_spec.index + 1U, p->filter_state.current_size, per_slice, (CSN_COMPLEXDAT *) p->filter_state.scratch, false);
    }
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnsig_zlfilter_arr_k_init(CSOUND *csound, CSN_LFILTER_Z *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    const MYFLT *axis_in = p->INOCOUNT == 6 ? p->opt_b : NULL;
    CSN_ARRAY *source_arr = NULL, *zi_arr = NULL, *b_arr = NULL, *a_arr = NULL;

    csound->LockMutex(reg->mutex);
    int32_t res = zlfilter_arr_init(csound, reg, p, axis_in, &source_arr, &zi_arr, &b_arr, &a_arr);
    if (res == OK) {
        SET_FROM_KDATA_WITH_ID_BEGIN(p->k_data, reg, p->array->shape, p->array->ndim, p->array->itype, p->handle_a->id);
        SET_FROM_KDATA_WITH_ID_BEGIN(p->k_data_state, reg, p->array_state->shape, p->array_state->ndim, p->array_state->itype, p->handle_b->id);
        set_array_version(&p->b_version, &b_arr->version);
        set_array_version(&p->a_version, &a_arr->version);
        set_array_version(&p->k_data.prev_source_version, &source_arr->version);
        set_array_version(&p->k_data.prev_source_version_b, &zi_arr->version);
        set_array_version(&p->k_data.prev_output_version, &p->array->version);
        set_array_version(&p->k_data_state.prev_output_version, &p->array_state->version);
        p->is_published = false;
    }
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnsig_zsosfilter_arr_k_init(CSOUND *csound, CSN_SOSFILTER_Z *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    const MYFLT *axis_in = p->INOCOUNT == 5 ? p->opt_b : NULL;
    CSN_ARRAY *source_arr = NULL, *zi_arr = NULL, *sos_arr = NULL;

    csound->LockMutex(reg->mutex);
    int32_t res = zsosfilter_arr_init(csound, reg, p, axis_in, &source_arr, &zi_arr, &sos_arr);
    if (res == OK) {
        SET_FROM_KDATA_WITH_ID_BEGIN(p->k_data, reg, p->array->shape, p->array->ndim, p->array->itype, p->handle_a->id);
        SET_FROM_KDATA_WITH_ID_BEGIN(p->k_data_state, reg, p->array_state->shape, p->array_state->ndim, p->array_state->itype, p->handle_b->id);
        set_array_version(&p->sos_version, &sos_arr->version);
        set_array_version(&p->k_data.prev_source_version, &source_arr->version);
        set_array_version(&p->k_data.prev_source_version_b, &zi_arr->version);
        set_array_version(&p->k_data.prev_output_version, &p->array->version);
        set_array_version(&p->k_data_state.prev_output_version, &p->array_state->version);
        p->is_published = false;
    }
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t zfilter_arr_perf(CSOUND *csound, OPDS *h, CSN_REGISTRY *reg, const CSN_ZFILTER_PASS *zp, CSN_SLOT *slot, CSN_ARRAY *zi_arr) {
    CSN_ARRAY *source_arr = slot->array;

    if (*zp->is_published) {
        CSN_SLOT *slot_y = get_slot(reg, zp->k_y->owned_handle);
        CSN_SLOT *slot_z = get_slot(reg, zp->k_z->owned_handle);
        bool is_same_source = is_same_array_version(&zp->k_y->prev_source_version, &source_arr->version);
        bool is_same_source_b = is_same_array_version(&zp->k_y->prev_source_version_b, &zi_arr->version);
        bool is_same_y = slot_y != NULL && is_same_array_version(&zp->k_y->prev_output_version, &slot_y->array->version);
        bool is_same_z = slot_z != NULL && is_same_array_version(&zp->k_z->prev_output_version, &slot_z->array->version);
        bool unchanged = is_same_source && is_same_source_b && is_same_y && is_same_z;
        if (unchanged) {
            zp->handle_y->id = zp->k_y->owned_handle;
            zp->handle_z->id = zp->k_z->owned_handle;
            return OK;
        }
    }

    uint32_t axis = zp->axis_spec->index;
    if (axis >= source_arr->ndim) {
        return csn_locked_perf_error(csound, h, "[csnarray] The source is now %u-D, which has no axis %u", source_arr->ndim, axis);
    }

    uint32_t z_ndim = 0;
    uint32_t z_shape[CSN_MAX_DIMS];
    int32_t res = zi_check(csound, h, zi_arr, source_arr, axis, zp->is_sos, zp->order, zp->n_sections, &z_ndim, z_shape);
    if (res != OK) return res;

    size_t per_slice = zp->is_sos ? 2 * zp->n_sections : zp->order;
    size_t slices = slice_count_of(source_arr, axis);
    size_t need = per_slice * slices;
    res = csn_scratch_reserve(csound, h, slot->rt_locked, zp->state, need > 0 ? need : 1, sizeof(CSN_COMPLEXDAT));
    if (res != OK) return res;
    zp->state->current_size = slices;
    uint32_t zaxis = zp->is_sos ? axis + 1U : axis;
    CSN_COMPLEXDAT *state = (CSN_COMPLEXDAT *) zp->state->scratch;
    res = zi_transfer(csound, h, zi_arr, zaxis, slices, per_slice, state, true);
    if (res != OK) return res;
    ARRAY_VERSION zi_version;
    set_array_version(&zi_version, &zi_arr->version); // zi_arr may be the zf slot rewritten below

    const char *err = NULL;
    ITEM_TYPE out_itype = (source_arr->itype == CSN_COMPLEX || zi_arr->itype == CSN_COMPLEX || zp->coef_is_complex) ? CSN_COMPLEX : CSN_REAL;
    res = NEED_TO_UPDATE_SLOT(csound, h, zp->array_y, zp->k_y, NULL, source_arr->ndim, source_arr->shape, source_arr->size, out_itype, err);
    if (res != OK) return res;
    size_t z_size = 0;
    get_array_size_from_shape(&z_size, z_ndim, z_shape);
    res = NEED_TO_UPDATE_SLOT(csound, h, zp->array_z, zp->k_z, NULL, z_ndim, z_shape, z_size, out_itype, err);
    if (res != OK) return res;

    if (zp->is_sos) {
        res = sosfilter_slices(csound, h, source_arr, *zp->array_y, axis, zp->sos, zp->n_sections, state, per_slice);
    } else {
        res = lfilter_slices(csound, h, source_arr, *zp->array_y, axis, zp->b, zp->a, zp->order, state, per_slice);
    }
    if (res != OK) return res;
    res = zi_transfer(csound, h, *zp->array_z, zaxis, slices, per_slice, state, false);
    if (res != OK) return res;

    SET_FROM_KDATA_END_WITH_ID(*zp->k_y, zp->handle_y, source_arr->shape, source_arr->ndim, out_itype);
    SET_FROM_KDATA_END_WITH_ID(*zp->k_z, zp->handle_z, z_shape, z_ndim, out_itype);
    set_array_version(&zp->k_y->prev_output_version, &(*zp->array_y)->version);
    set_array_version(&zp->k_z->prev_output_version, &(*zp->array_z)->version);
    set_array_version(&zp->k_y->prev_source_version, &source_arr->version);
    // a zi that is our own zf has just moved on: remember the version we wrote, so feeding it back counts as new input
    if (zi_arr == *zp->array_z) {
        set_array_version(&zp->k_y->prev_source_version_b, &zi_version);
    } else {
        set_array_version(&zp->k_y->prev_source_version_b, &zi_arr->version);
    }
    *zp->is_published = true;
    return OK;
}

int32_t csnsig_zlfilter_arr_k(CSOUND *csound, CSN_LFILTER_Z *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REG_HANDLE(csound, &p->h, reg, p->k_data.owned_handle);

    uint32_t hx = p->source_handle_a->id, hz = p->source_handle_b->id;
    // y must not be x; zi may be this opcode's own zf, the usual feedback
    if (CHECK_SELF_ALIAS(csound, &p->h, &p->k_data, hx, 0) != OK) return NOTOK;
    CHECK_KTRIG(p->opt_a);

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot_b = get_slot(reg, p->b->id);
    CSN_SLOT *slot_a = get_slot(reg, p->a->id);
    CSN_SLOT *slot = get_slot(reg, hx);
    CSN_SLOT *slot_zi = get_slot(reg, hz);
    int32_t res = OK;
    if (slot_b == NULL || slot_a == NULL || slot == NULL || slot_zi == NULL) {
        uint32_t missing = slot_b == NULL ? p->b->id : slot_a == NULL ? p->a->id : slot == NULL ? hx : hz;
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", missing);
    } else if (!is_same_array_version(&p->b_version, &slot_b->array->version) || !is_same_array_version(&p->a_version, &slot_a->array->version)) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] b or a changed after the filter was initialised");
    } else {
        CSN_ZFILTER_PASS zp = {
            .k_y = &p->k_data,
            .k_z = &p->k_data_state,
            .handle_y = p->handle_a,
            .handle_z = p->handle_b,
            .array_y = &p->array,
            .array_z = &p->array_state,
            .state = &p->filter_state,
            .axis_spec = &p->axis_spec,
            .is_published = &p->is_published,
            .coef_is_complex = p->coef_is_complex,
            .is_sos = false,
            .b = (const CSN_COMPLEXDAT *) p->b_buffer.data,
            .a = (const CSN_COMPLEXDAT *) p->a_buffer.data,
            .order = p->order,
        };

        res = zfilter_arr_perf(csound, &p->h, reg, &zp, slot, slot_zi->array);
    }
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnsig_zsosfilter_arr_k(CSOUND *csound, CSN_SOSFILTER_Z *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REG_HANDLE(csound, &p->h, reg, p->k_data.owned_handle);

    uint32_t hx = p->source_handle_a->id, hz = p->source_handle_b->id;
    if (CHECK_SELF_ALIAS(csound, &p->h, &p->k_data, hx, 0) != OK) return NOTOK;
    CHECK_KTRIG(p->opt_a);

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot_sos = get_slot(reg, p->sos->id);
    CSN_SLOT *slot = get_slot(reg, hx);
    CSN_SLOT *slot_zi = get_slot(reg, hz);
    int32_t res = OK;
    if (slot_sos == NULL || slot == NULL || slot_zi == NULL) {
        uint32_t missing = slot_sos == NULL ? p->sos->id : slot == NULL ? hx : hz;
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", missing);
    } else if (!is_same_array_version(&p->sos_version, &slot_sos->array->version)) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] sos changed after the filter was initialised");
    } else {
        CSN_ZFILTER_PASS zp = {
            .k_y = &p->k_data,
            .k_z = &p->k_data_state,
            .handle_y = p->handle_a,
            .handle_z = p->handle_b,
            .array_y = &p->array,
            .array_z = &p->array_state,
            .state = &p->filter_state,
            .axis_spec = &p->axis_spec,
            .is_published = &p->is_published,
            .coef_is_complex = p->coef_is_complex,
            .is_sos = true,
            .sos = (const CSN_COMPLEXDAT *) p->sos_buffer.data,
            .n_sections = p->n_sections,
        };

        res = zfilter_arr_perf(csound, &p->h, reg, &zp, slot, slot_zi->array);
    }
    csound->UnlockMutex(reg->mutex);
    return res;
}

/* ---- audio, zi -> zf ------------------------------------------------------ */

int32_t csnsig_zlfilter_audio_deinit(CSOUND *csound, CSN_LFILTER_AUDIO_Z *p) {
    deinit_scratch(csound, &p->filter_state);
    FREE_CSNARRDATA(csound, &p->b_buffer);
    FREE_CSNARRDATA(csound, &p->a_buffer);
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array_state, &p->h);
}

int32_t csnsig_zsosfilter_audio_deinit(CSOUND *csound, CSN_SOSFILTER_AUDIO_Z *p) {
    deinit_scratch(csound, &p->filter_state);
    FREE_CSNARRDATA(csound, &p->sos_buffer);
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array_state, &p->h);
}

// zi of an audio filter: real, and exactly the shape given
static int32_t audio_zi_check(CSOUND *csound, OPDS *perf_h, const CSN_ARRAY *zi, uint32_t ndim, const uint32_t *shape) {
    if (zi->itype != CSN_REAL || !same_shape(zi, ndim, shape)) {
        char want[64];
        shape_to_text(want, sizeof(want), ndim, shape);
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] The zi of an audio filter must be a real array of shape %s", want);
    }
    return OK;
}

int32_t csnsig_zlfilter_audio_init(CSOUND *csound, CSN_LFILTER_AUDIO_Z *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t hb = p->b->id, ha = p->a->id, hz = p->source_handle->id;
    const char *err = NULL;
    int32_t res = OK;

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot_b = get_slot(reg, hb);
    CSN_SLOT *slot_a = get_slot(reg, ha);
    CSN_SLOT *slot_zi = get_slot(reg, hz);
    if (slot_b == NULL || slot_a == NULL || slot_zi == NULL) {
        uint32_t missing = slot_b == NULL ? hb : slot_a == NULL ? ha : hz;
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", missing);
        goto done;
    }
    if (slot_b->array->itype == CSN_COMPLEX || slot_a->array->itype == CSN_COMPLEX) {
        res = csound->InitError(csound, "[csnarray] The audio lfilter needs real b and a");
        goto done;
    }
    FREE_CSNARRDATA(csound, &p->b_buffer);
    FREE_CSNARRDATA(csound, &p->a_buffer);
    res = load_tf_coeffs(csound, slot_b->array, slot_a->array, &p->b_buffer, &p->a_buffer, &p->order);
    if (res != OK) goto done;

    uint32_t z_shape[CSN_MAX_DIMS] = {0};
    z_shape[0] = (uint32_t) p->order;
    res = audio_zi_check(csound, NULL, slot_zi->array, 1U, z_shape);
    if (res != OK) goto done;

    res = csn_scratch_reserve(csound, NULL, false, &p->filter_state, p->order > 0 ? p->order : 1, sizeof(double));
    if (res != OK) goto done;

    uint32_t protect[3] = { hb, ha, hz };
    if (create_csnarray_locked(csound, reg, &p->h, 1U, z_shape, &p->array_state, p->handle, protect, 3U, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }
    memcpy(p->array_state->data, slot_zi->array->data, sizeof(double) * p->order); // zf starts as zi
    p->state_handle = p->handle->id;
    set_array_version(&p->b_version, &slot_b->array->version);
    set_array_version(&p->a_version, &slot_a->array->version);
    p->registry = reg;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnsig_zsosfilter_audio_init(CSOUND *csound, CSN_SOSFILTER_AUDIO_Z *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t hs = p->sos->id, hz = p->source_handle->id;
    const char *err = NULL;
    int32_t res = OK;

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot_sos = get_slot(reg, hs);
    CSN_SLOT *slot_zi = get_slot(reg, hz);
    if (slot_sos == NULL || slot_zi == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", slot_sos == NULL ? hs : hz);
        goto done;
    }
    if (slot_sos->array->itype == CSN_COMPLEX) {
        res = csound->InitError(csound, "[csnarray] The audio sosfilter needs real sections");
        goto done;
    }
    FREE_CSNARRDATA(csound, &p->sos_buffer);
    res = load_sos(csound, slot_sos->array, &p->sos_buffer, &p->n_sections);
    if (res != OK) goto done;

    uint32_t z_shape[CSN_MAX_DIMS] = {0};
    z_shape[0] = (uint32_t) p->n_sections;
    z_shape[1] = 2U;
    res = audio_zi_check(csound, NULL, slot_zi->array, 2U, z_shape);
    if (res != OK) goto done;

    res = csn_scratch_reserve(csound, NULL, false, &p->filter_state, 2 * p->n_sections, sizeof(double));
    if (res != OK) goto done;

    uint32_t protect[2] = { hs, hz };
    if (create_csnarray_locked(csound, reg, &p->h, 2U, z_shape, &p->array_state, p->handle, protect, 2U, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }
    memcpy(p->array_state->data, slot_zi->array->data, sizeof(double) * 2 * p->n_sections); // zf starts as zi
    p->state_handle = p->handle->id;
    set_array_version(&p->sos_version, &slot_sos->array->version);
    p->registry = reg;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

/* Every control period: zi read under the lock, the block filtered without
   it, zf written back in place (no allocation, so a real-time path is fine). */
static int32_t audio_load_zi(CSOUND *csound, OPDS *h, CSN_REGISTRY *reg, uint32_t zi_handle, uint32_t ndim, const uint32_t *shape, double *z, size_t n) {
    CSN_SLOT *slot_zi = get_slot(reg, zi_handle);
    if (slot_zi == NULL) {
        return csn_locked_perf_error(csound, h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", zi_handle);
    }
    int32_t res = audio_zi_check(csound, h, slot_zi->array, ndim, shape);
    if (res != OK) return res;
    memcpy(z, slot_zi->array->data, sizeof(double) * n);
    return OK;
}

static int32_t audio_store_zf(CSOUND *csound, OPDS *h, CSN_REGISTRY *reg, uint32_t zf_handle, const double *z, size_t n) {
    CSN_SLOT *slot_zf = get_slot(reg, zf_handle);
    if (slot_zf == NULL) {
        return csn_locked_perf_error(csound, h, "[csnarray] The zf array %u of this filter is no longer registered", zf_handle);
    }
    memcpy(slot_zf->array->data, z, sizeof(double) * n);
    update_array_data_version(&slot_zf->array->version);
    return OK;
}

static uint32_t audio_block(OPDS *h, MYFLT *out, uint32_t *offset) {
    *offset = h->insdshead->ksmps_offset;
    uint32_t early = h->insdshead->ksmps_no_end;
    uint32_t nsmps = h->insdshead->ksmps;
    if (UNLIKELY(*offset)) memset(out, 0, *offset * sizeof(MYFLT));
    if (UNLIKELY(early)) {
        nsmps -= early;
        memset(&out[nsmps], 0, early * sizeof(MYFLT));
    }
    return nsmps;
}

int32_t csnsig_zlfilter_audio(CSOUND *csound, CSN_LFILTER_AUDIO_Z *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    size_t order = p->order;
    double *z = (double *) p->filter_state.scratch;
    uint32_t z_shape[CSN_MAX_DIMS] = {0};
    z_shape[0] = (uint32_t) order;

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot_b = get_slot(reg, p->b->id);
    CSN_SLOT *slot_a = get_slot(reg, p->a->id);
    int32_t res = OK;
    if (slot_b == NULL || slot_a == NULL) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", slot_b == NULL ? p->b->id : p->a->id);
    } else if (!is_same_array_version(&p->b_version, &slot_b->array->version) || !is_same_array_version(&p->a_version, &slot_a->array->version)) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] b or a changed after the filter was initialised");
    } else {
        res = audio_load_zi(csound, &p->h, reg, p->source_handle->id, 1U, z_shape, z, order);
    }
    csound->UnlockMutex(reg->mutex);
    if (res != OK) return res;

    uint32_t offset = 0;
    uint32_t nsmps = audio_block(&p->h, p->sig_out, &offset);
    const CSN_COMPLEXDAT *b = (const CSN_COMPLEXDAT *) p->b_buffer.data;
    const CSN_COMPLEXDAT *a = (const CSN_COMPLEXDAT *) p->a_buffer.data;
    for (uint32_t i = offset; i < nsmps; i++) {
        double x = (double) p->sig_in[i];
        double y = b[0].re * x + (order > 0 ? z[0] : 0.0);
        for (size_t j = 0; j < order; j++) {
            z[j] = b[j + 1].re * x - a[j + 1].re * y + (j + 1 < order ? z[j + 1] : 0.0);
        }
        p->sig_out[i] = (MYFLT) y;
    }

    csound->LockMutex(reg->mutex);
    res = audio_store_zf(csound, &p->h, reg, p->state_handle, z, order);
    p->handle->id = p->state_handle;
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnsig_zsosfilter_audio(CSOUND *csound, CSN_SOSFILTER_AUDIO_Z *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    size_t n_sections = p->n_sections;
    double *z = (double *) p->filter_state.scratch;
    uint32_t z_shape[CSN_MAX_DIMS] = {0};
    z_shape[0] = (uint32_t) n_sections;
    z_shape[1] = 2U;

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot_sos = get_slot(reg, p->sos->id);
    int32_t res = OK;
    if (slot_sos == NULL) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", p->sos->id);
    } else if (!is_same_array_version(&p->sos_version, &slot_sos->array->version)) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] sos changed after the filter was initialised");
    } else {
        res = audio_load_zi(csound, &p->h, reg, p->source_handle->id, 2U, z_shape, z, 2 * n_sections);
    }
    csound->UnlockMutex(reg->mutex);
    if (res != OK) return res;

    uint32_t offset = 0;
    uint32_t nsmps = audio_block(&p->h, p->sig_out, &offset);
    const CSN_COMPLEXDAT *sos = (const CSN_COMPLEXDAT *) p->sos_buffer.data;
    for (uint32_t i = offset; i < nsmps; i++) {
        double v = (double) p->sig_in[i];
        for (size_t s = 0; s < n_sections; s++) {
            const CSN_COMPLEXDAT *sec = sos + 6 * s;
            double *st = z + 2 * s;
            double y = sec[SOS_B0].re * v + st[0];
            st[0] = sec[SOS_B1].re * v - sec[SOS_A1].re * y + st[1];
            st[1] = sec[SOS_B2].re * v - sec[SOS_A2].re * y;
            v = y;
        }
        p->sig_out[i] = (MYFLT) v;
    }

    csound->LockMutex(reg->mutex);
    res = audio_store_zf(csound, &p->h, reg, p->state_handle, z, 2 * n_sections);
    p->handle->id = p->state_handle;
    csound->UnlockMutex(reg->mutex);
    return res;
}
