#ifndef __CSN_LINALG
#define __CSN_LINALG

#include "csnregistry.h"
#include "csnum.h"
#include <csdl.h>
#include <stddef.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>


typedef struct {
    size_t size;
    size_t *pivots;
    int32_t parity;
    double tolerance;
    bool singular;
} CSN_LU_INFO;

typedef struct {
    OPDS h;
    // outputs
    CSN_ARRAY *handle;
    // inputs
    CSN_ARRAY *source_handle_a;
    CSN_ARRAY *source_handle_b;
    cs_float *trig;
    // private
    K_DATA k_data;
    CSN_SCRATCH buffer_a;
    CSN_SCRATCH buffer_b;
    CSN_LU_INFO lu_info;
    bool is_published;
} CSN_LINALG_SOLVE;

typedef struct {
    OPDS h;
    // outputs
    CSN_ARRAY *handle;
    // inputs
    CSN_ARRAY *source_handle;
    cs_float *trig;
    // private
    K_DATA k_data;
    CSN_SCRATCH buffer_a;
    CSN_SCRATCH buffer_b;
    CSN_LU_INFO lu_info;
    bool is_published;
} CSN_LINALG_INVERSE;

typedef struct {
    OPDS h;
    // outputs
    void *det;
    // inputs
    CSN_ARRAY *source_handle;
    cs_float *trig;
    // private
    K_DATA k_data;
    CSN_SCRATCH buffer;
    CSN_LU_INFO lu_info;
    bool is_published;
} CSN_LINALG_DETERMINANT_COMMON;

typedef struct {
    OPDS h;
    // outputs
    cs_float *det;
    // inputs
    CSN_ARRAY *source_handle;
    cs_float *trig;
    // private
    K_DATA k_data;
    CSN_SCRATCH buffer;
    CSN_LU_INFO lu_info;
    double prev_det_real;
    CSN_COMPLEXDAT prev_det_complex;
    bool is_published;
} CSN_LINALG_DET_REAL;

typedef struct {
    OPDS h;
    // outputs
    COMPLEXDAT *det;
    // inputs
    CSN_ARRAY *source_handle;
    cs_float *trig;
    // private
    K_DATA k_data;
    CSN_SCRATCH buffer;
    CSN_LU_INFO lu_info;
    double prev_det_real;
    CSN_COMPLEXDAT prev_det_complex;
    bool is_published;
} CSN_LINALG_DET_COMPLEX;

typedef struct {
    double *data;
    double *transposed;
    double *normal;
    double *inversed;
    double *pinversed;
} CSN_SAVGOL_TEMP_BUFFER;

/* Savitzky-Golay coefficient matrix C = (A^T A)^-1 A^T, row-major.
   Row d holds the least-squares coefficients of the d-th derivative,
   before the d! / delta^d scaling applied by get_coeffs(). */
typedef struct {
    double *coeffs;
    uint32_t nrows; // order + 1
    uint32_t ncols; // winsize
} CSN_SAVGOL_BUFFER;

typedef struct {
    OPDS h;
    // outputs
    CSN_ARRAY *handle; // savgol mat
    // inputs;
    cs_float *winsize;
    cs_float *order;
    cs_float *delta;
} CSN_SAVGOL_MATRIX;

typedef struct {
    OPDS h;
    // outputs
    CSN_ARRAY *handle;
    // inputs
    cs_float *n;
    cs_float *trig;
    // private
    K_DATA k_data;
    bool is_published;
} CSN_HILBERT_MAT;

int32_t lu_factor_real(double *a, size_t n, CSN_LU_INFO *info);
int32_t lu_solve_real(const double *lu, const CSN_LU_INFO *info, double *b, size_t nrhs);
int32_t lu_factor_complex(double *a, size_t n, CSN_LU_INFO *info);
int32_t lu_solve_complex(const double *lu, const CSN_LU_INFO *info, double *b, size_t nrhs);

int32_t csnarray_solve_deinit(CSOUND *csound, CSN_LINALG_SOLVE *p);
int32_t csnarray_solve(CSOUND *csound, CSN_LINALG_SOLVE *p);
int32_t csnarray_solve_k(CSOUND *csound, CSN_LINALG_SOLVE *p);
int32_t csnarray_inverse_deinit(CSOUND *csound, CSN_LINALG_INVERSE *p);
int32_t csnarray_inverse(CSOUND *csound, CSN_LINALG_INVERSE *p);
int32_t csnarray_inverse_k(CSOUND *csound, CSN_LINALG_INVERSE *p);
int32_t csnarray_det_real_deinit(CSOUND *csound, CSN_LINALG_DET_REAL *p);
int32_t csnarray_det_complex_deinit(CSOUND *csound, CSN_LINALG_DET_COMPLEX *p);
int32_t csnarray_determinant_real(CSOUND *csound, CSN_LINALG_DET_REAL *p);
int32_t csnarray_determinant_real_k(CSOUND *csound, CSN_LINALG_DET_REAL *p);
int32_t csnarray_determinant_complex(CSOUND *csound, CSN_LINALG_DET_COMPLEX *p);
int32_t csnarray_determinant_complex_k(CSOUND *csound, CSN_LINALG_DET_COMPLEX *p);

int32_t csnarray_savgol_mat(CSOUND *csound, CSN_SAVGOL_MATRIX *p); // savgol i-rate

int32_t csnarray_hilbertmat(CSOUND *csound, CSN_HILBERT_MAT *p);
int32_t csnarray_hilbertmat_k_init(CSOUND *csound, CSN_HILBERT_MAT *p);
int32_t csnarray_hilbertmat_k(CSOUND *csound, CSN_HILBERT_MAT *p);

#endif
