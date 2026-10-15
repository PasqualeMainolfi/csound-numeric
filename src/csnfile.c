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
    if (arr->itype == CSN_STRING) {
        uint64_t bytes = 0;
        for (size_t i = 0; i < arr->size; i++) {
            const char *item = arr->sdata[i].data;
            bytes += sizeof(uint32_t) + (item != NULL ? strlen(item) : 0);
        }
        header->data_bytes = bytes;
    } else {
        header->data_bytes = sizeof(double) * arr->size * arr->itype;
    }
}

static int32_t csnfile_write_strings(FILE *fptr, const CSN_ARRAY *arr) {
    for (size_t i = 0; i < arr->size; i++) {
        const char *item = arr->sdata[i].data != NULL ? arr->sdata[i].data : "";
        size_t length = strlen(item);
        if (length > UINT32_MAX) return NOTOK;
        uint32_t length32 = (uint32_t) length;
        if (fwrite(&length32, sizeof(uint32_t), 1, fptr) != 1) return NOTOK;
        if (length > 0 && fwrite(item, 1, length, fptr) != length) return NOTOK;
    }
    return OK;
}

CSN_FILE_ERROR_CODE csnfile_save_array_to_file(CSN_ARRAY *arr, const char *path) {
    CSN_FILE_HEADER header = {0};
    csnfile_write_header(&header, arr);

    CSN_FILE_ERROR_CODE res = CSN_FILE_NO_ERROR;

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
    if (arr->itype == CSN_STRING) {
        if (csnfile_write_strings(fptr, arr) != OK) res = CSN_FILE_INVALID_HEADER_DATA_RAW;
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

/* Reads and validates everything up to the payload: magic, version, dtype
   (any the format defines), layout, and a size that agrees with the shape. */
static CSN_FILE_ERROR_CODE csnfile_read_header(FILE *fptr, CSN_FILE_HEADER *header) {
    size_t count;
    count = fread(header->magic, sizeof(uint8_t), 4, fptr);
    if (count != 4) return CSN_FILE_INVALID_HEADER_MAGIC;
    if (memcmp(header->magic, CSN_MAGIC, 4) != 0) return CSN_FILE_INVALID_HEADER_MAGIC;

    count = fread(&header->major, sizeof(uint16_t), 1, fptr);
    if (count != 1) return CSN_FILE_INVALID_HEADER_VMAJOR;
    count = fread(&header->minor, sizeof(uint16_t), 1, fptr);
    if (count != 1) return CSN_FILE_INVALID_HEADER_VMINOR;

    CSN_FILE_ERROR_CODE res = VALIDATE_VERSION(header);
    if (res != CSN_FILE_NO_ERROR) return res;

    count = fread(&header->dtype, sizeof(uint32_t), 1, fptr);
    if (count != 1 || (header->dtype != CSN_DTYPE_F64 && header->dtype != CSN_DTYPE_C128 && header->dtype != CSN_DTYPE_STR)) {
        return CSN_FILE_INVALID_HEADER_DTYPE;
    }
    count = fread(&header->dim, sizeof(uint32_t), 1, fptr);
    if (count != 1) return CSN_FILE_INVALID_HEADER_DIM;
    count = fread(&header->size, sizeof(uint64_t), 1, fptr);
    if (count != 1) return CSN_FILE_INVALID_HEADER_SIZE;
    count = fread(header->shape, sizeof(uint32_t), CSN_MAX_DIMS, fptr);
    if (count != CSN_MAX_DIMS) return CSN_FILE_INVALID_HEADER_SHAPE;

    count = fread(&header->data_bytes, sizeof(uint64_t), 1, fptr);
    if (count != 1) return CSN_FILE_INVALID_HEADER_SHAPE;

    size_t check_size = 0;
    if (get_array_size_from_shape(&check_size, header->dim, header->shape) != OK) return CSN_FILE_EXCEEDS_SIZE_LIMIT;
    if (header->size != check_size) return CSN_FILE_SIZE_MISMATCH;
    return CSN_FILE_NO_ERROR;
}

CSN_FILE_ERROR_CODE csnfile_load_array_from_file(CSOUND *csound, CSN_FILE_HEADER *header, double **data, size_t *data_capacity, const char *path) {
    FILE *fptr = fopen(path, "rb");
    if (fptr == NULL) return CSN_FILE_NOT_FOUND;

    CSN_FILE_ERROR_CODE res = csnfile_read_header(fptr, header);
    if (res != CSN_FILE_NO_ERROR) goto done;
    if (header->dtype == CSN_DTYPE_STR) {
        res = CSN_FILE_DTYPE_KIND_MISMATCH;
        goto done;
    }

    ITEM_TYPE itype = (ITEM_TYPE) header->dtype;
    size_t check_data_bytes = (size_t) header->size * sizeof(double) * itype;
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

    size_t count = fread(*data, 1, (size_t) header->data_bytes, fptr);
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
        case CSN_FILE_DTYPE_KIND_MISMATCH:
            *error_message = "The file holds numbers and the variable strings, or the reverse: load it into a CsnArr or a CsnStr to match";
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
            switch (arr->itype) {
                case CSN_REAL:
                    res = print_buffer_append(csound, buffer, "%.5g", arr->data[offset]);
                    break;
                case CSN_COMPLEX: {
                        double re = arr->data[offset * 2];
                        double im = arr->data[offset * 2 + 1];
                        res = print_buffer_append(csound, buffer, "%.5g%+.5gj", re, im);
                    }
                    break;
                case CSN_STRING:
                    /* quoted as numpy prints them; a slot never written is "" */
                    res = print_buffer_append(csound, buffer, "'%s'", arr->sdata[offset].data != NULL ? arr->sdata[offset].data : "");
                    break;
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
    res = print_buffer_append(csound, buffer, itype == CSN_STRING ? "CsnStr(shape=(" : "CsnArr(shape=(");
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

    const char *dtype = itype == CSN_REAL ? "float64" : itype == CSN_STRING ? "str" : "complex128";
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

/* numpy's <U dtype holds one UCS4 code point per character, so a Csound
   string (UTF-8 bytes) is decoded. A byte that does not start a valid
   sequence stands for itself (as Latin-1), so every byte string can be
   saved. Reads stop at the terminator: a truncated sequence never runs past
   it, because '\0' is not a continuation byte. */
static size_t utf8_decode(const unsigned char *s, uint32_t *code_point) {
    unsigned char c = s[0];
    size_t n = 0;
    uint32_t value = 0;
    uint32_t smallest = 0;
    if (c < 0x80) {
        *code_point = c;
        return 1;
    } else if ((c & 0xE0) == 0xC0) {
        n = 2; value = c & 0x1Fu; smallest = 0x80;
    } else if ((c & 0xF0) == 0xE0) {
        n = 3; value = c & 0x0Fu; smallest = 0x800;
    } else if ((c & 0xF8) == 0xF0) {
        n = 4; value = c & 0x07u; smallest = 0x10000;
    } else {
        *code_point = c;
        return 1;
    }
    for (size_t i = 1; i < n; i++) {
        if ((s[i] & 0xC0) != 0x80) {
            *code_point = c;
            return 1;
        }
        value = (value << 6) | (s[i] & 0x3Fu);
    }
    if (value < smallest || value > 0x10FFFF || (value >= 0xD800 && value <= 0xDFFF)) {
        *code_point = c;
        return 1;
    }
    *code_point = value;
    return n;
}

static size_t utf8_length(const char *item) {
    const unsigned char *p = (const unsigned char *) item;
    size_t length = 0;
    uint32_t code_point = 0;
    while (*p != '\0') {
        p += utf8_decode(p, &code_point);
        length++;
    }
    return length;
}

/* Every item is padded with zero code points to the widest one, as numpy
   stores a fixed-width string array. Written little-endian, as declared. */
static int32_t numpy_write_strings(FILE *fptr, const CSN_ARRAY *arr, size_t width) {
    uint8_t chunk[4096];
    size_t used = 0;
    for (size_t i = 0; i < arr->size; i++) {
        const unsigned char *p = (const unsigned char *) (arr->sdata[i].data != NULL ? arr->sdata[i].data : "");
        for (size_t written = 0; written < width; written++) {
            uint32_t code_point = 0;
            if (*p != '\0') p += utf8_decode(p, &code_point);
            write_u32_le(chunk + used, code_point);
            used += 4;
            if (used == sizeof(chunk)) {
                if (fwrite(chunk, 1, used, fptr) != used) return NOTOK;
                used = 0;
            }
        }
    }
    if (used > 0 && fwrite(chunk, 1, used, fptr) != used) return NOTOK;
    return OK;
}

CSN_FILE_ERROR_CODE csnfile_save_array_to_numpy_file(CSN_ARRAY *arr, const char *path) {
    if (arr == NULL || arr->ndim == 0 || arr->ndim > CSN_MAX_DIMS || (arr->itype != CSN_REAL && arr->itype != CSN_COMPLEX && arr->itype != CSN_STRING)) return CSN_FILE_INVALID_HEADER_DTYPE;
    if (arr->size != 0 && arr->data == NULL) return CSN_FILE_INVALID_HEADER_DATA_RAW;
    const uint16_t endian_probe = 1;
    const bool little_endian = *(const uint8_t *) &endian_probe == 1;

    /* numpy has no zero-width string: an array of "" (or with no item) is <U1 */
    size_t width = 1;
    char string_fmt[32];
    if (arr->itype == CSN_STRING) {
        for (size_t i = 0; i < arr->size; i++) {
            size_t length = arr->sdata[i].data != NULL ? utf8_length(arr->sdata[i].data) : 0;
            if (length > width) width = length;
        }
        if (width > SIZE_MAX / 4 || (arr->size > 0 && width * 4 > SIZE_MAX / arr->size)) return CSN_FILE_EXCEEDS_SIZE_LIMIT;
        snprintf(string_fmt, sizeof(string_fmt), "<U%zu", width);
    }
    const char *fmt = arr->itype == CSN_STRING ? string_fmt
        : arr->itype == CSN_REAL ? (little_endian ? "<f8" : ">f8")
        : (little_endian ? "<c16" : ">c16");

    size_t size = 0;
    if (get_array_size_from_shape(&size, arr->ndim, arr->shape) != OK || size != arr->size)
        return CSN_FILE_INVALID_HEADER_SHAPE;

    char dict[sizeof(((CSN_FILE_NUMPY_HEADER *) 0)->dict)];
    if (get_numpy_file_dict(dict, sizeof(dict), fmt, false, arr->ndim, arr->shape) != OK)
        return CSN_FILE_INVALID_HEADER_SIZE;

    CSN_FILE_NUMPY_HEADER header = {0};
    csnfile_write_numpy_header(&header, dict, arr);
    if (arr->itype == CSN_STRING) header.data_bytes = (uint64_t) arr->size * width * 4;

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

    if (arr->itype == CSN_STRING) {
        if (numpy_write_strings(fptr, arr, width) != OK) res = CSN_FILE_INVALID_HEADER_DATA_RAW;
    } else if (header.data_bytes != 0) {
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
        case 'U': // UCS4 code points
        case 'S': // bytes
            item->itype = CSN_STRING;
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
        /* a width, not a byte count: <U5 is 5 code points, |S5 is 5 bytes */
        case 'U': case 'S': if (value > CSN_NUMPY_MAX_STRING_WIDTH) return NOTOK; break;
    }
    if (item->endian == '|' && value != 1 && item->kind != 'S') return NOTOK;
    if (item->kind == 'U' && item->endian == '|') return NOTOK;

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


/* Reads and validates the header up to the payload, which fptr is then at. */
static CSN_FILE_ERROR_CODE numpy_read_header(FILE *fptr, CSN_FILE_NUMPY_HEADER *header) {
    size_t count;
    count = fread(header->magic, sizeof(uint8_t), CSN_MAGIC_NUMPY_SIZE, fptr);
    if (count != CSN_MAGIC_NUMPY_SIZE) return CSN_FILE_INVALID_HEADER_MAGIC;
    if (memcmp(header->magic, CSN_NUMPY_MAGIC, CSN_MAGIC_NUMPY_SIZE) != 0) return CSN_FILE_INVALID_HEADER_MAGIC;

    count = fread(&header->major, 1, 1, fptr);
    if (count != 1) return CSN_FILE_INVALID_HEADER_VMAJOR;
    count = fread(&header->minor, 1, 1, fptr);
    if (count != 1) return CSN_FILE_INVALID_HEADER_VMINOR;

    if (header->major < 1 || header->major > 3) return CSN_FILE_INVALID_HEADER_VMAJOR;
    if (header->minor != 0) return CSN_FILE_INVALID_HEADER_VMINOR;

    uint8_t length_bytes[4] = {0};
    uint32_t length_size = header->major == 1 ? 2 : 4;
    if (fread(length_bytes, 1, length_size, fptr) != length_size) return CSN_FILE_INVALID_HEADER_SIZE;
    header->header_length = (uint32_t) numpy_uint(length_bytes, length_size, true);
    if (header->header_length == 0 || header->header_length >= sizeof(header->dict)) return CSN_FILE_INVALID_HEADER_SIZE;

    size_t hlen = (size_t) header->header_length;
    count = fread(header->dict, 1, hlen, fptr);
    if (count != hlen || header->dict[hlen - 1] != '\n') return CSN_FILE_INVALID_HEADER_SIZE;

    size_t dict_len = hlen - 1;
    while (dict_len > 0 && header->dict[dict_len - 1] == ' ') --dict_len;
    header->padding_len = hlen - dict_len - 1;
    header->dict[dict_len] = '\0';

    if (parse_numpy_descr(header->dict, &header->descr) != OK) return CSN_FILE_INVALID_HEADER_DTYPE;
    if (parse_numpy_fortran_order(header->dict, &header->fortran_order) != OK) return CSN_FILE_INVALID_HEADER_SIZE;
    if (parse_numpy_shape(header->dict, &header->dim, header->shape) != OK) return CSN_FILE_INVALID_HEADER_SHAPE;
    if (get_array_size_from_shape(&header->size, header->dim, header->shape) != OK) return CSN_FILE_EXCEEDS_SIZE_LIMIT;
    return CSN_FILE_NO_ERROR;
}

CSN_FILE_ERROR_CODE csnfile_load_array_from_numpy_file(CSOUND *csound, CSN_FILE_NUMPY_HEADER *header, double **data, size_t *data_capacity, const char *path) {
    if (csound == NULL || header == NULL || data == NULL || data_capacity == NULL || path == NULL)
        return CSN_FILE_INVALID_HEADER_DATA_RAW;
    FILE *fptr = fopen(path, "rb");
    if (fptr == NULL) return CSN_FILE_NOT_FOUND;

    CSN_FILE_ERROR_CODE res = numpy_read_header(fptr, header);
    if (res != CSN_FILE_NO_ERROR) goto done;
    if (header->descr.itype == CSN_STRING) {
        res = CSN_FILE_DTYPE_KIND_MISMATCH;
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

/* STRING LOADING */

static void free_string_items(CSOUND *csound, char **items, size_t count) {
    if (items == NULL) return;
    for (size_t i = 0; i < count; i++) {
        if (items[i] != NULL) csound->Free(csound, items[i]);
    }
    csound->Free(csound, items);
}

/* The inverse of utf8_decode: a code point numpy can hold, as UTF-8. */
static int32_t utf8_encode(uint32_t code_point, char *out, size_t *length) {
    if (code_point > 0x10FFFF || (code_point >= 0xD800 && code_point <= 0xDFFF)) return NOTOK;
    if (code_point < 0x80) {
        out[0] = (char) code_point;
        *length = 1;
    } else if (code_point < 0x800) {
        out[0] = (char) (0xC0 | (code_point >> 6));
        out[1] = (char) (0x80 | (code_point & 0x3F));
        *length = 2;
    } else if (code_point < 0x10000) {
        out[0] = (char) (0xE0 | (code_point >> 12));
        out[1] = (char) (0x80 | ((code_point >> 6) & 0x3F));
        out[2] = (char) (0x80 | (code_point & 0x3F));
        *length = 3;
    } else {
        out[0] = (char) (0xF0 | (code_point >> 18));
        out[1] = (char) (0x80 | ((code_point >> 12) & 0x3F));
        out[2] = (char) (0x80 | ((code_point >> 6) & 0x3F));
        out[3] = (char) (0x80 | (code_point & 0x3F));
        *length = 4;
    }
    return OK;
}

static char *copy_string_bytes(CSOUND *csound, const char *bytes, size_t length) {
    char *item = csound->Malloc(csound, length + 1);
    if (item == NULL) return NULL;
    if (length > 0) memcpy(item, bytes, length);
    item[length] = '\0';
    return item;
}

/* Every item is a uint32 byte count and its bytes; a count that runs past the
   payload the header declares is refused before anything is allocated. */
static CSN_FILE_ERROR_CODE csn_load_strings(CSOUND *csound, FILE *fptr, uint32_t *ndim, uint32_t *shape, char **items, size_t size) {
    CSN_FILE_HEADER header = {0};
    CSN_FILE_ERROR_CODE res = csnfile_read_header(fptr, &header);
    if (res != CSN_FILE_NO_ERROR) return res;
    if (header.dtype != CSN_DTYPE_STR) return CSN_FILE_DTYPE_KIND_MISMATCH;
    if (header.size != size) return CSN_FILE_SIZE_MISMATCH;

    uint64_t consumed = 0;
    for (size_t i = 0; i < size; i++) {
        uint32_t length = 0;
        if (fread(&length, sizeof(uint32_t), 1, fptr) != 1) return CSN_FILE_INVALID_HEADER_DATA_RAW;
        consumed += sizeof(uint32_t);
        if (consumed > header.data_bytes || length > header.data_bytes - consumed) return CSN_FILE_SIZE_MISMATCH;
        consumed += length;
        char *item = csound->Malloc(csound, (size_t) length + 1);
        if (item == NULL) return CSN_FILE_WRONG_MEMORY_ALLOCATION;
        items[i] = item;
        if (length > 0 && fread(item, 1, length, fptr) != length) return CSN_FILE_INVALID_HEADER_DATA_RAW;
        item[length] = '\0';
    }
    if (consumed != header.data_bytes) return CSN_FILE_SIZE_MISMATCH;

    *ndim = header.dim;
    memcpy(shape, header.shape, sizeof(uint32_t) * CSN_MAX_DIMS);
    return CSN_FILE_NO_ERROR;
}

/* <U items are UCS4 code points, re-encoded as UTF-8; |S items are bytes,
   taken as they are. numpy pads an item with zeros, and a C string ends at
   the first one, so that is where an item stops. */
static CSN_FILE_ERROR_CODE numpy_load_strings(CSOUND *csound, FILE *fptr, const CSN_FILE_NUMPY_HEADER *header, char **items) {
    bool is_unicode = header->descr.kind == 'U';
    size_t width = header->descr.item_size;
    size_t item_bytes = is_unicode ? width * 4 : width;

    const uint16_t endian_probe = 1;
    bool little_endian = header->descr.endian == '<' ||
        (header->descr.endian == '=' && *(const uint8_t *) &endian_probe == 1);

    size_t c_strides[CSN_MAX_DIMS] = {0};
    c_strides[header->dim - 1] = 1;
    for (uint32_t axis = header->dim - 1; axis-- > 0; )
        c_strides[axis] = c_strides[axis + 1] * header->shape[axis + 1];

    /* UTF-8 takes at most 4 bytes per code point, as UCS4 does */
    uint8_t *raw = csound->Malloc(csound, item_bytes + 1);
    char *text = csound->Malloc(csound, width * 4 + 1);
    CSN_FILE_ERROR_CODE res = CSN_FILE_NO_ERROR;
    if (raw == NULL || text == NULL) {
        res = CSN_FILE_WRONG_MEMORY_ALLOCATION;
        goto done;
    }

    for (size_t j = 0; j < header->size; j++) {
        if (item_bytes > 0 && fread(raw, 1, item_bytes, fptr) != item_bytes) {
            res = CSN_FILE_INVALID_HEADER_DATA_RAW;
            goto done;
        }
        size_t length = 0;
        if (is_unicode) {
            for (size_t k = 0; k < width; k++) {
                uint32_t code_point = (uint32_t) numpy_uint(raw + k * 4, 4, little_endian);
                if (code_point == 0) break;
                size_t encoded = 0;
                if (utf8_encode(code_point, text + length, &encoded) != OK) {
                    res = CSN_FILE_INVALID_HEADER_DATA_RAW;
                    goto done;
                }
                length += encoded;
            }
        } else {
            while (length < width && raw[length] != 0) {
                text[length] = (char) raw[length];
                length++;
            }
        }
        size_t dest = header->fortran_order
            ? numpy_c_offset(j, header->dim, header->shape, c_strides)
            : j;
        items[dest] = copy_string_bytes(csound, text, length);
        if (items[dest] == NULL) {
            res = CSN_FILE_WRONG_MEMORY_ALLOCATION;
            goto done;
        }
    }
    if (fgetc(fptr) != EOF || ferror(fptr)) res = CSN_FILE_SIZE_MISMATCH;

done:
    if (raw != NULL) csound->Free(csound, raw);
    if (text != NULL) csound->Free(csound, text);
    return res;
}

CSN_FILE_ERROR_CODE csnfile_load_strings(CSOUND *csound, const char *path, const char *ext, uint32_t *ndim, uint32_t *shape, char ***items, size_t *size) {
    *items = NULL;
    *size = 0;
    bool is_numpy = strcmp(ext, CSN_FILE_NPY_EXT) == 0;
    if (!is_numpy && strcmp(ext, CSN_FILE_EXT) != 0) return CSN_FILE_INVALID_HEADER_MAGIC;

    FILE *fptr = fopen(path, "rb");
    if (fptr == NULL) return CSN_FILE_NOT_FOUND;

    /* the header is read twice: once here for the item count the list is
       sized by, once by the loader that walks the payload after it */
    size_t count = 0;
    CSN_FILE_ERROR_CODE res = CSN_FILE_NO_ERROR;
    CSN_FILE_NUMPY_HEADER numpy_header = {0};
    if (is_numpy) {
        res = numpy_read_header(fptr, &numpy_header);
        if (res == CSN_FILE_NO_ERROR && numpy_header.descr.itype != CSN_STRING) res = CSN_FILE_DTYPE_KIND_MISMATCH;
        count = numpy_header.size;
    } else {
        CSN_FILE_HEADER header = {0};
        res = csnfile_read_header(fptr, &header);
        if (res == CSN_FILE_NO_ERROR && header.dtype != CSN_DTYPE_STR) res = CSN_FILE_DTYPE_KIND_MISMATCH;
        count = (size_t) header.size;
        if (res == CSN_FILE_NO_ERROR) rewind(fptr);
    }
    if (res != CSN_FILE_NO_ERROR) goto done;

    char **list = csound->Calloc(csound, sizeof(char *) * (count > 0 ? count : 1));
    if (list == NULL) {
        res = CSN_FILE_WRONG_MEMORY_ALLOCATION;
        goto done;
    }

    if (is_numpy) {
        res = numpy_load_strings(csound, fptr, &numpy_header, list);
        *ndim = numpy_header.dim;
        memcpy(shape, numpy_header.shape, sizeof(uint32_t) * CSN_MAX_DIMS);
    } else {
        res = csn_load_strings(csound, fptr, ndim, shape, list, count);
    }

    if (res != CSN_FILE_NO_ERROR) {
        free_string_items(csound, list, count);
        goto done;
    }
    *items = list;
    *size = count;

done:
    fclose(fptr);
    return res;
}
