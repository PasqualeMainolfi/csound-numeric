#include "csnspecial.h"
#include "csnregistry.h"
#include "csnum.h"
#include "csnum_internal.h"
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define CSN_RAD_PER_DEG (CSN_PI / 180.0)

/* ---------------------------------------------------------------------------
   The functions themselves, with no registry in sight.
   ------------------------------------------------------------------------- */

/* The standard upward recurrence in n at fixed m, started from the diagonal:

     P_m^m     = (2m - 1)!! (1 - x^2)^(m/2)
     P_{m+1}^m = x (2m + 1) P_m^m
     P_n^m     = ((2n - 1) x P_{n-1}^m - (n + m - 1) P_{n-2}^m) / (n - m)

   The Condon-Shortley (-1)^m is left out of the diagonal on purpose: see
   csnspecial.h. It is the recurrence that is stable upwards in n, and the
   one every reference implementation uses. */
double csn_legendre_p(int32_t n, int32_t m, double x) {
    double pmm = 1.0;
    if (m > 0) {
        /* (1 - x)(1 + x) rather than 1 - x*x: near the poles the product keeps
           the digits the subtraction would cancel. */
        double s = sqrt((1.0 - x) * (1.0 + x));
        double odd = 1.0;
        for (int32_t i = 1; i <= m; i++) {
            pmm *= odd * s;
            odd += 2.0;
        }
    }
    if (n == m) return pmm;

    double pm1 = x * (2.0 * (double) m + 1.0) * pmm;
    if (n == m + 1) return pm1;

    double pn = 0.0;
    for (int32_t l = m + 2; l <= n; l++) {
        pn = ((2.0 * (double) l - 1.0) * x * pm1 - (double) (l + m - 1) * pmm) / (double) (l - m);
        pmm = pm1;
        pm1 = pn;
    }
    return pn;
}

/* The harmonics do not go through csn_legendre_p. They carry the SN3D
   (Schmidt semi-normalised) value

     S_n^m = sqrt((2 - delta_m) (n - m)! / (n + m)!) P_n^m

   through its own recurrence instead, so the factorial ratio is never formed:
   (2m - 1)!! overflows long before the product it is divided by would, while
   every S_n^m stays within [-1, 1]. Folding the factor into the recurrence of
   P gives

     S_1^1     = s
     S_m^m     = sqrt((2m - 1) / (2m)) s S_{m-1}^{m-1}              m >= 2
     S_{m+1}^m = sqrt(2m + 1) x S_m^m
     S_n^m     = ((2n - 1) x S_{n-1}^m - sqrt((n-1)^2 - m^2) S_{n-2}^m)
                 / sqrt(n^2 - m^2)

   The first line is separate because the (2 - delta_m) doubles between m = 0
   and m = 1 and is constant after that.

   x is sin(elevation) and s is cos(elevation), signed, and not
   sqrt(1 - x^2): an elevation past the pole describes the direction at
   azimuth + pi, and only the signed cosine makes s^m trig(m az) agree with it
   there, since (-1)^m from s^m cancels the (-1)^m from the shifted
   azimuth. */
static double sn3d_diagonal_step(int32_t m, double s, double previous) {
    if (m == 1) return s * previous;
    return previous * s * sqrt((2.0 * (double) m - 1.0) / (2.0 * (double) m));
}

static double sn3d_next(int32_t n, int32_t m, double x, double s_nm1, double s_nm2) {
    double dn = (double) n;
    double dm = (double) m;
    double a = (2.0 * dn - 1.0) * x * s_nm1;
    double b = sqrt((dn - 1.0 - dm) * (dn - 1.0 + dm)) * s_nm2;
    return (a - b) / sqrt((dn - dm) * (dn + dm));
}

double csn_sh_sn3d(int32_t n, int32_t m, double azimuth, double elevation) {
    int32_t am = m < 0 ? -m : m;
    double x = sin(elevation);
    double s = cos(elevation);

    double smm = 1.0;
    for (int32_t k = 1; k <= am; k++) {
        smm = sn3d_diagonal_step(k, s, smm);
    }

    double value = smm;
    if (n > am) {
        double s_nm2 = smm;
        double s_nm1 = sqrt(2.0 * (double) am + 1.0) * x * smm;
        for (int32_t l = am + 2; l <= n; l++) {
            double s_n = sn3d_next(l, am, x, s_nm1, s_nm2);
            s_nm2 = s_nm1;
            s_nm1 = s_n;
        }
        value = s_nm1;
    }

    double trig = m < 0 ? sin((double) am * azimuth) : cos((double) am * azimuth);
    return value * trig;
}

/* One column of fixed m at a time, walking n upwards from the diagonal, so
   the whole set costs O(order^2) and needs no table: each column only ever
   looks two values back, and the diagonal is carried from one column to the
   next. */
void csn_sh_sn3d_all(int32_t order, double azimuth, double elevation, double *out, size_t stride) {
    double x = sin(elevation);
    double s = cos(elevation);
    double smm = 1.0;

    for (int32_t m = 0; m <= order; m++) {
        if (m > 0) smm = sn3d_diagonal_step(m, s, smm);

        double cm = cos((double) m * azimuth);
        double sm = sin((double) m * azimuth);
        double s_nm2 = 0.0;
        double s_nm1 = 0.0;

        for (int32_t n = m; n <= order; n++) {
            double value;
            if (n == m) {
                value = smm;
            } else if (n == m + 1) {
                value = sqrt(2.0 * (double) m + 1.0) * x * smm;
            } else {
                value = sn3d_next(n, m, x, s_nm1, s_nm2);
            }
            s_nm2 = s_nm1;
            s_nm1 = value;

            size_t centre = (size_t) n * (size_t) n + (size_t) n;
            if (m == 0) {
                out[centre * stride] = value;
            } else {
                out[(centre + (size_t) m) * stride] = value * cm;
                out[(centre - (size_t) m) * stride] = value * sm;
            }
        }
    }
}

/* ---------------------------------------------------------------------------
   Argument checks shared by the opcodes.
   ------------------------------------------------------------------------- */

/* n and m name a function, so they are integers: a fractional degree is not a
   nearby function, it is a different one that this file does not compute.
   With signed_m the degree runs over [-n, n], as for the harmonics; without
   it over [0, n], as for P, whose negative orders carry a different factor
   and are left out rather than half supported. */
static int32_t read_degree_pair(CSOUND *csound, OPDS *perf_h, double n_val, double m_val, bool signed_m, int32_t *n_out, int32_t *m_out) {
    if (!IS_VALID_VALUE_INT32(n_val) || !IS_VALID_VALUE_INT32(m_val)) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] n and m must be integers");
    }
    if (n_val < 0.0 || n_val > (double) CSN_SPECIAL_MAX_DEGREE) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] n must lie in [0, %d], got %g", CSN_SPECIAL_MAX_DEGREE, n_val);
    }
    if (signed_m) {
        if (m_val < -n_val || m_val > n_val) {
            return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] m must lie in [-n, n], got n = %g, m = %g", n_val, m_val);
        }
    } else {
        if (m_val < 0.0 || m_val > n_val) {
            return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] m must lie in [0, n], got n = %g, m = %g: negative orders are not computed", n_val, m_val);
        }
    }
    *n_out = (int32_t) n_val;
    *m_out = (int32_t) m_val;
    return OK;
}

static int32_t read_order(CSOUND *csound, double order_val, int32_t *order_out) {
    if (!IS_VALID_VALUE_INT32(order_val) || order_val < 0.0 || order_val > (double) CSN_SPECIAL_MAX_DEGREE) {
        return csound->InitError(csound, "[csnarray] Order must be an integer in [0, %d], got %g", CSN_SPECIAL_MAX_DEGREE, order_val);
    }
    *order_out = (int32_t) order_val;
    return OK;
}

static int32_t read_angle_unit(CSOUND *csound, OPDS *perf_h, double flag, bool *in_degrees) {
    if (!IS_VALID_ZERO_ONE(flag)) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] Invalid angle unit: 0 selects radians, 1 degrees");
    }
    *in_degrees = flag != 0.0;
    return OK;
}

/* P is defined on [-1, 1] and nowhere else. A value outside is refused rather
   than clamped: it is a wrong argument, not a rounding error, and the
   harmonics never get here since they feed sin(elevation) themselves. The
   negated comparison also catches a NaN. */
static bool legendre_arg_is_valid(double x) {
    return fabs(x) <= 1.0;
}

static size_t sh_channels(int32_t order) {
    return ((size_t) order + 1U) * ((size_t) order + 1U);
}

/* ---------------------------------------------------------------------------
   csnlegendre, scalar
   ------------------------------------------------------------------------- */

static int32_t legendre_scalar_helper(CSOUND *csound, OPDS *perf_h, CSN_LEGENDRE *p) {
    int32_t n = 0, m = 0;
    int32_t res = read_degree_pair(csound, perf_h, (double) *p->n, (double) *p->m, false, &n, &m);
    if (res != OK) return res;

    double x = (double) *p->x;
    if (!legendre_arg_is_valid(x)) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] P_n^m(x) is defined for x in [-1, 1], got %g", x);
    }

    double value = csn_legendre_p(n, m, x);
    if (!isfinite(value)) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] P_%d^%d(%g) overflows the double range", n, m, x);
    }
    *p->value = (MYFLT) value;
    return OK;
}

int32_t csnspecial_legendre(CSOUND *csound, CSN_LEGENDRE *p) {
    return legendre_scalar_helper(csound, NULL, p);
}

int32_t csnspecial_legendre_k(CSOUND *csound, CSN_LEGENDRE *p) {
    return legendre_scalar_helper(csound, &p->h, p);
}

/* ---------------------------------------------------------------------------
   csnlegendre, elementwise over a handle
   ------------------------------------------------------------------------- */

/* Checked in full before anything is written, so a refused array leaves the
   output as the last good pass published it. */
static bool legendre_source_is_valid(const CSN_ARRAY *source, double *bad) {
    for (size_t i = 0; i < source->size; i++) {
        if (!legendre_arg_is_valid(source->data[i])) {
            *bad = source->data[i];
            return false;
        }
    }
    return true;
}

/* Reads each cell before writing the one at the same index, so it is safe in
   place. Returns false when a value overflowed, which only a large m can do. */
static bool legendre_fill(const CSN_ARRAY *source, CSN_ARRAY *dest, int32_t n, int32_t m) {
    bool finite = true;
    for (size_t i = 0; i < source->size; i++) {
        double value = csn_legendre_p(n, m, source->data[i]);
        if (!isfinite(value)) finite = false;
        dest->data[i] = value;
    }
    return finite;
}

static int32_t legendre_arr_helper(CSOUND *csound, CSN_LEGENDRE_ARR *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    /* n and m pick the function, so they are i-arguments in both overloads
       and are validated once, here. */
    int32_t res = read_degree_pair(csound, NULL, (double) *p->n, (double) *p->m, false, &p->degree_n, &p->degree_m);
    if (res != OK) return res;

    uint32_t source_handle = p->source_handle->id;
    const char *err = NULL;
    double bad = 0.0;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
        goto done;
    }
    CSN_ARRAY *source_arr = source_slot->array;

    if (source_arr->itype != CSN_REAL) {
        res = csound->InitError(csound, "[csnarray] This operation is not implemented for complex arrays");
        goto done;
    }
    if (!legendre_source_is_valid(source_arr, &bad)) {
        res = csound->InitError(csound, "[csnarray] P_n^m(x) is defined for x in [-1, 1], got %g", bad);
        goto done;
    }

    const uint32_t protect[1] = { source_handle };
    if (create_csnarray_locked(csound, reg, &p->h, source_arr->ndim, source_arr->shape, &p->array, p->handle, protect, 1U, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;
    set_csnarray_layout(arr, source_arr->ndim, source_arr->shape, source_arr->size, CSN_REAL);
    if (!legendre_fill(source_arr, arr, p->degree_n, p->degree_m)) {
        res = csound->InitError(csound, "[csnarray] P_%d^%d overflows the double range", p->degree_n, p->degree_m);
        goto done;
    }
    update_array_data_version(&arr->version);
    SET_KDATA_BEGIN(p, reg);
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, arr, (double) p->degree_n, (double) p->degree_m);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t legendre_arr_k_helper(CSOUND *csound, CSN_LEGENDRE_ARR *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    uint32_t owned_handle = p->k_data.owned_handle;
    CHECK_REG_HANDLE(csound, &p->h, reg, owned_handle);

    CHECK_KTRIG(p->trig);

    uint32_t source_handle = p->source_handle->id;
    int32_t res = OK;
    const char *err = NULL;
    double bad = 0.0;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
        goto done;
    }
    CSN_ARRAY *source_arr = source_slot->array;

    if (source_arr->itype != CSN_REAL) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] This operation is not implemented for complex arrays");
        goto done;
    }

    /* Cell-local: X = csnlegendre(n, m, X) reads every cell before writing
       it, so only a change of layout is refused. */
    res = CHECK_SELF_ALIAS_CELL_LOCAL(csound, &p->h, &p->k_data, source_handle, source_arr, source_arr->ndim, source_arr->shape, CSN_REAL);
    if (res != OK) goto done;

    CSN_SLOT *out_slot = get_slot(reg, owned_handle);
    if (out_slot != NULL && CAN_REUSE_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, out_slot->array, (double) p->degree_n, (double) p->degree_m)) {
        p->handle->id = owned_handle;
        goto done;
    }

    if (!legendre_source_is_valid(source_arr, &bad)) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] P_n^m(x) is defined for x in [-1, 1], got %g", bad);
        goto done;
    }

    size_t req_size = 0;
    if (get_array_size_from_shape(&req_size, source_arr->ndim, source_arr->shape) != OK) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
        goto done;
    }

    CSN_ARRAY *arr = NULL;
    size_t logical_size = source_arr->size == 0 ? 0 : req_size;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, source_arr->ndim, source_arr->shape, logical_size, CSN_REAL, err);
    if (res != OK) goto done;
    p->array = arr;

    if (!legendre_fill(source_arr, arr, p->degree_n, p->degree_m)) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] P_%d^%d overflows the double range", p->degree_n, p->degree_m);
        goto done;
    }

    SET_KDATA_END(p, source_arr->shape, source_arr->ndim, CSN_REAL);
    PUBLISH_ELEMENTWISE(&p->k_data, source_handle, source_arr, 0, NULL, arr, (double) p->degree_n, (double) p->degree_m);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnspecial_legendre_arr(CSOUND *csound, CSN_LEGENDRE_ARR *p) {
    return legendre_arr_helper(csound, p);
}

int32_t csnspecial_legendre_arr_k(CSOUND *csound, CSN_LEGENDRE_ARR *p) {
    return legendre_arr_k_helper(csound, p);
}

int32_t csnspecial_legendre_arr_deinit(CSOUND *csound, CSN_LEGENDRE_ARR *p) {
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

/* ---------------------------------------------------------------------------
   csnsphharm, one harmonic at one direction
   ------------------------------------------------------------------------- */

static int32_t sphharm_scalar_helper(CSOUND *csound, OPDS *perf_h, CSN_SPHHARM *p) {
    int32_t n = 0, m = 0;
    int32_t res = read_degree_pair(csound, perf_h, (double) *p->n, (double) *p->m, true, &n, &m);
    if (res != OK) return res;

    bool in_degrees = false;
    res = read_angle_unit(csound, perf_h, (double) *p->degree, &in_degrees);
    if (res != OK) return res;

    double azimuth = (double) *p->azimuth;
    double elevation = (double) *p->elevation;
    if (in_degrees) {
        azimuth *= CSN_RAD_PER_DEG;
        elevation *= CSN_RAD_PER_DEG;
    }

    *p->value = (MYFLT) csn_sh_sn3d(n, m, azimuth, elevation);
    return OK;
}

int32_t csnspecial_sphharm(CSOUND *csound, CSN_SPHHARM *p) {
    return sphharm_scalar_helper(csound, NULL, p);
}

int32_t csnspecial_sphharm_k(CSOUND *csound, CSN_SPHHARM *p) {
    return sphharm_scalar_helper(csound, &p->h, p);
}

/* ---------------------------------------------------------------------------
   csnsphharmacn, the whole set at one direction
   ------------------------------------------------------------------------- */

static int32_t sphharm_acn_helper(CSOUND *csound, CSN_SPHHARM_ACN *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    /* The order fixes the length of the output and the unit how the angles
       read: both are i-arguments in both overloads, validated once here. */
    int32_t res = read_order(csound, (double) *p->order, &p->order_value);
    if (res != OK) return res;
    res = read_angle_unit(csound, NULL, (double) *p->degree, &p->is_degree);
    if (res != OK) return res;

    double azimuth = (double) *p->azimuth;
    double elevation = (double) *p->elevation;
    if (p->is_degree) {
        azimuth *= CSN_RAD_PER_DEG;
        elevation *= CSN_RAD_PER_DEG;
    }

    const char *err = NULL;
    uint32_t new_ndim = 1U;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    new_shape[0] = (uint32_t) sh_channels(p->order_value);

    csound->LockMutex(reg->mutex);
    /* Nothing to protect: every input is a scalar. */
    if (create_csnarray_locked(csound, reg, &p->h, new_ndim, new_shape, &p->array, p->handle, NULL, 0U, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    /* Filled on both passes, as csnrotmat does: in the k-rate form the angles
       hold whatever the init chain wrote into them, and the set at that
       direction is still a proper set for a consumer's own init to read. */
    csn_sh_sn3d_all(p->order_value, azimuth, elevation, p->array->data, 1U);
    update_array_data_version(&p->array->version);
    SET_KDATA_BEGIN(p, reg);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t sphharm_acn_k_helper(CSOUND *csound, CSN_SPHHARM_ACN *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REG_HANDLE(csound, &p->h, reg, p->k_data.owned_handle);

    CHECK_KTRIG(p->trig);

    int32_t res = OK;
    const char *err = NULL;

    double azimuth = (double) *p->azimuth;
    double elevation = (double) *p->elevation;
    if (p->is_degree) {
        azimuth *= CSN_RAD_PER_DEG;
        elevation *= CSN_RAD_PER_DEG;
    }

    uint32_t new_ndim = 1U;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    new_shape[0] = (uint32_t) sh_channels(p->order_value);

    csound->LockMutex(reg->mutex);

    /* The length never changes, but the slot is resolved again for the same
       reasons as csnrotmat: the handle may have been freed, or an in-place
       opcode may have rewritten its layout since the last pass. */
    CSN_ARRAY *array = NULL;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &array, &p->k_data, NULL, new_ndim, new_shape, (size_t) new_shape[0], CSN_REAL, err);
    if (res != OK) goto done;
    p->array = array;

    csn_sh_sn3d_all(p->order_value, azimuth, elevation, array->data, 1U);

    SET_KDATA_END(p, new_shape, new_ndim, CSN_REAL);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnspecial_sphharm_acn(CSOUND *csound, CSN_SPHHARM_ACN *p) {
    return sphharm_acn_helper(csound, p);
}

int32_t csnspecial_sphharm_acn_k(CSOUND *csound, CSN_SPHHARM_ACN *p) {
    return sphharm_acn_k_helper(csound, p);
}

int32_t csnspecial_sphharm_acn_deinit(CSOUND *csound, CSN_SPHHARM_ACN *p) {
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

/* ---------------------------------------------------------------------------
   csnsphharmacn, the whole set at many directions
   ------------------------------------------------------------------------- */

/* Resolves the two direction vectors. They must be real, one-dimensional and
   of the same non-zero length D; the result is then the (channels, D) matrix
   with one direction per column, the layout csnmatmul expects for column
   vectors and the one a mode-matching decoder is the pseudo-inverse of. */
static int32_t sphharm_mat_sources(CSOUND *csound, OPDS *perf_h, CSN_REGISTRY *reg, uint32_t az_handle, uint32_t el_handle, CSN_ARRAY **az_out, CSN_ARRAY **el_out) {
    CSN_SLOT *az_slot = get_slot(reg, az_handle);
    if (az_slot == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", az_handle);
    }
    CSN_SLOT *el_slot = get_slot(reg, el_handle);
    if (el_slot == NULL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", el_handle);
    }

    CSN_ARRAY *az = az_slot->array;
    CSN_ARRAY *el = el_slot->array;
    if (az->itype != CSN_REAL || el->itype != CSN_REAL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Azimuths and elevations must be real arrays");
    }
    if (az->ndim != 1U || el->ndim != 1U) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Azimuths and elevations must be 1-D, one value per direction");
    }
    if (az->size == 0 || az->size != az->shape[0] || el->size != el->shape[0]) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Azimuths and elevations must hold at least one direction");
    }
    if (az->size != el->size) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] %zu azimuths but %zu elevations: one of each per direction", az->size, el->size);
    }

    *az_out = az;
    *el_out = el;
    return OK;
}

static void sphharm_mat_fill(double *out, int32_t order, const CSN_ARRAY *az, const CSN_ARRAY *el, bool in_degrees) {
    size_t directions = az->size;
    double scale = in_degrees ? CSN_RAD_PER_DEG : 1.0;
    for (size_t d = 0; d < directions; d++) {
        csn_sh_sn3d_all(order, az->data[d] * scale, el->data[d] * scale, out + d, directions);
    }
}

static int32_t sphharm_mat_helper(CSOUND *csound, CSN_SPHHARM_MAT *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = read_order(csound, (double) *p->order, &p->order_value);
    if (res != OK) return res;
    res = read_angle_unit(csound, NULL, (double) *p->degree, &p->is_degree);
    if (res != OK) return res;

    uint32_t az_handle = p->azimuth_handle->id;
    uint32_t el_handle = p->elevation_handle->id;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *az = NULL;
    CSN_ARRAY *el = NULL;
    res = sphharm_mat_sources(csound, NULL, reg, az_handle, el_handle, &az, &el);
    if (res != OK) goto done;

    uint32_t new_ndim = 2U;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    new_shape[0] = (uint32_t) sh_channels(p->order_value);
    new_shape[1] = (uint32_t) az->size;

    const uint32_t protect[2] = { az_handle, el_handle };
    if (create_csnarray_locked(csound, reg, &p->h, new_ndim, new_shape, &p->array, p->handle, protect, 2U, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    sphharm_mat_fill(p->array->data, p->order_value, az, el, p->is_degree);
    update_array_data_version(&p->array->version);
    SET_KDATA_BEGIN(p, reg);
    PUBLISH_ELEMENTWISE(&p->k_data, az_handle, az, el_handle, el, p->array, (double) p->order_value, p->is_degree ? 1.0 : 0.0);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t sphharm_mat_k_helper(CSOUND *csound, CSN_SPHHARM_MAT *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    uint32_t owned_handle = p->k_data.owned_handle;
    CHECK_REG_HANDLE(csound, &p->h, reg, owned_handle);

    CHECK_KTRIG(p->trig);

    uint32_t az_handle = p->azimuth_handle->id;
    uint32_t el_handle = p->elevation_handle->id;
    const char *err = NULL;

    /* Every output cell reads a whole column of directions, and the output is
       never the shape of its inputs, so no aliasing is allowed at all. */
    int32_t res = CHECK_SELF_ALIAS(csound, &p->h, &p->k_data, az_handle, el_handle);
    if (res != OK) return res;

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *az = NULL;
    CSN_ARRAY *el = NULL;
    res = sphharm_mat_sources(csound, &p->h, reg, az_handle, el_handle, &az, &el);
    if (res != OK) goto done;

    CSN_SLOT *out_slot = get_slot(reg, owned_handle);
    if (out_slot != NULL && CAN_REUSE_ELEMENTWISE(&p->k_data, az_handle, az, el_handle, el, out_slot->array, (double) p->order_value, p->is_degree ? 1.0 : 0.0)) {
        p->handle->id = owned_handle;
        goto done;
    }

    uint32_t new_ndim = 2U;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    new_shape[0] = (uint32_t) sh_channels(p->order_value);
    new_shape[1] = (uint32_t) az->size;

    CSN_ARRAY *arr = NULL;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, new_ndim, new_shape, (size_t) new_shape[0] * (size_t) new_shape[1], CSN_REAL, err);
    if (res != OK) goto done;
    p->array = arr;

    sphharm_mat_fill(arr->data, p->order_value, az, el, p->is_degree);

    SET_KDATA_END(p, new_shape, new_ndim, CSN_REAL);
    PUBLISH_ELEMENTWISE(&p->k_data, az_handle, az, el_handle, el, arr, (double) p->order_value, p->is_degree ? 1.0 : 0.0);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnspecial_sphharm_mat(CSOUND *csound, CSN_SPHHARM_MAT *p) {
    return sphharm_mat_helper(csound, p);
}

int32_t csnspecial_sphharm_mat_k(CSOUND *csound, CSN_SPHHARM_MAT *p) {
    return sphharm_mat_k_helper(csound, p);
}

int32_t csnspecial_sphharm_mat_deinit(CSOUND *csound, CSN_SPHHARM_MAT *p) {
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}
