#include "csnregistry.h"
#include <csdl.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <time.h>


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

    pcg32_random_init(&reg->rng);

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

int32_t allocate_array(CSOUND *csound, CSN_ARRAY *array, uint32_t ndim, const uint32_t *shape, uint32_t array_id) {
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
    array->itype = REAL;

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

    return OK;
}

static void destroy_array(CSOUND *csound, CSN_ARRAY *array) {
    if (array != NULL) {
        if (array->data != NULL) {
            csound->Free(csound, array->data);
        }
        csound->Free(csound, array);
    }
}

int32_t activate_slot(CSOUND *csound, CSN_REGISTRY *registry, CSN_SLOT *slot, uint32_t ndim, const uint32_t *shape, uint32_t array_id) {
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

    int32_t res = allocate_array(csound, array, ndim, shape, array_id);
    if (res != OK) {
        csound->Free(csound, array);
        return NOTOK;
    }

    slot->state = ACTIVE_SLOT;
    slot->array = array;
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
    memcpy(dest->data, src->data, sizeof(double) * src->size * src->itype);
    memcpy(dest->shape, src->shape, sizeof(uint32_t) * src->ndim);
    memcpy(dest->strides, src->strides, sizeof(size_t) * src->ndim);
    dest->size = src->size;
    dest->capacity = src->capacity;
    dest->itype = src->itype;
}

void pcg32_random_init(PCG32_STATE *rng) {
    /* time() only moves once a second and inc was a fixed constant, so two
       Csound instances launched together produced the identical stream — very
       audible when the arrays feed noise. The registry's own address separates
       the sequences. */
    uintptr_t tag = (uintptr_t) rng;
    rng->state = (uint64_t) time(NULL) ^ ((uint64_t) tag * 0x9E3779B97F4A7C15ULL);
    rng->inc = (((uint64_t) tag) << 1u) | 1u;

    /* PCG's seeding step: one advance so the first draw is properly mixed
       rather than a thin function of the seed. */
    (void) pcg32_random(rng);
}

double pcg32_random(PCG32_STATE *rng) {
    uint64_t oldstate = rng->state;
    rng->state = oldstate * 6364136223846793005ULL + (rng->inc | 1);
    uint32_t xorshifted = (uint32_t) (((oldstate >> 18u) ^ oldstate) >> 27u);
    uint32_t rot = oldstate >> 59u;
    uint32_t gen = (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
    return (double) gen / 4294967296.0;
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
    return csound->AddVariableType(csound, pool, &CS_VAR_TYPE_CSNARRAY) ? OK : NOTOK;
}
