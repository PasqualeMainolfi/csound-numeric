#include "csnfile.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; \
} } while (0)

int32_t get_array_size_from_shape(size_t *size, uint32_t ndim, const uint32_t *shape) {
    if (ndim == 0 || ndim > CSN_MAX_DIMS) return NOTOK;
    size_t product = 1;
    for (uint32_t i = 0; i < ndim; ++i) {
        if (shape[i] && product > CSN_MAX_ELEMS / shape[i]) return NOTOK;
        product *= shape[i];
    }
    *size = product;
    return OK;
}

static void *test_malloc(CSOUND *csound, size_t size) { (void) csound; return malloc(size); }
static void *test_realloc(CSOUND *csound, void *ptr, size_t size) { (void) csound; return realloc(ptr, size); }

static int write_fixture(const char *path, uint8_t version, const char *descr,
                         bool fortran, uint32_t rows, uint32_t cols,
                         const uint8_t *raw, size_t raw_size) {
    char dict[256];
    int n = snprintf(dict, sizeof(dict),
        "{'descr': '%s', 'fortran_order': %s, 'shape': (%u, %u), }",
        descr, fortran ? "True" : "False", rows, cols);
    if (n < 0 || (size_t) n >= sizeof(dict)) return 1;
    size_t prefix = version == 1 ? 10 : 12;
    size_t padding = (16 - (prefix + (size_t) n + 1) % 16) % 16;
    size_t hlen = (size_t) n + padding + 1;
    FILE *file = fopen(path, "wb");
    if (!file) return 1;
    uint8_t lead[12] = {0x93, 'N', 'U', 'M', 'P', 'Y', version, 0,
        (uint8_t) hlen, (uint8_t) (hlen >> 8), (uint8_t) (hlen >> 16), (uint8_t) (hlen >> 24)};
    int failed = fwrite(lead, 1, prefix, file) != prefix ||
        fwrite(dict, 1, (size_t) n, file) != (size_t) n;
    for (size_t i = 0; i < padding; ++i) failed |= fputc(' ', file) == EOF;
    failed |= fputc('\n', file) == EOF;
    failed |= fwrite(raw, 1, raw_size, file) != raw_size;
    failed |= fclose(file) != 0;
    return failed;
}

int main(int argc, char **argv) {
    CHECK(argc == 2 || argc == 3);
    const char *path = argv[1];
    CSOUND csound = {0};
    csound.Malloc = test_malloc;
    csound.ReAlloc = test_realloc;
    CSN_FILE_NUMPY_HEADER header = {0};
    double *data = NULL;
    size_t capacity = 0;

    if (argc == 3 && strcmp(argv[2], "--read-numpy") == 0) {
        CHECK(csnfile_load_array_from_numpy_file(&csound, &header, &data, &capacity, path) == CSN_FILE_NO_ERROR);
        CHECK(header.fortran_order && header.descr.kind == 'f' && header.descr.endian == '>');
        CHECK(header.dim == 2 && header.shape[0] == 2 && header.shape[1] == 3);
        const double expected[] = {1.5, -2.0, 3.0, 4.0, 5.0, 6.0};
        for (size_t i = 0; i < 6; ++i) CHECK(data[i] == expected[i]);
        free(data);
        return 0;
    }
    if (argc == 3 && strcmp(argv[2], "--write-numpy") == 0) {
        double values[4] = {1.0, 2.0, 3.0, 4.0};
        CSN_ARRAY arr = {.data = values, .size = 4, .ndim = 2, .itype = CSN_REAL,
                         .shape = {2, 2}};
        return csnfile_save_array_to_numpy_file(&arr, path) == CSN_FILE_NO_ERROR ? 0 : 1;
    }
    if (argc == 3 && strcmp(argv[2], "--write-f4") == 0) {
        const uint8_t floats[] = {0,0,0x80,0x3f, 0,0,0,0x40, 0,0,0x40,0x40,
                                  0,0,0x80,0x40, 0,0,0xa0,0x40, 0,0,0xc0,0x40};
        return write_fixture(path, 1, "<f4", false, 2, 3, floats, sizeof(floats));
    }
    CHECK(argc == 2);

    const uint8_t fortran_i2[] = {0,1, 0,4, 0,2, 0,5, 0,3, 0,6};
    CHECK(write_fixture(path, 1, ">i2", true, 2, 3, fortran_i2, sizeof(fortran_i2)) == 0);
    CHECK(csnfile_load_array_from_numpy_file(&csound, &header, &data, &capacity, path) == CSN_FILE_NO_ERROR);
    CHECK(header.dim == 2 && header.shape[0] == 2 && header.shape[1] == 3);
    CHECK(header.size == 6 && header.fortran_order && header.descr.item_size == 2);
    for (size_t i = 0; i < 6; ++i) CHECK(data[i] == (double) (i + 1));

    const uint8_t negative_i8[] = {0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfe};
    CHECK(write_fixture(path, 1, ">i8", false, 1, 1, negative_i8, sizeof(negative_i8)) == 0);
    CHECK(csnfile_load_array_from_numpy_file(&csound, &header, &data, &capacity, path) == CSN_FILE_NO_ERROR);
    CHECK(data[0] == -2.0);

    CHECK(write_fixture(path, 1, "<u1", false, 0, 3, negative_i8, 0) == 0);
    CHECK(csnfile_load_array_from_numpy_file(&csound, &header, &data, &capacity, path) == CSN_FILE_NO_ERROR);
    CHECK(header.size == 0 && header.shape[0] == 0);

    const uint8_t complex_f4[] = {0,0,0x80,0x3f, 0,0,0x20,0xc0,
                                  0,0,0x40,0x40, 0,0,0x80,0x40};
    CHECK(write_fixture(path, 2, "<c8", false, 1, 2, complex_f4, sizeof(complex_f4)) == 0);
    CHECK(csnfile_load_array_from_numpy_file(&csound, &header, &data, &capacity, path) == CSN_FILE_NO_ERROR);
    CHECK(header.descr.itype == CSN_COMPLEX && capacity >= 4);
    CHECK(data[0] == 1.0 && data[1] == -2.5 && data[2] == 3.0 && data[3] == 4.0);

    const uint8_t half[] = {0x00,0x3c, 0x00,0xbc, 0x01,0x00};
    CHECK(write_fixture(path, 3, "<f2", false, 1, 3, half, sizeof(half)) == 0);
    CHECK(csnfile_load_array_from_numpy_file(&csound, &header, &data, &capacity, path) == CSN_FILE_NO_ERROR);
    CHECK(data[0] == 1.0 && data[1] == -1.0 && data[2] == 1.0 / 16777216.0);

    const uint8_t booleans[] = {0, 1, 2};
    CHECK(write_fixture(path, 1, "|b1", false, 1, 3, booleans, sizeof(booleans)) == 0);
    CHECK(csnfile_load_array_from_numpy_file(&csound, &header, &data, &capacity, path) == CSN_FILE_NO_ERROR);
    CHECK(data[0] == 0.0 && data[1] == 1.0 && data[2] == 1.0);

    CHECK(write_fixture(path, 1, "<f16", false, 1, 3, booleans, sizeof(booleans)) == 0);
    CHECK(csnfile_load_array_from_numpy_file(&csound, &header, &data, &capacity, path) == CSN_FILE_INVALID_HEADER_DTYPE);
    CHECK(write_fixture(path, 1, "<f8", false, 1, 3, booleans, sizeof(booleans)) == 0);
    CHECK(csnfile_load_array_from_numpy_file(&csound, &header, &data, &capacity, path) == CSN_FILE_INVALID_HEADER_DATA_RAW);

    uint32_t shape[2] = {2, 2};
    double values[4] = {1.0, 2.0, 3.0, 4.0};
    CSN_ARRAY arr = {.data = values, .size = 4, .ndim = 2, .itype = CSN_REAL};
    memcpy(arr.shape, shape, sizeof(shape));
    CHECK(csnfile_save_array_to_numpy_file(&arr, path) == CSN_FILE_NO_ERROR);
    CHECK(csnfile_load_array_from_numpy_file(&csound, &header, &data, &capacity, path) == CSN_FILE_NO_ERROR);
    CHECK(!header.fortran_order && header.size == 4);
    for (size_t i = 0; i < 4; ++i) CHECK(data[i] == values[i]);

    free(data);
    CHECK(remove(path) == 0);
    return 0;
}
