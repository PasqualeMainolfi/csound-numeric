#ifndef __CSN_COORD
#define __CSN_COORD

#include <csdl.h>
#include <stdint.h>


/* Three coordinate systems, each with its own opcode pair, so nothing has to
   be inferred from how many arguments were passed:

     polar        (r, phi)          planar, phi measured from +x in the xy plane
     cylindrical  (r, phi, z)       polar with the height carried through
     spherical    (r, theta, phi)   theta the inclination from +z, phi the
                                    azimuth from +x, the ISO 80000-2 order

   Both directions of a pair take and answer their angles in the same order, so
   feeding one into the other returns the point it started from. */
typedef enum {
    CSN_POL2CAR,
    CSN_CAR2POL,
    CSN_CYL2CAR,
    CSN_CAR2CYL,
    CSN_SPH2CAR,
    CSN_CAR2SPH
} CSN_COORDS_MODE;


typedef struct {
    OPDS h;
    // outputs
    MYFLT *a_out;     // x, or r
    MYFLT *b_out;     // y, or phi (cylindrical) / theta (spherical)
    MYFLT *c_out;     // z, or phi (spherical)
    // inputs
    MYFLT *a_in;
    MYFLT *b_in;
    MYFLT *c_in;
    MYFLT *degrees;   // 0 = radians (default), 1 = degrees
} CSN_COORD3;

typedef struct {
    OPDS h;
    // outputs
    MYFLT *a_out;     // x, or r
    MYFLT *b_out;     // y, or phi
    // inputs
    MYFLT *a_in;
    MYFLT *b_in;
    MYFLT *degrees;   // 0 = radians (default), 1 = degrees
} CSN_COORD2;

typedef struct {
    OPDS h;
    // outputs
    MYFLT *value;
    // inputs
    MYFLT *angle;
} CSN_COORD1;


int32_t csncoord_poltocar(CSOUND *csound, CSN_COORD2 *p);
int32_t csncoord_cartopol(CSOUND *csound, CSN_COORD2 *p);
int32_t csncoord_cyltocar(CSOUND *csound, CSN_COORD3 *p);
int32_t csncoord_cartocyl(CSOUND *csound, CSN_COORD3 *p);
int32_t csncoord_sphtocar(CSOUND *csound, CSN_COORD3 *p);
int32_t csncoord_cartosph(CSOUND *csound, CSN_COORD3 *p);

int32_t csncoord_poltocar_k(CSOUND *csound, CSN_COORD2 *p);
int32_t csncoord_cartopol_k(CSOUND *csound, CSN_COORD2 *p);
int32_t csncoord_cyltocar_k(CSOUND *csound, CSN_COORD3 *p);
int32_t csncoord_cartocyl_k(CSOUND *csound, CSN_COORD3 *p);
int32_t csncoord_sphtocar_k(CSOUND *csound, CSN_COORD3 *p);
int32_t csncoord_cartosph_k(CSOUND *csound, CSN_COORD3 *p);

int32_t csncoord_degtorad(CSOUND *csound, CSN_COORD1 *p);
int32_t csncoord_radtodeg(CSOUND *csound, CSN_COORD1 *p);

#endif
