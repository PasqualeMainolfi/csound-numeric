/* Opcode implementations for the io family.
   The public opcode inventory remains centralized in csnum.c. */
#include "csnum.h"
#include "csnum_internal.h"
#include "csnregistry.h"
#include "csnfile.h"
#include <ctype.h>
#include <float.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int32_t csnarray_save(CSOUND *csound, CSN_SAVE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    if (p->path == NULL || p->path->data == NULL || p->path->size <= 0 || p->path->data[0] == '\0') {
        return csn_init_error(csound, "[csnarray] File path cannot be empty");
    }

    const char *dot = strrchr(p->path->data, '.');
    if (dot == NULL || (strcmp(dot, CSN_FILE_EXT) != 0 && strcmp(dot, CSN_FILE_NPY_EXT) != 0)) {
        return csn_init_error(csound, "[csnarray] Invalid file extension: should be [%s] or [%s]", CSN_FILE_EXT, CSN_FILE_NPY_EXT);
    }

    int32_t res = OK;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);

    const char *path = p->path->data;
    CSN_FILE_ERROR_CODE err_code = csnfile_save_array(p->source_handle, path, dot);
    if (err_code != CSN_FILE_NO_ERROR) {
        const char *err_message = NULL;
        csnfile_dispatch_error(&err_message, err_code);
        res = csn_init_error(csound, "[csnarray] %s", err_message);
    }

    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

typedef union {
    CSN_FILE_HEADER csn_header;
    CSN_FILE_NUMPY_HEADER numpy_header;
} CSNFH;

int32_t csnarray_load(CSOUND *csound, CSN_LOAD *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    if (p->path == NULL || p->path->data == NULL || p->path->size <= 0 || p->path->data[0] == '\0') {
        return csn_init_error(csound, "[csnarray] File path cannot be empty");
    }

    const char *dot = strrchr(p->path->data, '.');
    if (dot == NULL || (strcmp(dot, CSN_FILE_EXT) != 0 && strcmp(dot, CSN_FILE_NPY_EXT) != 0)) {
        return csn_init_error(csound, "[csnarray] Invalid file extension: should be [%s] or [%s]", CSN_FILE_EXT, CSN_FILE_NPY_EXT);
    }

    int32_t res = OK;
    double *buffer = NULL;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle }, 1);

    const char *path = p->path->data;

    buffer = csound->Calloc(csound, sizeof(double) * DEFAULT_TEMPORARY_BUFFER_SIZE);
    if (buffer == NULL) {
        res = csn_init_error(csound, "[csnarray] Internal error: memory allocation failed");
        goto done;
    }
    size_t buffer_capacity = DEFAULT_TEMPORARY_BUFFER_SIZE;

    bool is_numpy = false;
    CSNFH header = {0};
    if (strcmp(dot, CSN_FILE_EXT) == 0) {
        CSN_FILE_ERROR_CODE err_code = csnfile_load_array_from_file(csound, &header.csn_header, &buffer, &buffer_capacity, path);
        if (err_code != CSN_FILE_NO_ERROR) {
            const char *err_message = NULL;
            csnfile_dispatch_error(&err_message, err_code);
            res = csn_init_error(csound, "[csnarray] %s", err_message);
            goto done;
        };
    } else {
        is_numpy = true;
        CSN_FILE_ERROR_CODE err_code = csnfile_load_array_from_numpy_file(csound, &header.numpy_header, &buffer, &buffer_capacity, path);
        if (err_code != CSN_FILE_NO_ERROR) {
            const char *err_message = NULL;
            csnfile_dispatch_error(&err_message, err_code);
            res = csn_init_error(csound, "[csnarray] %s", err_message);
            goto done;
        };
    }

    uint32_t ndim = 0;
    uint32_t shape[CSN_MAX_DIMS] = {0};
    ITEM_TYPE itype = CSN_REAL;
    size_t size = 0;
    if (!is_numpy) {
        ndim = header.csn_header.dim;
        memcpy(shape, header.csn_header.shape, sizeof(shape));
        itype = (ITEM_TYPE) header.csn_header.dtype;
        size = header.csn_header.size;
    } else {
        ndim = header.numpy_header.dim;
        memcpy(shape, header.numpy_header.shape, sizeof(shape));
        itype = (ITEM_TYPE) header.numpy_header.descr.itype;
        size = header.numpy_header.size;
    }

    CSNARR_RES out = prepare_output(csound, &p->h, NULL, reg, p->handle, ndim, shape, itype, NULL, 0, CSN_ALIAS_REFUSE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    CSN_ARRAY *arr = p->handle;
    if (size > 0) {
        memcpy(arr->data, buffer, size * itype * sizeof(double));
    }

done:
    if (buffer != NULL) csound->Free(csound, buffer);
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_save_k_deinit(CSOUND *csound, CSN_SAVE *p) {
    deinit_scratch(csound, &p->scratch);
    return OK;
}

int32_t csnarray_save_k_init(CSOUND *csound, CSN_SAVE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    stamp_globals(csound, &p->h);

    char *path_buffer = csound->Malloc(csound, DEFAULT_TEMPORARY_BUFFER_SIZE);
    if (path_buffer == NULL) {
        return csn_init_error(csound, "[csnarray] Internal error: memory allocation failed");
    }
    p->scratch.scratch = path_buffer;
    p->scratch.scratch_capacity = DEFAULT_TEMPORARY_BUFFER_SIZE;
    p->registry = reg;
    p->is_published = false;
    return OK;
}

int32_t csnarray_save_k(CSOUND *csound, CSN_SAVE *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    if (p->path == NULL || p->path->data == NULL || p->path->size <= 0 || p->path->data[0] == '\0') {
        return csn_perf_error(csound, &p->h, "[csnarray] File path cannot be empty");
    }

    const char *dot = strrchr(p->path->data, '.');
    if (dot == NULL || (strcmp(dot, CSN_FILE_EXT) != 0 && strcmp(dot, CSN_FILE_NPY_EXT) != 0)) {
        return csn_perf_error(csound, &p->h, "[csnarray] Invalid file extension: should be [%s] or [%s]", CSN_FILE_EXT, CSN_FILE_NPY_EXT);
    }

    int32_t res = OK;
    CHECK_KTRIG(p->trig);

    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);

    const char *path = p->path->data;
    CSN_ARRAY *arr = p->source_handle;

    if (p->is_published) {
        bool is_same_array = is_same_array_version(&arr->version, &p->prev_source_version);
        bool is_same_path = strcmp(path, p->scratch.scratch) == 0;
        if (is_same_array && is_same_path) goto done;
    }

    CSN_FILE_ERROR_CODE err_code = csnfile_save_array(arr, path, dot);
    if (err_code != CSN_FILE_NO_ERROR) {
        const char *err_message = NULL;
        csnfile_dispatch_error(&err_message, err_code);
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] %s", err_message);
        goto done;
    }

    size_t path_len = strlen(path);
    res = csn_scratch_reserve(csound, &p->h, arr->rt_locked, &p->scratch, path_len + 1, sizeof(char));
    if (res != OK) goto done;

    snprintf((char *) p->scratch.scratch, path_len + 1, "%s", path);
    set_array_version(&p->prev_source_version, &arr->version);
    p->is_published = true;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_load_k_init(CSOUND *csound, CSN_LOAD *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;

    double *buffer = csound->Calloc(csound, sizeof(double) * DEFAULT_TEMPORARY_BUFFER_SIZE);
    if (buffer == NULL) {
        return csn_init_error(csound, "[csnarray] Internal error: memory allocation failed");
    }
    p->buffer_scratch.scratch = buffer;
    p->buffer_scratch.scratch_capacity = DEFAULT_TEMPORARY_BUFFER_SIZE;

    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle }, 1);

    uint32_t shape[CSN_MAX_DIMS] = {0};
    shape[0] = 1U;

    CSNARR_RES out = prepare_output(csound, &p->h, NULL, reg, p->handle, 1U, shape, CSN_REAL, NULL, 0, CSN_ALIAS_REFUSE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    reset_empty_csnarray(p->handle, 1U, shape, CSN_REAL);
    p->registry = reg;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_load_k(CSOUND *csound, CSN_LOAD *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    if (p->path == NULL || p->path->data == NULL || p->path->size <= 0 || p->path->data[0] == '\0') {
        return csn_perf_error(csound, &p->h, "[csnarray] File path cannot be empty");
    }

    const char *dot = strrchr(p->path->data, '.');
    if (dot == NULL || (strcmp(dot, CSN_FILE_EXT) != 0 && strcmp(dot, CSN_FILE_NPY_EXT) != 0)) {
        return csn_perf_error(csound, &p->h, "[csnarray] Invalid file extension: should be [%s] or [%s]", CSN_FILE_EXT, CSN_FILE_NPY_EXT);
    }

    int32_t res = OK;

    /* No reload cache on purpose. csnload reads a file it does not own, so the
       path is not a usable key, and a stat stamp only narrows the window: on
       HFS+, SMB/NFS and FAT the mtime granularity is one to two seconds, wide
       enough for a same-size rewrite to hide in. The trigger is the contract
       and the only authority: it fires, we read. */
    CHECK_KTRIG(p->trig);
    const char *path = p->path->data;

    double *buffer = (double *) p->buffer_scratch.scratch;
    size_t capacity = p->buffer_scratch.scratch_capacity;

    CSNFH header = {0};
    bool is_numpy = false;
    if (strcmp(dot, CSN_FILE_EXT) == 0) {
        CSN_FILE_ERROR_CODE err_code = csnfile_load_array_from_file(csound, &header.csn_header, &buffer, &capacity, path);
        p->buffer_scratch.scratch = buffer;
        p->buffer_scratch.scratch_capacity = capacity;
        if (err_code != CSN_FILE_NO_ERROR) {
            const char *err_message = NULL;
            csnfile_dispatch_error(&err_message, err_code);
            return csn_perf_error(csound, &p->h, "[csnarray] %s", err_message);
        }
    } else {
        is_numpy = true;
        CSN_FILE_ERROR_CODE err_code = csnfile_load_array_from_numpy_file(csound, &header.numpy_header, &buffer, &capacity, path);
        p->buffer_scratch.scratch = buffer;
        p->buffer_scratch.scratch_capacity = capacity;
        if (err_code != CSN_FILE_NO_ERROR) {
            const char *err_message = NULL;
            csnfile_dispatch_error(&err_message, err_code);
            return csn_perf_error(csound, &p->h, "[csnarray] %s", err_message);
        }
    }

    uint32_t ndim = 0;
    uint32_t shape[CSN_MAX_DIMS] = {0};
    ITEM_TYPE itype = CSN_REAL;
    size_t size = 0;
    if (!is_numpy) {
        ndim = header.csn_header.dim;
        memcpy(shape, header.csn_header.shape, sizeof(shape));
        itype = (ITEM_TYPE) header.csn_header.dtype;
        size = header.csn_header.size;
    } else {
        ndim = header.numpy_header.dim;
        memcpy(shape, header.numpy_header.shape, sizeof(shape));
        itype = (ITEM_TYPE) header.numpy_header.descr.itype;
        size = header.numpy_header.size;
    }

    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle }, 1);

    /* prepare_output refuses a shape over the element limit, and growth on a
       real-time path */
    CSNARR_RES out = prepare_output(csound, &p->h, &p->h, reg, p->handle, ndim, shape, itype, NULL, 0, CSN_ALIAS_REFUSE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    if (size > 0) {
        memcpy(p->handle->data, buffer, size * itype * sizeof(double));
    }

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_show(CSOUND *csound, CSN_SHOW *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    stamp_globals(csound, &p->h);

    CSN_PRINT_BUFFER pbuffer = {0};
    char *data = csound->Malloc(csound, CSN_PRINT_BUFFER_INITIAL_CAPACITY);
    if (data == NULL) {
        return csn_init_error(csound, "[csnarray] Internal error: memory allocation failed");
    }
    pbuffer.data = data;
    pbuffer.length = 0;
    pbuffer.capacity = CSN_PRINT_BUFFER_INITIAL_CAPACITY;

    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);
    CSN_ARRAY *arr = p->source_handle;
    res = csnfile_show_array(csound, &pbuffer, arr);
    if (res == OK) csound->Message(csound, "%s", pbuffer.data);
    csound->Free(csound, pbuffer.data);

    if (res != OK) {
        res = csn_init_error(csound, "[csnarray] Internal error: wrong print buffer allocation");
    }

    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnarray_show_k_deinit(CSOUND *csound, CSN_SHOW *p) {
    if (p->pbuffer.data != NULL) {
        csound->Free(csound, p->pbuffer.data);
        p->pbuffer.data = NULL;
        p->pbuffer.length = 0;
        p->pbuffer.capacity = 0;
    }

    return OK;
}

int32_t csnarray_show_k_init(CSOUND *csound, CSN_SHOW *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    stamp_globals(csound, &p->h);

    p->pbuffer.data = NULL;
    p->pbuffer.length = 0;
    p->pbuffer.capacity = 0;

    char *data = csound->Malloc(csound, CSN_PRINT_BUFFER_INITIAL_CAPACITY);
    if (data == NULL) {
        return csn_init_error(csound, "[csnarray] Internal error: memory allocation failed");
    }

    p->pbuffer.data = data;
    p->pbuffer.length = 0;
    p->pbuffer.capacity = CSN_PRINT_BUFFER_INITIAL_CAPACITY;
    p->registry = reg;

    return OK;
}

int32_t csnarray_show_k(CSOUND *csound, CSN_SHOW *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    CHECK_KTRIG(p->trig);

    int32_t res = OK;
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);

    CSN_ARRAY *arr = p->source_handle;
    if (csnfile_show_array(csound, &p->pbuffer, arr) != OK) {
        res = csn_locked_perf_error(csound, &p->h, "[csnarray] Internal error: wrong print buffer allocation");
        goto done;
    }
    csound->Message(csound, "%s", p->pbuffer.data);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

// AUDIO FILES

/* declarations only: the implementations are compiled in dr_libs.c */
#include "dr_wav.h"
#include "dr_mp3.h"
#include "dr_flac.h"

/* An audio file decoded whole: interleaved float frames, as dr_libs
   returns them, released with free_decoded_audio. */
typedef struct {
    float *frames;
    uint32_t channels;
    uint32_t sample_rate;
    uint64_t frame_count;
    CSN_AUDIO_FILE_TYPE type;
} CSN_DECODED_AUDIO;

static bool has_extension(const char *dot, const char *ext) {
    for (; *dot != '\0' && *ext != '\0'; dot++, ext++) {
        if (tolower((unsigned char) *dot) != *ext) return false;
    }
    return *dot == '\0' && *ext == '\0';
}

/* The path of an audio file argument and its type, read from the extension
   whatever its case. NULL when the path is empty or the extension is none
   of .wav, .mp3 and .flac. */
static const char *audio_file_path(const STRINGDAT *path, CSN_AUDIO_FILE_TYPE *type) {
    if (path == NULL || path->data == NULL || path->size <= 0 || path->data[0] == '\0') return NULL;
    const char *dot = strrchr(path->data, '.');
    if (dot == NULL || dot == path->data) return NULL;
    if (has_extension(dot, CSN_WAV_EXT)) *type = CSN_WAV;
    else if (has_extension(dot, CSN_MP3_EXT)) *type = CSN_MP3;
    else if (has_extension(dot, CSN_FLAC_EXT)) *type = CSN_FLAC;
    else return NULL;
    return path->data;
}

static bool decode_audio_file(const char *path, CSN_AUDIO_FILE_TYPE type, CSN_DECODED_AUDIO *audio) {
    unsigned int channels = 0;
    unsigned int sample_rate = 0;
    memset(audio, 0, sizeof(*audio));
    audio->type = type;

    switch (type) {
        case CSN_WAV: {
            drwav_uint64 count = 0;
            audio->frames = drwav_open_file_and_read_pcm_frames_f32(path, &channels, &sample_rate, &count, NULL);
            audio->frame_count = (uint64_t) count;
            break;
        }
        case CSN_MP3: {
            drmp3_config config = {0};
            drmp3_uint64 count = 0;
            audio->frames = drmp3_open_file_and_read_pcm_frames_f32(path, &config, &count, NULL);
            channels = config.channels;
            sample_rate = config.sampleRate;
            audio->frame_count = (uint64_t) count;
            break;
        }
        case CSN_FLAC: {
            drflac_uint64 count = 0;
            audio->frames = drflac_open_file_and_read_pcm_frames_f32(path, &channels, &sample_rate, &count, NULL);
            audio->frame_count = (uint64_t) count;
            break;
        }
    }

    audio->channels = (uint32_t) channels;
    audio->sample_rate = (uint32_t) sample_rate;
    return audio->frames != NULL && audio->channels > 0 && audio->sample_rate > 0;
}

static void free_decoded_audio(CSN_DECODED_AUDIO *audio) {
    if (audio->frames == NULL) return;
    switch (audio->type) {
        case CSN_WAV: drwav_free(audio->frames, NULL); break;
        case CSN_MP3: drmp3_free(audio->frames, NULL); break;
        case CSN_FLAC: drflac_free(audio->frames, NULL); break;
    }
    audio->frames = NULL;
}

/* Decodes the file at path into out, one row per channel: a 1-D array of
   the samples for a single channel, or for every channel averaged when mono
   is set, else a 2-D array of shape (channels, frames). fs receives the
   file's sample rate. */
static int32_t load_audio_file(CSOUND *csound, OPDS *h, OPDS *perf_h, CSN_REGISTRY *reg, CSN_ARRAY *out, const STRINGDAT *path_arg, bool mono, cs_float *fs) {
    CSN_AUDIO_FILE_TYPE type = CSN_WAV;
    const char *path = audio_file_path(path_arg, &type);
    if (path == NULL) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] Invalid audio file path: it must end in %s, %s or %s", CSN_WAV_EXT, CSN_MP3_EXT, CSN_FLAC_EXT);
    }

    CSN_DECODED_AUDIO audio;
    if (!decode_audio_file(path, type, &audio)) {
        free_decoded_audio(&audio);
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] Cannot read audio file '%s'", path);
    }

    if (audio.frame_count > (uint64_t) UINT32_MAX) {
        free_decoded_audio(&audio);
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] Audio file '%s' holds more frames than an array axis can", path);
    }

    size_t frames = (size_t) audio.frame_count;
    size_t channels = (size_t) audio.channels;
    bool single = mono || channels == 1;

    uint32_t ndim = single ? 1U : 2U;
    uint32_t shape[CSN_MAX_DIMS] = {0};
    if (single) {
        shape[0] = (uint32_t) frames;
    } else {
        shape[0] = (uint32_t) channels;
        shape[1] = (uint32_t) frames;
    }

    int32_t res = OK;
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { out }, 1);

    CSNARR_RES prepared = prepare_output(csound, h, perf_h, reg, out, ndim, shape, CSN_REAL, NULL, 0, CSN_ALIAS_REFUSE);
    res = prepared.err;
    if (res != OK) goto done;
    pres.locked |= prepared.locked;

    if (single) {
        for (size_t j = 0; j < frames; j++) {
            double sum = 0.0;
            for (size_t c = 0; c < channels; c++) sum += (double) audio.frames[j * channels + c];
            out->data[j] = sum / (double) channels;
        }
    } else {
        for (size_t c = 0; c < channels; c++) {
            for (size_t j = 0; j < frames; j++) {
                out->data[c * frames + j] = (double) audio.frames[j * channels + c];
            }
        }
    }
    *fs = (cs_float) audio.sample_rate;

done:
    if (pres.locked) csn_unlock(csound, reg);
    free_decoded_audio(&audio);
    return res;
}

/* Writes source to path as a 32-bit float .wav file: a 1-D array is one
   channel, a 2-D array of shape (channels, frames) one channel per row,
   averaged to a single one when mono is set. scratch holds the interleaved
   frames. */
static int32_t save_audio_file(CSOUND *csound, OPDS *perf_h, CSN_REGISTRY *reg, CSN_ARRAY *source, const STRINGDAT *path_arg, uint32_t sample_rate, bool mono, CSN_SCRATCH *scratch) {
    CSN_AUDIO_FILE_TYPE type = CSN_WAV;
    const char *path = audio_file_path(path_arg, &type);
    if (path == NULL || type != CSN_WAV) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] Invalid audio file path: only %s files can be written", CSN_WAV_EXT);
    }

    int32_t res = OK;
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { source }, 1);

    if (source->itype != CSN_REAL) {
        res = CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] An audio file is written from a real array");
        goto done;
    }

    if (source->ndim != 1U && source->ndim != 2U) {
        res = CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] An audio file is written from a 1-D array or a 2-D array of shape (channels, frames), not a %u-D one", source->ndim);
        goto done;
    }

    size_t channels = source->ndim == 1U ? 1U : (size_t) source->shape[0];
    size_t frames = source->ndim == 1U ? (size_t) source->shape[0] : (size_t) source->shape[1];
    size_t out_channels = mono ? 1U : channels;
    if (channels == 0 || out_channels > UINT16_MAX) {
        res = CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] An audio file holds between 1 and %u channels, not %zu", (unsigned) UINT16_MAX, channels);
        goto done;
    }

    res = csn_scratch_reserve(csound, perf_h, source->rt_locked, scratch, out_channels * frames, sizeof(float));
    if (res != OK) goto done;

    float *interleaved = (float *) scratch->scratch;
    const double *data = source->data;
    if (mono) {
        for (size_t j = 0; j < frames; j++) {
            double sum = 0.0;
            for (size_t c = 0; c < channels; c++) sum += data[c * frames + j];
            interleaved[j] = (float) (sum / (double) channels);
        }
    } else {
        for (size_t c = 0; c < channels; c++) {
            for (size_t j = 0; j < frames; j++) {
                interleaved[j * channels + c] = (float) data[c * frames + j];
            }
        }
    }

    /* the frames are copied: the file is written without holding the lock */
    if (pres.locked) {
        csn_unlock(csound, reg);
        pres.locked = false;
    }

    drwav_data_format format = {0};
    format.container = drwav_container_riff;
    format.format = DR_WAVE_FORMAT_IEEE_FLOAT;
    format.channels = (drwav_uint32) out_channels;
    format.sampleRate = (drwav_uint32) sample_rate;
    format.bitsPerSample = 32;

    drwav wav;
    if (!drwav_init_file_write(&wav, path, &format, NULL)) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] Cannot open '%s' for writing", path);
    }
    drwav_uint64 written = drwav_write_pcm_frames(&wav, (drwav_uint64) frames, interleaved);
    drwav_result closed = drwav_uninit(&wav);
    if (written != (drwav_uint64) frames || closed != DRWAV_SUCCESS) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] Cannot write audio file '%s'", path);
    }

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnfile_load_audiofile(CSOUND *csound, CSN_LOAD_AUDIO_FILE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    if (!IS_VALID_ZERO_ONE((double) *p->mono)) {
        return csn_init_error(csound, "[csnarray] mono must be 0 = keep the channels or 1 = average them");
    }

    stamp_globals(csound, &p->h);
    return load_audio_file(csound, &p->h, NULL, reg, p->handle, p->file_path, *p->mono == FL(1.0), p->fs);
}

int32_t csnfile_load_audiofile_k_init(CSOUND *csound, CSN_LOAD_AUDIO_FILE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    if (!IS_VALID_ZERO_ONE((double) *p->mono)) {
        return csn_init_error(csound, "[csnarray] mono must be 0 = keep the channels or 1 = average them");
    }

    int32_t res = OK;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle }, 1);

    /* as csnload at k-rate: empty until the trigger first fires */
    uint32_t shape[CSN_MAX_DIMS] = {0};
    shape[0] = 1U;
    CSNARR_RES out = prepare_output(csound, &p->h, NULL, reg, p->handle, 1U, shape, CSN_REAL, NULL, 0, CSN_ALIAS_REFUSE);
    res = out.err;
    if (res != OK) goto done;
    pres.locked |= out.locked;

    reset_empty_csnarray(p->handle, 1U, shape, CSN_REAL);
    *p->fs = FL(0.0);
    p->is_mono = *p->mono == FL(1.0);
    p->registry = reg;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnfile_load_audiofile_k(CSOUND *csound, CSN_LOAD_AUDIO_FILE *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    /* No reload cache, as for csnload: the trigger fires, the file is read. */
    CHECK_KTRIG(p->trig);
    return load_audio_file(csound, &p->h, &p->h, reg, p->handle, p->file_path, p->is_mono, p->fs);
}

int32_t csnfile_save_audiofile_deinit(CSOUND *csound, CSN_SAVE_AUDIO_FILE *p) {
    deinit_scratch(csound, &p->buffer);
    return OK;
}

static int32_t check_save_audio_args(CSOUND *csound, CSN_SAVE_AUDIO_FILE *p) {
    if (!IS_VALID_ZERO_ONE((double) *p->mono)) {
        return csn_init_error(csound, "[csnarray] mono must be 0 = keep the channels or 1 = average them");
    }

    double fs = (double) *p->fs;
    if (!IS_VALID_SR(fs)) {
        return csn_init_error(csound, "[csnarray] Invalid sample rate %g: it must be a whole number of Hz greater than 0", fs);
    }

    p->sample_rate = (uint32_t) fs;
    p->is_mono = *p->mono == FL(1.0);
    return OK;
}

int32_t csnfile_save_audiofile(CSOUND *csound, CSN_SAVE_AUDIO_FILE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = check_save_audio_args(csound, p);
    if (res != OK) return res;

    stamp_globals(csound, &p->h);
    return save_audio_file(csound, NULL, reg, p->source_handle, p->file_path, p->sample_rate, p->is_mono, &p->buffer);
}

int32_t csnfile_save_audiofile_k_init(CSOUND *csound, CSN_SAVE_AUDIO_FILE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = check_save_audio_args(csound, p);
    if (res != OK) return res;

    stamp_globals(csound, &p->h);
    p->registry = reg;
    return OK;
}

int32_t csnfile_save_audiofile_k(CSOUND *csound, CSN_SAVE_AUDIO_FILE *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    CHECK_KTRIG(p->trig);
    return save_audio_file(csound, &p->h, reg, p->source_handle, p->file_path, p->sample_rate, p->is_mono, &p->buffer);
}
