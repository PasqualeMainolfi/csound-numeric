#ifndef __CSN_MEASURE
#define __CSN_MEASURE

#include "csnregistry.h"
#include "csnum.h"
#include <csdl.h>
#include <stdint.h>

#define CSN_ABCOEFF 0.161
#define T60_SABINE(v, a) (CSN_ABCOEFF * (v) / (a))
#define T60_EYRING(v, s, a) (-CSN_ABCOEFF * (v) / ((s) * log(1 - ((a) / (s)))))
#define F_SCHROEDER(t60, v) (2000.0 * sqrt((t60) / (v)))
#define G_FROM_T60(delay, t60) (pow(10.0, -3.0 * (delay) / (t60)))
#define T60_FROM_G(delay, g) (-3.0 * (delay) / log10(g))


typedef enum {
    CSN_RT60ABS = 0,
    CSN_FSCHROEDER,
    CSN_T602FBG,
    CSN_FBG2T60
} CSN_ACOUSTICS_MODE;

typedef enum {
    CSN_SAMPLES2MILLIS = 0,
    CSN_SAMPLES2SECONDS,
    CSN_MILLIS2SAMPLES,
    CSN_SECONDS2SAMPLES
} CSN_TIMECONV_MODE;

typedef struct {
    OPDS h;
    // outputs
    CSNREF *handle;
    // inputs
    CSNREF *source_handle_a; // volume m3 must be 1D
    /* S and alpha are mat of n x m, where n must be equal to
     * the number of rows of volumes.
     */
    CSNREF *source_handle_b; // S must be n x m [[s_0, s_1, ..., s_n - 1], ...],
    CSNREF *source_handle_c; // alpha must be n x m [[a_0, a_1, ..., a_n - 1], ...],
    MYFLT *trig;
    // private
    CSN_ARRAY *array;
    CSN_THREE_VERSION_K_STATE versions;
    K_DATA k_data;
    bool is_published;
} CSN_SABEYR_ARR;

typedef struct {
    OPDS h;
    // outputs
    MYFLT *value;
    // inputs
    MYFLT *volume;
    CSNREF *source_handle_a; // S 1D
    CSNREF *source_handle_b; // alpha 1D
    MYFLT *trig;
    // private
    CSN_REGISTRY *registry;
    CSN_THREE_VERSION_K_STATE versions;
    double prev_volume;
    double prev_result;
    bool is_published;
} CSN_SABEYR_SCALAR;

typedef struct {
    OPDS h;
    // outputs
    CSNREF *handle;
    // inputs
    CSNREF *source_handle_a; // volume m3 must be 1D
    CSNREF *source_handle_b; // T60 target must be 1D
    MYFLT *trig;
    // private
    CSN_ARRAY *array;
    K_DATA k_data;
    bool is_published;
} CSN_REQABSOLEV_HH;

typedef struct {
    OPDS h;
    // outputs
    CSNREF *handle;
    // inputs
    MYFLT *arg_a;
    CSNREF *source_handle; // T60 target
    MYFLT *trig;
    // private
    CSN_ARRAY *array;
    K_DATA k_data;
    bool is_published;
} CSN_REQABSOLEV_SH;

typedef struct {
    OPDS h;
    // outputs
    CSNREF *handle;
    // inputs
    CSNREF *source_handle;
    MYFLT *arg_b; // T60 target
    MYFLT *trig;
    // private
    CSN_ARRAY *array;
    K_DATA k_data;
    bool is_published;
} CSN_REQABSOLEV_HS;

typedef struct {
    OPDS h;
    // outputs
    CSNREF *handle;
    // inputs
    CSNREF *source_handle_a; // room must be n x 3 [[L, W, H], ...]
    CSNREF *source_handle_b; // mode index must be n x 3 [[p, q, r], ...]
    MYFLT *sound_speed;
    MYFLT *trig;
    // private
    CSN_ARRAY *array;
    K_DATA k_data;
    bool is_published;
} CSN_FMODUS_ARR;

typedef struct {
    OPDS h;
    // outputs
    MYFLT *value;
    // inputs
    CSNREF *source_handle_a; // room must be n x 3 [[L, W, H], ...]
    CSNREF *source_handle_b; // mode index [p, q, r]
    MYFLT *sound_speed;
    MYFLT *trig;
    // private
    CSN_REGISTRY *registry;
    double prev_fac;
} CSN_FMODUS_SCALAR;

typedef struct {
    OPDS h;
    // outputs
    CSNREF *handle;
    // inputs
    CSNREF *source_handle; // times in millis or seconds
    MYFLT *sr;
    MYFLT *trig;
    // private
    CSN_ARRAY *array;
    K_DATA k_data;
    bool is_published;
} CSN_SRTIMES;


int32_t csnarray_sabeyr_arr_deinit(CSOUND *csound, CSN_SABEYR_ARR *p);
int32_t csnarray_sabeyr_scalar_k_init(CSOUND *csound, CSN_SABEYR_SCALAR *p);

int32_t csnarray_sabine_arr(CSOUND *csound, CSN_SABEYR_ARR *p);
int32_t csnarray_sabine_scalar(CSOUND *csound, CSN_SABEYR_SCALAR *p);
int32_t csnarray_sabine_arr_k(CSOUND *csound, CSN_SABEYR_ARR *p);
int32_t csnarray_sabine_scalar_k(CSOUND *csound, CSN_SABEYR_SCALAR *p);
int32_t csnarray_eyring_arr(CSOUND *csound, CSN_SABEYR_ARR *p);
int32_t csnarray_eyring_scalar(CSOUND *csound, CSN_SABEYR_SCALAR *p);
int32_t csnarray_eyring_arr_k(CSOUND *csound, CSN_SABEYR_ARR *p);
int32_t csnarray_eyring_scalar_k(CSOUND *csound, CSN_SABEYR_SCALAR *p);

int32_t csnarray_reqabsorption_hh_deinit(CSOUND *csound, CSN_REQABSOLEV_HH *p);
int32_t csnarray_reqabsorption_sh_deinit(CSOUND *csound, CSN_REQABSOLEV_SH *p);
int32_t csnarray_reqabsorption_hs_deinit(CSOUND *csound, CSN_REQABSOLEV_HS *p);

int32_t csnarray_reqabsorption_hh(CSOUND *csound, CSN_REQABSOLEV_HH *p);
int32_t csnarray_reqabsorption_sh(CSOUND *csound, CSN_REQABSOLEV_SH *p);
int32_t csnarray_reqabsorption_hs(CSOUND *csound, CSN_REQABSOLEV_HS *p);
int32_t csnarray_reqabsorption_hh_k_init(CSOUND *csound, CSN_REQABSOLEV_HH *p);
int32_t csnarray_reqabsorption_hh_k(CSOUND *csound, CSN_REQABSOLEV_HH *p);
int32_t csnarray_reqabsorption_sh_k_init(CSOUND *csound, CSN_REQABSOLEV_SH *p);
int32_t csnarray_reqabsorption_hs_k_init(CSOUND *csound, CSN_REQABSOLEV_HS *p);
int32_t csnarray_reqabsorption_sh_k(CSOUND *csound, CSN_REQABSOLEV_SH *p);
int32_t csnarray_reqabsorption_hs_k(CSOUND *csound, CSN_REQABSOLEV_HS *p);
int32_t csnarray_reqabsorption_ss(CSOUND *csound, CSN_NONARR_OP *p);
int32_t csnarray_reqabsorption_ss_k(CSOUND *csound, CSN_NONARR_OP *p);

int32_t csnarray_fschroeder_hh(CSOUND *csound, CSN_REQABSOLEV_HH *p);
int32_t csnarray_fschroeder_sh(CSOUND *csound, CSN_REQABSOLEV_SH *p);
int32_t csnarray_fschroeder_hs(CSOUND *csound, CSN_REQABSOLEV_HS *p);
int32_t csnarray_fschroeder_hh_k_init(CSOUND *csound, CSN_REQABSOLEV_HH *p);
int32_t csnarray_fschroeder_hh_k(CSOUND *csound, CSN_REQABSOLEV_HH *p);
int32_t csnarray_fschroeder_sh_k_init(CSOUND *csound, CSN_REQABSOLEV_SH *p);
int32_t csnarray_fschroeder_hs_k_init(CSOUND *csound, CSN_REQABSOLEV_HS *p);
int32_t csnarray_fschroeder_sh_k(CSOUND *csound, CSN_REQABSOLEV_SH *p);
int32_t csnarray_fschroeder_hs_k(CSOUND *csound, CSN_REQABSOLEV_HS *p);
int32_t csnarray_fschroeder_ss(CSOUND *csound, CSN_NONARR_OP *p);
int32_t csnarray_fschroeder_ss_k(CSOUND *csound, CSN_NONARR_OP *p);

int32_t csnarray_fpqr_deinit(CSOUND *csound, CSN_FMODUS_ARR *p);

int32_t csnarray_fpqr_nd(CSOUND *csound, CSN_FMODUS_ARR *p);
int32_t csnarray_fpqr_1d(CSOUND *csound, CSN_FMODUS_SCALAR *p);
int32_t csnarray_fpqr_nd_k(CSOUND *csound, CSN_FMODUS_ARR *p);
int32_t csnarray_fpqr_1d_k(CSOUND *csound, CSN_FMODUS_SCALAR *p);

int32_t csnarray_srtimes_deinit(CSOUND *csound, CSN_SRTIMES *p);
int32_t csnarray_samplestomillis(CSOUND *csound, CSN_SRTIMES *p);
int32_t csnarray_samplestoseconds(CSOUND *csound, CSN_SRTIMES *p);
int32_t csnarray_millistosamples(CSOUND *csound, CSN_SRTIMES *p);
int32_t csnarray_secondstosamples(CSOUND *csound, CSN_SRTIMES *p);
int32_t csnarray_samplestomillis_k(CSOUND *csound, CSN_SRTIMES *p);
int32_t csnarray_samplestoseconds_k(CSOUND *csound, CSN_SRTIMES *p);
int32_t csnarray_millistosamples_k(CSOUND *csound, CSN_SRTIMES *p);
int32_t csnarray_secondstosamples_k(CSOUND *csound, CSN_SRTIMES *p);

int32_t csnarray_samplestomillis_s(CSOUND *csound, CSN_NONARR_OP *p);
int32_t csnarray_samplestoseconds_s(CSOUND *csound, CSN_NONARR_OP *p);
int32_t csnarray_millistosamples_s(CSOUND *csound, CSN_NONARR_OP *p);
int32_t csnarray_secondstosamples_s(CSOUND *csound, CSN_NONARR_OP *p);
int32_t csnarray_samplestomillis_s_k(CSOUND *csound, CSN_NONARR_OP *p);
int32_t csnarray_samplestoseconds_s_k(CSOUND *csound, CSN_NONARR_OP *p);
int32_t csnarray_millistosamples_s_k(CSOUND *csound, CSN_NONARR_OP *p);
int32_t csnarray_secondstosamples_s_k(CSOUND *csound, CSN_NONARR_OP *p);

int32_t csnarray_sumdb(CSOUND *csound, CSN_REDUCTION *p);
int32_t csnarray_sumdb_scalar(CSOUND *csound, CSN_REDUCTION_SCALAR *p);
int32_t csnarray_sumdb_k_init(CSOUND *csound, CSN_REDUCTION *p);
int32_t csnarray_sumdb_k(CSOUND *csound, CSN_REDUCTION *p);
int32_t csnarray_sumdb_scalar_k(CSOUND *csound, CSN_REDUCTION_SCALAR *p);
int32_t csnarray_sumdb_s(CSOUND *csound, CSN_NONARR_OP *p);
int32_t csnarray_sumdb_s_k(CSOUND *csound, CSN_NONARR_OP *p);

int32_t csnarray_feedbackgfromt60_hh(CSOUND *csound, CSN_REQABSOLEV_HH *p);
int32_t csnarray_feedbackgfromt60_sh(CSOUND *csound, CSN_REQABSOLEV_SH *p);
int32_t csnarray_feedbackgfromt60_hs(CSOUND *csound, CSN_REQABSOLEV_HS *p);
int32_t csnarray_feedbackgfromt60_ss(CSOUND *csound, CSN_NONARR_OP *p);
int32_t csnarray_t60fromfeedbackg_hh(CSOUND *csound, CSN_REQABSOLEV_HH *p);
int32_t csnarray_t60fromfeedbackg_sh(CSOUND *csound, CSN_REQABSOLEV_SH *p);
int32_t csnarray_t60fromfeedbackg_hs(CSOUND *csound, CSN_REQABSOLEV_HS *p);
int32_t csnarray_t60fromfeedbackg_ss(CSOUND *csound, CSN_NONARR_OP *p);
int32_t csnarray_feedbackgfromt60_hh_k(CSOUND *csound, CSN_REQABSOLEV_HH *p);
int32_t csnarray_feedbackgfromt60_sh_k(CSOUND *csound, CSN_REQABSOLEV_SH *p);
int32_t csnarray_feedbackgfromt60_hs_k(CSOUND *csound, CSN_REQABSOLEV_HS *p);
int32_t csnarray_feedbackgfromt60_hh_k_init(CSOUND *csound, CSN_REQABSOLEV_HH *p);
int32_t csnarray_feedbackgfromt60_sh_k_init(CSOUND *csound, CSN_REQABSOLEV_SH *p);
int32_t csnarray_feedbackgfromt60_hs_k_init(CSOUND *csound, CSN_REQABSOLEV_HS *p);
int32_t csnarray_feedbackgfromt60_ss_k(CSOUND *csound, CSN_NONARR_OP *p);
int32_t csnarray_t60fromfeedbackg_hh_k(CSOUND *csound, CSN_REQABSOLEV_HH *p);
int32_t csnarray_t60fromfeedbackg_sh_k(CSOUND *csound, CSN_REQABSOLEV_SH *p);
int32_t csnarray_t60fromfeedbackg_hs_k(CSOUND *csound, CSN_REQABSOLEV_HS *p);
int32_t csnarray_t60fromfeedbackg_hh_k_init(CSOUND *csound, CSN_REQABSOLEV_HH *p);
int32_t csnarray_t60fromfeedbackg_sh_k_init(CSOUND *csound, CSN_REQABSOLEV_SH *p);
int32_t csnarray_t60fromfeedbackg_hs_k_init(CSOUND *csound, CSN_REQABSOLEV_HS *p);
int32_t csnarray_t60fromfeedbackg_ss_k(CSOUND *csound, CSN_NONARR_OP *p);





#endif
