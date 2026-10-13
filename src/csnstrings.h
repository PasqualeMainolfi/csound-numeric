#ifndef __CSN_STRINGS_H
#define __CSN_STRINGS_H

#include "csdl.h"
#include "csnregistry.h"
#include "csnum.h"


typedef struct {
    OPDS h;
    // outputs
    CSN_ARRAY *handle;
    // inputs
    ARRAYDAT *shape;
    // private
    CSN_REGISTRY *registry;
} CSN_STR_EMPTY;

typedef struct {
    OPDS h;
    // outputs
    CSN_ARRAY *handle;
    // inputs
    ARRAYDAT *shape;
    STRINGDAT *value;
    // private
    CSN_REGISTRY *registry;
} CSN_FULLSTR;

typedef struct {
    OPDS h;
    // outputs
    CSN_ARRAY *handle;
    // inputs
    CSN_ARRAY *handle_from;
    STRINGDAT *value; // value for fill
    cs_float *trig;   // k form only
    // private
    CSN_REGISTRY *registry;
} CSN_STR_INIT_LIKE;

typedef struct {
    OPDS h;
    // ouputs
    STRINGDAT *value;
    // inputs
    CSN_ARRAY *source_handle;
    ARRAYDAT *indexes; // get -> indexes must be the same as dims
    // private
    CSN_REGISTRY *registry;
} CSN_GETSTRING;

typedef struct {
    OPDS h;
    // inputs
    CSN_ARRAY *source_handle;
    ARRAYDAT *indexes; // set -> indexes must be the same as dims
    STRINGDAT *value;
    // private
    CSN_REGISTRY *registry;
} CSN_SETSTRING;

/* v[i][j]: Csound passes each index as its own argument. */
typedef struct {
    OPDS h;
    // outputs
    STRINGDAT *value;
    // inputs
    CSN_ARRAY *source_handle;
    cs_float *idx[VARGMAX]; // one per index; Csound writes a pointer for each one given
    // private
    CSN_REGISTRY *registry;
} CSN_IDX_GETSTRING;

/* v[i][j] = x and v[i][j] init x */
typedef struct {
    OPDS h;
    // inputs
    CSN_ARRAY *source_handle;
    STRINGDAT *value;
    cs_float *idx[VARGMAX];
    // private
    CSN_REGISTRY *registry;
} CSN_IDX_SETSTRING;


int32_t create_empty_csnstrings(CSOUND *csound, CSN_STR_EMPTY *p);
int32_t create_empty_csnstrings_k_init(CSOUND *csound, CSN_STR_EMPTY *p);
int32_t create_empty_csnstrings_k(CSOUND *csound, CSN_STR_EMPTY *p);
int32_t create_fullstr_csnarray(CSOUND *csound, CSN_FULLSTR *p);
int32_t create_fullstr_csnarray_k_init(CSOUND *csound, CSN_FULLSTR *p);
int32_t create_fullstr_csnarray_k(CSOUND *csound, CSN_FULLSTR *p);
int32_t create_like_csnstrings(CSOUND *csound, CSN_STR_INIT_LIKE *p);
int32_t create_like_csnstrings_k_init(CSOUND *csound, CSN_STR_INIT_LIKE *p);
int32_t create_like_csnstrings_k(CSOUND *csound, CSN_STR_INIT_LIKE *p);

int32_t csnstrings_get(CSOUND *csound, CSN_GETSTRING *p);
int32_t csnstrings_get_k(CSOUND *csound, CSN_GETSTRING *p);
int32_t csnstrings_set(CSOUND *csound, CSN_SETSTRING *p);
int32_t csnstrings_set_k(CSOUND *csound, CSN_SETSTRING *p);

int32_t csnidxstrings_get(CSOUND *csound, CSN_IDX_GETSTRING *p);
int32_t csnidxstrings_get_k_init(CSOUND *csound, CSN_IDX_GETSTRING *p);
int32_t csnidxstrings_get_k(CSOUND *csound, CSN_IDX_GETSTRING *p);
int32_t csnidxstrings_set(CSOUND *csound, CSN_IDX_SETSTRING *p);
int32_t csnidxstrings_set_k_init(CSOUND *csound, CSN_IDX_SETSTRING *p);
int32_t csnidxstrings_set_k(CSOUND *csound, CSN_IDX_SETSTRING *p);

int32_t from_array_to_csnstrings(CSOUND *csound, CSN_FROM_ARRAY *p);
int32_t from_array_to_csnstrings_k_init(CSOUND *csound, CSN_FROM_ARRAY *p);
int32_t from_array_to_csnstrings_k(CSOUND *csound, CSN_FROM_ARRAY *p);
int32_t from_csnstrings_to_array(CSOUND *csound, CSN_TO_ARRAY *p);
int32_t from_csnstrings_to_array_k(CSOUND *csound, CSN_TO_ARRAY *p);

int32_t csnload_strings(CSOUND *csound, CSN_LOAD *p);
int32_t csnload_strings_k_init(CSOUND *csound, CSN_LOAD *p);
int32_t csnload_strings_k(CSOUND *csound, CSN_LOAD *p);

#endif
