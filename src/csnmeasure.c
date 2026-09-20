#include "csnmeasure.h"
#include "csnregistry.h"
#include "csnum.h"
#include "csnum_internal.h"
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>


/* Every formula below divides by a quantity the caller supplies, so each one
   is wrapped: an unusable operand yields 0 instead of the inf or NaN a score
   has no way to notice. */
static double sabine_t60(double v, double a) {
    if (!(a > 0.0)) return 0.0;
    return T60_SABINE(v, a);
}

static double eyring_t60(double v, double s, double a) {
    /* a >= s means the surfaces absorb everything: no reverberant field. */
    if (!(s > 0.0) || !(a > 0.0) || a >= s) return 0.0;
    return T60_EYRING(v, s, a);
}

/* The two operands carry a different meaning in each mode, and an error has to
   name what the caller actually passed. */
static void acoustics_operand_names(CSN_ACOUSTICS_MODE mode, const char **name_a, const char **name_b) {
    switch (mode) {
        case CSN_T602FBG:
            *name_a = "delay times"; *name_b = "T60 targets"; break;
        case CSN_FBG2T60:
            *name_a = "delay times"; *name_b = "feedback gains"; break;
        case CSN_RT60ABS:
        case CSN_FSCHROEDER:
        default:
            *name_a = "volumes"; *name_b = "T60 targets"; break;
    }
}

static double acoustics_value(double v, double t, CSN_ACOUSTICS_MODE mode) {
    if (!(t > 0.0)) return 0.0;
    switch (mode) {
        case CSN_RT60ABS:
            return CSN_ABCOEFF * v / t;
        case CSN_FSCHROEDER:
            if (!(v > 0.0)) return 0.0;
            return F_SCHROEDER(t, v);
        case CSN_T602FBG: // v = delay_t, t = t60
            if (!(v > 0.0)) return 0.0;
            return G_FROM_T60(v, t);
        case CSN_FBG2T60: // t = gain
            /* Only a gain strictly inside (0, 1) decays: at 1 the logarithm is
               zero and above it the comb grows, so neither has a T60. */
            if (!(v > 0.0) || t >= 1.0) return 0.0;
            return T60_FROM_G(v, t);
    }
    return 0.0;
}

/* p, q and r index a modal series, so any non-negative integer is legal. */
static bool is_valid_mode_index(double value) {
    return isfinite(value) && trunc(value) == value && value >= 0.0;
}

static int32_t sabery_arr_body(CSOUND *csound, OPDS *perf_h, CSN_REGISTRY *reg, uint32_t source_handle_a, uint32_t source_handle_b, uint32_t source_handle_c, CSN_ARRAY **source_array_a, CSN_ARRAY **source_array_b, CSN_ARRAY **source_array_c) {
    CSN_SLOT *slot_a = get_slot(reg, source_handle_a);
    if (slot_a == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle_a);
    }
    CSN_SLOT *slot_b = get_slot(reg, source_handle_b);
    if (slot_b == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle_b);
    }
    CSN_SLOT *slot_c = get_slot(reg, source_handle_c);
    if (slot_c == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle_c);
    }

    CSN_ARRAY *source_arr_a = slot_a->array;
    CSN_ARRAY *source_arr_b = slot_b->array;
    CSN_ARRAY *source_arr_c = slot_c->array;

    if (source_arr_a->ndim != 1U) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Array of volumes must be 1D");
    }
    if (source_arr_b->ndim != 2U || source_arr_c->ndim != 2U) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Arrays of surfaces and alpha coeffs must be 2D");
    }
    if (source_arr_b->shape[0] != source_arr_a->shape[0] || source_arr_c->shape[0] != source_arr_a->shape[0]) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] The number of rows for the surfaces and alpha coeffs must equal to array of volumes length");
    }
    if (source_arr_b->shape[1] != source_arr_c->shape[1]) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Surfaces and alpha coeffs must have the same number of columns");
    }
    if (source_arr_a->itype == CSN_COMPLEX || source_arr_b->itype == CSN_COMPLEX || source_arr_c->itype == CSN_COMPLEX) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Volumes, surfaces and alpha coeffs must be real arrays");
    }

    *source_array_a = source_arr_a;
    *source_array_b = source_arr_b;
    *source_array_c = source_arr_c;

    return OK;
}

static int32_t sabery_scalar_body(CSOUND *csound, OPDS *perf_h, CSN_REGISTRY *reg, uint32_t source_handle_a, uint32_t source_handle_b, CSN_ARRAY **source_array_a, CSN_ARRAY **source_array_b) {
    CSN_SLOT *slot_a = get_slot(reg, source_handle_a);
    if (slot_a == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle_a);
    }
    CSN_SLOT *slot_b = get_slot(reg, source_handle_b);
    if (slot_b == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle_b);
    }

    CSN_ARRAY *source_arr_a = slot_a->array;
    CSN_ARRAY *source_arr_b = slot_b->array;

    if (source_arr_a->ndim != 1U || source_arr_b->ndim != 1U) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Arrays of surfaces and alpha coeffs must be 1D");
    }
    if (source_arr_a->shape[0] != source_arr_b->shape[0]) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Surfaces and alpha coeffs arrays must have same length");
    }
    if (source_arr_a->itype == CSN_COMPLEX || source_arr_b->itype == CSN_COMPLEX) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Surfaces and alpha coeffs must be real arrays");
    }

    *source_array_a = source_arr_a;
    *source_array_b = source_arr_b;

    return OK;
}

static void sabery_arr_assign_value(CSN_ARRAY *arr, CSN_ARRAY *source_arr_a, CSN_ARRAY *source_arr_b, CSN_ARRAY *source_arr_c, bool is_sabine) {
    uint32_t nrows = source_arr_a->shape[0];
    uint32_t ncols = source_arr_b->shape[1];
    for (uint32_t i = 0; i < nrows; i++) {
        double v = source_arr_a->data[i];
        double a = 0.0;
        double s = 0.0;
        for (uint32_t j = 0; j < ncols; j++) {
            size_t indx = (size_t) i * ncols + j;
            /* Absorption is the surface-weighted sum of the coefficients. */
            a += source_arr_b->data[indx] * source_arr_c->data[indx];
            s += source_arr_b->data[indx];
        }
        arr->data[i] = is_sabine ? sabine_t60(v, a) : eyring_t60(v, s, a);
    }
}

static void sabery_scalar_assign_value(double *y, const double *volume, CSN_ARRAY *source_arr_a, CSN_ARRAY *source_arr_b, bool is_sabine) {
    size_t size = source_arr_a->size;
    double a = 0.0;
    double s = 0.0;
    for (size_t i = 0; i < size; i++) {
        /* a holds the surfaces, b the absorption coefficients. */
        a += source_arr_a->data[i] * source_arr_b->data[i];
        s += source_arr_a->data[i];
    }
    *y = is_sabine ? sabine_t60(*volume, a) : eyring_t60(*volume, s, a);
}


static int32_t csnarray_sabeyr_arr_helper(CSOUND *csound, CSN_SABEYR_ARR *p, bool is_sabine) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle_a = p->source_handle_a->id;
    uint32_t source_handle_b = p->source_handle_b->id;
    uint32_t source_handle_c = p->source_handle_c->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);
    CSN_ARRAY *source_arr_a = NULL;
    CSN_ARRAY *source_arr_b = NULL;
    CSN_ARRAY *source_arr_c = NULL;
    res = sabery_arr_body(csound, NULL, reg, source_handle_a, source_handle_b, source_handle_c, &source_arr_a, &source_arr_b, &source_arr_c);
    if (res != OK) goto done;

    uint32_t new_dim = 1U;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    new_shape[0] = source_arr_a->shape[0];

    uint32_t protect[3] = { source_handle_a, source_handle_b, source_handle_c };
    if (create_csnarray_locked(csound, reg, &p->h, new_dim, new_shape, &p->array, p->handle, protect, 3U, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    sabery_arr_assign_value(p->array, source_arr_a, source_arr_b, source_arr_c, is_sabine);

    SET_KDATA_BEGIN(p, reg);
    set_array_version(&p->k_data.prev_output_version, &p->array->version);
    set_array_version(&p->versions.prev_a_version, &source_arr_a->version);
    set_array_version(&p->versions.prev_b_version, &source_arr_b->version);
    set_array_version(&p->versions.prev_c_version, &source_arr_c->version);
    p->is_published = false;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_sabeyr_arr_k_helper(CSOUND *csound, CSN_SABEYR_ARR *p, bool is_sabine) {
    CSN_REGISTRY *reg = get_registry(csound);
    uint32_t owned_handle = p->k_data.owned_handle;
    CHECK_REG_HANDLE(csound, &p->h, reg, owned_handle);

    uint32_t source_handle_a = p->source_handle_a->id;
    uint32_t source_handle_b = p->source_handle_b->id;
    uint32_t source_handle_c = p->source_handle_c->id;

    int32_t res = OK;
    const char *err = NULL;
    res = CHECK_SELF_ALIAS(csound, &p->h, &p->k_data, source_handle_a, source_handle_b);
    if (res != OK) return res;
    res = CHECK_SELF_ALIAS(csound, &p->h, &p->k_data, source_handle_c, 0);
    if (res != OK) return res;

    CHECK_KTRIG(p->trig);

    csound->LockMutex(reg->mutex);
    CSN_ARRAY *source_arr_a = NULL;
    CSN_ARRAY *source_arr_b = NULL;
    CSN_ARRAY *source_arr_c = NULL;
    res = sabery_arr_body(csound, &p->h, reg, source_handle_a, source_handle_b, source_handle_c, &source_arr_a, &source_arr_b, &source_arr_c);
    if (res != OK) goto done;

    if (p->is_published) {
        bool is_same_vol = is_same_array_version(&p->versions.prev_a_version, &source_arr_a->version);
        bool is_same_sur = is_same_array_version(&p->versions.prev_b_version, &source_arr_b->version);
        bool is_same_alp = is_same_array_version(&p->versions.prev_c_version, &source_arr_c->version);
        bool is_same_result = false;
        CSN_SLOT *slot_res = get_slot(reg, owned_handle);
        if (slot_res != NULL) {
            is_same_result = is_same_array_version(&p->k_data.prev_output_version, &slot_res->array->version);
        }

        if (is_same_vol && is_same_sur && is_same_alp && is_same_result) {
            p->handle->id = owned_handle;
            goto done;
        }
    }

    uint32_t new_dim = 1U;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    new_shape[0] = source_arr_a->shape[0];

    size_t req_size = 0;
    if (get_array_size_from_shape(&req_size, new_dim, new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSN_ARRAY *arr = NULL;
    size_t logical_size = source_arr_a->size == 0 ? 0 : req_size;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, new_dim, new_shape, logical_size, CSN_REAL, err);
    if (res != OK) goto done;
    p->array = arr;

    sabery_arr_assign_value(p->array, source_arr_a, source_arr_b, source_arr_c, is_sabine);

    SET_KDATA_BEGIN(p, reg);
    set_array_version(&p->k_data.prev_output_version, &p->array->version);
    set_array_version(&p->versions.prev_a_version, &source_arr_a->version);
    set_array_version(&p->versions.prev_b_version, &source_arr_b->version);
    set_array_version(&p->versions.prev_c_version, &source_arr_c->version);
    p->is_published = true;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_sabeyr_scalar_helper(CSOUND *csound, CSN_SABEYR_SCALAR *p, bool is_sabine) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle_a = p->source_handle_a->id;
    uint32_t source_handle_b = p->source_handle_b->id;

    double volume = (double) *p->volume;
    if (!IS_VALID_VALUE(volume)) {
        return csound->InitError(csound, "[csnarray] Volume must be a valid float value");
    }

    int32_t res = OK;

    csound->LockMutex(reg->mutex);
    CSN_ARRAY *source_arr_a = NULL;
    CSN_ARRAY *source_arr_b = NULL;
    res = sabery_scalar_body(csound, NULL, reg, source_handle_a, source_handle_b, &source_arr_a, &source_arr_b);
    if (res != OK) goto done;

    double result = 0.0;
    sabery_scalar_assign_value(&result, &volume, source_arr_a, source_arr_b, is_sabine);
    *p->value = (MYFLT) result;

    p->registry = reg;
    set_array_version(&p->versions.prev_a_version, &source_arr_a->version);
    set_array_version(&p->versions.prev_b_version, &source_arr_b->version);
    p->is_published = false;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_sabeyr_scalar_k_init(CSOUND *csound, CSN_SABEYR_SCALAR *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle_a = p->source_handle_a->id;
    uint32_t source_handle_b = p->source_handle_b->id;

    int32_t res = OK;

    csound->LockMutex(reg->mutex);
    CSN_ARRAY *source_arr_a = NULL;
    CSN_ARRAY *source_arr_b = NULL;
    res = sabery_scalar_body(csound, NULL, reg, source_handle_a, source_handle_b, &source_arr_a, &source_arr_b);
    if (res != OK) goto done;

    p->registry = reg;
    set_array_version(&p->versions.prev_a_version, &source_arr_a->version);
    set_array_version(&p->versions.prev_b_version, &source_arr_b->version);
    p->is_published = false;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_sabeyr_scalar_k_helper(CSOUND *csound, CSN_SABEYR_SCALAR *p, bool is_sabine) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    uint32_t source_handle_a = p->source_handle_a->id;
    uint32_t source_handle_b = p->source_handle_b->id;

    double volume = (double) *p->volume;
    if (!IS_VALID_VALUE(volume)) {
        return csound->InitError(csound, "[csnarray] Volume must be a valid float value");
    }

    int32_t res = OK;

    CHECK_KTRIG(p->trig);

    csound->LockMutex(reg->mutex);
    CSN_ARRAY *source_arr_a = NULL;
    CSN_ARRAY *source_arr_b = NULL;
    res = sabery_scalar_body(csound, &p->h, reg, source_handle_a, source_handle_b, &source_arr_a, &source_arr_b);
    if (res != OK) goto done;

    if (p->is_published) {
        bool is_same_sur = is_same_array_version(&p->versions.prev_a_version, &source_arr_a->version);
        bool is_same_alp = is_same_array_version(&p->versions.prev_b_version, &source_arr_b->version);
        bool is_same_vol = volume == p->prev_volume;
        if (is_same_sur && is_same_alp && is_same_vol) {
            *p->value = (MYFLT) p->prev_result;
            goto done;
        }
    }

    double result = 0.0;
    sabery_scalar_assign_value(&result, &volume, source_arr_a, source_arr_b, is_sabine);
    *p->value = (MYFLT) result;

    set_array_version(&p->versions.prev_a_version, &source_arr_a->version);
    set_array_version(&p->versions.prev_b_version, &source_arr_b->version);
    p->prev_volume = volume;
    p->prev_result = result;
    p->is_published = true;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}


int32_t csnarray_sabeyr_arr_deinit(CSOUND *csound, CSN_SABEYR_ARR *p) {
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

int32_t csnarray_sabine_arr(CSOUND *csound, CSN_SABEYR_ARR *p) {
    return csnarray_sabeyr_arr_helper(csound, p, true);
}

int32_t csnarray_sabine_arr_k(CSOUND *csound, CSN_SABEYR_ARR *p) {
    return csnarray_sabeyr_arr_k_helper(csound, p, true);
}

int32_t csnarray_sabine_scalar(CSOUND *csound, CSN_SABEYR_SCALAR *p) {
    return csnarray_sabeyr_scalar_helper(csound, p, true);
}

int32_t csnarray_sabine_scalar_k(CSOUND *csound, CSN_SABEYR_SCALAR *p) {
    return csnarray_sabeyr_scalar_k_helper(csound, p, true);
}

int32_t csnarray_eyring_arr(CSOUND *csound, CSN_SABEYR_ARR *p) {
    return csnarray_sabeyr_arr_helper(csound, p, false);
}

int32_t csnarray_eyring_arr_k(CSOUND *csound, CSN_SABEYR_ARR *p) {
    return csnarray_sabeyr_arr_k_helper(csound, p, false);
}

int32_t csnarray_eyring_scalar(CSOUND *csound, CSN_SABEYR_SCALAR *p) {
    return csnarray_sabeyr_scalar_helper(csound, p, false);
}

int32_t csnarray_eyring_scalar_k(CSOUND *csound, CSN_SABEYR_SCALAR *p) {
    return csnarray_sabeyr_scalar_k_helper(csound, p, false);
}

/* volume and t60target are NULL whenever that operand comes from an array
   instead of a scalar argument; the 2D form pairs every volume with every
   target, so the result is laid out row-major as volumes x targets. */
static void reqabs_assign_value(uint32_t ndim, CSN_ARRAY *source_arr_a, CSN_ARRAY *source_arr_b, CSN_ARRAY *arr, const double *volume, const double *t60target, CSN_ACOUSTICS_MODE mode) {
    if (ndim == 2U) {
        uint32_t nrows = source_arr_a->shape[0];
        uint32_t ncols = source_arr_b->shape[0];
        for (uint32_t i = 0; i < nrows; i++) {
            double vol = source_arr_a->data[i];
            for (uint32_t j = 0; j < ncols; j++) {
                arr->data[(size_t) i * ncols + j] = acoustics_value(vol, source_arr_b->data[j], mode);
            }
        }
    } else {
        for (size_t i = 0; i < arr->size; i++) {
            double num = volume == NULL ? source_arr_a->data[i] : *volume;
            double den = t60target == NULL ? source_arr_b->data[i] : *t60target;
            arr->data[i] = acoustics_value(num, den, mode);
        }
    }
}

static int32_t csnarray_reqabsorption_helper(CSOUND *csound, OPDS *h, K_DATA *k_data, CSNREF *source_a, CSNREF *source_b, CSN_ARRAY **p_array, CSNREF *p_handle, const MYFLT *value_a, const MYFLT *value_b, bool *is_published, CSN_ACOUSTICS_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle_a = source_a == NULL ? 0 : source_a->id;
    uint32_t source_handle_b = source_b == NULL ? 0 : source_b->id;

    const char *name_a = NULL;
    const char *name_b = NULL;
    acoustics_operand_names(mode, &name_a, &name_b);

    double v = value_a == NULL ? 0.0 : (double) *value_a;
    if (value_a != NULL && !IS_VALID_VALUE(v)) {
        return csound->InitError(csound, "[csnarray] The %s operand must be a valid float value", name_a);
    }
    double t = value_b == NULL ? 0.0 : (double) *value_b;
    if (value_b != NULL && !IS_VALID_VALUE(t)) {
        return csound->InitError(csound, "[csnarray] The %s operand must be a valid float value", name_b);
    }

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);
    CSN_ARRAY *source_arr_a = NULL;
    if (source_handle_a != INVALID_HANDLE) {
        CSN_SLOT *slot_a = get_slot(reg, source_handle_a);
        if (slot_a == NULL) {
            res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle_a);
            goto done;
        }
        source_arr_a = slot_a->array;
        if (source_arr_a->ndim != 1U) {
            res = csound->InitError(csound, "[csnarray] Arrays of %s must be 1D", name_a);
            goto done;
        }
        if (source_arr_a->itype == CSN_COMPLEX) {
            res = csound->InitError(csound, "[csnarray] The %s must be real arrays", name_a);
            goto done;
        }
    }

    CSN_ARRAY *source_arr_b = NULL;
    if (source_handle_b != INVALID_HANDLE) {
        CSN_SLOT *slot_b = get_slot(reg, source_handle_b);
        if (slot_b == NULL) {
            res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle_b);
            goto done;
        }
        source_arr_b = slot_b->array;
        if (source_arr_b->ndim != 1U) {
            res = csound->InitError(csound, "[csnarray] Arrays of %s must be 1D", name_b);
            goto done;
        }
        if (source_arr_b->itype == CSN_COMPLEX) {
            res = csound->InitError(csound, "[csnarray] The %s must be real arrays", name_b);
            goto done;
        }
    }

    uint32_t new_ndim = (source_handle_a == INVALID_HANDLE || source_handle_b == INVALID_HANDLE) ? 1U : 2U;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    if (new_ndim == 2U) {
        new_shape[0] = source_arr_a->shape[0];
        new_shape[1] = source_arr_b->shape[0];
    } else {
        new_shape[0] = source_handle_a != INVALID_HANDLE ? source_arr_a->shape[0] : source_arr_b->shape[0];
    }

    uint32_t protect[2] = {0};
    uint32_t np = 0U;
    if (source_handle_a != INVALID_HANDLE) protect[np++] = source_handle_a;
    if (source_handle_b != INVALID_HANDLE) protect[np++] = source_handle_b;
    if (create_csnarray_locked(csound, reg, h, new_ndim, new_shape, p_array, p_handle, protect, np, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    reqabs_assign_value(new_ndim, source_arr_a, source_arr_b, *p_array, value_a == NULL ? NULL : &v, value_b == NULL ? NULL : &t, mode);

    SET_FROM_KDATA_WITH_ID_BEGIN(*k_data, reg, new_shape, new_ndim, CSN_REAL, p_handle->id);
    if (source_handle_a != INVALID_HANDLE && source_handle_b != INVALID_HANDLE) {
        set_array_version(&k_data->prev_source_version, &source_arr_a->version);
        set_array_version(&k_data->prev_source_version_b, &source_arr_b->version);
    } else if (source_handle_a != INVALID_HANDLE) {
        set_array_version(&k_data->prev_source_version, &source_arr_a->version);
    } else if (source_handle_b != INVALID_HANDLE) {
        set_array_version(&k_data->prev_source_version_b, &source_arr_b->version);
    }
    *is_published = false;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_reqabsorption_k_init_helper(CSOUND *csound, OPDS *h, K_DATA *k_data, CSNREF *source_a, CSNREF *source_b, CSN_ARRAY **p_array, CSNREF *p_handle, bool *is_published, CSN_ACOUSTICS_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle_a = source_a == NULL ? 0 : source_a->id;
    uint32_t source_handle_b = source_b == NULL ? 0 : source_b->id;

    const char *name_a = NULL;
    const char *name_b = NULL;
    acoustics_operand_names(mode, &name_a, &name_b);

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);
    CSN_ARRAY *source_arr_a = NULL;
    if (source_handle_a != INVALID_HANDLE) {
        CSN_SLOT *slot_a = get_slot(reg, source_handle_a);
        if (slot_a == NULL) {
            res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle_a);
            goto done;
        }
        source_arr_a = slot_a->array;
        if (source_arr_a->ndim != 1U) {
            res = csound->InitError(csound, "[csnarray] Arrays of %s must be 1D", name_a);
            goto done;
        }
        if (source_arr_a->itype == CSN_COMPLEX) {
            res = csound->InitError(csound, "[csnarray] The %s must be real arrays", name_a);
            goto done;
        }
    }

    CSN_ARRAY *source_arr_b = NULL;
    if (source_handle_b != INVALID_HANDLE) {
        CSN_SLOT *slot_b = get_slot(reg, source_handle_b);
        if (slot_b == NULL) {
            res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle_b);
            goto done;
        }
        source_arr_b = slot_b->array;
        if (source_arr_b->ndim != 1U) {
            res = csound->InitError(csound, "[csnarray] Arrays of %s must be 1D", name_b);
            goto done;
        }
        if (source_arr_b->itype == CSN_COMPLEX) {
            res = csound->InitError(csound, "[csnarray] The %s must be real arrays", name_b);
            goto done;
        }
    }

    uint32_t new_ndim = (source_handle_a == INVALID_HANDLE || source_handle_b == INVALID_HANDLE) ? 1U : 2U;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    if (new_ndim == 2U) {
        new_shape[0] = source_arr_a->shape[0];
        new_shape[1] = source_arr_b->shape[0];
    } else {
        new_shape[0] = source_handle_a != INVALID_HANDLE ? source_arr_a->shape[0] : source_arr_b->shape[0];
    }

    uint32_t protect[2] = {0};
    uint32_t np = 0U;
    if (source_handle_a != INVALID_HANDLE) protect[np++] = source_handle_a;
    if (source_handle_b != INVALID_HANDLE) protect[np++] = source_handle_b;
    if (create_csnarray_locked(csound, reg, h, new_ndim, new_shape, p_array, p_handle, protect, np, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    reset_empty_csnarray((*p_array), new_ndim, new_shape, CSN_REAL);

    SET_FROM_KDATA_WITH_ID_BEGIN(*k_data, reg, new_shape, new_ndim, CSN_REAL, p_handle->id);
    set_array_version(&k_data->prev_output_version, &(*p_array)->version);
    if (source_handle_a != INVALID_HANDLE && source_handle_b != INVALID_HANDLE) {
        set_array_version(&k_data->prev_source_version, &source_arr_a->version);
        set_array_version(&k_data->prev_source_version_b, &source_arr_b->version);
    } else if (source_handle_a != INVALID_HANDLE) {
        set_array_version(&k_data->prev_source_version, &source_arr_a->version);
    } else if (source_handle_b != INVALID_HANDLE) {
        set_array_version(&k_data->prev_source_version_b, &source_arr_b->version);
    }
    *is_published = false;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_reqabsorption_k_helper(CSOUND *csound, OPDS *h, K_DATA *k_data, CSNREF *source_a, CSNREF *source_b, CSN_ARRAY **p_array, CSNREF *p_handle, const MYFLT *volume, const MYFLT *t60target, bool *is_published, const MYFLT *trig, CSN_ACOUSTICS_MODE mode) {
    CSN_REGISTRY *reg = k_data->registry;
    uint32_t owned_handle = k_data->owned_handle;
    CHECK_REG_HANDLE(csound, h, reg, owned_handle);

    uint32_t source_handle_a = source_a == NULL ? 0 : source_a->id;
    uint32_t source_handle_b = source_b == NULL ? 0 : source_b->id;

    const char *name_a = NULL;
    const char *name_b = NULL;
    acoustics_operand_names(mode, &name_a, &name_b);

    double v = volume == NULL ? 0.0 : (double) *volume;
    if (volume != NULL && !IS_VALID_VALUE(v)) {
        return csound->PerfError(csound, h, "[csnarray] The %s operand must be a valid float value", name_a);
    }
    double t = t60target == NULL ? 0.0 : (double) *t60target;
    if (t60target != NULL && !IS_VALID_VALUE(t)) {
        return csound->PerfError(csound, h, "[csnarray] The %s operand must be a valid float value", name_b);
    }

    int32_t res = OK;
    const char *err = NULL;

    res = CHECK_SELF_ALIAS(csound, h, k_data, source_handle_a, source_handle_b);
    if (res != OK) return res;

    CHECK_KTRIG(trig);

    csound->LockMutex(reg->mutex);
    CSN_ARRAY *source_arr_a = NULL;
    if (source_handle_a != INVALID_HANDLE) {
        CSN_SLOT *slot_a = get_slot(reg, source_handle_a);
        if (slot_a == NULL) {
            res = csn_locked_perf_error(csound, h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle_a);
            goto done;
        }
        source_arr_a = slot_a->array;
        if (source_arr_a->ndim != 1U) {
            res = csn_locked_perf_error(csound, h, "[csnarray] Arrays of %s must be 1D", name_a);
            goto done;
        }
        if (source_arr_a->itype == CSN_COMPLEX) {
            res = csn_locked_perf_error(csound, h, "[csnarray] The %s must be real arrays", name_a);
            goto done;
        }
    }

    CSN_ARRAY *source_arr_b = NULL;
    if (source_handle_b != INVALID_HANDLE) {
        CSN_SLOT *slot_b = get_slot(reg, source_handle_b);
        if (slot_b == NULL) {
            res = csn_locked_perf_error(csound, h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle_b);
            goto done;
        }
        source_arr_b = slot_b->array;
        if (source_arr_b->ndim != 1U) {
            res = csn_locked_perf_error(csound, h, "[csnarray] Arrays of %s must be 1D", name_b);
            goto done;
        }
        if (source_arr_b->itype == CSN_COMPLEX) {
            res = csn_locked_perf_error(csound, h, "[csnarray] The %s must be real arrays", name_b);
            goto done;
        }
    }

    if (*is_published) {
        bool is_same_source_a = true;
        bool is_same_source_b = true;
        bool is_same_volume = true;
        bool is_same_target = true;
        bool is_same_result = false;
        if (source_handle_a != INVALID_HANDLE && source_handle_b != INVALID_HANDLE) {
            is_same_source_a = is_same_array_version(&k_data->prev_source_version, &source_arr_a->version);
            is_same_source_b = is_same_array_version(&k_data->prev_source_version_b, &source_arr_b->version);
        } else if (source_handle_a != INVALID_HANDLE) {
            is_same_target = k_data->prev_scalar_param_b == t;
            is_same_source_a = is_same_array_version(&k_data->prev_source_version, &source_arr_a->version);
        } else if (source_handle_b != INVALID_HANDLE) {
            is_same_volume = k_data->prev_scalar_param == v;
            is_same_source_b = is_same_array_version(&k_data->prev_source_version_b, &source_arr_b->version);
        }
        CSN_SLOT *res_slot = get_slot(reg, owned_handle);
        if (res_slot != NULL) {
            is_same_result = is_same_array_version(&k_data->prev_output_version, &res_slot->array->version);
        }

        if (is_same_source_a && is_same_source_b && is_same_volume && is_same_target && is_same_result) {
            p_handle->id = owned_handle;
            goto done;
        }
    }

    uint32_t new_ndim = (source_handle_a == INVALID_HANDLE || source_handle_b == INVALID_HANDLE) ? 1U : 2U;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    if (new_ndim == 2U) {
        new_shape[0] = source_arr_a->shape[0];
        new_shape[1] = source_arr_b->shape[0];
    } else {
        new_shape[0] = source_handle_a != INVALID_HANDLE ? source_arr_a->shape[0] : source_arr_b->shape[0];
    }

    size_t req_size = 0;
    if (get_array_size_from_shape(&req_size, new_ndim, new_shape) != OK) {
        res = csn_locked_perf_error(csound, h, "[csnarray] Invalid shape or element count exceeds the configured limit");
        goto done;
    }

    CSN_ARRAY *arr = NULL;
    size_t logical_size = req_size;
    res = NEED_TO_UPDATE_SLOT(csound, h, &arr, k_data, NULL, new_ndim, new_shape, logical_size, CSN_REAL, err);
    if (res != OK) goto done;
    *p_array = arr;

    reqabs_assign_value(new_ndim, source_arr_a, source_arr_b, *p_array, volume == NULL ? NULL : &v, t60target == NULL ? NULL : &t, mode);

    SET_FROM_KDATA_END_WITH_ID(*k_data, p_handle, new_shape, new_ndim, CSN_REAL);
    set_array_version(&k_data->prev_output_version, &(*p_array)->version);
    if (source_handle_a != INVALID_HANDLE && source_handle_b != INVALID_HANDLE) {
        set_array_version(&k_data->prev_source_version, &source_arr_a->version);
        set_array_version(&k_data->prev_source_version_b, &source_arr_b->version);
    } else if (source_handle_a != INVALID_HANDLE) {
        set_array_version(&k_data->prev_source_version, &source_arr_a->version);
    } else if (source_handle_b != INVALID_HANDLE) {
        set_array_version(&k_data->prev_source_version_b, &source_arr_b->version);
    }
    k_data->prev_scalar_param = v;
    k_data->prev_scalar_param_b = t;
    *is_published = true;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_reqabsorption_hh_deinit(CSOUND *csound, CSN_REQABSOLEV_HH *p) {
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

int32_t csnarray_reqabsorption_sh_deinit(CSOUND *csound, CSN_REQABSOLEV_SH *p) {
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

int32_t csnarray_reqabsorption_hs_deinit(CSOUND *csound, CSN_REQABSOLEV_HS *p) {
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

int32_t csnarray_reqabsorption_hh(CSOUND *csound, CSN_REQABSOLEV_HH *p) {
    return csnarray_reqabsorption_helper(csound, &p->h, &p->k_data, p->source_handle_a, p->source_handle_b, &p->array, p->handle, NULL, NULL, &p->is_published, CSN_RT60ABS);
}

int32_t csnarray_reqabsorption_sh(CSOUND *csound, CSN_REQABSOLEV_SH *p) {
    return csnarray_reqabsorption_helper(csound, &p->h, &p->k_data, NULL, p->source_handle, &p->array, p->handle, p->arg_a, NULL, &p->is_published, CSN_RT60ABS);
}

int32_t csnarray_reqabsorption_hs(CSOUND *csound, CSN_REQABSOLEV_HS *p) {
    return csnarray_reqabsorption_helper(csound, &p->h, &p->k_data, p->source_handle, NULL, &p->array, p->handle, NULL, p->arg_b, &p->is_published, CSN_RT60ABS);
}

int32_t csnarray_reqabsorption_sh_k_init(CSOUND *csound, CSN_REQABSOLEV_SH *p) {
    return csnarray_reqabsorption_k_init_helper(csound, &p->h, &p->k_data, NULL, p->source_handle, &p->array, p->handle, &p->is_published, CSN_RT60ABS);
}

int32_t csnarray_reqabsorption_hs_k_init(CSOUND *csound, CSN_REQABSOLEV_HS *p) {
    return csnarray_reqabsorption_k_init_helper(csound, &p->h, &p->k_data, p->source_handle, NULL, &p->array, p->handle, &p->is_published, CSN_RT60ABS);
}

int32_t csnarray_reqabsorption_hh_k_init(CSOUND *csound, CSN_REQABSOLEV_HH *p) {
    return csnarray_reqabsorption_k_init_helper(csound, &p->h, &p->k_data, p->source_handle_a, p->source_handle_b, &p->array, p->handle, &p->is_published, CSN_RT60ABS);
}

int32_t csnarray_reqabsorption_hh_k(CSOUND *csound, CSN_REQABSOLEV_HH *p) {
    return csnarray_reqabsorption_k_helper(csound, &p->h, &p->k_data, p->source_handle_a, p->source_handle_b, &p->array, p->handle, NULL, NULL, &p->is_published, p->trig, CSN_RT60ABS);
}

int32_t csnarray_reqabsorption_sh_k(CSOUND *csound, CSN_REQABSOLEV_SH *p) {
    return csnarray_reqabsorption_k_helper(csound, &p->h, &p->k_data, NULL, p->source_handle, &p->array, p->handle, p->arg_a, NULL, &p->is_published, p->trig, CSN_RT60ABS);
}

int32_t csnarray_reqabsorption_hs_k(CSOUND *csound, CSN_REQABSOLEV_HS *p) {
    return csnarray_reqabsorption_k_helper(csound, &p->h, &p->k_data, p->source_handle, NULL, &p->array, p->handle, NULL, p->arg_b, &p->is_published, p->trig, CSN_RT60ABS);
}

static int32_t csnarray_reqabsorption_ss_helper(CSOUND *csound, CSN_NONARR_OP *p, bool is_perf, CSN_ACOUSTICS_MODE mode) {
    OPDS *perf_h = is_perf ? &p->h : NULL;
    const char *name_a = NULL;
    const char *name_b = NULL;
    acoustics_operand_names(mode, &name_a, &name_b);

    double arg_a = (double) *p->arg_a;
    if (!IS_VALID_VALUE(arg_a)) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] The %s operand must be a valid float value", name_a);
    }
    double arg_b = (double) *p->arg_b;
    if (!IS_VALID_VALUE(arg_b)) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] The %s operand must be a valid float value", name_b);
    }

    /* Through the same guarded evaluation the array forms use, so that every
       overload of one opcode answers the same thing for the same operands. */
    *p->value = (MYFLT) acoustics_value(arg_a, arg_b, mode);
    return OK;
}

int32_t csnarray_reqabsorption_ss(CSOUND *csound, CSN_NONARR_OP *p) {
    return csnarray_reqabsorption_ss_helper(csound, p, false, CSN_RT60ABS);
}

int32_t csnarray_reqabsorption_ss_k(CSOUND *csound, CSN_NONARR_OP *p) {
    return csnarray_reqabsorption_ss_helper(csound, p, true, CSN_RT60ABS);
}

int32_t csnarray_fschroeder_hh(CSOUND *csound, CSN_REQABSOLEV_HH *p) {
    return csnarray_reqabsorption_helper(csound, &p->h, &p->k_data, p->source_handle_a, p->source_handle_b, &p->array, p->handle, NULL, NULL, &p->is_published, CSN_FSCHROEDER);
}

int32_t csnarray_fschroeder_sh(CSOUND *csound, CSN_REQABSOLEV_SH *p) {
    return csnarray_reqabsorption_helper(csound, &p->h, &p->k_data, NULL, p->source_handle, &p->array, p->handle, p->arg_a, NULL, &p->is_published, CSN_FSCHROEDER);
}

int32_t csnarray_fschroeder_hs(CSOUND *csound, CSN_REQABSOLEV_HS *p) {
    return csnarray_reqabsorption_helper(csound, &p->h, &p->k_data, p->source_handle, NULL, &p->array, p->handle, NULL, p->arg_b, &p->is_published, CSN_FSCHROEDER);
}

int32_t csnarray_fschroeder_hh_k_init(CSOUND *csound, CSN_REQABSOLEV_HH *p) {
    return csnarray_reqabsorption_k_init_helper(csound, &p->h, &p->k_data, p->source_handle_a, p->source_handle_b, &p->array, p->handle, &p->is_published, CSN_FSCHROEDER);
}

int32_t csnarray_fschroeder_hh_k(CSOUND *csound, CSN_REQABSOLEV_HH *p) {
    return csnarray_reqabsorption_k_helper(csound, &p->h, &p->k_data, p->source_handle_a, p->source_handle_b, &p->array, p->handle, NULL, NULL, &p->is_published, p->trig, CSN_FSCHROEDER);
}

int32_t csnarray_fschroeder_sh_k_init(CSOUND *csound, CSN_REQABSOLEV_SH *p) {
    return csnarray_reqabsorption_k_init_helper(csound, &p->h, &p->k_data, NULL, p->source_handle, &p->array, p->handle, &p->is_published, CSN_FSCHROEDER);
}

int32_t csnarray_fschroeder_hs_k_init(CSOUND *csound, CSN_REQABSOLEV_HS *p) {
    return csnarray_reqabsorption_k_init_helper(csound, &p->h, &p->k_data, p->source_handle, NULL, &p->array, p->handle, &p->is_published, CSN_FSCHROEDER);
}

int32_t csnarray_fschroeder_sh_k(CSOUND *csound, CSN_REQABSOLEV_SH *p) {
    return csnarray_reqabsorption_k_helper(csound, &p->h, &p->k_data, NULL, p->source_handle, &p->array, p->handle, p->arg_a, NULL, &p->is_published, p->trig, CSN_FSCHROEDER);
}
int32_t csnarray_fschroeder_hs_k(CSOUND *csound, CSN_REQABSOLEV_HS *p) {
    return csnarray_reqabsorption_k_helper(csound, &p->h, &p->k_data, p->source_handle, NULL, &p->array, p->handle, NULL, p->arg_b, &p->is_published, p->trig, CSN_FSCHROEDER);
}
int32_t csnarray_fschroeder_ss(CSOUND *csound, CSN_NONARR_OP *p) {
    return csnarray_reqabsorption_ss_helper(csound, p, false, CSN_FSCHROEDER);
}

int32_t csnarray_fschroeder_ss_k(CSOUND *csound, CSN_NONARR_OP *p) {
    return csnarray_reqabsorption_ss_helper(csound, p, true, CSN_FSCHROEDER);
}

int32_t csnarray_fpqr_deinit(CSOUND *csound, CSN_FMODUS_ARR *p) {
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}


int32_t csnarray_fpqr_nd(CSOUND *csound, CSN_FMODUS_ARR *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle_a = p->source_handle_a->id;
    uint32_t source_handle_b = p->source_handle_b->id;

    double c = (double) *p->sound_speed;
    if (!IS_VALID_VALUE(c)) {
        return csound->InitError(csound, "[csnarray] Speed of sound should be a valid float value");
    }
    double fac = c / 2.0;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot_a = get_slot(reg, source_handle_a);
    if (slot_a == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle_a);
        goto done;
    }

    CSN_ARRAY *source_arr_a = slot_a->array;
    if (source_arr_a->ndim != 2U) {
        res = csound->InitError(csound, "[csnarray] Arrays of room measures must be 2D");
        goto done;
    }
    if (source_arr_a->shape[1] != 3U) {
        res = csound->InitError(csound, "[csnarray] Arrays of measures must have three elements (L, W, H)");
        goto done;
    }

    if (source_arr_a->itype == CSN_COMPLEX) {
        res = csound->InitError(csound, "[csnarray] Room measures must be real arrays");
        goto done;
    }

    CSN_SLOT *slot_b = get_slot(reg, source_handle_b);
    if (slot_b == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle_b);
        goto done;
    }

    CSN_ARRAY *source_arr_b = slot_b->array;
    if (source_arr_b->ndim != 1U) {
        res = csound->InitError(csound, "[csnarray] Arrays of mode indexes must be 1D");
        goto done;
    }
    if (source_arr_b->shape[0] != 3U) {
        res = csound->InitError(csound, "[csnarray] Arrays of mode indexes must have three elements (p, q, r)");
        goto done;
    }
    if (source_arr_b->itype == CSN_COMPLEX) {
        res = csound->InitError(csound, "[csnarray] Mode indexes must be real arrays");
        goto done;
    }

    for (uint32_t i = 0; i < source_arr_b->shape[0]; i++) {
        if (!is_valid_mode_index(source_arr_b->data[i])) {
            res = csound->InitError(csound, "[csnarray] Mode indexes (p, q, r) must be non-negative integers");
            goto done;
        }
    }

    for (size_t i = 0; i < source_arr_a->size; i++) {
        if (!(source_arr_a->data[i] > 0.0)) {
            res = csound->InitError(csound, "[csnarray] Room measures (L, W, H) must be greater than zero");
            goto done;
        }
    }

    uint32_t new_dim = 1U;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    new_shape[0] = source_arr_a->shape[0];

    uint32_t protect[2] = { source_handle_a, source_handle_b };
    if (create_csnarray_locked(csound, reg, &p->h, new_dim, new_shape, &p->array, p->handle, protect, 2U, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    uint32_t nrows = source_arr_a->shape[0];
    uint32_t ncols = source_arr_a->shape[1];
    for (uint32_t i = 0; i < nrows; i++) {
        double s = 0.0;
        for (uint32_t j = 0; j < ncols; j++) {
            double m = source_arr_b->data[j] / source_arr_a->data[(size_t) i * ncols + j];
            s += m * m;
        }
        p->array->data[i] = fac * sqrt(s);
    }

    SET_KDATA_BEGIN(p, reg);
    set_array_version(&p->k_data.prev_output_version, &p->array->version);
    set_array_version(&p->k_data.prev_source_version, &source_arr_a->version);
    set_array_version(&p->k_data.prev_source_version_b, &source_arr_b->version);
    p->k_data.prev_scalar_param = fac;
    p->is_published = false;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_fpqr_1d(CSOUND *csound, CSN_FMODUS_SCALAR *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle_a = p->source_handle_a->id;
    uint32_t source_handle_b = p->source_handle_b->id;

    double c = (double) *p->sound_speed;
    if (!IS_VALID_VALUE(c)) {
        return csound->InitError(csound, "[csnarray] Speed of sound should be a valid float value");
    }
    double fac = c / 2.0;

    int32_t res = OK;

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot_a = get_slot(reg, source_handle_a);
    if (slot_a == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle_a);
        goto done;
    }

    CSN_ARRAY *source_arr_a = slot_a->array;
    if (source_arr_a->ndim != 1U) {
        res = csound->InitError(csound, "[csnarray] Arrays of room measures must be 1D");
        goto done;
    }
    if (source_arr_a->shape[0] != 3U) {
        res = csound->InitError(csound, "[csnarray] Arrays of measures must have three elements (L, W, H)");
        goto done;
    }

    if (source_arr_a->itype == CSN_COMPLEX) {
        res = csound->InitError(csound, "[csnarray] Room measures must be real arrays");
        goto done;
    }

    CSN_SLOT *slot_b = get_slot(reg, source_handle_b);
    if (slot_b == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle_b);
        goto done;
    }

    CSN_ARRAY *source_arr_b = slot_b->array;
    if (source_arr_b->ndim != 1U) {
        res = csound->InitError(csound, "[csnarray] Arrays of mode indexes must be 1D");
        goto done;
    }
    if (source_arr_b->shape[0] != 3U) {
        res = csound->InitError(csound, "[csnarray] Arrays of mode indexes must have three elements (p, q, r)");
        goto done;
    }
    if (source_arr_b->itype == CSN_COMPLEX) {
        res = csound->InitError(csound, "[csnarray] Mode indexes must be real arrays");
        goto done;
    }

    for (uint32_t i = 0; i < source_arr_b->shape[0]; i++) {
        if (!is_valid_mode_index(source_arr_b->data[i])) {
            res = csound->InitError(csound, "[csnarray] Mode indexes (p, q, r) must be non-negative integers");
            goto done;
        }
    }

    for (size_t i = 0; i < source_arr_a->size; i++) {
        if (!(source_arr_a->data[i] > 0.0)) {
            res = csound->InitError(csound, "[csnarray] Room measures (L, W, H) must be greater than zero");
            goto done;
        }
    }

    double s = 0.0;
    for (uint32_t i = 0; i < source_arr_a->shape[0]; i++) {
        double m = source_arr_b->data[i] / source_arr_a->data[i];
        s += m * m;
    }

    *p->value = (MYFLT) (fac * sqrt(s));
    p->prev_fac = fac;
    p->registry = reg;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_fpqr_nd_k(CSOUND *csound, CSN_FMODUS_ARR *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    uint32_t owned_handle = p->k_data.owned_handle;
    CHECK_REG_HANDLE(csound, &p->h, reg, owned_handle);

    uint32_t source_handle_a = p->source_handle_a->id;
    uint32_t source_handle_b = p->source_handle_b->id;

    double fac = p->k_data.prev_scalar_param;

    int32_t res = OK;
    const char *err = NULL;

    res = CHECK_SELF_ALIAS(csound, &p->h, &p->k_data, source_handle_a, source_handle_b);
    if (res != OK) return res;

    CHECK_KTRIG(p->trig);

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot_a = get_slot(reg, source_handle_a);
    if (slot_a == NULL) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle_a);
        goto done;
    }

    CSN_ARRAY *source_arr_a = slot_a->array;
    if (source_arr_a->ndim != 2U) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Arrays of room measures must be 2D");
        goto done;
    }
    if (source_arr_a->shape[1] != 3U) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Arrays of measures must have three elements (L, W, H)");
        goto done;
    }

    if (source_arr_a->itype == CSN_COMPLEX) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Room measures must be real arrays");
        goto done;
    }

    CSN_SLOT *slot_b = get_slot(reg, source_handle_b);
    if (slot_b == NULL) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle_b);
        goto done;
    }

    CSN_ARRAY *source_arr_b = slot_b->array;
    if (source_arr_b->ndim != 1U) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Arrays of mode indexes must be 1D");
        goto done;
    }
    if (source_arr_b->shape[0] != 3U) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Arrays of mode indexes must have three elements (p, q, r)");
        goto done;
    }
    if (source_arr_b->itype == CSN_COMPLEX) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Mode indexes must be real arrays");
        goto done;
    }

    if (p->is_published) {
        bool is_same_source_a = is_same_array_version(&p->k_data.prev_source_version, &source_arr_a->version);
        bool is_same_source_b = is_same_array_version(&p->k_data.prev_source_version_b, &source_arr_b->version);
        bool is_same_result = false;
        CSN_SLOT *res_slot = get_slot(reg, owned_handle);
        if (res_slot != NULL) {
            is_same_result = is_same_array_version(&p->k_data.prev_output_version, &res_slot->array->version);
        }

        if (is_same_source_a && is_same_source_b && is_same_result) {
            p->handle->id = owned_handle;
            goto done;
        }
    }

    for (uint32_t i = 0; i < source_arr_b->shape[0]; i++) {
        if (!is_valid_mode_index(source_arr_b->data[i])) {
            res = csn_locked_perf_error(csound, &p->h, "[csnarray] Mode indexes (p, q, r) must be non-negative integers");
            goto done;
        }
    }

    for (size_t i = 0; i < source_arr_a->size; i++) {
        if (!(source_arr_a->data[i] > 0.0)) {
            res = csn_locked_perf_error(csound, &p->h, "[csnarray] Room measures (L, W, H) must be greater than zero");
            goto done;
        }
    }

    uint32_t new_dim = 1U;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    new_shape[0] = source_arr_a->shape[0];

    size_t req_size = 0;
    if (get_array_size_from_shape(&req_size, new_dim, new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSN_ARRAY *arr = NULL;
    size_t logical_size = req_size;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, new_dim, new_shape, logical_size, CSN_REAL, err);
    if (res != OK) goto done;
    p->array = arr;

    uint32_t nrows = source_arr_a->shape[0];
    uint32_t ncols = source_arr_a->shape[1];
    for (uint32_t i = 0; i < nrows; i++) {
        double s = 0.0;
        for (uint32_t j = 0; j < ncols; j++) {
            double m = source_arr_b->data[j] / source_arr_a->data[(size_t) i * ncols + j];
            s += m * m;
        }
        p->array->data[i] = fac * sqrt(s);
    }

    SET_KDATA_END(p, new_shape, new_dim, CSN_REAL);
    set_array_version(&p->k_data.prev_output_version, &p->array->version);
    set_array_version(&p->k_data.prev_source_version, &source_arr_a->version);
    set_array_version(&p->k_data.prev_source_version_b, &source_arr_b->version);
    p->is_published = true;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_fpqr_1d_k(CSOUND *csound, CSN_FMODUS_SCALAR *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    uint32_t source_handle_a = p->source_handle_a->id;
    uint32_t source_handle_b = p->source_handle_b->id;

    double fac = p->prev_fac;

    int32_t res = OK;

    CHECK_KTRIG(p->trig);

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot_a = get_slot(reg, source_handle_a);
    if (slot_a == NULL) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle_a);
        goto done;
    }

    CSN_ARRAY *source_arr_a = slot_a->array;
    if (source_arr_a->ndim != 1U) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Arrays of room measures must be 1D");
        goto done;
    }
    if (source_arr_a->shape[0] != 3U) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Arrays of measures must have three elements (L, W, H)");
        goto done;
    }

    if (source_arr_a->itype == CSN_COMPLEX) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Room measures must be real arrays");
        goto done;
    }

    CSN_SLOT *slot_b = get_slot(reg, source_handle_b);
    if (slot_b == NULL) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle_b);
        goto done;
    }

    CSN_ARRAY *source_arr_b = slot_b->array;
    if (source_arr_b->ndim != 1U) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Arrays of mode indexes must be 1D");
        goto done;
    }
    if (source_arr_b->shape[0] != 3U) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Arrays of mode indexes must have three elements (p, q, r)");
        goto done;
    }
    if (source_arr_b->itype == CSN_COMPLEX) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Mode indexes must be real arrays");
        goto done;
    }

    for (uint32_t i = 0; i < source_arr_b->shape[0]; i++) {
        if (!is_valid_mode_index(source_arr_b->data[i])) {
            res = csn_locked_perf_error(csound, &p->h, "[csnarray] Mode indexes (p, q, r) must be non-negative integers");
            goto done;
        }
    }

    for (size_t i = 0; i < source_arr_a->size; i++) {
        if (!(source_arr_a->data[i] > 0.0)) {
            res = csn_locked_perf_error(csound, &p->h, "[csnarray] Room measures (L, W, H) must be greater than zero");
            goto done;
        }
    }

    double s = 0.0;
    for (uint32_t i = 0; i < source_arr_a->shape[0]; i++) {
        double m = source_arr_b->data[i] / source_arr_a->data[i];
        s += m * m;
    }

    *p->value = (MYFLT) (fac * sqrt(s));

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}


static int32_t csnarray_timeconv_helper(CSOUND *csound, CSN_SRTIMES *p, CSN_TIMECONV_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    const char *err = NULL;

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

    if (source_arr->itype == CSN_COMPLEX) {
        res = csound->InitError(csound, "[csnarray] Array of samples/times must be real array");
        goto done;
    }

    uint32_t new_ndim = source_arr->ndim;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    memcpy(new_shape, source_arr->shape, sizeof(new_shape));

    if (create_csnarray_locked(csound, reg, &p->h, new_ndim, new_shape, &p->array, p->handle, &source_handle, 1U, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;
    for (size_t i = 0; i < source_arr->size; i++) {
        double value = 0.0;
        double source = source_arr->data[i];
        switch (mode) {
            case CSN_SAMPLES2MILLIS:
                value = 1000.0 * source / sr;
                break;
            case CSN_SAMPLES2SECONDS:
                value = source / sr;
                break;
            case CSN_MILLIS2SAMPLES:
                value = source * sr / 1000.0;
                break;
            case CSN_SECONDS2SAMPLES:
                value = source * sr;
                break;
        }
        arr->data[i] = value;
    }

    SET_KDATA_BEGIN(p, reg);
    set_array_version(&p->k_data.prev_output_version, &arr->version);
    set_array_version(&p->k_data.prev_source_version, &source_arr->version);
    p->k_data.prev_scalar_param = sr;
    p->is_published = false;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_timeconv_k_helper(CSOUND *csound, CSN_SRTIMES *p, CSN_TIMECONV_MODE mode) {
    CSN_REGISTRY *reg = p->k_data.registry;
    uint32_t owned_handle = p->k_data.owned_handle;
    CHECK_REG_HANDLE(csound, &p->h, reg, owned_handle);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    const char *err = NULL;

    res = CHECK_SELF_ALIAS(csound, &p->h, &p->k_data, source_handle, 0);
    if (res != OK) return res;

    CHECK_KTRIG(p->trig);

    double sr = p->k_data.prev_scalar_param;

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
        goto done;
    }
    CSN_ARRAY *source_arr = slot->array;

    if (source_arr->itype == CSN_COMPLEX) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Array of samples/times must be real array");
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

    uint32_t new_ndim = source_arr->ndim;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    memcpy(new_shape, source_arr->shape, sizeof(new_shape));

    if (NEED_TO_UPDATE_SLOT(csound, &p->h, &p->array, &p->k_data, &owned_handle, new_ndim, new_shape, source_arr->size, CSN_REAL, err) != OK) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;
    for (size_t i = 0; i < source_arr->size; i++) {
        double value = 0.0;
        double source = source_arr->data[i];
        switch (mode) {
            case CSN_SAMPLES2MILLIS:
                value = 1000.0 * source / sr;
                break;
            case CSN_SAMPLES2SECONDS:
                value = source / sr;
                break;
            case CSN_MILLIS2SAMPLES:
                value = source * sr / 1000.0;
                break;
            case CSN_SECONDS2SAMPLES:
                value = source * sr;
                break;
        }
        arr->data[i] = value;
    }

    SET_KDATA_END(p, new_shape, new_ndim, CSN_REAL);
    set_array_version(&p->k_data.prev_output_version, &arr->version);
    set_array_version(&p->k_data.prev_source_version, &source_arr->version);
    p->is_published = true;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}


int32_t csnarray_srtimes_deinit(CSOUND *csound, CSN_SRTIMES *p) {
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

int32_t csnarray_samplestomillis(CSOUND *csound, CSN_SRTIMES *p) {
    return csnarray_timeconv_helper(csound, p, CSN_SAMPLES2MILLIS);
}

int32_t csnarray_samplestoseconds(CSOUND *csound, CSN_SRTIMES *p) {
    return csnarray_timeconv_helper(csound, p, CSN_SAMPLES2SECONDS);
}

int32_t csnarray_millistosamples(CSOUND *csound, CSN_SRTIMES *p) {
    return csnarray_timeconv_helper(csound, p, CSN_MILLIS2SAMPLES);
}

int32_t csnarray_secondstosamples(CSOUND *csound, CSN_SRTIMES *p) {
    return csnarray_timeconv_helper(csound, p, CSN_SECONDS2SAMPLES);
}

int32_t csnarray_samplestomillis_k(CSOUND *csound, CSN_SRTIMES *p) {
    return csnarray_timeconv_k_helper(csound, p, CSN_SAMPLES2MILLIS);
}

int32_t csnarray_samplestoseconds_k(CSOUND *csound, CSN_SRTIMES *p) {
    return csnarray_timeconv_k_helper(csound, p, CSN_SAMPLES2SECONDS);
}

int32_t csnarray_millistosamples_k(CSOUND *csound, CSN_SRTIMES *p) {
    return csnarray_timeconv_k_helper(csound, p, CSN_MILLIS2SAMPLES);
}

int32_t csnarray_secondstosamples_k(CSOUND *csound, CSN_SRTIMES *p) {
    return csnarray_timeconv_k_helper(csound, p, CSN_SECONDS2SAMPLES);
}


static int32_t csnarray_timeconv_s_helper(CSOUND *csound, CSN_NONARR_OP *p, CSN_TIMECONV_MODE mode) {
    double source = (double) *p->arg_a;
    if (!IS_VALID_VALUE(source)) {
        return csound->InitError(csound, "[csnarray] Invalid millis value");
    }
    double sr = (double) *p->arg_b;
    if (!IS_VALID_SR(sr)) {
        return csound->InitError(csound, "[csnarray] Invalid sample rate");
    }

    double value = 0.0;
    switch (mode) {
        case CSN_SAMPLES2MILLIS:
            value = 1000.0 * source / sr;
            break;
        case CSN_SAMPLES2SECONDS:
            value = source / sr;
            break;
        case CSN_MILLIS2SAMPLES:
            value = source * sr / 1000.0;
            break;
        case CSN_SECONDS2SAMPLES:
            value = source * sr;
            break;
    }

    *p->value = (MYFLT) value;

    return OK;
}

static int32_t csnarray_timeconv_s_k_helper(CSOUND *csound, CSN_NONARR_OP *p, CSN_TIMECONV_MODE mode) {
    double source = (double) *p->arg_a;
    if (!IS_VALID_VALUE(source)) {
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid millis value");
    }
    double sr = (double) *p->arg_b;
    if (!IS_VALID_SR(sr)) {
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid sample rate");
    }

    double value = 0.0;
    switch (mode) {
        case CSN_SAMPLES2MILLIS:
            value = 1000.0 * source / sr;
            break;
        case CSN_SAMPLES2SECONDS:
            value = source / sr;
            break;
        case CSN_MILLIS2SAMPLES:
            value = source * sr / 1000.0;
            break;
        case CSN_SECONDS2SAMPLES:
            value = source * sr;
            break;
    }

    *p->value = (MYFLT) value;

    return OK;
}

int32_t csnarray_samplestomillis_s(CSOUND *csound, CSN_NONARR_OP *p) {
    return csnarray_timeconv_s_helper(csound, p, CSN_SAMPLES2MILLIS);
}

int32_t csnarray_samplestoseconds_s(CSOUND *csound, CSN_NONARR_OP *p) {
    return csnarray_timeconv_s_helper(csound, p, CSN_SAMPLES2SECONDS);
}

int32_t csnarray_millistosamples_s(CSOUND *csound, CSN_NONARR_OP *p) {
    return csnarray_timeconv_s_helper(csound, p, CSN_MILLIS2SAMPLES);
}

int32_t csnarray_secondstosamples_s(CSOUND *csound, CSN_NONARR_OP *p) {
    return csnarray_timeconv_s_helper(csound, p, CSN_SECONDS2SAMPLES);
}

int32_t csnarray_samplestomillis_s_k(CSOUND *csound, CSN_NONARR_OP *p) {
    return csnarray_timeconv_s_k_helper(csound, p, CSN_SAMPLES2MILLIS);
}

int32_t csnarray_samplestoseconds_s_k(CSOUND *csound, CSN_NONARR_OP *p) {
    return csnarray_timeconv_s_k_helper(csound, p, CSN_SAMPLES2SECONDS);
}

int32_t csnarray_millistosamples_s_k(CSOUND *csound, CSN_NONARR_OP *p) {
    return csnarray_timeconv_s_k_helper(csound, p, CSN_MILLIS2SAMPLES);
}

int32_t csnarray_secondstosamples_s_k(CSOUND *csound, CSN_NONARR_OP *p) {
    return csnarray_timeconv_s_k_helper(csound, p, CSN_SECONDS2SAMPLES);
}

static int32_t sumdb_body(CSOUND *csound, OPDS *perf_h, CSN_REGISTRY *reg, uint32_t source_handle, CSN_ARRAY **source_array, const double in_axis, int32_t *axis_out, uint32_t *new_ndim, uint32_t *new_shape) {
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
    }

    CSN_ARRAY *source_arr = slot->array;
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;

    CSN_AXIS_SPEC axis_spec = csn_normalize_axis_value(in_axis, source_ndim);
    if (axis_spec.kind != CSN_AXIS_INDEX) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: finite integers %d..%u)", in_axis, source_ndim, -(int32_t) source_ndim, source_ndim - 1);
    }
    *axis_out = (int32_t) axis_spec.index;

    if (source_arr->itype != CSN_REAL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Level sum operation is defined for real array only");
    }

    *source_array = source_arr;
    *new_ndim = source_ndim - 1;
    for (uint32_t i = 0, j = 0; i < source_ndim; i++) {
        if (i != (uint32_t) *axis_out) new_shape[j++] = source_shape[i];
    }

    return OK;
}

int32_t csnarray_sumdb(CSOUND *csound, CSN_REDUCTION *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    const char *err = NULL;

    double in_axis = (double) *p->axis;

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    uint32_t new_ndim = 0;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    int32_t axis = -1;
    res = sumdb_body(csound, NULL, reg, source_handle, &source_arr, in_axis, &axis, &new_ndim, new_shape);
    if (res != OK) goto done;

    const uint32_t protect[1] = { source_handle };
    if (create_csnarray_locked(csound, reg, &p->h, new_ndim, new_shape, &p->array, p->handle, protect, 1U, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_AXIS_SLICE_ITER it;
    if (AXIS_ITER_SLICE_INIT(&it, source_arr, p->array, (uint32_t) axis) != OK) {
        res = csound->InitError(csound, "[csnarray] Invalid reduction iterator layout");
        goto done;
    }
    while (AXIS_SLICE_ITER_NEXT(&it)) {
        size_t linear = it.dst_base;
        double sum = 0.0;
        for (uint32_t i = 0; i < source_arr->shape[axis]; i++) {
            size_t source_off = it.src_base + i * it.src_axis_stride;
            sum += pow(10.0, source_arr->data[source_off] / 10.0);
        }
        p->array->data[linear] = 10.0 * log10(sum);
    }

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}


int32_t csnarray_sumdb_scalar(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
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

    CSN_ARRAY *source_arr = slot->array;
    if (source_arr->itype != CSN_REAL) {
        res = csound->InitError(csound, "[csnarray] Level sum operation is defined for real array only");
        goto done;
    }

    double y = 0.0;
    for (size_t i = 0; i < source_arr->size; i++) {
        y += pow(10.0, source_arr->data[i] / 10.0);
    }

    double result = 10.0 * log10(y);
    *p->value = (MYFLT) result;

    p->k_data.registry = reg;
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, NULL, result, 0.0);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_sumdb_k_init(CSOUND *csound, CSN_REDUCTION *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    const char *err = NULL;

    double in_axis = (double) *p->trig; // k overload

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    uint32_t new_ndim = 0;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    int32_t axis = -1;
    res = sumdb_body(csound, NULL, reg, source_handle, &source_arr, in_axis, &axis, &new_ndim, new_shape);
    if (res != OK) goto done;

    const uint32_t protect[1] = { source_handle };
    if (create_csnarray_locked(csound, reg, &p->h, new_ndim, new_shape, &p->array, p->handle, protect, 1U, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_AXIS_SLICE_ITER it;
    if (AXIS_ITER_SLICE_INIT(&it, source_arr, p->array, (uint32_t) axis) != OK) {
        res = csound->InitError(csound, "[csnarray] Invalid reduction iterator layout");
        goto done;
    }
    while (AXIS_SLICE_ITER_NEXT(&it)) {
        size_t linear = it.dst_base;
        double sum = 0.0;
        for (uint32_t i = 0; i < source_arr->shape[axis]; i++) {
            size_t source_off = it.src_base + i * it.src_axis_stride;
            sum += pow(10.0, source_arr->data[source_off] / 10.0);
        }
        p->array->data[linear] = 10.0 * log10(sum);
    }

    SET_KDATA_BEGIN(p, reg);
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, p->array, 0.0, 0.0);
    p->k_data.prev_axis_i = axis;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_sumdb_k(CSOUND *csound, CSN_REDUCTION *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    uint32_t owned_handle = p->k_data.owned_handle;
    CHECK_REG_HANDLE(csound, &p->h, reg, owned_handle);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    const char *err = NULL;
    res = CHECK_SELF_ALIAS(csound, &p->h, &p->k_data, source_handle, 0);
    if (res != OK) return res;

    double in_axis = (double) p->k_data.prev_axis_i;
    CHECK_KTRIG(p->axis); // axis -> trig in k overload

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *source_arr = NULL;
    uint32_t new_ndim = 0;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    int32_t axis = -1;
    res = sumdb_body(csound, &p->h, reg, source_handle, &source_arr, in_axis, &axis, &new_ndim, new_shape);
    if (res != OK) goto done;

    CSN_SLOT *slot_res = get_slot(reg, owned_handle);
    if (slot_res != NULL) {
        if (CAN_REUSE_LAST_RESULT(&p->k_data, source_handle, source_arr, slot_res->array)) {
            p->handle->id = owned_handle;
            goto done;
        }
    }

    size_t req_size = 0;
    if (get_array_size_from_shape(&req_size, new_ndim, new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSN_ARRAY *arr = NULL;
    size_t logical_size = source_arr->size == 0 ? 0 : req_size;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, new_ndim, new_shape, logical_size, CSN_REAL, err);
    if (res != OK) goto done;
    p->array = arr;

    CSN_AXIS_SLICE_ITER it;
    if (AXIS_ITER_SLICE_INIT(&it, source_arr, p->array, (uint32_t) axis) != OK) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Invalid reduction iterator layout");
        goto done;
    }
    while (AXIS_SLICE_ITER_NEXT(&it)) {
        size_t linear = it.dst_base;
        double sum = 0.0;
        for (uint32_t i = 0; i < source_arr->shape[axis]; i++) {
            size_t source_off = it.src_base + i * it.src_axis_stride;
            sum += pow(10.0, source_arr->data[source_off] / 10.0);
        }
        p->array->data[linear] = 10.0 * log10(sum);
    }

    SET_KDATA_END(p, new_shape, new_ndim, CSN_REAL);
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, p->array, 0.0, 0.0);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_sumdb_scalar_k(CSOUND *csound, CSN_REDUCTION_SCALAR *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    res = CHECK_SELF_ALIAS(csound, &p->h, &p->k_data, source_handle, 0);
    if (res != OK) return res;

    CHECK_KTRIG(p->trig);

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = slot->array;
    if (source_arr->itype != CSN_REAL) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Level sum operation is defined for real array only");
        goto done;
    }

    bool is_same_source = is_same_array_version(&p->k_data.prev_source_version, &source_arr->version);
    if (is_same_source) {
        *p->value = (MYFLT) p->k_data.prev_scalar_param;
        goto done;
    }

    double y = 0.0;
    for (size_t i = 0; i < source_arr->size; i++) {
        y += pow(10.0, source_arr->data[i] / 10.0);
    }

    double result = 10.0 * log10(y);
    *p->value = (MYFLT) result;

    PUBLISH_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, NULL, result, 0.0);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csnarray_sumdb_s_helper(CSOUND *csound, CSN_NONARR_OP *p) {
    (void) csound;
    double a = (double) *p->arg_a;
    double b = (double) *p->arg_b;
    double ay = pow(10.0, a / 10.0);
    double by = pow(10.0, b / 10.0);
    double y = 10.0 * log10(ay + by);
    *p->value = (MYFLT) y;
    return OK;
}

int32_t csnarray_sumdb_s(CSOUND *csound, CSN_NONARR_OP *p) {
    return csnarray_sumdb_s_helper(csound, p);
}

int32_t csnarray_sumdb_s_k(CSOUND *csound, CSN_NONARR_OP *p) {
    return csnarray_sumdb_s_helper(csound, p);
}

int32_t csnarray_feedbackgfromt60_hh(CSOUND *csound, CSN_REQABSOLEV_HH *p) {
    return csnarray_reqabsorption_helper(csound, &p->h, &p->k_data, p->source_handle_a, p->source_handle_b, &p->array, p->handle, NULL, NULL, &p->is_published, CSN_T602FBG);
}

int32_t csnarray_feedbackgfromt60_sh(CSOUND *csound, CSN_REQABSOLEV_SH *p) {
    return csnarray_reqabsorption_helper(csound, &p->h, &p->k_data, NULL, p->source_handle, &p->array, p->handle, p->arg_a, NULL, &p->is_published, CSN_T602FBG);
}

int32_t csnarray_feedbackgfromt60_hs(CSOUND *csound, CSN_REQABSOLEV_HS *p) {
    return csnarray_reqabsorption_helper(csound, &p->h, &p->k_data, p->source_handle, NULL, &p->array, p->handle, NULL, p->arg_b, &p->is_published, CSN_T602FBG);
}

int32_t csnarray_feedbackgfromt60_ss(CSOUND *csound, CSN_NONARR_OP *p) {
    return csnarray_reqabsorption_ss_helper(csound, p, false, CSN_T602FBG);
}

int32_t csnarray_t60fromfeedbackg_hh(CSOUND *csound, CSN_REQABSOLEV_HH *p) {
    return csnarray_reqabsorption_helper(csound, &p->h, &p->k_data, p->source_handle_a, p->source_handle_b, &p->array, p->handle, NULL, NULL, &p->is_published, CSN_FBG2T60);
}

int32_t csnarray_t60fromfeedbackg_sh(CSOUND *csound, CSN_REQABSOLEV_SH *p) {
    return csnarray_reqabsorption_helper(csound, &p->h, &p->k_data, NULL, p->source_handle, &p->array, p->handle, p->arg_a, NULL, &p->is_published, CSN_FBG2T60);
}

int32_t csnarray_t60fromfeedbackg_hs(CSOUND *csound, CSN_REQABSOLEV_HS *p) {
    return csnarray_reqabsorption_helper(csound, &p->h, &p->k_data, p->source_handle, NULL, &p->array, p->handle, NULL, p->arg_b, &p->is_published, CSN_FBG2T60);
}

int32_t csnarray_t60fromfeedbackg_ss(CSOUND *csound, CSN_NONARR_OP *p) {
    return csnarray_reqabsorption_ss_helper(csound, p, false, CSN_FBG2T60);
}

int32_t csnarray_feedbackgfromt60_hh_k(CSOUND *csound, CSN_REQABSOLEV_HH *p) {
    return csnarray_reqabsorption_k_helper(csound, &p->h, &p->k_data, p->source_handle_a, p->source_handle_b, &p->array, p->handle, NULL, NULL, &p->is_published, p->trig, CSN_T602FBG);
}

int32_t csnarray_feedbackgfromt60_sh_k(CSOUND *csound, CSN_REQABSOLEV_SH *p) {
    return csnarray_reqabsorption_k_helper(csound, &p->h, &p->k_data, NULL, p->source_handle, &p->array, p->handle, p->arg_a, NULL, &p->is_published, p->trig, CSN_T602FBG);
}

int32_t csnarray_feedbackgfromt60_hs_k(CSOUND *csound, CSN_REQABSOLEV_HS *p) {
    return csnarray_reqabsorption_k_helper(csound, &p->h, &p->k_data, p->source_handle, NULL, &p->array, p->handle, NULL, p->arg_b, &p->is_published, p->trig, CSN_T602FBG);
}

int32_t csnarray_feedbackgfromt60_hh_k_init(CSOUND *csound, CSN_REQABSOLEV_HH *p) {
    return csnarray_reqabsorption_k_init_helper(csound, &p->h, &p->k_data, p->source_handle_a, p->source_handle_b, &p->array, p->handle, &p->is_published, CSN_T602FBG);
}

int32_t csnarray_feedbackgfromt60_sh_k_init(CSOUND *csound, CSN_REQABSOLEV_SH *p) {
    return csnarray_reqabsorption_k_init_helper(csound, &p->h, &p->k_data, NULL, p->source_handle, &p->array, p->handle, &p->is_published, CSN_T602FBG);
}

int32_t csnarray_feedbackgfromt60_hs_k_init(CSOUND *csound, CSN_REQABSOLEV_HS *p) {
    return csnarray_reqabsorption_k_init_helper(csound, &p->h, &p->k_data, p->source_handle, NULL, &p->array, p->handle, &p->is_published, CSN_T602FBG);
}

int32_t csnarray_feedbackgfromt60_ss_k(CSOUND *csound, CSN_NONARR_OP *p) {
    return csnarray_reqabsorption_ss_helper(csound, p, true, CSN_T602FBG);
}

int32_t csnarray_t60fromfeedbackg_hh_k_init(CSOUND *csound, CSN_REQABSOLEV_HH *p) {
    return csnarray_reqabsorption_k_init_helper(csound, &p->h, &p->k_data, p->source_handle_a, p->source_handle_b, &p->array, p->handle, &p->is_published, CSN_FBG2T60);
}

int32_t csnarray_t60fromfeedbackg_sh_k_init(CSOUND *csound, CSN_REQABSOLEV_SH *p) {
    return csnarray_reqabsorption_k_init_helper(csound, &p->h, &p->k_data, NULL, p->source_handle, &p->array, p->handle, &p->is_published, CSN_FBG2T60);
}

int32_t csnarray_t60fromfeedbackg_hs_k_init(CSOUND *csound, CSN_REQABSOLEV_HS *p) {
    return csnarray_reqabsorption_k_init_helper(csound, &p->h, &p->k_data, p->source_handle, NULL, &p->array, p->handle, &p->is_published, CSN_FBG2T60);
}

int32_t csnarray_t60fromfeedbackg_hh_k(CSOUND *csound, CSN_REQABSOLEV_HH *p) {
    return csnarray_reqabsorption_k_helper(csound, &p->h, &p->k_data, p->source_handle_a, p->source_handle_b, &p->array, p->handle, NULL, NULL, &p->is_published, p->trig, CSN_FBG2T60);
}

int32_t csnarray_t60fromfeedbackg_sh_k(CSOUND *csound, CSN_REQABSOLEV_SH *p) {
    return csnarray_reqabsorption_k_helper(csound, &p->h, &p->k_data, NULL, p->source_handle, &p->array, p->handle, p->arg_a, NULL, &p->is_published, p->trig, CSN_FBG2T60);
}

int32_t csnarray_t60fromfeedbackg_hs_k(CSOUND *csound, CSN_REQABSOLEV_HS *p) {
    return csnarray_reqabsorption_k_helper(csound, &p->h, &p->k_data, p->source_handle, NULL, &p->array, p->handle, NULL, p->arg_b, &p->is_published, p->trig, CSN_FBG2T60);
}

int32_t csnarray_t60fromfeedbackg_ss_k(CSOUND *csound, CSN_NONARR_OP *p) {
    return csnarray_reqabsorption_ss_helper(csound, p, true, CSN_FBG2T60);
}
