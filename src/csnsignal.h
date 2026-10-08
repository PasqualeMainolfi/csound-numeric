#ifndef __CSN_SIGNAL
#define __CSN_SIGNAL

#include "csnregistry.h"
#include "csnum.h"
#include <stddef.h>
#include <stdint.h>
#include <csdl.h>

#define CSN_ROOTS_MAX_ITER 1000
#define CSN_MAX_ELLIP_ORDER 64

#define IIR_SPEC(fam, rp_, rs_) CSN_IIR_SPEC spec = { .family = (fam), .rp = (rp_), .rs = (rs_) }


typedef enum {
    W_RECT = 0,
    W_HANNING,
    W_HAMMING,
    W_BARTLETT,
    W_BLACKMAN,
    W_KAISER
} CSN_WINDOW_MODE;

typedef enum {
    BILINEAR_ZPK,
    LP2LP_ZPK,
    LP2HP_ZPK,
    LP2BP_ZPK,
    LP2BS_ZPK,
    BUTTAP,
    BUTTER,
    CHEBY1,
    CHEBY2,
    ELLIP,
    LP2LP,
    LP2HP,
    LP2BP,
    LP2BS,
    TF2ZPK,
    ZPK2TF,
    ZPK2SOS
} CSN_PF_DESIGN_MODE;

typedef enum {
    ROOT_ANY,
    ROOT_REAL,
    ROOT_COMPLEX
} CSN_ROOT_KIND;

typedef enum {
    CSN_LP = 0,
    CSN_HP,
    CSN_BP,
    CSN_BS
} CSN_FILTER_TYPE;

typedef enum {
    CSN_BUTTER,
    CSN_CHEBY1,
    CSN_CHEBY2,
    CSN_ELLIP
} CSN_IIR_TYPE;

typedef struct {
    CSN_COMPLEXDAT v;
    bool is_real;
    bool used;
} CSN_ROOTSLOT;

typedef struct {
    CSN_IIR_TYPE family;
    double rp; // passband ripple in dB, cheby1 and ellip
    double rs; // stopband attenuation in dB, cheby2 and ellip
} CSN_IIR_SPEC;

typedef struct {
    OPDS h;
    // outputs
    CSN_ARRAY *handle;
    // inputs
    cs_float *length;
    cs_float *beta;
    // private
    K_DATA k_data;
    int32_t prev_length;
    double prev_beta;
    bool is_i_time_length;
    bool is_published;
} CSN_WINDOW;

typedef struct {
    CSN_COMPLEXDAT value;
    CSN_COMPLEXDAT derivative;
} CSN_POLYDEV;

typedef struct {
    OPDS h;
    // outputs
    CSN_ARRAY *handle;
    // inputs
    CSN_ARRAY *source_handle;
    cs_float *axis;
} CSN_ROOTS;

typedef struct {
    OPDS h;
    // outputs
    CSN_ARRAY *handle_a;
    CSN_ARRAY *handle_b;
    cs_float *out_gain;
    // inputs
    CSN_ARRAY *source_handle_a; // zeros
                             // b in tf2zpk
    CSN_ARRAY *source_handle_b; // poles
                             // a in tf2zpk
    cs_float *arg_a; // gain
    cs_float *arg_b; // wo in lp2*_* in rad/s
                  // fs in bilinear_zpk in Hz
    cs_float *arg_c; // bw in lp2bp_zpk, lp2bs_zpk in rad/s
} CSN_FDESIGN3;

typedef struct {
    OPDS h;
    // outputs
    CSN_ARRAY *handle_a; // b in zpk2tf
    CSN_ARRAY *handle_b; // a in zpk2tf
    // inputs
    CSN_ARRAY *source_handle_a; // zeros in zpk2tf
                             // b in lp2*
    CSN_ARRAY *source_handle_b; // poles in zpk2tf
                             // a in lp2*
    cs_float *arg_a; // gain in zpk2tf
                  // wo in lp2*
    cs_float *arg_b; // bw in lp2bp, lp2bs (unused by zpk2tf)
} CSN_FDESIGN2;

typedef struct {
    OPDS h;
    // outputs
    CSN_ARRAY *handle_a;
    CSN_ARRAY *handle_b;
    // inputs
    cs_float *arg_a; // order
    ARRAYDAT *fcut; // cutoff
    cs_float *arg_b; // ripple in cheby1
                  // attenuation in cheby2
                  // ripple in ellip
                  // type in butter
    cs_float *arg_c; // type in cheby1
                  // type in cheby2
                  // attenuation in ellip
                  // fs in butter
    cs_float *arg_d; // fs in cheby1
                  // fs in cheby2
                  // type in ellip
    cs_float *arg_e; // fs in ellip
} CSN_FDESIGN1F2;

typedef struct {
    OPDS h;
    // outputs
    CSN_ARRAY *handle_a;
    CSN_ARRAY *handle_b;
    // inputs
    cs_float *arg_a; // order
    cs_float *fcut; // cutoff
    cs_float *arg_b; // ripple in cheby1
                  // attenuation in cheby2
                  // ripple in ellip
                  // type in butter
    cs_float *arg_c; // type in cheby1
                  // type in cheby2
                  // attenuation in ellip
                  // fs in butter
    cs_float *arg_d; // fs in cheby1
                  // fs in cheby2
                  // type in ellip
    cs_float *arg_e; // fs in ellip
} CSN_FDESIGN1F1;

typedef struct {
    OPDS h;
    // outputs
    CSN_ARRAY *handle;
    // inputs
    cs_float *arg_a; // order
    ARRAYDAT *fcut; // cutoff
    cs_float *arg_b; // ripple in cheby1
                  // attenuation in cheby2
                  // ripple in ellip
                  // type in butter
    cs_float *arg_c; // type in cheby1
                  // type in cheby2
                  // attenuation in ellip
                  // fs in butter
    cs_float *arg_d; // fs in cheby1
                  // fs in cheby2
                  // type in ellip
    cs_float *arg_e; // fs in ellip
} CSN_FDESIGN1F2_SOS;

typedef struct {
    OPDS h;
    // outputs
    CSN_ARRAY *handle;
    // inputs
    cs_float *arg_a; // order
    cs_float *fcut; // cutoff
    cs_float *arg_b; // ripple in cheby1
                  // attenuation in cheby2
                  // ripple in ellip
                  // type in butter
    cs_float *arg_c; // type in cheby1
                  // type in cheby2
                  // attenuation in ellip
                  // fs in butter
    cs_float *arg_d; // fs in cheby1
                  // fs in cheby2
                  // type in ellip
    cs_float *arg_e; // fs in ellip
} CSN_FDESIGN1F1_SOS;

typedef struct {
    OPDS h;
    // outputs
    CSN_ARRAY *handle_a; // zeros
    CSN_ARRAY *handle_b; // poles
    cs_float *gain;
    // inputs
    cs_float *order;
} CSN_BUTTAP;

typedef struct {
    OPDS h;
    // outputs
    CSN_ARRAY *handle; // sos[b, ... a, ...]
    // inputs
    CSN_ARRAY *source_handle_a; // zeros
    CSN_ARRAY *source_handle_b; // poles
    cs_float *gain;
} CSN_ZPK2SOS;

typedef struct {
    OPDS h;
    // outputs
    cs_float *sig_out;
    // inputs
    CSN_ARRAY *b;
    CSN_ARRAY *a;
    cs_float *sig_in;
    // private
    CSN_ARRAY b_buffer; // b / a[0], zero-padded to order + 1, complex (imaginary parts zero)
    CSN_ARRAY a_buffer;
    ARRAY_VERSION b_version;
    ARRAY_VERSION a_version;
    CSN_SCRATCH filter_state; // order complex states
    CSN_REGISTRY *registry;
    size_t order;
} CSN_LFILTER_AUDIO;

typedef struct {
    OPDS h;
    // outputs
    cs_float *sig_out;
    // inputs
    CSN_ARRAY *sos;
    cs_float *sig_in;
    // private
    CSN_ARRAY sos_buffer; // (n, 6), complex (imaginary parts zero)
    ARRAY_VERSION sos_version;
    CSN_SCRATCH filter_state; // 2 complex states per section
    CSN_REGISTRY *registry;
    size_t n_sections;
} CSN_SOSFILTER_AUDIO;

typedef struct {
    OPDS h;
    // outputs
    CSN_ARRAY *handle;
    // inputs
    CSN_ARRAY *b;
    CSN_ARRAY *a;
    CSN_ARRAY *source_handle;
    cs_float *opt_a; // axis in the i-rate form, trig in the k-rate forms
    cs_float *opt_b; // axis in the k-rate form
    // private
    CSN_ARRAY b_buffer; // b / a[0], zero-padded to order + 1, complex
    CSN_ARRAY a_buffer; // a / a[0], zero-padded to order + 1, complex
    CSN_SCRATCH filter_state; // order states per slice, complex; current_size = slices held
    ARRAY_VERSION b_version;
    ARRAY_VERSION a_version;
    CSN_AXIS_SPEC axis_spec;
    K_DATA k_data;
    size_t order;
    bool coef_is_complex;
    bool is_published;
} CSN_LFILTER;

typedef struct {
    OPDS h;
    // outputs
    CSN_ARRAY *handle;
    // inputs
    CSN_ARRAY *sos;
    CSN_ARRAY *source_handle;
    cs_float *opt_a; // axis in the i-rate form, trig in the k-rate forms
    cs_float *opt_b; // axis in the k-rate form
    // private
    CSN_ARRAY sos_buffer; // (n, 6), complex
    CSN_SCRATCH filter_state; // 2 states per section per slice, complex; current_size = slices held
    ARRAY_VERSION sos_version;
    CSN_AXIS_SPEC axis_spec;
    K_DATA k_data;
    size_t n_sections;
    bool coef_is_complex;
    bool is_published;
} CSN_SOSFILTER;

typedef struct {
    OPDS h;
    // outputs
    cs_float *sig_out;
    CSN_ARRAY *handle; // new_state z_i
    // inputs
    CSN_ARRAY *b;
    CSN_ARRAY *a;
    cs_float *sig_in;
    CSN_ARRAY *source_handle; // state
    // private
    CSN_ARRAY b_buffer; // b / a[0], zero-padded to order + 1, complex (imaginary parts zero)
    CSN_ARRAY a_buffer;
    ARRAY_VERSION b_version;
    ARRAY_VERSION a_version;
    CSN_SCRATCH filter_state; // order real states, loaded from zi every control period
    CSN_REGISTRY *registry;
    size_t order;
} CSN_LFILTER_AUDIO_Z;

typedef struct {
    OPDS h;
    // outputs
    cs_float *sig_out;
    CSN_ARRAY *handle; // new_state z_i
    // inputs
    CSN_ARRAY *sos;
    cs_float *sig_in;
    CSN_ARRAY *source_handle; // state
    // private
    CSN_ARRAY sos_buffer; // (n, 6), complex (imaginary parts zero)
    ARRAY_VERSION sos_version;
    CSN_SCRATCH filter_state; // 2 real states per section, loaded from zi every control period
    CSN_REGISTRY *registry;
    size_t n_sections;
} CSN_SOSFILTER_AUDIO_Z;

typedef struct {
    OPDS h;
    // outputs
    CSN_ARRAY *handle_a;
    CSN_ARRAY *handle_b; // new_state z_i
    // inputs
    CSN_ARRAY *b;
    CSN_ARRAY *a;
    CSN_ARRAY *source_handle_a;
    CSN_ARRAY *source_handle_b; // state
    cs_float *opt_a; // axis in the i-rate form, trig in the k-rate forms
    cs_float *opt_b; // axis in the k-rate form
    // private
    CSN_ARRAY b_buffer; // b / a[0], zero-padded to order + 1, complex
    CSN_ARRAY a_buffer; // a / a[0], zero-padded to order + 1, complex
    CSN_SCRATCH filter_state; // zi of every slice, order states each, complex; current_size = slices held
    ARRAY_VERSION b_version;
    ARRAY_VERSION a_version;
    CSN_AXIS_SPEC axis_spec;
    K_DATA k_data;       // y; prev_source_version_b follows zi
    ARRAY_VERSION prev_zf_version;
    size_t order;
    bool coef_is_complex;
    bool is_published;
} CSN_LFILTER_Z;

typedef struct {
    OPDS h;
    // outputs
    CSN_ARRAY *handle_a;
    CSN_ARRAY *handle_b;
    // inputs
    CSN_ARRAY *sos;
    CSN_ARRAY *source_handle_a;
    CSN_ARRAY *source_handle_b; // state
    cs_float *opt_a; // axis in the i-rate form, trig in the k-rate forms
    cs_float *opt_b; // axis in the k-rate form
    // private
    CSN_ARRAY sos_buffer; // (n, 6), complex
    CSN_SCRATCH filter_state; // 2 states per section per slice, complex; current_size = slices held
    ARRAY_VERSION sos_version;
    CSN_AXIS_SPEC axis_spec;
    K_DATA k_data;       // y; prev_source_version_b follows zi
    ARRAY_VERSION prev_zf_version;
    size_t n_sections;
    bool coef_is_complex;
    bool is_published;
} CSN_SOSFILTER_Z;

/* One run of the zi forms: the four arrays, the coefficient arrays the
   outputs may not be written over (coef_b NULL for sos), and the filter. */
typedef struct {
    CSN_ARRAY *x;
    CSN_ARRAY *zi;
    CSN_ARRAY *y;
    CSN_ARRAY *zf;
    const CSN_ARRAY *coef_a;
    const CSN_ARRAY *coef_b;
    CSN_SCRATCH *state;
    const CSN_AXIS_SPEC *axis_spec;
    bool coef_is_complex;
    bool is_sos;
    const CSN_COMPLEXDAT *b;
    const CSN_COMPLEXDAT *a;
    size_t order;
    const CSN_COMPLEXDAT *sos;
    size_t n_sections;
} CSN_ZFILTER_PASS;

typedef struct {
    OPDS h;
    // outputs
    cs_float *sig_out;
    // inputs
    cs_float *sig_in;
    cs_float *order; // order
    cs_float *fcut;  // cutoff
    cs_float *arg_a; // in BP/BS:
                  //    bw
                  // in LP/HP:
                  //    ripple in cheby1
                  //    attenuation in cheby2
                  //    ripple in ellip
                  //    type in butter
    cs_float *arg_b; // in BP/BS:
                  //    ripple in cheby1
                  //    attenuation in cheby2
                  //    ripple in ellip
                  //    type in butter
                  // in LP/HP:
                  //    type in cheby1
                  //    type in cheby2
                  //    attenuation in ellip
                  //    fs in butter
    cs_float *arg_c; // in BP/BS:
                  //    type in cheby1
                  //    type in cheby2
                  //    attenuation in ellip
                  //    fs in butter
                  // in LP/HP:
                  //    fs in cheby1
                  //    fs in cheby2
                  //    type in elli
    cs_float *arg_d; // in BP/BS:
                  //    fs in cheby1
                  //    fs in cheby2
                  //    type in ellip
                  // in LP/HP:
                  //    fs in ellip
    cs_float *arg_e; // in BP/BS:
                  //    fs in ellip
    // private
    CSN_ARRAY sos;
    CSN_ARRAY bilinear_zeros; // csnrtbutter LP/HP only
    CSN_ARRAY bilinear_poles; // csnrtbutter LP/HP only
    CSN_ARRAY ztap;
    CSN_ARRAY ptap;
    double ktap;
    CSN_SCRATCH filter_state;
    CSN_FILTER_TYPE ftype;
} CSN_FILTER_AUDIO_PERF; // use sos

typedef struct {
    size_t order;
    double fcut;
    double bw;
    double ripple;
    double attenuation;
    double fs;
    CSN_FILTER_TYPE type;
    CSN_IIR_TYPE mode;
} CSN_FILTER_ARG_TO_PARAMS;

void get_window_function(double *win, uint32_t wsize, CSN_WINDOW_MODE mode, double beta);

int32_t csnarray_hanning_k_init(CSOUND *csound, CSN_WINDOW *p);
int32_t csnarray_hamming_k_init(CSOUND *csound, CSN_WINDOW *p);
int32_t csnarray_bartlett_k_init(CSOUND *csound, CSN_WINDOW *p);
int32_t csnarray_blackman_k_init(CSOUND *csound, CSN_WINDOW *p);
int32_t csnarray_kaiser_k_init(CSOUND *csound, CSN_WINDOW *p);
int32_t csnarray_hanning(CSOUND *csound, CSN_WINDOW *p);
int32_t csnarray_hamming(CSOUND *csound, CSN_WINDOW *p);
int32_t csnarray_bartlett(CSOUND *csound, CSN_WINDOW *p);
int32_t csnarray_blackman(CSOUND *csound, CSN_WINDOW *p);
int32_t csnarray_kaiser(CSOUND *csound, CSN_WINDOW *p);
int32_t csnarray_hanning_k(CSOUND *csound, CSN_WINDOW *p);
int32_t csnarray_hamming_k(CSOUND *csound, CSN_WINDOW *p);
int32_t csnarray_bartlett_k(CSOUND *csound, CSN_WINDOW *p);
int32_t csnarray_blackman_k(CSOUND *csound, CSN_WINDOW *p);
int32_t csnarray_kaiser_k(CSOUND *csound, CSN_WINDOW *p);

int32_t csnsig_roots(CSOUND *csound, CSN_ROOTS *p);

int32_t csnsig_lp2lp_zpk(CSOUND *csound, CSN_FDESIGN3 *p);
int32_t csnsig_lp2hp_zpk(CSOUND *csound, CSN_FDESIGN3 *p);
int32_t csnsig_lp2bp_zpk(CSOUND *csound, CSN_FDESIGN3 *p);
int32_t csnsig_lp2bs_zpk(CSOUND *csound, CSN_FDESIGN3 *p);
int32_t csnsig_bilinear_zpk(CSOUND *csound, CSN_FDESIGN3 *p);
int32_t csnsig_tf2zpk(CSOUND *csound, CSN_FDESIGN3 *p);

int32_t csnsig_zpk2tf(CSOUND *csound, CSN_FDESIGN2 *p);
int32_t csnsig_lp2lp(CSOUND *csound, CSN_FDESIGN2 *p);
int32_t csnsig_lp2hp(CSOUND *csound, CSN_FDESIGN2 *p);
int32_t csnsig_lp2bp(CSOUND *csound, CSN_FDESIGN2 *p);
int32_t csnsig_lp2bs(CSOUND *csound, CSN_FDESIGN2 *p);

int32_t csnsig_butter_band(CSOUND *csound, CSN_FDESIGN1F2 *p);
int32_t csnsig_butter_noband(CSOUND *csound, CSN_FDESIGN1F1 *p);
int32_t csnsig_butter_band_sos(CSOUND *csound, CSN_FDESIGN1F2_SOS *p);
int32_t csnsig_butter_noband_sos(CSOUND *csound, CSN_FDESIGN1F1_SOS *p);
int32_t csnsig_cheby1_band(CSOUND *csound, CSN_FDESIGN1F2 *p);
int32_t csnsig_cheby1_noband(CSOUND *csound, CSN_FDESIGN1F1 *p);
int32_t csnsig_cheby2_band(CSOUND *csound, CSN_FDESIGN1F2 *p);
int32_t csnsig_cheby2_noband(CSOUND *csound, CSN_FDESIGN1F1 *p);
int32_t csnsig_cheby1_band_sos(CSOUND *csound, CSN_FDESIGN1F2_SOS *p);
int32_t csnsig_cheby1_noband_sos(CSOUND *csound, CSN_FDESIGN1F1_SOS *p);
int32_t csnsig_cheby2_band_sos(CSOUND *csound, CSN_FDESIGN1F2_SOS *p);
int32_t csnsig_cheby2_noband_sos(CSOUND *csound, CSN_FDESIGN1F1_SOS *p);
int32_t csnsig_ellip_band(CSOUND *csound, CSN_FDESIGN1F2 *p);
int32_t csnsig_ellip_noband(CSOUND *csound, CSN_FDESIGN1F1 *p);
int32_t csnsig_ellip_band_sos(CSOUND *csound, CSN_FDESIGN1F2_SOS *p);
int32_t csnsig_ellip_noband_sos(CSOUND *csound, CSN_FDESIGN1F1_SOS *p);

int32_t csnsig_zpk2sos(CSOUND *csound, CSN_ZPK2SOS *p);

int32_t csnsig_buttap(CSOUND *csound, CSN_BUTTAP *p);

int32_t csnsig_lfilter_arr_deinit(CSOUND *csound, CSN_LFILTER *p);
int32_t csnsig_lfilter_audio_deinit(CSOUND *csound, CSN_LFILTER_AUDIO *p);
int32_t csnsig_sosfilter_arr_deinit(CSOUND *csound, CSN_SOSFILTER *p);
int32_t csnsig_sosfilter_audio_deinit(CSOUND *csound, CSN_SOSFILTER_AUDIO *p);

int32_t csnsig_lfilter_arr_k_init(CSOUND *csound, CSN_LFILTER *p);
int32_t csnsig_sosfilter_arr_k_init(CSOUND *csound, CSN_SOSFILTER *p);
int32_t csnsig_lfilter_arr(CSOUND *csound, CSN_LFILTER *p);
int32_t csnsig_lfilter_arr_k(CSOUND *csound, CSN_LFILTER *p);
int32_t csnsig_sosfilter_arr(CSOUND *csound, CSN_SOSFILTER *p);
int32_t csnsig_sosfilter_arr_k(CSOUND *csound, CSN_SOSFILTER *p);

int32_t csnsig_lfilter_audio_init(CSOUND *csound, CSN_LFILTER_AUDIO *p);
int32_t csnsig_sosfilter_audio_init(CSOUND *csound, CSN_SOSFILTER_AUDIO *p);
int32_t csnsig_lfilter_audio(CSOUND *csound, CSN_LFILTER_AUDIO *p);
int32_t csnsig_sosfilter_audio(CSOUND *csound, CSN_SOSFILTER_AUDIO *p);

int32_t csnsig_zlfilter_arr_deinit(CSOUND *csound, CSN_LFILTER_Z *p);
int32_t csnsig_zlfilter_audio_deinit(CSOUND *csound, CSN_LFILTER_AUDIO_Z *p);
int32_t csnsig_zsosfilter_arr_deinit(CSOUND *csound, CSN_SOSFILTER_Z *p);
int32_t csnsig_zsosfilter_audio_deinit(CSOUND *csound, CSN_SOSFILTER_AUDIO_Z *p);

int32_t csnsig_zlfilter_arr_k_init(CSOUND *csound, CSN_LFILTER_Z *p);
int32_t csnsig_zsosfilter_arr_k_init(CSOUND *csound, CSN_SOSFILTER_Z *p);
int32_t csnsig_zlfilter_arr(CSOUND *csound, CSN_LFILTER_Z *p);
int32_t csnsig_zlfilter_arr_k(CSOUND *csound, CSN_LFILTER_Z *p);
int32_t csnsig_zsosfilter_arr(CSOUND *csound, CSN_SOSFILTER_Z *p);
int32_t csnsig_zsosfilter_arr_k(CSOUND *csound, CSN_SOSFILTER_Z *p);

int32_t csnsig_zlfilter_audio_init(CSOUND *csound, CSN_LFILTER_AUDIO_Z *p);
int32_t csnsig_zsosfilter_audio_init(CSOUND *csound, CSN_SOSFILTER_AUDIO_Z *p);
int32_t csnsig_zlfilter_audio(CSOUND *csound, CSN_LFILTER_AUDIO_Z *p);
int32_t csnsig_zsosfilter_audio(CSOUND *csound, CSN_SOSFILTER_AUDIO_Z *p);

int32_t csnsig_rtbutter_deinit(CSOUND *csound, CSN_FILTER_AUDIO_PERF *p);
int32_t csnsig_rtbutter_band_init(CSOUND *csound, CSN_FILTER_AUDIO_PERF *p);
int32_t csnsig_rtbutter_band_perf(CSOUND *csound, CSN_FILTER_AUDIO_PERF *p);
int32_t csnsig_rtbutter_noband_init(CSOUND *csound, CSN_FILTER_AUDIO_PERF *p);
int32_t csnsig_rtbutter_noband_perf(CSOUND *csound, CSN_FILTER_AUDIO_PERF *p);

int32_t csnsig_rtcheby1_deinit(CSOUND *csound, CSN_FILTER_AUDIO_PERF *p);
int32_t csnsig_rtcheby1_init(CSOUND *csound, CSN_FILTER_AUDIO_PERF *p);
int32_t csnsig_rtcheby1_band_init(CSOUND *csound, CSN_FILTER_AUDIO_PERF *p);
int32_t csnsig_rtcheby1_perf(CSOUND *csound, CSN_FILTER_AUDIO_PERF *p);
int32_t csnsig_rtcheby2_deinit(CSOUND *csound, CSN_FILTER_AUDIO_PERF *p);
int32_t csnsig_rtcheby2_init(CSOUND *csound, CSN_FILTER_AUDIO_PERF *p);
int32_t csnsig_rtcheby2_band_init(CSOUND *csound, CSN_FILTER_AUDIO_PERF *p);
int32_t csnsig_rtcheby2_perf(CSOUND *csound, CSN_FILTER_AUDIO_PERF *p);
int32_t csnsig_rtellip_deinit(CSOUND *csound, CSN_FILTER_AUDIO_PERF *p);
int32_t csnsig_rtellip_init(CSOUND *csound, CSN_FILTER_AUDIO_PERF *p);
int32_t csnsig_rtellip_band_init(CSOUND *csound, CSN_FILTER_AUDIO_PERF *p);
int32_t csnsig_rtellip_perf(CSOUND *csound, CSN_FILTER_AUDIO_PERF *p);

#endif
