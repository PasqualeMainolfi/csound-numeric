#ifndef __CSNUM_H
#define __CSNUM_H

#include <csdl.h>
#include <stdint.h>
#include "csnregistry.h"

#define CSN_SHAPE_STR_MAX (CSN_MAX_DIMS * 12 + 3)

typedef enum {
    GREATER_THAN = 0,
    LESS_THAN,
    EQUAL,
    NOT_EQUAL,
    NONZERO,
    /* No comparison can select a NaN — ordered ones and EQUAL are false, and
       NOT_EQUAL is true for everything. This mode is the only way to find them. */
    IS_NAN
} CSN_COMPARE_MODE;

typedef enum {
    RED_SUM = 0,
    RED_PROD,
    RED_SUB,
    RED_MEAN,
    RED_MEDIAN,
    RED_MIN,
    RED_MAX,
    RED_ARGMIN,
    RED_ARGMAX,
    RED_STD,
    RED_VAR,
    RED_ALL,
    RED_ANY
} CSN_REDUCTION_MODE;

typedef enum {
    CSN_ADD_HH = 0,
    CSN_ADD_HS,
    CSN_ADD_SH,
    CSN_SUB_HH,
    CSN_SUB_HS,
    CSN_SUB_SH,
    CSN_MUL_HH,
    CSN_MUL_HS,
    CSN_MUL_SH,
    CSN_DIV_HH,
    CSN_DIV_HS,
    CSN_DIV_SH,
    CSN_POW_HH,
    CSN_POW_HS,
    CSN_POW_SH,
    CSN_LOG_HH,
    CSN_LOG_HS,
    CSN_LOG_SH,
} CSN_BINOP_MODE;

typedef enum {
    CSN_SQRT = 0,
    CSN_CBRT,
    CSN_ABS,
    CSN_SIGN,
    CSN_EXP,
    CSN_SIN,
    CSN_COS,
    CSN_TAN,
    CSN_ASIN,
    CSN_ACOS,
    CSN_ATAN,
    CSN_SINH,
    CSN_COSH,
    CSN_TANH,
    CSN_ASINH,
    CSN_ACOSH,
    CSN_ATANH,
    CSN_FLOOR,
    CSN_CEIL,
    CSN_ROUND
} CSN_UNARY_MODE;

typedef enum {
    CSN_DOT = 0,
    CSN_DOT_SCALAR,
    CSN_INNER,
    CSN_INNER_SCALAR,
    CSN_OUTER,
    CSN_PAIR_DISTANCE,
    CSN_DISTANCE,
    CSN_CROSS,
    CSN_ANGLE,
    CSN_PROJECT,
    CSN_REJECT,
    CSN_REFLECT,
} CSN_VECOP_MODE;

typedef enum {
    CSN_DIFF = 0,
    CSN_GRADIENT,
    CSN_CUMSUM,
    CSN_CUMPROD,
    CSN_NORMALIZE
} CSN_UNARYOP_AX_MODE;

typedef enum {
    CSN_MOVMEAN = 0,
    CSN_MOVMEDIAN,
    CSN_MOVSTD,
    CSN_MOVVAR,
    CSN_MOVMIN,
    CSN_MOVMAX
} CSN_MOVSTATS_MODE;

typedef struct {
    double re;
    double im;
} CSN_COMPLEXDAT;

typedef struct {
    double value;
    uint32_t linear_index;
} ARRAY_ELEMENT;

typedef struct {
    OPDS h;
    // outputs
    CSNREF *handle;
    // inputs
    ARRAYDAT *shape;
    MYFLT *itype; // itype = REAL (0) or COMPLEX (1)
    // private
    CSN_ARRAY *array;
} CSN_ARR_INIT;

typedef struct {
    OPDS h;
    // outputs
    CSNREF *handle;
    // inputs
    ARRAYDAT *shape;
    MYFLT *value;
    MYFLT *itype;
    // private
    CSN_ARRAY *array;
} CSN_FULL;

typedef struct {
    OPDS h;
    // outputs
    CSNREF *handle;
    // inputs
    ARRAYDAT *shape;
    COMPLEXDAT *value;
    MYFLT *itype;
    // private
    CSN_ARRAY *array;
} CSN_FULLCOMPLEX;

typedef struct {
    OPDS h;
    // outputs
    CSNREF *handle;
    // inputs
    ARRAYDAT *shape;
    MYFLT *min;
    MYFLT *max;
    // private
    CSN_ARRAY *array;
} CSN_ARR_RND_INIT;

typedef struct {
    OPDS h;
    // outputs
    CSNREF *handle;
    // inputs
    CSNREF *handle_from; // determines if is real or complex
    MYFLT *value; // value for fill
    // private
    CSN_ARRAY *array;
} CSN_ARR_INIT_LIKE;

typedef struct {
    OPDS h;
    // outputs
    CSNREF *handle;
    // inputs
    ARRAYDAT *source;
    // private
    CSN_ARRAY *array;
} CSN_FROM_ARRAY;

typedef struct {
    OPDS h;
    // outputs
    ARRAYDAT *array;
    // inputs
    CSNREF *source_handle;
} CSN_TO_ARRAY;

typedef struct {
    OPDS h;
    // inputs
    CSNREF *handle;
} CSN_FREE;

typedef struct {
    OPDS h;
    // outputs
    CSNREF *handle;
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
    CSNREF *handle;
    // inputs
    MYFLT *num;
    MYFLT *itype;
    // private
    CSN_ARRAY *array;
} CSN_IDENTITY;

typedef struct {
    OPDS h;
    // outputs
    MYFLT *value;
    // inputs
    CSNREF *handle;
} CSN_SIZE_DIMS;

typedef struct {
    OPDS h;
    // outputs
    ARRAYDAT *shape;
    // inputs
    CSNREF *handle;
} CSN_SHAPE;

typedef struct {
    OPDS h;
    // outputs
    CSNREF *handle;
    // inputs
    CSNREF *source_handle;
    ARRAYDAT *new_shape; // shape for reshape
                         // optional axes in transpose
    // private
    CSN_ARRAY *array;
} CSN_RESHAPE;

typedef struct {
    OPDS h;
    // inputs
    CSNREF *source_handle;
    ARRAYDAT *new_shape; // shape for reshape
                         // optional axes for transpose
} CSN_RESHAPE_IN;

typedef struct {
    OPDS h;
    // outputs
    CSNREF *handle;
    // inputs
    CSNREF *source_handle;
    MYFLT *param_a; // axis for flip
                    // shift for roll/rollaxis
    MYFLT *param_b; // null for flip
                    // axis for rollaxis
    // private
    CSN_ARRAY *array;
} CSN_FLIP_ROLL;

typedef struct {
    OPDS h;
    // inputs
    CSNREF *source_handle;
    MYFLT *param_a; // axis for flip
                    // shift for roll/rollaxis
    MYFLT *param_b; // null for flip
                    // axis for rollaxis
} CSN_FLIP_ROLL_IN;

typedef struct {
    OPDS h;
    // ouputs
    MYFLT *value;
    // inputs
    CSNREF *source_handle;
    ARRAYDAT *indexes; // get -> indexes must be the same as dims
} CSN_GET;

typedef struct {
    OPDS h;
    // ouputs
    COMPLEXDAT *value;
    // inputs
    CSNREF *source_handle;
    ARRAYDAT *indexes; // get -> indexes must be the same as dims
} CSN_GETCOMPLEX;

typedef struct {
    OPDS h;
    // inputs
    CSNREF *source_handle;
    ARRAYDAT *indexes; // set -> indexes must be the same as dims
    MYFLT *value;
} CSN_SET;

typedef struct {
    OPDS h;
    // inputs
    CSNREF *source_handle;
    ARRAYDAT *indexes; // set -> indexes must be the same as dims
    COMPLEXDAT *value;
} CSN_SETCOMPLEX;

typedef struct {
    OPDS h;
    // ouputs
    CSNREF *handle;
    // inputs
    CSNREF *source_handle;
    MYFLT *axis;
    MYFLT *index;
    // private
    CSN_ARRAY *array;
} CSN_TAKE;

/* Two-argument take: flat index in, scalar out. No handle, so no deinit. */
typedef struct {
    OPDS h;
    // outputs
    MYFLT *value;
    // inputs
    CSNREF *source_handle;
    MYFLT *index;
} CSN_TAKE_FLAT;

typedef struct {
    OPDS h;
    // outputs
    COMPLEXDAT *value;
    // inputs
    CSNREF *source_handle;
    MYFLT *index;
} CSN_TAKECOMPLEX_FLAT;

typedef struct {
    OPDS h;
    // ouputs
    CSNREF *handle;
    // inputs
    CSNREF *source_handle;
    MYFLT *axis;
    MYFLT *start;
    MYFLT *stop;
    MYFLT *step;
    // private
    CSN_ARRAY *array;
} CSN_GET_SLICE;

typedef struct {
    OPDS h;
    // inputs
    CSNREF *source_handle;
    CSNREF *data_handle;
    MYFLT *axis;
    MYFLT *start;
    MYFLT *stop;
    MYFLT *step;
} CSN_SET_SLICE;

typedef struct {
    OPDS h;
    // inputs
    CSNREF *source_handle;
    MYFLT *in_value; // push at the top
    MYFLT *index;    // only for put (at index)
} CSN_PUSH;

typedef struct {
    OPDS h;
    // inputs
    CSNREF *source_handle;
    COMPLEXDAT *in_value; // push at the top
    MYFLT *index;    // only for put (at index)
} CSN_PUSHCOMPLEX;

typedef struct {
    OPDS h;
    // outputs
    MYFLT *out_value;
    // inputs
    CSNREF *source_handle; // remove the top element
    MYFLT *index;         // only for remove (at the index)
} CSN_POP;

typedef struct {
    OPDS h;
    // outputs
    COMPLEXDAT *out_value;
    // inputs
    CSNREF *source_handle; // remove the top element
    MYFLT *index;         // only for remove (at the index)
} CSN_POPCOMPLEX;

typedef struct {
    OPDS h;
    // inputs
    CSNREF *source_handle;
    CSNREF *data_handle; // in block
    MYFLT *axis;
    MYFLT *index;
} CSN_INSERT_BLOCK;

typedef struct {
    OPDS h;
    // outputs
    CSNREF *handle;
    // inputs
    CSNREF *source_handle;
    CSNREF *data_handle;
    MYFLT *axis; // only for .block
    // private
    CSN_ARRAY *array;
} CSN_CONCAT;

typedef struct {
    OPDS h;
    // outputs
    CSNREF *handle;
    // inputs
    CSNREF *source_handle;
    MYFLT *before;
    MYFLT *after;
    MYFLT *value;
    MYFLT *axis; // axis -> NULL default all axes
    // private
    CSN_ARRAY *array;
} CSN_PAD;

typedef struct {
    OPDS h;
    // outputs
    CSNREF *handle;
    // inputs
    CSNREF *source_handle;
    MYFLT *before;
    MYFLT *after;
    COMPLEXDAT *value;
    MYFLT *axis; // axis -> NULL default all axes
    // private
    CSN_ARRAY *array;
} CSN_PADCOMPLEX;

typedef struct {
    OPDS h;
    // inputs
    CSNREF *source_handle;
    MYFLT *before;
    MYFLT *after;
    MYFLT *value;
    MYFLT *axis; // axis -> NULL default all axes
} CSN_PAD_IN;

typedef struct {
    OPDS h;
    // inputs
    CSNREF *source_handle;
    MYFLT *before;
    MYFLT *after;
    COMPLEXDAT *value;
    MYFLT *axis; // axis -> NULL default all axes
} CSN_PADCOMPLEX_IN;

typedef struct {
    OPDS h;
    // outputs
    CSNREF *handle;
    // inputs
    CSNREF *source_handle;
    MYFLT *min;
    MYFLT *max;
    // private
    CSN_ARRAY *array;
} CSN_CLIP;

typedef struct {
    OPDS h;
    // inputs
    CSNREF *source_handle;
    MYFLT *min;
    MYFLT *max;
} CSN_CLIP_IN;

typedef struct {
    OPDS h;
    // outputs
    CSNREF *handle;
    // inputs
    CSNREF *source_handle; // array source
    CSNREF *data_handle;   // array of values (source array will compared with data_handle)
    // private
    CSN_ARRAY *array;
} CSN_ARGWHERE;

typedef struct {
    OPDS h;
    // outputs
    CSNREF *handle;
    // inputs
    CSNREF *source_handle; // array source
    MYFLT *cmp_value;     // only for gt, lt, ne
    // private
    CSN_ARRAY *array;
} CSN_COMPARE;

/* The count family returns how many elements matched, never an array, so its
   output is a plain number and it needs no handle or deinit. */
typedef struct {
    OPDS h;
    // outputs
    MYFLT *value;
    // inputs
    CSNREF *source_handle;
    MYFLT *cmp_value;     // only for cnteq
} CSN_COUNT;

/* Reducing along an axis drops that axis and yields an array. */
typedef struct {
    OPDS h;
    // outputs
    CSNREF *handle;
    // inputs
    CSNREF *source_handle;
    MYFLT *axis;
    // private
    CSN_ARRAY *array;
} CSN_REDUCTION;

/* Reducing over every axis collapses to one number. Splitting this out of
   CSN_REDUCTION is what lets the result type be known at compile time: the
   same opcode name cannot return an array in one call and a number in the
   next, now that the two are distinct types. */
typedef struct {
    OPDS h;
    // outputs
    MYFLT *value;
    // inputs
    CSNREF *source_handle;
} CSN_REDUCTION_SCALAR;

typedef struct {
    OPDS h;
    // outputs
    COMPLEXDAT *value;
    // inputs
    CSNREF *source_handle;
} CSN_REDUCTION_COMPLEX_S;

typedef struct {
    OPDS h;
    // outputs
    CSNREF *handle;
    // inputs
    CSNREF *source_handle_a;
    CSNREF *source_handle_b;
    // private
    CSN_ARRAY *array;
} CSN_BINOP_HH;

typedef struct {
    OPDS h;
    // outputs
    MYFLT *value;
    // inputs
    CSNREF *source_handle_a;
    CSNREF *source_handle_b;
    MYFLT *arg_a; // only for dist order (order Minkowski)
} CSN_BINOP_HH_SCALAR;

/* The three binop structs share this prefix and tail, which is what lets one
   deinit and one helper serve all of them. Only the order of the two input
   slots differs, so those are always passed explicitly, never read off a cast. */
typedef struct {
    OPDS h;
    // outputs
    CSNREF *handle;
    void *arg_a;
    void *arg_b;
    // private
    CSN_ARRAY *array;
} CSN_BINOP_COMMON;

typedef struct {
    OPDS h;
    // outputs
    CSNREF *handle;
    // inputs
    CSNREF *source_handle;
    MYFLT *scalar;
    // private
    CSN_ARRAY *array;
} CSN_BINOP_HS;

/* normalize: v / ||v||, con asse opzionale (-1 = tutto l'array) e ordine
   della norma opzionale. */
typedef struct {
    OPDS h;
    // outputs
    CSNREF *handle;
    // inputs
    CSNREF *source_handle;
    MYFLT *axis;
    MYFLT *order;
    // private
    CSN_ARRAY *array;
} CSN_UNARYOP_AX;

typedef struct {
    OPDS h;
    // inputs
    CSNREF *source_handle;
    MYFLT *axis;
    MYFLT *order;
} CSN_UNARYOP_AX_IN;

typedef struct {
    OPDS h;
    // outputs
    MYFLT *value;
    // inputs
    CSNREF *source_handle;
    MYFLT *scalar;
} CSN_BINOP_HS_SCALAR;

typedef struct {
    OPDS h;
    // outputs
    CSNREF *handle;
    // inputs
    MYFLT *scalar;
    CSNREF *source_handle;
    // private
    CSN_ARRAY *array;
} CSN_BINOP_SH;

typedef struct {
    OPDS h;
    // outputs
    CSNREF *handle;
    // inputs
    CSNREF *source_handle;
    // private
    CSN_ARRAY *array;
} CSN_UNARYOP;

typedef struct {
    OPDS h;
    // outputs
    MYFLT *value;
    // inputs
    CSNREF *source_handle;
} CSN_UNARYOP_SCALAR;

typedef struct {
    OPDS h;
    // inputs
    CSNREF *source_handle;
    MYFLT *arg_a; // in normalize is axis
} CSN_UNARYOP_IN;

typedef struct {
    OPDS h;
    // outputs
    CSNREF *handle;
    // inputs
    CSNREF *source_handle;
    /* L'ordine segue gli intypes ":CsnArr;ip": l'asse e' obbligatorio e viene
       prima, l'ordine della norma e' opzionale e chiude. */
    MYFLT *axis;
    MYFLT *order;
    // private
    CSN_ARRAY *array;
} CSN_NORM_REDUCTION;

typedef struct {
    OPDS h;
    // outputs
    MYFLT *value;
    // inputs
    CSNREF *source_handle;
    MYFLT *order;
} CSN_NORM_REDUCTION_SCALAR;

typedef struct {
    OPDS h;
    // outputs
    CSNREF *handle;
    // inputs
    CSNREF *source_handle;
    MYFLT *winsize;
    MYFLT *axis;
    // private
    CSN_ARRAY *array;
} CSN_MOVSTATS;

typedef struct {
    OPDS h;
    // outputs
    CSNREF *handle;
    // inputs
    CSNREF *source_handle;
    MYFLT *winsize;
    MYFLT *axis;
    // private
    CSN_ARRAY *array;
} CSN_MOVSTATS_IN;


// CREATION
int32_t create_empty_csnarray(CSOUND *csound, CSN_ARR_INIT *p);
int32_t create_zeros_csnarray(CSOUND *csound, CSN_ARR_INIT *p);
int32_t create_ones_csnarray(CSOUND *csound, CSN_ARR_INIT *p);
int32_t create_like_csnarray(CSOUND *csound, CSN_ARR_INIT_LIKE *p);
int32_t create_full_csnarray(CSOUND *csound, CSN_FULL *p);
int32_t create_fullcomp_csnarray(CSOUND *csound, CSN_FULLCOMPLEX *p);
int32_t create_random_csnarray(CSOUND *csound, CSN_ARR_RND_INIT *p);
int32_t from_array_to_csnarray(CSOUND *csound, CSN_FROM_ARRAY *p);
int32_t from_complexarray_to_csnarray(CSOUND *csound, CSN_FROM_ARRAY *p);
int32_t from_csnarray_to_array(CSOUND *csound, CSN_TO_ARRAY *p);
int32_t from_csnarray_to_complexarray(CSOUND *csound, CSN_TO_ARRAY *p);
int32_t free_csnarray(CSOUND *csound, CSN_FREE *p);
int32_t csnarray_arange(CSOUND *csound, CSN_SPACED_SPACE *p);
int32_t csnarray_linspace(CSOUND *csound, CSN_SPACED_SPACE *p);
int32_t csnarray_logspace(CSOUND *csound, CSN_SPACED_SPACE *p);
int32_t csnarray_geomspace(CSOUND *csound, CSN_SPACED_SPACE *p);

// SHAPE
int32_t csnarray_dims(CSOUND *csound, CSN_SIZE_DIMS *p);
int32_t csnarray_size(CSOUND *csound, CSN_SIZE_DIMS *p);
int32_t csnarray_is_empty(CSOUND *csound, CSN_SIZE_DIMS *p);
int32_t csnarray_shape(CSOUND *csound, CSN_SHAPE *p);
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

// INDEXING
int32_t csnarray_get(CSOUND *csound, CSN_GET *p);
int32_t csnarray_get_complex(CSOUND *csound, CSN_GETCOMPLEX *p);
int32_t csnarray_set(CSOUND *csound, CSN_SET *p);
int32_t csnarray_set_complex(CSOUND *csound, CSN_SETCOMPLEX *p);
int32_t csnarray_take(CSOUND *csound, CSN_TAKE *p);
int32_t csnarray_take_flat(CSOUND *csound, CSN_TAKE_FLAT *p);
int32_t csnarray_takecomp_flat(CSOUND *csound, CSN_TAKECOMPLEX_FLAT *p);
int32_t csnarray_get_slice(CSOUND *csound, CSN_GET_SLICE *p);
int32_t csnarray_set_slice(CSOUND *csound, CSN_SET_SLICE *p);
int32_t csnarray_push(CSOUND *csound, CSN_PUSH *p);
int32_t csnarray_pushcomp(CSOUND *csound, CSN_PUSHCOMPLEX *p);
int32_t csnarray_pop(CSOUND *csound, CSN_POP *p);
int32_t csnarray_popcomp(CSOUND *csound, CSN_POPCOMPLEX *p);
int32_t csnarray_insert(CSOUND *csound, CSN_PUSH *p);
int32_t csnarray_insertcomp(CSOUND *csound, CSN_PUSHCOMPLEX *p);
int32_t csnarray_remove(CSOUND *csound, CSN_POP *p);
int32_t csnarray_removecomp(CSOUND *csound, CSN_POPCOMPLEX *p);
int32_t csnarray_insert_block(CSOUND *csound, CSN_INSERT_BLOCK *p);
int32_t csnarray_remove_block(CSOUND *csound, CSN_TAKE *p);
int32_t csnarray_concat_flat(CSOUND *csound, CSN_CONCAT *p);
int32_t csnarray_concat_block(CSOUND *csound, CSN_CONCAT *p);
int32_t csnarray_pad(CSOUND *csound, CSN_PAD *p);
int32_t csnarray_pad_in(CSOUND *csound, CSN_PAD_IN *p);
int32_t csnarray_padcomp(CSOUND *csound, CSN_PADCOMPLEX *p);
int32_t csnarray_padcomp_in(CSOUND *csound, CSN_PADCOMPLEX_IN *p);
int32_t csnarray_clip(CSOUND *csound, CSN_CLIP *p);
int32_t csnarray_clip_in(CSOUND *csound, CSN_CLIP_IN *p);
int32_t csnarray_argwhere(CSOUND *csound, CSN_ARGWHERE *p);     // return (count, ndim)
int32_t csnarray_argnonzero(CSOUND *csound, CSN_ARGWHERE *p);   // return (count, ndim)
int32_t csnarray_argunique(CSOUND *csound, CSN_ARGWHERE *p);    // return (count, ndim)
int32_t csnarray_argisnan(CSOUND *csound, CSN_ARGWHERE *p);     // return (count, ndim)
int32_t csnarray_argmin(CSOUND *csound, CSN_REDUCTION *p);      // return (1, ndim) if axis == -1 else (shape[axis], ndim)
int32_t csnarray_argmax(CSOUND *csound, CSN_REDUCTION *p);      // return (1, ndim) if axis == -1 else (shape[axis], ndim)
int32_t csnarray_unique(CSOUND *csound, CSN_COMPARE *p);        // return array 1D
int32_t csnarray_greater_than(CSOUND *csound, CSN_COMPARE *p);  // return array 1D
int32_t csnarray_less_than(CSOUND *csound, CSN_COMPARE *p);     // return array 1D
int32_t csnarray_not_equal(CSOUND *csound, CSN_COMPARE *p);     // return array 1D
int32_t csnarray_count_equal(CSOUND *csound, CSN_COUNT *p);     // return count value
int32_t csnarray_count_nonzero(CSOUND *csound, CSN_COUNT *p);   // return count value
int32_t csnarray_count_nan(CSOUND *csound, CSN_COUNT *p);       // return count value

// REDUCTION
int32_t csnarray_sum(CSOUND *csound, CSN_REDUCTION *p);
int32_t csnarray_sum_all(CSOUND *csound, CSN_REDUCTION_SCALAR *p);
int32_t csnarray_sumcomp_all(CSOUND *csound, CSN_REDUCTION_COMPLEX_S *p);
int32_t csnarray_prod(CSOUND *csound, CSN_REDUCTION *p);
int32_t csnarray_prod_all(CSOUND *csound, CSN_REDUCTION_SCALAR *p);
int32_t csnarray_prodcomp_all(CSOUND *csound, CSN_REDUCTION_COMPLEX_S *p);
int32_t csnarray_sub(CSOUND *csound, CSN_REDUCTION *p);
int32_t csnarray_sub_all(CSOUND *csound, CSN_REDUCTION_SCALAR *p);
int32_t csnarray_subcomp_all(CSOUND *csound, CSN_REDUCTION_COMPLEX_S *p);
int32_t csnarray_mean(CSOUND *csound, CSN_REDUCTION *p);
int32_t csnarray_mean_all(CSOUND *csound, CSN_REDUCTION_SCALAR *p);
int32_t csnarray_meancomp_all(CSOUND *csound, CSN_REDUCTION_COMPLEX_S *p);
int32_t csnarray_min(CSOUND *csound, CSN_REDUCTION *p);
int32_t csnarray_min_all(CSOUND *csound, CSN_REDUCTION_SCALAR *p);
int32_t csnarray_max(CSOUND *csound, CSN_REDUCTION *p);
int32_t csnarray_max_all(CSOUND *csound, CSN_REDUCTION_SCALAR *p);
int32_t csnarray_all(CSOUND *csound, CSN_REDUCTION *p);
int32_t csnarray_all_all(CSOUND *csound, CSN_REDUCTION_SCALAR *p);
int32_t csnarray_any(CSOUND *csound, CSN_REDUCTION *p);
int32_t csnarray_any_all(CSOUND *csound, CSN_REDUCTION_SCALAR *p);
int32_t csnarray_median(CSOUND *csound, CSN_REDUCTION *p);
int32_t csnarray_median_all(CSOUND *csound, CSN_REDUCTION_SCALAR *p);
int32_t csnarray_std(CSOUND *csound, CSN_REDUCTION *p);
int32_t csnarray_std_all(CSOUND *csound, CSN_REDUCTION_SCALAR *p);
int32_t csnarray_var(CSOUND *csound, CSN_REDUCTION *p);
int32_t csnarray_var_all(CSOUND *csound, CSN_REDUCTION_SCALAR *p);

// ELEMENTS
int32_t csnarray_add_hh(CSOUND *csound, CSN_BINOP_HH *p);
int32_t csnarray_add_hs(CSOUND *csound, CSN_BINOP_HS *p);
int32_t csnarray_subtract_hh(CSOUND *csound, CSN_BINOP_HH *p);
int32_t csnarray_subtract_hs(CSOUND *csound, CSN_BINOP_HS *p);
int32_t csnarray_subtract_sh(CSOUND *csound, CSN_BINOP_SH *p);
int32_t csnarray_mul_hh(CSOUND *csound, CSN_BINOP_HH *p);
int32_t csnarray_mul_hs(CSOUND *csound, CSN_BINOP_HS *p);
int32_t csnarray_div_hh(CSOUND *csound, CSN_BINOP_HH *p);
int32_t csnarray_div_hs(CSOUND *csound, CSN_BINOP_HS *p);
int32_t csnarray_div_sh(CSOUND *csound, CSN_BINOP_SH *p);
int32_t csnarray_pow_hh(CSOUND *csound, CSN_BINOP_HH *p);
int32_t csnarray_pow_hs(CSOUND *csound, CSN_BINOP_HS *p);
int32_t csnarray_pow_sh(CSOUND *csound, CSN_BINOP_SH *p);
int32_t csnarray_log_hh(CSOUND *csound, CSN_BINOP_HH *p);
int32_t csnarray_log_sh(CSOUND *csound, CSN_BINOP_SH *p);
int32_t csnarray_log_hs(CSOUND *csound, CSN_BINOP_HS *p); // use base
int32_t csnarray_sqrt(CSOUND *csound, CSN_UNARYOP *p);
int32_t csnarray_cbrt(CSOUND *csound, CSN_UNARYOP *p);
int32_t csnarray_abs(CSOUND *csound, CSN_UNARYOP *p);
int32_t csnarray_exp(CSOUND *csound, CSN_UNARYOP *p);
int32_t csnarray_sin(CSOUND *csound, CSN_UNARYOP *p);
int32_t csnarray_cos(CSOUND *csound, CSN_UNARYOP *p);
int32_t csnarray_tan(CSOUND *csound, CSN_UNARYOP *p);
int32_t csnarray_asin(CSOUND *csound, CSN_UNARYOP *p);
int32_t csnarray_acos(CSOUND *csound, CSN_UNARYOP *p);
int32_t csnarray_atan(CSOUND *csound, CSN_UNARYOP *p);
int32_t csnarray_sinh(CSOUND *csound, CSN_UNARYOP *p);
int32_t csnarray_cosh(CSOUND *csound, CSN_UNARYOP *p);
int32_t csnarray_tanh(CSOUND *csound, CSN_UNARYOP *p);
int32_t csnarray_asinh(CSOUND *csound, CSN_UNARYOP *p);
int32_t csnarray_acosh(CSOUND *csound, CSN_UNARYOP *p);
int32_t csnarray_atanh(CSOUND *csound, CSN_UNARYOP *p);
int32_t csnarray_floor(CSOUND *csound, CSN_UNARYOP *p);
int32_t csnarray_ceil(CSOUND *csound, CSN_UNARYOP *p);
int32_t csnarray_round(CSOUND *csound, CSN_UNARYOP *p);
int32_t csnarray_sign(CSOUND *csound, CSN_UNARYOP *p);

// VECTORIAL
int32_t csnarray_dot(CSOUND *csound, CSN_BINOP_HH *p);
int32_t csnarray_dot_scalar(CSOUND *csound, CSN_BINOP_HH_SCALAR *p); // return a scalar
int32_t csnarray_inner(CSOUND *csound, CSN_BINOP_HH *p);
int32_t csnarray_inner_scalar(CSOUND *csound, CSN_BINOP_HH_SCALAR *p); // return a scalar
int32_t csnarray_outer(CSOUND *csound, CSN_BINOP_HH *p);
int32_t csnarray_norm(CSOUND *csound, CSN_NORM_REDUCTION *p); // generalized norm order (Minkowski)
int32_t csnarray_norm_scalar(CSOUND *csound, CSN_NORM_REDUCTION_SCALAR *p); // specify axis -1 -> all
int32_t csnarray_normalize(CSOUND *csound, CSN_UNARYOP_AX *p); // specify axis -1 -> all
int32_t csnarray_normalize_in(CSOUND *csound, CSN_UNARYOP_AX_IN *p); // specify axis -1 -> all
int32_t csnarray_distance(CSOUND *csound, CSN_BINOP_HH_SCALAR *p); // only between vecton in the same space
int32_t csnarray_pair_distance(CSOUND *csound, CSN_BINOP_HH *p); // only between vecton in the same space
int32_t csnarray_angle(CSOUND *csound, CSN_BINOP_HH_SCALAR *p);
int32_t csnarray_project(CSOUND *csound, CSN_BINOP_HH *p);
int32_t csnarray_reject(CSOUND *csound, CSN_BINOP_HH *p);
int32_t csnarray_reflect(CSOUND *csound, CSN_BINOP_HH *p);
int32_t csnarray_cross(CSOUND *csound, CSN_BINOP_HH *p); // only 1-D with size = 3

// NUMERIC ANALYSIS
int32_t csnarray_diff(CSOUND *csound, CSN_UNARYOP_AX *p);
int32_t csnarray_gradient(CSOUND *csound, CSN_UNARYOP_AX *p);
int32_t csnarray_cumsum(CSOUND *csound, CSN_UNARYOP_AX *p);
int32_t csnarray_cumprod(CSOUND *csound, CSN_UNARYOP_AX *p);

// MATRIX
int32_t csnarray_identity(CSOUND *csound, CSN_IDENTITY *p);
int32_t csnarray_matmul(CSOUND *csound, CSN_BINOP_HH *p); // as numpy (broadcast last two dims)
int32_t csnarray_matmul_scalar(CSOUND *csound, CSN_BINOP_HH_SCALAR *p);
int32_t csnarray_trace(CSOUND *csound, CSN_UNARYOP_SCALAR *p); // only 2D
int32_t csnarray_diag(CSOUND *csound, CSN_UNARYOP *p); // with 1D -> 2D, with 2D -> 1D

// STATS
int32_t csnarray_movmean(CSOUND *csound, CSN_MOVSTATS *p); // auto edges managment (see movemean_slice() function)
int32_t csnarray_movmedian(CSOUND *csound, CSN_MOVSTATS *p);
int32_t csnarray_movstd(CSOUND *csound, CSN_MOVSTATS *p);
int32_t csnarray_movvar(CSOUND *csound, CSN_MOVSTATS *p);
int32_t csnarray_movmin(CSOUND *csound, CSN_MOVSTATS *p);
int32_t csnarray_movmax(CSOUND *csound, CSN_MOVSTATS *p);
int32_t csnarray_movmean_in(CSOUND *csound, CSN_MOVSTATS_IN *p); // in-place
int32_t csnarray_movmedian_in(CSOUND *csound, CSN_MOVSTATS_IN *p);
int32_t csnarray_movstd_in(CSOUND *csound, CSN_MOVSTATS_IN *p);
int32_t csnarray_movvar_in(CSOUND *csound, CSN_MOVSTATS_IN *p);
int32_t csnarray_movmin_in(CSOUND *csound, CSN_MOVSTATS_IN *p);
int32_t csnarray_movmax_in(CSOUND *csound, CSN_MOVSTATS_IN *p);

// COMPLEX

#endif
