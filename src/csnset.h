#ifndef __CSN_SET
#define __CSN_SET

#include "csnum.h"
#include <csound.h>

typedef struct {
    OPDS h;
    // outputs
    CSNREF *handle;
    // inputs
    CSNREF *source_handle;
    MYFLT *trig;
    // private
    CSN_ARRAY *array;
    K_DATA k_data;
    CSN_SCRATCH buffer;
    bool is_published;
} CSNSET_UNARYOP;

typedef struct {
    OPDS h;
    // inputs
    CSNREF *source_handle;
    MYFLT *arg_a; // trig in unlikeset_k
                  // scalar in insert, remove
    MYFLT *arg_b; // trig in insert and remove
    // private
    K_DATA k_data;
    bool is_published;
} CSNSET_UNARYOP_IN;

typedef struct {
    OPDS h;
    // outputs
    MYFLT *value;
    // inputs
    CSNREF *source_handle;
    MYFLT *scalar;
    MYFLT *trig;
    // private
    K_DATA k_data;
    bool is_published;
    bool prev_result;
} CSNSET_BINARYOP_SCALAR;

typedef struct {
    OPDS h;
    // outputs
    CSNREF *handle;
    // inputs
    CSNREF *source_handle_a;
    CSNREF *source_handle_b;
    MYFLT *trig;
    // private
    CSN_ARRAY *array;
    K_DATA k_data;
    CSN_SCRATCH buffer;
    bool is_published;
} CSNSET_BINARYOP;

typedef struct {
    OPDS h;
    // outputs
    MYFLT *result;
    // inputs
    CSNREF *source_handle_a;
    CSNREF *source_handle_b;
    MYFLT *trig;
    // private
    CSN_REGISTRY *registry;
    ARRAY_VERSION prev_source_version_a;
    ARRAY_VERSION prev_source_version_b;
    CSN_SCRATCH buffer;
    bool is_published;
    double prev_result;
} CSNSET_BINARYOP_PREDICATE;


void binary_search(size_t *index, bool *founded, size_t low, const double *data, double value, size_t size, bool right_side);

int32_t csnarray_set_binaryop_deinit(CSOUND *csound, CSNSET_BINARYOP *p);
int32_t csnarray_set_binaryop_p_deinit(CSOUND *csound, CSNSET_BINARYOP_PREDICATE *p);
int32_t csnarray_likeset_deinit(CSOUND *csound, CSNSET_UNARYOP *p);
int32_t csnarray_likeset(CSOUND *csound, CSNSET_UNARYOP *p);
int32_t csnarray_unlikeset(CSOUND *csound, CSNSET_UNARYOP_IN *p);
int32_t csnarray_setinsert(CSOUND *csound, CSNSET_UNARYOP_IN *p);
int32_t csnarray_setremove(CSOUND *csound, CSNSET_UNARYOP_IN *p);
int32_t csnarray_setcontains(CSOUND *csound, CSNSET_BINARYOP_SCALAR *p);
int32_t csnarray_setunion(CSOUND *csound, CSNSET_BINARYOP *p);
int32_t csnarray_setintersect(CSOUND *csound, CSNSET_BINARYOP *p);
int32_t csnarray_setdiff(CSOUND *csound, CSNSET_BINARYOP *p);
int32_t csnarray_setsymdiff(CSOUND *csound, CSNSET_BINARYOP *p);
int32_t csnarray_setissubset(CSOUND *csound, CSNSET_BINARYOP_PREDICATE *p);
int32_t csnarray_setissuperset(CSOUND *csound, CSNSET_BINARYOP_PREDICATE *p);
int32_t csnarray_setisdisjoint(CSOUND *csound, CSNSET_BINARYOP_PREDICATE *p);
int32_t csnarray_setisequal(CSOUND *csound, CSNSET_BINARYOP_PREDICATE *p);

int32_t csnarray_likeset_k(CSOUND *csound, CSNSET_UNARYOP *p);
int32_t csnarray_likeset_k_init(CSOUND *csound, CSNSET_UNARYOP *p);
int32_t csnarray_unlikeset_k_init(CSOUND *csound, CSNSET_UNARYOP_IN *p);
int32_t csnarray_unlikeset_k(CSOUND *csound, CSNSET_UNARYOP_IN *p);
int32_t csnarray_setinsertremove_k_init(CSOUND *csound, CSNSET_UNARYOP_IN *p);
int32_t csnarray_setinsert_k(CSOUND *csound, CSNSET_UNARYOP_IN *p);
int32_t csnarray_setremove_k(CSOUND *csound, CSNSET_UNARYOP_IN *p);
int32_t csnarray_setcontains_k_init(CSOUND *csound, CSNSET_BINARYOP_SCALAR *p);
int32_t csnarray_setcontains_k(CSOUND *csound, CSNSET_BINARYOP_SCALAR *p);
int32_t csnarray_setunion_k(CSOUND *csound, CSNSET_BINARYOP *p);
int32_t csnarray_setintersect_k(CSOUND *csound, CSNSET_BINARYOP *p);
int32_t csnarray_setdiff_k(CSOUND *csound, CSNSET_BINARYOP *p);
int32_t csnarray_setsymdiff_k(CSOUND *csound, CSNSET_BINARYOP *p);
int32_t csnarray_setissubset_k(CSOUND *csound, CSNSET_BINARYOP_PREDICATE *p);
int32_t csnarray_setissuperset_k(CSOUND *csound, CSNSET_BINARYOP_PREDICATE *p);
int32_t csnarray_setisdisjoint_k(CSOUND *csound, CSNSET_BINARYOP_PREDICATE *p);
int32_t csnarray_setisequal_k(CSOUND *csound, CSNSET_BINARYOP_PREDICATE *p);


#endif
