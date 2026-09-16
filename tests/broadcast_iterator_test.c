#include "csnum_internal.h"
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>

typedef int32_t (*broadcast_init_fn)(uint32_t *, uint32_t *, size_t *, size_t [CSN_MAX_BROADCAST_INPUTS][CSN_MAX_DIMS], const CSN_ARRAY *const [], uint32_t);
typedef int32_t (*broadcast_iter_init_fn)(CSN_BROADCAST_ITER *, uint32_t, const uint32_t *, size_t, const size_t [CSN_MAX_BROADCAST_INPUTS][CSN_MAX_DIMS], uint32_t);
typedef bool (*broadcast_iter_next_fn)(CSN_BROADCAST_ITER *);
typedef int32_t (*nd_iter_init_fn)(CSN_BROADCAST_ITER *, uint32_t, const uint32_t *, size_t, const size_t *);
typedef int32_t (*nd_iter_seek_fn)(CSN_BROADCAST_ITER *, size_t);
typedef int32_t (*axis_slice_init_fn)(CSN_AXIS_SLICE_ITER *, const CSN_ARRAY *, const CSN_ARRAY *, uint32_t);
typedef bool (*axis_slice_next_fn)(CSN_AXIS_SLICE_ITER *);

static broadcast_init_fn broadcast_init_under_test;
static broadcast_iter_init_fn broadcast_iter_init_under_test;
static broadcast_iter_next_fn broadcast_iter_next_under_test;
static nd_iter_init_fn nd_iter_init_under_test;
static nd_iter_seek_fn nd_iter_seek_under_test;
static axis_slice_init_fn axis_slice_init_under_test;
static axis_slice_next_fn axis_slice_next_under_test;
#define BROADCAST_INIT broadcast_init_under_test
#define BROADCAST_ITER_INIT broadcast_iter_init_under_test
#define BROADCAST_ITER_NEXT broadcast_iter_next_under_test
#define ND_ITER_INIT nd_iter_init_under_test
#define ND_ITER_SEEK nd_iter_seek_under_test
#define AXIS_ITER_SLICE_INIT axis_slice_init_under_test
#define AXIS_SLICE_ITER_NEXT axis_slice_next_under_test

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expr); \
        return 1; \
    } \
} while (0)

static int test_offsets(void) {
    CSN_ARRAY a = { .ndim = 2, .shape = {2, 1}, .strides = {1, 1}, .size = 2 };
    CSN_ARRAY b = { .ndim = 2, .shape = {1, 3}, .strides = {3, 1}, .size = 3 };
    CSN_ARRAY c = { .ndim = 1, .shape = {3}, .strides = {1}, .size = 3 };
    const CSN_ARRAY *sources[] = {&a, &b, &c};
    uint32_t ndim = 0, shape[CSN_MAX_DIMS] = {0};
    size_t size = 0, strides[CSN_MAX_BROADCAST_INPUTS][CSN_MAX_DIMS] = {{0}};
    CHECK(BROADCAST_INIT(&ndim, shape, &size, strides, sources, 3) == OK);
    CHECK(ndim == 2 && shape[0] == 2 && shape[1] == 3 && size == 6);
    CHECK(strides[0][0] == 1 && strides[0][1] == 0);
    CHECK(strides[1][0] == 0 && strides[1][1] == 1);
    CHECK(strides[2][0] == 0 && strides[2][1] == 1);

    CSN_BROADCAST_ITER it;
    CHECK(BROADCAST_ITER_INIT(&it, ndim, shape, size, strides, 3) == OK);
    const size_t expected_a[] = {0, 0, 0, 1, 1, 1};
    const size_t expected_b[] = {0, 1, 2, 0, 1, 2};
    size_t index = 0;
    while (BROADCAST_ITER_NEXT(&it)) {
        CHECK(index < size);
        CHECK(it.offsets[0] == expected_a[index]);
        CHECK(it.offsets[1] == expected_b[index]);
        CHECK(it.offsets[2] == expected_b[index]);
        CHECK(it.coords[0] == index / 3 && it.coords[1] == index % 3);
        ++index;
    }
    CHECK(index == size);
    CHECK(!BROADCAST_ITER_NEXT(&it));
    return 0;
}

static int test_single_and_empty(void) {
    CSN_ARRAY one = { .ndim = 1, .shape = {1}, .strides = {1}, .size = 1 };
    const CSN_ARRAY *single[] = {&one};
    uint32_t ndim = 0, shape[CSN_MAX_DIMS] = {0};
    size_t size = 0, strides[CSN_MAX_BROADCAST_INPUTS][CSN_MAX_DIMS] = {{0}};
    CSN_BROADCAST_ITER it;
    CHECK(BROADCAST_INIT(&ndim, shape, &size, strides, single, 1) == OK);
    CHECK(BROADCAST_ITER_INIT(&it, ndim, shape, size, strides, 1) == OK);
    CHECK(BROADCAST_ITER_NEXT(&it));
    CHECK(it.offsets[0] == 0 && it.coords[0] == 0);
    CHECK(!BROADCAST_ITER_NEXT(&it));

    CSN_ARRAY zero = { .ndim = 2, .shape = {0, 3}, .strides = {3, 1}, .size = 0 };
    CSN_ARRAY row = { .ndim = 2, .shape = {1, 3}, .strides = {3, 1}, .size = 3 };
    const CSN_ARRAY *zero_sources[] = {&zero, &row};
    CHECK(BROADCAST_INIT(&ndim, shape, &size, strides, zero_sources, 2) == OK);
    CHECK(ndim == 2 && shape[0] == 0 && shape[1] == 3 && size == 0);
    CHECK(BROADCAST_ITER_INIT(&it, ndim, shape, size, strides, 2) == OK);
    CHECK(!BROADCAST_ITER_NEXT(&it));

    zero.shape[0] = 2; /* Reserved shape, but no logical elements. */
    const CSN_ARRAY *empty_sources[] = {&zero, &row};
    CHECK(BROADCAST_INIT(&ndim, shape, &size, strides, empty_sources, 2) == OK);
    CHECK(shape[0] == 2 && shape[1] == 3 && size == 0);
    CHECK(BROADCAST_ITER_INIT(&it, ndim, shape, size, strides, 2) == OK);
    CHECK(!BROADCAST_ITER_NEXT(&it));
    return 0;
}

static int test_invalid_plans(void) {
    CSN_ARRAY a = { .ndim = 1, .shape = {2}, .strides = {1}, .size = 2 };
    CSN_ARRAY b = { .ndim = 1, .shape = {3}, .strides = {1}, .size = 3 };
    const CSN_ARRAY *sources[] = {&a, &b};
    uint32_t ndim = 99, shape[CSN_MAX_DIMS] = {0};
    size_t size = 99, strides[CSN_MAX_BROADCAST_INPUTS][CSN_MAX_DIMS] = {{0}};
    CHECK(BROADCAST_INIT(&ndim, shape, &size, strides, sources, 2) == NOTOK);
    CHECK(ndim == 99 && size == 99);

    a.ndim = CSN_MAX_DIMS + 1;
    CHECK(BROADCAST_INIT(&ndim, shape, &size, strides, sources, 2) == NOTOK);
    a.ndim = 2;
    a.shape[0] = 16385; a.shape[1] = 1; a.size = 16385;
    b.ndim = 2;
    b.shape[0] = 1; b.shape[1] = 16385; b.size = 16385;
    CHECK(BROADCAST_INIT(&ndim, shape, &size, strides, sources, 2) == NOTOK);

    CSN_BROADCAST_ITER it;
    uint32_t small_shape[] = {2};
    CHECK(BROADCAST_ITER_INIT(&it, 1, small_shape, 3, strides, 1) == NOTOK);
    CHECK(BROADCAST_ITER_INIT(&it, 1, small_shape, 2, strides, 0) == NOTOK);
    return 0;
}

static int test_axis_slices(void) {
    CSN_ARRAY src = { .ndim = 2, .shape = {2, 3}, .strides = {3, 1}, .size = 6 };
    CSN_ARRAY dst = { .ndim = 2, .shape = {2, 4}, .strides = {4, 1}, .size = 8 };
    CSN_AXIS_SLICE_ITER it;
    CHECK(AXIS_ITER_SLICE_INIT(&it, &src, &dst, 1) == OK);
    CHECK(it.slice_count == 2 && it.axis_size == 3);
    CHECK(it.src_axis_stride == 1 && it.dst_axis_stride == 1);
    CHECK(AXIS_SLICE_ITER_NEXT(&it) && it.src_base == 0 && it.dst_base == 0);
    CHECK(AXIS_SLICE_ITER_NEXT(&it) && it.src_base == 3 && it.dst_base == 4);
    CHECK(!AXIS_SLICE_ITER_NEXT(&it));

    dst.shape[0] = 4; dst.shape[1] = 3;
    dst.strides[0] = 3; dst.strides[1] = 1;
    CHECK(AXIS_ITER_SLICE_INIT(&it, &src, &dst, 0) == OK);
    CHECK(it.slice_count == 3 && it.axis_size == 2);
    for (size_t i = 0; i < 3; ++i) {
        CHECK(AXIS_SLICE_ITER_NEXT(&it));
        CHECK(it.src_base == i && it.dst_base == i);
    }
    CHECK(!AXIS_SLICE_ITER_NEXT(&it));

    dst.ndim = 1; dst.shape[0] = 3; dst.shape[1] = 0;
    dst.strides[0] = 1; dst.strides[1] = 0;
    CHECK(AXIS_ITER_SLICE_INIT(&it, &src, &dst, 0) == OK);
    CHECK(it.slice_count == 3 && it.dst_axis_stride == 0);
    for (size_t i = 0; i < 3; ++i) {
        CHECK(AXIS_SLICE_ITER_NEXT(&it));
        CHECK(it.src_base == i && it.dst_base == i);
    }
    CHECK(!AXIS_SLICE_ITER_NEXT(&it));

    dst.shape[0] = 2;
    CHECK(AXIS_ITER_SLICE_INIT(&it, &src, &dst, 1) == OK);
    CHECK(it.slice_count == 2 && it.axis_size == 3);
    CHECK(AXIS_SLICE_ITER_NEXT(&it) && it.src_base == 0 && it.dst_base == 0);
    CHECK(AXIS_SLICE_ITER_NEXT(&it) && it.src_base == 3 && it.dst_base == 1);
    CHECK(!AXIS_SLICE_ITER_NEXT(&it));

    dst.shape[0] = 4;
    CHECK(AXIS_ITER_SLICE_INIT(&it, &src, &dst, 0) == NOTOK);
    CHECK(AXIS_ITER_SLICE_INIT(&it, &src, &dst, src.ndim) == NOTOK);
    CHECK(AXIS_ITER_SLICE_INIT(NULL, &src, &dst, 0) == NOTOK);
    CHECK(AXIS_ITER_SLICE_INIT(&it, NULL, &dst, 0) == NOTOK);
    CHECK(AXIS_ITER_SLICE_INIT(&it, &src, NULL, 0) == NOTOK);

    dst.ndim = 2; dst.shape[0] = 2; dst.shape[1] = 4;
    dst.strides[0] = 4; dst.strides[1] = 1; dst.size = 8;
    src.size = 0; /* Reserved shape, but no logical elements. */
    CHECK(AXIS_ITER_SLICE_INIT(&it, &src, &dst, 1) == OK);
    CHECK(it.slice_count == 0 && !AXIS_SLICE_ITER_NEXT(&it));
    src.size = 6; dst.size = 0;
    CHECK(AXIS_ITER_SLICE_INIT(&it, &src, &dst, 1) == OK);
    CHECK(it.slice_count == 0 && !AXIS_SLICE_ITER_NEXT(&it));
    return 0;
}

static int test_nd_coords(void) {
    uint32_t shape[2] = {2, 3};
    size_t strides[2] = {3, 1};
    CSN_BROADCAST_ITER it;
    CHECK(ND_ITER_INIT(&it, 2, shape, 6, strides) == OK);
    for (size_t i = 0; i < 6; ++i) {
        CHECK(BROADCAST_ITER_NEXT(&it));
        CHECK(it.linear_index == i && it.coords[0] == i / 3 && it.coords[1] == i % 3);
        CHECK(it.offsets[0] == i);
    }
    CHECK(!BROADCAST_ITER_NEXT(&it));
    CHECK(ND_ITER_INIT(&it, 2, shape, 0, NULL) == OK);
    CHECK(!BROADCAST_ITER_NEXT(&it));
    CHECK(ND_ITER_SEEK(&it, 0) == NOTOK);
    CHECK(ND_ITER_INIT(&it, 2, shape, 6, strides) == OK);
    CHECK(ND_ITER_SEEK(&it, 4) == OK);
    CHECK(it.linear_index == 4 && it.coords[0] == 1 && it.coords[1] == 1 && it.offsets[0] == 4);
    CHECK(BROADCAST_ITER_NEXT(&it));
    CHECK(it.linear_index == 5 && it.coords[0] == 1 && it.coords[1] == 2 && it.offsets[0] == 5);
    CHECK(!BROADCAST_ITER_NEXT(&it));
    CHECK(ND_ITER_SEEK(&it, 6) == NOTOK);
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 2) return 1;
    void *plugin = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (plugin == NULL) {
        fprintf(stderr, "dlopen: %s\n", dlerror());
        return 1;
    }
    void *symbol = dlsym(plugin, "BROADCAST_INIT");
    memcpy(&broadcast_init_under_test, &symbol, sizeof(symbol));
    symbol = dlsym(plugin, "BROADCAST_ITER_INIT");
    memcpy(&broadcast_iter_init_under_test, &symbol, sizeof(symbol));
    symbol = dlsym(plugin, "BROADCAST_ITER_NEXT");
    memcpy(&broadcast_iter_next_under_test, &symbol, sizeof(symbol));
    symbol = dlsym(plugin, "ND_ITER_INIT");
    memcpy(&nd_iter_init_under_test, &symbol, sizeof(symbol));
    symbol = dlsym(plugin, "ND_ITER_SEEK");
    memcpy(&nd_iter_seek_under_test, &symbol, sizeof(symbol));
    symbol = dlsym(plugin, "AXIS_ITER_SLICE_INIT");
    memcpy(&axis_slice_init_under_test, &symbol, sizeof(symbol));
    symbol = dlsym(plugin, "AXIS_SLICE_ITER_NEXT");
    memcpy(&axis_slice_next_under_test, &symbol, sizeof(symbol));
    if (broadcast_init_under_test == NULL || broadcast_iter_init_under_test == NULL || broadcast_iter_next_under_test == NULL || nd_iter_init_under_test == NULL || nd_iter_seek_under_test == NULL
        || axis_slice_init_under_test == NULL || axis_slice_next_under_test == NULL) {
        fprintf(stderr, "broadcast iterator symbols not found\n");
        dlclose(plugin);
        return 1;
    }
    if (test_offsets() != 0) return 1;
    if (test_single_and_empty() != 0) return 1;
    if (test_invalid_plans() != 0) return 1;
    if (test_axis_slices() != 0) return 1;
    if (test_nd_coords() != 0) return 1;
    dlclose(plugin);
    return 0;
}
