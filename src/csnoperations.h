#ifndef __CSN_OPERATIONS
#define __CSN_OPERATIONS

#include "csnregistry.h"
#include "csnum.h"
#include <csdl.h>
#include <stdbool.h>
#include <stdint.h>

/* The orchestra operators on CsnArr. Csound compiles A + B into the opcode
   ##add, A += B into ##addin, -A into ##mul(-1, A) and X = A into =.CsnArr, and
   picks the entry by argument types. CsnArr has no rate, so every entry has an
   init and a perf pass, and the perf pass works only while an operand is
   k-rate (CSN_ARRAY.rate): an i-rate expression is computed once, at init. */

/* out = a op b */
typedef struct {
    OPDS h;
    // outputs
    CSN_ARRAY *out;
    // inputs
    CSN_ARRAY *a;
    CSN_ARRAY *b;
    // private
    K_DATA k_data;
} CSN_OP_HH;

/* out = a op s */
typedef struct {
    OPDS h;
    // outputs
    CSN_ARRAY *out;
    // inputs
    CSN_ARRAY *a;
    cs_float *s;
    // private
    K_DATA k_data;
    bool scalar_k; // the k overload was picked: the scalar changes at perf
} CSN_OP_HS;

/* out = s op a */
typedef struct {
    OPDS h;
    // outputs
    CSN_ARRAY *out;
    // inputs
    cs_float *s;
    CSN_ARRAY *a;
    // private
    K_DATA k_data;
    bool scalar_k;
} CSN_OP_SH;

/* out = a op z, with a :Complex; scalar. Complex carries no rate either, so
   these run at perf when the scalar has changed since the last pass. */
typedef struct {
    OPDS h;
    // outputs
    CSN_ARRAY *out;
    // inputs
    CSN_ARRAY *a;
    COMPLEXDAT *z;
    // private
    K_DATA k_data;
    CSN_COMPLEXDAT last_z;
} CSN_OP_HC;

/* out = z op a */
typedef struct {
    OPDS h;
    // outputs
    CSN_ARRAY *out;
    // inputs
    COMPLEXDAT *z;
    CSN_ARRAY *a;
    // private
    K_DATA k_data;
    CSN_COMPLEXDAT last_z;
} CSN_OP_CH;

/* x op= y */
typedef struct {
    OPDS h;
    // outputs (read as well: the left operand)
    CSN_ARRAY *x;
    // inputs
    CSN_ARRAY *y;
    // private
    K_DATA k_data;
} CSN_OP_IN_HH;

/* x op= s */
typedef struct {
    OPDS h;
    // outputs (read as well: the left operand)
    CSN_ARRAY *x;
    // inputs
    cs_float *s;
    // private
    K_DATA k_data;
    bool scalar_k;
} CSN_OP_IN_HS;

/* out = in */
typedef struct {
    OPDS h;
    // outputs
    CSN_ARRAY *out;
    // inputs
    CSN_ARRAY *in;
    // private
    CSN_REGISTRY *registry;
    ARRAY_VERSION prev_in_version;
    ARRAY_VERSION prev_out_version;
    bool is_published;
} CSN_OP_ASSIGN;

/* out = a op s */
typedef struct {
    OPDS h;
    // outputs
    CSN_ARRAY *out;
    // inputs
    CSN_ARRAY *a;
    STRINGDAT *s;
    // private
    K_DATA k_data;
} CSN_OP_STRING_HS;

/* out = s op a */
typedef struct {
    OPDS h;
    // outputs
    CSN_ARRAY *out;
    // inputs
    STRINGDAT *s;
    CSN_ARRAY *a;
    // private
    K_DATA k_data;
} CSN_OP_STRING_SH;

typedef struct {
    OPDS h;
    // outputs (read as well: the left operand)
    CSN_ARRAY *x;
    // inputs
    STRINGDAT *s;
    // private
    K_DATA k_data;
} CSN_OP_STRING_IN_HS;

typedef struct {
    OPDS h;
    // outputs
    STRINGDAT *out;
    // inputs
    STRINGDAT *a;
    STRINGDAT *b;
    // private
    CSN_STRDATA buffer;
} CSN_OP_STRADD_SS;

typedef struct {
    OPDS h;
    // outputs
    STRINGDAT *a;
    // inputs
    STRINGDAT *b;
    // private
    CSN_STRDATA buffer;
} CSN_OP_STRADD_SS_IN;

typedef struct {
    OPDS h;
    // outputs
    STRINGDAT *out;
    // inputs
    STRINGDAT *a;
    cs_float *value;
    // private
    CSN_STRDATA buffer;
} CSN_OP_STRMUL_SS;

typedef struct {
    OPDS h;
    // outputs
    STRINGDAT *a;
    // inputs
    cs_float *value;
    // private
    CSN_STRDATA buffer;
} CSN_OP_STRMUL_SS_IN;

#define CSN_OP_DECLARE(name)                                                   \
    int32_t csnop_##name##_hh_init(CSOUND *csound, CSN_OP_HH *p);              \
    int32_t csnop_##name##_hh_perf(CSOUND *csound, CSN_OP_HH *p);              \
    int32_t csnop_##name##_hi_init(CSOUND *csound, CSN_OP_HS *p);              \
    int32_t csnop_##name##_hk_init(CSOUND *csound, CSN_OP_HS *p);              \
    int32_t csnop_##name##_hs_perf(CSOUND *csound, CSN_OP_HS *p);              \
    int32_t csnop_##name##_ih_init(CSOUND *csound, CSN_OP_SH *p);              \
    int32_t csnop_##name##_kh_init(CSOUND *csound, CSN_OP_SH *p);              \
    int32_t csnop_##name##_sh_perf(CSOUND *csound, CSN_OP_SH *p);              \
    int32_t csnop_##name##_hc_init(CSOUND *csound, CSN_OP_HC *p);              \
    int32_t csnop_##name##_hc_perf(CSOUND *csound, CSN_OP_HC *p);              \
    int32_t csnop_##name##_ch_init(CSOUND *csound, CSN_OP_CH *p);              \
    int32_t csnop_##name##_ch_perf(CSOUND *csound, CSN_OP_CH *p);

#define CSN_OP_IN_DECLARE(name)                                                \
    int32_t csnop_##name##in_hh_init(CSOUND *csound, CSN_OP_IN_HH *p);         \
    int32_t csnop_##name##in_hh_perf(CSOUND *csound, CSN_OP_IN_HH *p);         \
    int32_t csnop_##name##in_hi_init(CSOUND *csound, CSN_OP_IN_HS *p);         \
    int32_t csnop_##name##in_hk_init(CSOUND *csound, CSN_OP_IN_HS *p);         \
    int32_t csnop_##name##in_hs_perf(CSOUND *csound, CSN_OP_IN_HS *p);

CSN_OP_DECLARE(add)
CSN_OP_DECLARE(sub)
CSN_OP_DECLARE(mul)
CSN_OP_DECLARE(div)
CSN_OP_DECLARE(pow)

CSN_OP_IN_DECLARE(add)
CSN_OP_IN_DECLARE(sub)
CSN_OP_IN_DECLARE(mul)
CSN_OP_IN_DECLARE(div)

int32_t op_assign_run(CSOUND *csound, CSN_OP_ASSIGN *p, OPDS *perf_h);
int32_t csnop_assign_init(CSOUND *csound, CSN_OP_ASSIGN *p);
int32_t csnop_assign_perf(CSOUND *csound, CSN_OP_ASSIGN *p);

// cast in an expression: csnk(A), csni(A)
int32_t csnarray_cast_k_init(CSOUND *csound, CSN_OP_ASSIGN *p);
int32_t csnarray_cast_k_perf(CSOUND *csound, CSN_OP_ASSIGN *p);
int32_t csnarray_cast_i_init(CSOUND *csound, CSN_OP_ASSIGN *p);

// strings
int32_t opstr_add_hh_init(CSOUND *csound, CSN_OP_HH *p);
int32_t opstr_add_hh_perf(CSOUND *csound, CSN_OP_HH *p);
int32_t opstr_add_hs_init(CSOUND *csound, CSN_OP_STRING_HS *p);
int32_t opstr_add_hs_perf(CSOUND *csound, CSN_OP_STRING_HS *p);
int32_t opstr_add_sh_init(CSOUND *csound, CSN_OP_STRING_SH *p);
int32_t opstr_add_sh_perf(CSOUND *csound, CSN_OP_STRING_SH *p);
int32_t opstr_add_hs_deinit(CSOUND *csound, CSN_OP_STRING_HS *p);
int32_t opstr_add_sh_deinit(CSOUND *csound, CSN_OP_STRING_SH *p);
int32_t opstr_mul_hsi_init(CSOUND *csound, CSN_OP_HS *p);
int32_t opstr_mul_hsk_init(CSOUND *csound, CSN_OP_HS *p);
int32_t opstr_mul_hs_perf(CSOUND *csound, CSN_OP_HS *p);
int32_t opstr_mul_shi_init(CSOUND *csound, CSN_OP_SH *p);
int32_t opstr_mul_shk_init(CSOUND *csound, CSN_OP_SH *p);
int32_t opstr_mul_sh_perf(CSOUND *csound, CSN_OP_SH *p);
int32_t opstr_addin_hh_init(CSOUND *csound, CSN_OP_IN_HH *p);
int32_t opstr_addin_hh_perf(CSOUND *csound, CSN_OP_IN_HH *p);
int32_t opstr_addin_hs_init(CSOUND *csound, CSN_OP_STRING_IN_HS *p);
int32_t opstr_addin_hs_perf(CSOUND *csound, CSN_OP_STRING_IN_HS *p);
int32_t opstr_addin_hs_deinit(CSOUND *csound, CSN_OP_STRING_IN_HS *p);
int32_t opstr_mulin_hi_init(CSOUND *csound, CSN_OP_IN_HS *p);
int32_t opstr_mulin_hk_init(CSOUND *csound, CSN_OP_IN_HS *p);
int32_t opstr_mulin_hs_perf(CSOUND *csound, CSN_OP_IN_HS *p);

int32_t opstr_add_ss_deinit(CSOUND *csound, CSN_OP_STRADD_SS *p);
int32_t opstr_addin_ss_deinit(CSOUND *csound, CSN_OP_STRADD_SS_IN *p);
int32_t opstr_mul_ss_deinit(CSOUND *csound, CSN_OP_STRMUL_SS *p);
int32_t opstr_mulin_ss_deinit(CSOUND *csound, CSN_OP_STRMUL_SS_IN *p);

int32_t opstr_add_ss_init(CSOUND *csound, CSN_OP_STRADD_SS *p);
int32_t opstr_add_ss_perf(CSOUND *csound, CSN_OP_STRADD_SS *p);
int32_t opstr_addin_ss_init(CSOUND *csound, CSN_OP_STRADD_SS_IN *p);
int32_t opstr_addin_ss_perf(CSOUND *csound, CSN_OP_STRADD_SS_IN *p);
int32_t opstr_mul_ss_init(CSOUND *csound, CSN_OP_STRMUL_SS *p);
int32_t opstr_mul_ss_perf(CSOUND *csound, CSN_OP_STRMUL_SS *p);
int32_t opstr_mulin_ss_init(CSOUND *csound, CSN_OP_STRMUL_SS_IN *p);
int32_t opstr_mulin_ss_perf(CSOUND *csound, CSN_OP_STRMUL_SS_IN *p);

#endif
