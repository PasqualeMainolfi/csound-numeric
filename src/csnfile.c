#include "csnfile.h"
#include "csnregistry.h"
#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


CSN_FILE_ERROR_CODE csnfile_save_array(CSN_ARRAY *arr, const char *path, const char *ext) {
    if (strcmp(ext, CSN_FILE_EXT) == 0) {
        return csnfile_save_array_to_file(arr, path);
    } else if (strcmp(ext, CSN_FILE_NPY_EXT) == 0) {
        return csnfile_save_array_to_numpy_file(arr, path);
    }
    return CSN_FILE_INVALID_HEADER_MAGIC;
}

static inline int32_t get_numpy_file_dict(char *numpy_dict, size_t dict_size, const char *format, bool fort_order, uint32_t ndim, uint32_t *shape) {
    const char *forder = fort_order ? "True" : "False";
    size_t p = 0;
    int n = snprintf(numpy_dict + p, dict_size - p, "{'descr': '%s', 'fortran_order': %s, 'shape': (", format, forder);
    if (n < 0 || (size_t) n >= dict_size - p) return NOTOK;
    p += (size_t) n;
    for (uint32_t i = 0; i < ndim; i++) {
        const char *s = i + 1 < ndim ? "%u, " : "%u";
        n = snprintf(numpy_dict + p, dict_size - p, s, shape[i]);
        if (n < 0 || (size_t) n >= dict_size - p) return NOTOK;
        p += (size_t) n;
    }

    if (ndim == 1) {
        n = snprintf(numpy_dict + p, dict_size - p, ",");
        if (n < 0 || (size_t) n >= dict_size - p) return NOTOK;
        p += (size_t) n;
    }

    n = snprintf(numpy_dict + p, dict_size - p, "), }");
    if (n < 0 || (size_t) n >= dict_size - p) return NOTOK;

    return OK;
}

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
    if (header->size != check_size) {
        res = CSN_FILE_SIZE_MISMATCH;
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


static void csnfile_write_numpy_header(CSN_FILE_NUMPY_HEADER *header, const char *dict, CSN_ARRAY *arr) {
    memcpy(header->magic, CSN_NUMPY_MAGIC, CSN_MAGIC_NUMPY_SIZE);
    header->major = CSN_FILE_NUMPY_VERSION_MAJOR;
    header->minor = CSN_FILE_NUMPY_VERSION_MINOR;

    size_t plen = 12; // v 3.0
    size_t dict_len = strlen(dict);
    header->padding_len = (64 - ((plen + dict_len + 1) % 64)) % 64;
    header->header_length = (uint32_t) (dict_len + header->padding_len + 1);

    memcpy(header->dict, dict, dict_len + 1);
    header->data_bytes = sizeof(double) * arr->size * arr->itype;
}

static inline void write_u32_le(uint8_t *data, uint32_t value) {
    data[0] = (uint8_t) (value);
    data[1] = (uint8_t) (value >> 8u);
    data[2] = (uint8_t) (value >> 16);
    data[3] = (uint8_t) (value >> 24);
}

CSN_FILE_ERROR_CODE csnfile_save_array_to_numpy_file(CSN_ARRAY *arr, const char *path) {
    if (arr == NULL || arr->ndim == 0 || arr->ndim > CSN_MAX_DIMS ||
        (arr->itype != CSN_REAL && arr->itype != CSN_COMPLEX)) return CSN_FILE_INVALID_HEADER_DTYPE;
    if (arr->size != 0 && arr->data == NULL) return CSN_FILE_INVALID_HEADER_DATA_RAW;
    const uint16_t endian_probe = 1;
    const bool little_endian = *(const uint8_t *) &endian_probe == 1;
    const char *fmt = arr->itype == CSN_REAL
        ? (little_endian ? "<f8" : ">f8")
        : (little_endian ? "<c16" : ">c16");

    size_t size = 0;
    if (get_array_size_from_shape(&size, arr->ndim, arr->shape) != OK || size != arr->size)
        return CSN_FILE_INVALID_HEADER_SHAPE;

    char dict[sizeof(((CSN_FILE_NUMPY_HEADER *) 0)->dict)];
    if (get_numpy_file_dict(dict, sizeof(dict), fmt, false, arr->ndim, arr->shape) != OK)
        return CSN_FILE_INVALID_HEADER_SIZE;

    CSN_FILE_NUMPY_HEADER header = {0};
    csnfile_write_numpy_header(&header, dict, arr);

    CSN_FILE_ERROR_CODE res = CSN_FILE_NO_ERROR;

    FILE *fptr = fopen(path, "wb");
    if (fptr == NULL){
        return CSN_FILE_NOT_FOUND;
    }
    size_t count;
    count = fwrite(header.magic, sizeof(uint8_t), CSN_MAGIC_NUMPY_SIZE, fptr);
    if (count != CSN_MAGIC_NUMPY_SIZE){
        res = CSN_FILE_INVALID_HEADER_MAGIC;
        goto done;
    }
    count = fwrite(&header.major, sizeof(uint8_t), 1, fptr);
    if (count != 1){
        res = CSN_FILE_INVALID_HEADER_VMAJOR;
        goto done;
    }
    count = fwrite(&header.minor, sizeof(uint8_t), 1, fptr);
    if (count != 1){
        res = CSN_FILE_INVALID_HEADER_VMINOR;
        goto done;
    }

    uint8_t hlen[4] = {0};
    write_u32_le(hlen, header.header_length);
    count = fwrite(hlen, sizeof(uint8_t), 4, fptr);
    if (count != 4){
        res = CSN_FILE_INVALID_HEADER_DATA_RAW;
        goto done;
    }

    size_t dict_len = strlen(dict);
    if (dict_len + header.padding_len > UINT32_MAX) {
        res = CSN_FILE_INVALID_HEADER_DATA_RAW;
        goto done;
    }

    count = fwrite(header.dict, 1, dict_len, fptr);
    if (count != dict_len){
        res = CSN_FILE_INVALID_HEADER_DATA_RAW;
        goto done;
    }

    for (size_t i = 0; i < header.padding_len; i++) {
        if (fputc(' ', fptr) == EOF) { res = CSN_FILE_INVALID_HEADER_DATA_RAW; goto done; }
    }
    if (fputc('\n', fptr) == EOF) { res = CSN_FILE_INVALID_HEADER_DATA_RAW; goto done; }

    if (header.data_bytes != 0) {
        count = fwrite(arr->data, 1, (size_t) header.data_bytes, fptr);
        if (count != header.data_bytes) {
            res = CSN_FILE_INVALID_HEADER_DATA_RAW;
            goto done;
        }
    }

done:
    if (fclose(fptr) != 0 && res == CSN_FILE_NO_ERROR) res = CSN_FILE_INVALID_HEADER_DATA_RAW;
    return res;
}

static const char *numpy_dict_value(const char *dict, const char *key) {
    const char *p = strstr(dict, key);
    if (p == NULL) {
        size_t length = strlen(key);
        if (length >= 32) return NULL;
        char alternate[32];
        memcpy(alternate, key, length + 1);
        alternate[0] = '"';
        alternate[length - 1] = '"';
        p = strstr(dict, alternate);
    }
    if (p == NULL) return NULL;
    p += strlen(key);
    while (*p == ' ' || *p == '\t') ++p;
    if (*p++ != ':') return NULL;
    while (*p == ' ' || *p == '\t') ++p;
    return p;
}

static inline int32_t parse_numpy_shape(const char *dict, uint32_t *ndim, uint32_t *shape) {
    const char *p = numpy_dict_value(dict, "'shape'");
    if (p == NULL || *p++ != '(') return NOTOK;
    uint32_t n = 0;
    while (1) {
        while (*p == ' ' || *p == '\t') ++p;
        if (*p == ')') break;
        if (n >= CSN_MAX_DIMS || *p < '0' || *p > '9') return NOTOK;
        char *end = NULL;
        errno = 0;
        unsigned long value = strtoul(p, &end, 10);
        if (errno == ERANGE || value > UINT32_MAX) return NOTOK;
        shape[n++] = (uint32_t) value;
        p = end;
        while (*p == ' ' || *p == '\t') ++p;
        if (*p == ')') {
            if (n != 1) break;
            return NOTOK; /* one-dimensional tuples require a comma */
        }
        if (*p++ != ',') return NOTOK;
        while (*p == ' ' || *p == '\t') ++p;
        if (*p == ')') break;
    }
    if (n == 0) return NOTOK; /* scalar arrays have no CSN_ARRAY representation */
    *ndim = n;
    return OK;
}

static inline int32_t parse_numpy_descr(const char *dict, CSN_NUMPY_DESCR *item) {
    const char *p = numpy_dict_value(dict, "'descr'");
    if (p == NULL) return NOTOK;
    if (*p != '\'' && *p != '"') return NOTOK;
    char quote = *p++;
    item->endian = *p;
    if (*p == '<' || *p == '>' || *p == '=' || *p == '|') {
        ++p;
    } else {
        return NOTOK;
    }

    item->kind = *p++;
    switch (item->kind) {
        case 'f':
        case 'i':
        case 'u':
        case 'b':
            item->itype = CSN_REAL;
            break;
        case 'c':
            item->itype = CSN_COMPLEX;
            break;
        default:
            return NOTOK;
    }

    if (*p < '0' || *p > '9') return NOTOK;
    char *end = NULL;
    errno = 0;
    unsigned long value = strtoul(p, &end, 10);
    if (*end != quote || errno == ERANGE || value > UINT32_MAX) return NOTOK;
    item->item_size = (uint32_t) value;

    switch (item->kind) {
        case 'b': if (value != 1) return NOTOK; break;
        case 'i': case 'u':
            if (value != 1 && value != 2 && value != 4 && value != 8) return NOTOK;
            break;
        case 'f': if (value != 2 && value != 4 && value != 8) return NOTOK; break;
        case 'c': if (value != 8 && value != 16) return NOTOK; break;
    }
    if (item->endian == '|' && value != 1) return NOTOK;

    return OK;
}

static inline int32_t parse_numpy_fortran_order(const char *dict, bool *forder) {
    const char *p = numpy_dict_value(dict, "'fortran_order'");
    if (p == NULL) return NOTOK;
    if (strncmp(p, "False", 5) == 0 && (p[5] == ',' || p[5] == '}' || p[5] == ' ')) {
        *forder = false;
    } else if (strncmp(p, "True", 4) == 0 && (p[4] == ',' || p[4] == '}' || p[4] == ' ')) {
        *forder = true;
    } else {
        return NOTOK;
    }

    return OK;
}

static uint64_t numpy_uint(const uint8_t *bytes, uint32_t n, bool little_endian) {
    uint64_t value = 0;
    for (uint32_t i = 0; i < n; ++i) {
        uint32_t shift = little_endian ? i : n - i - 1;
        value |= (uint64_t) bytes[i] << (8 * shift);
    }
    return value;
}

static double numpy_float16(uint16_t raw) {
    uint32_t sign = (uint32_t) (raw & 0x8000) << 16;
    uint32_t exponent = (raw >> 10) & 31;
    uint32_t fraction = raw & 1023;
    uint32_t bits;
    if (exponent == 0 && fraction != 0) {
        int32_t e = -14;
        while ((fraction & 1024) == 0) { fraction <<= 1; --e; }
        bits = sign | (uint32_t) (e + 127) << 23 | (fraction & 1023) << 13;
    } else if (exponent == 0) {
        bits = sign;
    } else if (exponent == 31) {
        bits = sign | 0x7f800000u | fraction << 13;
    } else {
        bits = sign | (exponent + 112) << 23 | fraction << 13;
    }
    float value;
    memcpy(&value, &bits, sizeof(value));
    return (double) value;
}

static double numpy_numeric_value(const uint8_t *bytes, char kind, uint32_t size, bool little_endian) {
    uint64_t raw = numpy_uint(bytes, size, little_endian);
    if (kind == 'b') return raw != 0 ? 1.0 : 0.0;
    if (kind == 'u') return (double) raw;
    if (kind == 'i') {
        uint32_t bits = size * 8;
        uint64_t sign = UINT64_C(1) << (bits - 1);
        if ((raw & sign) == 0) return (double) raw;
        uint64_t mask = size == 8 ? UINT64_MAX : (UINT64_C(1) << bits) - 1;
        return -(double) ((~raw & mask) + 1);
    }
    if (size == 2) return numpy_float16((uint16_t) raw);
    if (size == 4) {
        uint32_t bits = (uint32_t) raw;
        float value;
        memcpy(&value, &bits, sizeof(value));
        return (double) value;
    }
    double value;
    memcpy(&value, &raw, sizeof(value));
    return value;
}

static size_t numpy_c_offset(size_t offset, uint32_t ndim, const uint32_t *shape, const size_t *c_strides) {
    size_t result = 0;
    for (uint32_t axis = 0; axis < ndim; ++axis) {
        result += (offset % shape[axis]) * c_strides[axis];
        offset /= shape[axis];
    }
    return result;
}


CSN_FILE_ERROR_CODE csnfile_load_array_from_numpy_file(CSOUND *csound, CSN_FILE_NUMPY_HEADER *header, double **data, size_t *data_capacity, const char *path) {
    if (csound == NULL || header == NULL || data == NULL || data_capacity == NULL || path == NULL)
        return CSN_FILE_INVALID_HEADER_DATA_RAW;
    FILE *fptr = fopen(path, "rb");
    if (fptr == NULL) return CSN_FILE_NOT_FOUND;

    CSN_FILE_ERROR_CODE res = CSN_FILE_NO_ERROR;

    size_t count;
    count = fread(header->magic, sizeof(uint8_t), CSN_MAGIC_NUMPY_SIZE, fptr);
    if (count != CSN_MAGIC_NUMPY_SIZE) {
        res = CSN_FILE_INVALID_HEADER_MAGIC;
        goto done;
    }
    if (memcmp(header->magic, CSN_NUMPY_MAGIC, CSN_MAGIC_NUMPY_SIZE) != 0) {
        res = CSN_FILE_INVALID_HEADER_MAGIC;
        goto done;
    }

    count = fread(&header->major, 1, 1, fptr);
    if (count != 1) {
        res = CSN_FILE_INVALID_HEADER_VMAJOR;
        goto done;
    }
    count = fread(&header->minor, 1, 1, fptr);
    if (count != 1) {
        res = CSN_FILE_INVALID_HEADER_VMINOR;
        goto done;
    }

    if (header->major < 1 || header->major > 3) {
        res = CSN_FILE_INVALID_HEADER_VMAJOR;
        goto done;
    }
    if (header->minor != 0) {
        res = CSN_FILE_INVALID_HEADER_VMINOR;
        goto done;
    }

    uint8_t length_bytes[4] = {0};
    uint32_t length_size = header->major == 1 ? 2 : 4;
    if (fread(length_bytes, 1, length_size, fptr) != length_size) {
        res = CSN_FILE_INVALID_HEADER_SIZE;
        goto done;
    }
    header->header_length = (uint32_t) numpy_uint(length_bytes, length_size, true);
    if (header->header_length == 0 || header->header_length >= sizeof(header->dict)) {
        res = CSN_FILE_INVALID_HEADER_SIZE;
        goto done;
    }

    size_t hlen = (size_t) header->header_length;
    count = fread(header->dict, 1, hlen, fptr);
    if (count != hlen || header->dict[hlen - 1] != '\n') {
        res = CSN_FILE_INVALID_HEADER_SIZE;
        goto done;
    }

    size_t dict_len = hlen - 1;
    while (dict_len > 0 && header->dict[dict_len - 1] == ' ') --dict_len;
    header->padding_len = hlen - dict_len - 1;
    header->dict[dict_len] = '\0';

    if (parse_numpy_descr(header->dict, &header->descr) != OK) {
        res = CSN_FILE_INVALID_HEADER_DTYPE;
        goto done;
    }
    if (parse_numpy_fortran_order(header->dict, &header->fortran_order) != OK) {
        res = CSN_FILE_INVALID_HEADER_SIZE;
        goto done;
    }
    if (parse_numpy_shape(header->dict, &header->dim, header->shape) != OK) {
        res = CSN_FILE_INVALID_HEADER_SHAPE;
        goto done;
    }
    if (get_array_size_from_shape(&header->size, header->dim, header->shape) != OK) {
        res = CSN_FILE_EXCEEDS_SIZE_LIMIT;
        goto done;
    }

    header->data_bytes = (uint64_t) header->size * header->descr.item_size;
    size_t requested_size = header->size * header->descr.itype;
    if (requested_size > SIZE_MAX / sizeof(double)) {
        res = CSN_FILE_EXCEEDS_SIZE_LIMIT;
        goto done;
    }
    if (requested_size > *data_capacity) {
        double *new_data = *data == NULL
            ? csound->Malloc(csound, sizeof(double) * requested_size)
            : csound->ReAlloc(csound, *data, sizeof(double) * requested_size);
        if (new_data == NULL) {
            res = CSN_FILE_WRONG_MEMORY_ALLOCATION;
            goto done;
        }
        *data = new_data;
        *data_capacity = requested_size;
    }

    const uint16_t endian_probe = 1;
    bool little_endian = header->descr.endian == '<' ||
        (header->descr.endian == '=' && *(const uint8_t *) &endian_probe == 1);
    uint32_t item_size = header->descr.item_size;
    size_t c_strides[CSN_MAX_DIMS] = {0};
    c_strides[header->dim - 1] = 1;
    for (uint32_t axis = header->dim - 1; axis-- > 0; )
        c_strides[axis] = c_strides[axis + 1] * header->shape[axis + 1];

    uint8_t raw[65536];
    size_t batch_capacity = sizeof(raw) / item_size;
    for (size_t start = 0; start < header->size; ) {
        size_t batch = header->size - start;
        if (batch > batch_capacity) batch = batch_capacity;
        if (fread(raw, item_size, batch, fptr) != batch) {
            res = CSN_FILE_INVALID_HEADER_DATA_RAW;
            goto done;
        }
        for (size_t j = 0; j < batch; ++j) {
            const uint8_t *item = raw + j * item_size;
            size_t dest = header->fortran_order
                ? numpy_c_offset(start + j, header->dim, header->shape, c_strides)
                : start + j;
            if (header->descr.itype == CSN_COMPLEX) {
                uint32_t component_size = item_size / 2;
                (*data)[dest * 2] = numpy_numeric_value(item, 'f', component_size, little_endian);
                (*data)[dest * 2 + 1] = numpy_numeric_value(item + component_size, 'f', component_size, little_endian);
            } else {
                (*data)[dest] = numpy_numeric_value(item, header->descr.kind, item_size, little_endian);
            }
        }
        start += batch;
    }
    if (fgetc(fptr) != EOF || ferror(fptr)) res = CSN_FILE_SIZE_MISMATCH;

done:
    fclose(fptr);
    return res;
}
