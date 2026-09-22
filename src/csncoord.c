#include "csncoord.h"
#include "csnregistry.h"
#include "csnum.h"
#include "csnum_internal.h"
#include <math.h>
#include <stddef.h>
#include <stdint.h>

#define CSN_DEG_PER_RAD (180.0 / CSN_PI)
#define CSN_RAD_PER_DEG (CSN_PI / 180.0)

static bool coord_is_to_cartesian(CSN_COORDS_MODE mode) {
    return mode == CSN_POL2CAR || mode == CSN_CYL2CAR || mode == CSN_SPH2CAR ||
           mode == CSN_HOA2CAR;
}

/* Which of the two trailing outputs carry an angle, so the degrees flag knows
   what to convert. The planar and cylindrical pairs carry one, the spherical
   and ambisonics pairs carry two: the inclination or the elevation joins the
   azimuth. */
static bool coord_has_two_angles(CSN_COORDS_MODE mode) {
    return mode == CSN_SPH2CAR || mode == CSN_CAR2SPH ||
           mode == CSN_HOA2CAR || mode == CSN_CAR2HOA;
}

/* z/r can leave [-1, 1] by an ulp once r has been rounded, and acos answers
   NaN there rather than the pole the caller clearly meant. */
static double coord_clamp_unit(double value) {
    if (value > 1.0) return 1.0;
    if (value < -1.0) return -1.0;
    return value;
}

static int32_t VALIDATE_ANGLE(CSOUND *csound, OPDS *perf_h, double flag, bool *in_degrees) {
    if (!IS_VALID_ZERO_ONE(flag)) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] Invalid angle unit: 0 selects radians, 1 degrees");
    }
    *in_degrees = flag != 0.0;
    return OK;
}

/* a, b and c are the three components in the order the opcode's own system
   lists them; c is unused by the planar pair. The degrees flag applies to
   whichever side of the conversion carries angles, so a round trip taken in
   degrees comes back in degrees. */
static int32_t coord_helper(CSOUND *csound, OPDS *perf_h, MYFLT *a_out, MYFLT *b_out, MYFLT *c_out, const MYFLT *a_in, const MYFLT *b_in, const MYFLT *c_in, const MYFLT *degrees, CSN_COORDS_MODE mode) {
    double ain = (double) *a_in;
    double bin = (double) *b_in;
    double cin = c_in == NULL ? 0.0 : (double) *c_in;
    double aout = 0.0;
    double bout = 0.0;
    double cout = 0.0;

    bool in_degrees = false;
    if (degrees != NULL) {
        int32_t res = VALIDATE_ANGLE(csound, perf_h, (double) *degrees, &in_degrees);
        if (res != OK) return res;
    }

    if (in_degrees && coord_is_to_cartesian(mode)) {
        bin *= CSN_RAD_PER_DEG;
        if (coord_has_two_angles(mode)) cin *= CSN_RAD_PER_DEG;
    }

    switch (mode) {
        case CSN_POL2CAR:
            aout = ain * cos(bin);
            bout = ain * sin(bin);
            break;
        case CSN_CAR2POL:
            aout = hypot(ain, bin);
            bout = atan2(bin, ain);
            break;
        case CSN_CYL2CAR:
            aout = ain * cos(bin);
            bout = ain * sin(bin);
            cout = cin;
            break;
        case CSN_CAR2CYL:
            aout = hypot(ain, bin);
            bout = atan2(bin, ain);
            cout = cin;
            break;
        case CSN_SPH2CAR:
            aout = ain * sin(bin) * cos(cin);
            bout = ain * sin(bin) * sin(cin);
            cout = ain * cos(bin);
            break;
        case CSN_CAR2SPH:
            /* hypot twice rather than sqrt of a sum of squares: the sum
               overflows for components the magnitude itself can hold. */
            aout = hypot(hypot(ain, bin), cin);
            if (aout == 0.0) {
                return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] The origin has no direction: the angles are undefined for r = 0");
            }
            bout = acos(coord_clamp_unit(cin / aout));
            cout = atan2(bin, ain);
            break;
        /* The ambisonics pair keeps the AmbiX frame: +x front, +y left,
           +z up, with the azimuth measured from +x towards +y and the
           elevation from the horizon rather than from the zenith. That last
           point is the whole reason this pair exists next to the spherical
           one, whose theta is the inclination from +z: the two differ by
           el = pi/2 - theta, and getting it wrong mirrors a source about the
           horizontal plane without raising anything. */
        case CSN_HOA2CAR:
            aout = ain * cos(cin) * cos(bin);
            bout = ain * cos(cin) * sin(bin);
            cout = ain * sin(cin);
            break;
        case CSN_CAR2HOA:
            /* hypot twice, as in CAR2SPH, for the same overflow reason. */
            aout = hypot(hypot(ain, bin), cin);
            if (aout == 0.0) {
                return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] The origin has no direction: the angles are undefined for d = 0");
            }
            bout = atan2(bin, ain);
            cout = atan2(cin, hypot(ain, bin));
            break;
    }

    if (in_degrees && !coord_is_to_cartesian(mode)) {
        bout *= CSN_DEG_PER_RAD;
        if (coord_has_two_angles(mode)) cout *= CSN_DEG_PER_RAD;
    }

    *a_out = (MYFLT) aout;
    *b_out = (MYFLT) bout;
    if (c_out != NULL) *c_out = (MYFLT) cout;

    return OK;
}

static int32_t coord2(CSOUND *csound, OPDS *perf_h, CSN_COORD2 *p, CSN_COORDS_MODE mode) {
    return coord_helper(csound, perf_h, p->a_out, p->b_out, NULL, p->a_in, p->b_in, NULL, p->degrees, mode);
}

static int32_t coord3(CSOUND *csound, OPDS *perf_h, CSN_COORD3 *p, CSN_COORDS_MODE mode) {
    return coord_helper(csound, perf_h, p->a_out, p->b_out, p->c_out, p->a_in, p->b_in, p->c_in, p->degrees, mode);
}

int32_t csncoord_poltocar(CSOUND *csound, CSN_COORD2 *p)   {
    return coord2(csound, NULL, p, CSN_POL2CAR);
}

int32_t csncoord_cartopol(CSOUND *csound, CSN_COORD2 *p)   {
    return coord2(csound, NULL, p, CSN_CAR2POL);
}

int32_t csncoord_cyltocar(CSOUND *csound, CSN_COORD3 *p)   {
    return coord3(csound, NULL, p, CSN_CYL2CAR);
}

int32_t csncoord_cartocyl(CSOUND *csound, CSN_COORD3 *p)   {
    return coord3(csound, NULL, p, CSN_CAR2CYL);
}

int32_t csncoord_sphtocar(CSOUND *csound, CSN_COORD3 *p)   {
    return coord3(csound, NULL, p, CSN_SPH2CAR);
}

int32_t csncoord_cartosph(CSOUND *csound, CSN_COORD3 *p)   {
    return coord3(csound, NULL, p, CSN_CAR2SPH);
}

int32_t csncoord_cartohoa(CSOUND *csound, CSN_COORD3 *p) {
    return coord3(csound, NULL, p, CSN_CAR2HOA);
}

int32_t csncoord_hoatocar(CSOUND *csound, CSN_COORD3 *p) {
    return coord3(csound, NULL, p, CSN_HOA2CAR);
}

int32_t csncoord_poltocar_k(CSOUND *csound, CSN_COORD2 *p) {
    return coord2(csound, &p->h, p, CSN_POL2CAR);
}

int32_t csncoord_cartopol_k(CSOUND *csound, CSN_COORD2 *p) {
    return coord2(csound, &p->h, p, CSN_CAR2POL);
}

int32_t csncoord_cyltocar_k(CSOUND *csound, CSN_COORD3 *p) {
    return coord3(csound, &p->h, p, CSN_CYL2CAR);
}

int32_t csncoord_cartocyl_k(CSOUND *csound, CSN_COORD3 *p) {
    return coord3(csound, &p->h, p, CSN_CAR2CYL);
}

int32_t csncoord_sphtocar_k(CSOUND *csound, CSN_COORD3 *p) {
    return coord3(csound, &p->h, p, CSN_SPH2CAR);
}

int32_t csncoord_cartosph_k(CSOUND *csound, CSN_COORD3 *p) {
    return coord3(csound, &p->h, p, CSN_CAR2SPH);
}

int32_t csncoord_cartohoa_k(CSOUND *csound, CSN_COORD3 *p) {
    return coord3(csound, &p->h, p, CSN_CAR2HOA);
}

int32_t csncoord_hoatocar_k(CSOUND *csound, CSN_COORD3 *p) {
    return coord3(csound, &p->h, p, CSN_HOA2CAR);
}

int32_t csncoord_degtorad(CSOUND *csound, CSN_COORD1 *p) {
    (void) csound;
    *p->value = (MYFLT) ((double) *p->arg_a * CSN_RAD_PER_DEG);
    return OK;
}

int32_t csncoord_radtodeg(CSOUND *csound, CSN_COORD1 *p) {
    (void) csound;
    *p->value = (MYFLT) ((double) *p->arg_a * CSN_DEG_PER_RAD);
    return OK;
}

static void ROTATE_MAT(double *mat, double angle, uint32_t axis) {
    double c = cos(angle);
    double s = sin(angle);
    mat[0] = axis == 0 ? 1.0 : c;
    mat[1] = axis == 2 ? -s  : 0.0;
    mat[2] = axis == 1 ?  s  : 0.0;
    mat[3] = axis == 2 ?  s  : 0.0;
    mat[4] = axis == 1 ? 1.0 : c;
    mat[5] = axis == 0 ? -s  : 0.0;
    mat[6] = axis == 1 ? -s  : 0.0;
    mat[7] = axis == 0 ?  s  : 0.0;
    mat[8] = axis == 2 ? 1.0 : c;
}

/* Rodrigues needs a unit axis: a direction of any other length would give a
   matrix that is not a rotation, and nothing downstream would say so. The
   normalisation happens here rather than being demanded of the caller, so a
   direction that arrives straight from csncross works. The null vector is the
   one case that cannot be rescued: it names no axis, and normalising it would
   divide by zero. */
/* Checked before the output array is touched, not inside the fill: a refusal
   that came after the slot update would leave the version advanced on a
   matrix nobody rewrote, telling every consumer downstream that it changed. */
static bool ROT_AXIS_IS_VALID(const double *dir) {
    return sqrt(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]) > 0.0;
}

static int32_t ROTATE_MAT_AXIS(double *mat, const double *dir, double angle) {
    double len = sqrt(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
    if (!(len > 0.0)) return NOTOK;

    double c = cos(angle);
    double s = sin(angle);
    double f = 1.0 - c;
    double x = dir[0] / len;
    double y = dir[1] / len;
    double z = dir[2] / len;
    double xx = x * x;
    double yy = y * y;
    double zz = z * z;
    mat[0] = c + xx * f;
    mat[1] = x * y * f - z * s;
    mat[2] = x * z * f + y * s;
    mat[3] = y * x * f + z * s;
    mat[4] = c + yy * f;
    mat[5] = y * z * f - x * s;
    mat[6] = z * x * f - y * s;
    mat[7] = z * y * f + x * s;
    mat[8] = c + zz * f;
    return OK;
}

static int32_t MAT_YPR(double *mat, double angle, CSN_YPR_MODE mode) {
    double c = cos(angle);
    double s = sin(angle);
    switch (mode) {
        case CSN_MATYAW:
            mat[0] = c;
            mat[1] = -s;
            mat[2] = 0.0;
            mat[3] = s;
            mat[4] = c;
            mat[5] = 0.0;
            mat[6] = 0.0;
            mat[7] = 0.0;
            mat[8] = 1.0;
            break;
        case CSN_MATPITCH:
            mat[0] = c;
            mat[1] = 0.0;
            mat[2] = -s;
            mat[3] = 0.0;
            mat[4] = 1.0;
            mat[5] = 0.0;
            mat[6] = s;
            mat[7] = 0.0;
            mat[8] = c;
            break;
        case CSN_MATROLL:
            mat[0] = 1.0;
            mat[1] = 0.0;
            mat[2] = 0.0;
            mat[3] = 0.0;
            mat[4] = c;
            mat[5] = -s;
            mat[6] = 0.0;
            mat[7] = s;
            mat[8] = c;
            break;
    }
    return OK;
}

static int32_t csncoord_rotmat_helper(CSOUND *csound, CSN_ROTMAT *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    const char *err = NULL;

    p->is_degree = false;
    double flag = (double) *p->degree;
    res = VALIDATE_ANGLE(csound, NULL, flag, &p->is_degree);
    if (res != OK) return res;

    /* The axis picks which matrix is built, not what it is built from, so it
       is an i-argument in both overloads and is validated once, here. It is
       not a data axis: a negative value does not count from the end. */
    double axis = (double) *p->axis;
    if (axis != 0.0 && axis != 1.0 && axis != 2.0) {
        return csound->InitError(csound, "[csnarray] Invalid rotation axis %g: 0 selects x, 1 y, 2 z", axis);
    }
    p->axis_index = (uint32_t) axis;

    double angle = (double) *p->angle;
    if (p->is_degree) angle *= CSN_RAD_PER_DEG;

    /* A rotation matrix is 3 x 3: two dimensions, nine elements. */
    uint32_t new_ndim = 2U;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    new_shape[0] = 3U;
    new_shape[1] = 3U;

    csound->LockMutex(reg->mutex);
    /* Nothing to protect: the angle and the axis are scalars, so this call
       cannot free an array it is about to read. */
    if (create_csnarray_locked(csound, reg, &p->h, new_ndim, new_shape, &p->array, p->handle, NULL, 0U, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    /* Filled on both passes, as csnidentity does with its own k-rate size.

       In the k-rate form the angle is read at whatever the init chain has
       already written into it, which is 0 unless an earlier line set it with
       init: a k-rate assignment does not write at i-time. So the matrix that
       reaches the first control period is normally the identity, which is the
       right answer for a rotation nobody has set yet, and is still a proper
       rotation for a consumer to multiply by. Publishing an empty array
       instead would make every consumer's own init pass fail on a handle that
       carries no elements. */
    ROTATE_MAT(p->array->data, angle, p->axis_index);

    p->k_data.prev_ndim = new_ndim;
    memcpy(p->k_data.prev_shape, p->array->shape, sizeof(uint32_t) * CSN_MAX_DIMS);
    p->k_data.prev_itype = CSN_REAL;
    p->k_data.owned_handle = p->handle->id;
    p->k_data.registry = reg;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csncoord_rotmat_k_helper(CSOUND *csound, CSN_ROTMAT *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REG_HANDLE(csound, &p->h, reg, p->k_data.owned_handle);

    CHECK_KTRIG(p->trig);

    int32_t res = OK;
    const char *err = NULL;

    /* Validated by the init: the axis and the angle unit are i-arguments and
       cannot have moved since. */
    double angle = (double) *p->angle;
    if (p->is_degree) angle *= CSN_RAD_PER_DEG;

    uint32_t new_ndim = 2U;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    new_shape[0] = 3U;
    new_shape[1] = 3U;

    csound->LockMutex(reg->mutex);

    /* The shape never changes, but the slot still has to be resolved again:
       the handle may have been freed, and an in-place opcode may have
       rewritten this array's layout since the last pass. The version bump
       that closes the call is what makes a consumer downstream recompute. */
    CSN_ARRAY *array = NULL;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &array, &p->k_data, NULL, new_ndim, new_shape, 9U, CSN_REAL, err);
    if (res != OK) goto done;
    p->array = array;

    ROTATE_MAT(array->data, angle, p->axis_index);

    SET_KDATA_END(p, new_shape, new_ndim, CSN_REAL);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csncoord_rotmat_deinit(CSOUND *csound, CSN_ROTMAT *p) {
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

int32_t csncoord_rotmat(CSOUND *csound, CSN_ROTMAT *p) {
    return csncoord_rotmat_helper(csound, p);
}

int32_t csncoord_rotmat_k_init(CSOUND *csound, CSN_ROTMAT *p) {
    return csncoord_rotmat_helper(csound, p);
}

int32_t csncoord_rotmat_k(CSOUND *csound, CSN_ROTMAT *p) {
    return csncoord_rotmat_k_helper(csound, p);
}

static int32_t csncoord_rotmat_axis_helper(CSOUND *csound, CSN_ROTMAT_AXIS *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    const char *err = NULL;

    p->is_degree = false;
    double flag = (double) *p->degree;
    res = VALIDATE_ANGLE(csound, NULL, flag, &p->is_degree);
    if (res != OK) return res;

    double angle = (double) *p->angle;
    if (p->is_degree) angle *= CSN_RAD_PER_DEG;

    uint32_t new_ndim = 2U;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    new_shape[0] = 3U;
    new_shape[1] = 3U;

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
        goto done;
    }
    CSN_ARRAY *source_arr = slot->array;
    if (source_arr->ndim != 1U || source_arr->shape[0] != 3U || source_arr->itype != CSN_REAL) {
        res = csound->InitError(csound, "[csnarray] Direction must be 1D real vector of size 3 (x, y, z)");
        goto done;
    }
    if (!ROT_AXIS_IS_VALID(source_arr->data)) {
        res = csound->InitError(csound, "[csnarray] Direction must not be the null vector: it names no axis");
        goto done;
    }

    if (create_csnarray_locked(csound, reg, &p->h, new_ndim, new_shape, &p->array, p->handle, &source_handle, 1U, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    /* Filled on both passes, for the same reason as csnrotmat: the direction
       is a handle and already holds its values here, the angle is 0 unless an
       earlier init set it, and a rotation of 0 about any axis is the
       identity. */
    if (ROTATE_MAT_AXIS(p->array->data, source_arr->data, angle) != OK) {
        res = csound->InitError(csound, "[csnarray] Direction must not be the null vector: it names no axis");
        goto done;
    }

    p->k_data.prev_ndim = new_ndim;
    memcpy(p->k_data.prev_shape, p->array->shape, sizeof(uint32_t) * CSN_MAX_DIMS);
    p->k_data.prev_itype = CSN_REAL;
    p->k_data.owned_handle = p->handle->id;
    p->k_data.registry = reg;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csncoord_rotmat_axis_k_helper(CSOUND *csound, CSN_ROTMAT_AXIS *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REG_HANDLE(csound, &p->h, reg, p->k_data.owned_handle);

    CHECK_KTRIG(p->trig);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    const char *err = NULL;

    double angle = (double) *p->angle;
    if (p->is_degree) angle *= CSN_RAD_PER_DEG;

    uint32_t new_ndim = 2U;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    new_shape[0] = 3U;
    new_shape[1] = 3U;

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = slot->array;
    if (source_arr->ndim != 1U || source_arr->shape[0] != 3U || source_arr->itype != CSN_REAL) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Direction must be 1D real vector of size 3 (x, y, z)");
        goto done;
    }
    if (!ROT_AXIS_IS_VALID(source_arr->data)) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Direction must not be the null vector: it names no axis");
        goto done;
    }

    /* The direction is re-read every pass, so it may have changed; the output
       slot is re-resolved for the same reasons as csnrotmat. */
    CSN_ARRAY *array = NULL;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &array, &p->k_data, NULL, new_ndim, new_shape, 9U, CSN_REAL, err);
    if (res != OK) goto done;
    p->array = array;

    (void) ROTATE_MAT_AXIS(array->data, source_arr->data, angle);

    SET_KDATA_END(p, new_shape, new_ndim, CSN_REAL);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csncoord_rotmat_axis_deinit(CSOUND *csound, CSN_ROTMAT_AXIS *p) {
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

int32_t csncoord_rotmat_axis(CSOUND *csound, CSN_ROTMAT_AXIS *p) {
    return csncoord_rotmat_axis_helper(csound, p);
}

int32_t csncoord_rotmat_axis_k_init(CSOUND *csound, CSN_ROTMAT_AXIS *p) {
    return csncoord_rotmat_axis_helper(csound, p);
}

int32_t csncoord_rotmat_axis_k(CSOUND *csound, CSN_ROTMAT_AXIS *p) {
    return csncoord_rotmat_axis_k_helper(csound, p);
}

/* Row-major 3 x 3 product, out = a * b. out must not alias either operand. */
static void MAT3_MUL(double *out, const double *a, const double *b) {
    for (int32_t i = 0; i < 3; i++) {
        for (int32_t j = 0; j < 3; j++) {
            out[3 * i + j] = a[3 * i] * b[j] + a[3 * i + 1] * b[3 + j] + a[3 * i + 2] * b[6 + j];
        }
    }
}

/* The composed yaw-pitch-roll matrix, in the audio convention:

     R = Rz(yaw) * Ry(-pitch) * Rx(roll)

   applied right to left, so a source is rolled first, then pitched, then
   yawed. The order is the whole point of handing back one matrix instead of
   three: a different order is a different rotation, and composing the three
   by hand is exactly where it goes wrong.

   The pitch is positive from the front upwards, which is a rotation about -y
   and not the right-handed Ry that csnrotmat builds. That sign is the reason
   this opcode carries its own name rather than being a mode of csnrotmat: a
   general constructor must not quietly hold an audio convention. */
static void MAT_YPR_COMPOSED(double *dest, double angle_y, double angle_p, double angle_r) {
    double m_yaw[9], m_pitch[9], m_roll[9], tmp[9];
    MAT_YPR(m_yaw, angle_y, CSN_MATYAW);
    MAT_YPR(m_pitch, angle_p, CSN_MATPITCH);
    MAT_YPR(m_roll, angle_r, CSN_MATROLL);
    MAT3_MUL(tmp, m_yaw, m_pitch);
    MAT3_MUL(dest, tmp, m_roll);
}

static int32_t ypr_angles(CSOUND *csound, CSN_ROTMAT_YPR *p, bool validate, double *out_y, double *out_p, double *out_r) {
    if (validate) {
        p->is_degree = false;
        int32_t res = VALIDATE_ANGLE(csound, NULL, (double) *p->degree, &p->is_degree);
        if (res != OK) return res;
    }

    *out_y = (double) *p->yaw;
    *out_p = (double) *p->pitch;
    *out_r = (double) *p->roll;
    if (p->is_degree) {
        *out_y *= CSN_RAD_PER_DEG;
        *out_p *= CSN_RAD_PER_DEG;
        *out_r *= CSN_RAD_PER_DEG;
    }
    return OK;
}

static int32_t csncoord_rotmat_ypr_helper(CSOUND *csound, CSN_ROTMAT_YPR *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    const char *err = NULL;

    double angle_y = 0.0, angle_p = 0.0, angle_r = 0.0;
    res = ypr_angles(csound, p, true, &angle_y, &angle_p, &angle_r);
    if (res != OK) return res;

    uint32_t new_ndim = 2U;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    new_shape[0] = 3U;
    new_shape[1] = 3U;

    csound->LockMutex(reg->mutex);
    /* Nothing to protect: the three angles are scalars. */
    if (create_csnarray_locked(csound, reg, &p->h, new_ndim, new_shape, &p->array, p->handle, NULL, 0U, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    /* Filled on both passes, as csnrotmat does. In the k-rate form the angles
       are read at whatever the init chain has already written into them, which
       is 0 unless an earlier line set them with init, so the matrix that
       reaches the first control period is normally the identity. */
    MAT_YPR_COMPOSED(p->array->data, angle_y, angle_p, angle_r);

    p->k_data.prev_ndim = new_ndim;
    memcpy(p->k_data.prev_shape, p->array->shape, sizeof(uint32_t) * CSN_MAX_DIMS);
    p->k_data.prev_itype = CSN_REAL;
    p->k_data.owned_handle = p->handle->id;
    p->k_data.registry = reg;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t csncoord_rotmat_ypr_k_helper(CSOUND *csound, CSN_ROTMAT_YPR *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REG_HANDLE(csound, &p->h, reg, p->k_data.owned_handle);

    CHECK_KTRIG(p->trig);

    int32_t res = OK;
    const char *err = NULL;

    /* The angle unit was validated by the init: it is an i-argument. */
    double angle_y = 0.0, angle_p = 0.0, angle_r = 0.0;
    (void) ypr_angles(csound, p, false, &angle_y, &angle_p, &angle_r);

    uint32_t new_ndim = 2U;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    new_shape[0] = 3U;
    new_shape[1] = 3U;

    csound->LockMutex(reg->mutex);

    CSN_ARRAY *array = NULL;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &array, &p->k_data, NULL, new_ndim, new_shape, 9U, CSN_REAL, err);
    if (res != OK) goto done;
    p->array = array;

    MAT_YPR_COMPOSED(array->data, angle_y, angle_p, angle_r);

    SET_KDATA_END(p, new_shape, new_ndim, CSN_REAL);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csncoord_rotmat_ypr_deinit(CSOUND *csound, CSN_ROTMAT_YPR *p) {
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

int32_t csncoord_rotmat_ypr(CSOUND *csound, CSN_ROTMAT_YPR *p) {
    return csncoord_rotmat_ypr_helper(csound, p);
}

int32_t csncoord_rotmat_ypr_k_init(CSOUND *csound, CSN_ROTMAT_YPR *p) {
    return csncoord_rotmat_ypr_helper(csound, p);
}

int32_t csncoord_rotmat_ypr_k(CSOUND *csound, CSN_ROTMAT_YPR *p) {
    return csncoord_rotmat_ypr_k_helper(csound, p);
}

static int32_t csncoord_nmtoacn_helper(CSOUND *csound, CSN_NM2ACN *p, bool is_perf) {
    double n_val = (double) *p->n;
    double m_val = (double) *p->m;

    if (!IS_VALID_VALUE_INT32(n_val) || !IS_VALID_VALUE_INT32(m_val)) {
        if (is_perf) {
            return csound->PerfError(csound, &p->h, "[csnarray] n, m should be valid integer values");
        } else {
            return csound->InitError(csound, "[csnarray] n, m should be valid integer values");
        }
    }

    int32_t n = (int32_t) n_val;
    int32_t m = (int32_t) m_val;

    if (n < 0) {
        if (is_perf) {
            return csound->PerfError(csound, &p->h, "[csnarray] n should be greater than zero");
        } else {
            return csound->InitError(csound, "[csnarray] n should be greater than zero");
        }
    }

    if (m < -n || m > n) {
        if (is_perf) {
            return csound->PerfError(csound, &p->h, "[csnarray] m should be between -n and +n");
        } else {
            return csound->InitError(csound, "[csnarray] m should be between -n and +n");
        }
    }

    int32_t acn = (n * n) + n + m;
    *p->acn = (MYFLT) acn;

    return OK;
}

static int32_t csncoord_acntonm_helper(CSOUND *csound, CSN_ACN2NM *p, bool is_perf) {
    double acn = (double) *p->acn;

    if (!IS_VALID_VALUE_INT32(acn)) {
        if (is_perf) {
            return csound->PerfError(csound, &p->h, "[csnarray] ACN should be valid integer values greater or equal to zero");
        } else {
            return csound->InitError(csound, "[csnarray] ACN should be valid integer values greater or equal to zero");
        }
    }

    if (acn < 0.0) {
        if (is_perf) {
            return csound->PerfError(csound, &p->h, "[csnarray] ACN should be valid integer values greater or equal to zero");
        } else {
            return csound->InitError(csound, "[csnarray] ACN should be valid integer values greater or equal to zero");
        }
    }

    double n = floor(sqrt(acn));
    double m = acn - (n * n) - n;

    *p->n = (MYFLT) n;
    *p->m = (MYFLT) m;

    return OK;
}

static int32_t csncoord_hoaordtochnls_helper(CSOUND *csound, CSN_COORD1 *p, bool is_perf) {
    double ord = (double) *p->arg_a;

    if (!IS_VALID_VALUE_INT32(ord)) {
        if (is_perf) {
            return csound->PerfError(csound, &p->h, "[csnarray] HOA order should be valid integer values greater or equal to zero");
        } else {
            return csound->InitError(csound, "[csnarray] HOA order should be valid integer values greater or equal to zero");
        }
    }

    if (ord < 0.0) {
        if (is_perf) {
            return csound->PerfError(csound, &p->h, "[csnarray] HOA order should be valid integer values greater or equal to zero");
        } else {
            return csound->InitError(csound, "[csnarray] HOA order should be valid integer values greater or equal to zero");
        }
    }

    double n_temp = ord + 1.0;
    double n = n_temp * n_temp;
    *p->value = (MYFLT) n;

    return OK;
}

static int32_t csncoord_chnlstohoaord_helper(CSOUND *csound, CSN_COORD1 *p, bool is_perf) {
    double chnls = (double) *p->arg_a;

    if (!IS_VALID_VALUE_INT32(chnls)) {
        if (is_perf) {
            return csound->PerfError(csound, &p->h, "[csnarray] Number of channels be valid integer values greater or equal to zero");
        } else {
            return csound->InitError(csound, "[csnarray] Number of channels should be valid integer values greater or equal to zero");
        }
    }

    /* Only a complete 3-D HOA set has an order: 1, 4, 9, 16, 25... channels.
       Anything else is refused rather than rounded, because a fractional order
       is not a smaller set, it is a set that does not exist. */
    double root = floor(sqrt(chnls) + 0.5);
    if (chnls < 1.0 || root * root != chnls) {
        if (is_perf) {
            return csound->PerfError(csound, &p->h, "[csnarray] %g channels is not a complete 3-D HOA set: the count must be a perfect square (1, 4, 9, 16, 25...)", chnls);
        } else {
            return csound->InitError(csound, "[csnarray] %g channels is not a complete 3-D HOA set: the count must be a perfect square (1, 4, 9, 16, 25...)", chnls);
        }
    }

    double c = root - 1.0;
    *p->value = (MYFLT) c;

    return OK;
}


int32_t csncoord_nmtoacn(CSOUND *csound, CSN_NM2ACN *p) {
    return csncoord_nmtoacn_helper(csound, p, false);
}

int32_t csncoord_nmtoacn_k(CSOUND *csound, CSN_NM2ACN *p) {
    return csncoord_nmtoacn_helper(csound, p, true);
}

int32_t csncoord_acntonm(CSOUND *csound, CSN_ACN2NM *p) {
    return csncoord_acntonm_helper(csound, p, false);
}

int32_t csncoord_acntonm_k(CSOUND *csound, CSN_ACN2NM *p) {
    return csncoord_acntonm_helper(csound, p, true);
}

int32_t csncoord_hoaordtochnls(CSOUND *csound, CSN_COORD1 *p) {
    return csncoord_hoaordtochnls_helper(csound, p, false);
}

int32_t csncoord_hoaordtochnls_k(CSOUND *csound, CSN_COORD1 *p) {
    return csncoord_hoaordtochnls_helper(csound, p, true);
}

int32_t csncoord_chnlstohoaord(CSOUND *csound, CSN_COORD1 *p) {
    return csncoord_chnlstohoaord_helper(csound, p, false);
}

int32_t csncoord_chnlstohoaord_k(CSOUND *csound, CSN_COORD1 *p) {
    return csncoord_chnlstohoaord_helper(csound, p, false);
}

static int32_t csncoord_sn3d_helper(CSOUND *csound, CSN_SN3D *p, bool is_n3dtosn3d) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    const char *err = NULL;

    double order_value = (double) *p->order;
    if (!IS_VALID_VALUE_INT32(order_value) || order_value < 0.0) {
        return csound->InitError(csound, "[csnarray] Order should be valid integer values greater or equal to zero");
    }
    size_t order = (size_t) order_value;
    size_t nchnls = (order + 1) * (order + 1);

    csound->LockMutex(reg->mutex);
    uint32_t new_ndim = 1U;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    new_shape[0] = (uint32_t) nchnls;

    if (create_csnarray_locked(csound, reg, &p->h, new_ndim, new_shape, &p->array, p->handle, NULL, 0U, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    for (size_t i = 0; i <= order; i++) {
        size_t start = i * i;
        size_t end = (i + 1) * (i + 1);
        double gain = sqrt(2.0 * (double) i + 1.0);
        if (is_n3dtosn3d) gain = 1.0 / gain;
        for (size_t j = start; j < end; j++) {
            p->array->data[j] = gain;
        }
    }

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csncoord_sn3d_deinit(CSOUND *csound, CSN_SN3D *p) {
    return csnarray_deinit_by_handle(csound, &p->handle->id, &p->array, &p->h);
}

int32_t csncoord_sn3dton3d(CSOUND *csound, CSN_SN3D *p) {
    return csncoord_sn3d_helper(csound, p, false);
}

int32_t csncoord_n3dtosn3d(CSOUND *csound, CSN_SN3D *p) {
    return csncoord_sn3d_helper(csound, p, true);
}
