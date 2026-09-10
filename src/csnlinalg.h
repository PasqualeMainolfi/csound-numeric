#ifndef __CSN_LINALG
#define __CSN_LINALG

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

int32_t lu_factor_real(double *a, size_t n, CSN_LU_INFO *info);
int32_t lu_solve_real(const double *lu, const CSN_LU_INFO *info, double *b, size_t nrhs);
int32_t lu_factor_complex(double *a, size_t n, CSN_LU_INFO *info);
int32_t lu_solve_complex(const double *lu, const CSN_LU_INFO *info, double *b, size_t nrhs);

#endif
