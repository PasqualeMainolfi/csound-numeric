#ifndef __CSN_FILE
#define __CSN_FILE

#include "csnregistry.h"
#include <stdint.h>

#define CSN_FILE_EXT ".csn"
#define CSN_MAGIC "CSDN"
#define CSN_FILE_VERSION_MAJOR 1
#define CSN_FILE_VERSION_MINOR 0
#define CSN_ERROR_MESSAGE_SIZE 128

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

CSN_FILE_ERROR_CODE csnfile_save_array_to_file(CSN_ARRAY *arr, const char *path);
CSN_FILE_ERROR_CODE csnfile_load_array_from_file(CSOUND *csound, CSN_FILE_HEADER *header, double **data, size_t *data_capacity, const char *path);
void csnfile_dispatch_error(const char **error_message, CSN_FILE_ERROR_CODE error_code);


#endif
