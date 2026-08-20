#ifndef __CSN_REG_H
#define __CSN_REG_H

#include <csdl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CSN_MAX_DIMS 8
#define CSN_MAX_SLTS 4096
#define CSN_REGISTRY_NAME "::csound_numeric::registry"
#define CSN_GEN_BITS 12U
#define CSN_SLT_BITS 12U
#define CSN_GEN_MASK ((1u << CSN_GEN_BITS) - 1u)
#define CSN_SLT_MASK ((1u << CSN_SLT_BITS) - 1u)

/* Upper bound on element count, so shape products can be overflow-checked
   before they reach Calloc. 2^28 doubles is 2 GB at capacity 2x. */
#define CSN_MAX_ELEMS ((size_t) 1 << 28)

#define BUILD_HANDLE(slot, genr) ((((genr) & CSN_GEN_MASK) << CSN_SLT_BITS) | ((slot) & CSN_SLT_MASK))
#define SLT_FROM_HANDLE(handle) ((handle) & CSN_SLT_MASK)
#define GEN_FROM_HANDLE(handle) ((handle) >> CSN_SLT_BITS & CSN_GEN_MASK)

#define CHECK_ITYPE(csound, x)                                                             \
    do {                                                                                   \
        if ((x) != 0.0 && (x) != 1.0) {                                                    \
            return (csound)->InitError(                                                    \
                (csound),                                                                  \
                "[csnarray] Invalid array type %g: itype must be 0 (real) or 1 (complex)", \
                (double) (x)                                                               \
            );                                                                             \
        }                                                                                  \
    } while (0)

#define CHECK_KTYPE(csound, h, x)                                                          \
    do {                                                                                   \
        if ((x) != 0.0 && (x) != 1.0) {                                                    \
            return (csound)->PerfError(                                                    \
                (csound),                                                                  \
                (h),                                                                       \
                "[csnarray] Invalid array type %g: itype must be 0 (real) or 1 (complex)", \
                (double) (x)                                                               \
            );                                                                             \
        }                                                                                  \
    } while (0)


#define CSN_ITYPE_FROM_ARG(x) ((ITEM_TYPE) ((x) != 0.0 ? CSN_COMPLEX : CSN_REAL))

typedef enum {
    INVALID_HANDLE = 0,
    VALID_HANDLE
} CSN_HANDLE_STATE;

typedef enum {
    INACTIVE_SLOT = 0,
    ACTIVE_SLOT
} CSN_SLOT_STATE;

typedef uint32_t ITEM_TYPE;
enum {
    CSN_REAL = 1,
    CSN_COMPLEX
};

typedef struct {
    uint32_t array_id;
    double *data;
    size_t size;
    size_t capacity;
    uint32_t ndim;
    size_t strides[CSN_MAX_DIMS];
    uint32_t shape[CSN_MAX_DIMS];
    /* Emptiness is derived: size == 0. No flag to keep in sync. */
    ITEM_TYPE itype; // needed for complex
} CSN_ARRAY;

typedef struct {
    CSN_ARRAY *array;
    uint32_t gen_id;
    CSN_SLOT_STATE state;
} CSN_SLOT;

typedef struct {
    uint64_t state;
    uint64_t inc;
} PCG32_STATE;

typedef struct {
    CSN_SLOT slots[CSN_MAX_SLTS];
    uint32_t capacity;
    uint32_t active_count;
    void *mutex;
    PCG32_STATE rng; // random generator
} CSN_REGISTRY;


// CSN TYPE SYSTEM

typedef struct {
    uint32_t id;
} CSNREF;


// REGISTRY INTERFACE

CSN_REGISTRY *get_registry(CSOUND *csound);
uint32_t find_free_slot(CSN_REGISTRY *registry);
CSN_SLOT *get_slot(CSN_REGISTRY *registry, uint32_t handle);
int32_t activate_slot(CSOUND *csound, CSN_REGISTRY *registry, CSN_SLOT *slot, uint32_t ndim, const uint32_t *shape, uint32_t array_id, ITEM_TYPE itype);
/* Frees the array the slot owns; the caller must not keep its own pointer. */
int32_t release_slot(CSOUND *csound, CSN_REGISTRY *registry, CSN_SLOT *slot);
void compute_strides(const uint32_t *shape, size_t *strides, const uint32_t ndim);
int32_t allocate_array(CSOUND *csound, CSN_ARRAY *array, uint32_t ndim, const uint32_t *shape, uint32_t array_id, ITEM_TYPE itype);
void travase_csnarray(CSN_ARRAY *dest, const CSN_ARRAY *src);
void pcg32_random_init(PCG32_STATE *rng);
double pcg32_random(PCG32_STATE *rng);

int32_t csn_register_type(CSOUND *csound);
int32_t get_array_size_from_shape(size_t *size, uint32_t ndim, const uint32_t *shape);
/* Caller must hold registry->mutex. */
int32_t update_slot_array_locked(
    CSOUND *csound,
    CSN_REGISTRY *registry,
    uint32_t handle,
    uint32_t ndim,
    const uint32_t *shape,
    ITEM_TYPE itype,
    CSN_ARRAY **out_array,
    const char **err);
int32_t update_slot_array(
    CSOUND *csound,
    CSN_REGISTRY *registry,
    uint32_t handle,
    uint32_t ndim,
    const uint32_t *shape,
    ITEM_TYPE itype,
    CSN_ARRAY **out_array,
    const char **err);

#endif
