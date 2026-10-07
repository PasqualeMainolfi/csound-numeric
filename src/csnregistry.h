#ifndef __CSN_REG_H
#define __CSN_REG_H

#include <csdl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CSN_MAX_DIMS 8
#define CSN_REGISTRY_NAME "::csound_numeric::registry"
#define CSN_GEN_BITS 18U
#define CSN_SLT_BITS 14U
#define CSN_GEN_MASK ((1u << CSN_GEN_BITS) - 1u)
#define CSN_SLT_MASK ((1u << CSN_SLT_BITS) - 1u)

/* Upper bound on element count, so shape products can be overflow-checked
   before they reach Calloc. 2^28 doubles is 2 GB at capacity 2x. */
#define CSN_MAX_ELEMS ((size_t) 1 << 28)

#define CSN_RND_DEFAULT_STATE 42
#define CSN_ITYPE_FROM_ARG(x) ((ITEM_TYPE) ((x) != 0.0 ? CSN_COMPLEX : CSN_REAL))
/* Reserved for a consumer that has never computed: a cache living in a
   Csound-zeroed opcode struct reads NULL, which no live array ever carries. */
#define CSN_ARRAY_NULL_VERSION 0
#define CSN_ARRAY_FIRST_VERSION 1

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

typedef uint32_t ITEM_TYPE;
enum {
    CSN_REAL = 1,
    CSN_COMPLEX
};

typedef enum {
    CSNARR = 0,
    CSNSET,
} CSN_ARRAY_KIND;

typedef struct {
    uint64_t array_uid;
    uint64_t data_version;
    uint64_t shape_version;
    uint64_t ndim_version;
    uint64_t itype_version;
} ARRAY_VERSION;

typedef struct {
    double *data;
    size_t size;
    size_t capacity;
    uint32_t ndim;
    size_t strides[CSN_MAX_DIMS];
    uint32_t shape[CSN_MAX_DIMS];
    ITEM_TYPE itype; // needed for complex
    ARRAY_VERSION version;
    CSN_ARRAY_KIND kind;
    uint64_t set_data_version;
    bool external_lock;
    bool rt_locked;
    bool shared;
    uint8_t rate; // CSN_RATE: who produces or writes it
#ifdef CSN_VERSION_CROSSCHECK
    double *shadow;
    size_t shadow_capacity;
    size_t shadow_size;
    uint64_t shadow_data_version;
#endif
} CSN_ARRAY;

typedef const CSN_ARRAY *CSN_CONARR[];

/* The rate an array lives at: i-rate by default, which is what a zeroed
   variable reads as, and k-rate once a k-rate opcode produces it or writes it
   at perf. CsnArr itself has no rate, so the operators (+, -, ...) run their
   perf pass only while one of their operands is k-rate. */
typedef enum {
    CSN_RATE_I = 0,
    CSN_RATE_K = 1
} CSN_RATE;

typedef struct {
    uint64_t state;
    uint64_t inc;
} PCG32_STATE;

typedef struct {
    void *mutex;
    PCG32_STATE rng; // random generator
    bool rt_glock_global;
    bool rt_glock_locked;
    INSDS *rt_glock_owner;
#ifdef CSN_VERSION_CROSSCHECK
    /* get_slot is the one place every opcode passes through to reach an array,
       which makes it the natural place to audit the counters — but its
       signature carries no CSOUND to report through, so the cross-check build
       keeps one here. */
    CSOUND *csound;
#endif
} CSN_REGISTRY;


// REGISTRY INTERFACE

CSN_REGISTRY *get_registry(CSOUND *csound);
void compute_strides(const uint32_t *shape, size_t *strides, const uint32_t ndim);
int32_t allocate_array(CSOUND *csound, CSN_ARRAY *array, uint32_t ndim, const uint32_t *shape, ITEM_TYPE itype);
void travase_csnarray(CSN_ARRAY *dest, const CSN_ARRAY *src);
uint32_t pcg32_bounded_u32(PCG32_STATE *rng, uint32_t bound);
double pcg32_random(PCG32_STATE *rng);
void pcg32_random_init(PCG32_STATE *rng, uint64_t seed);

int32_t csn_register_type(CSOUND *csound);
int32_t get_array_size_from_shape(size_t *size, uint32_t ndim, const uint32_t *shape);

bool is_same_array_version(const ARRAY_VERSION *version_a, const ARRAY_VERSION *version_b);
bool is_same_array_data_version(const ARRAY_VERSION *version_a, const ARRAY_VERSION *version_b);
void set_array_version(ARRAY_VERSION *version_a, const ARRAY_VERSION *version_b);
void update_array_version(ARRAY_VERSION *version);
void update_array_data_version(ARRAY_VERSION *version);
void update_array_layout_version(ARRAY_VERSION *version, bool shape_changed, bool ndim_changed, bool itype_changed);
void init_array_version(ARRAY_VERSION *version);
int32_t ensure_mutation_capacity(CSOUND *csound, OPDS *perf_h, CSN_ARRAY *arr, size_t required_size, bool rt_locked);
void stamp_globals(CSOUND *csound, OPDS *h);
/* Take and release the registry mutex through these, never directly: they
   record per thread whether this thread holds it, which csn_locked_perf_error
   needs now that most opcodes run without the lock. */
void csn_lock(CSOUND *csound, CSN_REGISTRY *reg);
void csn_unlock(CSOUND *csound, CSN_REGISTRY *reg);
bool csn_lock_is_held(void);
/* Option A for opcodes that do not call prepare_output: takes the lock only
   when one of the arrays is global. Returns whether the caller must
   csn_unlock. NULL entries are skipped. */
bool csn_lock_if_shared(CSOUND *csound, CSN_REGISTRY *reg, const CSN_ARRAY *const *arrays, size_t n);

#ifdef CSN_VERSION_CROSSCHECK
void csn_release_shadow(CSOUND *csound, CSN_ARRAY *array);
#endif
#endif
