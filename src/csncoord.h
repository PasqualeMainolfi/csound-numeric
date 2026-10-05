#ifndef __CSN_COORD
#define __CSN_COORD

#include "csnregistry.h"
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
    CSN_CAR2SPH,
    CSN_CAR2HOA,
    CSN_HOA2CAR
} CSN_COORDS_MODE;

typedef enum {
    CSN_MATYAW = 0,
    CSN_MATPITCH,
    CSN_MATROLL
} CSN_YPR_MODE;

typedef struct {
    OPDS h;
    // outputs
    cs_float *a_out;     // x, or r
    cs_float *b_out;     // y, or phi (cylindrical) / theta (spherical)
    cs_float *c_out;     // z, or phi (spherical)
    // inputs
    cs_float *a_in;
    cs_float *b_in;
    cs_float *c_in;
    cs_float *degrees;   // 0 = radians (default), 1 = degrees
} CSN_COORD3;

typedef struct {
    OPDS h;
    // outputs
    cs_float *a_out;     // x, or r
    cs_float *b_out;     // y, or phi
    // inputs
    cs_float *a_in;
    cs_float *b_in;
    cs_float *degrees;   // 0 = radians (default), 1 = degrees
} CSN_COORD2;

typedef struct {
    OPDS h;
    // outputs
    cs_float *value;
    // inputs
    cs_float *arg_a;
} CSN_COORD1;

typedef struct {
    OPDS h;
    // outputs
    CSN_ARRAY *handle;
    // inputs
    cs_float *angle;
    cs_float *axis;
    cs_float *degree;
    cs_float *trig;
    // private
    CSN_REGISTRY *registry;
    bool is_degree;
    /* Which basis axis the matrix turns about, 0, 1 or 2. Not a data axis:
       negative values do not count from the end here, they are an error. */
    uint32_t axis_index;
} CSN_ROTMAT;

typedef struct {
    OPDS h;
    // outputs
    CSN_ARRAY *handle;
    // inputs
    CSN_ARRAY *source_handle;
    cs_float *angle;
    cs_float *degree;
    cs_float *trig;
    // private
    CSN_REGISTRY *registry;
    bool is_degree;
} CSN_ROTMAT_AXIS;

typedef struct {
    OPDS h;
    // outputs
    CSN_ARRAY *handle;
    // inputs
    cs_float *yaw;
    cs_float *pitch;
    cs_float *roll;
    cs_float *degree;
    cs_float *trig;
    // private
    CSN_REGISTRY *registry;
    bool is_degree;
} CSN_ROTMAT_YPR;

typedef struct {
    OPDS h;
    // outputs
    cs_float *acn;
    // inputs
    cs_float *n;
    cs_float *m;
} CSN_NM2ACN;

typedef struct {
    OPDS h;
    // outputs
    cs_float *n;
    cs_float *m;
    // inputs
    cs_float *acn;
} CSN_ACN2NM;

typedef struct {
    OPDS h;
    // outputs
    CSN_ARRAY *handle;
    // inputs
    cs_float *order;
} CSN_SN3D;




int32_t csncoord_poltocar(CSOUND *csound, CSN_COORD2 *p);
int32_t csncoord_cartopol(CSOUND *csound, CSN_COORD2 *p);
int32_t csncoord_cyltocar(CSOUND *csound, CSN_COORD3 *p);
int32_t csncoord_cartocyl(CSOUND *csound, CSN_COORD3 *p);
int32_t csncoord_sphtocar(CSOUND *csound, CSN_COORD3 *p);
int32_t csncoord_cartosph(CSOUND *csound, CSN_COORD3 *p);

int32_t csncoord_cartohoa(CSOUND *csound, CSN_COORD3 *p);
int32_t csncoord_hoatocar(CSOUND *csound, CSN_COORD3 *p);
int32_t csncoord_cartohoa_k(CSOUND *csound, CSN_COORD3 *p);
int32_t csncoord_hoatocar_k(CSOUND *csound, CSN_COORD3 *p);

int32_t csncoord_rotmat(CSOUND *csound, CSN_ROTMAT *p);
int32_t csncoord_rotmat_k_init(CSOUND *csound, CSN_ROTMAT *p);
int32_t csncoord_rotmat_k(CSOUND *csound, CSN_ROTMAT *p);
int32_t csncoord_rotmat_axis(CSOUND *csound, CSN_ROTMAT_AXIS *p);
int32_t csncoord_rotmat_axis_k_init(CSOUND *csound, CSN_ROTMAT_AXIS *p);
int32_t csncoord_rotmat_axis_k(CSOUND *csound, CSN_ROTMAT_AXIS *p);
int32_t csncoord_rotmat_ypr_k_init(CSOUND *csound, CSN_ROTMAT_YPR *p);
int32_t csncoord_rotmat_ypr(CSOUND *csound, CSN_ROTMAT_YPR *p);
int32_t csncoord_rotmat_ypr_k(CSOUND *csound, CSN_ROTMAT_YPR *p);

int32_t csncoord_poltocar_k(CSOUND *csound, CSN_COORD2 *p);
int32_t csncoord_cartopol_k(CSOUND *csound, CSN_COORD2 *p);
int32_t csncoord_cyltocar_k(CSOUND *csound, CSN_COORD3 *p);
int32_t csncoord_cartocyl_k(CSOUND *csound, CSN_COORD3 *p);
int32_t csncoord_sphtocar_k(CSOUND *csound, CSN_COORD3 *p);
int32_t csncoord_cartosph_k(CSOUND *csound, CSN_COORD3 *p);

int32_t csncoord_degtorad(CSOUND *csound, CSN_COORD1 *p);
int32_t csncoord_radtodeg(CSOUND *csound, CSN_COORD1 *p);

int32_t csncoord_nmtoacn(CSOUND *csound, CSN_NM2ACN *p);
int32_t csncoord_nmtoacn_k(CSOUND *csound, CSN_NM2ACN *p);
int32_t csncoord_acntonm(CSOUND *csound, CSN_ACN2NM *p);
int32_t csncoord_acntonm_k(CSOUND *csound, CSN_ACN2NM *p);
int32_t csncoord_hoaordtochnls(CSOUND *csound, CSN_COORD1 *p);
int32_t csncoord_hoaordtochnls_k(CSOUND *csound, CSN_COORD1 *p);
int32_t csncoord_chnlstohoaord(CSOUND *csound, CSN_COORD1 *p);
int32_t csncoord_chnlstohoaord_k(CSOUND *csound, CSN_COORD1 *p);

int32_t csncoord_sn3dton3d(CSOUND *csound, CSN_SN3D *p);
int32_t csncoord_n3dtosn3d(CSOUND *csound, CSN_SN3D *p);

#endif
