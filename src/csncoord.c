#include "csncoord.h"
#include "csnum.h"
#include <math.h>
#include <stdint.h>

#define CSN_DEG_PER_RAD (180.0 / M_PI)
#define CSN_RAD_PER_DEG (M_PI / 180.0)

static bool coord_is_to_cartesian(CSN_COORDS_MODE mode) {
    return mode == CSN_POL2CAR || mode == CSN_CYL2CAR || mode == CSN_SPH2CAR;
}

/* z/r can leave [-1, 1] by an ulp once r has been rounded, and acos answers
   NaN there rather than the pole the caller clearly meant. */
static double coord_clamp_unit(double value) {
    if (value > 1.0) return 1.0;
    if (value < -1.0) return -1.0;
    return value;
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
        double flag = (double) *degrees;
        if (!IS_VALID_ZERO_ONE(flag)) {
            return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] Invalid angle unit: 0 selects radians, 1 degrees");
        }
        in_degrees = flag != 0.0;
    }

    if (in_degrees && coord_is_to_cartesian(mode)) {
        bin *= CSN_RAD_PER_DEG;
        if (mode == CSN_SPH2CAR) cin *= CSN_RAD_PER_DEG;
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
    }

    if (in_degrees && !coord_is_to_cartesian(mode)) {
        bout *= CSN_DEG_PER_RAD;
        if (mode == CSN_CAR2SPH) cout *= CSN_DEG_PER_RAD;
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

int32_t csncoord_degtorad(CSOUND *csound, CSN_COORD1 *p) {
    (void) csound;
    *p->value = (MYFLT) ((double) *p->angle * CSN_RAD_PER_DEG);
    return OK;
}

int32_t csncoord_radtodeg(CSOUND *csound, CSN_COORD1 *p) {
    (void) csound;
    *p->value = (MYFLT) ((double) *p->angle * CSN_DEG_PER_RAD);
    return OK;
}
