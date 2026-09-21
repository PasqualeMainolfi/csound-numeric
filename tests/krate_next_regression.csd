<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>

<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

/* csnnext walks an array one element per control period. Five behaviours are
   pinned here: the plain walk and its exhaustion, the level-triggered rewind,
   what holding that rewind high does, an empty array, and the complex
   overload.

   Each note accumulates the values and the states it saw into a global, and
   the assertions read those globals at i-time in instrument 10, which the
   score starts after the notes have ended. A k-rate assert would not do: its
   init pass is evaluated once at i-time, before any element has been read.

   Every source is built from an i-rate array. csnfromarray on a k[] carries a
   performance pass that re-imports the Csound array every control period,
   which csnnext would see as a write to its source and refuse. */

gkWalkValues init 0
gkWalkStates init 0
gkRewindValues init 0
gkRewindStates init 0
gkPinValues init 0
gkPinStates init 0
gkEmptyValues init 0
gkEmptyStates init 0
gkCpxReal init 0
gkCpxImag init 0
gkCpxStates init 0

instr 1
    /* 10 20 30 over five passes: three elements, then the value output holds
       the last one and the state falls to 0.
       values 10+20+30+30+30 = 120, states 1+1+1+0+0 = 3 */
    src:i[]    = fillarray(10, 20, 30)
    vec:CsnArr = csnfromarray(src)

    kon  init 1
    kn   init 0

    kvalue, kstate csnnext vec, kon

    gkWalkValues += kvalue
    gkWalkStates += kstate
    kn += 1
    if kn == 5 then
        turnoff
    endif
endin

instr 2
    /* reset pulsed for one control period rewinds and the walk continues:
       1 2 1 2 3, so values 9 and states 5. The pulse is armed after csnnext
       has run, so it lands on the pass after the one that sets it. */
    src:i[]    = fillarray(1, 2, 3)
    vec:CsnArr = csnfromarray(src)

    kon    init 1
    kreset init 0
    kn     init 0

    kvalue, kstate csnnext vec, kon, kreset

    gkRewindValues += kvalue
    gkRewindStates += kstate

    if kn == 1 then
        kreset = 1
    elseif kn == 2 then
        kreset = 0
    endif

    kn += 1
    if kn == 5 then
        turnoff
    endif
endin

instr 3
    /* reset is level triggered, so holding it high rewinds on every pass and
       the iterator never leaves the first element: 5 5 5 5, values 20 and
       states 4, with no error anywhere. This is the shape that looks like a
       mistake and is not reported as one. */
    src:i[]    = fillarray(5, 6, 7)
    vec:CsnArr = csnfromarray(src)

    kon    init 1
    kreset init 1
    kn     init 0

    kvalue, kstate csnnext vec, kon, kreset

    gkPinValues += kvalue
    gkPinStates += kstate
    kn += 1
    if kn == 4 then
        turnoff
    endif
endin

instr 4
    /* an empty array reports a zero state from the very first pass rather
       than handing back fabricated elements. The rewind is held high on
       purpose: it is the path that has to notice there is nothing to rewind
       to, rather than restarting a walk over zero elements. */
    shape:i[]  = fillarray(4)
    vec:CsnArr = csnempty(shape)

    kon    init 1
    kreset init 1
    kn     init 0

    kvalue, kstate csnnext vec, kon, kreset

    gkEmptyValues += kvalue
    gkEmptyStates += kstate
    kn += 1
    if kn == 3 then
        turnoff
    endif
endin

instr 5
    /* the complex overload, selected by the type of the value output:
       (1+0j) and (2+0j) times (2+5j) are 2+5j and 4+10j, so over four passes
       the real parts sum to 2+4+4+4 = 14, the imaginary ones to 5+10+10+10 = 35
       and the states to 1+1+0+0 = 2 */
    src:i[]     = fillarray(1, 2)
    base:CsnArr = csnfromarray(src)
    j:Complex   = init(2, 5, 0)
    cpx:CsnArr  = csnmul(csntocomplex(base), j)

    kon init 1
    kn  init 0

    cvalue:Complex, kstate csnnext cpx, kon

    gkCpxReal += real(cvalue)
    gkCpxImag += imag(cvalue)
    gkCpxStates += kstate
    kn += 1
    if kn == 4 then
        turnoff
    endif
endin

instr 10
    iWalkValues = i(gkWalkValues)
    iWalkStates = i(gkWalkStates)
    assert(iWalkValues == 120 && iWalkStates == 3)

    iRewindValues = i(gkRewindValues)
    iRewindStates = i(gkRewindStates)
    assert(iRewindValues == 9 && iRewindStates == 5)

    iPinValues = i(gkPinValues)
    iPinStates = i(gkPinStates)
    assert(iPinValues == 20 && iPinStates == 4)

    iEmptyValues = i(gkEmptyValues)
    iEmptyStates = i(gkEmptyStates)
    assert(iEmptyValues == 0 && iEmptyStates == 0)

    iCpxReal = i(gkCpxReal)
    iCpxImag = i(gkCpxImag)
    iCpxStates = i(gkCpxStates)
    assert(iCpxReal == 14 && iCpxImag == 35 && iCpxStates == 2)
endin
</CsInstruments>

<CsScore>
i1  0   0.1
i2  0   0.1
i3  0   0.1
i4  0   0.1
i5  0   0.1
i10 0.2 0
</CsScore>
</CsoundSynthesizer>
