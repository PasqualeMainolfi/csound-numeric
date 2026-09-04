#include "csnregistry.h"
#include <csdl.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

/* Two Csound instances inside one host build their registries on different
   threads and both bump the auto-seed counter without holding a lock, so it
   has to be atomic. A toolchain without C11 atomics falls back to a plain
   counter: the worst case there is two instances reading the same tick, and
   the timestamp and the registry address still separate them. */
#if defined(__STDC_NO_ATOMICS__)
typedef uint64_t CSN_AUTO_SEED_COUNTER;
#else
#include <stdatomic.h>
typedef _Atomic uint64_t CSN_AUTO_SEED_COUNTER;
#endif


static int32_t reset_registry(CSOUND *csound, void *userdata) {
    CSN_REGISTRY *reg = (CSN_REGISTRY *) userdata;
    if (reg == NULL) {
        return OK;
    }

    csound->LockMutex(reg->mutex);

    for (uint32_t i = 0; i < reg->capacity; i++) {
        CSN_SLOT *slot = &reg->slots[i];
        if (slot->state == ACTIVE_SLOT && slot->array != NULL) {
            release_slot(csound, reg, slot);
        }
    }
    reg->active_count = 0;

    csound->UnlockMutex(reg->mutex);
    csound->DestroyMutex(reg->mutex);
    reg->mutex = NULL;

    csound->DestroyGlobalVariable(csound, CSN_REGISTRY_NAME);
    return OK;
}


CSN_REGISTRY *get_registry(CSOUND *csound) {
    CSN_REGISTRY *reg = (CSN_REGISTRY *) csound->QueryGlobalVariable(csound, CSN_REGISTRY_NAME);
    if (reg != NULL) {
        return reg;
    }

    int32_t res = csound->CreateGlobalVariable(csound, CSN_REGISTRY_NAME, sizeof(CSN_REGISTRY));
    if (res != 0) {
        csound->ErrorMsg(csound, "[csn] Global registry allocation failed\n");
        return NULL;
    }

    reg = (CSN_REGISTRY *) csound->QueryGlobalVariable(csound, CSN_REGISTRY_NAME);
    if (reg == NULL) {
        csound->ErrorMsg(csound, "[csn] Allocated global registry not found\n");
        return NULL;
    }

    reg->capacity = CSN_MAX_SLTS;
    reg->active_count = 0;
#ifdef CSN_VERSION_CROSSCHECK
    reg->csound = csound;
#endif

    for (uint32_t i = 0; i < CSN_MAX_SLTS; i++) {
        reg->slots[i].array = NULL;
        reg->slots[i].gen_id = 1;
        reg->slots[i].state = INACTIVE_SLOT;
    }

    reg->mutex = csound->Create_Mutex(0);
    if (reg->mutex == NULL) {
        csound->DestroyGlobalVariable(csound, CSN_REGISTRY_NAME);
        return NULL;
    }

    pcg32_random_init(&reg->rng, CSN_RND_DEFAULT_STATE);

    if (csound->RegisterResetCallback(csound, reg, reset_registry) != OK) {
        csound->DestroyMutex(reg->mutex);
        csound->DestroyGlobalVariable(csound, CSN_REGISTRY_NAME);
        return NULL;
    }

    return reg;
}

uint32_t find_free_slot(CSN_REGISTRY *registry) {
    for (uint32_t i = 1; i < registry->capacity; i++) {
        CSN_SLOT *slot = &registry->slots[i];
        if (slot->state == INACTIVE_SLOT) {
            return BUILD_HANDLE(i, slot->gen_id);
        }
    }
    return 0; // full registry
}

#ifdef CSN_VERSION_CROSSCHECK
void csn_release_shadow(CSOUND *csound, CSN_ARRAY *array) {
    if (array == NULL || array->shadow == NULL) return;
    csound->Free(csound, array->shadow);
    array->shadow = NULL;
    array->shadow_capacity = 0;
    array->shadow_size = 0;
    array->shadow_data_version = CSN_ARRAY_NULL_VERSION;
}

void csn_verify_shadow(CSN_REGISTRY *registry, CSN_ARRAY *array, const char *where) {
    if (registry == NULL || registry->csound == NULL || array == NULL) return;
    CSOUND *csound = registry->csound;

    size_t doubles = array->data == NULL ? 0 : array->size * (size_t) array->itype;

    /* Only a generation the snapshot was actually taken at can be audited.
       CSN_ARRAY_NULL_VERSION means no snapshot yet, and a version that has
       moved since is exactly what the counters are supposed to report. */
    if (array->shadow_data_version != CSN_ARRAY_NULL_VERSION
        && array->shadow_data_version == array->version.data_version) {
        if (doubles != array->shadow_size) {
            csound->Message(csound,
                "[csnarray] VERSION CROSSCHECK (%s): array %u held data version %llu while its element count went from %zu to %zu — a writer did not advance it\n",
                where, array->array_id, (unsigned long long) array->version.data_version,
                array->shadow_size, doubles);
        } else if (doubles > 0 && memcmp(array->data, array->shadow, sizeof(double) * doubles) != 0) {
            csound->Message(csound,
                "[csnarray] VERSION CROSSCHECK (%s): array %u held data version %llu while its payload changed — a writer did not advance it\n",
                where, array->array_id, (unsigned long long) array->version.data_version);
        }
    }

    if (doubles > array->shadow_capacity) {
        double *grown = csound->ReAlloc(csound, array->shadow, sizeof(double) * doubles);
        if (grown == NULL) return; /* leave the old snapshot rather than lie about a new one */
        array->shadow = grown;
        array->shadow_capacity = doubles;
    }

    if (doubles > 0) {
        memcpy(array->shadow, array->data, sizeof(double) * doubles);
    }
    array->shadow_size = doubles;
    array->shadow_data_version = array->version.data_version;
}
#endif

CSN_SLOT *get_slot(CSN_REGISTRY *registry, uint32_t handle) {
    if (registry == NULL || handle == 0) {
        return NULL;
    }

    uint32_t hslt = SLT_FROM_HANDLE(handle);
    uint32_t hgen = GEN_FROM_HANDLE(handle);

    if (hslt >= registry->capacity) return NULL;

    CSN_SLOT *slot = &registry->slots[hslt];
    if (slot->state == INACTIVE_SLOT) return NULL;
    if (slot->gen_id != hgen) return NULL;
    if (slot->array == NULL) return NULL;

#ifdef CSN_VERSION_CROSSCHECK
    /* Every opcode resolves its handle here before touching an array, so an
       unbumped write is caught on whichever pass next looks the array up —
       whoever that turns out to be. */
    csn_verify_shadow(registry, slot->array, "get_slot");
#endif

    return slot;
}

void compute_strides(const uint32_t *shape, size_t *strides, const uint32_t ndim) {
    if (ndim == 0) {
        return;
    }

    strides[ndim - 1] = 1;
    for (uint32_t i = ndim - 1; i-- > 0; ) {
        strides[i] = strides[i + 1] * shape[i + 1];
    }
}

int32_t get_array_size_from_shape(size_t *size, uint32_t ndim, const uint32_t *shape) {
    if (size == NULL || shape == NULL || ndim == 0 || ndim > CSN_MAX_DIMS) {
        return NOTOK;
    }

    size_t array_length = 1;
    for (uint32_t i = 0; i < ndim; i++) {
        uint32_t s = shape[i];
        if (s != 0 && array_length > CSN_MAX_ELEMS / s) {
            return NOTOK;
        }
        array_length *= s;
    }

    *size = array_length;
    return OK;
}

/* Changes the array owned by an active slot without changing the slot,
   generation, handle or CSN_ARRAY object. The caller holds the registry lock,
   so a layout update can be followed by an opcode-specific fill atomically. */
int32_t update_slot_array_locked(
    CSOUND *csound,
    CSN_REGISTRY *registry,
    uint32_t handle,
    uint32_t ndim,
    const uint32_t *shape,
    ITEM_TYPE itype,
    CSN_ARRAY **out_array,
    const char **err
) {
    if (registry == NULL || out_array == NULL || err == NULL) {
        return NOTOK;
    }
    if (itype != CSN_REAL && itype != CSN_COMPLEX) {
        *err = "Invalid array type";
        return NOTOK;
    }

    size_t size = 0;
    if (get_array_size_from_shape(&size, ndim, shape) != OK) {
        *err = "Invalid shape or element count exceeds the configured limit";
        return NOTOK;
    }

    size_t capacity = size > 0 ? size * 2 : 1;
    if (capacity > SIZE_MAX / (sizeof(double) * (size_t) itype)) {
        *err = "Array allocation size overflow";
        return NOTOK;
    }

    size_t strides[CSN_MAX_DIMS] = {0};
    compute_strides(shape, strides, ndim);

    CSN_SLOT *slot = get_slot(registry, handle);
    if (slot == NULL) {
        *err = "Output slot is no longer active";
        return NOTOK;
    }

    // for safe but unreachble for now
    if (slot->rt_locked) {
        *err = "[csnarray] Array is on a real-time path and cannot be reallocated at perf time (clear the mark with csnrtlock, or pass irt=0 at the audio source it descends from)";
        return NOTOK;
    }

    size_t bytes = sizeof(double) * capacity * (size_t) itype;
    double *new_data = csound->Calloc(csound, bytes);
    if (new_data == NULL) {
        *err = "Memory allocation failed";
        return NOTOK;
    }

    CSN_ARRAY *array = slot->array;
    double *old_data = array->data;

    array->data = new_data;
    array->size = size;
    array->capacity = capacity;
    array->ndim = ndim;
    array->itype = itype;
    memset(array->shape, 0, sizeof(array->shape));
    memset(array->strides, 0, sizeof(array->strides));
    memcpy(array->shape, shape, sizeof(uint32_t) * ndim);
    memcpy(array->strides, strides, sizeof(size_t) * ndim);
    *out_array = array;

    /* Fresh Calloc'd storage under a possibly new layout: nothing a consumer
       cached about this array survives, so every counter moves. */
    update_array_version(&array->version);

    csound->Free(csound, old_data);
    return OK;
}

int32_t update_slot_array(
    CSOUND *csound,
    CSN_REGISTRY *registry,
    uint32_t handle,
    uint32_t ndim,
    const uint32_t *shape,
    ITEM_TYPE itype,
    CSN_ARRAY **out_array,
    const char **err
) {
    if (registry == NULL) {
        if (err != NULL) {
            *err = "Array registry is not available";
        }
        return NOTOK;
    }

    csound->LockMutex(registry->mutex);
    int32_t res = update_slot_array_locked(csound, registry, handle, ndim, shape, itype, out_array, err);
    csound->UnlockMutex(registry->mutex);
    return res;
}

int32_t allocate_array(CSOUND *csound, CSN_ARRAY *array, uint32_t ndim, const uint32_t *shape, uint32_t array_id, ITEM_TYPE itype) {
    if (ndim == 0 || ndim > CSN_MAX_DIMS || shape == NULL) {
        return NOTOK;
    }

    /* A zero extent is legal and yields a zero-length array: that is how an
       empty stack is represented, and it matches numpy's np.zeros(0). */
    size_t array_length = 1;
    for (uint32_t i = 0; i < ndim; i++) {
        uint32_t s = shape[i];
        if (s != 0 && array_length > CSN_MAX_ELEMS / s) {
            return NOTOK;
        }
        array_length *= s;
        array->shape[i] = s;
    }

    /* Every array built so far holds plain reals. ITEM_TYPE doubles as the
       item's width in doubles (REAL == 1), so the byte math below is already
       correct once COMPLEX arrays start being allocated. */
    array->itype = itype;

    /* Always keep room for at least one element, so data is never NULL and
       the first push into an empty array needs no special case. capacity
       counts items, not doubles. */
    array->capacity = array_length > 0 ? array_length * 2 : 1;
    double *array_data = csound->Calloc(csound, sizeof(double) * array->capacity * array->itype);
    if (array_data == NULL) {
        return NOTOK;
    }

    /* Row-major: last axis is contiguous, each earlier stride spans the
       whole extent of the axis to its right. */
    compute_strides(array->shape, array->strides, ndim);

    array->data = array_data;
    array->size = array_length;
    array->ndim = ndim;
    array->array_id = array_id;

    /* init array version */
    init_array_version(&array->version);

#ifdef CSN_VERSION_CROSSCHECK
    array->shadow = NULL;
    array->shadow_capacity = 0;
    array->shadow_size = 0;
    array->shadow_data_version = CSN_ARRAY_NULL_VERSION;
#endif

    return OK;
}

static void destroy_array(CSOUND *csound, CSN_ARRAY *array) {
    if (array != NULL) {
#ifdef CSN_VERSION_CROSSCHECK
        csn_release_shadow(csound, array);
#endif
        if (array->data != NULL) {
            csound->Free(csound, array->data);
        }
        csound->Free(csound, array);
    }
}

int32_t activate_slot(CSOUND *csound, CSN_REGISTRY *registry, CSN_SLOT *slot, uint32_t ndim, const uint32_t *shape, uint32_t array_id, ITEM_TYPE itype) {
    if (registry == NULL || slot == NULL) {
        return NOTOK;
    }

    if (slot->state != INACTIVE_SLOT || slot->array != NULL) {
        return NOTOK;
    }
    CSN_ARRAY *array = csound->Calloc(csound, sizeof(CSN_ARRAY));
    if (array == NULL) {
        return NOTOK;
    }

    int32_t res = allocate_array(csound, array, ndim, shape, array_id, itype);
    if (res != OK) {
        csound->Free(csound, array);
        return NOTOK;
    }

    slot->state = ACTIVE_SLOT;
    slot->array = array;
    slot->rt_locked = false;
    registry->active_count++;
    return OK;
}

int32_t release_slot(CSOUND *csound, CSN_REGISTRY *registry, CSN_SLOT *slot) {
    if (registry == NULL || slot == NULL) {
        return NOTOK;
    }

    if (slot->state != ACTIVE_SLOT || slot->array == NULL) {
        return NOTOK;
    }

    destroy_array(csound, slot->array);

    slot->array = NULL;
    slot->state = INACTIVE_SLOT;
    slot->gen_id = (slot->gen_id + 1U) & CSN_GEN_MASK;

    if (slot->gen_id == 0) {
        slot->gen_id = 1;
    }

    if (registry->active_count > 0) {
        registry->active_count--;
    }

    return OK;
}

/* Copies the payload and the layout, but not ndim: both callers preserve the
   rank and only reshape one axis. A caller that changes rank must set it. */
void travase_csnarray(CSN_ARRAY *dest, const CSN_ARRAY *src) {
    bool shape_changed = dest->size != src->size || memcmp(dest->shape, src->shape, sizeof(uint32_t) * src->ndim) != 0;
    bool itype_changed = dest->itype != src->itype;

    memcpy(dest->data, src->data, sizeof(double) * src->size * src->itype);
    memcpy(dest->shape, src->shape, sizeof(uint32_t) * src->ndim);
    memcpy(dest->strides, src->strides, sizeof(size_t) * src->ndim);
    dest->size = src->size;
    dest->capacity = src->capacity;
    dest->itype = src->itype;

    /* dest keeps its own identity and its own counters; it does not inherit
       src's. Its payload was just overwritten, so its data version moves
       whatever src's happens to be. */
    update_array_data_version(&dest->version);
    update_array_layout_version(&dest->version, shape_changed, false, itype_changed);
}

/* A consumer caches the version it last computed from inside its own opcode
   struct, which Csound zeroes at allocation. Arrays therefore start at
   CSN_ARRAY_FIRST_VERSION and never reach CSN_ARRAY_NULL_VERSION again: if
   they started at zero too, a consumer's first pass would compare equal and
   skip the very computation that fills its buffer. */
/* Never reset and never reused, so no two arrays in a run share a uid. The
   registry mutex serializes every perf-time creation and init is single
   threaded, which is what lets a plain counter stand in for an atomic. A clock
   reading would not do: array creation runs at some six thousand arrays per
   millisecond, so any wall-clock stamp would hand the same value to thousands
   of them. At this rate the counter takes on the order of a hundred thousand
   years to wrap. */
static uint64_t csn_array_uid_counter = 0;

void init_array_version(ARRAY_VERSION *version) {
    version->array_uid = ++csn_array_uid_counter;
    version->data_version = CSN_ARRAY_FIRST_VERSION;
    version->shape_version = CSN_ARRAY_FIRST_VERSION;
    version->ndim_version = CSN_ARRAY_FIRST_VERSION;
    version->itype_version = CSN_ARRAY_FIRST_VERSION;
}

/* Each counter moves on its own. A consumer that only needs the layout (an
   index map, a stride plan, a window whose length follows the shape) keeps its
   cache while the values underneath it change, and the reverse holds for one
   that only reads values. Bumping them together would collapse the four
   counters back into a single bit. */
void update_array_data_version(ARRAY_VERSION *version) {
    version->data_version++;
}

void update_array_layout_version(ARRAY_VERSION *version, bool shape_changed, bool ndim_changed, bool itype_changed) {
    if (shape_changed) version->shape_version++;
    if (ndim_changed) version->ndim_version++;
    if (itype_changed) version->itype_version++;
}

void update_array_version(ARRAY_VERSION *version) {
    version->data_version++;
    version->shape_version++;
    version->ndim_version++;
    version->itype_version++;
}

bool is_same_array_version(const ARRAY_VERSION *version_a, const ARRAY_VERSION *version_b) {
    return version_a->array_uid == version_b->array_uid
        && version_a->data_version == version_b->data_version
        && version_a->shape_version == version_b->shape_version
        && version_a->ndim_version == version_b->ndim_version
        && version_a->itype_version == version_b->itype_version;
}

bool is_same_array_data_version(const ARRAY_VERSION *version_a, const ARRAY_VERSION *version_b) {
    return version_a->array_uid == version_b->array_uid
        && version_a->data_version == version_b->data_version;
}

void set_array_version(ARRAY_VERSION *version_a, const ARRAY_VERSION *version_b) {
    version_a->array_uid = version_b->array_uid;
    version_a->data_version = version_b->data_version;
    version_a->shape_version = version_b->shape_version;
    version_a->ndim_version = version_b->ndim_version;
    version_a->itype_version = version_b->itype_version;
}

static uint32_t pcg32_random_u32(PCG32_STATE *rng) {
    uint64_t oldstate = rng->state;
    rng->state = oldstate * UINT64_C(6364136223846793005) + (rng->inc | UINT64_C(1));
    uint32_t xorshifted = (uint32_t)(((oldstate >> 18u) ^ oldstate) >> 27u);
    uint32_t rot = (uint32_t)(oldstate >> 59u);
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31u));
}

uint32_t pcg32_bounded_u32(PCG32_STATE *rng, uint32_t bound) {
    uint32_t threshold = (uint32_t)(-bound) % bound;
    for (;;) {
        uint32_t r = pcg32_random_u32(rng);
        if (r >= threshold) return r % bound;
    }
}

double pcg32_random(PCG32_STATE *rng) {
    return (double) pcg32_random_u32(rng) / 4294967296.0;
}

static void pcg32_seed(PCG32_STATE *rng, uint64_t seed, uint64_t sequence) {
    rng->state = 0U;
    rng->inc = (sequence << 1U) | 1U;
    (void) pcg32_random(rng);
    rng->state += seed;
    (void) pcg32_random(rng);
}

static void pcg32_manual_seed(PCG32_STATE *rng, uint64_t seed) {
    pcg32_seed(rng, seed, 0);
}

static void pcg32_auto_seed(PCG32_STATE *rng) {
    /* Three independent sources, because each one alone repeats:
       time() only ticks once a second, so two `csnseed 0` calls inside the
       same second replayed the same stream; the registry address is fixed for
       the whole process; and the counter alone is identical in two instances
       launched together. The nanosecond field separates reseeds, the address
       separates instances, the counter separates reseeds that land in the
       same clock tick. */
    static CSN_AUTO_SEED_COUNTER auto_seed_counter = 0;

    struct timespec ts;
    uint64_t now = (uint64_t) time(NULL) * UINT64_C(1000000000);
    if (timespec_get(&ts, TIME_UTC) == TIME_UTC) {
        now = (uint64_t) ts.tv_sec * UINT64_C(1000000000) + (uint64_t) ts.tv_nsec;
    }

    uint64_t tag = (uint64_t) (uintptr_t) rng;
    uint64_t tick = (uint64_t) (++auto_seed_counter);
    uint64_t seed = now ^ (tag * UINT64_C(0x9E3779B97F4A7C15)) ^ (tick * UINT64_C(0xBF58476D1CE4E5B9));

    pcg32_seed(rng, seed, tag);
}

void pcg32_random_init(PCG32_STATE *rng, uint64_t seed) {
    if (seed == 0) pcg32_auto_seed(rng);
    else pcg32_manual_seed(rng, seed);
}

// CSN TYPE SYSTEM

static void csnref_init_memory(CSOUND *csound, CS_VARIABLE *var, MYFLT *memblock) {
    (void) csound;
    memset(memblock, 0, var->memBlockSize);
}

#ifdef CS_TYPE_ARG_4ARG
static struct csvariable *csnref_create(void *cs, const struct cstype *type, const void *typeArg, struct insds *ctx) {
    (void) type;
    (void) typeArg;
#else
static struct csvariable *csnref_create(void *cs, void *p, struct insds *ctx) {
    (void) p;
#endif
    CSOUND *csound = (CSOUND *) cs;
    CS_VARIABLE *var = (CS_VARIABLE *) csound->Calloc(csound, sizeof(CS_VARIABLE));
    if (var == NULL) return NULL;

    var->memBlockSize = CS_FLOAT_ALIGN(sizeof(CSNREF));
    var->initializeVariableMemory = &csnref_init_memory;
    var->ctx = ctx;
    return var;
}

static void csnref_copy_value(CSOUND *csound, const struct cstype *cstype, void *dest, const void *src, struct insds *ctx) {
    (void) csound; (void) cstype; (void) ctx;
    if (src != NULL && dest != NULL) {
        memcpy(dest, src, sizeof(CSNREF));
    }
}

// ARRAY REAL
static CS_TYPE CS_VAR_TYPE_CSNARRAY = {
    "CsnArr",
    "csn array registry-id",
    CS_ARG_TYPE_BOTH,
    csnref_create,
    csnref_copy_value,
    NULL,
    NULL,
    0
};

int32_t csn_register_type(CSOUND *csound) {
    TYPE_POOL *pool = csound->GetTypePool(csound);
    if (pool == NULL) {
        return OK;
    }

    /* Csound may initialise a command-line plugin once while parsing options
       and again when compiling the orchestra. Treat the second registration
       as success: it is the same process-wide type, not a conflicting one. */
    if (csound->GetType(csound, "CsnArr") != NULL) {
        return OK;
    }

    return csound->AddVariableType(csound, pool, &CS_VAR_TYPE_CSNARRAY) ? OK : NOTOK;
}
