/* Opcode implementations for the create family.
   The public opcode inventory remains centralized in csnum.c. */
#include "csnum.h"
#include "csnum_internal.h"
#include "csnregistry.h"
#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "arrays.h"


/* Parses the shape argument and prepares the output. The lock, if taken, is
   handed back to the caller, which still has to fill the array under it. */
static CSNARR_RES create_csnarray_from_shape(CSOUND *csound, OPDS *h, CSN_REGISTRY *reg, const ARRAYDAT *source_shape, CSN_ARRAY *out, ITEM_TYPE itype) {
    uint32_t ndim = 0;
    uint32_t shape[CSN_MAX_DIMS] = {0};

    int32_t res_shape = parse_shape_array(csound, source_shape, &ndim, shape);
    if (res_shape != OK) {
        return (CSNARR_RES) { .err = res_shape };
    }

    return prepare_output(csound, h, NULL, reg, out, ndim, shape, itype, NULL, 0, CSN_ALIAS_REFUSE);
}

int32_t create_empty_csnarray(CSOUND *csound, CSN_ARR_INIT *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    CHECK_ITYPE(csound, *p->itype);
    ITEM_TYPE itype = CSN_ITYPE_FROM_ARG(*p->itype);
    uint32_t ndim = 0;
    uint32_t shape[CSN_MAX_DIMS] = {0};
    int32_t res = parse_shape_array(csound, p->shape, &ndim, shape);
    if (res != OK) return res;

    CSNARR_RES pres = prepare_output(csound, &p->h, NULL, reg, p->handle, ndim, shape, itype, NULL, 0, CSN_ALIAS_REFUSE);
    res = pres.err;
    if (res != OK) goto done;
    reset_empty_csnarray(p->handle, ndim, shape, itype);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

static int32_t create_shape_csnarray_k_init(CSOUND *csound, CSN_ARR_INIT *p, CSN_K_SHAPE_INIT_MODE mode) {
    CHECK_ITYPE(csound, *p->itype);
    ITEM_TYPE itype = CSN_ITYPE_FROM_ARG(*p->itype);
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    p->registry = reg;

    uint32_t ndim = 0;
    uint32_t shape[CSN_MAX_DIMS] = {0};
    int32_t res = parse_shape_array(csound, p->shape, &ndim, shape);
    if (res != OK) return res;

    CSNARR_RES pres = prepare_output(csound, &p->h, NULL, reg, p->handle, ndim, shape, itype, NULL, 0, CSN_ALIAS_REFUSE);
    res = pres.err;
    if (res != OK) goto done;

    /* csnempty publishes the reserved shape with no elements; zeros and ones
       publish a filled array, so that reading the handle before the first
       triggered pass gives the same thing the i-rate form would. */
    if (mode == CSN_K_EMPTY) {
        reset_empty_csnarray(p->handle, ndim, shape, itype);
    } else {
        fill_csnarray(p->handle, mode == CSN_K_ONES ? 1.0 : 0.0);
    }

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t create_empty_csnarray_k_init(CSOUND *csound, CSN_ARR_INIT *p) {
    return create_shape_csnarray_k_init(csound, p, CSN_K_EMPTY);
}

int32_t create_zeros_csnarray_k_init(CSOUND *csound, CSN_ARR_INIT *p) {
    return create_shape_csnarray_k_init(csound, p, CSN_K_ZEROS);
}

int32_t create_ones_csnarray_k_init(CSOUND *csound, CSN_ARR_INIT *p) {
    return create_shape_csnarray_k_init(csound, p, CSN_K_ONES);
}

static int32_t create_shape_csnarray_k(CSOUND *csound, CSN_ARR_INIT *p, CSN_K_SHAPE_INIT_MODE mode) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    /* itype is an i-argument, so it cannot change between passes and the init
       has already validated it. */
    ITEM_TYPE itype = CSN_ITYPE_FROM_ARG(*p->itype);

    uint32_t ndim = 0;
    uint32_t shape[CSN_MAX_DIMS] = {0};
    int32_t res = parse_shape_array_k(csound, &p->h, p->shape, &ndim, shape);
    if (res != OK) return res;

    size_t requested_size = 0;
    if (get_array_size_from_shape(&requested_size, ndim, shape) != OK) {
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    /* prepare_output lays the output out for the requested shape, zeroes it
       and bumps its version, refusing growth on a marked array */
    CSNARR_RES pres = prepare_output(csound, &p->h, &p->h, reg, p->handle, ndim, shape, itype, NULL, 0, CSN_ALIAS_REFUSE);
    res = pres.err;
    if (res != OK) goto done;

    /* csnempty reserves the requested shape but publishes no element yet */
    if (mode == CSN_K_EMPTY) {
        reset_empty_csnarray(p->handle, ndim, shape, itype);
    } else if (mode == CSN_K_ONES) {
        fill_csnarray(p->handle, 1.0);
    }

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t create_empty_csnarray_k(CSOUND *csound, CSN_ARR_INIT *p) {
    return create_shape_csnarray_k(csound, p, CSN_K_EMPTY);
}

int32_t create_zeros_csnarray_k(CSOUND *csound, CSN_ARR_INIT *p) {
    return create_shape_csnarray_k(csound, p, CSN_K_ZEROS);
}

int32_t create_ones_csnarray_k(CSOUND *csound, CSN_ARR_INIT *p) {
    return create_shape_csnarray_k(csound, p, CSN_K_ONES);
}

int32_t create_zeros_csnarray(CSOUND *csound, CSN_ARR_INIT *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    CHECK_ITYPE(csound, *p->itype);
    ITEM_TYPE itype = CSN_ITYPE_FROM_ARG(*p->itype);
    /* prepare_output hands the array back already zeroed */
    CSNARR_RES pres = create_csnarray_from_shape(csound, &p->h, reg, p->shape, p->handle, itype);
    if (pres.locked) csn_unlock(csound, reg);
    return pres.err;
}

int32_t create_ones_csnarray(CSOUND *csound, CSN_ARR_INIT *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    CHECK_ITYPE(csound, *p->itype);
    ITEM_TYPE itype = CSN_ITYPE_FROM_ARG(*p->itype);
    CSNARR_RES pres = create_csnarray_from_shape(csound, &p->h, reg, p->shape, p->handle, itype);
    if (pres.err != OK) return pres.err;
    fill_csnarray(p->handle, 1.0);
    if (pres.locked) csn_unlock(csound, reg);
    return OK;
}

int32_t create_like_csnarray_k_init(CSOUND *csound, CSN_ARR_INIT_LIKE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;

    CSN_ARRAY *source_arr = p->handle_from;
    uint32_t ndim = source_arr->ndim;

    uint32_t shape[CSN_MAX_DIMS] = {0};
    memcpy(shape, source_arr->shape, sizeof(uint32_t) * CSN_MAX_DIMS);

    const CSN_ARRAY *operands[1] = { source_arr };
    CSNARR_RES pres = prepare_output(csound, &p->h, NULL, reg, p->handle, ndim, shape, source_arr->itype, operands, 1, CSN_ALIAS_ELEMENTWISE);
    res = pres.err;
    if (res != OK) goto done;

    double fill_value = (double) *p->value;
    fill_csnarray(p->handle, fill_value);
    /* An output that is its own source (ELEMENTWISE) reaches here untouched by
       prepare_output, which then moved no version: the fill has to. */
    if (pres.aliased) update_array_data_version(&p->handle->version);

    p->registry = reg;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t create_like_csnarray_k(CSOUND *csound, CSN_ARR_INIT_LIKE *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    int32_t res = OK;

    CSN_ARRAY *source_arr = p->handle_from;
    uint32_t ndim = source_arr->ndim;
    ITEM_TYPE itype = source_arr->itype;
    if (itype != CSN_REAL && itype != CSN_COMPLEX) {
        return csound->PerfError(csound, &p->h, "[csnarray] Source array has invalid internal type %u", itype);
    }

    uint32_t shape[CSN_MAX_DIMS] = {0};
    memcpy(shape, source_arr->shape, sizeof(uint32_t) * ndim);

    size_t requested_size = 0;
    if (get_array_size_from_shape(&requested_size, ndim, shape) != OK) {
        return csound->PerfError(csound, &p->h, "[csnarray] Source array has an invalid shape");
    }

    const CSN_ARRAY *operands[1] = { source_arr };
    CSNARR_RES pres = prepare_output(csound, &p->h, &p->h, reg, p->handle, ndim, shape, itype, operands, 1, CSN_ALIAS_ELEMENTWISE);
    res = pres.err;
    if (res != OK) goto done;

    /* ELEMENTWISE has already refused an aliased output whose layout would
       change, so the output has the requested layout either way */
    fill_csnarray(p->handle, (double) *p->value);
    if (pres.aliased) update_array_data_version(&p->handle->version);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t create_like_csnarray(CSOUND *csound, CSN_ARR_INIT_LIKE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;

    CSN_ARRAY *source_arr = p->handle_from;
    uint32_t ndim = source_arr->ndim;
    uint32_t shape[CSN_MAX_DIMS] = {0};
    memcpy(shape, source_arr->shape, sizeof(uint32_t) * CSN_MAX_DIMS);

    const CSN_ARRAY *operands[1] = { source_arr };
    CSNARR_RES pres = prepare_output(csound, &p->h, NULL, reg, p->handle, ndim, shape, source_arr->itype, operands, 1, CSN_ALIAS_ELEMENTWISE);
    res = pres.err;
    if (res != OK) goto done;

    double fill_value = (double) *p->value;
    fill_csnarray(p->handle, fill_value);
    if (pres.aliased) update_array_data_version(&p->handle->version);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

static int32_t create_random_csnarray_helper(CSOUND *csound, CSN_ARR_RND_INIT *p, bool is_int) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;

    uint32_t ndim = 0;
    uint32_t shape[CSN_MAX_DIMS] = {0};

    res = parse_shape_array(csound, p->shape, &ndim, shape);
    if (res != OK) return res;

    double min = (double) *p->min;
    double max = (double) *p->max;
    if (min == max || min > max || !IS_VALID_VALUE(min) || !IS_VALID_VALUE(max)) {
        return csound->InitError(csound, "[csnarray] Invalid random range");
    }

    if (is_int && (!IS_VALID_VALUE_INT32(min) || !IS_VALID_VALUE_INT32(max))) {
        return csound->InitError(csound, "[csnarray] Invalid random range");
    }

    CSNARR_RES pres = prepare_output(csound, &p->h, NULL, reg, p->handle, ndim, shape, CSN_REAL, NULL, 0, CSN_ALIAS_REFUSE);
    res = pres.err;
    if (res != OK) goto done;

    /* reg->rng is registry-wide state that every draw mutates, so the fill
       runs under the lock even for a local output: two threads drawing at once
       would otherwise interleave into the generator and could hand out the
       same stream twice. */
    if (!pres.locked) {
        csn_lock(csound, reg);
        pres.locked = true;
    }
    CSN_ARRAY *arr = p->handle;
    for (size_t i = 0; i < arr->size; i++) {
        if (is_int) {
            arr->data[i] = min + (double) pcg32_bounded_u32(&reg->rng, (uint32_t) (max - min));
        } else {
            arr->data[i] = min + pcg32_random(&reg->rng) * (max - min);
        }
    }

    p->registry = reg;
done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

static int32_t create_random_csnarray_k_helper(CSOUND *csound, CSN_ARR_RND_INIT *p, bool is_int) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    CHECK_KTRIG(p->trig);

    int32_t res = OK;

    uint32_t new_ndim = 0;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    res = parse_shape_array_k(csound, &p->h, p->shape, &new_ndim, new_shape);
    if (res != OK) return res;

    double min = (double) *p->min;
    double max = (double) *p->max;
    if (min == max || min > max || !IS_VALID_VALUE(min) || !IS_VALID_VALUE(max)) {
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid random range");
    }

    if (is_int && (!IS_VALID_VALUE_INT32(min) || !IS_VALID_VALUE_INT32(max))) {
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid random range");
    }

    CSNARR_RES pres = prepare_output(csound, &p->h, &p->h, reg, p->handle, new_ndim, new_shape, CSN_REAL, NULL, 0, CSN_ALIAS_REFUSE);
    res = pres.err;
    if (res != OK) goto done;

    /* the generator is shared: draw under the lock even for a local output */
    if (!pres.locked) {
        csn_lock(csound, reg);
        pres.locked = true;
    }
    CSN_ARRAY *arr = p->handle;
    for (size_t i = 0; i < arr->size; i++) {
        if (is_int) {
            arr->data[i] = min + (double) pcg32_bounded_u32(&reg->rng, (uint32_t) (max - min));
        } else {
            arr->data[i] = min + pcg32_random(&reg->rng) * (max - min);
        }
    }

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t create_random_csnarray(CSOUND *csound, CSN_ARR_RND_INIT *p) {
    return create_random_csnarray_helper(csound, p, false);
}

int32_t create_randomint_csnarray(CSOUND *csound, CSN_ARR_RND_INIT *p) {
    return create_random_csnarray_helper(csound, p, true);
}

int32_t create_random_csnarray_k_init(CSOUND *csound, CSN_ARR_RND_INIT *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t ndim = 1U;
    uint32_t shape[CSN_MAX_DIMS] = {0};
    shape[0] = 1U;

    CSNARR_RES pres = prepare_output(csound, &p->h, NULL, reg, p->handle, ndim, shape, CSN_REAL, NULL, 0, CSN_ALIAS_REFUSE);
    int32_t res = pres.err;
    if (res != OK) goto done;

    reset_empty_csnarray(p->handle, ndim, shape, CSN_REAL);
    p->registry = reg;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t create_random_csnarray_k(CSOUND *csound, CSN_ARR_RND_INIT *p) {
    return create_random_csnarray_k_helper(csound, p, false);
}
int32_t create_randomint_csnarray_k(CSOUND *csound, CSN_ARR_RND_INIT *p) {
    return create_random_csnarray_k_helper(csound, p, true);
}

int32_t create_full_csnarray(CSOUND *csound, CSN_FULL *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    CHECK_ITYPE(csound, *p->itype);
    ITEM_TYPE itype = CSN_ITYPE_FROM_ARG(*p->itype);
    CSNARR_RES pres = create_csnarray_from_shape(csound, &p->h, reg, p->shape, p->handle, itype);
    if (pres.err != OK) return pres.err;

    fill_csnarray(p->handle, (double) *p->value);
    p->registry = reg;

    if (pres.locked) csn_unlock(csound, reg);
    return OK;
}

int32_t create_full_csnarray_k_init(CSOUND *csound, CSN_FULL *p) {
    return create_full_csnarray(csound, p);
}

int32_t create_full_csnarray_k(CSOUND *csound, CSN_FULL *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    /* Validated by the init: itype is an i-argument. */
    ITEM_TYPE itype = CSN_ITYPE_FROM_ARG(*p->itype);

    uint32_t ndim = 0;
    uint32_t shape[CSN_MAX_DIMS] = {0};
    int32_t res = parse_shape_array_k(csound, &p->h, p->shape, &ndim, shape);
    if (res != OK) return res;

    size_t requested_size = 0;
    if (get_array_size_from_shape(&requested_size, ndim, shape) != OK) {
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSNARR_RES pres = prepare_output(csound, &p->h, &p->h, reg, p->handle, ndim, shape, itype, NULL, 0, CSN_ALIAS_REFUSE);
    res = pres.err;
    if (res != OK) goto done;

    fill_csnarray(p->handle, (double) *p->value);
    p->registry = reg;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t create_fullcomp_csnarray_k_init(CSOUND *csound, CSN_FULLCOMPLEX *p) {
    return create_fullcomp_csnarray(csound, p);
}

int32_t create_fullcomp_csnarray_k(CSOUND *csound, CSN_FULLCOMPLEX *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    uint32_t ndim = 0;
    uint32_t shape[CSN_MAX_DIMS] = {0};
    int32_t res = parse_shape_array_k(csound, &p->h, p->shape, &ndim, shape);
    if (res != OK) return res;

    size_t requested_size = 0;
    if (get_array_size_from_shape(&requested_size, ndim, shape) != OK) {
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSNARR_RES pres = prepare_output(csound, &p->h, &p->h, reg, p->handle, ndim, shape, CSN_COMPLEX, NULL, 0, CSN_ALIAS_REFUSE);
    res = pres.err;
    if (res != OK) goto done;

    double re, im;
    complexdat_to_rect(p->value, &re, &im);
    fill_csnarray_complex(p->handle, re, im);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t create_fullcomp_csnarray(CSOUND *csound, CSN_FULLCOMPLEX *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    CSNARR_RES pres = create_csnarray_from_shape(csound, &p->h, reg, p->shape, p->handle, CSN_COMPLEX);
    if (pres.err != OK) return pres.err;

    double re, im;
    complexdat_to_rect(p->value, &re, &im);
    fill_csnarray_complex(p->handle, re, im);
    p->registry = reg;

    if (pres.locked) csn_unlock(csound, reg);
    return OK;
}

int32_t from_array_to_csnarray(CSOUND *csound, CSN_FROM_ARRAY *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    if (p->source == NULL
        || p->source->data == NULL
        || p->source->sizes == NULL
        || p->source->dimensions <= 0
        || p->source->dimensions > CSN_MAX_DIMS) {
        return csound->InitError(csound, "[csnarray] Source must be an i-array with 1 to %d dimensions and allocated data", CSN_MAX_DIMS);
    }

    uint32_t ndim = (uint32_t) p->source->dimensions;
    uint32_t shape[CSN_MAX_DIMS] = {0};

    for (uint32_t i = 0; i < ndim; i++) {
        int32_t size = p->source->sizes[i];
        if (size <= 0) {
            return csound->InitError(csound, "[csnarray] Source extent %u must be >= 1", i);
        }
        shape[i] = (uint32_t) size;
    }

    size_t total_size = 0;
    if (get_array_size_from_shape(&total_size, ndim, shape) != OK) {
        return csound->InitError(csound, "[csnarray] Source shape is invalid or its element count exceeds the configured limit");
    }

    CSNARR_RES pres = prepare_output(csound, &p->h, NULL, reg, p->handle, ndim, shape, CSN_REAL, NULL, 0, CSN_ALIAS_REFUSE);
    int32_t res = pres.err;
    if (res != OK) goto done;

    for (size_t i = 0; i < total_size; i++) {
        p->handle->data[i] = (double) p->source->data[i];
    }

    p->registry = reg;
done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t from_array_to_csnarray_k_init(CSOUND *csound, CSN_FROM_ARRAY *p) {
    return from_array_to_csnarray(csound, p);
}

int32_t from_array_to_csnarray_k(CSOUND *csound, CSN_FROM_ARRAY *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    if (p->source == NULL
        || p->source->data == NULL
        || p->source->sizes == NULL
        || p->source->dimensions <= 0
        || p->source->dimensions > CSN_MAX_DIMS) {
        return csound->PerfError(csound, &p->h, "[csnarray] Source must be a k-array with 1 to %d dimensions and allocated data", CSN_MAX_DIMS);
    }

    uint32_t ndim = (uint32_t) p->source->dimensions;
    uint32_t shape[CSN_MAX_DIMS] = {0};

    for (uint32_t i = 0; i < ndim; i++) {
        int32_t size = p->source->sizes[i];
        if (size <= 0) {
            return csound->PerfError(csound, &p->h, "[csnarray] Source extent %u must be >= 1", i);
        }
        shape[i] = (uint32_t) size;
    }

    size_t requested_size = 0;
    if (get_array_size_from_shape(&requested_size, ndim, shape) != OK) {
        return csound->PerfError(csound, &p->h, "[csnarray] Source shape is invalid or its element count exceeds the configured limit");
    }

    int32_t res = OK;

    CSNARR_RES pres = prepare_output(csound, &p->h, &p->h, reg, p->handle, ndim, shape, CSN_REAL, NULL, 0, CSN_ALIAS_REFUSE);
    res = pres.err;
    if (res != OK) goto done;

    for (size_t i = 0; i < requested_size; i++) {
        p->handle->data[i] = (double) p->source->data[i];
    }

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

/* ARRAYDAT complex is a vector of COMPLEXDAT */
int32_t from_complexarray_to_csnarray(CSOUND *csound, CSN_FROM_ARRAY *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    if (p->source == NULL
        || p->source->data == NULL
        || p->source->sizes == NULL
        || p->source->dimensions <= 0
        || p->source->dimensions > CSN_MAX_DIMS) {
        return csound->InitError(csound, "[csnarray] Source must be a complex array with 1 to %d dimensions and allocated data", CSN_MAX_DIMS);
    }

    uint32_t ndim = (uint32_t) p->source->dimensions;
    uint32_t shape[CSN_MAX_DIMS] = {0};

    for (uint32_t i = 0; i < ndim; i++) {
        int32_t size = p->source->sizes[i];
        if (size <= 0) {
            return csound->InitError(csound, "[csnarray] Source extent %u must be >= 1", i);
        }
        shape[i] = (uint32_t) size;
    }

    size_t total_size = 0;
    if (get_array_size_from_shape(&total_size, ndim, shape) != OK) {
        return csound->InitError(csound, "[csnarray] Source shape is invalid or its element count exceeds the configured limit");
    }

    int32_t res = OK;
    CSNARR_RES pres = prepare_output(csound, &p->h, NULL, reg, p->handle, ndim, shape, CSN_COMPLEX, NULL, 0, CSN_ALIAS_REFUSE);
    res = pres.err;
    if (res != OK) goto done;

    const COMPLEXDAT *src = (const COMPLEXDAT *) p->source->data;
    for (size_t i = 0; i < total_size; i++) {
        double re, im;
        complexdat_to_rect(&src[i], &re, &im);
        p->handle->data[i * 2] = re;
        p->handle->data[i * 2 + 1] = im;
    }

    p->registry = reg;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t from_complexarray_to_csnarray_k_init(CSOUND *csound, CSN_FROM_ARRAY *p) {
    return from_complexarray_to_csnarray(csound, p);
}

int32_t from_complexarray_to_csnarray_k(CSOUND *csound, CSN_FROM_ARRAY *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    if (p->source == NULL
        || p->source->data == NULL
        || p->source->sizes == NULL
        || p->source->dimensions <= 0
        || p->source->dimensions > CSN_MAX_DIMS) {
        return csound->PerfError(csound, &p->h, "[csnarray] Source must be a complex array with 1 to %d dimensions and allocated data", CSN_MAX_DIMS);
    }

    uint32_t ndim = (uint32_t) p->source->dimensions;
    uint32_t shape[CSN_MAX_DIMS] = {0};

    for (uint32_t i = 0; i < ndim; i++) {
        int32_t size = p->source->sizes[i];
        if (size <= 0) {
            return csound->PerfError(csound, &p->h, "[csnarray] Source extent %u must be >= 1", i);
        }
        shape[i] = (uint32_t) size;
    }

    size_t requested_size = 0;
    if (get_array_size_from_shape(&requested_size, ndim, shape) != OK) {
        return csound->PerfError(csound, &p->h, "[csnarray] Source shape is invalid or its element count exceeds the configured limit");
    }

    int32_t res = OK;

    CSNARR_RES pres = prepare_output(csound, &p->h, &p->h, reg, p->handle, ndim, shape, CSN_COMPLEX, NULL, 0, CSN_ALIAS_REFUSE);
    res = pres.err;
    if (res != OK) goto done;

    const COMPLEXDAT *src = (const COMPLEXDAT *) p->source->data;
    for (size_t i = 0; i < requested_size; i++) {
        double re, im;
        complexdat_to_rect(&src[i], &re, &im);
        p->handle->data[i * 2] = re;
        p->handle->data[i * 2 + 1] = im;
    }

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

/* A k-rate Csound array output is filled on the audio thread too. Csound owns
   it, so init reserves room for everything the source can hold without
   reallocating, and a marked source that needs more is refused instead of
   letting tabinit allocate. Csound's own copy-on-write detach of a shared
   output array stays outside this guarantee. */
static bool krate_array_output_fits(const ARRAYDAT *out, size_t items) {
    return out->data != NULL && out->arrayMemberSize > 0
        && out->allocated / (size_t) out->arrayMemberSize >= items;
}

static int32_t reserve_krate_array_output(CSOUND *csound, OPDS *h, ARRAYDAT *out, size_t items) {
    if (h->perf == NULL || items == 0) return OK;
    if (items > (size_t) INT32_MAX) return NOTOK;
    int32_t logical_size = out->dimensions == 1 && out->sizes != NULL ? out->sizes[0] : 0;
    tabinit(csound, out, (int32_t) items, h->insdshead);
    if (out->dimensions == 1 && out->sizes != NULL) out->sizes[0] = logical_size;
    return krate_array_output_fits(out, items) ? OK : NOTOK;
}

int32_t from_csnarray_to_array(CSOUND *csound, CSN_TO_ARRAY *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    stamp_globals(csound, &p->h);
    p->registry = reg;

    CSN_ARRAY *src = p->source_handle;
    int32_t res = OK;
    bool locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { src }, 1);
    uint32_t ndim = src->ndim;

    /* The k form converts on every pass, and a k result its producer could
       not compute at init is still empty here: only the room the perf pass
       needs is reserved now. */
    if (src->size == 0 && csn_is_k_form(&p->h)) {
        if (reserve_krate_array_output(csound, &p->h, p->array, src->capacity) != OK) {
            res = csound->InitError(csound, "[csnarray] Could not reserve the output array");
        }
        goto done;
    }

    if (src->size == 0) {
        res = csound->InitError(csound, "[csnarray] Cannot convert an empty CsnArr to ARRAYDAT");
        goto done;
    }

    if (src->itype != CSN_REAL) {
        res = csound->InitError(csound, "[csnarray] Handle holds a complex array; declare the output as :Complex;[]");
        goto done;
    }

    int32_t declared = p->array->dimensions > 0 ? p->array->dimensions : 1;
    if ((uint32_t) declared != ndim) {
        res = csound->InitError(csound, "[csnarray] Handle holds a %u-D array but the output is declared %d-D; declare it with %u bracket pairs", ndim, declared, ndim);
        goto done;
    }

    size_t total_size = src->size;
    if (total_size > (size_t) INT32_MAX) {
        res = csound->InitError(csound, "[csnarray] Array holds %zu elements, too many for an i-array output (limit %d)", total_size, INT32_MAX);
        goto done;
    }

    res = tabinit(csound, p->array, (int32_t) total_size, p->h.insdshead);
    if (res!= OK || p->array->data == NULL || p->array->sizes == NULL || reserve_krate_array_output(csound, &p->h, p->array, src->capacity) != OK) {
        res = csound->InitError(csound, "[csnarray] Could not allocate the %u-D output i-array of %zu elements", ndim, total_size);
        goto done;
    }

    p->array->dimensions = (int32_t) ndim;
    for (uint32_t i = 0; i < ndim; i++) {
        p->array->sizes[i] = (int32_t) src->shape[i];
    }

    for (size_t i = 0; i < total_size; i++) {
        p->array->data[i] = (cs_float) src->data[i];
    }

done:
    if (locked) csn_unlock(csound, reg);
    return res;
}

int32_t from_csnarray_to_array_k(CSOUND *csound, CSN_TO_ARRAY *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    CSN_ARRAY *src = p->source_handle;
    int32_t res = OK;
    bool locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { src }, 1);
    uint32_t ndim = src->ndim;

    if (src->itype != CSN_REAL) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Handle holds a complex array; declare the output as :Complex;[]");
        goto done;
    }

    if (src->size == 0) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Cannot convert an empty CsnArr to ARRAYDAT");
        goto done;
    }

    int32_t declared = p->array->dimensions > 0 ? p->array->dimensions : 1;
    if ((uint32_t) declared != ndim) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Handle holds a %u-D array but the output is declared %d-D; declare it with %u bracket pairs", ndim, declared, ndim);
        goto done;
    }

    size_t total_size = src->size;
    if (total_size > (size_t) INT32_MAX) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Array holds %zu elements, too many for an k-array output (limit %d)", total_size, INT32_MAX);
        goto done;
    }

    if (src->rt_locked && !krate_array_output_fits(p->array, total_size)) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Array is on a real-time path and has grown to %zu elements, past what '%s' reserved at init; clear the mark with csnrtunlock, or pass irt=0 at the audio source it descends from", total_size, get_out_name(&p->h));
        goto done;
    }

    // use int32_t as return value on tabinit in Csound recent version
    res = tabinit(csound, p->array, (int32_t) total_size, p->h.insdshead);
    if (p->array->data == NULL || p->array->sizes == NULL || res != OK) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Could not allocate the %u-D output k-array of %zu elements", ndim, total_size);
        goto done;
    }

    p->array->dimensions = (int32_t) ndim;
    for (uint32_t i = 0; i < ndim; i++) {
        p->array->sizes[i] = (int32_t) src->shape[i];
    }

    for (size_t i = 0; i < total_size; i++) {
        p->array->data[i] = (cs_float) src->data[i];
    }

done:
    if (locked) csn_unlock(csound, reg);
    return res;
}

int32_t from_csnarray_to_complexarray(CSOUND *csound, CSN_TO_ARRAY *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    stamp_globals(csound, &p->h);
    p->registry = reg;

    CSN_ARRAY *src = p->source_handle;
    int32_t res = OK;
    bool locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { src }, 1);
    uint32_t ndim = src->ndim;

    /* The k form converts on every pass, and a k result its producer could
       not compute at init is still empty here: only the room the perf pass
       needs is reserved now. */
    if (src->size == 0 && csn_is_k_form(&p->h)) {
        if (reserve_krate_array_output(csound, &p->h, p->array, src->capacity) != OK) {
            res = csound->InitError(csound, "[csnarray] Could not reserve the output array");
        }
        goto done;
    }

    if (src->size == 0) {
        res = csound->InitError(csound, "[csnarray] Cannot convert an empty CsnArr to ARRAYDAT");
        goto done;
    }

    if (src->itype != CSN_COMPLEX) {
        res = csound->InitError(csound, "[csnarray] Handle holds a real array; use csntoarray for it");
        goto done;
    }

    int32_t declared = p->array->dimensions > 0 ? p->array->dimensions : 1;
    if ((uint32_t) declared != ndim) {
        res = csound->InitError(csound, "[csnarray] Handle holds a %u-D array but the output is declared %d-D; declare it with %u bracket pairs", ndim, declared, ndim);
        goto done;
    }

    size_t total_size = src->size;
    if (total_size > (size_t) INT32_MAX) {
        res = csound->InitError(csound, "[csnarray] Array holds %zu elements, too many for a complex-array output (limit %d)", total_size, INT32_MAX);
        goto done;
    }

    res = tabinit(csound, p->array, (int32_t) total_size, p->h.insdshead);
    if (res != OK || p->array->data == NULL || p->array->sizes == NULL || reserve_krate_array_output(csound, &p->h, p->array, src->capacity) != OK) {
        res = csound->InitError(csound, "[csnarray] Could not allocate the %u-D output complex array of %zu elements", ndim, total_size);
        goto done;
    }

    p->array->dimensions = (int32_t) ndim;
    for (uint32_t i = 0; i < ndim; i++) {
        p->array->sizes[i] = (int32_t) src->shape[i];
    }

    COMPLEXDAT *dst = (COMPLEXDAT *) p->array->data;
    for (size_t i = 0; i < total_size; i++) {
        dst[i].real = (cs_float) src->data[i * 2];
        dst[i].imag = (cs_float) src->data[i * 2 + 1];
        dst[i].isPolar = 0;
    }

done:
    if (locked) csn_unlock(csound, reg);
    return res;
}

int32_t from_csnarray_to_complexarray_k(CSOUND *csound, CSN_TO_ARRAY *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    CSN_ARRAY *src = p->source_handle;
    int32_t res = OK;
    bool locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { src }, 1);
    uint32_t ndim = src->ndim;

    if (src->size == 0) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Cannot convert an empty CsnArr to ARRAYDAT");
        goto done;
    }

    if (src->itype != CSN_COMPLEX) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Handle holds a real array; use csntoarray for it");
        goto done;
    }

    int32_t declared = p->array->dimensions > 0 ? p->array->dimensions : 1;
    if ((uint32_t) declared != ndim) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Handle holds a %u-D array but the output is declared %d-D; declare it with %u bracket pairs", ndim, declared, ndim);
        goto done;
    }

    size_t total_size = src->size;
    if (total_size > (size_t) INT32_MAX) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Array holds %zu elements, too many for a complex-array output (limit %d)", total_size, INT32_MAX);
        goto done;
    }

    if (src->rt_locked && !krate_array_output_fits(p->array, total_size)) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Array is on a real-time path and has grown to %zu elements, past what '%s' reserved at init; clear the mark with csnrtunlock, or pass irt=0 at the audio source it descends from", total_size, get_out_name(&p->h));
        goto done;
    }

    res = tabinit(csound, p->array, (int32_t) total_size, p->h.insdshead);
    if (res != OK || p->array->data == NULL || p->array->sizes == NULL) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Could not allocate the %u-D output k-array of %zu elements", ndim, total_size);
        goto done;
    }

    p->array->dimensions = (int32_t) ndim;
    for (uint32_t i = 0; i < ndim; i++) {
        p->array->sizes[i] = (int32_t) src->shape[i];
    }

    COMPLEXDAT *dst = (COMPLEXDAT *) p->array->data;
    for (size_t i = 0; i < total_size; i++) {
        dst[i].real = (cs_float) src->data[i * 2];
        dst[i].imag = (cs_float) src->data[i * 2 + 1];
        dst[i].isPolar = 0;
    }

done:
    if (locked) csn_unlock(csound, reg);
    return res;
}

// get dims
int32_t csnarray_dims(CSOUND *csound, CSN_SIZE_DIMS *p) {
    /* a global source may be resized by another thread: read it under the
       lock, looked up only then, so a local read costs nothing */
    CSN_REGISTRY *reg = p->source_handle->shared ? get_registry(csound) : NULL;
    if (reg != NULL) csn_lock(csound, reg);
    cs_float dims = (cs_float) p->source_handle->ndim;
    *p->value = dims;
    if (reg != NULL) csn_unlock(csound, reg);
    return OK;
}

// get total size (considering all dims)
int32_t csnarray_size(CSOUND *csound, CSN_SIZE_DIMS *p) {
    /* a global source may be resized by another thread: read it under the
       lock, looked up only then, so a local read costs nothing */
    CSN_REGISTRY *reg = p->source_handle->shared ? get_registry(csound) : NULL;
    if (reg != NULL) csn_lock(csound, reg);
    cs_float total_size = (cs_float) p->source_handle->size;
    *p->value = total_size;
    if (reg != NULL) csn_unlock(csound, reg);
    return OK;
}

/* Emptiness is derived from the size rather than tracked, so it cannot
   disagree with what the array actually holds. */
int32_t csnarray_is_empty(CSOUND *csound, CSN_SIZE_DIMS *p) {
    /* a global source may be resized by another thread: read it under the
       lock, looked up only then, so a local read costs nothing */
    CSN_REGISTRY *reg = p->source_handle->shared ? get_registry(csound) : NULL;
    if (reg != NULL) csn_lock(csound, reg);
    cs_float empty = p->source_handle->size == 0 ? FL(1.0) : FL(0.0);
    *p->value = empty;
    if (reg != NULL) csn_unlock(csound, reg);
    return OK;
}

// get shape -> shape[size dim 1, size dim 2, size dim 3, ...]
int32_t csnarray_shape(CSOUND *csound, CSN_SHAPE *p) {
    /* copied under the lock when the source is global, so the output is
       written from a consistent snapshot without holding it across tabinit */
    CSN_REGISTRY *reg = p->source_handle->shared ? get_registry(csound) : NULL;
    if (reg != NULL) csn_lock(csound, reg);
    uint32_t dims = p->source_handle->ndim;
    uint32_t shape[CSN_MAX_DIMS] = {0};
    memcpy(shape, p->source_handle->shape, sizeof(shape));
    if (reg != NULL) csn_unlock(csound, reg);

    /* The k form reserves every dimension an array can have, so a source that
       changes rank at perf time never makes tabinit allocate. */
    int32_t res = tabinit(csound, p->shape, (int32_t) dims, p->h.insdshead);
    if (reserve_krate_array_output(csound, &p->h, p->shape, CSN_MAX_DIMS) != OK || res != OK) {
        return csound->InitError(csound, "[csnarray] Could not allocate the shape output of %u elements", (uint32_t) CSN_MAX_DIMS);
    }

    for (uint32_t i = 0; i < dims; i++) {
        p->shape->data[i] = (cs_float) shape[i];
    }

    return OK;
}

static int32_t spaced_space_body(CSOUND *csound, OPDS *perf_h, double step_num, const cs_float *in_start, const cs_float *in_stop, const cs_float *in_base, double *base, double *ratio, uint32_t *usize, CSN_SPACED_SPACE_MODE mode) {
    double start = (double) *in_start;
    double stop = (double) *in_stop;
    switch (mode) {
        case CSN_ARANGE:
            if ((stop > start && step_num < 0) || (stop < start && step_num > 0)) {
                return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Step %g has the wrong sign to go from start %g to stop %g", step_num, start, stop);
            }
            int32_t size = (int32_t) ceil((stop - start) / step_num);
            if (size == 0) {
                return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Start %g, stop %g and step %g produce an empty array", start, stop, step_num);
            }
            *usize = (uint32_t) size;
            break;
        case CSN_LINSPACE:
        case CSN_LOGSPACE:
        case CSN_GEOMSPACE:
            if (mode == CSN_LOGSPACE) {
                if (in_base == NULL) {
                    return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] The base argument is missing");
                }
                *base = (double) *in_base;
                if (*base <= 0) {
                    return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] The base argument must be > 0, got %g", *base);
                }
            }
            if (mode == CSN_GEOMSPACE) {
                *ratio = pow(stop / start, 1.0 / (step_num - 1));
            }
            *usize = (uint32_t) step_num;
            break;
    }

    return OK;
}

/* arange_step stays a double all the way down: a fractional step would floor to
   zero and a negative one would wrap if this took an unsigned integer, while
   the element count above is already computed in double. */
static void spaced_space_assign_value(CSN_ARRAY *array, uint32_t usize, double arange_step, double start, double stop, double base, double ratio, CSN_SPACED_SPACE_MODE mode) {
    switch (mode) {
        case CSN_ARANGE:
            for (uint32_t i = 0; i < usize; i++) {
                array->data[i] =  start + ((double) i * arange_step);
            }
            break;
        case CSN_LINSPACE:
            if (usize == 1) {
                array->data[0] = start;
            } else {
                double step = (stop - start) / (double) (usize - 1);
                for (uint32_t i = 0; i < usize; i++) {
                    array->data[i] =  (double) start + (i * step);
                }
                array->data[usize - 1] = stop;
            }
            break;
        case CSN_LOGSPACE:
            if (usize == 1) {
                array->data[0] = pow(base, start);
            } else {
                double step = (stop - start) / (double) (usize - 1);
                for (uint32_t i = 0; i < usize; i++) {
                    double exponent = start + (double) i * step;
                    array->data[i] =  pow(base, exponent);
                }
                array->data[usize - 1] = pow(base, stop);
            }
            break;
        case CSN_GEOMSPACE:
            if (usize == 1) {
                array->data[0] = start;
            } else {
                for (uint32_t i = 0; i < usize; i++) {
                    array->data[i] =  start * pow(ratio, i);
                }
                array->data[usize - 1] = stop;
            }
    }
}

static int32_t spaced_space_helper(CSOUND *csound, CSN_SPACED_SPACE *p, CSN_SPACED_SPACE_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    double num = (double) *p->step_num;
    if (mode == CSN_ARANGE) {
        if (num == 0.0) return csound->InitError(csound, "[csnarray] The step argument must not be zero");
    } else {
        if (num <= 0.0) return csound->InitError(csound, "[csnarray] The num argument must be > 0, got %d", (int32_t) num);
    }

    double arange_step = num;
    double base = 1.0;
    const cs_float *in_base = mode == CSN_LOGSPACE ? p->arg_a : NULL;
    double ratio = 0.0;
    uint32_t usize = 0;
    /* Init pass, and the only caller that reaches spaced_space_body without
       the registry lock: the report has to go through InitError, both because
       that is the phase we are in and because the locked report would release
       a mutex this path never took. */
    int32_t res = spaced_space_body(csound, NULL, arange_step, p->start, p->stop, in_base, &base, &ratio, &usize, mode);
    if (res != OK) return res;

    uint32_t shape[CSN_MAX_DIMS] = {0};
    shape[0] = usize;

    CSNARR_RES pres = prepare_output(csound, &p->h, NULL, reg, p->handle, 1U, shape, CSN_REAL, NULL, 0, CSN_ALIAS_REFUSE);
    res = pres.err;
    if (res != OK) goto done;

    double start = (double) *p->start;
    double stop = (double) *p->stop;
    spaced_space_assign_value(p->handle, usize, arange_step, start, stop, base, ratio, mode);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

static int32_t spaced_space_k_init_helper(CSOUND *csound, CSN_SPACED_SPACE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    uint32_t shape[CSN_MAX_DIMS] = {0};
    shape[0] = 1U;
    CSNARR_RES pres = prepare_output(csound, &p->h, NULL, reg, p->handle, 1U, shape, CSN_REAL, NULL, 0, CSN_ALIAS_REFUSE);
    res = pres.err;
    if (res != OK) goto done;

    reset_empty_csnarray(p->handle, 1U, shape, CSN_REAL);
    p->registry = reg;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

static int32_t spaced_space_k_helper(CSOUND *csound, CSN_SPACED_SPACE *p, CSN_SPACED_SPACE_MODE mode) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    const cs_float *trig = mode == CSN_LOGSPACE ? p->arg_b : p->arg_a;
    CHECK_KTRIG(trig);

    double num = (double) *p->step_num;
    if (mode == CSN_ARANGE) {
        if (num == 0.0) return csound->PerfError(csound, &p->h, "[csnarray] The step argument must not be zero");
    } else {
        if (num <= 0.0) return csound->PerfError(csound, &p->h, "[csnarray] The num argument must be > 0, got %d", (int32_t) num);
    }

    int32_t res = OK;
    /* initialised before the first goto: done reads it */
    CSNARR_RES pres = {0};

    double arange_step = num;
    double base = 1.0;
    const cs_float *in_base = mode == CSN_LOGSPACE ? p->arg_a : NULL;
    double ratio = 0.0;
    uint32_t usize = 0;
    res = spaced_space_body(csound, &p->h, arange_step, p->start, p->stop, in_base, &base, &ratio, &usize, mode);
    if (res != OK) goto done;

    uint32_t shape[CSN_MAX_DIMS] = {0};
    shape[0] = usize;

    pres = prepare_output(csound, &p->h, &p->h, reg, p->handle, 1U, shape, CSN_REAL, NULL, 0, CSN_ALIAS_REFUSE);
    res = pres.err;
    if (res != OK) goto done;

    double start = (double) *p->start;
    double stop = (double) *p->stop;
    spaced_space_assign_value(p->handle, usize, arange_step, start, stop, base, ratio, mode);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_spaced_space_k_init(CSOUND *csound, CSN_SPACED_SPACE *p) {
    return spaced_space_k_init_helper(csound, p);
}

int32_t csnarray_arange(CSOUND *csound, CSN_SPACED_SPACE *p) {
    return spaced_space_helper(csound, p, CSN_ARANGE);
}

int32_t csnarray_arange_k(CSOUND *csound, CSN_SPACED_SPACE *p) {
    return spaced_space_k_helper(csound, p, CSN_ARANGE);
}

int32_t csnarray_linspace(CSOUND *csound, CSN_SPACED_SPACE *p) {
    return spaced_space_helper(csound, p, CSN_LINSPACE);
}

int32_t csnarray_linspace_k(CSOUND *csound, CSN_SPACED_SPACE *p) {
    return spaced_space_k_helper(csound, p, CSN_LINSPACE);
}

int32_t csnarray_logspace(CSOUND *csound, CSN_SPACED_SPACE *p) {
    return spaced_space_helper(csound, p, CSN_LOGSPACE);
}

int32_t csnarray_logspace_k(CSOUND *csound, CSN_SPACED_SPACE *p) {
    return spaced_space_k_helper(csound, p, CSN_LOGSPACE);
}

int32_t csnarray_geomspace(CSOUND *csound, CSN_SPACED_SPACE *p) {
    return spaced_space_helper(csound, p, CSN_GEOMSPACE);
}

int32_t csnarray_geomspace_k(CSOUND *csound, CSN_SPACED_SPACE *p) {
    return spaced_space_k_helper(csound, p, CSN_GEOMSPACE);
}
