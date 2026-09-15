/* Opcode implementations for the io family.
   The public opcode inventory remains centralized in csnum.c. */
#include "csnum_internal.h"
#include "csnregistry.h"
#include "csnfile.h"
#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "arrays.h"

int32_t csnarray_save(CSOUND *csound, CSN_SAVE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = (uint32_t) p->source_handle->id;

    if (p->path == NULL || p->path->data == NULL || p->path->size <= 0 || p->path->data[0] == '\0') {
        return csound->InitError(csound, "[csnarray] File path cannot be empty");
    }

    const char *dot = strrchr(p->path->data, '.');
    if (dot == NULL || strcmp(dot, CSN_FILE_EXT) != 0) {
        return csound->InitError(csound, "[csnarray] Invalid file extension: should be [%s]", CSN_FILE_EXT);
    }

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        return csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
    }
    CSN_ARRAY *arr = slot->array;

    const char *path = p->path->data;
    CSN_FILE_ERROR_CODE err_code = csnfile_save_array_to_file(arr, path);
    if (err_code != CSN_FILE_NO_ERROR) {
        const char *err_message = NULL;
        csnfile_dispatch_error(&err_message, err_code);
        return csound->InitError(csound, "[csnarray] %s", err_message);
    }

    return OK;
}

int32_t csnarray_load(CSOUND *csound, CSN_LOAD *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    if (p->path == NULL || p->path->data == NULL || p->path->size <= 0 || p->path->data[0] == '\0') {
        return csound->InitError(csound, "[csnarray] File path cannot be empty");
    }

    const char *dot = strrchr(p->path->data, '.');
    if (dot == NULL || strcmp(dot, CSN_FILE_EXT) != 0) {
        return csound->InitError(csound, "[csnarray] Invalid file extension: should be [%s]", CSN_FILE_EXT);
    }

    int32_t res = OK;
    const char *err = NULL;
    const char *path = p->path->data;

    double *buffer = csound->Calloc(csound, sizeof(double) * DEFAULT_TEMPORARY_BUFFER_SIZE);
    if (buffer == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
    }
    size_t buffer_capacity = DEFAULT_TEMPORARY_BUFFER_SIZE;

    CSN_FILE_HEADER header = {0};
    CSN_FILE_ERROR_CODE err_code = csnfile_load_array_from_file(csound, &header, &buffer, &buffer_capacity, path);
    if (err_code != CSN_FILE_NO_ERROR) {
        csound->Free(csound, buffer);
        const char *err_message = NULL;
        csnfile_dispatch_error(&err_message, err_code);
        return csound->InitError(csound, "[csnarray] %s", err_message);
    };

    csound->LockMutex(reg->mutex);
    if (create_csnarray_locked(csound, reg, &p->h, header.dim, header.shape, &p->array, p->handle, NULL, 0, &err, (ITEM_TYPE) header.dtype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;
    if (header.size > 0) {
        memcpy(arr->data, buffer, (size_t) header.data_bytes);
        update_array_data_version(&arr->version);
    }

done:
    csound->Free(csound, buffer);
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_save_k_deinit(CSOUND *csound, CSN_SAVE *p) {
    deinit_scratch(csound, &p->scratch);
    return OK;
}

int32_t csnarray_save_k_init(CSOUND *csound, CSN_SAVE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = (uint32_t) p->source_handle->id;
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        return csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
    }
    CSN_ARRAY *arr = slot->array;

    char *path_buffer = csound->Malloc(csound, DEFAULT_TEMPORARY_BUFFER_SIZE);
    if (path_buffer == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
    }
    p->scratch.scratch = path_buffer;
    p->scratch.scratch_capacity = DEFAULT_TEMPORARY_BUFFER_SIZE;
    p->registry = reg;
    p->prev_source_version = arr->version;
    p->prev_array_id = 0;
    p->is_published = false;
    return OK;
}

int32_t csnarray_save_k(CSOUND *csound, CSN_SAVE *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    uint32_t source_handle = (uint32_t) p->source_handle->id;

    if (p->path == NULL || p->path->data == NULL || p->path->size <= 0 || p->path->data[0] == '\0') {
        return csound->PerfError(csound, &p->h, "[csnarray] File path cannot be empty");
    }

    const char *dot = strrchr(p->path->data, '.');
    if (dot == NULL || strcmp(dot, CSN_FILE_EXT) != 0) {
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid file extension: should be [%s]", CSN_FILE_EXT);
    }

    CHECK_KTRIG(p->trig);

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
    }

    const char *path = p->path->data;
    CSN_ARRAY *arr = slot->array;

    if (p->is_published) {
        bool is_same_array = is_same_array_version(&arr->version, &p->prev_source_version);
        bool is_same_path = strcmp(path, p->scratch.scratch) == 0;
        bool is_same_array_id = source_handle == p->prev_array_id;
        if (is_same_array && is_same_path && is_same_array_id) return OK;
    }

    CSN_FILE_ERROR_CODE err_code = csnfile_save_array_to_file(arr, path);
    if (err_code != CSN_FILE_NO_ERROR) {
        const char *err_message = NULL;
        csnfile_dispatch_error(&err_message, err_code);
        return csound->PerfError(csound, &p->h, "[csnarray] %s", err_message);
    }

    size_t path_len = strlen(path);
    if (path_len + 1 > p->scratch.scratch_capacity) {
        size_t new_path_cap = (path_len + 1) * 2;
        char *path_buffer = csound->ReAlloc(csound, p->scratch.scratch, sizeof(char) * new_path_cap);
        if (path_buffer == NULL) {
            return csound->PerfError(csound, &p->h, "[csnarray] Internal error: memory allocation failed");
        }
        p->scratch.scratch = path_buffer;
        p->scratch.scratch_capacity = new_path_cap;
    }

    snprintf((char *) p->scratch.scratch, path_len + 1, "%s", path);
    p->prev_array_id = source_handle;
    set_array_version(&p->prev_source_version, &arr->version);
    p->is_published = true;

    return OK;
}

int32_t csnarray_load_k_init(CSOUND *csound, CSN_LOAD *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    const char *err = NULL;

    double *buffer = csound->Calloc(csound, sizeof(double) * DEFAULT_TEMPORARY_BUFFER_SIZE);
    if (buffer == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
    }
    p->buffer_scratch.scratch = buffer;
    p->buffer_scratch.scratch_capacity = DEFAULT_TEMPORARY_BUFFER_SIZE;

    char *path_buffer = csound->Malloc(csound, DEFAULT_TEMPORARY_BUFFER_SIZE);
    if (path_buffer == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
    }
    p->path_scratch.scratch = path_buffer;
    p->path_scratch.scratch_capacity = DEFAULT_TEMPORARY_BUFFER_SIZE;

    csound->LockMutex(reg->mutex);

    uint32_t shape[CSN_MAX_DIMS] = {0};
    shape[0] = 1U;

    if (create_csnarray_locked(csound, reg, &p->h, 1U, shape, &p->array, p->handle, NULL, 0, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    reset_empty_csnarray(p->array, 1U, shape, CSN_REAL);
    SET_KDATA_BEGIN(p, reg);
    p->is_published = false;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_load_k(CSOUND *csound, CSN_LOAD *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REG_HANDLE(csound, &p->h, reg, p->k_data.owned_handle);

    if (p->path == NULL || p->path->data == NULL || p->path->size <= 0 || p->path->data[0] == '\0') {
        return csound->PerfError(csound, &p->h, "[csnarray] File path cannot be empty");
    }

    const char *dot = strrchr(p->path->data, '.');
    if (dot == NULL || strcmp(dot, CSN_FILE_EXT) != 0) {
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid file extension: should be [%s]", CSN_FILE_EXT);
    }

    int32_t res = OK;
    const char *err = NULL;
    const char *path = p->path->data;

    /* No reload cache on purpose. csnload reads a file it does not own, so the
       path is not a usable key, and a stat stamp only narrows the window: on
       HFS+, SMB/NFS and FAT the mtime granularity is one to two seconds, wide
       enough for a same-size rewrite to hide in. The trigger is the contract
       and the only authority: it fires, we read. */
    CHECK_KTRIG(p->trig);

    double *buffer = (double *) p->buffer_scratch.scratch;
    size_t capacity = p->buffer_scratch.scratch_capacity;

    CSN_FILE_HEADER header = {0};
    CSN_FILE_ERROR_CODE err_code = csnfile_load_array_from_file(csound, &header, &buffer, &capacity, path);
    p->buffer_scratch.scratch = buffer;
    p->buffer_scratch.scratch_capacity = capacity;
    if (err_code != CSN_FILE_NO_ERROR) {
        const char *err_message = NULL;
        csnfile_dispatch_error(&err_message, err_code);
        return csound->PerfError(csound, &p->h, "[csnarray] %s", err_message);
    }

    csound->LockMutex(reg->mutex);
    size_t req_size = 0;
    if (get_array_size_from_shape(&req_size, header.dim, header.shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }
    ITEM_TYPE itype = (ITEM_TYPE) header.dtype;
    CSN_ARRAY *arr = NULL;
    size_t logical_size = req_size;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, header.dim, header.shape, logical_size, itype, err);
    if (res != OK) goto done;
    p->array = arr;

    if (header.size > 0) {
        memcpy(arr->data, buffer, (size_t) header.data_bytes);
        update_array_data_version(&arr->version);
    }

    size_t path_len = strlen(path);
    if (path_len + 1 > p->path_scratch.scratch_capacity) {
        size_t new_path_cap = (path_len + 1) * 2;
        char *path_buffer = csound->ReAlloc(csound, p->path_scratch.scratch, sizeof(char) * new_path_cap);
        if (path_buffer == NULL) {
            csound->UnlockMutex(reg->mutex);
            return csound->PerfError(csound, &p->h, "[csnarray] Internal error: memory allocation failed");
        }
        p->path_scratch.scratch = path_buffer;
        p->path_scratch.scratch_capacity = new_path_cap;
    }
    snprintf((char *) p->path_scratch.scratch, path_len + 1, "%s", path);

    SET_KDATA_END(p, arr->shape, arr->ndim, arr->itype);
    p->is_published = true;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_show(CSOUND *csound, CSN_SHOW *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = (uint32_t) p->source_handle->id;
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        return csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
    }
    CSN_ARRAY *arr = slot->array;
    CSN_PRINT_BUFFER pbuffer = {0};
    char *data = csound->Malloc(csound, CSN_PRINT_BUFFER_INITIAL_CAPACITY);
    if (data == NULL) {
        return csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
    }
    pbuffer.data = data;
    pbuffer.length = 0;
    pbuffer.capacity = CSN_PRINT_BUFFER_INITIAL_CAPACITY;

    int32_t res = csnfile_show_array(csound, &pbuffer, arr);
    if (res == OK) {
        csound->Message(csound, "%s", pbuffer.data);
    }
    csound->Free(csound, pbuffer.data);

    if (res != OK) {
        return csound->InitError(csound, "[csnarray] Internal error: wrong print buffer allocation");
    }
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

    uint32_t source_handle = (uint32_t) p->source_handle->id;
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
    }

    CSN_ARRAY *arr = slot->array;

    if (csnfile_show_array(csound, &p->pbuffer, arr) != OK) {
        return csound->PerfError(csound, &p->h, "[csnarray] Internal error: wrong print buffer allocation");
    }

    csound->Message(csound, "%s", p->pbuffer.data);
    return OK;
}

