/* Opcode implementations for the audio family.
   The public opcode inventory remains centralized in csnum.c. */
#include "csnum.h"
#include "csnum_internal.h"
#include "csnregistry.h"
#include <float.h>
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
        return csn_init_error(csound, "[csnarray] Resize param must be 0 = realloc at perf-time or 1 = do not realloc at perf-time");
    }

    uint32_t shape[CSN_MAX_DIMS] = {0};
    shape[0] = CS_KSMPS;

    CSNARR_RES pres = prepare_output(csound, &p->h, NULL, reg, p->handle, 1U, shape, CSN_REAL, NULL, 0, CSN_ALIAS_REFUSE);
    if (pres.err != OK) return pres.err;

    bool is_locked = (*p->rt_lock != 0.0);
    p->handle->rt_locked = is_locked;
    SET_KDATA_BEGIN(p->k_data, reg, shape, 1U, CSN_REAL);
    if (pres.locked) csn_unlock(csound, reg);
    return OK;
}

int32_t csnarray_from_audio(CSOUND *csound, CSN_FROM_AUDIO *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    uint32_t nsamples = CS_KSMPS;
    uint32_t offset = p->h.insdshead->ksmps_offset;
    uint32_t early = p->h.insdshead->ksmps_no_end;

    /* The layout was fixed at init and never changes here, so per pass only
       the lock is decided; the check still guards a global output that some
       other opcode has reshaped since. */
    CSN_ARRAY *buffer = p->handle;
    bool locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { buffer }, 1);
    if (buffer->size != CS_KSMPS || buffer->ndim != 1U || buffer->itype != CSN_REAL) {
        if (locked) csn_unlock(csound, reg);
        return csn_perf_error(csound, &p->h, "[csnarray] Invalid array: size and dimension mismatch");
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
    SET_KDATA_END(p->k_data, buffer->shape, 1U, CSN_REAL);
    if (locked) csn_unlock(csound, reg);

    return OK;
}

int32_t csnarray_to_audio_init(CSOUND *csound, CSN_TO_AUDIO *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    stamp_globals(csound, &p->h);
    p->registry = reg;
    return OK;
}

int32_t csnarray_to_audio(CSOUND *csound, CSN_TO_AUDIO *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);
    uint32_t nsamples = CS_KSMPS;
    uint32_t offset = p->h.insdshead->ksmps_offset;
    uint32_t early = p->h.insdshead->ksmps_no_end;

    CSN_ARRAY *buffer = p->source_handle;
    bool locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { buffer }, 1);
    if (buffer->size != CS_KSMPS || buffer->itype != CSN_REAL) {
        if (locked) csn_unlock(csound, reg);
        return csn_perf_error(csound, &p->h, "[csnarray] Invalid csn-audio buffer: size/dim/dtype mismatch");
    }

    if (offset) memset(p->sig, 0, sizeof(cs_float) * offset);
    if (early) {
        nsamples -= early;
        memset(p->sig + nsamples, 0, sizeof(cs_float) * early);
    }

    for (uint32_t i = offset; i < nsamples; i++) {
        p->sig[i] = (cs_float) buffer->data[i];
    }

    if (locked) csn_unlock(csound, reg);
    return OK;
}

int32_t csnarray_pack_audio_init(CSOUND *csound, CSN_PACK_AUDIO *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    if (p->source_sig == NULL || p->source_sig->sizes == NULL || p->source_sig->sizes[0] == 0) {
        return csn_init_error(csound, "[csnarray] Empty audio array");
    }

    if (!IS_VALID_ZERO_ONE((double) *p->rt_lock)) {
        return csn_init_error(csound, "[csnarray] Resize param must be 0 = realloc at perf-time or 1 = do not realloc at perf-time");
    }

    uint32_t nchnls = (uint32_t) p->source_sig->sizes[0];

    uint32_t shape[CSN_MAX_DIMS] = {0};
    shape[0] = nchnls;
    shape[1] = CS_KSMPS;

    CSNARR_RES pres = prepare_output(csound, &p->h, NULL, reg, p->handle, 2U, shape, CSN_REAL, NULL, 0, CSN_ALIAS_REFUSE);
    if (pres.err != OK) return pres.err;

    bool is_locked = (*p->rt_lock != 0.0);
    p->handle->rt_locked = is_locked;
    p->prev_nchnls = nchnls;
    p->registry = reg;
    if (pres.locked) csn_unlock(csound, reg);
    return OK;
}

int32_t csnarray_pack_audio(CSOUND *csound, CSN_PACK_AUDIO *p) {
    uint32_t nsamples = CS_KSMPS;
    uint32_t offset = p->h.insdshead->ksmps_offset;
    uint32_t early = p->h.insdshead->ksmps_no_end;
    uint32_t end_offset = nsamples - early;

    if (p->source_sig == NULL || p->source_sig->sizes == NULL || p->source_sig->sizes[0] == 0) {
        return csn_perf_error(csound, &p->h, "[csnarray] Empty audio array");
    }

    uint32_t nchnls = (uint32_t) p->source_sig->sizes[0];
    if (nchnls != p->prev_nchnls) {
        return csn_perf_error(csound, &p->h, "[csnarray] Shape change not allowed on audio path");
    }

    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);
    CSN_ARRAY *buffer = p->handle;
    bool locked = csn_lock_if_shared(csound, reg, (CSN_CONARR){ buffer }, 1);
    if (buffer->ndim != 2U || buffer->shape[0] != nchnls || buffer->shape[1] != CS_KSMPS) {
        if (locked) csn_unlock(csound, reg);
        return csn_perf_error(csound, &p->h, "[csnarray] Invalid csn-audio buffer: size/dim/dtype mismatch");
    }
    for (size_t i = 0; i < (size_t) nchnls; i++) {
        cs_float *channel = p->source_sig->data + i * CS_KSMPS;
        double *row = buffer->data + i * CS_KSMPS;
        for (size_t j = 0; j < nsamples; j++) {
            row[j] = (j < offset || j >= end_offset) ? 0.0 : (double) channel[j];
        }
    }

    update_array_data_version(&buffer->version);
    if (locked) csn_unlock(csound, reg);
    return OK;
}


int32_t csnarray_unpack_audio_init(CSOUND *csound, CSN_UNPACK_AUDIO *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    stamp_globals(csound, &p->h);
    p->registry = reg;

    CSN_ARRAY *buffer = p->source_handle;
    bool locked = csn_lock_if_shared(csound, reg, (CSN_CONARR){ buffer }, 1);
    if (buffer->ndim != 2U || buffer->itype != CSN_REAL || buffer->shape[1] != CS_KSMPS) {
        if (locked) csn_unlock(csound, reg);
        return csn_init_error(csound, "[csnarray] Invalid csn-audio buffer: size/dim/dtype mismatch");
    }

    int32_t nchnls = (int32_t) buffer->shape[0];
    p->prev_nchnls = buffer->shape[0];
    if (locked) csn_unlock(csound, reg);
    tabinit(csound, p->sig, nchnls, p->h.insdshead);
    if (p->sig->data == NULL || p->sig->sizes == NULL ||
        p->sig->arrayMemberSize <= 0 ||
        p->sig->allocated / (size_t) p->sig->arrayMemberSize < (size_t) nchnls) {
        return csn_init_error(csound, "[csnarray] Internal error: memory allocation failed");
    }

    return OK;
}

int32_t csnarray_unpack_audio(CSOUND *csound, CSN_UNPACK_AUDIO *p) {
    uint32_t nsamples = CS_KSMPS;
    uint32_t offset = p->h.insdshead->ksmps_offset;
    uint32_t early = p->h.insdshead->ksmps_no_end;
    uint32_t end_offset = nsamples - early;

    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);
    CSN_ARRAY *buffer = p->source_handle;
    bool locked = csn_lock_if_shared(csound, reg, (CSN_CONARR){ buffer }, 1);
    if (buffer->ndim != 2U || buffer->itype != CSN_REAL || buffer->shape[1] != CS_KSMPS) {
        if (locked) csn_unlock(csound, reg);
        return csn_perf_error(csound, &p->h, "[csnarray] Invalid csn-audio buffer: size/dim/dtype mismatch");
    }

    uint32_t nchnls = buffer->shape[0];
    if (nchnls != p->prev_nchnls) {
        if (locked) csn_unlock(csound, reg);
        return csn_perf_error(csound, &p->h, "[csnarray] Shape change not allowed on audio path");
    }

    for (uint32_t i = 0; i < nchnls; i++) {
        cs_float *channel_out = p->sig->data + i * CS_KSMPS;
        double *channel_in = buffer->data + i * CS_KSMPS;
        for (uint32_t j = 0; j < nsamples; j++) {
            channel_out[j] = (j < offset || j >= end_offset) ? FL(0.0) : (cs_float) channel_in[j];
        }
    }

    if (locked) csn_unlock(csound, reg);
    return OK;
}

int32_t csnarray_frame_audio_init(CSOUND *csound, CSN_FRAME_AUDIO *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    if (!IS_VALID_ZERO_ONE((double) *p->rt_lock)) {
        return csn_init_error(csound, "[csnarray] Resize param must be 0 = realloc at perf-time or 1 = do not realloc at perf-time");
    }

    int32_t res = OK;

    double frame_size_temp = (double) *p->frame_size;
    if (!IS_VALID_LENGTH(frame_size_temp)) {
        return csn_init_error(csound, "[csnarray] Invalid frame size");
    }
    if (frame_size_temp == 0.0) {
        return csn_init_error(csound, "[csnarray] Zero-length frame size");
    }

    double hop_size_temp = (double) *p->hop_size;
    if (!IS_VALID_LENGTH(hop_size_temp)) {
        return csn_init_error(csound, "[csnarray] Invalid hop size");
    }
    hop_size_temp = hop_size_temp == 0.0 ? frame_size_temp : hop_size_temp;

    if (hop_size_temp < CS_KSMPS) {
        return csn_init_error(csound, "[csnarray] Hop size must be greater or equal to ksmps");
    }

    if (hop_size_temp > frame_size_temp) {
        return csn_init_error(csound, "[csnarray] Hop size must be less or equal to frame size");
    }


    p->fsize = (size_t) frame_size_temp;
    p->hsize = (size_t) hop_size_temp;

    size_t buffer_cap = p->fsize * 2;
    double *buffer_temp = csound->Calloc(csound, sizeof(double) * buffer_cap);
    if (buffer_temp == NULL) {
        return csn_init_error(csound, "[csnarray] Internal error: memory allocation failed");
    }

    p->buffer.scratch = buffer_temp;
    p->buffer.scratch_capacity = buffer_cap;
    p->buffer.current_size = 0;
    p->buffer.reader = 0;
    p->buffer.writer = 0;
    p->buffer.sample_count = 0;

    *p->is_ready = FL(0.0);

    uint32_t shape[CSN_MAX_DIMS] = {0};
    shape[0] = (uint32_t) p->fsize;
    CSNARR_RES pres = prepare_output(csound, &p->h, NULL, reg, p->handle, 1U, shape, CSN_REAL, NULL, 0, CSN_ALIAS_REFUSE);
    res = pres.err;
    if (res != OK) goto done;

    /* the explicit request wins over the default prepare_output computed */
    p->handle->rt_locked = (*p->rt_lock != 0.0);
    p->registry = reg;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_frame_audio(CSOUND *csound, CSN_FRAME_AUDIO *p) {
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
        CSN_REGISTRY *reg = p->registry;
        CHECK_REGISTRY(csound, &p->h, reg);
        CSN_ARRAY *frame = p->handle;
        bool locked = csn_lock_if_shared(csound, reg, (CSN_CONARR){ frame }, 1);
        if (frame->ndim != 1U || frame->itype != CSN_REAL || frame->size != fsize) {
            if (locked) csn_unlock(csound, reg);
            return csn_perf_error(csound, &p->h, "[csnarray] Frame layout changed since init: expected a 1-D real array of %zu elements", fsize);
        }
        // reader
        for (size_t i = 0; i < fsize; i++) {
            size_t index = (buffer->reader + i) % buffer->scratch_capacity;
            frame->data[i] = cbuffer[index];
        }
        buffer->reader = (buffer->reader + hsize) % buffer->scratch_capacity;
        buffer->current_size -= hsize;

        update_array_data_version(&frame->version);
        if (locked) csn_unlock(csound, reg);
        *p->is_ready = FL(1.0);
    } else {
        *p->is_ready = FL(0.0);
    }

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
        return csn_init_error(csound, "[csnarray] Invalid hop size");
    }

    if (hop_size_temp < CS_KSMPS) {
        return csn_init_error(csound, "[csnarray] Hop size %d must be greater or equal to ksmps %d", (int32_t) hop_size_temp, (int32_t) CS_KSMPS);
    }

    stamp_globals(csound, &p->h);
    p->registry = reg;

    CSN_ARRAY *frame = p->source_handle;
    bool locked = csn_lock_if_shared(csound, reg, (CSN_CONARR){ frame }, 1);
    if (frame->ndim != 1U || frame->itype != CSN_REAL || frame->size == 0) {
        if (locked) csn_unlock(csound, reg);
        return csn_init_error(csound, "[csnarray] Invalid csn-audio frame: expected a non-empty 1-D real array");
    }

    p->fsize = frame->size;
    p->hsize = (size_t) hop_size_temp;
    /* remember the frame as it is now, so the first pass only adds a frame
       that has been written since */
    set_array_version(&p->prev_version, &frame->version);
    if (locked) csn_unlock(csound, reg);

    if (p->hsize > p->fsize) {
        return csn_init_error(csound, "[csnarray] Hop size %zu must be less or equal to the frame size %zu", p->hsize, p->fsize);
    }

    size_t buffer_cap = p->fsize * 2;
    double *buffer_temp = csound->Calloc(csound, sizeof(double) * buffer_cap);
    if (buffer_temp == NULL) {
        return csn_init_error(csound, "[csnarray] Internal error: memory allocation failed");
    }

    p->buffer.scratch = buffer_temp;
    p->buffer.scratch_capacity = buffer_cap;
    p->buffer.current_size = 0;
    p->buffer.reader = 0;
    p->buffer.writer = 0;
    p->buffer.sample_count = 0;

    p->phase = 0;
    *p->is_ready = FL(0.0);

    return OK;
}

int32_t csnarray_ola_audio(CSOUND *csound, CSN_OLA_AUDIO *p) {
    uint32_t nsamples = CS_KSMPS;
    uint32_t offset = p->h.insdshead->ksmps_offset;
    uint32_t early = p->h.insdshead->ksmps_no_end;
    uint32_t end_offset = nsamples - early;

    size_t fsize = p->fsize;
    size_t hsize = p->hsize;
    CSN_SCRATCH *buffer = &p->buffer;
    double *acc = (double *) buffer->scratch;
    size_t capacity = buffer->scratch_capacity;

    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);
    CSN_ARRAY *frame = p->source_handle;
    bool locked = csn_lock_if_shared(csound, reg, (CSN_CONARR){ frame }, 1);
    if (frame->ndim != 1U || frame->itype != CSN_REAL || frame->size != fsize) {
        if (locked) csn_unlock(csound, reg);
        return csn_perf_error(csound, &p->h, "[csnarray] Frame layout changed since init: expected a 1-D real array of %zu elements", fsize);
    }

    bool is_same = is_same_array_data_version(&frame->version, &p->prev_version);

    p->phase += nsamples;
    bool is_consumed = p->phase >= hsize;
    if (is_consumed) p->phase -= hsize;

    if (is_consumed && !is_same) {
        for (size_t i = 0; i < fsize; i++) {
            acc[(buffer->writer + i) % capacity] += frame->data[i];
        }
        buffer->writer = (buffer->writer + hsize) % capacity;
        buffer->current_size += hsize;

        set_array_version(&p->prev_version, &frame->version);
    }
    if (locked) csn_unlock(csound, reg);

    if (buffer->current_size >= (size_t) nsamples) {
        for (uint32_t j = 0; j < nsamples; j++) {
            size_t index = (buffer->reader + j) % capacity;
            p->sig[j] = (j < offset || j >= end_offset) ? FL(0.0) : (cs_float) acc[index];
            acc[index] = 0.0;
        }
        buffer->reader = (buffer->reader + nsamples) % capacity;
        buffer->current_size -= nsamples;
        *p->is_ready = FL(1.0);
    } else {
        memset(p->sig, 0, sizeof(cs_float) * nsamples);
        *p->is_ready = FL(0.0);
    }

    return OK;
}
