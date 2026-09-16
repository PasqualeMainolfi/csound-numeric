#ifndef __CSN_FILE
#define __CSN_FILE

#include "csnregistry.h"
#include <stddef.h>
#include <stdint.h>

#define CSN_FILE_EXT ".csn"
#define CSN_FILE_NPY_EXT ".npy"
#define CSN_MAGIC_SIZE 4
#define CSN_MAGIC_NUMPY_SIZE 6
#define CSN_NUMPY_MAX_HEADER_SIZE 10000
#define CSN_NUMPY_MAGIC "\x93NUMPY"
#define CSN_MAGIC "CSDN"
#define CSN_FILE_VERSION_MAJOR 1
#define CSN_FILE_VERSION_MINOR 0
#define CSN_FILE_NUMPY_VERSION_MAJOR 3
#define CSN_FILE_NUMPY_VERSION_MINOR 0
#define CSN_ERROR_MESSAGE_SIZE 128
#define CSN_NUMPY_DOUBLE "<f8"
#define CSN_NUMPY_COMPLEX "<c16"


typedef enum {
    CSN_FILE_NO_ERROR = 0,
    CSN_FILE_NOT_FOUND,
    CSN_FILE_INVALID_HEADER_MAGIC,
    CSN_FILE_INVALID_HEADER_VMAJOR,
    CSN_FILE_INVALID_HEADER_VMINOR,
    CSN_FILE_INVALID_HEADER_DTYPE,
    CSN_FILE_INVALID_HEADER_DIM,
    CSN_FILE_INVALID_HEADER_SHAPE,
    CSN_FILE_INVALID_HEADER_DATA_BYTES,
    CSN_FILE_INVALID_HEADER_DATA_RAW,
    CSN_FILE_INVALID_SHAPE_SIZE,
    CSN_FILE_INVALID_HEADER_SIZE,
    CSN_FILE_EXCEEDS_SIZE_LIMIT,
    CSN_FILE_SIZE_MISMATCH,
    CSN_FILE_WRONG_MEMORY_ALLOCATION,
} CSN_FILE_ERROR_CODE;

typedef enum {
    CSN_DTYPE_F64 = 1,
    CSN_DTYPE_C128
} CSN_DTYPE;

typedef struct {
    uint8_t magic[4];             // 4 bytes
    uint16_t major;               // 2 bytes
    uint16_t minor;               // 2 bytes
    uint32_t dtype;               // 4 bytes
    uint32_t dim;                 // 4 bytes
    uint64_t size;                // 8 bytes
    uint32_t shape[CSN_MAX_DIMS]; // 32 bytes
    uint64_t data_bytes;          // 8 bytes
} CSN_FILE_HEADER;

typedef struct {
    ITEM_TYPE itype;
    char kind;
    char endian;
    uint32_t item_size;
} CSN_NUMPY_DESCR;

typedef struct {
    uint8_t magic[CSN_MAGIC_NUMPY_SIZE];
    uint8_t major;
    uint8_t minor;
    uint32_t header_length;
    char dict[CSN_NUMPY_MAX_HEADER_SIZE + 1];
    uint64_t data_bytes;
    size_t padding_len;
    uint32_t dim;
    uint32_t shape[CSN_MAX_DIMS];
    size_t size;
    CSN_NUMPY_DESCR descr;
    bool fortran_order;
} CSN_FILE_NUMPY_HEADER;

CSN_FILE_ERROR_CODE csnfile_save_array_to_file(CSN_ARRAY *arr, const char *path);
CSN_FILE_ERROR_CODE csnfile_load_array_from_file(CSOUND *csound, CSN_FILE_HEADER *header, double **data, size_t *data_capacity, const char *path);
void csnfile_dispatch_error(const char **error_message, CSN_FILE_ERROR_CODE error_code);


#define CSN_PRINT_BUFFER_INITIAL_CAPACITY 4096
#define CSN_PRINT_THRESHOLD 1000
#define CSN_PRINT_EDGE_ITEMS 3
#define CSN_PRINT_SUMMARIZE_SIMBOL "..."

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} CSN_PRINT_BUFFER;

int32_t csnfile_show_array(CSOUND *csound, CSN_PRINT_BUFFER *buffer, const CSN_ARRAY *arr);

CSN_FILE_ERROR_CODE csnfile_save_array_to_numpy_file(CSN_ARRAY *arr, const char *path);
CSN_FILE_ERROR_CODE csnfile_load_array_from_numpy_file(CSOUND *csound, CSN_FILE_NUMPY_HEADER *header, double **data, size_t *data_capacity, const char *path);

CSN_FILE_ERROR_CODE csnfile_save_array(CSN_ARRAY *arr, const char *path, const char *ext);

#endif
