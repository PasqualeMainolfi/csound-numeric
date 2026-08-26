#ifndef __CSNUM_H
#define __CSNUM_H

#include <csdl.h>
#include <stddef.h>
#include <stdint.h>
#include "csnregistry.h"

#define CSN_SHAPE_STR_MAX (CSN_MAX_DIMS * 12 + 3)
#define IS_REQUEST_CHANGED(k_data, ndim, itype, shape) (k_data)->prev_ndim != (ndim) || (k_data)->prev_itype != (itype) || memcmp((k_data)->prev_shape, (shape), sizeof(uint32_t) * (ndim)) != 0
#define SHOULD_SLOT_BE_UPDATED(request_changed, array, mode_type, requested_size) (request_changed) || (array)->data == NULL || (array)->itype != (mode_type) || (array)->capacity < (requested_size)
#define CHECK_KTRIG(trig)                         \
    do {                                          \
        if ((double) *(trig) == 0.0) return OK;   \
    } while (0)

#define CSN_ACCESSOR_ERROR(csound, perf_h, ...) \
    ((perf_h) != NULL ? (csound)->PerfError((csound), (perf_h), __VA_ARGS__) : (csound)->InitError((csound), __VA_ARGS__))

#define CHECK_REG_HANDLE(csound, h, reg, handle)                     \
do {                                                                 \
    if ((reg) == NULL || (handle) == 0) {                            \
        return (csound)->PerfError(                                  \
            (csound),                                                \
            (h),                                                    \
            "[csnarray] k-rate output slot was not initialized"      \
        );                                                           \
    }                                                                \
} while (0)

#define SET_KDATA_BEGIN(p, reg)                                                              \
    do {                                                                                     \
        memset((p)->k_data.prev_shape, 0, sizeof((p)->k_data.prev_shape));                   \
        memcpy((p)->k_data.prev_shape, (p)->array->shape, sizeof((p)->k_data.prev_shape));   \
        (p)->k_data.prev_ndim = (p)->array->ndim;                                            \
        (p)->k_data.prev_itype = (p)->array->itype;                                          \
        (p)->k_data.owned_handle = (p)->handle->id;                                          \
        (p)->k_data.registry = (reg);                                                        \
    } while (0)

#define SET_KDATA_WITH_ID_BEGIN(p, reg, shape, ndim, itype, handle)                                                              \
    do {                                                                           \
        memset((p)->k_data.prev_shape, 0, sizeof((p)->k_data.prev_shape));         \
        memcpy((p)->k_data.prev_shape, (shape), sizeof((p)->k_data.prev_shape));   \
        (p)->k_data.prev_ndim = (ndim);                                            \
        (p)->k_data.prev_itype = (itype);                                          \
        (p)->k_data.owned_handle = (handle);                                       \
        (p)->k_data.registry = (reg);                                              \
    } while (0)

#define SET_KDATA_END(p, shape, ndim, itype)                                       \
    do {                                                                           \
        memset((p)->k_data.prev_shape, 0, sizeof((p)->k_data.prev_shape));         \
        memcpy((p)->k_data.prev_shape, (shape), sizeof((p)->k_data.prev_shape));   \
        (p)->k_data.prev_ndim = (ndim);                                            \
        (p)->k_data.prev_itype = (itype);                                          \
        (p)->handle->id = (p)->k_data.owned_handle;                                \
    } while (0)

#define SET_KDATA_NO_ID_END(p, shape, ndim, itype)                                 \
    do {                                                                           \
        memset((p)->k_data.prev_shape, 0, sizeof((p)->k_data.prev_shape));         \
        memcpy((p)->k_data.prev_shape, (shape), sizeof((p)->k_data.prev_shape));   \
        (p)->k_data.prev_ndim = (ndim);                                            \
        (p)->k_data.prev_itype = (itype);                                          \
    } while (0)



typedef enum {
    GREATER_THAN = 0,
    LESS_THAN,
    EQUAL,
    NOT_EQUAL,
    GREATER_EQUAL,
    LESS_EQUAL,
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
    CSN_LOGICAL_AND_HH,
    CSN_LOGICAL_OR_HH,
    CSN_LOGICAL_AND_HS,
    CSN_LOGICAL_OR_HS,
    CSN_LOGICAL_AND_SH,
    CSN_LOGICAL_OR_SH
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
    CSN_ROUND,
    CSN_LOGICAL_NOT
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
    CSN_ANGLE_DISTANCE,
    CSN_PROJECT,
    CSN_REJECT,
    CSN_REFLECT,
} CSN_VECOP_MODE;

typedef enum {
    CSN_REAL_PART = 0,
    CSN_IMAG_PART,
    CSN_REAL_TO_COMPLEX,
    CSN_ANGLE_PART,
    CSN_CONJ_PART,
    CSN_ABS_PART,
    CSN_WRAP,
    CSN_UNWRAP,
    CSN_COMPLEX_TO_ANGLE
} CSN_COMPLEXOP_MODE;

typedef enum {
    CSN_DIFF = 0,
    CSN_GRADIENT,
    CSN_CUMSUM,
    CSN_CUMPROD,
    CSN_NORMALIZE,
    CSN_SORT,
    CSN_ARGSORT
} CSN_UNARYOP_AX_MODE;

typedef enum {
    CSN_MOVMEAN = 0,
    CSN_MOVMEDIAN,
    CSN_MOVSTD,
    CSN_MOVVAR,
    CSN_MOVMIN,
    CSN_MOVMAX
} CSN_MOVSTATS_MODE;

typedef enum {
    DEGREE = 0,
    RADIANS
} CSN_ANGLE_MODE;

typedef enum {
    CSN_K_EMPTY,
    CSN_K_ZEROS,
    CSN_K_ONES
} CSN_K_SHAPE_INIT_MODE;

typedef enum {
    CSN_ARANGE,
    CSN_LINSPACE,
    CSN_LOGSPACE,
    CSN_GEOMSPACE
} CSN_SPACED_SPACE_MODE;

typedef struct {
    double re;
    double im;
} CSN_COMPLEXDAT;

typedef struct {
    double value;
    uint32_t linear_index;
} ARRAY_ELEMENT;

typedef struct {
    uint32_t prev_shape[CSN_MAX_DIMS];
    uint32_t prev_axes[CSN_MAX_DIMS];
    uint32_t prev_axis;
    uint32_t prev_index;
    int32_t prev_roll_shift;
    size_t prev_size;
    uint32_t prev_ndim;
    ITEM_TYPE prev_itype;
    uint32_t owned_handle;
    uint32_t owned_data_handle;
    CSN_REGISTRY *registry;
} K_DATA;

typedef struct {
    OPDS h;
    // outputs
    CSNREF *handle;
    // inputs
    ARRAYDAT *shape;
    MYFLT *itype; // itype = REAL (0) or COMPLEX (1)
    // private
    CSN_ARRAY *array;
    K_DATA k_data;
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
    K_DATA k_data;
} CSN_FULL;

typedef struct {
    OPDS h;
    // outputs
    CSNREF *handle;
    // inputs
    ARRAYDAT *shape;
    /* The value's own type fixes the array's, so there is no itype argument. */
    COMPLEXDAT *value;
    // private
    CSN_ARRAY *array;
    K_DATA k_data;
} CSN_FULLCOMPLEX;

typedef struct {
    OPDS h;
    // outputs
    CSNREF *handle;
    // inputs
    ARRAYDAT *shape;
    MYFLT *min;
    MYFLT *max;
    MYFLT *trig;
    // private
    CSN_ARRAY *array;
    K_DATA k_data;
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
    K_DATA k_data;
} CSN_ARR_INIT_LIKE;

typedef struct {
    OPDS h;
    // outputs
    CSNREF *handle;
    // inputs
    ARRAYDAT *source;
    // private
    CSN_ARRAY *array;
    K_DATA k_data;
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
    MYFLT *arg_a;    // trig for all the others
                     // only for logspace -> base
    MYFLT *arg_b;    // trig only for logspace
    // private
    CSN_ARRAY *array;
    uint32_t handle_id;
    K_DATA k_data;
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
    K_DATA k_data;
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
    K_DATA k_data;
} CSN_RESHAPE;

typedef struct {
    OPDS h;
    // inputs
    CSNREF *source_handle;
    ARRAYDAT *new_shape; // shape for reshape
                         // optional axes for transpose
    // private
    K_DATA k_data;
    double *scratch;
    size_t scratch_capacity;
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
    K_DATA k_data;
} CSN_FLIP_ROLL;

typedef struct {
    OPDS h;
    // inputs
    CSNREF *source_handle;
    MYFLT *param_a; // axis for flip
                    // shift for roll/rollaxis
    MYFLT *param_b; // null for flip
                    // axis for rollaxis
    K_DATA k_data;
    double *scratch;
    size_t scratch_capacity;
} CSN_FLIP_ROLL_IN;

typedef struct {
    OPDS h;
    // ouputs
    MYFLT *value;
    // inputs
    CSNREF *source_handle;
    ARRAYDAT *indexes; // get -> indexes must be the same as dims
    // private
    K_DATA k_data;
} CSN_GET;

typedef struct {
    OPDS h;
    // ouputs
    COMPLEXDAT *value;
    // inputs
    CSNREF *source_handle;
    ARRAYDAT *indexes; // get -> indexes must be the same as dims
    // private
    K_DATA k_data;
} CSN_GETCOMPLEX;

typedef struct {
    OPDS h;
    // inputs
    CSNREF *source_handle;
    ARRAYDAT *indexes; // set -> indexes must be the same as dims
    MYFLT *value;
    // private
    K_DATA k_data;
} CSN_SET;

typedef struct {
    OPDS h;
    // inputs
    CSNREF *source_handle;
    ARRAYDAT *indexes; // set -> indexes must be the same as dims
    COMPLEXDAT *value;
    // private
    K_DATA k_data;
} CSN_SETCOMPLEX;

typedef struct {
    OPDS h;
    // ouputs
    CSNREF *handle;
    // inputs
    CSNREF *source_handle;
    MYFLT *axis;
    MYFLT *index;
    MYFLT *trig;  // usend only remove_block.k as trigger
    // private
    CSN_ARRAY *array;
    K_DATA k_data;
} CSN_TAKE;

/* Two-argument take: flat index in, scalar out. No handle, so no deinit. */
typedef struct {
    OPDS h;
    // outputs
    MYFLT *value;
    // inputs
    CSNREF *source_handle;
    MYFLT *index;
    MYFLT *trig;  // usend only remove_block.k as trigger
    // private
    K_DATA k_data;
} CSN_TAKE_FLAT;

typedef struct {
    OPDS h;
    // outputs
    COMPLEXDAT *value;
    // inputs
    CSNREF *source_handle;
    MYFLT *index;
    // private
    K_DATA k_data;
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
    K_DATA k_data;
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
    // private
    K_DATA k_data;
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
    // inputs
    CSNREF *source_handle;
    MYFLT *in_value; // push at the top
    MYFLT *arg_a;    // trig in push_k
                     // index in insert_k
    MYFLT *arg_b;    // trig in insert_k
    // private
    CSN_REGISTRY *registry;
} CSN_PUSH_K;

typedef struct {
    OPDS h;
    // inputs
    CSNREF *source_handle;
    COMPLEXDAT *in_value; // push at the top
    MYFLT *arg_a;    // trig in push_k
                     // index in insert_k
    MYFLT *arg_b;    // trig in insert_k
    // private
    CSN_REGISTRY *registry;
} CSN_PUSHCOMPLEX_K;

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
    MYFLT *out_value;
    // inputs
    CSNREF *source_handle; // remove the top element
    MYFLT *arg_a;    // trig in pop_k
                     // index in remove_k
    MYFLT *arg_b;    // trig in remove_k
    // private
    CSN_REGISTRY *registry;
} CSN_POP_K;

typedef struct {
    OPDS h;
    // outputs
    COMPLEXDAT *out_value;
    // inputs
    CSNREF *source_handle; // remove the top element
    MYFLT *index;          // only for remove (at the index)
} CSN_POPCOMPLEX;

typedef struct {
    OPDS h;
    // outputs
    COMPLEXDAT *out_value;
    // inputs
    CSNREF *source_handle; // remove the top element
    MYFLT *arg_a;    // trig in pop_k
                     // index in remove_k
    MYFLT *arg_b;    // trig in remove_k
    // private
    CSN_REGISTRY *registry;
} CSN_POPCOMPLEX_K;

typedef struct {
    OPDS h;
    // inputs
    CSNREF *source_handle;
    CSNREF *data_handle; // in block
    MYFLT *axis;
    MYFLT *index;
    MYFLT *trig;
    // private
    CSN_REGISTRY *registry;
    CSN_ARRAY *scratch;
} CSN_INSERT_BLOCK;

typedef struct {
    OPDS h;
    // outputs
    CSNREF *handle;
    // inputs
    CSNREF *source_handle;
    CSNREF *data_handle;
    MYFLT *arg_a; // trig for .flat
                  // axis for .block
    MYFLT *arg_b; // trig for .block
    // private
    CSN_ARRAY *array;
    K_DATA k_data;
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
    MYFLT *arg_a; // axis in pad.ax (INOCOUNT > 4)
                  // trig in pad.k
                  // axis in pad.ax.k (INOCOUNT > 5)
    MYFLT *arg_b; // unused in pad and pad.k
                  // trig in pad.ax.k
    // private
    CSN_ARRAY *array;
    K_DATA k_data;
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
    MYFLT *arg_a; // axis in pad.ax.c (INOCOUNT > 4)
                  // trig in pad.c.k
                  // axis in pad.ax.c.k (INOCOUNT > 5)
    MYFLT *arg_b; // unused in pad.c and pad.c.k
                  // trig in pad.ax.c.k
    // private
    CSN_ARRAY *array;
    K_DATA k_data;
} CSN_PADCOMPLEX;

typedef struct {
    OPDS h;
    // inputs
    CSNREF *source_handle;
    MYFLT *before;
    MYFLT *after;
    MYFLT *value;
    MYFLT *arg_a; // axis in pad.ax.in (INOCOUNT > 4), -1 default all axes
                  // trig in pad.in.k
                  // axis in pad.ax.in.k (INOCOUNT > 5)
    MYFLT *arg_b; // unused in pad.in and pad.in.k
                  // trig in pad.ax.in.k
    // private
    CSN_REGISTRY *registry;
    CSN_ARRAY *scratch;
} CSN_PAD_IN;

typedef struct {
    OPDS h;
    // inputs
    CSNREF *source_handle;
    MYFLT *before;
    MYFLT *after;
    COMPLEXDAT *value;
    MYFLT *arg_a; // axis in pad.ax.in.c (INOCOUNT > 4), -1 default all axes
                  // trig in pad.in.c.k
                  // axis in pad.ax.in.c.k (INOCOUNT > 5)
    MYFLT *arg_b; // unused in pad.in.c and pad.in.c.k
                  // trig in pad.ax.in.c.k
    // private
    CSN_REGISTRY *registry;
    CSN_ARRAY *scratch;
} CSN_PADCOMPLEX_IN;

typedef struct {
    OPDS h;
    // outputs
    CSNREF *handle;
    // inputs
    CSNREF *source_handle;
    MYFLT *min;
    MYFLT *max;
    MYFLT *trig;
    // private
    CSN_ARRAY *array;
    K_DATA k_data;
} CSN_CLIP;

typedef struct {
    OPDS h;
    // inputs
    CSNREF *source_handle;
    MYFLT *min;
    MYFLT *max;
    MYFLT *trig;
    // private
    CSN_REGISTRY *registry;
} CSN_CLIP_IN;

typedef struct {
    OPDS h;
    // outputs
    CSNREF *handle;
    // inputs
    CSNREF *source_handle; // array source
    CSNREF *data_handle;   // array of values (source array will compared with data_handle)
    MYFLT *trig;
    // private
    CSN_ARRAY *array;
    K_DATA k_data;
    ARRAY_ELEMENT *buffer;
    size_t buffer_capacity;
} CSN_ARGWHERE;

typedef struct {
    OPDS h;
    // outputs
    CSNREF *handle;
    // inputs
    CSNREF *source_handle; // array source
    MYFLT *cmp_value;     // only for gt, lt, ne
    MYFLT *trig;
    // private
    CSN_ARRAY *array;
    K_DATA k_data;
    ARRAY_ELEMENT *buffer;
    size_t buffer_capacity;
} CSN_COMPARE;

/* The count family returns how many elements matched, never an array, so its
   output is a plain number and it needs no handle or deinit. */
typedef struct {
    OPDS h;
    // outputs
    MYFLT *value;
    // inputs
    CSNREF *source_handle;
    MYFLT *arg_a;     // compared value for cnteq
                      // trig for all
    MYFLT *arg_b;     // trig only for cnteq
    // private
    CSN_REGISTRY *registry;
} CSN_COUNT;

/* Reducing along an axis drops that axis and yields an array. */
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
    double *scratch;
    size_t scratch_capacity;
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
    MYFLT *trig;
    // private
    K_DATA k_data;
    double *scratch;
    size_t scratch_capacity;
} CSN_REDUCTION_SCALAR;

/* The .c and .c.k overloads share the output and input types, so rate alone
   cannot tell them apart: the k form carries a trigger, which both separates
   the signatures and keeps an O(n) reduction off every control period. */
typedef struct {
    OPDS h;
    // outputs
    COMPLEXDAT *value;
    // inputs
    CSNREF *source_handle;
    MYFLT *trig; // .c.k only
    // private
    K_DATA k_data;
} CSN_REDUCTION_COMPLEX_S;

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
} CSN_BINOP_HH;

typedef struct {
    OPDS h;
    // outputs
    MYFLT *value;
    // inputs
    CSNREF *source_handle_a;
    CSNREF *source_handle_b;
    MYFLT *arg_a; // trig (all)
                  // dist order (order Minkowski)
    MYFLT *arg_b; // trig for dist
    // private
    CSN_REGISTRY *registry;
} CSN_BINOP_HH_SCALAR;

typedef struct {
    OPDS h;
    // outputs
    COMPLEXDAT *value;
    // inputs
    CSNREF *source_handle_a;
    CSNREF *source_handle_b;
    MYFLT *trig;
    // private
    CSN_REGISTRY *registry;
} CSN_BINOPCOMPLEX_HH_SCALAR;

/* The three binop structs share this prefix and tail, which is what lets one
   deinit and one helper serve all of them. Only the order of the two input
   slots differs, so those are always passed explicitly, never read off a cast. */
typedef struct {
    OPDS h;
    // outputs
    CSNREF *handle;
    void *arg_a;
    void *arg_b;
    void *arg_c;
    // private
    CSN_ARRAY *array;
    K_DATA k_data;
} CSN_BINOP_COMMON;

typedef struct {
    OPDS h;
    // outputs
    CSNREF *handle;
    // inputs
    CSNREF *source_handle;
    MYFLT *scalar;
    MYFLT *trig;
    // private
    CSN_ARRAY *array;
    K_DATA k_data;
} CSN_BINOP_HS;

typedef struct {
    OPDS h;
    // outputs
    CSNREF *handle;
    // inputs
    CSNREF *source_handle;
    COMPLEXDAT *scalar;
    MYFLT *trig;
    // private
    CSN_ARRAY *array;
    K_DATA k_data;
} CSN_BINOPCOMPLEX_HS;


typedef struct {
    void *scratch;
    size_t scratch_capacity;
} CSN_SCRATCH;

typedef struct {
    OPDS h;
    // outputs
    CSNREF *handle;
    // inputs
    CSNREF *source_handle;
    MYFLT *axis;
    MYFLT *order;
    MYFLT *trig;
    // private
    CSN_ARRAY *array;
    K_DATA k_data;
    CSN_SCRATCH scratch;
} CSN_UNARYOP_AX;

typedef struct {
    OPDS h;
    // inputs
    CSNREF *source_handle;
    MYFLT *axis;
    MYFLT *order;
    MYFLT *trig;
    // private
    K_DATA k_data;
    CSN_SCRATCH scratch;
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
    MYFLT *trig;
    // private
    CSN_ARRAY *array;
    K_DATA k_data;
} CSN_BINOP_SH;

typedef struct {
    OPDS h;
    // outputs
    CSNREF *handle;
    // inputs
    COMPLEXDAT *scalar;
    CSNREF *source_handle;
    MYFLT *trig;
    // private
    CSN_ARRAY *array;
    K_DATA k_data;
} CSN_BINOPCOMPLEX_SH;

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
} CSN_UNARYOP;

typedef struct {
    OPDS h;
    // outputs
    MYFLT *value;
    // inputs
    CSNREF *source_handle;
    MYFLT *trig;
    // private
    CSN_REGISTRY *registry;
} CSN_UNARYOP_SCALAR;

typedef struct {
    OPDS h;
    // outputs
    COMPLEXDAT *value;
    // inputs
    CSNREF *source_handle;
    MYFLT *trig;
    // private
    CSN_REGISTRY *registry;
} CSN_UNARYOPCOMPLEX_SCALAR;

typedef struct {
    OPDS h;
    // inputs
    CSNREF *source_handle;
    MYFLT *trig;
    // private
    CSN_REGISTRY *registry;
} CSN_UNARYOP_IN;

typedef struct {
    OPDS h;
    // outputs
    CSNREF *handle;
    // inputs
    CSNREF *source_handle;
    MYFLT *axis;
    MYFLT *order;
    MYFLT *trig;
    // private
    CSN_ARRAY *array;
    K_DATA k_data;
    double *scratch;
    size_t scratch_capacity;
} CSN_NORM_REDUCTION;

typedef struct {
    OPDS h;
    // outputs
    MYFLT *value;
    // inputs
    CSNREF *source_handle;
    MYFLT *order;
    MYFLT *trig;
    // private
    CSN_REGISTRY *registry;
} CSN_NORM_REDUCTION_SCALAR;

typedef struct {
    OPDS h;
    // outputs
    CSNREF *handle;
    // inputs
    CSNREF *source_handle;
    MYFLT *winsize;
    MYFLT *axis;
    MYFLT *trig;
    // private
    CSN_ARRAY *array;
    K_DATA k_data;
    double *median_buffer;
    size_t median_buffer_capacity;
} CSN_MOVSTATS;

typedef struct {
    OPDS h;
    // inputs
    CSNREF *source_handle;
    MYFLT *winsize;
    MYFLT *axis;
    MYFLT *trig;
    // private
    CSN_REGISTRY *registry;
    double *median_buffer;
    size_t median_buffer_capacity;
} CSN_MOVSTATS_IN;

typedef struct {
    OPDS h;
    // outputs
    CSNREF *handle;
    // inputs
    CSNREF *source_handle;
    MYFLT *arg_a;  // in angle is trig
                   // in wrap is period zero-centered
                   // in unwrap is period (see numpy)
    MYFLT *arg_b;  // in wrap is trig
                   // discount for unwrap
    MYFLT *arg_c;  // axis
    MYFLT *arg_d;  // trig in unwrap
    // private
    CSN_ARRAY *array;
    K_DATA k_data;
} CSN_ANGLE;

typedef struct {
    OPDS h;
    // inputs
    CSNREF *source_handle;
    MYFLT *arg_a;  // in angle is trig
                   // in wrap is period zero-centered
                   // in unwrap is period (see numpy)
    MYFLT *arg_b;  // in wrap is trig
                   // discount for unwrap
    MYFLT *arg_c;  // axis
    MYFLT *arg_d;  // trig in unwrap
    // private
    CSN_REGISTRY *registry;
} CSN_ANGLE_IN;

typedef struct {
    OPDS h;
    // outputs
    CSNREF *handle;
    // inputs
    CSNREF *source_handle;
    MYFLT *quantity;
    MYFLT *axis;
    MYFLT *trig;
    // private
    CSN_ARRAY *array;
    K_DATA k_data;
    double *buffer;
    size_t buffer_capacity;
} CSN_PERCQUANT_AX;

typedef struct {
    OPDS h;
    // outputs
    MYFLT *value;
    // inputs
    CSNREF *source_handle;
    MYFLT *quantity;
    MYFLT *trig;
    // private
    CSN_REGISTRY *registry;
    double *buffer;
    size_t buffer_capacity;
} CSN_PERCQUANT;

// i-rate
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
int32_t csnarray_not_equal(CSOUND *csound, CSN_COMPARE *p);
int32_t csnarray_greater_equal(CSOUND *csound, CSN_COMPARE *p);
int32_t csnarray_less_equal(CSOUND *csound, CSN_COMPARE *p);
int32_t csnarray_equal(CSOUND *csound, CSN_COMPARE *p);// return array 1D
int32_t csnarray_count_equal(CSOUND *csound, CSN_COUNT *p);     // return count value
int32_t csnarray_count_nonzero(CSOUND *csound, CSN_COUNT *p);   // return count value
int32_t csnarray_count_nan(CSOUND *csound, CSN_COUNT *p);       // return count value
int32_t csnarray_copy(CSOUND *csound, CSN_UNARYOP *p);
int32_t csnarray_reverse(CSOUND *csound, CSN_UNARYOP *p);
int32_t csnarray_reverse_in(CSOUND *csound, CSN_UNARYOP_IN *p);
int32_t csnarray_sort(CSOUND *csound, CSN_UNARYOP_AX *p);
int32_t csnarray_sort_in(CSOUND *csound, CSN_UNARYOP_AX_IN *p);
int32_t csnarray_argsort(CSOUND *csound, CSN_UNARYOP_AX *p);

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
int32_t csnarray_percentile(CSOUND *csound, CSN_PERCQUANT_AX *p);
int32_t csnarray_quantile(CSOUND *csound, CSN_PERCQUANT_AX *p);
int32_t csnarray_percentile_scalar(CSOUND *csound, CSN_PERCQUANT *p);
int32_t csnarray_quantile_scalar(CSOUND *csound, CSN_PERCQUANT *p);

// ELEMENTS
int32_t csnarray_add_hh(CSOUND *csound, CSN_BINOP_HH *p);
int32_t csnarray_add_hs(CSOUND *csound, CSN_BINOP_HS *p);
int32_t csnarray_addcomp_hs(CSOUND *csound, CSN_BINOPCOMPLEX_HS *p);
int32_t csnarray_subtract_hh(CSOUND *csound, CSN_BINOP_HH *p);
int32_t csnarray_subtract_hs(CSOUND *csound, CSN_BINOP_HS *p);
int32_t csnarray_subtract_sh(CSOUND *csound, CSN_BINOP_SH *p);
int32_t csnarray_subtractcomp_hs(CSOUND *csound, CSN_BINOPCOMPLEX_HS *p);
int32_t csnarray_subtractcomp_sh(CSOUND *csound, CSN_BINOPCOMPLEX_SH *p);
int32_t csnarray_mul_hh(CSOUND *csound, CSN_BINOP_HH *p);
int32_t csnarray_mul_hs(CSOUND *csound, CSN_BINOP_HS *p);
int32_t csnarray_mulcomp_hs(CSOUND *csound, CSN_BINOPCOMPLEX_HS *p);
int32_t csnarray_div_hh(CSOUND *csound, CSN_BINOP_HH *p);
int32_t csnarray_div_hs(CSOUND *csound, CSN_BINOP_HS *p);
int32_t csnarray_div_sh(CSOUND *csound, CSN_BINOP_SH *p);
int32_t csnarray_divcomp_hs(CSOUND *csound, CSN_BINOPCOMPLEX_HS *p);
int32_t csnarray_divcomp_sh(CSOUND *csound, CSN_BINOPCOMPLEX_SH *p);
int32_t csnarray_pow_hh(CSOUND *csound, CSN_BINOP_HH *p);
int32_t csnarray_pow_hs(CSOUND *csound, CSN_BINOP_HS *p);
int32_t csnarray_pow_sh(CSOUND *csound, CSN_BINOP_SH *p);
int32_t csnarray_powcomp_hs(CSOUND *csound, CSN_BINOPCOMPLEX_HS *p);
int32_t csnarray_powcomp_sh(CSOUND *csound, CSN_BINOPCOMPLEX_SH *p);
int32_t csnarray_log_hh(CSOUND *csound, CSN_BINOP_HH *p);
int32_t csnarray_log_sh(CSOUND *csound, CSN_BINOP_SH *p);
int32_t csnarray_log_hs(CSOUND *csound, CSN_BINOP_HS *p); // use base
int32_t csnarray_logcomp_sh(CSOUND *csound, CSN_BINOPCOMPLEX_SH *p);
int32_t csnarray_logcomp_hs(CSOUND *csound, CSN_BINOPCOMPLEX_HS *p); // use base
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
int32_t csnarray_sign(CSOUND *csound, CSN_UNARYOP *p);
int32_t csnarray_floor(CSOUND *csound, CSN_UNARYOP *p);
int32_t csnarray_ceil(CSOUND *csound, CSN_UNARYOP *p);
int32_t csnarray_round(CSOUND *csound, CSN_UNARYOP *p);
int32_t csnarray_logical_and_hh(CSOUND *csound, CSN_BINOP_HH *p);
int32_t csnarray_logical_or_hh(CSOUND *csound, CSN_BINOP_HH *p);
int32_t csnarray_logical_and_hs(CSOUND *csound, CSN_BINOP_HS *p);
int32_t csnarray_logical_or_hs(CSOUND *csound, CSN_BINOP_HS *p);
int32_t csnarray_logical_and_sh(CSOUND *csound, CSN_BINOP_SH *p);
int32_t csnarray_logical_or_sh(CSOUND *csound, CSN_BINOP_SH *p);
int32_t csnarray_logical_not(CSOUND *csound, CSN_UNARYOP *p);

// VECTORIAL
int32_t csnarray_dot(CSOUND *csound, CSN_BINOP_HH *p);
int32_t csnarray_dot_scalar(CSOUND *csound, CSN_BINOP_HH_SCALAR *p); // return a scalar
int32_t csnarray_inner(CSOUND *csound, CSN_BINOP_HH *p);
int32_t csnarray_inner_scalar(CSOUND *csound, CSN_BINOP_HH_SCALAR *p); // return a scalar
int32_t csnarray_dotcomp_scalar(CSOUND *csound, CSN_BINOPCOMPLEX_HH_SCALAR *p);
int32_t csnarray_innercomp_scalar(CSOUND *csound, CSN_BINOPCOMPLEX_HH_SCALAR *p);
int32_t csnarray_outer(CSOUND *csound, CSN_BINOP_HH *p);
int32_t csnarray_norm(CSOUND *csound, CSN_NORM_REDUCTION *p); // generalized norm order (Minkowski)
int32_t csnarray_norm_scalar(CSOUND *csound, CSN_NORM_REDUCTION_SCALAR *p); // specify axis -1 -> all
int32_t csnarray_normalize(CSOUND *csound, CSN_UNARYOP_AX *p); // specify axis -1 -> all
int32_t csnarray_normalize_in(CSOUND *csound, CSN_UNARYOP_AX_IN *p); // specify axis -1 -> all
int32_t csnarray_distance(CSOUND *csound, CSN_BINOP_HH_SCALAR *p); // only between vecton in the same space
int32_t csnarray_pair_distance(CSOUND *csound, CSN_BINOP_HH *p); // only between vecton in the same space
int32_t csnarray_angle_distance(CSOUND *csound, CSN_BINOP_HH_SCALAR *p);
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
int32_t csnarray_tracecomp(CSOUND *csound, CSN_UNARYOPCOMPLEX_SCALAR *p);
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
int32_t csnarray_real(CSOUND *csound, CSN_UNARYOP *p);
int32_t csnarray_imag(CSOUND *csound, CSN_UNARYOP *p);
int32_t csnarray_complex_to_real(CSOUND *csound, CSN_UNARYOP *p);
int32_t csnarray_real_to_complex(CSOUND *csound, CSN_UNARYOP *p);
int32_t csnarray_angle(CSOUND *csound, CSN_ANGLE *p);
int32_t csnarray_wrap_angle(CSOUND *csound, CSN_ANGLE *p);
int32_t csnarray_unwrap_angle(CSOUND *csound, CSN_ANGLE *p);
int32_t csnarray_wrap_angle_in(CSOUND *csound, CSN_ANGLE_IN *p);
int32_t csnarray_unwrap_angle_in(CSOUND *csound, CSN_ANGLE_IN *p);
int32_t csnarray_conj(CSOUND *csound, CSN_UNARYOP *p);
int32_t csnarray_type(CSOUND *csound, CSN_UNARYOP_SCALAR *p); // return 0 for real array and 1 for complex array



// k-rate
// CREATION
int32_t create_empty_csnarray_k(CSOUND *csound, CSN_ARR_INIT *p);
int32_t create_zeros_csnarray_k(CSOUND *csound, CSN_ARR_INIT *p);
int32_t create_ones_csnarray_k(CSOUND *csound, CSN_ARR_INIT *p);
int32_t create_like_csnarray_k(CSOUND *csound, CSN_ARR_INIT_LIKE *p);
int32_t create_full_csnarray_k(CSOUND *csound, CSN_FULL *p);
int32_t create_fullcomp_csnarray_k(CSOUND *csound, CSN_FULLCOMPLEX *p);
int32_t create_random_csnarray_k(CSOUND *csound, CSN_ARR_RND_INIT *p);
int32_t from_array_to_csnarray_k(CSOUND *csound, CSN_FROM_ARRAY *p);
int32_t from_complexarray_to_csnarray_k(CSOUND *csound, CSN_FROM_ARRAY *p);
int32_t from_csnarray_to_array_k(CSOUND *csound, CSN_TO_ARRAY *p);
int32_t from_csnarray_to_complexarray_k(CSOUND *csound, CSN_TO_ARRAY *p);
int32_t csnarray_arange_k(CSOUND *csound, CSN_SPACED_SPACE *p);
int32_t csnarray_linspace_k(CSOUND *csound, CSN_SPACED_SPACE *p);
int32_t csnarray_logspace_k(CSOUND *csound, CSN_SPACED_SPACE *p);
int32_t csnarray_geomspace_k(CSOUND *csound, CSN_SPACED_SPACE *p);

// SHAPE
int32_t csnarray_dims_k(CSOUND *csound, CSN_SIZE_DIMS *p);
int32_t csnarray_size_k(CSOUND *csound, CSN_SIZE_DIMS *p);
int32_t csnarray_is_empty_k(CSOUND *csound, CSN_SIZE_DIMS *p);
int32_t csnarray_shape_k(CSOUND *csound, CSN_SHAPE *p);
int32_t csnarray_reshape_k(CSOUND *csound, CSN_RESHAPE *p);
int32_t csnarray_reshape_in_k(CSOUND *csound, CSN_RESHAPE_IN *p);
int32_t csnarray_flatten_k(CSOUND *csound, CSN_RESHAPE *p);
int32_t csnarray_flatten_in_k(CSOUND *csound, CSN_RESHAPE_IN *p);
int32_t csnarray_transpose_k(CSOUND *csound, CSN_RESHAPE *p);
int32_t csnarray_transpose_in_k(CSOUND *csound, CSN_RESHAPE_IN *p);
int32_t csnarray_flip_k(CSOUND *csound, CSN_FLIP_ROLL *p);
int32_t csnarray_flip_in_k(CSOUND *csound, CSN_FLIP_ROLL_IN *p);
int32_t csnarray_roll_k(CSOUND *csound, CSN_FLIP_ROLL *p);
int32_t csnarray_roll_in_k(CSOUND *csound, CSN_FLIP_ROLL_IN *p);
int32_t csnarray_rollaxis_k(CSOUND *csound, CSN_FLIP_ROLL *p);
int32_t csnarray_rollaxis_in_k(CSOUND *csound, CSN_FLIP_ROLL_IN *p);

// INDEXING
int32_t csnarray_get_k(CSOUND *csound, CSN_GET *p);
int32_t csnarray_get_complex_k(CSOUND *csound, CSN_GETCOMPLEX *p);
int32_t csnarray_set_k(CSOUND *csound, CSN_SET *p);
int32_t csnarray_set_complex_k(CSOUND *csound, CSN_SETCOMPLEX *p);
int32_t csnarray_take_k(CSOUND *csound, CSN_TAKE *p);
int32_t csnarray_take_flat_k(CSOUND *csound, CSN_TAKE_FLAT *p);
int32_t csnarray_takecomp_flat_k(CSOUND *csound, CSN_TAKECOMPLEX_FLAT *p);
int32_t csnarray_get_slice_k(CSOUND *csound, CSN_GET_SLICE *p);
int32_t csnarray_set_slice_k(CSOUND *csound, CSN_SET_SLICE *p);
int32_t csnarray_push_k(CSOUND *csound, CSN_PUSH_K *p);
int32_t csnarray_pushcomp_k(CSOUND *csound, CSN_PUSHCOMPLEX_K *p);
int32_t csnarray_pop_k(CSOUND *csound, CSN_POP_K *p);
int32_t csnarray_popcomp_k(CSOUND *csound, CSN_POPCOMPLEX_K *p);
int32_t csnarray_insert_k(CSOUND *csound, CSN_PUSH_K *p);
int32_t csnarray_insertcomp_k(CSOUND *csound, CSN_PUSHCOMPLEX_K *p);
int32_t csnarray_remove_k(CSOUND *csound, CSN_POP_K *p);
int32_t csnarray_removecomp_k(CSOUND *csound, CSN_POPCOMPLEX_K *p);
int32_t csnarray_insert_block_k(CSOUND *csound, CSN_INSERT_BLOCK *p);
int32_t csnarray_remove_block_k(CSOUND *csound, CSN_TAKE *p);
int32_t csnarray_concat_flat_k(CSOUND *csound, CSN_CONCAT *p);
int32_t csnarray_concat_block_k(CSOUND *csound, CSN_CONCAT *p);
int32_t csnarray_pad_k(CSOUND *csound, CSN_PAD *p);
int32_t csnarray_pad_in_k(CSOUND *csound, CSN_PAD_IN *p);
int32_t csnarray_padcomp_k(CSOUND *csound, CSN_PADCOMPLEX *p);
int32_t csnarray_padcomp_in_k(CSOUND *csound, CSN_PADCOMPLEX_IN *p);
int32_t csnarray_clip_k(CSOUND *csound, CSN_CLIP *p);
int32_t csnarray_clip_in_k(CSOUND *csound, CSN_CLIP_IN *p);
int32_t csnarray_argwhere_k(CSOUND *csound, CSN_ARGWHERE *p);     // return (count, ndim)
int32_t csnarray_argnonzero_k(CSOUND *csound, CSN_ARGWHERE *p);   // return (count, ndim)
int32_t csnarray_argunique_k(CSOUND *csound, CSN_ARGWHERE *p);    // return (count, ndim)
int32_t csnarray_argisnan_k(CSOUND *csound, CSN_ARGWHERE *p);     // return (count, ndim)
int32_t csnarray_argmin_k(CSOUND *csound, CSN_REDUCTION *p);      // return (1, ndim) if axis == -1 else (shape[axis], ndim)
int32_t csnarray_argmax_k(CSOUND *csound, CSN_REDUCTION *p);      // return (1, ndim) if axis == -1 else (shape[axis], ndim)
int32_t csnarray_unique_k(CSOUND *csound, CSN_COMPARE *p);        // return array 1D
int32_t csnarray_greater_than_k(CSOUND *csound, CSN_COMPARE *p);  // return array 1D
int32_t csnarray_less_than_k(CSOUND *csound, CSN_COMPARE *p);     // return array 1D
int32_t csnarray_not_equal_k(CSOUND *csound, CSN_COMPARE *p);
int32_t csnarray_greater_equal_k(CSOUND *csound, CSN_COMPARE *p);
int32_t csnarray_less_equal_k(CSOUND *csound, CSN_COMPARE *p);
int32_t csnarray_equal_k(CSOUND *csound, CSN_COMPARE *p);// return array 1D
int32_t csnarray_count_equal_k(CSOUND *csound, CSN_COUNT *p);     // return count value
int32_t csnarray_count_nonzero_k(CSOUND *csound, CSN_COUNT *p);   // return count value
int32_t csnarray_count_nan_k(CSOUND *csound, CSN_COUNT *p);       // return count value
int32_t csnarray_copy_k(CSOUND *csound, CSN_UNARYOP *p);
int32_t csnarray_reverse_k(CSOUND *csound, CSN_UNARYOP *p);
int32_t csnarray_reverse_in_k(CSOUND *csound, CSN_UNARYOP_IN *p);
int32_t csnarray_sort_k(CSOUND *csound, CSN_UNARYOP_AX *p);
int32_t csnarray_sort_in_k(CSOUND *csound, CSN_UNARYOP_AX_IN *p);
int32_t csnarray_argsort_k(CSOUND *csound, CSN_UNARYOP_AX *p);

// REDUCTION
int32_t csnarray_sum_k(CSOUND *csound, CSN_REDUCTION *p);
int32_t csnarray_sum_all_k(CSOUND *csound, CSN_REDUCTION_SCALAR *p);
int32_t csnarray_sumcomp_all_k(CSOUND *csound, CSN_REDUCTION_COMPLEX_S *p);
int32_t csnarray_prod_k(CSOUND *csound, CSN_REDUCTION *p);
int32_t csnarray_prod_all_k(CSOUND *csound, CSN_REDUCTION_SCALAR *p);
int32_t csnarray_prodcomp_all_k(CSOUND *csound, CSN_REDUCTION_COMPLEX_S *p);
int32_t csnarray_sub_k(CSOUND *csound, CSN_REDUCTION *p);
int32_t csnarray_sub_all_k(CSOUND *csound, CSN_REDUCTION_SCALAR *p);
int32_t csnarray_subcomp_all_k(CSOUND *csound, CSN_REDUCTION_COMPLEX_S *p);
int32_t csnarray_mean_k(CSOUND *csound, CSN_REDUCTION *p);
int32_t csnarray_mean_all_k(CSOUND *csound, CSN_REDUCTION_SCALAR *p);
int32_t csnarray_meancomp_all_k(CSOUND *csound, CSN_REDUCTION_COMPLEX_S *p);
int32_t csnarray_min_k(CSOUND *csound, CSN_REDUCTION *p);
int32_t csnarray_min_all_k(CSOUND *csound, CSN_REDUCTION_SCALAR *p);
int32_t csnarray_max_k(CSOUND *csound, CSN_REDUCTION *p);
int32_t csnarray_max_all_k(CSOUND *csound, CSN_REDUCTION_SCALAR *p);
int32_t csnarray_all_k(CSOUND *csound, CSN_REDUCTION *p);
int32_t csnarray_all_all_k(CSOUND *csound, CSN_REDUCTION_SCALAR *p);
int32_t csnarray_any_k(CSOUND *csound, CSN_REDUCTION *p);
int32_t csnarray_any_all_k(CSOUND *csound, CSN_REDUCTION_SCALAR *p);
int32_t csnarray_median_k(CSOUND *csound, CSN_REDUCTION *p);
int32_t csnarray_median_all_k(CSOUND *csound, CSN_REDUCTION_SCALAR *p);
int32_t csnarray_std_k(CSOUND *csound, CSN_REDUCTION *p);
int32_t csnarray_std_all_k(CSOUND *csound, CSN_REDUCTION_SCALAR *p);
int32_t csnarray_var_k(CSOUND *csound, CSN_REDUCTION *p);
int32_t csnarray_var_all_k(CSOUND *csound, CSN_REDUCTION_SCALAR *p);
int32_t csnarray_percentile_k(CSOUND *csound, CSN_PERCQUANT_AX *p);
int32_t csnarray_quantile_k(CSOUND *csound, CSN_PERCQUANT_AX *p);
int32_t csnarray_percentile_scalar_k(CSOUND *csound, CSN_PERCQUANT *p);
int32_t csnarray_quantile_scalar_k(CSOUND *csound, CSN_PERCQUANT *p);

// ELEMENTS
int32_t csnarray_add_hh_k(CSOUND *csound, CSN_BINOP_HH *p);
int32_t csnarray_add_hs_k(CSOUND *csound, CSN_BINOP_HS *p);
int32_t csnarray_addcomp_hs_k(CSOUND *csound, CSN_BINOPCOMPLEX_HS *p);
int32_t csnarray_subtract_hh_k(CSOUND *csound, CSN_BINOP_HH *p);
int32_t csnarray_subtract_hs_k(CSOUND *csound, CSN_BINOP_HS *p);
int32_t csnarray_subtract_sh_k(CSOUND *csound, CSN_BINOP_SH *p);
int32_t csnarray_subtractcomp_hs_k(CSOUND *csound, CSN_BINOPCOMPLEX_HS *p);
int32_t csnarray_subtractcomp_sh_k(CSOUND *csound, CSN_BINOPCOMPLEX_SH *p);
int32_t csnarray_mul_hh_k(CSOUND *csound, CSN_BINOP_HH *p);
int32_t csnarray_mul_hs_k(CSOUND *csound, CSN_BINOP_HS *p);
int32_t csnarray_mulcomp_hs_k(CSOUND *csound, CSN_BINOPCOMPLEX_HS *p);
int32_t csnarray_div_hh_k(CSOUND *csound, CSN_BINOP_HH *p);
int32_t csnarray_div_hs_k(CSOUND *csound, CSN_BINOP_HS *p);
int32_t csnarray_div_sh_k(CSOUND *csound, CSN_BINOP_SH *p);
int32_t csnarray_divcomp_hs_k(CSOUND *csound, CSN_BINOPCOMPLEX_HS *p);
int32_t csnarray_divcomp_sh_k(CSOUND *csound, CSN_BINOPCOMPLEX_SH *p);
int32_t csnarray_pow_hh_k(CSOUND *csound, CSN_BINOP_HH *p);
int32_t csnarray_pow_hs_k(CSOUND *csound, CSN_BINOP_HS *p);
int32_t csnarray_pow_sh_k(CSOUND *csound, CSN_BINOP_SH *p);
int32_t csnarray_powcomp_hs_k(CSOUND *csound, CSN_BINOPCOMPLEX_HS *p);
int32_t csnarray_powcomp_sh_k(CSOUND *csound, CSN_BINOPCOMPLEX_SH *p);
int32_t csnarray_log_hh_k(CSOUND *csound, CSN_BINOP_HH *p);
int32_t csnarray_log_sh_k(CSOUND *csound, CSN_BINOP_SH *p);
int32_t csnarray_log_hs_k(CSOUND *csound, CSN_BINOP_HS *p); // use base
int32_t csnarray_logcomp_sh_k(CSOUND *csound, CSN_BINOPCOMPLEX_SH *p);
int32_t csnarray_logcomp_hs_k(CSOUND *csound, CSN_BINOPCOMPLEX_HS *p); // use base
int32_t csnarray_sqrt_k(CSOUND *csound, CSN_UNARYOP *p);
int32_t csnarray_cbrt_k(CSOUND *csound, CSN_UNARYOP *p);
int32_t csnarray_abs_k(CSOUND *csound, CSN_UNARYOP *p);
int32_t csnarray_exp_k(CSOUND *csound, CSN_UNARYOP *p);
int32_t csnarray_sin_k(CSOUND *csound, CSN_UNARYOP *p);
int32_t csnarray_cos_k(CSOUND *csound, CSN_UNARYOP *p);
int32_t csnarray_tan_k(CSOUND *csound, CSN_UNARYOP *p);
int32_t csnarray_asin_k(CSOUND *csound, CSN_UNARYOP *p);
int32_t csnarray_acos_k(CSOUND *csound, CSN_UNARYOP *p);
int32_t csnarray_atan_k(CSOUND *csound, CSN_UNARYOP *p);
int32_t csnarray_sinh_k(CSOUND *csound, CSN_UNARYOP *p);
int32_t csnarray_cosh_k(CSOUND *csound, CSN_UNARYOP *p);
int32_t csnarray_tanh_k(CSOUND *csound, CSN_UNARYOP *p);
int32_t csnarray_asinh_k(CSOUND *csound, CSN_UNARYOP *p);
int32_t csnarray_acosh_k(CSOUND *csound, CSN_UNARYOP *p);
int32_t csnarray_atanh_k(CSOUND *csound, CSN_UNARYOP *p);
int32_t csnarray_sign_k(CSOUND *csound, CSN_UNARYOP *p);
int32_t csnarray_floor_k(CSOUND *csound, CSN_UNARYOP *p);
int32_t csnarray_ceil_k(CSOUND *csound, CSN_UNARYOP *p);
int32_t csnarray_round_k(CSOUND *csound, CSN_UNARYOP *p);
int32_t csnarray_logical_and_hh_k(CSOUND *csound, CSN_BINOP_HH *p);
int32_t csnarray_logical_or_hh_k(CSOUND *csound, CSN_BINOP_HH *p);
int32_t csnarray_logical_and_hs_k(CSOUND *csound, CSN_BINOP_HS *p);
int32_t csnarray_logical_or_hs_k(CSOUND *csound, CSN_BINOP_HS *p);
int32_t csnarray_logical_and_sh_k(CSOUND *csound, CSN_BINOP_SH *p);
int32_t csnarray_logical_or_sh_k(CSOUND *csound, CSN_BINOP_SH *p);
int32_t csnarray_logical_not_k(CSOUND *csound, CSN_UNARYOP *p);


// VECTORIAL
int32_t csnarray_dot_k(CSOUND *csound, CSN_BINOP_HH *p);
int32_t csnarray_dot_scalar_k(CSOUND *csound, CSN_BINOP_HH_SCALAR *p); // return a scalar
int32_t csnarray_inner_k(CSOUND *csound, CSN_BINOP_HH *p);
int32_t csnarray_inner_scalar_k(CSOUND *csound, CSN_BINOP_HH_SCALAR *p); // return a scalar
int32_t csnarray_dotcomp_scalar_k(CSOUND *csound, CSN_BINOPCOMPLEX_HH_SCALAR *p);
int32_t csnarray_innercomp_scalar_k(CSOUND *csound, CSN_BINOPCOMPLEX_HH_SCALAR *p);
int32_t csnarray_outer_k(CSOUND *csound, CSN_BINOP_HH *p);
int32_t csnarray_norm_k(CSOUND *csound, CSN_NORM_REDUCTION *p); // generalized norm order (Minkowski)
int32_t csnarray_norm_scalar_k(CSOUND *csound, CSN_NORM_REDUCTION_SCALAR *p); // specify axis -1 -> all
int32_t csnarray_normalize_k(CSOUND *csound, CSN_UNARYOP_AX *p); // specify axis -1 -> all
int32_t csnarray_normalize_in_k(CSOUND *csound, CSN_UNARYOP_AX_IN *p); // specify axis -1 -> all
int32_t csnarray_distance_k(CSOUND *csound, CSN_BINOP_HH_SCALAR *p); // only between vecton in the same space
int32_t csnarray_pair_distance_k(CSOUND *csound, CSN_BINOP_HH *p); // only between vecton in the same space
int32_t csnarray_angle_distance_k(CSOUND *csound, CSN_BINOP_HH_SCALAR *p);
int32_t csnarray_project_k(CSOUND *csound, CSN_BINOP_HH *p);
int32_t csnarray_reject_k(CSOUND *csound, CSN_BINOP_HH *p);
int32_t csnarray_reflect_k(CSOUND *csound, CSN_BINOP_HH *p);
int32_t csnarray_cross_k(CSOUND *csound, CSN_BINOP_HH *p); // only 1-D with size = 3

// NUMERIC ANALYSIS
int32_t csnarray_diff_k(CSOUND *csound, CSN_UNARYOP_AX *p);
int32_t csnarray_gradient_k(CSOUND *csound, CSN_UNARYOP_AX *p);
int32_t csnarray_cumsum_k(CSOUND *csound, CSN_UNARYOP_AX *p);
int32_t csnarray_cumprod_k(CSOUND *csound, CSN_UNARYOP_AX *p);

// MATRIX
int32_t csnarray_identity_k(CSOUND *csound, CSN_IDENTITY *p);
int32_t csnarray_matmul_k(CSOUND *csound, CSN_BINOP_HH *p); // as numpy (broadcast last two dims)
int32_t csnarray_matmul_scalar_k(CSOUND *csound, CSN_BINOP_HH_SCALAR *p);
int32_t csnarray_trace_k(CSOUND *csound, CSN_UNARYOP_SCALAR *p); // only 2D
int32_t csnarray_tracecomp_k(CSOUND *csound, CSN_UNARYOPCOMPLEX_SCALAR *p);
int32_t csnarray_diag_k(CSOUND *csound, CSN_UNARYOP *p); // with 1D -> 2D, with 2D -> 1D

// STATS
int32_t csnarray_movmean_k(CSOUND *csound, CSN_MOVSTATS *p); // auto edges managment (see movemean_slice() function)
int32_t csnarray_movmedian_k(CSOUND *csound, CSN_MOVSTATS *p);
int32_t csnarray_movstd_k(CSOUND *csound, CSN_MOVSTATS *p);
int32_t csnarray_movvar_k(CSOUND *csound, CSN_MOVSTATS *p);
int32_t csnarray_movmin_k(CSOUND *csound, CSN_MOVSTATS *p);
int32_t csnarray_movmax_k(CSOUND *csound, CSN_MOVSTATS *p);
int32_t csnarray_movmean_in_k(CSOUND *csound, CSN_MOVSTATS_IN *p); // in-place
int32_t csnarray_movmedian_in_k(CSOUND *csound, CSN_MOVSTATS_IN *p);
int32_t csnarray_movstd_in_k(CSOUND *csound, CSN_MOVSTATS_IN *p);
int32_t csnarray_movvar_in_k(CSOUND *csound, CSN_MOVSTATS_IN *p);
int32_t csnarray_movmin_in_k(CSOUND *csound, CSN_MOVSTATS_IN *p);
int32_t csnarray_movmax_in_k(CSOUND *csound, CSN_MOVSTATS_IN *p);

// COMPLEX
int32_t csnarray_real_k(CSOUND *csound, CSN_UNARYOP *p);
int32_t csnarray_imag_k(CSOUND *csound, CSN_UNARYOP *p);
int32_t csnarray_complex_to_real_k(CSOUND *csound, CSN_UNARYOP *p);
int32_t csnarray_real_to_complex_k(CSOUND *csound, CSN_UNARYOP *p);
int32_t csnarray_angle_k(CSOUND *csound, CSN_ANGLE *p);
int32_t csnarray_wrap_angle_k(CSOUND *csound, CSN_ANGLE *p);
int32_t csnarray_unwrap_angle_k(CSOUND *csound, CSN_ANGLE *p);
int32_t csnarray_wrap_angle_in_k(CSOUND *csound, CSN_ANGLE_IN *p);
int32_t csnarray_unwrap_angle_in_k(CSOUND *csound, CSN_ANGLE_IN *p);
int32_t csnarray_conj_k(CSOUND *csound, CSN_UNARYOP *p);
int32_t csnarray_type_k(CSOUND *csound, CSN_UNARYOP_SCALAR *p); // return 0 for real array and 1 for complex array

#endif
