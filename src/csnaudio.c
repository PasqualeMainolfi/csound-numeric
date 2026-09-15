/* Opcode implementations for the audio family.
   The public opcode inventory remains centralized in csnum.c. */
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

int32_t csnarray_from_audio_init(CSOUND *csound, CSN_FROM_AUDIO *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    if (!IS_VALID_ZERO_ONE((double) *p->rt_lock)) {
        return csound->InitError(csound, "[csnarray] Resize param must be 0 = realloc at perf-time or 1 = do not realloc at perf-time");
    }

    uint32_t shape[CSN_MAX_DIMS] = {0};
    shape[0] = CS_KSMPS;

    int32_t res = create_csnarray_init(csound, &p->h, 1U, shape, &p->array, p->handle, CSN_REAL);
    if (res != OK) return res;

    csound->LockMutex(reg->mutex);
    bool is_locked = (*p->rt_lock != 0.0);
    CSN_SLOT *slot = get_slot(reg, p->handle->id);
    if (slot != NULL) slot->rt_locked = is_locked;
    fill_csnarray(p->array, 0.0);
    SET_KDATA_WITH_ID_BEGIN(p, reg, shape, 1U, CSN_REAL, p->handle->id);
    csound->UnlockMutex(reg->mutex);
    return OK;
}

int32_t csnarray_from_audio(CSOUND *csound, CSN_FROM_AUDIO *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    uint32_t source_handle = p->k_data.owned_handle;
    CHECK_REG_HANDLE(csound, &p->h, reg, source_handle);

    uint32_t nsamples = CS_KSMPS;
    uint32_t offset = p->h.insdshead->ksmps_offset;
    uint32_t early = p->h.insdshead->ksmps_no_end;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
    }

    p->array = slot->array;
    CSN_ARRAY *buffer = p->array;
    if (buffer->size != CS_KSMPS || buffer->ndim != 1U || buffer->itype != CSN_REAL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid slot: array size and dimension mismatch");
    }

    if (offset) memset(buffer->data, 0, sizeof(double) * offset);
    if (early) {
        nsamples -= early;
        memset(buffer->data + nsamples, 0, sizeof(double) * early);
    }

    for (uint32_t i = offset; i < nsamples; i++) {
        buffer->data[i] = (double) p->source_sig[i];
    }

    update_array_data_version(&buffer->version);
    SET_KDATA_END(p, buffer->shape, 1U, CSN_REAL);
    csound->UnlockMutex(reg->mutex);

    return OK;
}

int32_t csnarray_to_audio_init(CSOUND *csound, CSN_TO_AUDIO *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = (uint32_t) p->source_handle->id;
    uint32_t nsamples = CS_KSMPS;
    uint32_t shape[CSN_MAX_DIMS] = {0};
    shape[0] = nsamples;

    int32_t res = OK;

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
        goto done;
    }

    CSN_ARRAY *buffer = slot->array;
    if (buffer->size != CS_KSMPS || buffer->itype != CSN_REAL) {
        res = csound->InitError(csound, "[csnarray] Invalid csn-audio buffer: size/dim/dtype mismatch");
        goto done;
    }

    p->registry = reg;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_to_audio(CSOUND *csound, CSN_TO_AUDIO *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    uint32_t source_handle = (uint32_t) p->source_handle->id;

    uint32_t nsamples = CS_KSMPS;
    uint32_t offset = p->h.insdshead->ksmps_offset;
    uint32_t early = p->h.insdshead->ksmps_no_end;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
    }

    CSN_ARRAY *buffer = slot->array;
    if (buffer->size != CS_KSMPS || buffer->itype != CSN_REAL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid csn-audio buffer: size/dim/dtype mismatch");
    }

    if (offset) memset(p->sig, 0, sizeof(MYFLT) * offset);
    if (early) {
        nsamples -= early;
        memset(p->sig + nsamples, 0, sizeof(MYFLT) * early);
    }

    for (uint32_t i = offset; i < nsamples; i++) {
        p->sig[i] = (MYFLT) buffer->data[i];
    }

    csound->UnlockMutex(reg->mutex);
    return OK;
}

int32_t csnarray_pack_audio_init(CSOUND *csound, CSN_PACK_AUDIO *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    if (p->source_sig == NULL || p->source_sig->sizes == NULL || p->source_sig->sizes[0] == 0) {
        return csound->InitError(csound, "[csnarray] Empty audio array");
    }

    if (!IS_VALID_ZERO_ONE((double) *p->rt_lock)) {
        return csound->InitError(csound, "[csnarray] Resize param must be 0 = realloc at perf-time or 1 = do not realloc at perf-time");
    }

    uint32_t nchnls = (uint32_t) p->source_sig->sizes[0];

    uint32_t shape[CSN_MAX_DIMS] = {0};
    shape[0] = nchnls;
    shape[1] = CS_KSMPS;

    int32_t res = create_csnarray_init(csound, &p->h, 2U, shape, &p->array, p->handle, CSN_REAL);
    if (res != OK) return res;

    csound->LockMutex(reg->mutex);
    bool is_locked = (*p->rt_lock != 0.0);
    CSN_SLOT *slot = get_slot(reg, p->handle->id);
    if (slot != NULL) slot->rt_locked = is_locked;
    fill_csnarray(p->array, 0.0);
    SET_KDATA_WITH_ID_BEGIN(p, reg, shape, 2U, CSN_REAL, p->handle->id);
    p->prev_nchnls = nchnls;
    csound->UnlockMutex(reg->mutex);
    return OK;
}

int32_t csnarray_pack_audio(CSOUND *csound, CSN_PACK_AUDIO *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    uint32_t source_handle = p->k_data.owned_handle;
    CHECK_REG_HANDLE(csound, &p->h, reg, source_handle);

    uint32_t nsamples = CS_KSMPS;
    uint32_t offset = p->h.insdshead->ksmps_offset;
    uint32_t early = p->h.insdshead->ksmps_no_end;
    uint32_t end_offset = nsamples - early;

    if (p->source_sig == NULL || p->source_sig->sizes == NULL || p->source_sig->sizes[0] == 0) {
        return csound->PerfError(csound, &p->h, "[csnarray] Empty audio array");
    }

    uint32_t nchnls = (uint32_t) p->source_sig->sizes[0];
    if (nchnls != p->prev_nchnls) {
        return csound->PerfError(csound, &p->h, "[csnarray] Shape change not allowed on audio path");
    }

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
    }

    CSN_ARRAY *buffer = slot->array;
    for (size_t i = 0; i < (size_t) nchnls; i++) {
        MYFLT *channel = p->source_sig->data + i * CS_KSMPS;
        double *row = buffer->data + i * CS_KSMPS;
        for (size_t j = 0; j < nsamples; j++) {
            row[j] = (j < offset || j >= end_offset) ? 0.0 : (double) channel[j];
        }
    }

    update_array_data_version(&buffer->version);
    SET_KDATA_END(p, buffer->shape, 2U, CSN_REAL);

    csound->UnlockMutex(reg->mutex);
    return OK;
}


int32_t csnarray_unpack_audio_init(CSOUND *csound, CSN_UNPACK_AUDIO *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = (uint32_t) p->source_handle->id;
    int32_t res = OK;

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
        goto done;
    }

    CSN_ARRAY *buffer = slot->array;
    if (buffer->ndim != 2U || buffer->itype != CSN_REAL || buffer->shape[1] != CS_KSMPS) {
        res = csound->InitError(csound, "[csnarray] Invalid csn-audio buffer: size/dim/dtype mismatch");
        goto done;
    }

    int32_t nchnls = (int32_t) buffer->shape[0];
    if (tabinit(csound, p->sig, nchnls, p->h.insdshead) != OK) {
        res = csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
        goto done;
    }

    p->registry = reg;
    p->prev_nchnls = buffer->shape[0];

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_unpack_audio(CSOUND *csound, CSN_UNPACK_AUDIO *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    uint32_t source_handle = (uint32_t) p->source_handle->id;

    uint32_t nsamples = CS_KSMPS;
    uint32_t offset = p->h.insdshead->ksmps_offset;
    uint32_t early = p->h.insdshead->ksmps_no_end;
    uint32_t end_offset = nsamples - early;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
    }

    CSN_ARRAY *buffer = slot->array;
    if (buffer->ndim != 2U || buffer->itype != CSN_REAL || buffer->shape[1] != CS_KSMPS) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid csn-audio buffer: size/dim/dtype mismatch");
    }

    uint32_t nchnls = buffer->shape[0];
    if (nchnls != p->prev_nchnls) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Shape change not allowed on audio path");
    }

    for (uint32_t i = 0; i < nchnls; i++) {
        MYFLT *channel_out = p->sig->data + i * CS_KSMPS;
        double *channel_in = buffer->data + i * CS_KSMPS;
        for (uint32_t j = 0; j < nsamples; j++) {
            channel_out[j] = (j < offset || j >= end_offset) ? FL(0.0) : (MYFLT) channel_in[j];
        }
    }

    csound->UnlockMutex(reg->mutex);
    return OK;
}

int32_t csnarray_frame_audio_init(CSOUND *csound, CSN_FRAME_AUDIO *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    if (!IS_VALID_ZERO_ONE((double) *p->rt_lock)) {
        return csound->InitError(csound, "[csnarray] Resize param must be 0 = realloc at perf-time or 1 = do not realloc at perf-time");
    }

    int32_t res = OK;
    const char *err = NULL;

    double frame_size_temp = (double) *p->frame_size;
    if (!IS_VALID_LENGTH(frame_size_temp)) {
        return csound->InitError(csound, "[csnarray] Invalid frame size");
    }
    if (frame_size_temp == 0.0) {
        return csound->InitError(csound, "[csnarray] Zero-length frame size");
    }

    double hop_size_temp = (double) *p->hop_size;
    if (!IS_VALID_LENGTH(hop_size_temp)) {
        return csound->InitError(csound, "[csnarray] Invalid hop size");
    }
    hop_size_temp = hop_size_temp == 0.0 ? frame_size_temp : hop_size_temp;

    if (hop_size_temp < CS_KSMPS) {
        return csound->InitError(csound, "[csnarray] Hop size must be greater or equal to ksmps");
    }

    if (hop_size_temp > frame_size_temp) {
        return csound->InitError(csound, "[csnarray] Hop size must be less or equal to frame size");
    }


    p->fsize = (size_t) frame_size_temp;
    p->hsize = (size_t) hop_size_temp;

    size_t buffer_cap = p->fsize * 2;
    double *buffer_temp = csound->Calloc(csound, sizeof(double) * buffer_cap);
    if (buffer_temp == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
    }

    p->buffer.scratch = buffer_temp;
    p->buffer.scratch_capacity = buffer_cap;
    p->buffer.current_size = 0;
    p->buffer.reader = 0;
    p->buffer.writer = 0;
    p->buffer.sample_count = 0;

    *p->is_ready = FL(0.0);

    csound->LockMutex(reg->mutex);
    uint32_t shape[CSN_MAX_DIMS] = {0};
    shape[0] = (uint32_t) p->fsize;
    if (create_csnarray_locked(csound, reg, &p->h, 1U, shape, &p->array, p->handle, NULL, 0, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    bool is_locked = (*p->rt_lock != 0.0);
    CSN_SLOT *slot = get_slot(reg, p->handle->id);
    if (slot != NULL) slot->rt_locked = is_locked;
    fill_csnarray(p->array, 0.0);
    SET_KDATA_WITH_ID_BEGIN(p, reg, shape, 1U, CSN_REAL, p->handle->id);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_frame_audio(CSOUND *csound, CSN_FRAME_AUDIO *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    uint32_t source_handle = p->k_data.owned_handle;
    CHECK_REG_HANDLE(csound, &p->h, reg, source_handle);

    uint32_t nsamples = CS_KSMPS;
    uint32_t offset = p->h.insdshead->ksmps_offset;
    uint32_t early = p->h.insdshead->ksmps_no_end;
    uint32_t end_offset = nsamples - early;

    size_t fsize = p->fsize;
    size_t hsize = p->hsize;
    CSN_SCRATCH *buffer = &p->buffer;
    double *cbuffer = (double *) buffer->scratch;

    // writer
    for (uint32_t i = 0; i < nsamples; i++) {
        size_t index = buffer->writer;
        cbuffer[index] = (i < offset || i >= end_offset) ? 0.0 : (double) p->source_sig[i];
        buffer->writer = (buffer->writer + 1U) % buffer->scratch_capacity;
        if (buffer->current_size < buffer->scratch_capacity) {
            buffer->current_size++;
        }
    }

    if (buffer->current_size >= fsize) {
        csound->LockMutex(reg->mutex);
        CSN_SLOT *slot = get_slot(reg, source_handle);
        if (slot == NULL) {
            csound->UnlockMutex(reg->mutex);
            return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
        }

        CSN_ARRAY *frame = p->array;
        // reader
        for (size_t i = 0; i < fsize; i++) {
            size_t index = (buffer->reader + i) % buffer->scratch_capacity;
            frame->data[i] = cbuffer[index];
        }
        buffer->reader = (buffer->reader + hsize) % buffer->scratch_capacity;
        buffer->current_size -= hsize;

        update_array_data_version(&frame->version);
        *p->is_ready = FL(1.0);
        csound->UnlockMutex(reg->mutex);
    } else {
        *p->is_ready = FL(0.0);
    }

    p->handle->id = p->k_data.owned_handle;
    return OK;
}

int32_t csnarray_ola_audio_deinit(CSOUND *csound, CSN_OLA_AUDIO *p) {
    deinit_scratch(csound, &p->buffer);
    return OK;
}

int32_t csnarray_ola_audio_init(CSOUND *csound, CSN_OLA_AUDIO *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    double hop_size_temp = (double) *p->hop_size;
    if (!IS_VALID_LENGTH(hop_size_temp)) {
        return csound->InitError(csound, "[csnarray] Invalid hop size");
    }

    if (hop_size_temp < CS_KSMPS) {
        return csound->InitError(csound, "[csnarray] Hop size %d must be greater or equal to ksmps %d", (int32_t) hop_size_temp, (int32_t) CS_KSMPS);
    }

    int32_t res = OK;
    uint32_t source_handle = (uint32_t) p->source_handle->id;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
        goto done;
    }

    CSN_ARRAY *frame = slot->array;
    if (frame->ndim != 1U || frame->itype != CSN_REAL || frame->size == 0) {
        res = csound->InitError(csound, "[csnarray] Invalid csn-audio frame: expected a non-empty 1-D real array");
        goto done;
    }

    p->fsize = frame->size;
    p->hsize = (size_t) hop_size_temp;

    if (p->hsize > p->fsize) {
        res = csound->InitError(csound, "[csnarray] Hop size %zu must be less or equal to the frame size %zu", p->hsize, p->fsize);
        goto done;
    }

    size_t buffer_cap = p->fsize * 2;
    double *buffer_temp = csound->Calloc(csound, sizeof(double) * buffer_cap);
    if (buffer_temp == NULL) {
        res = csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
        goto done;
    }

    p->buffer.scratch = buffer_temp;
    p->buffer.scratch_capacity = buffer_cap;
    p->buffer.current_size = 0;
    p->buffer.reader = 0;
    p->buffer.writer = 0;
    p->buffer.sample_count = 0;

    p->prev_handle = source_handle;
    set_array_version(&frame->version, &p->prev_version);

    p->phase = 0;
    *p->is_ready = FL(0.0);
    p->registry = reg;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_ola_audio(CSOUND *csound, CSN_OLA_AUDIO *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    uint32_t source_handle = (uint32_t) p->source_handle->id;

    uint32_t nsamples = CS_KSMPS;
    uint32_t offset = p->h.insdshead->ksmps_offset;
    uint32_t early = p->h.insdshead->ksmps_no_end;
    uint32_t end_offset = nsamples - early;

    size_t fsize = p->fsize;
    size_t hsize = p->hsize;
    CSN_SCRATCH *buffer = &p->buffer;
    double *acc = (double *) buffer->scratch;
    size_t capacity = buffer->scratch_capacity;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
    }

    CSN_ARRAY *frame = slot->array;
    if (frame->ndim != 1U || frame->itype != CSN_REAL || frame->size != fsize) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Frame layout changed since init: expected a 1-D real array of %zu elements", fsize);
    }

    bool is_same_version = is_same_array_data_version(&frame->version, &p->prev_version);
    bool is_same = p->prev_handle == source_handle && is_same_version;

    p->phase += nsamples;
    bool is_consumed = p->phase >= hsize;
    if (is_consumed) p->phase -= hsize;

    if (is_consumed && !is_same) {
        for (size_t i = 0; i < fsize; i++) {
            acc[(buffer->writer + i) % capacity] += frame->data[i];
        }
        buffer->writer = (buffer->writer + hsize) % capacity;
        buffer->current_size += hsize;

        p->prev_handle = source_handle;
        set_array_version(&p->prev_version, &frame->version);
    }

    csound->UnlockMutex(reg->mutex);

    if (buffer->current_size >= (size_t) nsamples) {
        for (uint32_t j = 0; j < nsamples; j++) {
            size_t index = (buffer->reader + j) % capacity;
            p->sig[j] = (j < offset || j >= end_offset) ? FL(0.0) : (MYFLT) acc[index];
            acc[index] = 0.0;
        }
        buffer->reader = (buffer->reader + nsamples) % capacity;
        buffer->current_size -= nsamples;
        *p->is_ready = FL(1.0);
    } else {
        memset(p->sig, 0, sizeof(MYFLT) * nsamples);
        *p->is_ready = FL(0.0);
    }

    return OK;
}



