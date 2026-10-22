#ifndef __CSN_BINARY_H
#define __CSN_BINARY_H

#include "csnregistry.h"
#include "csnum.h"
#include <csdl.h>
#include <stdint.h>


typedef enum {
    CSN_UINT8 = 0,
    CSN_INT8,
    CSN_UINT16LE,
    CSN_UINT16BE,
    CSN_INT16LE,
    CSN_INT16BE,
    CSN_UINT32LE,
    CSN_UINT32BE,
    CSN_INT32LE,
    CSN_INT32BE,
    CSN_FLOAT32LE,
    CSN_FLOAT32BE,
    CSN_FLOAT64LE,
    CSN_FLOAT64BE
} CSN_BINDTYPE;

typedef struct {
    OPDS h;
    // outputs
    STRINGDAT *string_out;
    // inputs
    cs_float *value;
    cs_float *width; // i-rate only (minimum number of chars)
    cs_float *trig;
    // private
    double prev_value; // NaN until the first value is written
    CSN_STRDATA buffer;
} CSN_TOBINHEX;

typedef struct {
    OPDS h;
    // outputs
    cs_float *value;
    // inputs
    STRINGDAT *string_in;
    cs_float *trig;
} CSN_FROMBINHEX;

typedef struct {
    OPDS h;
    // outputs
    CSN_ARRAY *handle; // -> CsnStr in to/ -> CsnArr in from
    // inputs
    CSN_ARRAY *source_handle; // -> CsnArr in to/ -> CsnStr in from
    cs_float *arg_a; // width in tobinary/tohex (i-rate only)
                     // trig in frombinary/fromhex
    cs_float *arg_b; // trig in tobinary/tohex
    // private
    K_DATA k_data;
} CSN_BINHEX_ARR;

typedef struct {
    OPDS h;
    // outputs
    STRINGDAT *string_out;
    // inputs
    CSN_ARRAY *source_handle;
    cs_float *trig;
    // private
    K_DATA k_data;
    CSN_STRDATA buffer;
} CSN_TOBASE64;

typedef struct {
    OPDS h;
    // outputs
    STRINGDAT *string_out;
    // inputs
    STRINGDAT *string_in;
    cs_float *trig;
    // private
    CSN_STRDATA buffer;
} CSN_BASE64CONV_STR; // csntobase64 and csnfrombase64 from one S to one S

typedef struct {
    OPDS h;
    // outputs
    CSN_ARRAY *handle;
    // inputs
    STRINGDAT *string_in;
    cs_float *trig;
    // private
    K_DATA k_data;
} CSN_FROMBASE64;

typedef struct {
    OPDS h;
    // outputs
    CSN_ARRAY *handle; // CsnArr of 0/1 in binstrtoarr, CsnStr of digits in hexstrtoarr
    // inputs
    STRINGDAT *string_in;
    cs_float *trig;
    // private
    K_DATA k_data;
} CSN_STRTOARR;

typedef struct {
    OPDS h;
    // outputs
    CSN_ARRAY *handle;
    // inputs
    CSN_ARRAY *source_handle;
    STRINGDAT *dtype; // as numpy
    cs_float *trig;
    // private
    K_DATA k_data;
} CSN_BYTEBUFFER;


int32_t csnbinary_tobinhex_deinit(CSOUND *csound, CSN_TOBINHEX *p);

int32_t csnbinary_tobinary(CSOUND *csound, CSN_TOBINHEX *p);
int32_t csnbinary_tobinary_k_init(CSOUND *csound, CSN_TOBINHEX *p);
int32_t csnbinary_tobinary_k(CSOUND *csound, CSN_TOBINHEX *p);
int32_t csnbinary_tobinary_arr(CSOUND *csound, CSN_BINHEX_ARR *p);
int32_t csnbinary_tobinary_arr_k(CSOUND *csound, CSN_BINHEX_ARR *p);
int32_t csnbinary_frombinary(CSOUND *csound, CSN_FROMBINHEX *p);
int32_t csnbinary_frombinary_k(CSOUND *csound, CSN_FROMBINHEX *p);
int32_t csnbinary_frombinary_arr(CSOUND *csound, CSN_BINHEX_ARR *p);
int32_t csnbinary_frombinary_arr_k(CSOUND *csound, CSN_BINHEX_ARR *p);

int32_t csnbinary_tohex(CSOUND *csound, CSN_TOBINHEX *p);
int32_t csnbinary_tohex_k_init(CSOUND *csound, CSN_TOBINHEX *p);
int32_t csnbinary_tohex_k(CSOUND *csound, CSN_TOBINHEX *p);
int32_t csnbinary_tohex_arr(CSOUND *csound, CSN_BINHEX_ARR *p);
int32_t csnbinary_tohex_arr_k(CSOUND *csound, CSN_BINHEX_ARR *p);
int32_t csnbinary_fromhex(CSOUND *csound, CSN_FROMBINHEX *p);
int32_t csnbinary_fromhex_k(CSOUND *csound, CSN_FROMBINHEX *p);
int32_t csnbinary_fromhex_arr(CSOUND *csound, CSN_BINHEX_ARR *p);
int32_t csnbinary_fromhex_arr_k(CSOUND *csound, CSN_BINHEX_ARR *p);

int32_t csnbinary_tobase64_deinit(CSOUND *csound, CSN_TOBASE64 *p);
int32_t csnbinary_base64conv_deinit(CSOUND *csound, CSN_BASE64CONV_STR *p);

int32_t csnbinary_tobase64(CSOUND *csound, CSN_TOBASE64 *p);
int32_t csnbinary_tobase64_k_init(CSOUND *csound, CSN_TOBASE64 *p);
int32_t csnbinary_tobase64_k(CSOUND *csound, CSN_TOBASE64 *p);
int32_t csnbinary_tobase64_ss(CSOUND *csound, CSN_BASE64CONV_STR *p);
int32_t csnbinary_tobase64_ss_k_init(CSOUND *csound, CSN_BASE64CONV_STR *p);
int32_t csnbinary_tobase64_ss_k(CSOUND *csound, CSN_BASE64CONV_STR *p);
int32_t csnbinary_frombase64(CSOUND *csound, CSN_FROMBASE64 *p);
int32_t csnbinary_frombase64_k_init(CSOUND *csound, CSN_FROMBASE64 *p);
int32_t csnbinary_frombase64_k(CSOUND *csound, CSN_FROMBASE64 *p);
int32_t csnbinary_frombase64_ss(CSOUND *csound, CSN_BASE64CONV_STR *p);
int32_t csnbinary_frombase64_ss_k_init(CSOUND *csound, CSN_BASE64CONV_STR *p);
int32_t csnbinary_frombase64_ss_k(CSOUND *csound, CSN_BASE64CONV_STR *p);

int32_t csnbinary_tobuffer_deinit(CSOUND *csound, CSN_BYTEBUFFER *p);
int32_t csnbinary_tobuffer(CSOUND *csound, CSN_BYTEBUFFER *p);
int32_t csnbinary_tobuffer_k(CSOUND *csound, CSN_BYTEBUFFER *p);
int32_t csnbinary_frombuffer_deinit(CSOUND *csound, CSN_BYTEBUFFER *p);
int32_t csnbinary_frombuffer(CSOUND *csound, CSN_BYTEBUFFER *p);
int32_t csnbinary_frombuffer_k(CSOUND *csound, CSN_BYTEBUFFER *p);

int32_t csnbinary_strtoarr_deinit(CSOUND *csound, CSN_STRTOARR *p);

int32_t csnbinary_binstrtoarr(CSOUND *csound, CSN_STRTOARR *p);
int32_t csnbinary_binstrtoarr_k_init(CSOUND *csound, CSN_STRTOARR *p);
int32_t csnbinary_binstrtoarr_k(CSOUND *csound, CSN_STRTOARR *p);
int32_t csnbinary_hexstrtoarr(CSOUND *csound, CSN_STRTOARR *p);
int32_t csnbinary_hexstrtoarr_k_init(CSOUND *csound, CSN_STRTOARR *p);
int32_t csnbinary_hexstrtoarr_k(CSOUND *csound, CSN_STRTOARR *p);


#endif
