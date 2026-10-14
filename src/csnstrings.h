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

typedef struct {
    OPDS h;
    // outputs
    CSN_ARRAY *handle;
    // inputs
    CSN_ARRAY *source_handle;
    cs_float *before;
    cs_float *after;
    STRINGDAT *value;
    cs_float *arg_a; // axis in pad.ax (INOCOUNT > 4), trigger in pad[.ax].k
    cs_float *arg_b; // axis in pad.ax.k (INOCOUNT > 5)
    // private
    K_DATA k_data;
} CSN_PADSTRING;

typedef struct {
    OPDS h;
    // inputs
    CSN_ARRAY *source_handle;
    cs_float *before;
    cs_float *after;
    STRINGDAT *value;
    cs_float *arg_a; // axis in pad.ax.in (INOCOUNT > 4), trigger in pad[.ax].in.k
    cs_float *arg_b; // axis in pad.ax.in.k (INOCOUNT > 5)
    // private
    CSN_REGISTRY *registry;
    CSN_ARRAY *scratch;
    K_DATA k_data;
} CSN_PADSTRING_IN;

typedef struct {
    OPDS h;
    // outputs
    STRINGDAT *value;
    // inputs
    CSN_ARRAY *source_handle;
    cs_float *index;
    // private
    CSN_REGISTRY *registry;
} CSN_TAKESTRING_FLAT;

typedef struct {
    OPDS h;
    // inputs
    CSN_ARRAY *source_handle;
    STRINGDAT *in_value; // push at the top
    cs_float *index;    // only for put (at index)
} CSN_PUSHSTRING;

typedef struct {
    OPDS h;
    // inputs
    CSN_ARRAY *source_handle;
    STRINGDAT *in_value; // push at the top
    cs_float *arg_a;    // trig in push_k
                     // index in insert_k
    cs_float *arg_b;    // trig in insert_k
    // private
    CSN_REGISTRY *registry;
} CSN_PUSHSTRING_K;

typedef struct {
    OPDS h;
    // outputs
    STRINGDAT *out_value;
    // inputs
    CSN_ARRAY *source_handle; // remove the top element
    cs_float *index;         // only for remove (at the index)
} CSN_POPSTRING;

typedef struct {
    OPDS h;
    // outputs
    STRINGDAT *out_value;
    // inputs
    CSN_ARRAY *source_handle; // remove the top element
    cs_float *arg_a;    // trig in pop_k
                     // index in remove_k
    cs_float *arg_b;    // trig in remove_k
    // private
    CSN_REGISTRY *registry;
} CSN_POPSTRING_K;

typedef struct {
    OPDS h;
    // outputs
    CSN_ARRAY *handle;
    // inputs
    CSN_ARRAY *source_handle;       // mask
    CSN_ARRAY *source_handle_true;
    STRINGDAT *source_string_false;
    cs_float *trig;
    // private
    K_DATA k_data;
    CSN_SCRATCH scratch;
    CSN_THREE_VERSION_K_STATE versions;
    bool is_published;
} CSN_WHERESTRING_HS;

typedef struct {
    OPDS h;
    // inputs
    CSN_ARRAY *dest_handle;
    CSN_ARRAY *mask_handle;
    void *values;           // CSN_ARRAY * in putmask_hh, STRINGDAT * in putmask_hs
    cs_float *trig;
    // private
    CSN_REGISTRY *registry;
} CSN_PUTMASKSTRING;

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

int32_t check_strings_not_rt(CSOUND *csound, OPDS *h, const CSN_ARRAY *out);
int32_t transpose_stringdata_assign(CSOUND *csound, char **source, char **destination, size_t size, uint32_t ndim, const uint32_t *shape, const size_t *strides, const uint32_t *axes, bool in_place);
void swap_strings(char **data, size_t a, size_t b);
void rotate_string_run(char **data, size_t base, size_t count, size_t stride, int32_t shift);
void reverse_string_in_place(CSN_ARRAY *arr);
int32_t concat_block_string_assign_value(CSOUND *csound, const CSN_ARRAY *source_arr, const CSN_ARRAY *data_arr, CSN_ARRAY *destination, uint32_t axis);
int32_t pad_assign_string_value(CSOUND *csound, const CSN_ARRAY *source_arr, CSN_ARRAY *destination, STRINGDAT *value, int32_t axis, uint32_t before);

int32_t csnstrings_pad(CSOUND *csound, CSN_PADSTRING *p);
int32_t csnstrings_pad_k_init(CSOUND *csound, CSN_PADSTRING *p);
int32_t csnstrings_pad_k(CSOUND *csound, CSN_PADSTRING *p);
int32_t csnstrings_pad_in(CSOUND *csound, CSN_PADSTRING_IN *p);
int32_t csnstrings_pad_in_k_init(CSOUND *csound, CSN_PADSTRING_IN *p);
int32_t csnstrings_pad_in_k(CSOUND *csound, CSN_PADSTRING_IN *p);
int32_t csnstrings_pad_k_deinit(CSOUND *csound, CSN_PADSTRING *p);
int32_t csnstrings_pad_in_k_deinit(CSOUND *csound, CSN_PADSTRING_IN *p);
int32_t csnstrings_take_flat(CSOUND *csound, CSN_TAKESTRING_FLAT *p);
int32_t csnstrings_take_flat_k(CSOUND *csound, CSN_TAKESTRING_FLAT *p);

int32_t csnstrings_push(CSOUND *csound, CSN_PUSHSTRING *p);
int32_t csnstrings_push_k_init(CSOUND *csound, CSN_PUSHSTRING_K *p);
int32_t csnstrings_push_k(CSOUND *csound, CSN_PUSHSTRING_K *p);
int32_t csnstrings_pop(CSOUND *csound, CSN_POPSTRING *p);
int32_t csnstrings_pop_k_init(CSOUND *csound, CSN_POPSTRING_K *p);
int32_t csnstrings_pop_k(CSOUND *csound, CSN_POPSTRING_K *p);
int32_t csnstrings_insert(CSOUND *csound, CSN_PUSHSTRING *p);
int32_t csnstrings_insert_k(CSOUND *csound, CSN_PUSHSTRING_K *p);
int32_t csnstrings_remove(CSOUND *csound, CSN_POPSTRING *p);
int32_t csnstrings_remove_k(CSOUND *csound, CSN_POPSTRING_K *p);

int32_t csnstrings_where_hh(CSOUND *csound, CSN_WHERE_HH *p);
int32_t csnstrings_where_hs(CSOUND *csound, CSN_WHERESTRING_HS *p);
int32_t csnstrings_where_hh_k_init(CSOUND *csound, CSN_WHERE_HH *p);
int32_t csnstrings_where_hs_k_init(CSOUND *csound, CSN_WHERESTRING_HS *p);
int32_t csnstrings_where_hh_k(CSOUND *csound, CSN_WHERE_HH *p);
int32_t csnstrings_where_hs_k(CSOUND *csound, CSN_WHERESTRING_HS *p);
int32_t csnstrings_where_deinit(CSOUND *csound, void *p);
int32_t csnstrings_putmask_hh(CSOUND *csound, CSN_PUTMASKSTRING *p);
int32_t csnstrings_putmask_hs(CSOUND *csound, CSN_PUTMASKSTRING *p);
int32_t csnstrings_putmask_k_init(CSOUND *csound, CSN_PUTMASKSTRING *p);
int32_t csnstrings_putmask_hh_k(CSOUND *csound, CSN_PUTMASKSTRING *p);
int32_t csnstrings_putmask_hs_k(CSOUND *csound, CSN_PUTMASKSTRING *p);

#endif
