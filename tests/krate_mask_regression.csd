<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>

<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

/* Classification masks at k-rate must start as valid masks, follow writes to
   their source, and remain safe when the result is assigned back to its own
   input. A zero trigger holds the mask computed by the init pass. */

giPosInf = exp(1000)
giNaN = sqrt(-1)
giSpecial[] = fillarray(7, giPosInf, -giPosInf, giNaN, 0)

Special@global:CsnArr = csnfromarray(giSpecial)
NaNMask@global:CsnArr = csnfromarray(giSpecial)
InfMask@global:CsnArr = csnfromarray(giSpecial)
FinMask@global:CsnArr = csnfromarray(giSpecial)
HeldMask@global:CsnArr = csnfromarray(giSpecial)
SelfMask@global:CsnArr = csnfromarray(giSpecial)

instr 1
    kOne init 1
    kZero init 0
    NaNMask = csnisnan(Special, kOne)
    InfMask = csnisinf(Special, kOne)
    FinMask = csnisfin(Special, kOne)
    HeldMask = csnisinf(Special, kZero)

    /* isnan is not idempotent: the first application produces a 0/1 mask and
       the next application turns that mask into all zeroes. A result cache
       must therefore never freeze an elementwise self-alias after one pass. */
    SelfMask = csnisnan(SelfMask, kOne)
endin

instr 2
    /* Turn the first finite value into an infinity and the NaN into a finite
       value. The three live masks must all notice the source generation. */
    kIndex0[] = fillarray(0)
    kIndex3[] = fillarray(3)
    csnset Special, kIndex0, giPosInf
    csnset Special, kIndex3, 4
endin

instr 10
    i0[] = array(0)
    i1[] = array(1)
    i2[] = array(2)
    i3[] = array(3)
    i4[] = array(4)

    assert(csnget(NaNMask, i0) == 0 && csnget(NaNMask, i3) == 0)
    assert(csnget(InfMask, i0) == 1 && csnget(InfMask, i1) == 1 && csnget(InfMask, i2) == 1)
    assert(csnget(InfMask, i3) == 0 && csnget(InfMask, i4) == 0)
    assert(csnget(FinMask, i0) == 0 && csnget(FinMask, i1) == 0 && csnget(FinMask, i2) == 0)
    assert(csnget(FinMask, i3) == 1 && csnget(FinMask, i4) == 1)

    /* HeldMask is the init-time classification of the original source. */
    assert(csnget(HeldMask, i0) == 0 && csnget(HeldMask, i1) == 1 && csnget(HeldMask, i2) == 1)
    assert(csnget(HeldMask, i3) == 0 && csnget(HeldMask, i4) == 0)

    assert(csnget(SelfMask, i0) == 0 && csnget(SelfMask, i1) == 0)
    assert(csnget(SelfMask, i2) == 0 && csnget(SelfMask, i3) == 0 && csnget(SelfMask, i4) == 0)
endin
</CsInstruments>

<CsScore>
i1  0    0.3
i2  0.05 0
i10 0.2  0
</CsScore>
</CsoundSynthesizer>
