/* Opcode implementations for the stats family.
   The public opcode inventory remains centralized in csnum.c. */
#include "csnum_internal.h"
#include "csnregistry.h"
#include "csnset.h"
#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "arrays.h"

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

void complex_prod(CSN_COMPLEXDAT *out, CSN_COMPLEXDAT a, CSN_COMPLEXDAT b) {
    out->re =  a.re * b.re - a.im * b.im;
    out->im =  a.re * b.im + a.im * b.re;
}

void complex_scalar_prod(CSN_COMPLEXDAT *out, CSN_COMPLEXDAT a, double b) {
    out->re =  a.re * b;
    out->im =  a.im * b;
}

void complex_add(CSN_COMPLEXDAT *out, CSN_COMPLEXDAT a, CSN_COMPLEXDAT b) {
    out->re =  a.re + b.re;
    out->im =  a.im + b.im;
}

void complex_sub(CSN_COMPLEXDAT *out, CSN_COMPLEXDAT a, CSN_COMPLEXDAT b) {
    out->re =  a.re - b.re;
    out->im =  a.im - b.im;
}

int32_t complex_div(CSN_COMPLEXDAT *out, CSN_COMPLEXDAT a, CSN_COMPLEXDAT b) {
    double den = b.re * b.re + b.im * b.im;
    if (den == 0.0) return NOTOK;
    out->re = (a.re * b.re + a.im * b.im) / den;
    out->im = (a.im * b.re - a.re * b.im) / den;
    return OK;
}

void complex_log(CSN_COMPLEXDAT *out, CSN_COMPLEXDAT z) {
    double modulus = hypot(z.re, z.im);
    double phase = atan2(z.im, z.re);
    out->re = log(modulus);
    out->im = phase;
}

void complex_exp(CSN_COMPLEXDAT *out, CSN_COMPLEXDAT z) {
    double exp_re = exp(z.re);
    out->re = exp_re * cos(z.im);
    out->im = exp_re * sin(z.im);
}

int32_t complex_pow(CSN_COMPLEXDAT *out, CSN_COMPLEXDAT base, CSN_COMPLEXDAT exponent) {
    if (base.re == 0.0 && base.im == 0.0) return NOTOK;

    CSN_COMPLEXDAT l = {0};
    CSN_COMPLEXDAT t = {0};
    complex_log(&l, base);
    complex_prod(&t, exponent, l);
    complex_exp(out, t);
    return OK;
}

void complex_sqrt(CSN_COMPLEXDAT *out, CSN_COMPLEXDAT z) {
    double r = hypot(z.re, z.im);
    double real = sqrt((r + z.re) * 0.5);
    double imag = sqrt((r - z.re) * 0.5);

    if (z.im < 0.0) imag = -imag;

    out->re = real;
    out->im = imag;
}

void complex_sign(CSN_COMPLEXDAT *out, CSN_COMPLEXDAT z) {
    double r = hypot(z.re, z.im);

    if (r == 0.0) {
        out->re = 0.0;
        out->im = 0.0;
        return;
    }

    out->re = z.re / r;
    out->im = z.im / r;
}

void complex_sin(CSN_COMPLEXDAT *out, CSN_COMPLEXDAT z) {
    out->re = sin(z.re) * cosh(z.im);
    out->im = cos(z.re) * sinh(z.im);
}

void complex_cos(CSN_COMPLEXDAT *out, CSN_COMPLEXDAT z) {
    out->re = cos(z.re) * cosh(z.im);
    out->im = -sin(z.re) * sinh(z.im);
}

void complex_tan(CSN_COMPLEXDAT *out, CSN_COMPLEXDAT z) {
    CSN_COMPLEXDAT s = {0};
    CSN_COMPLEXDAT c = {0};
    complex_sin(&s, z);
    complex_cos(&c, z);
    complex_div(out, s, c);
}

void complex_sinh(CSN_COMPLEXDAT *out, CSN_COMPLEXDAT z) {
    out->re = sinh(z.re) * cos(z.im);
    out->im = cosh(z.re) * sin(z.im);
}

void complex_cosh(CSN_COMPLEXDAT *out, CSN_COMPLEXDAT z) {
    out->re = cosh(z.re) * cos(z.im);
    out->im = sinh(z.re) * sin(z.im);
}

void complex_tanh(CSN_COMPLEXDAT *out, CSN_COMPLEXDAT z) {
    CSN_COMPLEXDAT s = {0};
    CSN_COMPLEXDAT c = {0};
    complex_sinh(&s, z);
    complex_cosh(&c, z);
    complex_div(out, s, c);
}

void complex_asin(CSN_COMPLEXDAT *out, CSN_COMPLEXDAT z) {
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

void complex_acos(CSN_COMPLEXDAT *out, CSN_COMPLEXDAT z) {
    CSN_COMPLEXDAT a = {0};
    complex_asin(&a, z);

    out->re = M_PI_2 - a.re;
    out->im = -a.im;
}

void complex_atan(CSN_COMPLEXDAT *out, CSN_COMPLEXDAT z) {
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

void complex_asinh(CSN_COMPLEXDAT *out, CSN_COMPLEXDAT z) {
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

void complex_acosh(CSN_COMPLEXDAT *out, CSN_COMPLEXDAT z) {
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

void complex_atanh(CSN_COMPLEXDAT *out, CSN_COMPLEXDAT z) {
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

    if (out_handle != NULL) {
        CSN_AXIS_SPEC axis_spec = csn_normalize_axis_value(in_axis, source_ndim);
        if (axis_spec.kind != CSN_AXIS_INDEX) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: finite integers %d..%u)", in_axis, source_ndim, -(int32_t) source_ndim, source_ndim - 1);
        }
        *out_axis = (int32_t) axis_spec.index;
    } else {
        *out_axis = -1;
    }

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

int32_t csnarray_sum_k_init(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction_k_init_helper(csound, &p->h, p->source_handle, (double) *p->trig, p->handle, &p->array, NULL, NULL, RED_SUM, &p->k_data);
}

int32_t csnarray_sum_k(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction_k(csound, &p->h, p->source_handle, (double) *p->trig, p->handle, &p->array, NULL, NULL, RED_SUM, &p->k_data, p->axis);
}

int32_t csnarray_sum_all(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, NULL, RED_SUM);
}

int32_t csnarray_sum_all_k_init(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_accumulate_reduction_k_init_helper(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, NULL, RED_SUM, &p->k_data);
}

int32_t csnarray_sum_all_k(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_accumulate_reduction_k(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, NULL, RED_SUM, &p->k_data, p->trig);
}

int32_t csnarray_sumcomp_all(CSOUND *csound, CSN_REDUCTION_COMPLEX_S *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, -1, NULL, NULL, NULL, p->value, RED_SUM);
}

int32_t csnarray_sumcomp_all_k_init(CSOUND *csound, CSN_REDUCTION_COMPLEX_S *p) {
    return csnarray_accumulate_reduction_k_init_helper(csound, &p->h, p->source_handle, -1, NULL, NULL, NULL, p->value, RED_SUM, &p->k_data);
}

int32_t csnarray_sumcomp_all_k(CSOUND *csound, CSN_REDUCTION_COMPLEX_S *p) {
    CHECK_KTRIG(p->trig);
    return csnarray_accumulate_reduction_k(csound, &p->h, p->source_handle, -1, NULL, NULL, NULL, p->value, RED_SUM, &p->k_data, p->trig);
}

int32_t csnarray_prod(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, (double) *p->axis, p->handle, &p->array, NULL, NULL, RED_PROD);
}

int32_t csnarray_prod_k_init(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction_k_init_helper(csound, &p->h, p->source_handle, (double) *p->trig, p->handle, &p->array, NULL, NULL, RED_PROD, &p->k_data);
}

int32_t csnarray_prod_k(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction_k(csound, &p->h, p->source_handle, (double) *p->trig, p->handle, &p->array, NULL, NULL, RED_PROD, &p->k_data, p->axis);
}

int32_t csnarray_prod_all(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, NULL, RED_PROD);
}

int32_t csnarray_prod_all_k_init(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_accumulate_reduction_k_init_helper(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, NULL, RED_PROD, &p->k_data);
}

int32_t csnarray_prod_all_k(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_accumulate_reduction_k(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, NULL, RED_PROD, &p->k_data, p->trig);
}

int32_t csnarray_prodcomp_all(CSOUND *csound, CSN_REDUCTION_COMPLEX_S *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, -1, NULL, NULL, NULL, p->value, RED_PROD);
}

int32_t csnarray_prodcomp_all_k_init(CSOUND *csound, CSN_REDUCTION_COMPLEX_S *p) {
    return csnarray_accumulate_reduction_k_init_helper(csound, &p->h, p->source_handle, -1, NULL, NULL, NULL, p->value, RED_PROD, &p->k_data);
}

int32_t csnarray_prodcomp_all_k(CSOUND *csound, CSN_REDUCTION_COMPLEX_S *p) {
    CHECK_KTRIG(p->trig);
    return csnarray_accumulate_reduction_k(csound, &p->h, p->source_handle, -1, NULL, NULL, NULL, p->value, RED_PROD, &p->k_data, p->trig);
}

int32_t csnarray_sub(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, (double) *p->axis, p->handle, &p->array, NULL, NULL, RED_SUB);
}

int32_t csnarray_sub_k_init(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction_k_init_helper(csound, &p->h, p->source_handle, (double) *p->trig, p->handle, &p->array, NULL, NULL, RED_SUB, &p->k_data);
}

int32_t csnarray_sub_k(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction_k(csound, &p->h, p->source_handle, (double) *p->trig, p->handle, &p->array, NULL, NULL, RED_SUB, &p->k_data, p->axis);
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

int32_t csnarray_subcomp_all_k_init(CSOUND *csound, CSN_REDUCTION_COMPLEX_S *p) {
    return csnarray_accumulate_reduction_k_init_helper(csound, &p->h, p->source_handle, -1, NULL, NULL, NULL, p->value, RED_SUB, &p->k_data);
}

int32_t csnarray_mean(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, (double) *p->axis, p->handle, &p->array, NULL, NULL, RED_MEAN);
}

int32_t csnarray_mean_k(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction_k(csound, &p->h, p->source_handle, (double) *p->trig, p->handle, &p->array, NULL, NULL, RED_MEAN, &p->k_data, p->axis);
}

int32_t csnarray_mean_k_init(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction_k_init_helper(csound, &p->h, p->source_handle, (double) *p->trig, p->handle, &p->array, NULL, NULL, RED_MEAN, &p->k_data);
}

int32_t csnarray_mean_all(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, NULL, RED_MEAN);
}

int32_t csnarray_mean_all_k(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_accumulate_reduction_k(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, NULL, RED_MEAN, &p->k_data, p->trig);
}

int32_t csnarray_mean_all_k_init(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_accumulate_reduction_k_init_helper(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, NULL, RED_MEAN, &p->k_data);
}

int32_t csnarray_meancomp_all(CSOUND *csound, CSN_REDUCTION_COMPLEX_S *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, -1, NULL, NULL, NULL, p->value, RED_MEAN);
}

int32_t csnarray_meancomp_all_k_init(CSOUND *csound, CSN_REDUCTION_COMPLEX_S *p) {
    return csnarray_accumulate_reduction_k_init_helper(csound, &p->h, p->source_handle, -1, NULL, NULL, NULL, p->value, RED_MEAN, &p->k_data);
}

int32_t csnarray_meancomp_all_k(CSOUND *csound, CSN_REDUCTION_COMPLEX_S *p) {
    CHECK_KTRIG(p->trig);
    return csnarray_accumulate_reduction_k(csound, &p->h, p->source_handle, -1, NULL, NULL, NULL, p->value, RED_MEAN, &p->k_data, p->trig);
}

int32_t csnarray_min(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, (double) *p->axis, p->handle, &p->array, NULL, NULL, RED_MIN);
}

int32_t csnarray_min_k_init(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction_k_init_helper(csound, &p->h, p->source_handle, (double) *p->trig, p->handle, &p->array, NULL, NULL, RED_MIN, &p->k_data);
}

int32_t csnarray_min_k(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction_k(csound, &p->h, p->source_handle, (double) *p->trig, p->handle, &p->array, NULL, NULL, RED_MIN, &p->k_data, p->axis);
}

int32_t csnarray_min_all(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, NULL, RED_MIN);
}

int32_t csnarray_min_all_k_init(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_accumulate_reduction_k_init_helper(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, NULL, RED_MIN, &p->k_data);
}

int32_t csnarray_min_all_k(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_accumulate_reduction_k(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, NULL, RED_MIN, &p->k_data, p->trig);
}

int32_t csnarray_max(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, (double) *p->axis, p->handle, &p->array, NULL, NULL, RED_MAX);
}

int32_t csnarray_max_k_init(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction_k_init_helper(csound, &p->h, p->source_handle, (double) *p->trig, p->handle, &p->array, NULL, NULL, RED_MAX, &p->k_data);
}

int32_t csnarray_max_k(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction_k(csound, &p->h, p->source_handle, (double) *p->trig, p->handle, &p->array, NULL, NULL, RED_MAX, &p->k_data, p->axis);
}

int32_t csnarray_max_all(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, NULL, RED_MAX);
}

int32_t csnarray_max_all_k_init(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_accumulate_reduction_k_init_helper(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, NULL, RED_MAX, &p->k_data);
}

int32_t csnarray_max_all_k(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_accumulate_reduction_k(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, NULL, RED_MAX, &p->k_data, p->trig);
}

int32_t csnarray_all(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, (double) *p->axis, p->handle, &p->array, NULL, NULL, RED_ALL);
}

int32_t csnarray_all_k_init(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction_k_init_helper(csound, &p->h, p->source_handle, (double) *p->trig, p->handle, &p->array, NULL, NULL, RED_ALL, &p->k_data);
}

int32_t csnarray_all_k(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction_k(csound, &p->h, p->source_handle, (double) *p->trig, p->handle, &p->array, NULL, NULL, RED_ALL, &p->k_data, p->axis);
}

int32_t csnarray_all_all(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, NULL, RED_ALL);
}

int32_t csnarray_all_all_k_init(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_accumulate_reduction_k_init_helper(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, NULL, RED_ALL, &p->k_data);
}

int32_t csnarray_all_all_k(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_accumulate_reduction_k(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, NULL, RED_ALL, &p->k_data, p->trig);
}

int32_t csnarray_any(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, (double) *p->axis, p->handle, &p->array, NULL, NULL, RED_ANY);
}

int32_t csnarray_any_k(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction_k(csound, &p->h, p->source_handle, (double) *p->trig, p->handle, &p->array, NULL, NULL, RED_ANY, &p->k_data, p->axis);
}

int32_t csnarray_any_k_init(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction_k_init_helper(csound, &p->h, p->source_handle, (double) *p->trig, p->handle, &p->array, NULL, NULL, RED_ANY, &p->k_data);
}

int32_t csnarray_any_all(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, NULL, RED_ANY);
}

int32_t csnarray_any_all_k_init(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_accumulate_reduction_k_init_helper(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, NULL, RED_ANY, &p->k_data);
}

int32_t csnarray_any_all_k(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_accumulate_reduction_k(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, NULL, RED_ANY, &p->k_data, p->trig);
}

int32_t csnarray_rms(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, (double) *p->axis, p->handle, &p->array, NULL, NULL, RED_RMS);
}

int32_t csnarray_rms_k_init(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction_k_init_helper(csound, &p->h, p->source_handle, (double) *p->trig, p->handle, &p->array, NULL, NULL, RED_RMS, &p->k_data);
}

int32_t csnarray_rms_k(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_accumulate_reduction_k(csound, &p->h, p->source_handle, (double) *p->trig, p->handle, &p->array, NULL, NULL, RED_RMS, &p->k_data, p->axis);
}

int32_t csnarray_rms_all(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_accumulate_reduction(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, NULL, RED_RMS);
}

int32_t csnarray_rms_all_k_init(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_accumulate_reduction_k_init_helper(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, NULL, RED_RMS, &p->k_data);
}

int32_t csnarray_rms_all_k(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_accumulate_reduction_k(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, NULL, RED_RMS, &p->k_data, p->trig);
}

int compare_double(const void *a, const void *b) {
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
    if (out_handle != NULL) {
        CSN_AXIS_SPEC axis_spec = csn_normalize_axis_value(axis_value, source_ndim);
        if (axis_spec.kind != CSN_AXIS_INDEX) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: finite integers %d..%u)", axis_value, source_ndim, -(int32_t) source_ndim, source_ndim - 1);
        }
        *out_axis = (int32_t) axis_spec.index;
    } else {
        *out_axis = -1;
    }
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

int32_t csnarray_std_k_init(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_stdvar_k_init_helper(csound, &p->h, p->source_handle, (double) *p->trig, p->handle, &p->array, NULL, RED_STD, &p->k_data);
}

int32_t csnarray_std_k(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_stdvar_k_helper(csound, &p->h, p->source_handle, (double) *p->trig, p->handle, &p->array, NULL, RED_STD, &p->k_data, p->axis);
}

int32_t csnarray_std_all(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_stdvar_helper(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, RED_STD);
}

int32_t csnarray_std_all_k_init(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_stdvar_k_init_helper(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, RED_STD, &p->k_data);
}

int32_t csnarray_std_all_k(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_stdvar_k_helper(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, RED_STD, &p->k_data, p->trig);
}

int32_t csnarray_var(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_stdvar_helper(csound, &p->h, p->source_handle, (double) *p->axis, p->handle, &p->array, NULL, RED_VAR);
}

int32_t csnarray_var_k_init(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_stdvar_k_init_helper(csound, &p->h, p->source_handle, (double) *p->trig, p->handle, &p->array, NULL, RED_VAR, &p->k_data);
}

int32_t csnarray_var_k(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_stdvar_k_helper(csound, &p->h, p->source_handle, (double) *p->trig, p->handle, &p->array, NULL, RED_VAR, &p->k_data, p->axis);
}

int32_t csnarray_var_all(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_stdvar_helper(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, RED_VAR);
}

int32_t csnarray_var_all_k_init(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
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

static int32_t argminmax_body(CSOUND *csound, OPDS *perf_h, uint32_t source_handle, CSN_REGISTRY *reg, CSN_ARRAY **source_array, const MYFLT *axis_in, int32_t *out_axis) {
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

    CSN_AXIS_SPEC axis_spec = csn_normalize_axis(axis_in, source_ndim, CSN_AXIS_DEFAULT_FLATTEN);
    if (axis_spec.kind == CSN_AXIS_INVALID) {
        double value = axis_in == NULL ? 0.0 : (double) *axis_in;
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: finite integers %d..%u)", value, source_ndim, -(int32_t) source_ndim, source_ndim - 1);
    }
    *out_axis = axis_spec.kind == CSN_AXIS_FLATTEN ? -1 : (int32_t) axis_spec.index;
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
    const MYFLT *axis_in = p->INOCOUNT > 1 ? p->axis : NULL;
    res = argminmax_body(csound, NULL, source_handle, reg, &source_arr, axis_in, &axis);
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
    const MYFLT *axis_in = p->INOCOUNT > 2 ? p->trig : NULL;
    res = argminmax_body(csound, NULL, source_handle, reg, &source_arr, axis_in, &axis);
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

    CHECK_KTRIG(p->axis);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = CHECK_SELF_ALIAS(csound, &p->h, &p->k_data, source_handle, 0);
    if (res != OK) return res;

    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    int32_t axis = 0;
    const MYFLT *axis_in = p->INOCOUNT > 2 ? p->trig : NULL;
    res = argminmax_body(csound, &p->h, source_handle, reg, &source_arr, axis_in, &axis);
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

    if (out_handle != NULL) {
        CSN_AXIS_SPEC axis_spec = csn_normalize_axis_value(in_axis, source_ndim);
        if (axis_spec.kind != CSN_AXIS_INDEX) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: finite integers %d..%u)", in_axis, source_ndim, -(int32_t) source_ndim, source_ndim - 1);
        }
        *out_axis = (int32_t) axis_spec.index;
    } else {
        *out_axis = -1;
    }

    return OK;
}

/* Sorts scratch in place and returns the median. A NaN anywhere makes the
   result NaN, as in numpy, and the early return also spares compare_double
   from having to order NaNs. */
double median_of_scratch(double *scratch, size_t n) {
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

int32_t csnarray_median_scalar_k_deinit(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
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

    /* Median needs a sorted copy, so it cannot stream like the folds do. Every
       run the perf pass can meet, flat or along any axis, fits in the source's
       capacity, so a marked path never has to grow this later. */
    size_t run = (axis == -1) ? source_arr->size : source_shape[axis];
    size_t scratch_capacity_temp = source_arr->capacity > 0 ? source_arr->capacity : 1;
    double *scratch_temp = csound->Calloc(csound, sizeof(double) * scratch_capacity_temp);
    if (scratch_temp == NULL) {
        res = csound->InitError(csound, "[csnarray] Out of memory: allocation of %zu bytes failed", (size_t) (sizeof(double) * scratch_capacity_temp));
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
    /* The scratch lives in the caller's opcode struct. */
    void **scratch = &scratch_ref->scratch;
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
    if (axis == (int32_t) k_data->prev_axis_u
        && (!has_array_output || *out_array != NULL)
        && CAN_REUSE_LAST_RESULT(k_data, source_handle, source_arr, has_array_output ? *out_array : NULL)) {
        if (has_array_output) out_handle->id = k_data->owned_handle;
        goto done;
    }
    k_data->prev_axis_u = (uint32_t) axis;

    /* Median needs a sorted copy, so it cannot stream like the folds do. The
       copy serves the output slot, or the source when the result is a scalar;
       init reserved the source's capacity, which bounds every run. */
    size_t runs_size = (axis == -1) ? source_arr->size : source_shape[axis];
    bool rt_locked = csn_slot_rt_locked(reg, has_array_output ? k_data->owned_handle : source_handle);
    res = csn_scratch_reserve(csound, h, rt_locked, scratch_ref, runs_size, sizeof(double));
    if (res != OK) goto done;

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

int32_t csnarray_median_k_init(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_median_impl_k_init(csound, &p->h, p->source_handle, (double) *p->trig, p->handle, &p->array, NULL, &p->scratch, &p->k_data);
}

int32_t csnarray_median_k(CSOUND *csound, CSN_REDUCTION *p) {
    return csnarray_median_impl_k(csound, &p->h, p->source_handle, (double) *p->trig, p->handle, &p->array, NULL, &p->k_data, &p->scratch, p->axis);
}

int32_t csnarray_median_all(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_median_impl(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value);
}

int32_t csnarray_median_all_k_init(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_median_impl_k_init(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, &p->scratch, &p->k_data);
}

int32_t csnarray_median_all_k(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    return csnarray_median_impl_k(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, &p->k_data, &p->scratch, p->trig);
}

int32_t csnarray_argmin(CSOUND *csound, CSN_REDUCTION *p) {
    return argminmax_helper(csound, p, RED_ARGMIN);
}

int32_t csnarray_argmin_k_init(CSOUND *csound, CSN_REDUCTION *p) {
    return argminmax_k_init_helper(csound, p);
}

int32_t csnarray_argmin_k(CSOUND *csound, CSN_REDUCTION *p) {
    return argminmax_k_helper(csound, p, RED_ARGMIN);
}

int32_t csnarray_argmax(CSOUND *csound, CSN_REDUCTION *p) {
    return argminmax_helper(csound, p, RED_ARGMAX);
}

int32_t csnarray_argmax_k_init(CSOUND *csound, CSN_REDUCTION *p) {
    return argminmax_k_init_helper(csound, p);
}

int32_t csnarray_argmax_k(CSOUND *csound, CSN_REDUCTION *p) {
    return argminmax_k_helper(csound, p, RED_ARGMAX);
}

/* numpy's broadcasting rule: line the shapes up from the trailing axis, and
   accept a pair of extents when they match or when one of them is 1. Axes an
   operand does not have count as 1, so (2,3) pairs with (3,) but not with (2,).
   The result takes the larger extent on every axis. */
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

static void sw_push(CSN_SORTED_SLIDING_WINDOW *w, double x) {
    if (isnan(x)) {
        w->nan_count++;
        return;
    }
    size_t pos = 0;
    binary_search(&pos, NULL, 0, w->sorted, x, w->count);
    memmove(w->sorted + pos + 1, w->sorted + pos, sizeof(double) * (w->count - pos));
    w->sorted[pos] = x;
    w->count++;
}

static void sw_pop(CSN_SORTED_SLIDING_WINDOW *w, double x) {
    if (isnan(x)) {
        w->nan_count--;
        return;
    }
    size_t pos = 0;
    binary_search(&pos, NULL, 0, w->sorted, x, w->count);
    memmove(w->sorted + pos, w->sorted + pos + 1, sizeof(double) * (w->count - pos - 1));
    w->count--;
}

static void sw_replace(CSN_SORTED_SLIDING_WINDOW *w, double out, double in) {
    if (isnan(out) || isnan(in)) {
        sw_pop(w, out);
        sw_push(w, in);
        return;
    }

    double *s = w->sorted;
    size_t po;
    binary_search(&po, NULL, 0, s, out, w->count);
    size_t pi;
    if (in > out) {
        binary_search(&pi, NULL, po + 1, s, in, w->count);
        memmove(s + po, s + po + 1, (pi - po - 1) * sizeof(double));
        s[pi - 1] = in;
    } else if (in < out) {
        binary_search(&pi, NULL, 0, s, in, po);
        memmove(s + pi + 1, s + pi, (po - pi) * sizeof(double));
        s[pi] = in;
    }
}

static double sw_median(const CSN_SORTED_SLIDING_WINDOW *w) {
    if (w->nan_count > 0) return NAN;
    size_t c = w->count;
    return (c & 1) ? w->sorted[c / 2] : 0.5 * (w->sorted[c / 2 - 1] + w->sorted[c / 2]);
}

/* The window is held twice, sorted for the rank and in arrival order for what
   leaves next, so the scratch is two windows long. */
size_t sliding_median_scratch_size(size_t win_size) {
    return 2 * win_size;
}

/* scratch holds sliding_median_scratch_size(win_size) doubles. dst may alias
   src: the leaving value comes from the ring, and the arriving one lies ahead
   of every write. */
void sliding_median_slice(double *dst, const double *src, double *scratch, size_t n, size_t stride, size_t win_size, CSN_MEDIAN_EDGES edge) {
    CSN_SORTED_SLIDING_WINDOW w = { scratch, scratch + win_size, 0, 0 };
    ptrdiff_t left = (ptrdiff_t) (win_size / 2);
    ptrdiff_t right = (ptrdiff_t) win_size - left - 1;
    ptrdiff_t len = (ptrdiff_t) n;
    bool zero = (edge == CSN_MEDIAN_EDGE_ZERO);

    size_t head = 0;
    for (ptrdiff_t j = -left; j <= right; ++j, ++head) {
        if (j < len && j >= 0) {
            w.ring[head] = src[(size_t) j * stride];
            sw_push(&w, w.ring[head]);
        } else if (zero) {
            w.ring[head] = 0.0;
            sw_push(&w, 0.0);
        }
    }

    /* Index i - left leaves and i + right + 1 arrives: both map to the same
       ring slot, i % win_size. */
    head = 0;
    for (ptrdiff_t i = 0; i < len; ++i) {
        dst[(size_t) i * stride] = sw_median(&w);

        ptrdiff_t ji = i + right + 1;
        bool has_out = (i - left >= 0) || zero;
        bool has_in = (ji < len) || zero;
        double in = ji < len ? src[(size_t) ji * stride] : 0.0;

        if (has_out && has_in) sw_replace(&w, w.ring[head], in);
        else if (has_out) sw_pop(&w, w.ring[head]);
        else if (has_in) sw_push(&w, in);
        if (has_in) w.ring[head] = in;

        if (++head == win_size) head = 0;
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
    CSN_AXIS_SPEC axis_spec = csn_normalize_axis(in_axis, source_ndim, CSN_AXIS_DEFAULT_FLATTEN);
    if (axis_spec.kind == CSN_AXIS_INVALID) {
        double value = in_axis == NULL ? 0.0 : (double) *in_axis;
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: finite integers %d..%u)", value, source_ndim, -(int32_t) source_ndim, source_ndim - 1);
    }
    *out_axis = axis_spec.kind == CSN_AXIS_FLATTEN ? -1 : (int32_t) axis_spec.index;
    return OK;
}

static int32_t movstats_assign_value(CSOUND *csound, OPDS *perf_h, CSN_ARRAY *source_arr, CSN_ARRAY *arr, double *median_buffer, size_t winsize, int32_t axis, ITEM_TYPE itype, CSN_MOVSTATS_MODE mode) {
    if (axis == -1) {
        switch (mode) {
            case CSN_MOVMEAN:
                movmean_slice(arr->data, source_arr->data, source_arr->size, 1, winsize, itype);
                break;
            case CSN_MOVMEDIAN:
                sliding_median_slice(arr->data, source_arr->data, median_buffer, source_arr->size, 1, winsize, CSN_MEDIAN_EDGE_SHRINK);
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
                sliding_median_slice(arr->data + dst_base, source_arr->data + src_base, median_buffer, source_shape[axis], src_stride, winsize, CSN_MEDIAN_EDGE_SHRINK);
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
        median_buffer = csound->Calloc(csound, sizeof(double) * sliding_median_scratch_size(winsize));
        if (median_buffer == NULL) {
            return csound->InitError(csound, "[csnarray] Memory allocation failed");
        }
    }

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    int32_t axis = -1;
    const MYFLT *axis_in = p->INOCOUNT > 2 ? p->axis : NULL;
    res = movstats_body(csound, NULL, reg, &source_arr, source_handle, mode, axis_in, &axis);
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

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    int32_t axis = -1;
    const MYFLT *axis_in = p->INOCOUNT > 3 ? p->trig : NULL;
    res = movstats_body(csound, NULL, reg, &source_arr, source_handle, mode, axis_in, &axis);
    if (res != OK) goto done;

    const uint32_t protect[1] = { source_handle };
    ITEM_TYPE out_itype = (mode == CSN_MOVSTD || mode == CSN_MOVVAR) ? CSN_REAL : source_arr->itype;
    if (create_csnarray_locked(csound, reg, &p->h, source_arr->ndim, source_arr->shape, &p->array, p->handle, protect, 1U, &err, out_itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    if (mode == CSN_MOVMEDIAN) {
        /* Reserved for the widest window the perf pass can accept, whatever
           this one reads now: a window never exceeds the length it slides
           along, and that length cannot grow while the output keeps its shape.
           A new shape reaches NEED_TO_UPDATE_SLOT first, which refuses it on a
           marked output. So the buffer never grows mid-note on a real-time
           path, whether the mark came from the source, csnrtlockstart, rtlockall or
           a csnrtlock on the output after this init. */
        size_t bound = source_arr->size > winsize ? source_arr->size : winsize;
        size_t cap = bound > 0 ? sliding_median_scratch_size(bound) : 1;
        double *median_buffer = csound->Calloc(csound, sizeof(double) * cap);
        if (median_buffer == NULL) {
            res = csound->InitError(csound, "[csnarray] Memory allocation failed");
            goto done;
        }
        p->scratch.scratch = median_buffer;
        p->scratch.scratch_capacity = cap;
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

    CHECK_KTRIG(p->axis);
    size_t winsize = (size_t) *p->winsize;

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    int32_t axis = -1;
    const MYFLT *axis_in = p->INOCOUNT > 3 ? p->trig : NULL;
    res = movstats_body(csound, &p->h, reg, &source_arr, source_handle, mode, axis_in, &axis);
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
        && axis == (int32_t) p->k_data.prev_axis_u
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

    /* After the slot, so a shape the marked output cannot take is refused
       there before this buffer is consulted. */
    if (mode == CSN_MOVMEDIAN) {
        res = csn_scratch_reserve(csound, &p->h, csn_slot_rt_locked(reg, p->k_data.owned_handle), &p->scratch, sliding_median_scratch_size(winsize), sizeof(double));
        if (res != OK) goto done;
    }

    res = movstats_assign_value(csound, &p->h, source_arr, arr, p->scratch.scratch, winsize,  axis, itype, mode);
    if (res != OK) goto done;
    SET_KDATA_END(p, new_shape, new_dim, itype);

    p->array = arr;
    p->k_data.prev_axis_u = (uint32_t) axis;
    p->k_data.prev_scalar_param = (double) winsize;
    PUBLISH_DERIVED_RESULT(&p->k_data, source_handle, source_arr, arr);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_movmean(CSOUND *csound, CSN_MOVSTATS *p) {
    return csnarray_movstats_helper(csound, p, CSN_MOVMEAN);
}

int32_t csnarray_movmean_k_init(CSOUND *csound, CSN_MOVSTATS *p) {
    return csnarray_movstats_k_init_helper(csound, p, CSN_MOVMEAN);
}

int32_t csnarray_movmean_k(CSOUND *csound, CSN_MOVSTATS *p) {
    return csnarray_movstats_k_helper(csound, p, CSN_MOVMEAN);
}

int32_t csnarray_movmedian(CSOUND *csound, CSN_MOVSTATS *p) {
    return csnarray_movstats_helper(csound, p, CSN_MOVMEDIAN);
}

int32_t csnarray_movmedian_k_init(CSOUND *csound, CSN_MOVSTATS *p) {
    return csnarray_movstats_k_init_helper(csound, p, CSN_MOVMEDIAN);
}

int32_t csnarray_movmedian_k(CSOUND *csound, CSN_MOVSTATS *p) {
    return csnarray_movstats_k_helper(csound, p, CSN_MOVMEDIAN);
}

int32_t csnarray_movstd(CSOUND *csound, CSN_MOVSTATS *p) {
    return csnarray_movstats_helper(csound, p, CSN_MOVSTD);
}

int32_t csnarray_movstd_k_init(CSOUND *csound, CSN_MOVSTATS *p) {
    return csnarray_movstats_k_init_helper(csound, p, CSN_MOVSTD);
}

int32_t csnarray_movstd_k(CSOUND *csound, CSN_MOVSTATS *p) {
    return csnarray_movstats_k_helper(csound, p, CSN_MOVSTD);
}

int32_t csnarray_movvar(CSOUND *csound, CSN_MOVSTATS *p) {
    return csnarray_movstats_helper(csound, p, CSN_MOVVAR);
}

int32_t csnarray_movvar_k_init(CSOUND *csound, CSN_MOVSTATS *p) {
    return csnarray_movstats_k_init_helper(csound, p, CSN_MOVVAR);
}

int32_t csnarray_movvar_k(CSOUND *csound, CSN_MOVSTATS *p) {
    return csnarray_movstats_k_helper(csound, p, CSN_MOVVAR);
}

int32_t csnarray_movmin(CSOUND *csound, CSN_MOVSTATS *p) {
    return csnarray_movstats_helper(csound, p, CSN_MOVMIN);
}

int32_t csnarray_movmin_k_init(CSOUND *csound, CSN_MOVSTATS *p) {
    return csnarray_movstats_k_init_helper(csound, p, CSN_MOVMIN);
}

int32_t csnarray_movmin_k(CSOUND *csound, CSN_MOVSTATS *p) {
    return csnarray_movstats_k_helper(csound, p, CSN_MOVMIN);
}

int32_t csnarray_movmax(CSOUND *csound, CSN_MOVSTATS *p) {
    return csnarray_movstats_helper(csound, p, CSN_MOVMAX);
}

int32_t csnarray_movmax_k_init(CSOUND *csound, CSN_MOVSTATS *p) {
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
            sliding_median_slice(dst, src, median_buffer, size, stride, winsize, CSN_MEDIAN_EDGE_SHRINK);
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

static int32_t ensure_movstats_source_copy(CSOUND *csound, OPDS *perf_h, CSN_SCRATCH *scratch, const CSN_ARRAY *source_arr, bool rt_locked) {
    return csn_scratch_reserve(csound, perf_h, rt_locked, scratch, source_arr->size * (size_t) source_arr->itype, sizeof(double));
}

static int32_t movstats_in_body(CSOUND *csound, OPDS *perf_h, CSN_REGISTRY *reg, uint32_t source_handle, CSN_ARRAY **source_array, const MYFLT *in_axis, int32_t *out_axis, size_t winsize, CSN_MOVSTATS_MODE mode) {
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

    CSN_AXIS_SPEC axis_spec = csn_normalize_axis(in_axis, source_ndim, CSN_AXIS_DEFAULT_FLATTEN);
    if (axis_spec.kind == CSN_AXIS_INVALID) {
        double value = in_axis == NULL ? 0.0 : (double) *in_axis;
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: finite integers %d..%u)", value, source_ndim, -(int32_t) source_ndim, source_ndim - 1);
    }
    int32_t axis = axis_spec.kind == CSN_AXIS_FLATTEN ? -1 : (int32_t) axis_spec.index;

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
   elements the pass has already replaced. The median is the exception and
   passes NULL: its ring keeps every value until it leaves the window, so it
   reads the source it is rewriting. */
static int32_t movstats_in_assign_value(CSOUND *csound, OPDS *perf_h, CSN_ARRAY *source_arr, double *src_copy, double *median_buffer, int32_t axis, size_t winsize, CSN_MOVSTATS_MODE mode) {
    ITEM_TYPE itype = source_arr->itype;
    if (mode == CSN_MOVMEDIAN) {
        src_copy = source_arr->data;
    } else {
        memcpy(src_copy, source_arr->data, sizeof(double) * source_arr->size * (size_t) itype);
    }

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
        median_buffer = csound->Calloc(csound, sizeof(double) * sliding_median_scratch_size(winsize));
        if (median_buffer == NULL) {
            return csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
        }
    }

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    int32_t axis = -1;
    const MYFLT *axis_in = p->INOCOUNT > 2 ? p->axis : NULL;
    res = movstats_in_body(csound, NULL, reg, source_handle, &source_arr, axis_in, &axis, winsize, mode);
    if (res != OK) goto done;

    if (mode != CSN_MOVMEDIAN) {
        res = ensure_movstats_source_copy(csound, NULL, &src_scratch, source_arr, false);
        if (res != OK) goto done;
    }

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

int32_t csnarray_movstats_in_k_deinit(CSOUND *csound, CSN_MOVSTATS_IN *p) {
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

    csound->LockMutex(reg->mutex);

    /* The filter rewrites its own source, which can grow to its capacity
       without reallocating, and a window never exceeds the length it slides
       along. Reserving for that capacity now keeps the perf pass from
       allocating on a marked source, whenever and however the mark arrives.
       The window is a k-argument and usually still reads 0 here, so it only
       widens the reservation when it is already larger. An unknown handle
       keeps a one-item seed and is reported where the window is checked. */
    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    size_t capacity = source_slot != NULL ? source_slot->array->capacity : 1;
    size_t bound = capacity > winsize ? capacity : winsize;
    CSN_SCRATCH *reserved = &p->src_scratch;
    size_t items = capacity * (source_slot != NULL ? (size_t) source_slot->array->itype : 1U);
    if (mode == CSN_MOVMEDIAN) {
        /* The median's ring stands in for the copy of the source. */
        reserved = &p->scratch;
        items = sliding_median_scratch_size(bound);
    }
    items = items > 0 ? items : 1;
    reserved->scratch = csound->Calloc(csound, sizeof(double) * items);
    if (reserved->scratch == NULL) {
        res = csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
        goto done;
    }
    reserved->scratch_capacity = items;

    /* Same reason as the output form: with a k window this check can only run
       once the first performance pass knows the value. */
    if (winsize != 0) {
        CSN_ARRAY *source_arr = NULL;
        int32_t axis = -1;
        const MYFLT *axis_in = p->INOCOUNT > 3 ? p->trig : NULL;
        res = movstats_in_body(csound, NULL, reg, source_handle, &source_arr, axis_in, &axis, winsize, mode);
    }

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
    CHECK_KTRIG(p->axis);

    uint32_t source_handle = p->source_handle->id;
    size_t winsize = (size_t) *p->winsize;

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    int32_t axis = -1;
    const MYFLT *axis_in = p->INOCOUNT > 3 ? p->trig : NULL;
    res = movstats_in_body(csound, &p->h, reg, source_handle, &source_arr, axis_in, &axis, winsize, mode);
    if (res != OK) goto done;

    if (CAN_REUSE_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, NULL, (double) winsize, (double) axis)) {
        goto done;
    }

    /* Both buffers serve the source being rewritten, so its mark decides. */
    bool rt_locked = csn_slot_rt_locked(reg, source_handle);
    if (mode == CSN_MOVMEDIAN) {
        res = csn_scratch_reserve(csound, &p->h, rt_locked, &p->scratch, sliding_median_scratch_size(winsize), sizeof(double));
    } else {
        res = ensure_movstats_source_copy(csound, &p->h, &p->src_scratch, source_arr, rt_locked);
    }
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

int32_t csnarray_movmean_in_k_init(CSOUND *csound, CSN_MOVSTATS_IN *p) {
    return csnarray_movstats_in_k_init_helper(csound, p, CSN_MOVMEAN);
}

int32_t csnarray_movmean_in_k(CSOUND *csound, CSN_MOVSTATS_IN *p) {
    return csnarray_movstats_in_k_helper(csound, p, CSN_MOVMEAN);
}

int32_t csnarray_movmedian_in(CSOUND *csound, CSN_MOVSTATS_IN *p) {
    return csnarray_movstats_in_helper(csound, p, CSN_MOVMEDIAN);
}

int32_t csnarray_movmedian_in_k_init(CSOUND *csound, CSN_MOVSTATS_IN *p) {
    return csnarray_movstats_in_k_init_helper(csound, p, CSN_MOVMEDIAN);
}

int32_t csnarray_movmedian_in_k(CSOUND *csound, CSN_MOVSTATS_IN *p) {
    return csnarray_movstats_in_k_helper(csound, p, CSN_MOVMEDIAN);
}

int32_t csnarray_movstd_in(CSOUND *csound, CSN_MOVSTATS_IN *p) {
    return csnarray_movstats_in_helper(csound, p, CSN_MOVSTD);
}

int32_t csnarray_movstd_in_k_init(CSOUND *csound, CSN_MOVSTATS_IN *p) {
    return csnarray_movstats_in_k_init_helper(csound, p, CSN_MOVSTD);
}

int32_t csnarray_movstd_in_k(CSOUND *csound, CSN_MOVSTATS_IN *p) {
    return csnarray_movstats_in_k_helper(csound, p, CSN_MOVSTD);
}

int32_t csnarray_movvar_in(CSOUND *csound, CSN_MOVSTATS_IN *p) {
    return csnarray_movstats_in_helper(csound, p, CSN_MOVVAR);
}

int32_t csnarray_movvar_in_k_init(CSOUND *csound, CSN_MOVSTATS_IN *p) {
    return csnarray_movstats_in_k_init_helper(csound, p, CSN_MOVVAR);
}

int32_t csnarray_movvar_in_k(CSOUND *csound, CSN_MOVSTATS_IN *p) {
    return csnarray_movstats_in_k_helper(csound, p, CSN_MOVVAR);
}

int32_t csnarray_movmin_in(CSOUND *csound, CSN_MOVSTATS_IN *p) {
    return csnarray_movstats_in_helper(csound, p, CSN_MOVMIN);
}

int32_t csnarray_movmin_in_k_init(CSOUND *csound, CSN_MOVSTATS_IN *p) {
    return csnarray_movstats_in_k_init_helper(csound, p, CSN_MOVMIN);
}

int32_t csnarray_movmin_in_k(CSOUND *csound, CSN_MOVSTATS_IN *p) {
    return csnarray_movstats_in_k_helper(csound, p, CSN_MOVMIN);
}

int32_t csnarray_movmax_in(CSOUND *csound, CSN_MOVSTATS_IN *p) {
    return csnarray_movstats_in_helper(csound, p, CSN_MOVMAX);
}

int32_t csnarray_movmax_in_k_init(CSOUND *csound, CSN_MOVSTATS_IN *p) {
    return csnarray_movstats_in_k_init_helper(csound, p, CSN_MOVMAX);
}

int32_t csnarray_movmax_in_k(CSOUND *csound, CSN_MOVSTATS_IN *p) {
    return csnarray_movstats_in_k_helper(csound, p, CSN_MOVMAX);
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

    int32_t axis = -1;
    if (out_handle != NULL) {
        CSN_AXIS_SPEC axis_spec = csn_normalize_axis_value(axis_value, source_ndim);
        if (axis_spec.kind != CSN_AXIS_INDEX) {
            res = csound->InitError(csound, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: finite integers %d..%u)", axis_value, source_ndim, -(int32_t) source_ndim, source_ndim - 1);
            goto done;
        }
        axis = (int32_t) axis_spec.index;
    }

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

int32_t csnarray_perquant_s_k_deinit(CSOUND *csound, CSN_PERCQUANT *p) {
    if (p->scratch.scratch != NULL) {
        csound->Free(csound, p->scratch.scratch);
    }

    return OK;
}

int32_t csnarray_perquant_k_init(CSOUND *csound, CSN_PERCQUANT_AX *p) {
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
        memcpy(p->array->data, source_arr->data, sizeof(double) * source_size);
        p->array->size = source_size;
    }

    /* Any run along any axis fits in the source's capacity. */
    size_t cap = source_arr->capacity > 0 ? source_arr->capacity : 1;
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

int32_t csnarray_perquant_scalar_k_init(CSOUND *csound, CSN_PERCQUANT *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    p->registry = reg;

    /* Sized to what the source can hold without reallocating, so a marked
       source never makes the perf pass grow this. An unknown handle keeps the
       default and is reported by the perf pass. */
    size_t init_capacity = DEFAULT_TEMPORARY_BUFFER_SIZE;
    csound->LockMutex(reg->mutex);
    CSN_SLOT *source_slot = get_slot(reg, p->source_handle->id);
    if (source_slot != NULL && source_slot->array->capacity > init_capacity) {
        init_capacity = source_slot->array->capacity;
    }
    csound->UnlockMutex(reg->mutex);

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
    /* The scratch lives in the caller's opcode struct. */
    void **buffer = &buffer_ref->scratch;
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

    int32_t axis = -1;
    if (out_handle != NULL) {
        CSN_AXIS_SPEC axis_spec = csn_normalize_axis_value(axis_value, source_ndim);
        if (axis_spec.kind != CSN_AXIS_INDEX) {
            csound->UnlockMutex(reg->mutex);
            return csound->PerfError(csound, h, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: finite integers %d..%u)", axis_value, source_ndim, -(int32_t) source_ndim, source_ndim - 1);
        }
        axis = (int32_t) axis_spec.index;
    }

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
        && axis == (int32_t) k_data->prev_axis_u
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
        /* The sort buffer serves the output slot here and the source in the
           scalar form below; the k init reserved the source's capacity. */
        size_t r_size = (size_t) source_shape[axis];
        res = csn_scratch_reserve(csound, h, csn_slot_rt_locked(reg, k_data->owned_handle), buffer_ref, r_size, sizeof(double));
        if (res != OK) goto done;

        for (size_t linear = 0; linear < arr->size; ++linear) {
            uint32_t dst_coords[CSN_MAX_DIMS] = {0};
            uint32_t src_coords[CSN_MAX_DIMS] = {0};

            from_linear_to_coords(dst_coords, arr->shape, linear, arr->ndim);
            double value = 0.0;
            accumulate_perquant_reduction_axis_helper(&value, q, *buffer, source_arr, src_coords, dst_coords, is_percentile, (uint32_t) axis);
            arr->data[linear] = value;
        }

        if (k_data != NULL) {
            k_data->prev_axis_u = (uint32_t) axis;
            k_data->prev_scalar_param = q;
            PUBLISH_DERIVED_RESULT(k_data, source_handle, source_arr, arr);
        }
    } else {
        size_t r_size = (size_t) source_arr->size;
        res = csn_scratch_reserve(csound, h, source_slot->rt_locked, buffer_ref, r_size, sizeof(double));
        if (res != OK) goto done;

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
    return csnarray_perquant_k_reduction(csound, &p->h, p->source_handle, (double) *p->trig, p->handle, &p->array, NULL, true, (double) *p->quantity, &p->k_data, NULL, p->axis, &p->scratch);
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
    return csnarray_perquant_k_reduction(csound, &p->h, p->source_handle, (double) *p->trig, p->handle, &p->array, NULL, false, (double) *p->quantity, &p->k_data, NULL, p->axis, &p->scratch);
}

int32_t csnarray_quantile_scalar(CSOUND *csound, CSN_PERCQUANT *p) {
    return csnarray_perquant_reduction(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, false, (double) *p->quantity);
}

int32_t csnarray_quantile_scalar_k(CSOUND *csound, CSN_PERCQUANT *p) {
    return csnarray_perquant_k_reduction(csound, &p->h, p->source_handle, -1, NULL, NULL, p->value, false, (double) *p->quantity, NULL, p->registry, p->trig, &p->scratch);
}

