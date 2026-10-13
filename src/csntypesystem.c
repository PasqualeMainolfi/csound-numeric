#include "csntypesystem.h"
#include "csnregistry.h"
#include <string.h>
#include <csdl.h>
#include <stdint.h>

static void csnarr_init_memory(CSOUND *csound, CS_VARIABLE *var, cs_float *memblock) {
    (void) csound;
    memset(memblock, 0, var->memBlockSize);
}

#ifdef CS_TYPE_ARG_4ARG
static struct csvariable *csnarr_create(void *cs, const struct cstype *type, const void *typeArg, struct insds *ctx) {
    (void) type;
    (void) typeArg;
#else
static struct csvariable *csnarr_create(void *cs, void *p, struct insds *ctx) {
    (void) p;
#endif
    CSOUND *csound = (CSOUND *) cs;
    CS_VARIABLE *var = (CS_VARIABLE *) csound->Calloc(csound, sizeof(CS_VARIABLE));
    if (var == NULL) return NULL;

    var->memBlockSize = CS_FLOAT_ALIGN(sizeof(CSN_ARRAY));
    var->initializeVariableMemory = &csnarr_init_memory;
    var->ctx = ctx;
    return var;
}

static void csnarr_copy_value(CSOUND *csound, const struct cstype *cstype, void *dest, const void *src, struct insds *ctx) {
    (void) cstype;
    (void) ctx;
    CSN_ARRAY *d = (CSN_ARRAY *) dest;
    const CSN_ARRAY *s = (const CSN_ARRAY *) src;
    if (d == s || s->data == NULL) return;
    if (d->data == NULL) init_array_version(&d->version);
    if (d->itype != s->itype) {
        d->capacity = d->itype ? d->capacity * d->itype / s->itype : 0;
        d->itype = s->itype;
    }
    size_t req_size = s->size == 0 ? 1 : s->size;
    if (ensure_mutation_capacity(csound, NULL, d, req_size, false) != OK) return;
    travase_csnarray(d, s);
    update_array_version(&d->version);
    csn_copy_array_kind(d, s);
}

static void csnarr_free(void *cs, void *mem) {
    CSOUND *csound = (CSOUND *) cs;
    CSN_ARRAY *a = (CSN_ARRAY *) mem;
#ifdef CSN_VERSION_CROSSCHECK
    csn_release_shadow(csound, a);
#endif
    if (a->data != NULL) csound->Free(csound, a->data);
    memset(a, 0, sizeof(CSN_ARRAY));
}


static CS_TYPE CS_VAR_TYPE_CSNARRAY = {
    "CsnArr",
    "csn array",
    CS_ARG_TYPE_BOTH,
    csnarr_create,
    csnarr_copy_value,
    csnarr_free,
    NULL,
    0
};

#ifdef CS_TYPE_ARG_4ARG
static struct csvariable *csnstr_create(void *cs, const struct cstype *type, const void *typeArg, struct insds *ctx) {
    (void) type;
    (void) typeArg;
#else
static struct csvariable *csnstr_create(void *cs, void *p, struct insds *ctx) {
    (void) p;
#endif
    CSOUND *csound = (CSOUND *) cs;
    CS_VARIABLE *var = (CS_VARIABLE *) csound->Calloc(csound, sizeof(CS_VARIABLE));
    if (var == NULL) return NULL;

    var->memBlockSize = CS_FLOAT_ALIGN(sizeof(CSN_ARRAY));
    var->initializeVariableMemory = &csnarr_init_memory;
    var->ctx = ctx;
    return var;
}

static void csnstr_copy_value(CSOUND *csound, const struct cstype *cstype, void *dest, const void *src, struct insds *ctx) {
    (void) cstype;
    (void) ctx;
    CSN_ARRAY *d = (CSN_ARRAY *) dest;
    const CSN_ARRAY *s = (const CSN_ARRAY *) src;
    if (d == s || s->sdata == NULL) return;
    if (d->sdata == NULL) init_array_version(&d->version);
    d->itype = CSN_STRING;
    size_t req_size = s->size == 0 ? 1 : s->size;
    if (ensure_mutation_capacity(csound, NULL, d, req_size, false) != OK) return;
    travase_csnstrings(csound, d, s);
    update_array_version(&d->version);
    csn_copy_array_kind(d, s);
}

static void csnstr_free(void *cs, void *mem) {
    CSOUND *csound = (CSOUND *) cs;
    CSN_ARRAY *a = (CSN_ARRAY *) mem;
#ifdef CSN_VERSION_CROSSCHECK
    csn_release_shadow(csound, a);
#endif
    free_csnstrings(csound, a);
    memset(a, 0, sizeof(CSN_ARRAY));
}

static CS_TYPE CS_VAR_TYPE_CSNSTRINGS = {
    "CsnStr",
    "csn strings",
    CS_ARG_TYPE_BOTH,
    csnstr_create,
    csnstr_copy_value,
    csnstr_free,
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
    if (csound->GetType(csound, "CsnArr") == NULL) {
        if (!csound->AddVariableType(csound, pool, &CS_VAR_TYPE_CSNARRAY)) return NOTOK;
    }

    if (csound->GetType(csound, "CsnStr") == NULL) {
        if (!csound->AddVariableType(csound, pool, &CS_VAR_TYPE_CSNSTRINGS)) return NOTOK;
    }

    return OK;
}
