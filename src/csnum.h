#ifndef __CSNUM_H
#define __CSNUM_H

#include <csdl.h>
#include <stdint.h>
#include "csnregistry.h"


/*
1. CREATION:
    X empty
    X zeros
    X ones
    X full
    X empty_like
    X zeros_like
    X ones_like
    X full_like
    X arange
    X linspace
    X logspace
    X geomspace
    X identity (see matrix)

2. SHAPE
    X size
    X dims
    X shape
    X reshape
    X flatten
    X transpose
    X flip
    X roll
    X rollaxis
    ? stack
    ? vstack
    ? hstack
    ? split
    ? repeat

3. INDEXING
    - get (-> get at index)
    - push
    - pop
    - put
    - concatenate
    - pad
    - remove
    - slice
    - select
    - clip
    - where
    - nonzero
    - unique
    - is_empty

4. REDUCTION
    - sum
    - prod
    - mean
    - median
    - min
    - max
    - argmin
    - argmax
    - std
    - var
    - all
    - any
    - count_nonzero

5. VECTIORIAL
    - dot
    - inner
    - outer
    - norm
    - normalize
    - distance
    - angle
    - project
    - reject
    - reflect
    - cross (only 3D)

6. ELEMENTS
    - add
    - subtract
    - multiply
    - divide
    - power
    - sqrt
    - abs
    - exp
    - log
    - log10
    - sin
    - cos
    - tan
    - floor
    - ceil
    - round

7. NUMERIC ANALYSIS
    - diff
    - gradient
    - cumsum
    - cumprod

8. REALTIME STATS
    - movmean
    - movmedian
    - movstd

8. MATIRX
    - matmul
    - transpose
    - trace
    - diag
    - identity
    - outer
 */


typedef struct {
    OPDS h;
    // outputs
    MYFLT *handle;
    // inputs
    ARRAYDAT *shape;
    MYFLT *value; // only for full
    // private
    CSN_ARRAY *array;
    /* Deinit resolves the slot through this rather than through *array, so an
       explicit csnfree cannot turn the deinit pass into a use-after-free. */
    uint32_t handle_id;
} CSN_ARR_INIT;

typedef struct {
    OPDS h;
    // outputs
    MYFLT *handle;
    // inputs
    MYFLT *handle_from;
    MYFLT *value;
} CSN_ARR_INIT_LIKE;

typedef struct {
    OPDS h;
    // outputs
    MYFLT *handle;
    // inputs
    ARRAYDAT *source;
    // private
    CSN_ARRAY *array;
    uint32_t handle_id;
} CSN_FROM_ARRAY;

typedef struct {
    OPDS h;
    // outputs
    ARRAYDAT *array;
    // inputs
    MYFLT *source_handle;
} CSN_TO_ARRAY;

typedef struct {
    OPDS h;
    // inputs
    MYFLT *handle;
} CSN_FREE;

typedef struct {
    OPDS h;
    // outputs
    MYFLT *value;
    // inputs
    MYFLT *handle;
} CSN_SIZE_DIMS;

typedef struct {
    OPDS h;
    // outputs
    ARRAYDAT *shape;
    // inputs
    MYFLT *handle;
} CSN_SHAPE;

typedef struct {
    OPDS h;
    // outputs
    MYFLT *handle;
    // inputs
    MYFLT *start;
    MYFLT *stop;
    MYFLT *step_num; // step for arange
                     // num for linspace, logspace and geomspace
    MYFLT *base;     // only for logspace
    // private
    CSN_ARRAY *array;
    uint32_t handle_id;
} CSN_SPACED_SPACE;

typedef struct {
    OPDS h;
    // outputs
    MYFLT *handle;
    // inputs
    MYFLT *num;
    // private
    CSN_ARRAY *array;
    uint32_t handle_id;
} CSN_IDENTITY;

typedef struct {
    OPDS h;
    // outputs
    MYFLT *handle;
    // inputs
    MYFLT *source_handle;
    ARRAYDAT *new_shape; // shape for reshape
                         // optional axes in transpose
    // private
    CSN_ARRAY *array;
    uint32_t handle_id;
} CSN_RESHAPE;

typedef struct {
    OPDS h;
    // inputs
    MYFLT *source_handle;
    ARRAYDAT *new_shape; // shape for reshape
                         // optional axes for transpose
} CSN_RESHAPE_IN;

typedef struct {
    OPDS h;
    // outputs
    MYFLT *handle;
    // inputs
    MYFLT *source_handle;
    MYFLT *param_a; // axis for flip
                    // shift for roll/rollaxis
    MYFLT *param_b; // null for flip
                    // axis for rollaxis
    // private
    CSN_ARRAY *array;
    uint32_t handle_id;
} CSN_FLIP_ROLL;

typedef struct {
    OPDS h;
    // inputs
    MYFLT *source_handle;
    MYFLT *param_a; // axis for flip
                    // shift for roll/rollaxis
    MYFLT *param_b; // null for flip
                    // axis for rollaxis
} CSN_FLIP_ROLL_IN;

typedef struct {
    OPDS h;
    // ouputs
    MYFLT *handle;
    // inputs
    MYFLT *source_handle;
    ARRAYDAT *indexes;
} CSN_GET;



// CSN-INTERFACE

int32_t create_empty_csnarray(CSOUND *csound, CSN_ARR_INIT *p);
int32_t create_zeros_csnarray(CSOUND *csound, CSN_ARR_INIT *p);
int32_t create_ones_csnarray(CSOUND *csound, CSN_ARR_INIT *p);
int32_t create_full_csnarray(CSOUND *csound, CSN_ARR_INIT *p);
int32_t create_csnarray_like(CSOUND *csound, CSN_ARR_INIT_LIKE *p);

int32_t from_array_to_csnarray(CSOUND *csound, CSN_FROM_ARRAY *p);
int32_t from_csnarray_to_array(CSOUND *csound, CSN_TO_ARRAY *p);

int32_t free_csnarray(CSOUND *csound, CSN_FREE *p);

int32_t csnarray_dims(CSOUND *csound, CSN_SIZE_DIMS *p);
int32_t csnarray_size(CSOUND *csound, CSN_SIZE_DIMS *p);
int32_t csnarray_shape(CSOUND *csound, CSN_SHAPE *p);

int32_t csnarray_arange(CSOUND *csound, CSN_SPACED_SPACE *p);
int32_t csnarray_linspace(CSOUND *csound, CSN_SPACED_SPACE *p);
int32_t csnarray_logspace(CSOUND *csound, CSN_SPACED_SPACE *p);
int32_t csnarray_geomspace(CSOUND *csound, CSN_SPACED_SPACE *p);
int32_t csnarray_identity(CSOUND *csound, CSN_IDENTITY *p);

int32_t csnarray_reshape(CSOUND *csound, CSN_RESHAPE *p);
int32_t csnarray_reshape_in(CSOUND *csound, CSN_RESHAPE_IN *p);
int32_t csnarray_flatten(CSOUND *csound, CSN_RESHAPE *p);
int32_t csnarray_flatten_in(CSOUND *csound, CSN_RESHAPE_IN *p);
int32_t csnarray_transpose(CSOUND *csound, CSN_RESHAPE *p);
int32_t csnarray_transpose_in(CSOUND *csound, CSN_RESHAPE_IN *p);
int32_t csnarray_flip(CSOUND *csound, CSN_FLIP_ROLL *p);
int32_t csnarray_flip_in(CSOUND *csound, CSN_FLIP_ROLL_IN *p);
int32_t csnarray_roll(CSOUND *csound, CSN_FLIP_ROLL *p);
int32_t csnarray_roll_in(CSOUND *csound, CSN_FLIP_ROLL_IN *p);
int32_t csnarray_rollaxis(CSOUND *csound, CSN_FLIP_ROLL *p);
int32_t csnarray_rollaxis_in(CSOUND *csound, CSN_FLIP_ROLL_IN *p);

#endif
