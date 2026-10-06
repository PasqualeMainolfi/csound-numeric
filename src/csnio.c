/* Opcode implementations for the io family.
   The public opcode inventory remains centralized in csnum.c. */
#include "csnum.h"
#include "csnum_internal.h"
#include "csnregistry.h"
#include "csnfile.h"
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
        return csound->InitError(csound, "[csnarray] File path cannot be empty");
    }

    const char *dot = strrchr(p->path->data, '.');
    if (dot == NULL || (strcmp(dot, CSN_FILE_EXT) != 0 && strcmp(dot, CSN_FILE_NPY_EXT) != 0)) {
        return csound->InitError(csound, "[csnarray] Invalid file extension: should be [%s] or [%s]", CSN_FILE_EXT, CSN_FILE_NPY_EXT);
    }

    int32_t res = OK;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (const CSN_ARRAY *[]) { p->source_handle }, 1);

    const char *path = p->path->data;
    CSN_FILE_ERROR_CODE err_code = csnfile_save_array(p->source_handle, path, dot);
    if (err_code != CSN_FILE_NO_ERROR) {
        const char *err_message = NULL;
        csnfile_dispatch_error(&err_message, err_code);
        res = csound->InitError(csound, "[csnarray] %s", err_message);
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
        return csound->InitError(csound, "[csnarray] File path cannot be empty");
    }

    const char *dot = strrchr(p->path->data, '.');
    if (dot == NULL || (strcmp(dot, CSN_FILE_EXT) != 0 && strcmp(dot, CSN_FILE_NPY_EXT) != 0)) {
        return csound->InitError(csound, "[csnarray] Invalid file extension: should be [%s] or [%s]", CSN_FILE_EXT, CSN_FILE_NPY_EXT);
    }

    int32_t res = OK;
    double *buffer = NULL;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (const CSN_ARRAY *[]) { p->handle }, 1);

    const char *path = p->path->data;

    buffer = csound->Calloc(csound, sizeof(double) * DEFAULT_TEMPORARY_BUFFER_SIZE);
    if (buffer == NULL) {
        res = csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
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
            res = csound->InitError(csound, "[csnarray] %s", err_message);
            goto done;
        };
    } else {
        is_numpy = true;
        CSN_FILE_ERROR_CODE err_code = csnfile_load_array_from_numpy_file(csound, &header.numpy_header, &buffer, &buffer_capacity, path);
        if (err_code != CSN_FILE_NO_ERROR) {
            const char *err_message = NULL;
            csnfile_dispatch_error(&err_message, err_code);
            res = csound->InitError(csound, "[csnarray] %s", err_message);
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
        return csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
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
        return csound->PerfError(csound, &p->h, "[csnarray] File path cannot be empty");
    }

    const char *dot = strrchr(p->path->data, '.');
    if (dot == NULL || (strcmp(dot, CSN_FILE_EXT) != 0 && strcmp(dot, CSN_FILE_NPY_EXT) != 0)) {
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid file extension: should be [%s] or [%s]", CSN_FILE_EXT, CSN_FILE_NPY_EXT);
    }

    int32_t res = OK;
    CHECK_KTRIG(p->trig);

    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (const CSN_ARRAY *[]) { p->source_handle }, 1);

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
        return csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
    }
    p->buffer_scratch.scratch = buffer;
    p->buffer_scratch.scratch_capacity = DEFAULT_TEMPORARY_BUFFER_SIZE;

    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (const CSN_ARRAY *[]) { p->handle }, 1);

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
        return csound->PerfError(csound, &p->h, "[csnarray] File path cannot be empty");
    }

    const char *dot = strrchr(p->path->data, '.');
    if (dot == NULL || (strcmp(dot, CSN_FILE_EXT) != 0 && strcmp(dot, CSN_FILE_NPY_EXT) != 0)) {
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid file extension: should be [%s] or [%s]", CSN_FILE_EXT, CSN_FILE_NPY_EXT);
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
            return csound->PerfError(csound, &p->h, "[csnarray] %s", err_message);
        }
    } else {
        is_numpy = true;
        CSN_FILE_ERROR_CODE err_code = csnfile_load_array_from_numpy_file(csound, &header.numpy_header, &buffer, &capacity, path);
        p->buffer_scratch.scratch = buffer;
        p->buffer_scratch.scratch_capacity = capacity;
        if (err_code != CSN_FILE_NO_ERROR) {
            const char *err_message = NULL;
            csnfile_dispatch_error(&err_message, err_code);
            return csound->PerfError(csound, &p->h, "[csnarray] %s", err_message);
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
    pres.locked = csn_lock_if_shared(csound, reg, (const CSN_ARRAY *[]) { p->handle }, 1);

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
        return csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
    }
    pbuffer.data = data;
    pbuffer.length = 0;
    pbuffer.capacity = CSN_PRINT_BUFFER_INITIAL_CAPACITY;

    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (const CSN_ARRAY *[]) { p->source_handle }, 1);
    CSN_ARRAY *arr = p->source_handle;
    res = csnfile_show_array(csound, &pbuffer, arr);
    if (res == OK) csound->Message(csound, "%s", pbuffer.data);
    csound->Free(csound, pbuffer.data);

    if (res != OK) {
        res = csound->InitError(csound, "[csnarray] Internal error: wrong print buffer allocation");
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
        return csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
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
    pres.locked = csn_lock_if_shared(csound, reg, (const CSN_ARRAY *[]) { p->source_handle }, 1);

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
