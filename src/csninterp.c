/* Opcode implementations for the interp family.
   The public opcode inventory remains centralized in csnum.c. */
#include "csnum_internal.h"
#include "csnregistry.h"
#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int32_t from_ftable_to_csnarray(CSOUND *csound, CSN_FROM_FTABLE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    /* FTFind names the offending table number itself and counts the init
       error, so reporting again here would delete the note with two errors
       for one mistake. */
    FUNC *ftable = csound->FTFind(csound, p->ftable);
    if (ftable == NULL) {
        return NOTOK;
    }

    /* flen counts the data; the guard point past it is not part of the table. */
    uint32_t size = ftable->flen;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);
    uint32_t shape[CSN_MAX_DIMS] = {0};
    shape[0] = size;

    if (create_csnarray_locked(csound, reg, &p->h, 1U, shape, &p->array, p->handle, NULL, 0, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    CSN_ARRAY *arr = p->array;
    for (uint32_t i = 0; i < size; i++) {
        arr->data[i] = (double) ftable->ftable[i];
    }

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t from_csnarray_to_ftable(CSOUND *csound, CSN_TO_FTABLE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = (uint32_t) p->source_handle->id;

    if (!IS_VALID_ZERO_ONE((double) *p->resize)) {
        return csound->InitError(csound, "[csnarray] Resize param must be 0 = do not resize or 1 = resize");
    }

    uint32_t resize = (uint32_t) *p->resize;

    /* FTFind maps both 0 and -1 to the global sine table, so an unchecked
       number here would silently overwrite the sine every oscillator reads. */
    int32_t fno = (int32_t) MYFLT2LRND(*p->ftable);
    if (fno <= 0) {
        return csound->InitError(csound, "[csnarray] Invalid ftable number %d: it must be greater than 0", fno);
    }

    FUNC *ftable = NULL;
    int32_t res = OK;

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
        goto done;
    }
    CSN_ARRAY *source_arr = slot->array;
    if (source_arr->itype == CSN_COMPLEX) {
        res = csound->InitError(csound, "[csnarray] csntoftable requires real array");
        goto done;
    }

    size_t size = source_arr->size;
    if (size == 0) {
        res = csound->InitError(csound, "[csnarray] Array %u is empty: there is nothing to write into ftable %d", source_handle, fno);
        goto done;
    }
    if (size > (size_t) INT32_MAX) {
        res = csound->InitError(csound, "[csnarray] Array %u has %zu points: too many for an ftable", source_handle, size);
        goto done;
    }

    if (resize == 0) {
        ftable = csound->FTFind(csound, p->ftable);
        if (ftable == NULL) {
            res = NOTOK;
            goto done;
        }

        if ((size_t) ftable->flen < size) {
            res = csound->InitError(csound, "[csnarray] Ftable %d holds %u points but the array has %zu: the ftable length must be equal or greater", fno, (uint32_t) ftable->flen, size);
            goto done;
        }
    } else {
        if (csound->FTAlloc(csound, fno, (int32_t) size) != 0) {
            res = csound->InitError(csound, "[csnarray] Cannot size ftable %d to %zu points", fno, size);
            goto done;
        }

        ftable = csound->FTFind(csound, p->ftable);
        if (ftable == NULL) {
            res = NOTOK;
            goto done;
        }
        ftable->gen01args.sample_rate = ftable->sr;
    }

    for (size_t i = 0; i < size; i++) {
        ftable->ftable[i] = (MYFLT) source_arr->data[i];
    }

    /* The guard point past flen is allocated but never initialised. Write it
       only when the array reaches the end of the table, wrapping to the first
       point the way the cyclic gens do; a shorter array leaves the tail, and
       the guard that belongs to it, untouched. */
    if ((size_t) ftable->flen == size) {
        ftable->ftable[size] = ftable->ftable[0];
    }

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

/*
 * Largest i in [0, data_size - 2] with xdata[i] <= x.
 *
 * Branchless: the trip count depends only on data_size, so the loop is
 * perfectly predicted and the single data-dependent step compiles to a
 * conditional move. That makes the cost insensitive to the access pattern,
 * unlike a plain left/right binary search whose comparison branch mispredicts
 * on unsorted input.
 * Requires xdata[0] <= x <= xdata[data_size - 1].
 */
static int32_t find_interval(double x, const MYFLT *xdata, int32_t data_size) {
    int32_t base = 0;
    int32_t len = data_size - 1;

    while (len > 1) {
        const int32_t half = len >> 1;
        base += (x >= xdata[base + half]) ? half : 0;
        len -= half;
    }

    return base;
}

static void find_lerp_interval(CSN_LERP_INTERVAL *interval, double x, const MYFLT *xdata, const MYFLT *ydata, int32_t data_size, CSN_INTERP_BOUNDS_MODE bounds) {
    if (x < xdata[0] || x > xdata[data_size - 1]) {
        switch (bounds) {
            case REMAP_ERROR:
                interval->bmode = REMAP_NOT_VALID;
                return;
            case REMAP_CLAMP:
                if (x < xdata[0]) {
                    interval->bmode = REMAP_CLAMP_LEFT;
                } else {
                    interval->bmode = REMAP_CLAMP_RIGHT;
                }
                return;
            case REMAP_FILL:
                interval->bmode = REMAP_FILL_VALUE;
                return;
            case REMAP_EXTRAPOLATE:
                if (x < xdata[0]) {
                    interval->x0 = xdata[0];
                    interval->x1 = xdata[1];
                    interval->y0 = ydata[0];
                    interval->y1 = ydata[1];
                    interval->index = 0;
                    interval->bmode = REMAP_EXTRAPOLATE_LEFT;
                } else {
                    interval->x0 = xdata[data_size - 2];
                    interval->x1 = xdata[data_size - 1];
                    interval->y0 = ydata[data_size - 2];
                    interval->y1 = ydata[data_size - 1];
                    interval->index = data_size - 2;
                    interval->bmode = REMAP_EXTRAPOLATE_RIGHT;
                }
                return;
        }
    }

    const int32_t left = find_interval(x, xdata, data_size);

    interval->x0 = xdata[left];
    interval->x1 = xdata[left + 1];
    interval->y0 = ydata[left];
    interval->y1 = ydata[left + 1];
    interval->index = left;
    interval->bmode = REMAP_VALID;
    return;
}


/*
 * LINEAR INTERPOLATION
 * y = (y0 (x1 - x) + y1 (x - x0)) / (x1 - x0)
 */
static double lerp(double x, double x0, double x1, double y0, double y1) {
    return (y0 * (x1 - x) + y1 * (x - x0)) / (x1 - x0);
}

/*
 * NEAREST INTERPOLATION
 *     | y0 if (x - x0) <= (x1 - x)
 * y = |
 *     | y1 if (x - x0) > (x1 - x)
 */
double nearest(double x, double x0, double x1, double y0, double y1) {
    return ((x - x0) <= (x1 - x)) ? y0 : y1;
}

/*
 * CUBIC PCHIP
 */
static double pchip_endpoint(double h0, double h1, double d0, double d1) {
    double m = ((2.0 * h0 + h1) * d0 - h0 * d1) / (h0 + h1);
    if ((m > 0.0) != (d0 > 0.0)) {
        return 0.0;
    }

    if (((d0 > 0.0) != (d1 > 0.0)) && fabs(m) > 3.0 * fabs(d0)) {
        return 3.0 * d0;
    }

    return m;
}

/*
 * PCHIP slope at breakpoint i.
 *
 * The Fritsch-Carlson scheme is local: m[i] only depends on the one or two
 * intervals adjacent to i. Evaluating it on demand therefore costs O(1) and
 * removes the need to tabulate (and re-tabulate) the whole curve whenever the
 * breakpoints change at k-rate.
 */
static double pchip_slope(const MYFLT *x, const MYFLT *y, int32_t n, int32_t i) {
    double h0, h1, d0, d1;

    if (n == 2) {
        return ((double) y[1] - (double) y[0]) / ((double) x[1] - (double) x[0]);
    }

    if (i == 0) {
        h0 = (double) x[1] - (double) x[0];
        h1 = (double) x[2] - (double) x[1];
        d0 = ((double) y[1] - (double) y[0]) / h0;
        d1 = ((double) y[2] - (double) y[1]) / h1;
        return pchip_endpoint(h0, h1, d0, d1);
    }

    if (i == n - 1) {
        h0 = (double) x[n - 1] - (double) x[n - 2];
        h1 = (double) x[n - 2] - (double) x[n - 3];
        d0 = ((double) y[n - 1] - (double) y[n - 2]) / h0;
        d1 = ((double) y[n - 2] - (double) y[n - 3]) / h1;
        return pchip_endpoint(h0, h1, d0, d1);
    }

    h0 = (double) x[i] - (double) x[i - 1];
    h1 = (double) x[i + 1] - (double) x[i];
    d0 = ((double) y[i] - (double) y[i - 1]) / h0;
    d1 = ((double) y[i + 1] - (double) y[i]) / h1;
    if (d0 == 0.0 || d1 == 0.0 || ((d0 > 0.0) != (d1 > 0.0))) {
        return 0.0;
    }

    const double w1 = 2.0 * h1 + h0;
    const double w2 = h1 + 2.0 * h0;
    return (w1 + w2) / (w1 / d0 + w2 / d1);
}

/*
 * Cubic Hermite coefficients of segment i, with the PCHIP slopes resolved on
 * the fly. Kept separate from the evaluation so that the vector opcode can
 * reuse them across every input value falling in the same segment.
 */
static void pchip_segment(CSN_PCHIP_SEGMENT *s, const MYFLT *xdata, const MYFLT *ydata, int32_t n, int32_t i) {
    const double x0 = (double) xdata[i];
    const double y0 = (double) ydata[i];
    const double h = (double) xdata[i + 1] - x0;

    s->x0 = x0;
    s->a = y0;

    if (h <= 0.0) {               /* non-increasing x: no usable segment */
        s->b = s->c = s->d = 0.0;
        return;
    }

    const double delta = ((double) ydata[i + 1] - y0) / h;
    const double m0 = pchip_slope(xdata, ydata, n, i);
    const double m1 = pchip_slope(xdata, ydata, n, i + 1);

    s->b = m0;
    s->c = (3.0 * delta - 2.0 * m0 - m1) / h;
    s->d = (m0 + m1 - 2.0 * delta) / (h * h);
}

/*
 * Outside [x[i], x[i+1]] this extrapolates along the same cubic.
 */
static double pchip_segment_eval(const CSN_PCHIP_SEGMENT *s, double x) {
    const double t = x - s->x0;
    return ((s->d * t + s->c) * t + s->b) * t + s->a;
}

static double pchip_eval(double x, const MYFLT *xdata, const MYFLT *ydata, int32_t n, int32_t i) {
    CSN_PCHIP_SEGMENT s;
    pchip_segment(&s, xdata, ydata, n, i);
    return pchip_segment_eval(&s, x);
}

/*
 * The interval search assumes strictly increasing breakpoints, for every
 * interpolation mode. Only checked for i-rate data, where it costs nothing
 * at performance time.
 */
static int32_t check_increasing(CSN_ARRAY *vec) {
    const MYFLT *v = vec->data;
    for (size_t i = 0; i < vec->size - 1; ++i) {
        if (v[i + 1] <= v[i]) {
            return NOTOK;
        }
    }
    return OK;
}


static int32_t remap_assign_value(CSOUND *csound, OPDS *perf_h, const double *x_data, const double *y_data, size_t data_size, const double x, int32_t ibounds, double fill_value, double *y_out, CSN_INTERP_MODE mode) {
    int32_t size = (int32_t) data_size;
    double y = 0.0;
    CSN_LERP_INTERVAL l_interval = {0};
    find_lerp_interval(&l_interval, x, x_data, y_data, size, (CSN_INTERP_BOUNDS_MODE) ibounds);
    switch (l_interval.bmode) {
        case REMAP_NOT_VALID:
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[remap] x value out of bounds");
        case REMAP_CLAMP_LEFT:
            y = y_data[0];
            break;
        case REMAP_CLAMP_RIGHT:
            y = y_data[size - 1];
            break;
        case REMAP_FILL_VALUE:
            y = fill_value;
            break;
        case REMAP_VALID:
        case REMAP_EXTRAPOLATE_LEFT:
        case REMAP_EXTRAPOLATE_RIGHT:
            switch (mode) {
                case REMAP_LINEAR:
                    y = lerp(x, l_interval.x0, l_interval.x1, l_interval.y0, l_interval.y1);
                    break;
                case REMAP_NEAREST:
                    y = nearest(x, l_interval.x0, l_interval.x1, l_interval.y0, l_interval.y1);
                    break;
                case REMAP_PREVIOUS:
                    y = l_interval.y0;
                    break;
                case REMAP_NEXT:
                    y = l_interval.y1;
                    break;
                case REMAP_CUBIC:
                    y = pchip_eval(x, x_data, y_data, size, l_interval.index);
                    break;
            }
            break;
    }

    *y_out = y;
    return OK;
}

int32_t csnarray_remap_scalar(CSOUND *csound, CSN_REMAP_SCALAR *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t imode = (int32_t) *p->mode;
    int32_t ibounds = (int32_t) *p->bounds;
    double fill_value = (double) *p->fill;

    uint32_t x_data_handle = (uint32_t) p->data_handle_x->id;
    uint32_t y_data_handle = (uint32_t) p->data_handle_y->id;

    int32_t is_valid_mode = imode >= REMAP_LINEAR && imode <= REMAP_CUBIC;
    if (!is_valid_mode) {
        return csound->InitError(csound, "[csnarray] Not valid mode value");
    }

    int32_t is_valid_bounds = ibounds >= REMAP_ERROR && ibounds <= REMAP_EXTRAPOLATE;
    if (!is_valid_bounds) {
        return csound->InitError(csound, "[csnarray] Invalid bounds mode: %d", ibounds);
    }

    int32_t res = OK;

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot_x = get_slot(reg, x_data_handle);
    if (slot_x == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", x_data_handle);
        goto done;
    }

    CSN_SLOT *slot_y = get_slot(reg, y_data_handle);
    if (slot_y == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", y_data_handle);
        goto done;
    }

    CSN_ARRAY *x_data = slot_x->array;
    size_t x_data_size = x_data->size;
    uint32_t x_data_ndim = x_data->ndim;

    CSN_ARRAY *y_data = slot_y->array;
    size_t y_data_size = y_data->size;
    uint32_t y_data_ndim = y_data->ndim;

    if (x_data->itype != CSN_REAL || y_data->itype != CSN_REAL) {
        res = csound->InitError(csound, "[csnarray] x and y array must be real array");
        goto done;
    }

    if (x_data_size < 2 || y_data_size < 2 || x_data_ndim != 1U || y_data_ndim != 1U) {
        res = csound->InitError(csound, "[csnarray] x and y array should have length greater or equal to two and dimension equal to 1");
        goto done;
    }

    if (x_data_size != y_data_size) {
        res = csound->InitError(csound, "[csnarray] x and y array should have same length");
        goto done;
    }

    if (check_increasing(x_data) != OK) {
        res = csound->InitError(csound, "[remap] x array must be strictly increasing");
        goto done;
    }

    double x = (double) *p->x;
    double y = 0.0;
    res = remap_assign_value(csound, NULL, x_data->data, y_data->data, x_data->size, x, (CSN_INTERP_BOUNDS_MODE) ibounds, fill_value, &y, (CSN_INTERP_MODE) imode);
    if (res != OK) goto done;

    *p->y = (MYFLT) y;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_remap_scalar_k_init(CSOUND *csound, CSN_REMAP_SCALAR *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    p->registry = reg;

    int32_t imode = (int32_t) *p->mode;
    int32_t ibounds = (int32_t) *p->bounds;
    double fill_value = (double) *p->fill;

    int32_t is_valid_mode = imode >= REMAP_LINEAR && imode <= REMAP_CUBIC;
    if (!is_valid_mode) {
        return csound->InitError(csound, "[csnarray] Not valid mode value");
    }

    int32_t is_valid_bounds = ibounds >= REMAP_ERROR && ibounds <= REMAP_EXTRAPOLATE;
    if (!is_valid_bounds) {
        return csound->InitError(csound, "[csnarray] Invalid bounds mode: %d", ibounds);
    }

    p->imode = (CSN_INTERP_MODE) imode;
    p->ibounds = (CSN_INTERP_BOUNDS_MODE) ibounds;
    p->fill_value = fill_value;

    p->is_published = false;
    return OK;
}

int32_t csnarray_remap_scalar_k(CSOUND *csound, CSN_REMAP_SCALAR *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    uint32_t x_data_handle = (uint32_t) p->data_handle_x->id;
    uint32_t y_data_handle = (uint32_t) p->data_handle_y->id;

    CHECK_KTRIG(p->trig);

    int32_t res = OK;

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot_x = get_slot(reg, x_data_handle);
    if (slot_x == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", x_data_handle);
    }

    CSN_SLOT *slot_y = get_slot(reg, y_data_handle);
    if (slot_y == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", y_data_handle);
    }

    CSN_ARRAY *x_data = slot_x->array;
    size_t x_data_size = x_data->size;
    uint32_t x_data_ndim = x_data->ndim;

    CSN_ARRAY *y_data = slot_y->array;
    size_t y_data_size = y_data->size;
    uint32_t y_data_ndim = y_data->ndim;

    bool is_same_x_data = p->is_published ? is_same_array_version(&x_data->version, &p->prev_x_data_version) : false;
    bool is_same_y_data = p->is_published ? is_same_array_version(&y_data->version, &p->prev_y_data_version) : false;

    double x = (double) *p->x;
    if (is_same_x_data && is_same_y_data && p->prev_x == x) {
        *p->y = (MYFLT) p->prev_y;
        goto done;
    }

    if (x_data->itype != CSN_REAL || y_data->itype != CSN_REAL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] x and y array must be real array");
    }

    if (x_data_size < 2 || y_data_size < 2 || x_data_ndim != 1U || y_data_ndim != 1U) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] x and y array should have length greater or equal to two and dimension equal to 1");
    }

    if (x_data_size != y_data_size) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] x and y array should have same length");
    }

    if (!is_same_x_data) {
        if (check_increasing(x_data) != OK) {
            csound->UnlockMutex(reg->mutex);
            return csound->PerfError(csound, &p->h, "[remap] x array must be strictly increasing");
        }
    }

    double y = 0.0;
    res = remap_assign_value(csound, &p->h, x_data->data, y_data->data, x_data->size, x, p->ibounds, p->fill_value, &y, p->imode);
    if (res != OK) goto done;

    set_array_version(&p->prev_x_data_version, &x_data->version);
    set_array_version(&p->prev_y_data_version, &y_data->version);
    p->prev_x = x;
    p->prev_y = y;
    p->is_published = true;
    *p->y = (MYFLT) y;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_remap_k_init(CSOUND *csound, CSN_REMAP *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t imode = (int32_t) *p->mode;
    int32_t ibounds = (int32_t) *p->bounds;
    double fill_value = (double) *p->fill;

    int32_t is_valid_mode = imode >= REMAP_LINEAR && imode <= REMAP_CUBIC;
    if (!is_valid_mode) {
        return csound->InitError(csound, "[csnarray] Not valid mode value");
    }

    int32_t is_valid_bounds = ibounds >= REMAP_ERROR && ibounds <= REMAP_EXTRAPOLATE;
    if (!is_valid_bounds) {
        return csound->InitError(csound, "[csnarray] Invalid bounds mode: %d", ibounds);
    }

    p->imode = (CSN_INTERP_MODE) imode;
    p->ibounds = (CSN_INTERP_BOUNDS_MODE) ibounds;
    p->fill_value = fill_value;

    int32_t res = OK;
    const char *err = NULL;

    uint32_t source_handle = (uint32_t) p->source_handle->id;
    uint32_t x_data_handle = (uint32_t) p->data_handle_x->id;
    uint32_t y_data_handle = (uint32_t) p->data_handle_y->id;

    csound->LockMutex(reg->mutex);
    CSN_SLOT *source_slot = get_slot(reg, source_handle);
    if (source_slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
        goto done;
    }
    if (get_slot(reg, x_data_handle) == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", x_data_handle);
        goto done;
    }
    if (get_slot(reg, y_data_handle) == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", y_data_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = source_slot->array;
    uint32_t protect[3] = { source_handle, x_data_handle, y_data_handle };
    if (create_csnarray_locked(csound, reg, &p->h, source_arr->ndim, source_arr->shape, &p->array, p->handle, protect, 3U, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    reset_empty_csnarray(p->array, source_arr->ndim, source_arr->shape, CSN_REAL);

    SET_KDATA_BEGIN(p, reg);
    p->is_published = false;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

/*
 * Reads source_size query points out of source (source_stride apart), maps each
 * one through the breakpoints (x_data, y_data, data_size) and writes the result
 * to dst (dst_stride apart). csninterp keeps a value where it found it, so both
 * strides match there; csnresample changes the length of the mapped axis, so it
 * walks a contiguous query grid and writes with the destination stride.
 */
static int32_t remap_slice(CSOUND *csound, OPDS *perf_h, double *dst, size_t dst_stride, const double *x_data, const double *y_data, size_t data_size, const double *source, size_t source_size, size_t source_stride, CSN_INTERP_BOUNDS_MODE ibounds, CSN_INTERP_MODE imode, const double fill_value) {
    int32_t res = OK;
    for (size_t i = 0; i < source_size; ++i) {
        CSN_COMPLEXDAT y = { 0.0, 0.0 };
        CSN_COMPLEXDAT z = slice_get(source, i, source_stride, CSN_REAL);
        res = remap_assign_value(csound, perf_h, x_data, y_data, data_size, z.re, ibounds, fill_value, &y.re, imode);
        if (res != OK) return res;
        slice_put(dst, i, dst_stride, CSN_REAL, y);
    }
    return res;
}


int32_t csnarray_remap_k(CSOUND *csound, CSN_REMAP *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REG_HANDLE(csound, &p->h, reg, p->k_data.owned_handle);

    uint32_t source_handle = (uint32_t) p->source_handle->id;
    uint32_t x_data_handle = (uint32_t) p->data_handle_x->id;
    uint32_t y_data_handle = (uint32_t) p->data_handle_y->id;

    int32_t res = OK;
    const char *err = NULL;

    CHECK_KTRIG(p->axis);

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot_source = get_slot(reg, source_handle);
    if (slot_source == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
    }

    CSN_SLOT *slot_x = get_slot(reg, x_data_handle);
    if (slot_x == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", x_data_handle);
    }

    CSN_SLOT *slot_y = get_slot(reg, y_data_handle);
    if (slot_y == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", y_data_handle);
    }

    CSN_ARRAY *source_arr = slot_source->array;
    size_t source_size = source_arr->size;
    uint32_t *source_shape = source_arr->shape;

    CSN_ARRAY *x_data = slot_x->array;
    size_t x_data_size = x_data->size;
    uint32_t x_data_ndim = x_data->ndim;
    uint32_t *x_data_shape = x_data->shape;

    CSN_ARRAY *y_data = slot_y->array;
    size_t y_data_size = y_data->size;
    uint32_t y_data_ndim = y_data->ndim;
    uint32_t *y_data_shape = y_data->shape;

    if (x_data_size < 2 || y_data_size < 2 || x_data_ndim != 1U || y_data_ndim != 1U) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] x and y array should have length greater or equal to two and dimension equal to 1");
    }

    const MYFLT *axis_in = p->INOCOUNT > 7 ? p->trig : NULL;
    CSN_AXIS_SPEC axis_spec = csn_normalize_axis(axis_in, source_arr->ndim, CSN_AXIS_DEFAULT_FLATTEN);
    if (axis_spec.kind == CSN_AXIS_INVALID) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "Axis out of bounds");
    }
    int32_t axis = axis_spec.kind == CSN_AXIS_FLATTEN ? -1 : (int32_t) axis_spec.index;

    bool is_same_source = false;
    bool is_same_x_data = false;
    bool is_same_y_data = false;
    bool is_same_axis = false;
    if (p->is_published) {
        is_same_source = is_same_array_version(&source_arr->version, &p->prev_x_source_version);
        is_same_x_data = is_same_array_version(&x_data->version, &p->prev_x_data_version);
        is_same_y_data = is_same_array_version(&y_data->version, &p->prev_y_data_version);
        is_same_axis = axis == p->k_data.prev_axis_u;
    }

    if (is_same_x_data && is_same_y_data && is_same_source && is_same_axis) {
        p->handle->id = p->k_data.owned_handle;
        goto done;
    }

    if (x_data->itype != CSN_REAL || y_data->itype != CSN_REAL || source_arr->itype != CSN_REAL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Interp requires real array");
    }

    if (x_data_ndim != y_data_ndim) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] x and y array should have same shape and dim");
    }

    bool is_same_shape = memcmp(x_data_shape, y_data_shape, sizeof(uint32_t) * (size_t) x_data_ndim) != 0;

    if (is_same_shape || x_data_size != y_data_size) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "x and y array should have same shape and dim");
    }

    if (!is_same_x_data) {
        if (check_increasing(x_data) != OK) {
            csound->UnlockMutex(reg->mutex);
            return csound->PerfError(csound, &p->h, "[remap] x array must be strictly increasing");
        }
    }

    uint32_t new_ndim = source_arr->ndim;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    memcpy(new_shape, source_arr->shape, sizeof(uint32_t) * CSN_MAX_DIMS);

    size_t req_size = 0;
    if (get_array_size_from_shape(&req_size, new_ndim, new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSN_ARRAY *arr = NULL;
    size_t logical_size = source_size == 0 ? 0 : req_size;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, new_ndim, new_shape, logical_size, CSN_REAL, err);
    if (res != OK) goto done;
    p->array = arr;

    if (axis == -1) {
        res = remap_slice(csound, &p->h, arr->data, 1U, x_data->data, y_data->data, x_data_size, source_arr->data, source_size, 1U, p->ibounds, p->imode, p->fill_value);
        if (res != OK) goto done;
    } else {
            uint32_t reduced_shape[CSN_MAX_DIMS] = {0};
            uint32_t reduced_ndim = 0;
            size_t slice_count = 1;
            for (uint32_t i = 0; i < source_arr->ndim; ++i) {
                if (i != (uint32_t) axis) {
                    reduced_shape[reduced_ndim++] = source_arr->shape[i];
                    slice_count *= source_arr->shape[i];
                }
            }

            size_t src_stride = source_arr->strides[axis];

            for (size_t linear = 0; linear < slice_count; ++linear) {
                uint32_t dst_coords[CSN_MAX_DIMS] = {0};
                uint32_t src_coords[CSN_MAX_DIMS] = {0};

                from_linear_to_coords(dst_coords, reduced_shape, linear, reduced_ndim);
                for (uint32_t i = 0, j = 0; i < source_arr->ndim; ++i) {
                    src_coords[i] = (i == (uint32_t) axis) ? 0 : dst_coords[j++];
                }

                size_t src_base = from_coords_to_offset(src_coords, source_arr->strides, source_arr->ndim);
                size_t dst_base = from_coords_to_offset(src_coords, arr->strides, source_arr->ndim);
                res = remap_slice(csound, &p->h, arr->data + dst_base, arr->strides[axis], x_data->data, y_data->data, x_data_size, source_arr->data + src_base, source_shape[axis], src_stride, p->ibounds, p->imode, p->fill_value);
                if (res != OK) goto done;
            }
    }


    set_array_version(&p->prev_x_source_version, &source_arr->version);
    set_array_version(&p->prev_x_data_version, &x_data->version);
    set_array_version(&p->prev_y_data_version, &y_data->version);
    SET_KDATA_END(p, new_shape, new_ndim, CSN_REAL);
    p->is_published = true;
    p->k_data.prev_axis_u = axis;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

/*
 * Query grid of a resample. The breakpoints are the source indexes 0..n-1 and
 * the new_length query points span that whole range, so the first and the last
 * output sample always land exactly on the source endpoints and nothing is
 * asked outside the table (whatever the bounds mode is). A single output point
 * has no span to divide and takes the first sample.
 */
static void fill_resample_grid(double *x_data, size_t data_size, double *x_source, size_t new_length) {
    for (size_t i = 0; i < data_size; ++i) {
        x_data[i] = (double) i;
    }

    if (new_length == 1) {
        x_source[0] = 0.0;
        return;
    }

    const double span = (double) (data_size - 1);
    const double step = span / (double) (new_length - 1);
    for (size_t i = 0; i < new_length; ++i) {
        x_source[i] = (double) i * step;
    }
    /* Rounding must never push the last query past the last breakpoint. */
    x_source[new_length - 1] = span;
}

/*
 * Grows one of the resample scratch buffers. ReAlloc doubles as malloc for a
 * NULL buffer, so the k-rate opcode can start with nothing allocated and only
 * pay for the sizes it actually sees.
 */
static int32_t ensure_resample_buffer(CSOUND *csound, CSN_SCRATCH *scratch, size_t required) {
    if (scratch->scratch != NULL && scratch->scratch_capacity >= required) {
        return OK;
    }

    const size_t new_capacity = required * 2;
    double *data = csound->ReAlloc(csound, scratch->scratch, sizeof(double) * new_capacity);
    if (data == NULL) {
        return NOTOK;
    }

    scratch->scratch = data;
    scratch->scratch_capacity = new_capacity;
    return OK;
}

/*
 * Shared body of the two resample opcodes. axis == -1 resamples the array read
 * as a flat vector; otherwise every slice along axis is resampled on its own.
 * y_scratch is only touched in the second case, where the breakpoints are a
 * strided slice and the interval search wants them contiguous.
 */
static int32_t resample_run(CSOUND *csound, OPDS *perf_h, CSN_ARRAY *arr, CSN_ARRAY *source_arr, int32_t axis, const double *x_data, const double *x_source, size_t new_length, double *y_scratch, CSN_INTERP_BOUNDS_MODE ibounds, CSN_INTERP_MODE imode, const double fill_value) {
    if (axis == -1) {
        return remap_slice(csound, perf_h, arr->data, 1U, x_data, source_arr->data, source_arr->size, x_source, new_length, 1U, ibounds, imode, fill_value);
    }

    uint32_t reduced_shape[CSN_MAX_DIMS] = {0};
    uint32_t reduced_ndim = 0;
    size_t slice_count = 1;
    for (uint32_t i = 0; i < source_arr->ndim; ++i) {
        if (i != (uint32_t) axis) {
            reduced_shape[reduced_ndim++] = source_arr->shape[i];
            slice_count *= source_arr->shape[i];
        }
    }

    const size_t data_size = (size_t) source_arr->shape[axis];
    const size_t src_stride = source_arr->strides[axis];
    const size_t dst_stride = arr->strides[axis];

    for (size_t linear = 0; linear < slice_count; ++linear) {
        uint32_t dst_coords[CSN_MAX_DIMS] = {0};
        uint32_t src_coords[CSN_MAX_DIMS] = {0};

        from_linear_to_coords(dst_coords, reduced_shape, linear, reduced_ndim);
        for (uint32_t i = 0, j = 0; i < source_arr->ndim; ++i) {
            src_coords[i] = (i == (uint32_t) axis) ? 0 : dst_coords[j++];
        }

        const size_t src_base = from_coords_to_offset(src_coords, source_arr->strides, source_arr->ndim);
        const size_t dst_base = from_coords_to_offset(src_coords, arr->strides, source_arr->ndim);

        for (size_t i = 0; i < data_size; ++i) {
            y_scratch[i] = source_arr->data[src_base + i * src_stride];
        }

        int32_t res = remap_slice(csound, perf_h, arr->data + dst_base, dst_stride, x_data, y_scratch, data_size, x_source, new_length, 1U, ibounds, imode, fill_value);
        if (res != OK) return res;
    }

    return OK;
}

/* The resampled extent, and the shape it produces. Flattening collapses the
   result to a single dimension, since the source layout does not survive it. */
static void resample_layout(const CSN_ARRAY *source_arr, int32_t axis, uint32_t new_length, uint32_t *new_ndim, uint32_t *new_shape, size_t *data_size) {
    if (axis == -1) {
        *new_ndim = 1U;
        new_shape[0] = new_length;
        *data_size = source_arr->size;
        return;
    }

    *new_ndim = source_arr->ndim;
    memcpy(new_shape, source_arr->shape, sizeof(uint32_t) * CSN_MAX_DIMS);
    new_shape[axis] = new_length;
    *data_size = (size_t) source_arr->shape[axis];
}

int32_t csnarray_resample(CSOUND *csound, CSN_RESAMPLE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t imode = (int32_t) *p->mode;
    int32_t ibounds = (int32_t) *p->bounds;
    double fill_value = (double) *p->fill;

    uint32_t source_handle = p->source_handle->id;

    if (!IS_VALID_LENGTH((double) *p->new_length) || (double) *p->new_length < 1.0) {
        return csound->InitError(csound, "[csnarray] Invalid new length");
    }
    uint32_t new_length = (uint32_t) *p->new_length;

    int32_t is_valid_mode = imode >= REMAP_LINEAR && imode <= REMAP_CUBIC;
    if (!is_valid_mode) {
        return csound->InitError(csound, "[csnarray] Not valid mode value");
    }

    int32_t is_valid_bounds = ibounds >= REMAP_ERROR && ibounds <= REMAP_EXTRAPOLATE;
    if (!is_valid_bounds) {
        return csound->InitError(csound, "[csnarray] Invalid bounds mode: %d", ibounds);
    }

    p->imode = (CSN_INTERP_MODE) imode;
    p->ibounds = (CSN_INTERP_BOUNDS_MODE) ibounds;
    p->fill_value = fill_value;

    int32_t res = OK;
    const char *err = NULL;
    /* Declared before the first jump to done, which frees them. */
    double *x_data = NULL;
    double *x_source = NULL;
    double *y_scratch = NULL;

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = slot->array;

    if (source_arr->itype != CSN_REAL) {
        res = csound->InitError(csound, "[csnarray] Source array must be real array");
        goto done;
    }

    const MYFLT *axis_in = p->INOCOUNT > 5 ? p->axis : NULL;
    CSN_AXIS_SPEC axis_spec = csn_normalize_axis(axis_in, source_arr->ndim, CSN_AXIS_DEFAULT_FLATTEN);
    if (axis_spec.kind == CSN_AXIS_INVALID) {
        res = csound->InitError(csound, "[csnarray] Axis out of bounds");
        goto done;
    }
    int32_t axis = axis_spec.kind == CSN_AXIS_FLATTEN ? -1 : (int32_t) axis_spec.index;

    uint32_t new_ndim = 1U;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    size_t data_size = 0;
    resample_layout(source_arr, axis, new_length, &new_ndim, new_shape, &data_size);

    if (data_size < 2) {
        res = csound->InitError(csound, "[csnarray] The resampled axis should have length greater or equal to two");
        goto done;
    }

    const uint32_t protect[1] = { source_handle };
    if (create_csnarray_locked(csound, reg, &p->h, new_ndim, new_shape, &p->array, p->handle, protect, 1U, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    x_data = csound->Calloc(csound, sizeof(double) * data_size);
    x_source = csound->Calloc(csound, sizeof(double) * new_length);
    if (axis != -1) {
        y_scratch = csound->Calloc(csound, sizeof(double) * data_size);
    }

    if (x_data == NULL || x_source == NULL || (axis != -1 && y_scratch == NULL)) {
        res = csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
        goto done;
    }

    fill_resample_grid(x_data, data_size, x_source, new_length);

    res = resample_run(csound, NULL, p->array, source_arr, axis, x_data, x_source, new_length, y_scratch, p->ibounds, p->imode, p->fill_value);

done:
    if (x_data != NULL) csound->Free(csound, x_data);
    if (x_source != NULL) csound->Free(csound, x_source);
    if (y_scratch != NULL) csound->Free(csound, y_scratch);
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_resample_k_init(CSOUND *csound, CSN_RESAMPLE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t imode = (int32_t) *p->mode;
    int32_t ibounds = (int32_t) *p->bounds;
    double fill_value = (double) *p->fill;

    int32_t is_valid_mode = imode >= REMAP_LINEAR && imode <= REMAP_CUBIC;
    if (!is_valid_mode) {
        return csound->InitError(csound, "[csnarray] Not valid mode value");
    }

    int32_t is_valid_bounds = ibounds >= REMAP_ERROR && ibounds <= REMAP_EXTRAPOLATE;
    if (!is_valid_bounds) {
        return csound->InitError(csound, "[csnarray] Invalid bounds mode: %d", ibounds);
    }

    p->imode = (CSN_INTERP_MODE) imode;
    p->ibounds = (CSN_INTERP_BOUNDS_MODE) ibounds;
    p->fill_value = fill_value;

    if (!IS_VALID_LENGTH((double) *p->new_length) || (double) *p->new_length < 1.0) {
        return csound->InitError(csound, "[csnarray] Invalid new length");
    }
    uint32_t new_length = (uint32_t) *p->new_length;

    int32_t res = OK;
    const char *err = NULL;
    uint32_t source_handle = p->source_handle->id;

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = slot->array;
    if (source_arr->itype != CSN_REAL) {
        res = csound->InitError(csound, "[csnarray] Source array must be real array");
        goto done;
    }
    const MYFLT *axis_in = p->INOCOUNT > 6 ? p->trig : NULL;
    CSN_AXIS_SPEC axis_spec = csn_normalize_axis(axis_in, source_arr->ndim, CSN_AXIS_DEFAULT_FLATTEN);
    if (axis_spec.kind == CSN_AXIS_INVALID) {
        res = csound->InitError(csound, "[csnarray] Axis out of bounds");
        goto done;
    }
    int32_t axis = axis_spec.kind == CSN_AXIS_FLATTEN ? -1 : (int32_t) axis_spec.index;

    uint32_t new_ndim = 1U;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    size_t data_size = 0;
    resample_layout(source_arr, axis, new_length, &new_ndim, new_shape, &data_size);
    if (data_size < 2) {
        res = csound->InitError(csound, "[csnarray] The resampled axis should have length greater or equal to two");
        goto done;
    }

    p->x_source_scratch.scratch = NULL;
    p->x_data_scratch.scratch = NULL;
    p->y_data_scratch.scratch = NULL;
    p->x_source_scratch.scratch_capacity = 0;
    p->x_data_scratch.scratch_capacity = 0;
    p->y_data_scratch.scratch_capacity = 0;
    if (ensure_resample_buffer(csound, &p->x_data_scratch, source_arr->capacity) != OK
        || ensure_resample_buffer(csound, &p->x_source_scratch, (size_t) new_length) != OK
        || (axis != -1 && ensure_resample_buffer(csound, &p->y_data_scratch, source_arr->capacity) != OK)) {
        res = csound->InitError(csound, "[csnarray] Internal error: memory allocation failed");
        goto done;
    }

    uint32_t protect[1] = { source_handle };
    if (create_csnarray_locked(csound, reg, &p->h, new_ndim, new_shape, &p->array, p->handle, protect, 1U, &err, CSN_REAL) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    reset_empty_csnarray(p->array, new_ndim, new_shape, CSN_REAL);

    SET_KDATA_BEGIN(p, reg);
    p->is_published = false;

done:
    csound->UnlockMutex(reg->mutex);
    if (res != OK) {
        deinit_scratch(csound, &p->x_source_scratch);
        deinit_scratch(csound, &p->x_data_scratch);
        deinit_scratch(csound, &p->y_data_scratch);
    }
    return res;
}

int32_t csnarray_resample_k(CSOUND *csound, CSN_RESAMPLE *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REG_HANDLE(csound, &p->h, reg, p->k_data.owned_handle);

    uint32_t source_handle = p->source_handle->id;

    if (!IS_VALID_LENGTH((double) *p->new_length) || (double) *p->new_length < 1.0) {
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid new length");
    }
    uint32_t new_length = (uint32_t) *p->new_length;

    int32_t res = OK;
    const char *err = NULL;

    CHECK_KTRIG(p->axis);

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
    }

    CSN_ARRAY *source_arr = slot->array;

    if (source_arr->itype != CSN_REAL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Source array must be real array");
    }

    const MYFLT *axis_in = p->INOCOUNT > 6 ? p->trig : NULL;
    CSN_AXIS_SPEC axis_spec = csn_normalize_axis(axis_in, source_arr->ndim, CSN_AXIS_DEFAULT_FLATTEN);
    if (axis_spec.kind == CSN_AXIS_INVALID) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "Axis out of bounds");
    }
    int32_t axis = axis_spec.kind == CSN_AXIS_FLATTEN ? -1 : (int32_t) axis_spec.index;

    bool is_same_source = false;
    bool is_same_axis = false;
    bool is_same_length = false;
    if (p->is_published) {
        is_same_source = is_same_array_version(&source_arr->version, &p->prev_x_source_version);
        is_same_axis = axis == (int32_t) p->k_data.prev_axis_u;
        is_same_length = new_length == p->k_data.prev_size;
    }

    if (is_same_source && is_same_axis && is_same_length) {
        p->handle->id = p->k_data.owned_handle;
        goto done;
    }

    uint32_t new_ndim = 1U;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    size_t data_size = 0;
    resample_layout(source_arr, axis, new_length, &new_ndim, new_shape, &data_size);

    if (data_size < 2) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] The resampled axis should have length greater or equal to two");
    }

    size_t req_size = 0;
    if (get_array_size_from_shape(&req_size, new_ndim, new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSN_ARRAY *arr = NULL;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, new_ndim, new_shape, req_size, CSN_REAL, err);
    if (res != OK) goto done;
    p->array = arr;

    /* The grids serve the output slot. The new length is already part of its
       shape, but the source length is not: a source that outgrew the capacity
       reserved at init would otherwise grow these on a marked output. */
    bool rt_locked = csn_slot_rt_locked(reg, p->k_data.owned_handle);
    res = csn_scratch_reserve(csound, &p->h, rt_locked, &p->x_data_scratch, data_size, sizeof(double));
    if (res == OK) res = csn_scratch_reserve(csound, &p->h, rt_locked, &p->x_source_scratch, (size_t) new_length, sizeof(double));
    if (res == OK && axis != -1) res = csn_scratch_reserve(csound, &p->h, rt_locked, &p->y_data_scratch, data_size, sizeof(double));
    if (res != OK) goto done;

    fill_resample_grid(p->x_data_scratch.scratch, data_size, p->x_source_scratch.scratch, (size_t) new_length);

    res = resample_run(csound, &p->h, arr, source_arr, axis, p->x_data_scratch.scratch, p->x_source_scratch.scratch, (size_t) new_length, p->y_data_scratch.scratch, p->ibounds, p->imode, p->fill_value);
    if (res != OK) goto done;

    set_array_version(&p->prev_x_source_version, &source_arr->version);
    SET_KDATA_END(p, new_shape, new_ndim, CSN_REAL);
    p->is_published = true;
    p->k_data.prev_size = new_length;
    p->k_data.prev_axis_u = axis;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t truncate_assign_shape(CSOUND *csound, OPDS *perf_h, const uint32_t *source_shape, uint32_t *new_shape, uint32_t source_ndim, size_t source_size, size_t new_length, const MYFLT *in_axis, int32_t *out_axis, CSN_RESIZE_MODE mode) {
    if (mode == CSN_HEAD_ARR && source_ndim != 1U) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Dimension greater than one not allowed for head");
    }

    if (mode == CSN_HEAD_ARR) {
        if (new_length > source_size) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Head length %zu must not exceed array size %zu", new_length, source_size);
        }
        new_shape[0] = new_length;
    } else if (mode == CSN_TRUNCATE_ARR) {
        CSN_AXIS_SPEC axis_spec = csn_normalize_axis(in_axis, source_ndim, CSN_AXIS_DEFAULT_ALL);
        if (axis_spec.kind == CSN_AXIS_INVALID) {
            double value = in_axis == NULL ? 0.0 : (double) *in_axis;
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Axis %g is invalid for a %u-D array (valid axes: finite integers %d..%u)", value, source_ndim, -(int32_t) source_ndim, source_ndim - 1U);
        }
        *out_axis = axis_spec.kind == CSN_AXIS_ALL ? -1 : (int32_t) axis_spec.index;
        int32_t axis = *out_axis;

        if (axis == -1) {
            for (uint32_t i = 0; i < source_ndim; i++) {
                if (new_length > source_shape[i]) {
                    return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] New length %zu must not exceed array axis length %u", new_length, source_shape[i]);
                }
                new_shape[i] = new_length;
            }
        } else {
            if (new_length > source_shape[axis]) {
                return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] New length %zu must not exceed array axis length %u", new_length, source_shape[axis]);
            }
            new_shape[axis] = new_length;
        }
    } else {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Mode not allowed here");
    }

    return OK;
}

static void truncate_assign_value(CSN_ARRAY *arr, CSN_ARRAY *source_arr, uint32_t *new_shape, uint32_t new_ndim, int32_t axis) {
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;
    size_t source_size = source_arr->size;
    /* Every offset and every count below is in elements; a complex array keeps
       two doubles per element, so each one is scaled by itype. */
    ITEM_TYPE itype = source_arr->itype;
    if (axis == -1) {
        uint32_t coords[CSN_MAX_DIMS] = {0};
        for (size_t i = 0; i < arr->size; i++) {
            from_linear_to_coords(coords, new_shape, i, new_ndim);
            size_t src_offset = from_coords_to_offset(coords, source_arr->strides, source_ndim);
            arr->data[i * itype] = source_arr->data[src_offset * itype];
            if (itype == CSN_COMPLEX) {
                arr->data[i * itype + 1] = source_arr->data[src_offset * itype + 1];
            }
        }
    } else if (axis > 0 && (uint32_t) axis < source_ndim) {
        size_t block_size = 1;
        for (uint32_t i = (uint32_t) axis + 1; i < source_ndim; i++) {
            block_size *= source_shape[i];
        }

        uint32_t src_axis_size = source_shape[axis];
        uint32_t dst_axis_size = new_shape[axis];
        size_t outer_count = source_size / (src_axis_size * block_size);
        for (size_t outer = 0; outer < outer_count; outer++) {
            size_t src_base = outer * src_axis_size * block_size;
            size_t dst_base = outer * dst_axis_size * block_size;
            size_t copy_count = block_size * dst_axis_size;
            memcpy(arr->data + dst_base * itype, source_arr->data + src_base * itype, sizeof(double) * copy_count * itype);
        }
    } else {
        /* axis == 0 || CSN_HEAD_ARR: the kept elements are already a prefix */
        memcpy(arr->data, source_arr->data, sizeof(double) * arr->size * itype);
    }
}

static int32_t truncate_assign_value_in(CSOUND *csound, OPDS *perf_h, CSN_ARRAY *source_arr, uint32_t new_ndim, const uint32_t *new_shape, int32_t axis) {
    uint32_t source_ndim = source_arr->ndim;
    uint32_t *source_shape = source_arr->shape;
    size_t source_size = source_arr->size;
    size_t new_size = 0;
    if (get_array_size_from_shape(&new_size, new_ndim, new_shape) != OK) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }
    /* Same element/double distinction as truncate_assign_value. */
    ITEM_TYPE itype = source_arr->itype;
    if (axis == -1) {
        uint32_t coords[CSN_MAX_DIMS] = {0};
        for (size_t i = 0; i < new_size; i++) {
            from_linear_to_coords(coords, new_shape, i, new_ndim);
            size_t src_offset = from_coords_to_offset(coords, source_arr->strides, source_ndim);
            source_arr->data[i * itype] = source_arr->data[src_offset * itype];
            if (itype == CSN_COMPLEX) {
                source_arr->data[i * itype + 1] = source_arr->data[src_offset * itype + 1];
            }
        }
    } else if (axis > 0 && (uint32_t) axis < source_ndim) {
        size_t block_size = 1;
        for (uint32_t i = (uint32_t) axis + 1; i < source_ndim; i++) {
            block_size *= source_shape[i];
        }

        uint32_t src_axis_size = source_shape[axis];
        uint32_t dst_axis_size = new_shape[axis];
        size_t outer_count = source_size / (src_axis_size * block_size);
        for (size_t outer = 0; outer < outer_count; outer++) {
            size_t src_base = outer * src_axis_size * block_size;
            size_t dst_base = outer * dst_axis_size * block_size;
            size_t copy_count = block_size * dst_axis_size;
            memmove(source_arr->data + dst_base * itype, source_arr->data + src_base * itype, sizeof(double) * copy_count * itype);
        }
    }
    /* also for axis == 0 || CSN_HEAD_ARR */
    set_csnarray_layout(source_arr, new_ndim, new_shape, new_size, source_arr->itype);
    return OK;
}

static int32_t truncate_helper(CSOUND *csound, CSN_TRUNCATE *p, CSN_RESIZE_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = (uint32_t) p->source_handle->id;

    if (!IS_VALID_LENGTH((double) *p->length)) {
        return csound->InitError(csound, "[csnarray] Invalid new array length");
    }
    uint32_t new_length = (uint32_t) *p->length;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = slot->array;
    size_t source_size = source_arr->size;
    uint32_t *source_shape = source_arr->shape;
    uint32_t source_ndim = source_arr->ndim;
    ITEM_TYPE itype = source_arr->itype;

    uint32_t new_ndim = source_ndim;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    memcpy(new_shape, source_shape, sizeof(uint32_t) * CSN_MAX_DIMS);

    int32_t axis = -1;
    const MYFLT *axis_in = mode == CSN_TRUNCATE_ARR && p->INOCOUNT > 2 ? p->arg_a : NULL;
    res = truncate_assign_shape(csound, NULL, source_shape, new_shape, source_ndim, source_size, (size_t) new_length, axis_in, &axis, mode);
    if (res != OK) goto done;

    uint32_t protect[1] = { source_handle };
    if (create_csnarray_locked(csound, reg, &p->h, new_ndim, new_shape, &p->array, p->handle, protect, 1U, &err, itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    truncate_assign_value(p->array, source_arr, new_shape, new_ndim, axis);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_truncate(CSOUND *csound, CSN_TRUNCATE *p) {
    return truncate_helper(csound, p, CSN_TRUNCATE_ARR);
}

int32_t csnarray_head(CSOUND *csound, CSN_TRUNCATE *p) {
    return truncate_helper(csound, p, CSN_HEAD_ARR);
}

int32_t csnarray_truncate_in(CSOUND *csound, CSN_TRUNCATE_IN *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = (uint32_t) p->source_handle->id;

    if (!IS_VALID_LENGTH((double) *p->length)) {
        return csound->InitError(csound, "[csnarray] Invalid new array length");
    }
    uint32_t new_length = (uint32_t) *p->length;

    int32_t res = OK;

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = slot->array;
    size_t source_size = source_arr->size;
    uint32_t *source_shape = source_arr->shape;
    uint32_t source_ndim = source_arr->ndim;

    uint32_t new_ndim = source_ndim;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    memcpy(new_shape, source_shape, sizeof(uint32_t) * CSN_MAX_DIMS);

    int32_t axis = -1;
    const MYFLT *axis_in = p->INOCOUNT > 2 ? p->axis : NULL;
    res = truncate_assign_shape(csound, NULL, source_shape, new_shape, source_ndim, source_size, (size_t) new_length, axis_in, &axis, CSN_TRUNCATE_ARR);
    if (res != OK) goto done;

    res = truncate_assign_value_in(csound, NULL, source_arr, new_ndim, new_shape, axis);
    if (res != OK) goto done;

    /* truncate_assign_value_in moves the layout version; the payload moved too. */
    update_array_data_version(&source_arr->version);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t resize_parse_shape_and_size(CSOUND *csound, OPDS *perf_h, uint32_t *ndim, uint32_t *out_shape, const ARRAYDAT *in_shape, size_t *new_size) {
    if (perf_h == NULL) {
        int32_t res_shape = parse_shape_array(csound, in_shape, ndim, out_shape);
        if (res_shape != OK) return res_shape;
    } else {
        int32_t res_shape = parse_shape_array_k(csound, perf_h, in_shape, ndim, out_shape);
        if (res_shape != OK) return res_shape;
    }

    if (get_array_size_from_shape(new_size, *ndim, out_shape) != OK) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] Shape is invalid or its element count exceeds the configured limit");
    }
    return OK;
}

static void resize_assign_value(CSN_ARRAY *arr, CSN_ARRAY *source_arr, size_t new_size) {
    size_t count = source_arr->size < new_size ? source_arr->size : new_size;
    memcpy(arr->data, source_arr->data, sizeof(double) * count * arr->itype);
    if (new_size > source_arr->size) {
        size_t count_diff = new_size - source_arr->size;
        memset(arr->data + source_arr->size * arr->itype, 0, sizeof(double) * count_diff * arr->itype);
    }
}

int32_t csnarray_resize(CSOUND *csound, CSN_RESIZE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;

    uint32_t ndim = 0;
    uint32_t shape[CSN_MAX_DIMS] = {0};
    size_t new_size = 0;

    int32_t res = resize_parse_shape_and_size(csound, NULL, &ndim, shape, p->new_shape, &new_size);
    if (res != OK) return res;

    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = slot->array;
    /* The output is created first: p->array is still NULL on this pass, and the
       item type has to come from the source. */
    if (create_csnarray_locked(csound, reg, &p->h, ndim, shape, &p->array, p->handle, &source_handle, 1U, &err, source_arr->itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    resize_assign_value(p->array, source_arr, new_size);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

static int32_t resize_in_assign_value(CSOUND *csound, OPDS *perf_h, CSN_ARRAY **source_array, size_t new_size, ITEM_TYPE itype, bool rt_locked) {
    CSN_ARRAY *source_arr = *source_array;
    const size_t old_size = source_arr->size;

    if (new_size > source_arr->capacity) {
        if (perf_h != NULL && rt_locked) {
            return rt_growth_refused(csound, perf_h, source_arr, new_size);
        }
        size_t new_cap = new_size * 2;
        /* ReAlloc keeps the elements already there, so only the grown tail
           below needs writing. */
        double *new_data = csound->ReAlloc(csound, source_arr->data, sizeof(double) * new_cap * itype);
        if (new_data == NULL) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Internal error: memory allocation failed");
        }
        source_arr->data = new_data;
        source_arr->capacity = new_cap;
    }

    /* Growing past the old contents must read as zeros even when the buffer did
       not have to move: the spare capacity still holds whatever a larger
       earlier layout left in it. */
    if (new_size > old_size) {
        memset(source_arr->data + old_size * itype, 0, sizeof(double) * (new_size - old_size) * itype);
    }

    source_arr->size = new_size;
    return OK;
}

int32_t csnarray_resize_in(CSOUND *csound, CSN_RESIZE_IN *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;

    uint32_t ndim = 0;
    uint32_t shape[CSN_MAX_DIMS] = {0};
    size_t new_size = 0;

    int32_t res = resize_parse_shape_and_size(csound, NULL, &ndim, shape, p->new_shape, &new_size);
    if (res != OK) return res;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = slot->array;
    ITEM_TYPE itype = source_arr->itype;

    res = resize_in_assign_value(csound, NULL, &source_arr, new_size, itype, false);
    if (res != OK) goto done;

    set_csnarray_layout(source_arr, ndim, shape, new_size, itype);
    /* set_csnarray_layout moves the layout version; the payload changed too. */
    update_array_data_version(&source_arr->version);

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}


static int32_t truncate_k_init_helper(CSOUND *csound, CSN_TRUNCATE *p, CSN_RESIZE_MODE mode) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = (uint32_t) p->source_handle->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = slot->array;
    size_t source_size = source_arr->size;
    uint32_t *source_shape = source_arr->shape;
    uint32_t source_ndim = source_arr->ndim;
    ITEM_TYPE itype = source_arr->itype;

    uint32_t new_ndim = source_ndim;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    memcpy(new_shape, source_shape, sizeof(uint32_t) * CSN_MAX_DIMS);

    uint32_t protect[1] = { source_handle };
    if (create_csnarray_locked(csound, reg, &p->h, new_ndim, new_shape, &p->array, p->handle, protect, 1U, &err, itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    reset_empty_csnarray(p->array, new_ndim, new_shape, itype);
    if (source_size > 0) {
        memcpy(p->array->data, source_arr->data, sizeof(double) * source_arr->size * itype);
        p->array->size = source_size;
    }

    SET_KDATA_BEGIN(p, reg);
    p->is_published = false;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_truncate_k_init(CSOUND *csound, CSN_TRUNCATE *p) {
    return truncate_k_init_helper(csound, p, CSN_TRUNCATE_ARR);
}

int32_t csnarray_head_k_init(CSOUND *csound, CSN_TRUNCATE *p) {
    return truncate_k_init_helper(csound, p, CSN_HEAD_ARR);
}

static int32_t truncate_k_helper(CSOUND *csound, CSN_TRUNCATE *p, CSN_RESIZE_MODE mode) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REG_HANDLE(csound, &p->h, reg, p->k_data.owned_handle);

    uint32_t source_handle = (uint32_t) p->source_handle->id;

    if (!IS_VALID_LENGTH((double) *p->length)) {
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid new array length");
    }
    uint32_t new_length = (uint32_t) *p->length;

    int32_t res = OK;
    const char *err = NULL;

    CHECK_KTRIG(p->arg_a);

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
    }

    CSN_ARRAY *source_arr = slot->array;
    size_t source_size = source_arr->size;
    uint32_t *source_shape = source_arr->shape;
    uint32_t source_ndim = source_arr->ndim;
    ITEM_TYPE itype = source_arr->itype;

    uint32_t new_ndim = source_ndim;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    memcpy(new_shape, source_shape, sizeof(uint32_t) * CSN_MAX_DIMS);


    int32_t axis = -1;
    const MYFLT *axis_in = mode == CSN_TRUNCATE_ARR && p->INOCOUNT > 3 ? p->arg_b : NULL;
    res = truncate_assign_shape(csound, &p->h, source_shape, new_shape, source_ndim, source_size, (size_t) new_length, axis_in, &axis, mode);
    if (res != OK) goto done;

    if (p->is_published) {
        bool is_same_version = is_same_array_version(&p->k_data.prev_source_version, &source_arr->version);
        bool is_same_length = p->k_data.prev_size == new_length;
        bool is_same_axis = (int32_t) p->k_data.prev_axis_u == axis;
        if (is_same_version && is_same_length && is_same_axis) {
            p->handle->id = p->k_data.owned_handle;
            goto done;
        }
    }

    size_t req_size = 0;
    if (get_array_size_from_shape(&req_size, new_ndim, new_shape) != OK) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid shape or element count exceeds the configured limit");
    }

    CSN_ARRAY *arr = NULL;
    size_t logical_size = source_size == 0 ? 0 : req_size;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, new_ndim, new_shape, logical_size, itype, err);
    if (res != OK) goto done;
    p->array = arr;

    truncate_assign_value(p->array, source_arr, new_shape, new_ndim, axis);
    SET_KDATA_END(p, new_shape, new_ndim, itype);
    set_array_version(&p->k_data.prev_source_version, &source_arr->version);
    p->k_data.prev_axis_u = axis;
    p->k_data.prev_size = new_length;
    p->is_published = true;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_truncate_k(CSOUND *csound, CSN_TRUNCATE *p) {
    return truncate_k_helper(csound, p, CSN_TRUNCATE_ARR);
}

int32_t csnarray_head_k(CSOUND *csound, CSN_TRUNCATE *p) {
    return truncate_k_helper(csound, p, CSN_HEAD_ARR);
}

int32_t csnarray_truncate_in_k_init(CSOUND *csound, CSN_TRUNCATE_IN *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    p->k_data.registry = reg;
    p->is_published = false;
    return OK;
}

int32_t csnarray_truncate_in_k(CSOUND *csound, CSN_TRUNCATE_IN *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    uint32_t source_handle = (uint32_t) p->source_handle->id;

    if (!IS_VALID_LENGTH((double) *p->length)) {
        return csound->PerfError(csound, &p->h, "[csnarray] Invalid new array length");
    }
    uint32_t new_length = (uint32_t) *p->length;

    int32_t res = OK;

    CHECK_KTRIG(p->axis);

    csound->LockMutex(reg->mutex);
    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", source_handle);
    }

    CSN_ARRAY *source_arr = slot->array;
    size_t source_size = source_arr->size;
    uint32_t *source_shape = source_arr->shape;
    uint32_t source_ndim = source_arr->ndim;

    uint32_t new_ndim = source_ndim;
    uint32_t new_shape[CSN_MAX_DIMS] = {0};
    memcpy(new_shape, source_shape, sizeof(uint32_t) * CSN_MAX_DIMS);

    int32_t axis = -1;
    const MYFLT *axis_in = p->INOCOUNT > 3 ? p->trig : NULL;
    res = truncate_assign_shape(csound, &p->h, source_shape, new_shape, source_ndim, source_size, (size_t) new_length, axis_in, &axis, CSN_TRUNCATE_ARR);
    if (res != OK) goto done;

    if (p->is_published) {
        bool is_same_version = is_same_array_version(&p->k_data.prev_source_version, &source_arr->version);
        bool is_same_length = p->k_data.prev_size == new_length;
        bool is_same_axis = (int32_t) p->k_data.prev_axis_u == axis;
        if (is_same_version && is_same_length && is_same_axis) goto done;
    }

    res = truncate_assign_value_in(csound, &p->h, source_arr, new_ndim, new_shape, axis);
    if (res != OK) goto done;

    /* truncate_assign_value_in already moved the layout version; this adds the
       data generation and records it, so the next pass recognizes its own
       write instead of truncating again. */
    PUBLISH_INPLACE_WRITE(&p->k_data, source_handle, source_arr, false, false, false);
    p->k_data.prev_size = new_length;
    p->k_data.prev_axis_u = axis;
    p->is_published = true;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}


int32_t csnarray_resize_k_init(CSOUND *csound, CSN_RESIZE *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    uint32_t source_handle = p->source_handle->id;

    int32_t res = OK;
    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        res = csound->InitError(csound, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
        goto done;
    }

    CSN_ARRAY *source_arr = slot->array;
    if (create_csnarray_locked(csound, reg, &p->h, source_arr->ndim, source_arr->shape, &p->array, p->handle, &source_handle, 1U, &err, source_arr->itype) != OK) {
        res = csound->InitError(csound, "[csnarray] %s", err);
        goto done;
    }

    reset_empty_csnarray(p->array, source_arr->ndim, source_arr->shape, source_arr->itype);
    SET_KDATA_BEGIN(p, reg);
    p->is_published = false;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}


int32_t csnarray_resize_k(CSOUND *csound, CSN_RESIZE *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REG_HANDLE(csound, &p->h, reg, p->k_data.owned_handle);

    uint32_t source_handle = p->source_handle->id;

    CHECK_KTRIG(p->trig);

    uint32_t ndim = 0;
    uint32_t shape[CSN_MAX_DIMS] = {0};
    size_t new_size = 0;

    int32_t res = resize_parse_shape_and_size(csound, &p->h, &ndim, shape, p->new_shape, &new_size);
    if (res != OK) return res;

    const char *err = NULL;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
    }

    CSN_ARRAY *source_arr = slot->array;

    if (p->is_published) {
        bool is_same_version = is_same_array_version(&p->k_data.prev_source_version, &source_arr->version);
        bool is_same_shape = memcmp(p->k_data.prev_shape, shape, sizeof(uint32_t) * CSN_MAX_DIMS) == 0;
        if (is_same_version && is_same_shape) goto done;
    }

    CSN_ARRAY *arr = NULL;
    size_t logical_size = source_arr->size == 0 ? 0 : new_size;
    res = NEED_TO_UPDATE_SLOT(csound, &p->h, &arr, &p->k_data, NULL, ndim, shape, logical_size, source_arr->itype, err);
    if (res != OK) goto done;
    p->array = arr;

    resize_assign_value(arr, source_arr, new_size);
    SET_KDATA_END(p, shape, ndim, source_arr->itype);
    p->is_published = true;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}

int32_t csnarray_resize_in_k_init(CSOUND *csound, CSN_RESIZE_IN *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    p->k_data.registry = reg;
    p->is_published = false;
    return OK;
}

int32_t csnarray_resize_in_k(CSOUND *csound, CSN_RESIZE_IN *p) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    uint32_t source_handle = p->source_handle->id;

    uint32_t ndim = 0;
    uint32_t shape[CSN_MAX_DIMS] = {0};
    size_t new_size = 0;

    CHECK_KTRIG(p->trig);

    int32_t res = resize_parse_shape_and_size(csound, &p->h, &ndim, shape, p->new_shape, &new_size);
    if (res != OK) return res;

    csound->LockMutex(reg->mutex);

    CSN_SLOT *slot = get_slot(reg, source_handle);
    if (slot == NULL) {
        csound->UnlockMutex(reg->mutex);
        return csound->PerfError(csound, &p->h, "[csnarray] Unknown array handle %u: no array with this id is registered (it may have been freed already)", (uint32_t) source_handle);
    }

    CSN_ARRAY *source_arr = slot->array;
    ITEM_TYPE itype = source_arr->itype;

    if (p->is_published) {
        bool is_same_version = is_same_array_version(&p->k_data.prev_source_version, &source_arr->version);
        bool is_same_shape = memcmp(p->k_data.prev_shape, shape, sizeof(uint32_t) * CSN_MAX_DIMS) == 0;
        if (is_same_version && is_same_shape) goto done;
    }

    res = resize_in_assign_value(csound, &p->h, &source_arr, new_size, itype, slot->rt_locked);
    if (res != OK) goto done;

    set_csnarray_layout(source_arr, ndim, shape, new_size, itype);
    memset(p->k_data.prev_shape, 0, sizeof(uint32_t) * CSN_MAX_DIMS);
    memcpy(p->k_data.prev_shape, shape, sizeof(uint32_t) * CSN_MAX_DIMS);
    PUBLISH_INPLACE_WRITE(&p->k_data, source_handle, source_arr, false, false, false);
    p->is_published = true;

done:
    csound->UnlockMutex(reg->mutex);
    return res;
}
