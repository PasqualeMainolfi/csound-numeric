#include "csnbinary.h"
#include "csnregistry.h"
#include "csnstrings.h"
#include "csnum.h"
#include "csnum_internal.h"
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define HEXALPHABET "0123456789ABCDEF"
#define BASE64TABLE "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"
#define BINHEX_NAME(is_binary) ((is_binary) ? "binary" : "hex")

/* 2^64: the first double a size_t cannot hold */
#define BINHEX_VALUE_LIMIT 18446744073709551616.0


static bool is_valid_binhex_value(double value) {
    return isfinite(value) && trunc(value) == value && value >= 0.0 && value < BINHEX_VALUE_LIMIT;
}

static size_t count_digits(size_t value, size_t min_width, bool is_binary) {
    size_t shift = is_binary ? 1 : 4;
    size_t n = 1;
    while (value >>= shift) n++;
    return n > min_width ? n : min_width;
}

/* The longest string any value takes at this width, terminator included: 64
   binary or 16 hex digits for the largest one. The k forms reserve it at init,
   so a perf pass writes without growing. */
static size_t max_binhex_room(size_t width, bool is_binary) {
    size_t digits = is_binary ? 64 : 16;
    return (width > digits ? width : digits) + 1;
}

static inline char binhex_digit(size_t value, size_t shift, bool is_binary) {
    return is_binary ? (char) ('0' + ((value >> shift) & 1U)) : HEXALPHABET[(value >> shift) & 0xFU];
}

static void tobinex_conversion(char *dest, size_t value, size_t len, bool is_binary) {
    size_t fac = is_binary ? 1 : 4;
    for (size_t i = 0; i < len; i++) {
        size_t bit = (len - i - 1) * fac;
        dest[i] = bit >= 64 ? '0' : binhex_digit(value, bit, is_binary);
    }
    dest[len] = '\0';
}


static int32_t tobinex_helper(CSOUND *csound, CSN_TOBINHEX *p, bool is_k_init, bool is_binary) {
    double width_temp = (double) *p->width;
    if (width_temp < 0.0 || !IS_VALID_VALUE_INT32(width_temp)) {
        return csn_init_error(csound, "[csnarray] Invalid width %.15g: must be a non-negative integer", width_temp);
    }
    size_t width = (size_t) width_temp;

    if (is_k_init) {
        /* NaN equals no value, so the first perf pass always writes */
        p->prev_value = NAN;
        int32_t res = ensure_string_capacity(csound, NULL, &p->buffer, max_binhex_room(width, is_binary), false);
        if (res != OK) return res;
        if (!csn_known_at_init(csound, p->value)) return OK;
    }

    double value = (double) *p->value;
    if (!is_valid_binhex_value(value)) {
        return csn_init_error(csound, "[csnarray] Invalid value %.15g: must be a non-negative integer below 2^64", value);
    }

    size_t n_digits = count_digits((size_t) value, width, is_binary);
    int32_t res = ensure_string_capacity(csound, NULL, &p->buffer, n_digits + 1, false);
    if (res != OK) return res;

    tobinex_conversion(p->buffer.data, (size_t) value, n_digits, is_binary);
    COPY_STRING_TO_STRINGDAT(csound, &p->h, p->string_out, p->buffer.data);
    if (is_k_init) p->prev_value = value;

    return OK;
}

static int32_t tobinex_k_helper(CSOUND *csound, CSN_TOBINHEX *p, bool is_binary) {
    CHECK_KTRIG(p->trig);

    size_t width = (size_t) *p->width;

    double value = (double) *p->value;
    if (!is_valid_binhex_value(value)) {
        return csn_perf_error(csound, &p->h, "[csnarray] Invalid value %.15g: must be a non-negative integer below 2^64", value);
    }

    if (value == p->prev_value) return OK;

    size_t n_digits = count_digits((size_t) value, width, is_binary);
    int32_t res = ensure_string_capacity(csound, &p->h, &p->buffer, n_digits + 1, false);
    if (res != OK) return res;

    tobinex_conversion(p->buffer.data, (size_t) value, n_digits, is_binary);
    COPY_STRING_TO_STRINGDAT(csound, &p->h, p->string_out, p->buffer.data);
    p->prev_value = value;

    return OK;
}

/* The digits after an optional 0b (binary) or 0x (hex) prefix, or NULL when
   what follows is not a non-empty run of digits of that base. */
static const char *skip_prefix_and_validate(const char *source, bool is_binary) {
    if (source[0] == '0') {
        char x = source[1];
        if (is_binary ? (x == 'b' || x == 'B') : (x == 'x' || x == 'X')) source += 2;
    }

    if (*source == '\0') return NULL;
    for (const char *c = source; *c != '\0'; c++) {
        unsigned char u = (unsigned char) *c;
        if (is_binary ? (u != '0' && u != '1') : !isxdigit(u)) return NULL;
    }

    return source;
}

static int32_t frombinex(size_t *value, const char *source, bool is_binary) {
    const char *digits = skip_prefix_and_validate(source, is_binary);
    if (digits == NULL) return NOTOK;
    errno = 0;
    char *end;
    unsigned long long value_temp = strtoull(digits, &end, is_binary ? 2 : 16);
    if (errno == ERANGE || *end != '\0') return NOTOK;
    *value = (size_t) value_temp;
    return OK;
}

static int32_t frombinex_helper(CSOUND *csound, CSN_FROMBINHEX *p, bool is_binary) {
    if (!IS_VALID_STRINGDAT(p->string_in)) {
        return csn_init_error(csound, "[csnarray] Invalid/NULL string");
    }

    const char *source = p->string_in->data;
    size_t value = 0;
    if (frombinex(&value, source, is_binary) != OK) {
        return csn_init_error(csound, "[csnarray] '%s' is not a %s number below 2^64", source, BINHEX_NAME(is_binary));
    }
    *p->value = (cs_float) value;
    return OK;
}

static int32_t frombinex_k_helper(CSOUND *csound, CSN_FROMBINHEX *p, bool is_binary) {
    CHECK_KTRIG(p->trig);

    if (!IS_VALID_STRINGDAT(p->string_in)) {
        return csn_perf_error(csound, &p->h, "[csnarray] Invalid/NULL string");
    }

    const char *source = p->string_in->data;
    size_t value = 0;
    if (frombinex(&value, source, is_binary) != OK) {
        return csn_perf_error(csound, &p->h, "[csnarray] '%s' is not a %s number below 2^64", source, BINHEX_NAME(is_binary));
    }
    *p->value = (cs_float) value;
    return OK;
}

/* Writes every element of src as a string into out, which prepare_output has
   just sized like src. Each slot grows to at least reserve bytes. Runs with
   the registry mutex held. */
static int32_t tobinex_fill(CSOUND *csound, OPDS *perf_h, const CSN_ARRAY *src, CSN_ARRAY *out, size_t width, size_t reserve, bool is_binary) {
    for (size_t i = 0; i < src->size; i++) {
        double value = src->data[i];
        if (!is_valid_binhex_value(value)) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] Invalid value %.15g at index %zu: must be a non-negative integer below 2^64", value, i);
        }
        size_t len = count_digits((size_t) value, width, is_binary);
        size_t room = len + 1 > reserve ? len + 1 : reserve;
        CSN_STRDATA *slot = &out->sdata[i];
        int32_t res = ensure_string_capacity(csound, perf_h, slot, room, out->rt_locked);
        if (res != OK) return res;
        tobinex_conversion(slot->data, (size_t) value, len, is_binary);
    }
    return OK;
}

/* Parses every string of src into out, sized like src. Runs with the
   registry mutex held. */
static int32_t frombinex_fill(CSOUND *csound, OPDS *perf_h, const CSN_ARRAY *src, CSN_ARRAY *out, bool is_binary) {
    for (size_t i = 0; i < src->size; i++) {
        const char *s = slot_string(&src->sdata[i]);
        size_t value = 0;
        if (frombinex(&value, s, is_binary) != OK) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] '%s' at index %zu is not a %s number below 2^64", s, i, BINHEX_NAME(is_binary));
        }
        out->data[i] = (double) value;
    }
    return OK;
}

/* Shared by the i form and, as its init, by the k form. */
static int32_t tobinex_arr_helper(CSOUND *csound, CSN_BINHEX_ARR *p, bool is_binary) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    double width_temp = (double) *p->arg_a;
    if (width_temp < 0.0 || !IS_VALID_VALUE_INT32(width_temp)) {
        return csn_init_error(csound, "[csnarray] Invalid width %.15g: must be a non-negative integer", width_temp);
    }
    size_t width = (size_t) width_temp;
    /* the k form reserves every slot's longest string, so a perf pass on a
       real-time path rewrites the slots without growing one */
    size_t reserve = csn_is_k_form(&p->h) ? max_binhex_room(width, is_binary) : 0;

    int32_t res = OK;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle, p->handle }, 2);

    CSN_ARRAY *source_arr = p->source_handle;

    if (source_arr->itype != CSN_REAL) {
        res = csn_init_error(csound, "[csnarray] To binary/hex conversions requires real array");
        goto done;
    }

    const CSN_ARRAY *op[1] = { source_arr };
    CSNARR_RES out_res = prepare_output(csound, &p->h, NULL, reg, p->handle, source_arr->ndim, source_arr->shape, CSN_STRING, op, 1, CSN_ALIAS_REFUSE);
    res = out_res.err;
    if (res != OK) goto done;
    pres.locked |= out_res.locked;

    res = tobinex_fill(csound, NULL, source_arr, p->handle, width, reserve, is_binary);
    if (res != OK) goto done;

    PUBLISH_DERIVED_RESULT(&p->k_data, p->source_handle, p->handle);
    p->k_data.registry = reg;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

static int32_t tobinex_arr_k_helper(CSOUND *csound, CSN_BINHEX_ARR *p, bool is_binary) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    CHECK_KTRIG(p->arg_b);

    size_t width = (size_t) *p->arg_a;
    int32_t res = OK;
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle, p->handle }, 2);

    CSN_ARRAY *source_arr = p->source_handle;

    if (CAN_REUSE_LAST_RESULT(&p->k_data, source_arr, p->handle)) goto done;

    if (source_arr->itype != CSN_REAL) {
        res = CSN_ACCESSOR_ERROR_LOCKED(csound, &p->h, "[csnarray] To binary/hex conversions requires real array");
        goto done;
    }

    const CSN_ARRAY *op[1] = { source_arr };
    CSNARR_RES out_res = prepare_output(csound, &p->h, &p->h, reg, p->handle, source_arr->ndim, source_arr->shape, CSN_STRING, op, 1, CSN_ALIAS_REFUSE);
    res = out_res.err;
    if (res != OK) goto done;
    pres.locked |= out_res.locked;

    res = tobinex_fill(csound, &p->h, source_arr, p->handle, width, 0, is_binary);
    if (res != OK) goto done;

    PUBLISH_DERIVED_RESULT(&p->k_data, p->source_handle, p->handle);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

/* Shared by the i form and, as its init, by the k form. */
static int32_t frombinex_arr_helper(CSOUND *csound, CSN_BINHEX_ARR *p, bool is_binary) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);

    int32_t res = OK;
    stamp_globals(csound, &p->h);
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle, p->handle }, 2);

    CSN_ARRAY *source_arr = p->source_handle;

    if (source_arr->itype != CSN_STRING) {
        res = csn_init_error(csound, "[csnarray] From binary/hex conversions requires string array");
        goto done;
    }

    const CSN_ARRAY *op[1] = { source_arr };
    CSNARR_RES out_res = prepare_output(csound, &p->h, NULL, reg, p->handle, source_arr->ndim, source_arr->shape, CSN_REAL, op, 1, CSN_ALIAS_REFUSE);
    res = out_res.err;
    if (res != OK) goto done;
    pres.locked |= out_res.locked;

    res = frombinex_fill(csound, NULL, source_arr, p->handle, is_binary);
    if (res != OK) goto done;

    PUBLISH_DERIVED_RESULT(&p->k_data, p->source_handle, p->handle);
    p->k_data.registry = reg;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

static int32_t frombinex_arr_k_helper(CSOUND *csound, CSN_BINHEX_ARR *p, bool is_binary) {
    CSN_REGISTRY *reg = p->k_data.registry;
    CHECK_REGISTRY(csound, &p->h, reg);

    CHECK_KTRIG(p->arg_a);

    int32_t res = OK;
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle, p->handle }, 2);

    CSN_ARRAY *source_arr = p->source_handle;

    if (CAN_REUSE_LAST_RESULT(&p->k_data, source_arr, p->handle)) goto done;

    if (source_arr->itype != CSN_STRING) {
        res = CSN_ACCESSOR_ERROR_LOCKED(csound, &p->h, "[csnarray] From binary/hex conversions requires string array");
        goto done;
    }

    const CSN_ARRAY *op[1] = { source_arr };
    CSNARR_RES out_res = prepare_output(csound, &p->h, &p->h, reg, p->handle, source_arr->ndim, source_arr->shape, CSN_REAL, op, 1, CSN_ALIAS_REFUSE);
    res = out_res.err;
    if (res != OK) goto done;
    pres.locked |= out_res.locked;

    res = frombinex_fill(csound, &p->h, source_arr, p->handle, is_binary);
    if (res != OK) goto done;

    PUBLISH_DERIVED_RESULT(&p->k_data, p->source_handle, p->handle);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}



int32_t csnbinary_tobinhex_deinit(CSOUND *csound, CSN_TOBINHEX *p) {
    csn_str_release(csound, &p->buffer);
    return OK;
}

int32_t csnbinary_tobinary(CSOUND *csound, CSN_TOBINHEX *p) {
    return tobinex_helper(csound, p, false, true);
}

int32_t csnbinary_tobinary_k_init(CSOUND *csound, CSN_TOBINHEX *p) {
    return tobinex_helper(csound, p, true, true);
}

int32_t csnbinary_tobinary_k(CSOUND *csound, CSN_TOBINHEX *p) {
    return tobinex_k_helper(csound, p, true);
}

int32_t csnbinary_tobinary_arr(CSOUND *csound, CSN_BINHEX_ARR *p) {
    return tobinex_arr_helper(csound, p, true);
}

int32_t csnbinary_tobinary_arr_k(CSOUND *csound, CSN_BINHEX_ARR *p) {
    return tobinex_arr_k_helper(csound, p, true);
}

int32_t csnbinary_frombinary(CSOUND *csound, CSN_FROMBINHEX *p) {
    return frombinex_helper(csound, p, true);
}

int32_t csnbinary_frombinary_k(CSOUND *csound, CSN_FROMBINHEX *p) {
    return frombinex_k_helper(csound, p, true);
}

int32_t csnbinary_frombinary_arr(CSOUND *csound, CSN_BINHEX_ARR *p) {
    return frombinex_arr_helper(csound, p, true);
}

int32_t csnbinary_frombinary_arr_k(CSOUND *csound, CSN_BINHEX_ARR *p) {
    return frombinex_arr_k_helper(csound, p, true);
}

int32_t csnbinary_tohex(CSOUND *csound, CSN_TOBINHEX *p) {
    return tobinex_helper(csound, p, false, false);
}

int32_t csnbinary_tohex_k_init(CSOUND *csound, CSN_TOBINHEX *p) {
    return tobinex_helper(csound, p, true, false);
}

int32_t csnbinary_tohex_k(CSOUND *csound, CSN_TOBINHEX *p) {
    return tobinex_k_helper(csound, p, false);
}

int32_t csnbinary_tohex_arr(CSOUND *csound, CSN_BINHEX_ARR *p) {
    return tobinex_arr_helper(csound, p, false);
}

int32_t csnbinary_tohex_arr_k(CSOUND *csound, CSN_BINHEX_ARR *p) {
    return tobinex_arr_k_helper(csound, p, false);
}

int32_t csnbinary_fromhex(CSOUND *csound, CSN_FROMBINHEX *p) {
    return frombinex_helper(csound, p, false);
}

int32_t csnbinary_fromhex_k(CSOUND *csound, CSN_FROMBINHEX *p) {
    return frombinex_k_helper(csound, p, false);
}

int32_t csnbinary_fromhex_arr(CSOUND *csound, CSN_BINHEX_ARR *p) {
    return frombinex_arr_helper(csound, p, false);
}

int32_t csnbinary_fromhex_arr_k(CSOUND *csound, CSN_BINHEX_ARR *p) {
    return frombinex_arr_k_helper(csound, p, false);
}

/* The length of the base64 text of len bytes, padding included, or SIZE_MAX
   when it does not fit a size_t. */
static size_t tobase64_len(size_t len) {
    size_t groups = len / 3 + (len % 3 != 0);
    if (groups > (SIZE_MAX - 1) / 4) return SIZE_MAX;
    return groups * 4;
}

static int32_t base64_value(unsigned char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

static bool is_byte(double value) {
    return isfinite(value) && trunc(value) == value && value >= 0.0 && value <= 255.0;
}

/* The number of bytes base64 text decodes to, or SIZE_MAX with the offset of
   the first bad group of 4 in bad (len when the length is not a multiple of
   4). The text must be canonical RFC 4648: padded, no blanks, '=' only at the
   end, the unused bits of the last group zero; Python's b64decode does not
   check that last rule. Checked whole before anything is written, so a
   refused string leaves the output as it was. */
static size_t base64_check(const char *src, size_t len, size_t *bad) {
    *bad = len;
    if (len % 4 != 0) return SIZE_MAX;
    size_t out = 0;
    for (size_t i = 0; i < len; i += 4) {
        *bad = i;
        bool last = i + 4 == len;
        bool pad2 = src[i + 2] == '=';
        bool pad3 = src[i + 3] == '=';
        int32_t a = base64_value((unsigned char) src[i]);
        int32_t b = base64_value((unsigned char) src[i + 1]);
        int32_t c = pad2 ? 0 : base64_value((unsigned char) src[i + 2]);
        int32_t d = pad3 ? 0 : base64_value((unsigned char) src[i + 3]);
        if (a < 0 || b < 0 || c < 0 || d < 0) return SIZE_MAX;
        if ((pad2 || pad3) && !last) return SIZE_MAX;
        if (pad2 && !pad3) return SIZE_MAX;
        if (pad2 && (b & 0x0F)) return SIZE_MAX;
        if (pad3 && !pad2 && (c & 0x03)) return SIZE_MAX;
        out += pad2 ? 1 : pad3 ? 2 : 3;
    }
    return out;
}

/* Decodes text base64_check accepted into bytes, or into values when bytes
   is NULL. */
static void base64_decode(const char *src, size_t len, char *bytes, double *values) {
    size_t j = 0;
    for (size_t i = 0; i < len; i += 4) {
        bool pad2 = src[i + 2] == '=';
        bool pad3 = src[i + 3] == '=';
        uint32_t n = (uint32_t) base64_value((unsigned char) src[i]) << 18
                   | (uint32_t) base64_value((unsigned char) src[i + 1]) << 12
                   | (pad2 ? 0U : (uint32_t) base64_value((unsigned char) src[i + 2]) << 6)
                   | (pad3 ? 0U : (uint32_t) base64_value((unsigned char) src[i + 3]));
        size_t count = pad2 ? 1 : pad3 ? 2 : 3;
        for (size_t k = 0; k < count; k++, j++) {
            unsigned char byte = (unsigned char) (n >> (16 - 8 * k));
            if (bytes != NULL) bytes[j] = (char) byte;
            else values[j] = (double) byte;
        }
    }
}

/* The i-th byte to encode: of the string s, or of the values, already checked
   as bytes, when s is NULL. */
static unsigned char byte_at(const char *s, const double *values, size_t i) {
    return s != NULL ? (unsigned char) s[i] : (unsigned char) values[i];
}

/* Writes the padded base64 text of len bytes into dst, which holds
   tobase64_len(len) + 1 bytes. */
static void base64_encode(const char *s, const double *values, size_t len, char *dst) {
    size_t j = 0;
    for (size_t i = 0; i < len; i += 3) {
        size_t rest = len - i;
        uint32_t n = (uint32_t) byte_at(s, values, i) << 16;
        if (rest > 1) n |= (uint32_t) byte_at(s, values, i + 1) << 8;
        if (rest > 2) n |= (uint32_t) byte_at(s, values, i + 2);
        dst[j++] = BASE64TABLE[(n >> 18) & 0x3F];
        dst[j++] = BASE64TABLE[(n >> 12) & 0x3F];
        dst[j++] = rest > 1 ? BASE64TABLE[(n >> 6) & 0x3F] : '=';
        dst[j++] = rest > 2 ? BASE64TABLE[n & 0x3F] : '=';
    }
    dst[j] = '\0';
}

/* The text of a real 1-D array of bytes, into buffer. Runs with the registry
   mutex held. */
static int32_t tobase64_into(CSOUND *csound, OPDS *perf_h, const CSN_ARRAY *src, CSN_STRDATA *buffer) {
    if (src->itype != CSN_REAL) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] csntobase64 takes a real array of bytes");
    }
    if (src->ndim > 1U) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] csntobase64 takes a 1-D array, got %u dimensions", src->ndim);
    }
    for (size_t i = 0; i < src->size; i++) {
        if (!is_byte(src->data[i])) {
            return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] csntobase64 takes bytes, whole numbers from 0 to 255, got %.15g at index %zu", src->data[i], i);
        }
    }
    size_t len_out = tobase64_len(src->size);
    if (len_out == SIZE_MAX) {
        return CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] csntobase64: %zu bytes give a text longer than a string can hold", src->size);
    }
    int32_t res = ensure_string_capacity(csound, perf_h, buffer, len_out + 1, false);
    if (res != OK) return res;
    base64_encode(NULL, src->data, src->size, buffer->data);
    return OK;
}

/* Shared by the i form, the k init and the k form. The k forms run again
   only when the source changed. */
static int32_t tobase64_helper(CSOUND *csound, CSN_TOBASE64 *p, bool is_perf, bool is_k_init) {
    OPDS *perf_h = is_perf ? &p->h : NULL;
    CSN_REGISTRY *reg = is_perf ? p->k_data.registry : get_registry(csound);
    CHECK_REGISTRY(csound, perf_h, reg);
    if (is_perf) CHECK_KTRIG(p->trig);
    if (!is_perf) {
        stamp_globals(csound, &p->h);
        p->k_data.registry = reg;
    }
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->source_handle }, 1);
    int32_t res = OK;
    if (is_perf && CAN_REUSE_LAST_RESULT(&p->k_data, p->source_handle, NULL)) goto done;
    res = tobase64_into(csound, perf_h, p->source_handle, &p->buffer);
    if (res != OK) goto done;
    COPY_STRING_TO_STRINGDAT(csound, &p->h, p->string_out, p->buffer.data);
    if (is_perf || is_k_init) PUBLISH_DERIVED_RESULT(&p->k_data, p->source_handle, NULL);
done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

/* One S to the base64 text of its bytes. An S carries no version, so the k
   form encodes on every pass its trigger is non-zero, as csnstrtoascii
   does. */
static int32_t tobase64_ss_helper(CSOUND *csound, CSN_BASE64CONV_STR *p, bool is_perf) {
    OPDS *perf_h = is_perf ? &p->h : NULL;
    if (is_perf) CHECK_KTRIG(p->trig);
    if (!IS_VALID_STRINGDAT(p->string_in)) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] Invalid/NULL string");
    }
    const char *s = p->string_in->data;
    size_t len = GET_STRINGDAT_LEN(s);
    size_t len_out = tobase64_len(len);
    if (len_out == SIZE_MAX) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] csntobase64: %zu bytes give a text longer than a string can hold", len);
    }
    int32_t res = ensure_string_capacity(csound, perf_h, &p->buffer, len_out + 1, false);
    if (res != OK) return res;
    base64_encode(s, NULL, len, p->buffer.data);
    COPY_STRING_TO_STRINGDAT(csound, &p->h, p->string_out, p->buffer.data);
    return OK;
}

/* One S of base64 text to the S of its bytes. A string cannot hold a zero
   byte, so text that decodes to one is refused. */
static int32_t frombase64_ss_helper(CSOUND *csound, CSN_BASE64CONV_STR *p, bool is_perf) {
    OPDS *perf_h = is_perf ? &p->h : NULL;
    if (is_perf) CHECK_KTRIG(p->trig);
    if (!IS_VALID_STRINGDAT(p->string_in)) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] Invalid/NULL string");
    }
    const char *s = p->string_in->data;
    size_t len = GET_STRINGDAT_LEN(s);
    size_t bad = 0;
    size_t size = base64_check(s, len, &bad);
    if (size == SIZE_MAX) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] csnfrombase64: not padded base64 (RFC 4648) at character %zu of %zu", bad, len);
    }
    int32_t res = ensure_string_capacity(csound, perf_h, &p->buffer, size + 1, false);
    if (res != OK) return res;
    base64_decode(s, len, p->buffer.data, NULL);
    const char *zero = memchr(p->buffer.data, '\0', size);
    if (zero != NULL) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] csnfrombase64: byte %zu is 0, which a string cannot hold; decode to an array instead", (size_t) (zero - p->buffer.data));
    }
    p->buffer.data[size] = '\0';
    COPY_STRING_TO_STRINGDAT(csound, &p->h, p->string_out, p->buffer.data);
    return OK;
}

/* One S of base64 text to a real 1-D array of its bytes, from 0 to 255. As
   for csnstrtoascii, the k form decodes on every pass its trigger is
   non-zero. */
static int32_t frombase64_helper(CSOUND *csound, CSN_FROMBASE64 *p, bool is_perf) {
    OPDS *perf_h = is_perf ? &p->h : NULL;
    CSN_REGISTRY *reg = is_perf ? p->k_data.registry : get_registry(csound);
    CHECK_REGISTRY(csound, perf_h, reg);
    if (is_perf) CHECK_KTRIG(p->trig);
    if (!IS_VALID_STRINGDAT(p->string_in)) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] Invalid/NULL string");
    }
    if (!is_perf) {
        stamp_globals(csound, &p->h);
        p->k_data.registry = reg;
    }
    const char *s = p->string_in->data;
    size_t len = GET_STRINGDAT_LEN(s);
    size_t bad = 0;
    size_t size = base64_check(s, len, &bad);
    if (size == SIZE_MAX) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] csnfrombase64: not padded base64 (RFC 4648) at character %zu of %zu", bad, len);
    }
    if (size > UINT32_MAX) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] csnfrombase64: %zu bytes are more than an array axis holds", size);
    }
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle }, 1);
    uint32_t shape[CSN_MAX_DIMS] = { (uint32_t) size };
    CSNARR_RES r = prepare_output(csound, &p->h, perf_h, reg, p->handle, 1U, shape, CSN_REAL, NULL, 0, CSN_ALIAS_REFUSE);
    pres.locked |= r.locked;
    int32_t res = r.err;
    if (res == OK) base64_decode(s, len, NULL, p->handle->data);
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnbinary_tobase64_deinit(CSOUND *csound, CSN_TOBASE64 *p) {
    csn_str_release(csound, &p->buffer);
    return OK;
}

int32_t csnbinary_base64conv_deinit(CSOUND *csound, CSN_BASE64CONV_STR *p) {
    csn_str_release(csound, &p->buffer);
    return OK;
}

int32_t csnbinary_tobase64(CSOUND *csound, CSN_TOBASE64 *p) {
    return tobase64_helper(csound, p, false, false);
}

int32_t csnbinary_tobase64_k_init(CSOUND *csound, CSN_TOBASE64 *p) {
    return tobase64_helper(csound, p, false, true);
}

int32_t csnbinary_tobase64_k(CSOUND *csound, CSN_TOBASE64 *p) {
    return tobase64_helper(csound, p, true, false);
}

int32_t csnbinary_tobase64_ss(CSOUND *csound, CSN_BASE64CONV_STR *p) {
    return tobase64_ss_helper(csound, p, false);
}

int32_t csnbinary_tobase64_ss_k_init(CSOUND *csound, CSN_BASE64CONV_STR *p) {
    return tobase64_ss_helper(csound, p, false);
}

int32_t csnbinary_tobase64_ss_k(CSOUND *csound, CSN_BASE64CONV_STR *p) {
    return tobase64_ss_helper(csound, p, true);
}

int32_t csnbinary_frombase64(CSOUND *csound, CSN_FROMBASE64 *p) {
    return frombase64_helper(csound, p, false);
}

int32_t csnbinary_frombase64_k_init(CSOUND *csound, CSN_FROMBASE64 *p) {
    return frombase64_helper(csound, p, false);
}

int32_t csnbinary_frombase64_k(CSOUND *csound, CSN_FROMBASE64 *p) {
    return frombase64_helper(csound, p, true);
}

int32_t csnbinary_frombase64_ss(CSOUND *csound, CSN_BASE64CONV_STR *p) {
    return frombase64_ss_helper(csound, p, false);
}

int32_t csnbinary_frombase64_ss_k_init(CSOUND *csound, CSN_BASE64CONV_STR *p) {
    return frombase64_ss_helper(csound, p, false);
}

int32_t csnbinary_frombase64_ss_k(CSOUND *csound, CSN_BASE64CONV_STR *p) {
    return frombase64_ss_helper(csound, p, true);
}


int32_t csnbinary_tobuffer(CSOUND *csound, CSN_BYTEBUFFER *p);
int32_t csnbinary_tobuffer_k_init(CSOUND *csound, CSN_BYTEBUFFER *p);
int32_t csnbinary_tobuffer_k(CSOUND *csound, CSN_BYTEBUFFER *p);
int32_t csnbinary_frombuffer(CSOUND *csound, CSN_BYTEBUFFER *p);
int32_t csnbinary_frombuffer_k_init(CSOUND *csound, CSN_BYTEBUFFER *p);
int32_t csnbinary_frombuffer_k(CSOUND *csound, CSN_BYTEBUFFER *p);



/* The digits of one S, after an optional 0b or 0x prefix, one element each:
   binstrtoarr gives a real array of 0 and 1, hexstrtoarr a CsnStr of the
   hex digits as written. An empty S gives an empty array, so a k form whose
   string is still empty at init (chnget, OSC) starts empty instead of
   failing. A string carries no version, so the k form compares it with the
   copy k_data keeps and recomputes only when it changed or something else
   wrote into the output. */
static int32_t strtoarr_helper(CSOUND *csound, CSN_STRTOARR *p, bool is_perf, bool is_binary) {
    const char *name = is_binary ? "csnbinstrtoarr" : "csnhexstrtoarr";
    OPDS *perf_h = is_perf ? &p->h : NULL;
    CSN_REGISTRY *reg = is_perf ? p->k_data.registry : get_registry(csound);
    CHECK_REGISTRY(csound, perf_h, reg);
    if (is_perf) CHECK_KTRIG(p->trig);
    if (!IS_VALID_STRINGDAT(p->string_in)) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] Invalid/NULL string");
    }

    const char *s = p->string_in->data;
    const char *digits = s[0] == '\0' ? s : skip_prefix_and_validate(s, is_binary);
    if (digits == NULL) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] %s: '%s' is not a %s string", name, s, BINHEX_NAME(is_binary));
    }
    size_t len = GET_STRINGDAT_LEN(digits);
    if (len > UINT32_MAX) {
        return CSN_ACCESSOR_ERROR(csound, perf_h, "[csnarray] %s: %zu digits are more than an array axis holds", name, len);
    }

    if (!is_perf) {
        stamp_globals(csound, &p->h);
        p->k_data.registry = reg;
    }
    int32_t res = OK;
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { p->handle }, 1);

    /* The output stands in for the source: its version moves when anything
       else writes into it. */
    if (is_perf && CAN_REUSE_STRING_ELEMENTWISE(&p->k_data, p->handle, NULL, NULL, s, NULL)) goto done;

    uint32_t shape[CSN_MAX_DIMS] = { (uint32_t) len };
    ITEM_TYPE itype = is_binary ? CSN_REAL : CSN_STRING;
    CSNARR_RES r = prepare_output(csound, &p->h, perf_h, reg, p->handle, 1U, shape, itype, NULL, 0, CSN_ALIAS_REFUSE);
    pres.locked |= r.locked;
    res = r.err;
    if (res != OK) goto done;

    if (is_binary) {
        for (size_t i = 0; i < len; i++) p->handle->data[i] = digits[i] == '1' ? 1.0 : 0.0;
    } else {
        CSN_STR_WRITER w = csn_str_writer(csound, perf_h, p->handle);
        for (size_t i = 0; i < len; i++) {
            const char digit[2] = { digits[i], '\0' };
            res = csn_str_write(&w, &p->handle->sdata[i], digit, 1);
            if (res != OK) goto done;
        }
    }

    PUBLISH_STRING_ELEMENTWISE(csound, &p->k_data, p->handle, NULL, NULL, s, NULL, !p->handle->rt_locked);

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnbinary_strtoarr_deinit(CSOUND *csound, CSN_STRTOARR *p) {
    csn_kdata_release_strings(csound, &p->k_data);
    return OK;
}

int32_t csnbinary_binstrtoarr(CSOUND *csound, CSN_STRTOARR *p) {
    return strtoarr_helper(csound, p, false, true);
}

int32_t csnbinary_binstrtoarr_k_init(CSOUND *csound, CSN_STRTOARR *p) {
    return strtoarr_helper(csound, p, false, true);
}

int32_t csnbinary_binstrtoarr_k(CSOUND *csound, CSN_STRTOARR *p) {
    return strtoarr_helper(csound, p, true, true);
}

int32_t csnbinary_hexstrtoarr(CSOUND *csound, CSN_STRTOARR *p) {
    return strtoarr_helper(csound, p, false, false);
}

int32_t csnbinary_hexstrtoarr_k_init(CSOUND *csound, CSN_STRTOARR *p) {
    return strtoarr_helper(csound, p, false, false);
}

int32_t csnbinary_hexstrtoarr_k(CSOUND *csound, CSN_STRTOARR *p) {
    return strtoarr_helper(csound, p, true, false);
}
