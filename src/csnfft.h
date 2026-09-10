#ifndef __CSN_FFT
#define __CSN_FFT

#include "csnregistry.h"
#include "csnum.h"
#include <stddef.h>
#include <csdl.h>


#define NUMBER_OF_STFT_WINDOWS 3
#define NUMBER_OF_EDGES_MODE 3

typedef enum {
    CSNFFT = 0,
    CSNRFFT,
    CSNIFFT,
    CSNIRFFT,
    CSNFFTFREQ,
    CSNRFFTFREQ,
    CSNFFTSHIFT,
    CSNIFFTSHIFT,
} CSN_FFT_MODE;

typedef enum {
    EDGES_FULL = 0,
    EDGES_SAME,
    EDGES_VALID
} CSN_EDGES_MODE;

typedef enum {
    CSN_CONVOLUTION = 0,
    CSN_CORRELATION,
} CSN_CORRCONV_MODE;

typedef struct {
    void *fft_setup;
    size_t nfft;
    size_t hopsize;
    size_t buffer_out_size;
    size_t buffer_work_size;
    double sr;
    CSN_FFT_MODE mode;
} K_DATA_FFT;

typedef struct {
    OPDS h;
    // outputs
    CSNREF *handle;
    // inputs
    CSNREF *source_handle;
    MYFLT *fft_size;
    MYFLT *axis; // -1 last axis (as numpy)
    MYFLT *trig;
    // private
    CSN_ARRAY *array;
    CSN_SCRATCH buffer;
    K_DATA k_data;
    K_DATA_FFT k_data_fft;
    bool is_published;
} CSN_FFT;

typedef struct {
    OPDS h;
    // outputs
    CSNREF *handle;
    // inputs
    CSNREF *source_handle;
    MYFLT *rows_fft_size;
    MYFLT *cols_fft_size;
    MYFLT *trig;
    // private
    CSN_ARRAY *array;
    CSN_ARRAY intermediate;
    CSN_SCRATCH row_buffer;
    CSN_SCRATCH col_buffer;
    K_DATA k_data;
    K_DATA_FFT k_data_row_fft;
    K_DATA_FFT k_data_col_fft;
    bool is_published;
} CSN_FFT2;

typedef struct {
    OPDS h;
    // outputs
    CSNREF *handle_f;
    CSNREF *handle_t;
    CSNREF *handle_z;
    // inputs
    CSNREF *source_handle;
    MYFLT *winsize;
    MYFLT *hopsize;
    MYFLT *sr;
    MYFLT *window_type;
    MYFLT *trig;
    // private
    CSN_ARRAY *array_f;
    CSN_ARRAY *array_t;
    CSN_ARRAY *array_z;
    CSN_SCRATCH buffer;
    CSN_SCRATCH window;
    K_DATA k_data_f;
    K_DATA k_data_t;
    K_DATA k_data_z;
    K_DATA_FFT k_data_fft;
    bool is_published;
} CSN_STFT;

typedef struct {
    OPDS h;
    // outputs
    CSNREF *handle_t;
    CSNREF *handle_x;
    // inputs
    CSNREF *source_handle;
    MYFLT *winsize;
    MYFLT *hopsize;
    MYFLT *sr;
    MYFLT *window_type;
    MYFLT *trig;
    // private
    CSN_ARRAY *array_t;
    CSN_ARRAY *array_x;
    CSN_SCRATCH buffer;
    CSN_SCRATCH window;
    CSN_SCRATCH window_sum;
    K_DATA k_data_t;
    K_DATA k_data_x;
    K_DATA_FFT k_data_fft;
    bool is_published;
} CSN_ISTFT;

typedef struct {
    OPDS h;
    // outputs
    CSNREF *handle;
    // inputs
    MYFLT *size;
    MYFLT *d;
    MYFLT *trig;
    // private
    CSN_ARRAY *array;
    K_DATA k_data;
    bool is_published;
} CSN_FFTFREQ;

typedef struct {
    OPDS h;
    // outputs
    CSNREF *handle;
    // inputs
    CSNREF *source_handle;
    MYFLT *axis;
    MYFLT *trig;
    // private
    CSN_ARRAY *array;
    K_DATA k_data;
    bool is_published;
} CSN_FFTSHIFT;

typedef struct {
    uint32_t x_shape_fft[CSN_MAX_DIMS];
    uint32_t h_shape_fft[CSN_MAX_DIMS];
    uint32_t shape_padded_x[CSN_MAX_DIMS];
    uint32_t shape_padded_h[CSN_MAX_DIMS];
    uint32_t ifft_shape[CSN_MAX_DIMS];
    uint32_t padded_ndim;
    uint32_t x_ndim;
    uint32_t h_ndim;
    size_t x_out_size;
    size_t h_out_size;
    size_t x_work_size;
    size_t h_work_size;
    /* One entry per axis. The 1-D forms transform a single axis and use slot
       0; the N-D forms transform every axis in turn, each with its own length,
       and the crop then reads one offset per axis. */
    int64_t crop_offset[CSN_MAX_DIMS];
    size_t fft_sizes[CSN_MAX_DIMS];
    size_t axis_out_size[CSN_MAX_DIMS];
    size_t axis_work_size[CSN_MAX_DIMS];
    size_t ifft_axis_out_size[CSN_MAX_DIMS];
    size_t ifft_axis_work_size[CSN_MAX_DIMS];
    uint32_t ifft_ndim;
    size_t ifft_out_size;
    size_t ifft_work_size;
} CSN_FFTCORRCONV_DATA;

typedef struct {
    OPDS h;
    // outputs
    CSNREF *handle;
    // inputs
    CSNREF *source_handle_a;
    CSNREF *source_handle_b;
    MYFLT *arg_a; // mode for convolve1d and correlate1d
                  // mode for convolve and correlate N-D
    MYFLT *arg_b; // axis for convolve1d and correlate1d
                  // trig for convolve and correlate N-D
    MYFLT *arg_c; // trig for convolve1d and correlate1d
    // private
    CSN_ARRAY *array;
    K_DATA_FFT k_data_fft_x;
    K_DATA_FFT k_data_fft_h;
    K_DATA_FFT k_data_ifft;
    CSN_SCRATCH buffer_x;
    CSN_SCRATCH buffer_h;
    CSN_SCRATCH buffer_ifft;
    CSN_ARRAY fft_buffer_x;
    CSN_ARRAY fft_buffer_h;
    CSN_ARRAY ifft_buffer;
    CSN_ARRAY ifft_out;
    CSN_ARRAY x_padded;
    CSN_ARRAY h_padded;
    CSN_FFTCORRCONV_DATA fc;
    K_DATA k_data;
    bool is_published;
} CSN_CORRCONV;


int32_t csnarray_fft_deinit(CSOUND *csound, CSN_FFT *p);
int32_t csnarray_fft2_deinit(CSOUND *csound, CSN_FFT2 *p);
int32_t csnarray_stft_deinit(CSOUND *csound, CSN_STFT *p);
int32_t csnarray_istft_deinit(CSOUND *csound, CSN_ISTFT *p);
int32_t csnarray_fftfreq_deinit(CSOUND *csound, CSN_FFTFREQ *p);
int32_t csnarray_fftshift_deinit(CSOUND *csound, CSN_FFTSHIFT*p);

int32_t csnarray_fft(CSOUND *csound, CSN_FFT *p);
int32_t csnarray_rfft(CSOUND *csound, CSN_FFT *p);
int32_t csnarray_ifft(CSOUND *csound, CSN_FFT *p);
int32_t csnarray_irfft(CSOUND *csound, CSN_FFT *p);
int32_t csnarray_fft2(CSOUND *csound, CSN_FFT2 *p);
int32_t csnarray_rfft2(CSOUND *csound, CSN_FFT2 *p);
int32_t csnarray_ifft2(CSOUND *csound, CSN_FFT2 *p);
int32_t csnarray_irfft2(CSOUND *csound, CSN_FFT2 *p);
int32_t csnarray_stft(CSOUND *csound, CSN_STFT *p);
int32_t csnarray_istft(CSOUND *csound, CSN_ISTFT *p);
int32_t csnarray_fftfreq(CSOUND *csound, CSN_FFTFREQ *p);
int32_t csnarray_rfftfreq(CSOUND *csound, CSN_FFTFREQ *p);
int32_t csnarray_fftshift(CSOUND *csound, CSN_FFTSHIFT *p);
int32_t csnarray_ifftshift(CSOUND *csound, CSN_FFTSHIFT *p);

int32_t csnarray_fft_k(CSOUND *csound, CSN_FFT *p);
int32_t csnarray_rfft_k(CSOUND *csound, CSN_FFT *p);
int32_t csnarray_ifft_k(CSOUND *csound, CSN_FFT *p);
int32_t csnarray_irfft_k(CSOUND *csound, CSN_FFT *p);
int32_t csnarray_fft2_k(CSOUND *csound, CSN_FFT2 *p);
int32_t csnarray_rfft2_k(CSOUND *csound, CSN_FFT2 *p);
int32_t csnarray_ifft2_k(CSOUND *csound, CSN_FFT2 *p);
int32_t csnarray_irfft2_k(CSOUND *csound, CSN_FFT2 *p);
int32_t csnarray_stft_k(CSOUND *csound, CSN_STFT *p);
int32_t csnarray_istft_k(CSOUND *csound, CSN_ISTFT *p);
int32_t csnarray_fftfreq_k_init(CSOUND *csound, CSN_FFTFREQ *p);
int32_t csnarray_rfftfreq_k_init(CSOUND *csound, CSN_FFTFREQ *p);
int32_t csnarray_fftfreq_k(CSOUND *csound, CSN_FFTFREQ *p);
int32_t csnarray_rfftfreq_k(CSOUND *csound, CSN_FFTFREQ *p);
int32_t csnarray_fftshift_k_init(CSOUND *csound, CSN_FFTSHIFT *p);
int32_t csnarray_ifftshift_k_init(CSOUND *csound, CSN_FFTSHIFT *p);
int32_t csnarray_fftshift_k(CSOUND *csound, CSN_FFTSHIFT *p);
int32_t csnarray_ifftshift_k(CSOUND *csound, CSN_FFTSHIFT *p);

int32_t csnarray_fftconvolve1d(CSOUND *csound, CSN_CORRCONV *p);
int32_t csnarray_fftcorrelate1d(CSOUND *csound, CSN_CORRCONV *p);
int32_t csnarray_fftconvolve(CSOUND *csound, CSN_CORRCONV *p);
int32_t csnarray_fftcorrelate(CSOUND *csound, CSN_CORRCONV *p);

int32_t csnarray_fftcorrconv1d_k_init(CSOUND *csound, CSN_CORRCONV *p);
int32_t csnarray_fftconvolve1d_k(CSOUND *csound, CSN_CORRCONV *p);
int32_t csnarray_fftcorrelate1d_k(CSOUND *csound, CSN_CORRCONV *p);
int32_t csnarray_fftcorrconv_k_init(CSOUND *csound, CSN_CORRCONV *p);
int32_t csnarray_fftconvolve_k(CSOUND *csound, CSN_CORRCONV *p);
int32_t csnarray_fftcorrelate_k(CSOUND *csound, CSN_CORRCONV *p);

int32_t csnarray_corrconv_deinit(CSOUND *csound, CSN_CORRCONV *p);
int32_t csnarray_convolve1d(CSOUND *csound, CSN_CORRCONV *p); // 1d convolution by axes
int32_t csnarray_correlate1d(CSOUND *csound, CSN_CORRCONV *p); // 1d correlation by axes
int32_t csnarray_convolve(CSOUND *csound, CSN_CORRCONV *p); // N-D convolution
int32_t csnarray_correlate(CSOUND *csound, CSN_CORRCONV *p); // N-D correlation

int32_t csnarray_convolve1d_k(CSOUND *csound, CSN_CORRCONV *p);
int32_t csnarray_correlate1d_k(CSOUND *csound, CSN_CORRCONV *p);
int32_t csnarray_convolve_k(CSOUND *csound, CSN_CORRCONV *p);
int32_t csnarray_correlate_k(CSOUND *csound, CSN_CORRCONV *p);

#endif
