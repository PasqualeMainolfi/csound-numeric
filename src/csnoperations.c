/* The orchestra operators on CsnArr: + - * / ^, their compound forms
   += -= *= /=, unary minus and assignment. The computation is the csnadd
   family's (csn_binop_hh_* and csn_binop_hs_sh_* in csnmath.c); this file only
   decides when it runs. The public opcode inventory remains centralized in
   csnum.c. */
#include "csnstrings.h"
#include "csnum.h"
#include "csnum_internal.h"
#include "csnregistry.h"
#include "csnoperations.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* Records z as the last scalar seen and reports whether it moved. */
static bool complex_scalar_moved(const COMPLEXDAT *z, CSN_COMPLEXDAT *last) {
    double re = 0.0, im = 0.0;
    complexdat_to_rect(z, &re, &im);
    bool moved = re != last->re || im != last->im;
    last->re = re;
    last->im = im;
    return moved;
}

/* Every operator has a perf pass, so prepare_output marks its output k-rate as
   it would for any k-rate opcode. An operator's output is k-rate only when an
   operand is, so the inits set it themselves once the core has run. The perf
   passes read the operands' rate again on every pass rather than trusting the
   init: a k-rate opcode that writes an operand may come later in the
   instrument, and marks it only at its own init. */

// out = a op b

static int32_t op_hh_init(CSOUND *csound, CSN_OP_HH *p, CSN_BINOP_MODE mode) {
    int32_t res = csn_binop_hh_init(csound, &p->h, p->out, p->a, p->b, &p->k_data, mode, is_krate(p->a) || is_krate(p->b));
    if (res == OK) p->out->rate = rate_of(is_krate(p->a) || is_krate(p->b));
    return res;
}

static int32_t op_hh_perf(CSOUND *csound, CSN_OP_HH *p, CSN_BINOP_MODE mode) {
    if (!is_krate(p->a) && !is_krate(p->b)) return OK;
    p->out->rate = CSN_RATE_K;
    return csn_binop_hh_perf(csound, &p->h, p->out, p->a, p->b, &p->k_data, mode);
}

// out = a op s, out = s op a

static int32_t op_hs_init(CSOUND *csound, CSN_OP_HS *p, CSN_BINOP_MODE mode, bool scalar_k) {
    p->scalar_k = scalar_k;
    int32_t res = csn_binop_hs_sh_init(csound, &p->h, p->out, p->a, p->s, NULL, &p->k_data, mode, is_krate(p->a) || scalar_k);
    if (res == OK) p->out->rate = rate_of(is_krate(p->a) || scalar_k);
    return res;
}

static int32_t op_hs_perf(CSOUND *csound, CSN_OP_HS *p, CSN_BINOP_MODE mode) {
    if (!is_krate(p->a) && !p->scalar_k) return OK;
    p->out->rate = CSN_RATE_K;
    return csn_binop_hs_sh_perf(csound, &p->h, p->out, p->a, p->s, NULL, &p->k_data, mode);
}

static int32_t op_sh_init(CSOUND *csound, CSN_OP_SH *p, CSN_BINOP_MODE mode, bool scalar_k) {
    p->scalar_k = scalar_k;
    int32_t res = csn_binop_hs_sh_init(csound, &p->h, p->out, p->a, p->s, NULL, &p->k_data, mode, is_krate(p->a) || scalar_k);
    if (res == OK) p->out->rate = rate_of(is_krate(p->a) || scalar_k);
    return res;
}

static int32_t op_sh_perf(CSOUND *csound, CSN_OP_SH *p, CSN_BINOP_MODE mode) {
    if (!is_krate(p->a) && !p->scalar_k) return OK;
    p->out->rate = CSN_RATE_K;
    return csn_binop_hs_sh_perf(csound, &p->h, p->out, p->a, p->s, NULL, &p->k_data, mode);
}

// out = a op z, out = z op a: the scalar's rate is unknown, its value is not

static int32_t op_hc_init(CSOUND *csound, CSN_OP_HC *p, CSN_BINOP_MODE mode) {
    (void) complex_scalar_moved(p->z, &p->last_z);
    int32_t res = csn_binop_hs_sh_init(csound, &p->h, p->out, p->a, NULL, p->z, &p->k_data, mode, is_krate(p->a));
    if (res == OK) p->out->rate = rate_of(is_krate(p->a));
    return res;
}

static int32_t op_hc_perf(CSOUND *csound, CSN_OP_HC *p, CSN_BINOP_MODE mode) {
    bool moved = complex_scalar_moved(p->z, &p->last_z);
    if (!is_krate(p->a) && !moved) return OK;
    p->out->rate = CSN_RATE_K;
    return csn_binop_hs_sh_perf(csound, &p->h, p->out, p->a, NULL, p->z, &p->k_data, mode);
}

static int32_t op_ch_init(CSOUND *csound, CSN_OP_CH *p, CSN_BINOP_MODE mode) {
    (void) complex_scalar_moved(p->z, &p->last_z);
    int32_t res = csn_binop_hs_sh_init(csound, &p->h, p->out, p->a, NULL, p->z, &p->k_data, mode, is_krate(p->a));
    if (res == OK) p->out->rate = rate_of(is_krate(p->a));
    return res;
}

static int32_t op_ch_perf(CSOUND *csound, CSN_OP_CH *p, CSN_BINOP_MODE mode) {
    bool moved = complex_scalar_moved(p->z, &p->last_z);
    if (!is_krate(p->a) && !moved) return OK;
    p->out->rate = CSN_RATE_K;
    return csn_binop_hs_sh_perf(csound, &p->h, p->out, p->a, NULL, p->z, &p->k_data, mode);
}

/* out = a op sig, out = sig op a: the audio block is a k-rate operand of shape
   (ksmps,), so a (2, 1) array times a signal gives (2, ksmps), one row per
   channel, ready for csnunpack. The block is marked as a real-time path, as
   csnfromaudio marks its output by default, and the mark travels to out. The
   samples outside the note's active window read as zero. Nothing has computed
   the signal during the init pass, so the init only lays the output out and
   the first pass computes. */

static void audio_block_fill(const OPDS *h, CSN_ARRAY *block, const cs_float *sig) {
    uint32_t nsamples = h->insdshead->ksmps;
    uint32_t offset = h->insdshead->ksmps_offset;
    uint32_t early = h->insdshead->ksmps_no_end;

    if (offset) memset(block->data, 0, sizeof(double) * offset);
    if (early) {
        nsamples -= early;
        memset(block->data + nsamples, 0, sizeof(double) * early);
    }
    for (uint32_t i = offset; i < nsamples; i++) block->data[i] = (double) sig[i];
    block->pending = false;
    update_array_data_version(&block->version);
}

/* Freed first, so a reinit does not leak the block of the previous init. */
static int32_t audio_block_init(CSOUND *csound, OPDS *h, CSN_ARRAY *block) {
    FREE_CSNARRDATA(csound, block);
    uint32_t shape[CSN_MAX_DIMS] = {0};
    shape[0] = h->insdshead->ksmps;
    if (allocate_array(csound, block, 1U, shape, CSN_REAL) != OK) {
        return csn_init_error(csound, "[csnarray] Out of memory: allocating the audio block of '%s' failed", get_out_name(h));
    }
    block->rate = CSN_RATE_K;
    block->rt_locked = true;
    block->pending = true;
    return OK;
}

static int32_t op_ha_init(CSOUND *csound, CSN_OP_HA *p, CSN_BINOP_MODE mode) {
    int32_t res = audio_block_init(csound, &p->h, &p->block);
    if (res != OK) return res;
    res = csn_binop_hh_init(csound, &p->h, p->out, p->a, &p->block, &p->k_data, mode, true);
    if (res == OK) p->out->rate = CSN_RATE_K;
    return res;
}

static int32_t op_ha_perf(CSOUND *csound, CSN_OP_HA *p, CSN_BINOP_MODE mode) {
    audio_block_fill(&p->h, &p->block, p->sig);
    p->out->rate = CSN_RATE_K;
    return csn_binop_hh_perf(csound, &p->h, p->out, p->a, &p->block, &p->k_data, mode);
}

static int32_t op_ah_init(CSOUND *csound, CSN_OP_AH *p, CSN_BINOP_MODE mode) {
    int32_t res = audio_block_init(csound, &p->h, &p->block);
    if (res != OK) return res;
    res = csn_binop_hh_init(csound, &p->h, p->out, &p->block, p->a, &p->k_data, mode, true);
    if (res == OK) p->out->rate = CSN_RATE_K;
    return res;
}

static int32_t op_ah_perf(CSOUND *csound, CSN_OP_AH *p, CSN_BINOP_MODE mode) {
    audio_block_fill(&p->h, &p->block, p->sig);
    p->out->rate = CSN_RATE_K;
    return csn_binop_hh_perf(csound, &p->h, p->out, &p->block, p->a, &p->k_data, mode);
}

int32_t csnop_ha_deinit(CSOUND *csound, CSN_OP_HA *p) {
    FREE_CSNARRDATA(csound, &p->block);
    return OK;
}

int32_t csnop_ah_deinit(CSOUND *csound, CSN_OP_AH *p) {
    FREE_CSNARRDATA(csound, &p->block);
    return OK;
}

/* x op= y, x op= s. x is the left operand and the output at once, which
   CSN_ALIAS_ELEMENTWISE lets the core compute in place (and refuses when y
   would broadcast x to a larger shape). x keeps its own rate and turns k-rate
   only when the right operand is: X += Y on i-rate arrays runs once, as
   ix += 1 does, while X += kY accumulates on every pass, as kx += 1 does. */

static int32_t op_in_hh_init(CSOUND *csound, CSN_OP_IN_HH *p, CSN_BINOP_MODE mode) {
    uint8_t rate = p->x->rate;
    int32_t res = csn_binop_hh_init(csound, &p->h, p->x, p->x, p->y, &p->k_data, mode, is_krate(p->x) || is_krate(p->y));
    p->x->rate = rate;
    if (res == OK && is_krate(p->y)) csn_mark_krate(p->x);
    return res;
}

static int32_t op_in_hh_perf(CSOUND *csound, CSN_OP_IN_HH *p, CSN_BINOP_MODE mode) {
    if (!is_krate(p->x) && !is_krate(p->y)) return OK;
    csn_mark_krate(p->x);
    return csn_binop_hh_perf(csound, &p->h, p->x, p->x, p->y, &p->k_data, mode);
}

static int32_t op_in_hs_init(CSOUND *csound, CSN_OP_IN_HS *p, CSN_BINOP_MODE mode, bool scalar_k) {
    p->scalar_k = scalar_k;
    uint8_t rate = p->x->rate;
    int32_t res = csn_binop_hs_sh_init(csound, &p->h, p->x, p->x, p->s, NULL, &p->k_data, mode, is_krate(p->x) || scalar_k);
    p->x->rate = rate;
    if (res == OK && scalar_k) csn_mark_krate(p->x);
    return res;
}

static int32_t op_in_hs_perf(CSOUND *csound, CSN_OP_IN_HS *p, CSN_BINOP_MODE mode) {
    if (!is_krate(p->x) && !p->scalar_k) return OK;
    csn_mark_krate(p->x);
    return csn_binop_hs_sh_perf(csound, &p->h, p->x, p->x, p->s, NULL, &p->k_data, mode);
}

/* One family per operator. The array-scalar forms reuse the hs/sh modes, the
   complex-scalar forms the same ones with a :Complex; operand, and the audio
   forms the hh mode, the block being an array. */
#define CSN_OP_DEFINE(name, HH, HS, SH)                                                                                      \
    int32_t csnop_##name##_hh_init(CSOUND *csound, CSN_OP_HH *p) { return op_hh_init(csound, p, HH); }                       \
    int32_t csnop_##name##_hh_perf(CSOUND *csound, CSN_OP_HH *p) { return op_hh_perf(csound, p, HH); }                       \
    int32_t csnop_##name##_hi_init(CSOUND *csound, CSN_OP_HS *p) { return op_hs_init(csound, p, HS, false); }                \
    int32_t csnop_##name##_hk_init(CSOUND *csound, CSN_OP_HS *p) { return op_hs_init(csound, p, HS, true); }                 \
    int32_t csnop_##name##_hs_perf(CSOUND *csound, CSN_OP_HS *p) { return op_hs_perf(csound, p, HS); }                       \
    int32_t csnop_##name##_ih_init(CSOUND *csound, CSN_OP_SH *p) { return op_sh_init(csound, p, SH, false); }                \
    int32_t csnop_##name##_kh_init(CSOUND *csound, CSN_OP_SH *p) { return op_sh_init(csound, p, SH, true); }                 \
    int32_t csnop_##name##_sh_perf(CSOUND *csound, CSN_OP_SH *p) { return op_sh_perf(csound, p, SH); }                       \
    int32_t csnop_##name##_hc_init(CSOUND *csound, CSN_OP_HC *p) { return op_hc_init(csound, p, HS); }                       \
    int32_t csnop_##name##_hc_perf(CSOUND *csound, CSN_OP_HC *p) { return op_hc_perf(csound, p, HS); }                       \
    int32_t csnop_##name##_ch_init(CSOUND *csound, CSN_OP_CH *p) { return op_ch_init(csound, p, SH); }                       \
    int32_t csnop_##name##_ch_perf(CSOUND *csound, CSN_OP_CH *p) { return op_ch_perf(csound, p, SH); }                       \
    int32_t csnop_##name##_ha_init(CSOUND *csound, CSN_OP_HA *p) { return op_ha_init(csound, p, HH); }                       \
    int32_t csnop_##name##_ha_perf(CSOUND *csound, CSN_OP_HA *p) { return op_ha_perf(csound, p, HH); }                       \
    int32_t csnop_##name##_ah_init(CSOUND *csound, CSN_OP_AH *p) { return op_ah_init(csound, p, HH); }                       \
    int32_t csnop_##name##_ah_perf(CSOUND *csound, CSN_OP_AH *p) { return op_ah_perf(csound, p, HH); }

#define CSN_OP_IN_DEFINE(name, HH, HS)                                                                                       \
    int32_t csnop_##name##in_hh_init(CSOUND *csound, CSN_OP_IN_HH *p) { return op_in_hh_init(csound, p, HH); }               \
    int32_t csnop_##name##in_hh_perf(CSOUND *csound, CSN_OP_IN_HH *p) { return op_in_hh_perf(csound, p, HH); }               \
    int32_t csnop_##name##in_hi_init(CSOUND *csound, CSN_OP_IN_HS *p) { return op_in_hs_init(csound, p, HS, false); }        \
    int32_t csnop_##name##in_hk_init(CSOUND *csound, CSN_OP_IN_HS *p) { return op_in_hs_init(csound, p, HS, true); }         \
    int32_t csnop_##name##in_hs_perf(CSOUND *csound, CSN_OP_IN_HS *p) { return op_in_hs_perf(csound, p, HS); }

CSN_OP_DEFINE(add, CSN_ADD_HH, CSN_ADD_HS, CSN_ADD_SH)
CSN_OP_DEFINE(sub, CSN_SUB_HH, CSN_SUB_HS, CSN_SUB_SH)
CSN_OP_DEFINE(mul, CSN_MUL_HH, CSN_MUL_HS, CSN_MUL_SH)
CSN_OP_DEFINE(div, CSN_DIV_HH, CSN_DIV_HS, CSN_DIV_SH)
CSN_OP_DEFINE(pow, CSN_POW_HH, CSN_POW_HS, CSN_POW_SH)

CSN_OP_IN_DEFINE(add, CSN_ADD_HH, CSN_ADD_HS)
CSN_OP_IN_DEFINE(sub, CSN_SUB_HH, CSN_SUB_HS)
CSN_OP_IN_DEFINE(mul, CSN_MUL_HH, CSN_MUL_HS)
CSN_OP_IN_DEFINE(div, CSN_DIV_HH, CSN_DIV_HS)

/* X = A. The copy goes through prepare_output, so X keeps its storage when it
   is large enough, is refused growth at perf on a real-time path, and gets a
   new data version. X takes A's rate; the perf pass copies only while A is
   k-rate, and only when A or X moved since the last copy. */
int32_t op_assign_run(CSOUND *csound, CSN_OP_ASSIGN *p, OPDS *perf_h) {
    CSN_REGISTRY *reg = p->registry;
    CSN_ARRAY *in = p->in;
    CSN_ARRAY *out = p->out;
    if (in == out) return OK;

    int32_t res = OK;
    CSNARR_RES pres = {0};
    pres.locked = csn_lock_if_shared(csound, reg, (CSN_CONARR) { out, in }, 2);

    if (in->data == NULL) {
        res = CSN_ACCESSOR_ERROR_LOCKED(csound, perf_h, "[csnarray] The array being assigned has never been created");
        goto done;
    }

    if (perf_h != NULL && p->is_published
        && is_same_array_version(&in->version, &p->prev_in_version)
        && is_same_array_version(&out->version, &p->prev_out_version)) goto done;

    const CSN_ARRAY *ops[1] = { in };
    CSNARR_RES r = prepare_output(csound, &p->h, perf_h, reg, out, in->ndim, in->shape, in->itype, ops, 1, CSN_ALIAS_REFUSE);
    res = r.err;
    if (res != OK) goto done;
    pres.locked |= r.locked;

    out->size = in->size;
    /* strings go into X's slot buffers; on a real-time path one that does
       not fit stops the pass */
    CSN_STR_WRITER w = csn_str_writer(csound, perf_h, out);
    res = CSN_MEMCPY(&w, out, in);
    update_array_data_version(&out->version);
    if (res != OK) goto done;
    out->rate = in->rate;
    csn_copy_array_kind(out, in);

    set_array_version(&p->prev_in_version, &in->version);
    set_array_version(&p->prev_out_version, &out->version);
    p->is_published = true;

done:
    if (pres.locked) csn_unlock(csound, reg);
    return res;
}

int32_t csnop_assign_init(CSOUND *csound, CSN_OP_ASSIGN *p) {
    CSN_REGISTRY *reg = get_registry(csound);
    CHECK_REGISTRY(csound, NULL, reg);
    p->registry = reg;
    p->is_published = false;
    stamp_globals(csound, &p->h);
    return op_assign_run(csound, p, NULL);
}

int32_t csnop_assign_perf(CSOUND *csound, CSN_OP_ASSIGN *p) {
    CSN_REGISTRY *reg = p->registry;
    CHECK_REGISTRY(csound, &p->h, reg);
    if (!is_krate(p->in)) return OK;
    return op_assign_run(csound, p, &p->h);
}


// --- strings ---

/* The same rules on CsnStr, with the csnstradd and csnstrmul cores. An S has
   no rate, so no overload says whether a string operand changes at perf: the
   k_data keeps a copy of it from init on, and a perf pass on an i-rate array
   runs only when the string moved, turning its output k-rate as an operand
   changing at perf does. The copy is released at deinit. */

static bool string_scalar_moved(const K_DATA *k_data, const STRINGDAT *s) {
    if (s == NULL || s->data == NULL) return true;
    if (k_data->prev_strings_stale || k_data->prev_string.data == NULL) return true;
    return strcmp(k_data->prev_string.data, s->data) != 0;
}

/* addstring_hs_sh_init keeps the copy only for a k array; an i-rate one
   needs it too, to see the string move. The rate is read first: for x += s,
   a is the output, which prepare_output marks k-rate. */
static int32_t opstr_add_init(CSOUND *csound, OPDS *h, CSN_ARRAY *out, CSN_ARRAY *a, STRINGDAT *s, K_DATA *k_data, bool is_left) {
    bool a_krate = is_krate(a);
    int32_t res = addstring_hs_sh_init(csound, h, out, a, s, k_data, a_krate, is_left);
    if (res == OK && !a_krate) csn_kdata_keep_strings(csound, k_data, s->data, NULL, true);
    return res;
}

int32_t opstr_add_hh_init(CSOUND *csound, CSN_OP_HH *p) {
    int32_t res = string_binop_hh_init(csound, &p->h, p->out, p->a, p->b, &p->k_data, CSN_STR_ADD, is_krate(p->a) || is_krate(p->b));
    if (res == OK) p->out->rate = rate_of(is_krate(p->a) || is_krate(p->b));
    return res;
}

int32_t opstr_add_hh_perf(CSOUND *csound, CSN_OP_HH *p) {
    if (!is_krate(p->a) && !is_krate(p->b)) return OK;
    p->out->rate = CSN_RATE_K;
    return string_binop_hh_perf(csound, &p->h, p->out, p->a, p->b, &p->k_data, CSN_STR_ADD);
}

int32_t opstr_add_hs_init(CSOUND *csound, CSN_OP_STRING_HS *p) {
    int32_t res = opstr_add_init(csound, &p->h, p->out, p->a, p->s, &p->k_data, false);
    if (res == OK) p->out->rate = rate_of(is_krate(p->a));
    return res;
}

int32_t opstr_add_hs_perf(CSOUND *csound, CSN_OP_STRING_HS *p) {
    if (!is_krate(p->a) && !string_scalar_moved(&p->k_data, p->s)) return OK;
    p->out->rate = CSN_RATE_K;
    return addstring_hs_sh_perf(csound, &p->h, p->out, p->a, p->s, &p->k_data, false);
}

int32_t opstr_add_hs_deinit(CSOUND *csound, CSN_OP_STRING_HS *p) {
    csn_kdata_release_strings(csound, &p->k_data);
    return OK;
}

int32_t opstr_add_sh_init(CSOUND *csound, CSN_OP_STRING_SH *p) {
    int32_t res = opstr_add_init(csound, &p->h, p->out, p->a, p->s, &p->k_data, true);
    if (res == OK) p->out->rate = rate_of(is_krate(p->a));
    return res;
}

int32_t opstr_add_sh_perf(CSOUND *csound, CSN_OP_STRING_SH *p) {
    if (!is_krate(p->a) && !string_scalar_moved(&p->k_data, p->s)) return OK;
    p->out->rate = CSN_RATE_K;
    return addstring_hs_sh_perf(csound, &p->h, p->out, p->a, p->s, &p->k_data, true);
}

int32_t opstr_add_sh_deinit(CSOUND *csound, CSN_OP_STRING_SH *p) {
    csn_kdata_release_strings(csound, &p->k_data);
    return OK;
}

int32_t opstr_mul_hsi_init(CSOUND *csound, CSN_OP_HS *p) {
    p->scalar_k = false;
    int32_t res = strmul_hs_init(csound, &p->h, p->out, p->a, p->s, &p->k_data, is_krate(p->a));
    if (res == OK) p->out->rate = rate_of(is_krate(p->a));
    return res;
}

int32_t opstr_mul_hsk_init(CSOUND *csound, CSN_OP_HS *p) {
    p->scalar_k = true;
    int32_t res = strmul_hs_init(csound, &p->h, p->out, p->a, p->s, &p->k_data, true);
    if (res == OK) p->out->rate = CSN_RATE_K;
    return res;
}

int32_t opstr_mul_hs_perf(CSOUND *csound, CSN_OP_HS *p) {
    if (!is_krate(p->a) && !p->scalar_k) return OK;
    p->out->rate = CSN_RATE_K;
    return strmul_hs_perf(csound, &p->h, p->out, p->a, p->s, &p->k_data);
}

int32_t opstr_mul_shi_init(CSOUND *csound, CSN_OP_SH *p) {
    p->scalar_k = false;
    int32_t res = strmul_hs_init(csound, &p->h, p->out, p->a, p->s, &p->k_data, is_krate(p->a));
    if (res == OK) p->out->rate = rate_of(is_krate(p->a));
    return res;
}

int32_t opstr_mul_shk_init(CSOUND *csound, CSN_OP_SH *p) {
    p->scalar_k = true;
    int32_t res = strmul_hs_init(csound, &p->h, p->out, p->a, p->s, &p->k_data, true);
    if (res == OK) p->out->rate = CSN_RATE_K;
    return res;
}

int32_t opstr_mul_sh_perf(CSOUND *csound, CSN_OP_SH *p) {
    if (!is_krate(p->a) && !p->scalar_k) return OK;
    p->out->rate = CSN_RATE_K;
    return strmul_hs_perf(csound, &p->h, p->out, p->a, p->s, &p->k_data);
}

/* x += y, x += s, x *= n, as their numeric forms: x keeps its own rate and
   turns k-rate only when the right operand is, or, for a string, when it
   moves at perf. X += "a" on an i-rate X appends once, and again only when
   the string changes; on a k-rate X it appends on every pass, as kx += 1
   adds. */

int32_t opstr_addin_hh_init(CSOUND *csound, CSN_OP_IN_HH *p) {
    uint8_t rate = p->x->rate;
    int32_t res = string_binop_hh_init(csound, &p->h, p->x, p->x, p->y, &p->k_data, CSN_STR_ADD, is_krate(p->x) || is_krate(p->y));
    p->x->rate = rate;
    if (res == OK && is_krate(p->y)) csn_mark_krate(p->x);
    return res;
}

int32_t opstr_addin_hh_perf(CSOUND *csound, CSN_OP_IN_HH *p) {
    if (!is_krate(p->x) && !is_krate(p->y)) return OK;
    csn_mark_krate(p->x);
    return string_binop_hh_perf(csound, &p->h, p->x, p->x, p->y, &p->k_data, CSN_STR_ADD);
}

int32_t opstr_addin_hs_init(CSOUND *csound, CSN_OP_STRING_IN_HS *p) {
    uint8_t rate = p->x->rate;
    int32_t res = opstr_add_init(csound, &p->h, p->x, p->x, p->s, &p->k_data, false);
    p->x->rate = rate;
    return res;
}

int32_t opstr_addin_hs_perf(CSOUND *csound, CSN_OP_STRING_IN_HS *p) {
    if (!is_krate(p->x) && !string_scalar_moved(&p->k_data, p->s)) return OK;
    csn_mark_krate(p->x);
    return addstring_hs_sh_perf(csound, &p->h, p->x, p->x, p->s, &p->k_data, false);
}

int32_t opstr_addin_hs_deinit(CSOUND *csound, CSN_OP_STRING_IN_HS *p) {
    csn_kdata_release_strings(csound, &p->k_data);
    return OK;
}

static int32_t opstr_mulin_init(CSOUND *csound, CSN_OP_IN_HS *p, bool scalar_k) {
    p->scalar_k = scalar_k;
    uint8_t rate = p->x->rate;
    int32_t res = strmul_hs_init(csound, &p->h, p->x, p->x, p->s, &p->k_data, is_krate(p->x) || scalar_k);
    p->x->rate = rate;
    if (res == OK && scalar_k) csn_mark_krate(p->x);
    return res;
}

int32_t opstr_mulin_hi_init(CSOUND *csound, CSN_OP_IN_HS *p) {
    return opstr_mulin_init(csound, p, false);
}

int32_t opstr_mulin_hk_init(CSOUND *csound, CSN_OP_IN_HS *p) {
    return opstr_mulin_init(csound, p, true);
}

int32_t opstr_mulin_hs_perf(CSOUND *csound, CSN_OP_IN_HS *p) {
    if (!is_krate(p->x) && !p->scalar_k) return OK;
    csn_mark_krate(p->x);
    return strmul_hs_perf(csound, &p->h, p->x, p->x, p->s, &p->k_data);
}

int32_t opstr_add_ss_deinit(CSOUND *csound, CSN_OP_STRADD_SS *p) {
    csn_str_release(csound, &p->buffer);
    csn_kdata_release_strings(csound, &p->k_data);
    return OK;
}

int32_t opstr_addin_ss_deinit(CSOUND *csound, CSN_OP_STRADD_SS_IN *p) {
    csn_str_release(csound, &p->buffer);
    csn_kdata_release_strings(csound, &p->k_data);
    return OK;
}

int32_t opstr_mul_ss_deinit(CSOUND *csound, CSN_OP_STRMUL_SS *p) {
    csn_str_release(csound, &p->buffer);
    csn_kdata_release_strings(csound, &p->k_data);
    return OK;
}

int32_t opstr_mulin_ss_deinit(CSOUND *csound, CSN_OP_STRMUL_SS_IN *p) {
    csn_str_release(csound, &p->buffer);
    csn_kdata_release_strings(csound, &p->k_data);
    return OK;
}

/* S1 + S2 and Sx += Sy: at init, then at perf when an operand changed. */
int32_t opstr_add_ss_init(CSOUND *csound, CSN_OP_STRADD_SS *p) {
    return string_addmul_ss_op(csound, &p->h, false, false, &p->k_data, &p->buffer, p->out, p->a, p->b, NULL, true);
}

int32_t opstr_add_ss_perf(CSOUND *csound, CSN_OP_STRADD_SS *p) {
    return string_addmul_ss_op(csound, &p->h, true, false, &p->k_data, &p->buffer, p->out, p->a, p->b, NULL, true);
}

int32_t opstr_addin_ss_init(CSOUND *csound, CSN_OP_STRADD_SS_IN *p) {
    return string_addmul_ss_op(csound, &p->h, false, false, &p->k_data, &p->buffer, p->a, p->a, p->b, NULL, true);
}

int32_t opstr_addin_ss_perf(CSOUND *csound, CSN_OP_STRADD_SS_IN *p) {
    return string_addmul_ss_op(csound, &p->h, true, false, &p->k_data, &p->buffer, p->a, p->a, p->b, NULL, true);
}

/* S * n and Sx *= n: an i count follows the string's changes, a k count
   runs on every pass. */
int32_t opstr_mul_ss_init(CSOUND *csound, CSN_OP_STRMUL_SS *p) {
    return string_addmul_ss_op(csound, &p->h, false, false, &p->k_data, &p->buffer, p->out, p->a, NULL, p->value, false);
}

int32_t opstr_mul_ss_perf(CSOUND *csound, CSN_OP_STRMUL_SS *p) {
    return string_addmul_ss_op(csound, &p->h, true, false, &p->k_data, &p->buffer, p->out, p->a, NULL, p->value, false);
}

int32_t opstr_mul_ssk_perf(CSOUND *csound, CSN_OP_STRMUL_SS *p) {
    return string_addmul_ss_op(csound, &p->h, true, true, &p->k_data, &p->buffer, p->out, p->a, NULL, p->value, false);
}

int32_t opstr_mulin_ss_init(CSOUND *csound, CSN_OP_STRMUL_SS_IN *p) {
    return string_addmul_ss_op(csound, &p->h, false, false, &p->k_data, &p->buffer, p->a, p->a, NULL, p->value, false);
}

int32_t opstr_mulin_ss_perf(CSOUND *csound, CSN_OP_STRMUL_SS_IN *p) {
    return string_addmul_ss_op(csound, &p->h, true, false, &p->k_data, &p->buffer, p->a, p->a, NULL, p->value, false);
}

int32_t opstr_mulin_ssk_perf(CSOUND *csound, CSN_OP_STRMUL_SS_IN *p) {
    return string_addmul_ss_op(csound, &p->h, true, true, &p->k_data, &p->buffer, p->a, p->a, NULL, p->value, false);
}
