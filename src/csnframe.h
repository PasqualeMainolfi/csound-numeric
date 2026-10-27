#ifndef  __CSN_FRAME_H
#define __CSN_FRAME_H

#include "csnregistry.h"
#include "csnum.h"
#include <csdl.h>
#include <stdint.h>


typedef struct {
    OPDS h;
    // outputs
    CSN_ARRAY *handle;
    // inputs
    CSN_ARRAY *source_handle;
    cs_float *frame_size;
    cs_float *hop_size;
    cs_float *arg_a; // axis in the i-rate overload; trigger in the k-rate one
    cs_float *arg_b; // axis in the k-rate overload
    // private
    K_DATA k_data;
} CSN_FRAME;

typedef struct {
    OPDS h;
    // outputs
    CSN_ARRAY *handle;
    // inputs
    CSN_ARRAY *source_handle;
    cs_float *arg_a; // trig in zcr k-rate
                     // axis in zcr i-rate
                     // maxlag in amdf, i-rate in both forms (it sets the output shape)
    cs_float *arg_b; // axis in zcr k-rate
                     // trig in amdf k-rate
                     // axis in amdf i-rate
    cs_float *arg_c; // axis in amdf k-rate
    // private
    K_DATA k_data;
} CSN_TFEATURE;

typedef struct {
    OPDS h;
    // outputs
    cs_float *value;
    // inputs
    CSN_ARRAY *source_handle;
    cs_float *trig;
    // private
    K_DATA k_data;
} CSN_TFEATURE_SCALAR;

typedef struct {
    OPDS h;
    // outputs
    CSN_ARRAY *handle;
    // inputs
    CSN_ARRAY *source_handle;
    cs_float *n_perm;
    cs_float *trig;
    // private
    K_DATA k_data;
    CSN_SCRATCH buffer;
} CSN_PERM;



int32_t csnframe_framing(CSOUND *csound, CSN_FRAME *p);
int32_t csnframe_framing_k(CSOUND *csound, CSN_FRAME *p);

int32_t csnframe_zcr(CSOUND *csound, CSN_TFEATURE *p);
int32_t csnframe_zcr_k(CSOUND *csound, CSN_TFEATURE *p);
int32_t csnframe_amdf(CSOUND *csound, CSN_TFEATURE *p);
int32_t csnframe_amdf_k(CSOUND *csound, CSN_TFEATURE *p);

int32_t csnframe_zcr_scalar(CSOUND *csound, CSN_TFEATURE_SCALAR *p);
int32_t csnframe_zcr_scalar_k(CSOUND *csound, CSN_TFEATURE_SCALAR *p);

int32_t csnframe_perm_deinit(CSOUND *csound, CSN_PERM *p);
int32_t csnframe_perm(CSOUND *csound, CSN_PERM *p);
int32_t csnframe_perm_k(CSOUND *csound, CSN_PERM *p);

#endif
