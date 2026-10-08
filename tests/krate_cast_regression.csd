<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

/* The rate casts. An operator runs at k only while one of its operands is
   k-rate; csnk marks an array k (in place) or returns a k-rate copy of it,
   csni returns a copy frozen at init. Four passes, B = 1. */

gkAcc   init 0
gkNoCast init 0
gkExpr  init 0
gkAssign init 0
gkLive  init 0
gkSnap  init 0
gkSnapSum init 0

instr 1
    iShape[] = fillarray(2)
    B:CsnArr = csnfull(iShape, 1)

    ; in place: A lives at k, so A += B accumulates on every pass
    Acc:CsnArr = csnzeros(iShape)
    csnk Acc
    Acc += B

    ; no cast: all operands i-rate, applied once at init
    NoCast:CsnArr = csnzeros(iShape)
    NoCast += B

    ; expression cast of a fixed array: computed, then reused
    Fixed:CsnArr = csnfull(iShape, 10)
    Expr:CsnArr = csnk(Fixed) + B

    ; a k-rate assignment recopies its source on every pass, as kx = 5
    ; followed by kx += 1 gives 6 on every pass
    Assign:CsnArr = csnk(Fixed)
    Assign += B

    ; csni freezes a k array at its init value: 0 here, since the first
    ; accumulation happens at perf
    Live:CsnArr = csnzeros(iShape)
    csnk Live
    Live += B
    Snap:CsnArr = csni(Live)
    SnapSum:CsnArr = csni(Live) + B

    kAcc[] = csntoarray(Acc)
    kNoCast[] = csntoarray(NoCast)
    kExpr[] = csntoarray(Expr)
    kAssign[] = csntoarray(Assign)
    kLive[] = csntoarray(Live)
    kSnap[] = csntoarray(Snap)
    kSnapSum[] = csntoarray(SnapSum)
    gkAcc = kAcc[0]
    gkNoCast = kNoCast[0]
    gkExpr = kExpr[0]
    gkAssign = kAssign[0]
    gkLive = kLive[0]
    gkSnap = kSnap[0]
    gkSnapSum = kSnapSum[0]
endin

instr 2
    assert(i(gkAcc) == 4)
    assert(i(gkNoCast) == 1)
    assert(i(gkExpr) == 11)
    assert(i(gkAssign) == 11)
    assert(i(gkLive) == 4)
    assert(i(gkSnap) == 0)
    assert(i(gkSnapSum) == 1)
endin
</CsInstruments>
<CsScore>
i 1 0 [4 * 32 / 48000]
i 2 0.01 0.001
e
</CsScore>
</CsoundSynthesizer>
