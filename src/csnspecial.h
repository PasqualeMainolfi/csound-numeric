#ifndef __CSN_SPECIAL
#define __CSN_SPECIAL

#include "csnregistry.h"
#include "csnum.h"
#include <csdl.h>
#include <stdint.h>

/* Special functions. math.h has none of these, so each one is written from its
   recurrence here and kept free of the registry, so the opcodes are thin
   wrappers around a function that can be checked on its own.

   Two conventions hold for the whole file and are not flags:

     - no Condon-Shortley phase. csn_legendre_p(n, m, x) is P_n^m(x) without
       the (-1)^m factor, so scipy.special.lpmv(m, n, x) equals
       (-1)^m * csn_legendre_p(n, m, x). Ambisonics does not use the phase,
       and the spherical harmonics below are built on this same P.

     - the spherical harmonics are the real ones in the AmbiX convention: ACN
       order, SN3D normalisation, no global 1/sqrt(4*pi), azimuth from +x
       towards +y and elevation from the horizon, as in csnhoatocar. */

/* The largest degree n accepted anywhere in this file. The recurrences are
   O(n) per value, so the cap keeps a stray argument from stalling a note, and
   it is well past any ambisonics order in use. */
#define CSN_SPECIAL_MAX_DEGREE 1000

/* P_n^m(x) for 0 <= m <= n and |x| <= 1, unnormalised and without the
   Condon-Shortley phase. The caller validates the arguments. The result can
   overflow to infinity for a large m, since (2m - 1)!! grows faster than any
   power; the caller checks for that rather than this function guessing. */
double csn_legendre_p(int32_t n, int32_t m, double x);

/* The real SN3D spherical harmonic Y_n^m at one direction, -n <= m <= n. */
double csn_sh_sn3d(int32_t n, int32_t m, double azimuth, double elevation);

/* Every harmonic up to the given order at one direction, (order + 1)^2
   values written at out[ACN * stride]. */
void csn_sh_sn3d_all(int32_t order, double azimuth, double elevation, double *out, size_t stride);

typedef struct {
    OPDS h;
    // outputs
    MYFLT *value;
    // inputs
    MYFLT *n;
    MYFLT *m;
    MYFLT *x;
} CSN_LEGENDRE;

typedef struct {
    OPDS h;
    // outputs
    CSNREF *handle;
    // inputs
    MYFLT *n;
    MYFLT *m;
    CSNREF *source_handle;
    MYFLT *trig;
    // private
    CSN_ARRAY *array;
    K_DATA k_data;
    int32_t degree_n;
    int32_t degree_m;
} CSN_LEGENDRE_ARR;

typedef struct {
    OPDS h;
    // outputs
    MYFLT *value;
    // inputs
    MYFLT *n;
    MYFLT *m;
    MYFLT *azimuth;
    MYFLT *elevation;
    MYFLT *degree;
} CSN_SPHHARM;

typedef struct {
    OPDS h;
    // outputs
    CSNREF *handle;
    // inputs
    MYFLT *order;
    MYFLT *azimuth;
    MYFLT *elevation;
    MYFLT *degree;
    MYFLT *trig;
    // private
    CSN_ARRAY *array;
    K_DATA k_data;
    int32_t order_value;
    bool is_degree;
} CSN_SPHHARM_ACN;

typedef struct {
    OPDS h;
    // outputs
    CSNREF *handle;
    // inputs
    MYFLT *order;
    CSNREF *azimuth_handle;
    CSNREF *elevation_handle;
    MYFLT *degree;
    MYFLT *trig;
    // private
    CSN_ARRAY *array;
    K_DATA k_data;
    int32_t order_value;
    bool is_degree;
} CSN_SPHHARM_MAT;

int32_t csnspecial_legendre(CSOUND *csound, CSN_LEGENDRE *p);
int32_t csnspecial_legendre_k(CSOUND *csound, CSN_LEGENDRE *p);
int32_t csnspecial_legendre_arr(CSOUND *csound, CSN_LEGENDRE_ARR *p);
int32_t csnspecial_legendre_arr_k(CSOUND *csound, CSN_LEGENDRE_ARR *p);
int32_t csnspecial_legendre_arr_deinit(CSOUND *csound, CSN_LEGENDRE_ARR *p);

int32_t csnspecial_sphharm(CSOUND *csound, CSN_SPHHARM *p);
int32_t csnspecial_sphharm_k(CSOUND *csound, CSN_SPHHARM *p);

int32_t csnspecial_sphharm_acn(CSOUND *csound, CSN_SPHHARM_ACN *p);
int32_t csnspecial_sphharm_acn_k(CSOUND *csound, CSN_SPHHARM_ACN *p);
int32_t csnspecial_sphharm_acn_deinit(CSOUND *csound, CSN_SPHHARM_ACN *p);

int32_t csnspecial_sphharm_mat(CSOUND *csound, CSN_SPHHARM_MAT *p);
int32_t csnspecial_sphharm_mat_k(CSOUND *csound, CSN_SPHHARM_MAT *p);
int32_t csnspecial_sphharm_mat_deinit(CSOUND *csound, CSN_SPHHARM_MAT *p);

#endif
