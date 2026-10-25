#ifndef __CSN_REG_H
#define __CSN_REG_H

#include <csdl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Csound's InitError and PerfError print the message as given: since Csound 7
   they add no line ending of their own. These two format the message and end
   it, so every call site passes the bare text, and so does every future one. */
CS_PRINTF2 int32_t csn_init_error(CSOUND *csound, const char *fmt, ...);
CS_PRINTF3 int32_t csn_perf_error(CSOUND *csound, OPDS *h, const char *fmt, ...);

#define CSN_MAX_DIMS 8
#define CSN_REGISTRY_NAME "::csound_numeric::registry"

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
            return csn_init_error(                                                    \
                (csound),                                                                  \
                "[csnarray] Invalid array type %g: itype must be 0 (real) or 1 (complex)", \
                (double) (x)                                                               \
            );                                                                             \
        }                                                                                  \
    } while (0)

typedef uint32_t ITEM_TYPE;
enum {
    CSN_REAL = 1,
    CSN_COMPLEX,
    CSN_STRING
};

typedef enum {
    CSNARR = 0,
    CSNSET
} CSN_ARRAY_KIND;

typedef struct {
    uint64_t array_uid;
    uint64_t data_version;
    uint64_t shape_version;
    uint64_t ndim_version;
    uint64_t itype_version;
} ARRAY_VERSION;

/* One string slot. data is owned by the slot and NULL reads as "". capacity
   counts the bytes data holds, terminator included, so a shorter or equal
   string is written in place and only a longer one reallocates. The pair
   moves as a unit: a swap or a move hands the buffer over with its size. */
typedef struct {
    char *data;
    size_t capacity;
} CSN_STRDATA;

typedef struct {
    union {
        double *data;       // CSN_REAL, CSN_COMPLEX
        CSN_STRDATA *sdata; // CSN_STRING
    };
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
    bool pending; // during the init pass: its values wait for a perf pass (an audio block, or derived from one)
#ifdef CSN_VERSION_CROSSCHECK
    double *shadow;
    size_t shadow_capacity;
    size_t shadow_size;
    uint64_t shadow_data_version;
#endif
} CSN_ARRAY;

/* Copy the classification and preserve whether the source's set invariant is
   valid. The destination has its own data version, so stamp that version only
   after the payload has been copied and all version bumps are complete. */
static inline void csn_copy_array_kind(CSN_ARRAY *dest, const CSN_ARRAY *src) {
    bool valid_set = src->kind == CSNSET && src->set_data_version == src->version.data_version;
    dest->kind = src->kind;
    dest->set_data_version = valid_set ? dest->version.data_version : CSN_ARRAY_NULL_VERSION;
}

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
} CSN_REGISTRY;


/* Where a pass writes its strings. A perf pass on a real-time path (perf_h set,
   the array marked) writes only into the buffers its slots already hold: a
   string that does not fit stops the pass with a performance error instead of
   growing the slot. Anywhere else a slot grows as it needs. err keeps the
   first error a write raised, already reported, so a caller that also
   reports its own failures can tell the two apart. */
typedef struct {
    CSOUND *csound;
    OPDS *perf_h;
    bool no_growth;
    int32_t err;
} CSN_STR_WRITER;

static inline CSN_STR_WRITER csn_str_writer(CSOUND *csound, OPDS *perf_h, const CSN_ARRAY *arr) {
    return (CSN_STR_WRITER) { csound, perf_h, perf_h != NULL && arr->rt_locked, OK };
}

// REGISTRY INTERFACE

CSN_REGISTRY *get_registry(CSOUND *csound);
void compute_strides(const uint32_t *shape, size_t *strides, const uint32_t ndim);
int32_t allocate_array(CSOUND *csound, CSN_ARRAY *array, uint32_t ndim, const uint32_t *shape, ITEM_TYPE itype);
void travase_csnarray(CSN_ARRAY *dest, const CSN_ARRAY *src);
void travase_csnstrings(CSOUND *csound, CSN_ARRAY *dest, const CSN_ARRAY *src);
void move_csnstrings(CSOUND *csound, CSN_ARRAY *dest, CSN_ARRAY *src);
void free_csnstrings(CSOUND *csound, CSN_ARRAY *array);
void clear_csnstrings(CSOUND *csound, CSN_ARRAY *array);
int32_t csn_str_reserve(CSOUND *csound, CSN_STRDATA *slot, size_t bytes);
int32_t csn_str_set(CSOUND *csound, CSN_STRDATA *slot, const char *value);
void csn_str_release(CSOUND *csound, CSN_STRDATA *slot);
int32_t csn_str_reserve_slots(CSOUND *csound, CSN_ARRAY *array, size_t bytes);
size_t csn_str_longest(const CSN_ARRAY *array);
uint32_t pcg32_bounded_u32(PCG32_STATE *rng, uint32_t bound);
double pcg32_random(PCG32_STATE *rng);
void pcg32_random_init(PCG32_STATE *rng, uint64_t seed);

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
