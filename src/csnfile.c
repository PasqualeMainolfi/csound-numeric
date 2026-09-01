#include "csnfile.h"
#include "csnregistry.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>


static void csnfile_write_header(CSN_FILE_HEADER *header, CSN_ARRAY *arr) {
    memcpy(header->magic, CSN_MAGIC, 4);
    header->major = CSN_FILE_VERSION_MAJOR;
    header->minor = CSN_FILE_VERSION_MINOR;
    header->dtype = (uint32_t) arr->itype;
    header->dim = arr->ndim;
    header->size = arr->size;
    memcpy(header->shape, arr->shape, sizeof(uint32_t) * CSN_MAX_DIMS);
    header->data_bytes = sizeof(double) * arr->size * arr->itype;
}

CSN_FILE_ERROR_CODE csnfile_save_array_to_file(CSN_ARRAY *arr, const char *path) {
    CSN_FILE_HEADER header = {0};
    csnfile_write_header(&header, arr);

    int32_t res = CSN_FILE_NO_ERROR;

    FILE *fptr = fopen(path, "wb");
    if (fptr == NULL){
        return CSN_FILE_NOT_FOUND;
    }
    size_t count;
    count = fwrite(header.magic, sizeof(uint8_t), 4, fptr);
    if (count != 4){
        res = CSN_FILE_INVALID_HEADER_MAGIC;
        goto done;
    }
    count = fwrite(&header.major, sizeof(uint16_t), 1, fptr);
    if (count != 1){
        res = CSN_FILE_INVALID_HEADER_VMAJOR;
        goto done;
    }
    count = fwrite(&header.minor, sizeof(uint16_t), 1, fptr);
    if (count != 1){
        res = CSN_FILE_INVALID_HEADER_VMINOR;
        goto done;
    }
    count = fwrite(&header.dtype, sizeof(uint32_t), 1, fptr);
    if (count != 1){
        res = CSN_FILE_INVALID_HEADER_DTYPE;
        goto done;
    }
    count = fwrite(&header.dim, sizeof(uint32_t), 1, fptr);
    if (count != 1){
        res = CSN_FILE_INVALID_HEADER_DIM;
        goto done;
    }
    count = fwrite(&header.size, sizeof(uint64_t), 1, fptr);
    if (count != 1){
        res = CSN_FILE_INVALID_HEADER_SIZE;
        goto done;
    }
    count = fwrite(header.shape, sizeof(uint32_t), CSN_MAX_DIMS, fptr);
    if (count != CSN_MAX_DIMS){
        res = CSN_FILE_INVALID_HEADER_SHAPE;
        goto done;
    }
    count = fwrite(&header.data_bytes, sizeof(uint64_t), 1, fptr);
    if (count != 1){
        res = CSN_FILE_INVALID_HEADER_DATA_BYTES;
        goto done;
    }
    count = fwrite(arr->data, 1, (size_t) header.data_bytes, fptr);
    if (count != header.data_bytes) {
        res = CSN_FILE_INVALID_HEADER_DATA_RAW;
        goto done;
    }

done:
    fclose(fptr);
    return res;
}

static CSN_FILE_ERROR_CODE VALIDATE_VERSION(CSN_FILE_HEADER *header) {
    switch (header->major) {
        case 1:
            switch (header->minor) {
                case 0:
                    if (sizeof(header->shape) != sizeof(uint32_t) * 8) {
                        return CSN_FILE_INVALID_SHAPE_SIZE;
                    }
                    break;
                default:
                    return CSN_FILE_INVALID_HEADER_VMINOR;
                    break;
            }
            break;
        default:
            return CSN_FILE_INVALID_HEADER_VMAJOR;
            break;
    }

    return CSN_FILE_NO_ERROR;
}

CSN_FILE_ERROR_CODE csnfile_load_array_from_file(CSOUND *csound, CSN_FILE_HEADER *header, double **data, size_t *data_capacity, const char *path) {
    FILE *fptr = fopen(path, "rb");
    if (fptr == NULL) return CSN_FILE_NOT_FOUND;

    int32_t res = CSN_FILE_NO_ERROR;

    size_t count;
    count = fread(header->magic, sizeof(uint8_t), 4, fptr);
    if (count != 4) {
        res = CSN_FILE_INVALID_HEADER_MAGIC;
        goto done;
    }
    if (memcmp(header->magic, CSN_MAGIC, 4) != 0) {
        res = CSN_FILE_INVALID_HEADER_MAGIC;
        goto done;
    }

    count = fread(&header->major, sizeof(uint16_t), 1, fptr);
    if (count != 1) {
        res = CSN_FILE_INVALID_HEADER_VMAJOR;
        goto done;
    }
    count = fread(&header->minor, sizeof(uint16_t), 1, fptr);
    if (count != 1) {
        res = CSN_FILE_INVALID_HEADER_VMINOR;
        goto done;
    }

    res = VALIDATE_VERSION(header);
    if (res != OK) goto done;

    count = fread(&header->dtype, sizeof(uint32_t), 1, fptr);
    if (count != 1 || (header->dtype != CSN_DTYPE_F64 && header->dtype != CSN_DTYPE_C128)) {
        res = CSN_FILE_INVALID_HEADER_DTYPE;
        goto done;
    }
    count = fread(&header->dim, sizeof(uint32_t), 1, fptr);
    if (count != 1) {
        res = CSN_FILE_INVALID_HEADER_DIM;
        goto done;
    }
    count = fread(&header->size, sizeof(uint64_t), 1, fptr);
    if (count != 1) {
        res = CSN_FILE_INVALID_HEADER_SIZE;
        goto done;
    }
    count = fread(header->shape, sizeof(uint32_t), CSN_MAX_DIMS, fptr);
    if (count != CSN_MAX_DIMS) {
        res = CSN_FILE_INVALID_HEADER_SHAPE;
        goto done;
    }

    count = fread(&header->data_bytes, sizeof(uint64_t), 1, fptr);
    if (count != 1) {
        res = CSN_FILE_INVALID_HEADER_SHAPE;
        goto done;
    }

    size_t check_size = 0;
    if (get_array_size_from_shape(&check_size, header->dim, header->shape) != OK) {
        res = CSN_FILE_EXCEEDS_SIZE_LIMIT;
        goto done;
    }

    ITEM_TYPE itype = (ITEM_TYPE) header->dtype;
    size_t check_data_bytes = check_size * sizeof(double) * itype;
    if (header->data_bytes != check_data_bytes) {
        res = CSN_FILE_SIZE_MISMATCH;
        goto done;
    }

    size_t requested_size = (size_t) header->size * itype;
    if (requested_size > *data_capacity) {
        size_t new_cap = requested_size * 2;
        double *new_data = csound->ReAlloc(csound, *data, sizeof(double) * new_cap);
        if (new_data == NULL) {
            res = CSN_FILE_WRONG_MEMORY_ALLOCATION;
            goto done;
        }
        *data = new_data;
        *data_capacity = new_cap;
    }

    count = fread(*data, 1, (size_t) header->data_bytes, fptr);
    if (count != header->data_bytes) res = CSN_FILE_INVALID_HEADER_DATA_BYTES;

done:
    fclose(fptr);
    return res;
}

void csnfile_dispatch_error(const char **error_message, CSN_FILE_ERROR_CODE error_code) {
    switch (error_code) {
        case CSN_FILE_NOT_FOUND:
            *error_message = "File not found";
            break;
        case CSN_FILE_INVALID_HEADER_MAGIC:
            *error_message = "Invalid file signature (magic header mismatch)";
            break;
        case CSN_FILE_INVALID_HEADER_VMAJOR:
            *error_message = "Unsupported or invalid major file version";
            break;
        case CSN_FILE_INVALID_HEADER_VMINOR:
            *error_message = "Unsupported or invalid minor file version";
            break;
        case CSN_FILE_INVALID_HEADER_DTYPE:
            *error_message = "Invalid or unsupported array dtype";
            break;
        case CSN_FILE_INVALID_HEADER_DIM:
            *error_message = "Invalid number of array dimensions";
            break;
        case CSN_FILE_INVALID_HEADER_SHAPE:
            *error_message = "Invalid array shape in file header";
            break;
        case CSN_FILE_INVALID_HEADER_DATA_BYTES:
            *error_message = "Invalid data byte count in file header";
            break;
        case CSN_FILE_INVALID_HEADER_DATA_RAW:
            *error_message = "Invalid or incomplete raw array data";
            break;
        case CSN_FILE_INVALID_SHAPE_SIZE:
            *error_message = "Array size does not match the declared shape";
            break;
        case CSN_FILE_INVALID_HEADER_SIZE:
            *error_message = "Invalid array size in file header";
            break;
        case CSN_FILE_EXCEEDS_SIZE_LIMIT:
            *error_message = "Array exceeds the maximum supported size";
            break;
        case CSN_FILE_SIZE_MISMATCH:
            *error_message = "File size does not match the declared array data size";
            break;
        case CSN_FILE_WRONG_MEMORY_ALLOCATION:
            *error_message = "Memory allocation failed while loading array data";
            break;
        default:
            *error_message = "Unknown file error";
            break;
    }
}


/* PRINTING */

static int32_t print_buffer_append(CSOUND *csound, CSN_PRINT_BUFFER *buffer, const char *fmt, ...) {
    if (buffer == NULL || fmt == NULL || buffer->data == NULL || buffer->length >= buffer->capacity) {
        return NOTOK;
    }

    va_list args;
    va_start(args, fmt);
    va_list copy;
    va_copy(copy, args);

    int needed = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);

    if (needed < 0) {
        va_end(args);
        return NOTOK;
    }

    size_t needed_size = (size_t) needed;
    if (needed_size > SIZE_MAX - buffer->length - 1U) {
        va_end(args);
        return NOTOK;
    }

    size_t required_length = buffer->length + needed_size + 1U;
    if (required_length > buffer->capacity) {
        size_t new_cap = buffer->capacity;
        while (new_cap < required_length) {
            if (new_cap > SIZE_MAX / 2U) {
                new_cap = required_length;
                break;
            }
            new_cap *= 2U;
        }
        char *new_data = csound->ReAlloc(csound, buffer->data, new_cap);
        if (new_data == NULL) {
            va_end(args);
            return NOTOK;
        }
        buffer->data = new_data;
        buffer->capacity = new_cap;
    }

    int written = vsnprintf(buffer->data + buffer->length, needed_size + 1U, fmt, args);
    va_end(args);
    if (written != needed) return NOTOK;

    buffer->length += needed_size;
    return OK;
}

static int32_t format_array_dimension(CSOUND *csound, CSN_PRINT_BUFFER *buffer, const CSN_ARRAY *arr, uint32_t dim, size_t base_offset, bool summarize) {
    int32_t res;
    res = print_buffer_append(csound, buffer, "[");
    if (res != OK) return NOTOK;


    uint32_t extent = arr->shape[dim];
    bool should_summarize = summarize && extent > CSN_PRINT_EDGE_ITEMS * 2;

    if (dim == arr->ndim - 1) {
        bool first = true;
        for (uint32_t i = 0; i < extent; ++i) {
            if (should_summarize && i == CSN_PRINT_EDGE_ITEMS) {
                if (!first) {
                    res = print_buffer_append(csound, buffer, " ");
                    if (res != OK) return NOTOK;
                }
                res = print_buffer_append(csound, buffer, "%s", CSN_PRINT_SUMMARIZE_SIMBOL);
                if (res != OK) return NOTOK;
                first = false;
                i = extent - CSN_PRINT_EDGE_ITEMS - 1;
                continue;
            }
            if (!first) {
                res = print_buffer_append(csound, buffer, " ");
                if (res != OK) return NOTOK;
            }
            size_t offset = base_offset + (size_t) i * arr->strides[dim];
            if (arr->itype == CSN_REAL) {
                res = print_buffer_append(csound, buffer, "%.5g", arr->data[offset]);
            }
            else {
                double re = arr->data[offset * 2];
                double im = arr->data[offset * 2 + 1];

                res = print_buffer_append(csound, buffer, "%.5g%+.5gj", re, im);
            }
            if (res != OK) return NOTOK;
            first = false;
        }
    }
    else {
        bool first = true;
        for (uint32_t i = 0; i < extent; ++i) {
            if (should_summarize && i == CSN_PRINT_EDGE_ITEMS) {
                uint32_t nlines = arr->ndim - dim - 1;
                if (!first) {
                    for (uint32_t n = 0; n < nlines; ++n) {
                        res = print_buffer_append(csound, buffer, "\n");
                        if (res != OK) return NOTOK;
                    }

                    for (uint32_t s = 0; s < dim + 1; ++s) {
                        res = print_buffer_append(csound, buffer, " ");
                        if (res != OK) return NOTOK;
                    }
                }
                res = print_buffer_append(csound, buffer, "%s", CSN_PRINT_SUMMARIZE_SIMBOL);
                if (res != OK) return NOTOK;
                first = false;
                i = extent - CSN_PRINT_EDGE_ITEMS - 1;
                continue;
            }
            size_t offset = base_offset + (size_t) i * arr->strides[dim];

            if (!first) {
                uint32_t nlines = arr->ndim - dim - 1;
                for (uint32_t n = 0; n < nlines; ++n) {
                    res = print_buffer_append(csound, buffer, "\n");
                    if (res != OK) return NOTOK;
                }
                for (uint32_t s = 0; s < dim + 1; ++s) {
                    res = print_buffer_append(csound, buffer, " ");
                    if (res != OK) return NOTOK;
                }
            }
            res = format_array_dimension(csound, buffer, arr, dim + 1, offset, summarize);
            if (res != OK) return NOTOK;
            first = false;
        }
    }

    return print_buffer_append(csound, buffer, "]");
}

int32_t csnfile_show_array(CSOUND *csound, CSN_PRINT_BUFFER *buffer, const CSN_ARRAY *arr) {
    if (buffer == NULL || buffer->data == NULL || buffer->capacity == 0 || arr == NULL || arr->ndim == 0) {
        return NOTOK;
    }

    /* Each call represents one complete print operation. In particular, a
       k-rate opcode must replace its previous rendering instead of appending
       the entire print history to the reusable buffer. */
    buffer->length = 0;
    buffer->data[0] = '\0';

    uint32_t ndim = arr->ndim;
    const uint32_t *shape = arr->shape;
    size_t size = arr->size;
    ITEM_TYPE itype = arr->itype;

    bool summarize = size > CSN_PRINT_THRESHOLD;

    int32_t res = OK;
    res = print_buffer_append(csound, buffer, "CsnArr(shape=(");
    if (res != OK) return NOTOK;
    for (uint32_t i = 0; i < ndim; i++) {
        if (i > 0) {
            res = print_buffer_append(csound, buffer, ", ");
            if (res != OK) return NOTOK;
        }
        res = print_buffer_append(csound, buffer, "%u", shape[i]);
        if (res != OK) return NOTOK;
    }
    if (ndim == 1) {
        res = print_buffer_append(csound, buffer, ",");
        if (res != OK) return NOTOK;
    }

    const char *dtype = itype == CSN_REAL ? "float64" : "complex128";
    res = print_buffer_append(csound, buffer, "), dtype=%s)\n", dtype);
    if (res != OK) return NOTOK;

    if (size == 0) {
        res = print_buffer_append(csound, buffer, "[]");
    }
    else {
        res = format_array_dimension(csound, buffer, arr, 0, 0, summarize);
    }
    if (res != OK) return NOTOK;

    return print_buffer_append(csound, buffer, "\n");
}
