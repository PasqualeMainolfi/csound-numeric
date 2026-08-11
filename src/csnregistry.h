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

typedef enum {
    INVALID_HANDLE = 0,
    VALID_HANDLE
} CSN_HANDLE_STATE;

typedef enum {
    INACTIVE_SLOT = 0,
    ACTIVE_SLOT
} CSN_SLOT_STATE;

typedef struct {
    uint32_t array_id;
    double *data;
    size_t size;
    size_t capacity;
    uint32_t ndim;
    size_t strides[CSN_MAX_DIMS];
    uint32_t shape[CSN_MAX_DIMS];
    /* Emptiness is derived: size == 0. No flag to keep in sync. */
} CSN_ARRAY;

typedef struct {
    CSN_ARRAY *array;
    uint32_t gen_id;
    CSN_SLOT_STATE state;
} CSN_SLOT;

typedef struct {
    CSN_SLOT slots[CSN_MAX_SLTS];
    uint32_t capacity;
    uint32_t active_count;
    void *mutex;
} CSN_REGISTRY;


// REGISTRY INTERFACE

CSN_REGISTRY *get_registry(CSOUND *csound);
uint32_t find_free_slot(CSN_REGISTRY *registry);
CSN_SLOT *get_slot(CSN_REGISTRY *registry, uint32_t handle);
int32_t activate_slot(CSOUND *csound, CSN_REGISTRY *registry, CSN_SLOT *slot, uint32_t ndim, const uint32_t *shape, uint32_t array_id);
/* Frees the array the slot owns; the caller must not keep its own pointer. */
int32_t release_slot(CSOUND *csound, CSN_REGISTRY *registry, CSN_SLOT *slot);
void compute_strides(const uint32_t *shape, size_t *strides, const uint32_t ndim);

#endif
