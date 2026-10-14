/* The orchestra operators on CsnArr: + - * / ^, their compound forms
   += -= *= /=, unary minus and assignment. The computation is the csnadd
   family's (csn_binop_hh_* and csn_binop_hs_sh_* in csnmath.c); this file only
   decides when it runs. The public opcode inventory remains centralized in
   csnum.c. */
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

/* One family per operator. The array-scalar forms reuse the hs/sh modes, and
   the complex-scalar forms the same ones with a :Complex; operand. */
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
    int32_t csnop_##name##_ch_perf(CSOUND *csound, CSN_OP_CH *p) { return op_ch_perf(csound, p, SH); }

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

    /* a pass that copies strings allocates them; one with nothing to copy
       has already returned */
    if (perf_h != NULL && in->itype == CSN_STRING && out->rt_locked) {
        res = csn_locked_perf_error(csound, perf_h, "[csnarray] '%s' allocates its strings at perf time and the array is on a real-time path; build it at i-rate, clear the mark with csnrtunlock, or pass irt=0 at the audio source it descends from", get_out_name(&p->h));
        goto done;
    }

    const CSN_ARRAY *ops[1] = { in };
    CSNARR_RES r = prepare_output(csound, &p->h, perf_h, reg, out, in->ndim, in->shape, in->itype, ops, 1, CSN_ALIAS_REFUSE);
    res = r.err;
    if (res != OK) goto done;
    pres.locked |= r.locked;

    out->size = in->size;
    CSN_MEMCPY(csound, out, in);
    update_array_data_version(&out->version);
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
