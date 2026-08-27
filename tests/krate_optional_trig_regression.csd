<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>

<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

/* The k-rate trigger is an optional argument wherever the signature keeps
   another k-rate marker without it. Omitting it must mean "run", passing 0
   must still mean "hold", and passing 1 must behave exactly as before, so
   orchestras written against the old mandatory argument keep working.

   Where the trigger was the only k in the signature it stays mandatory: an
   optional there would make the k-rate entry indistinguishable from the i-rate
   one, and Csound would silently bind the i-rate form. csnabs is the witness
   for that case and is checked here as a plain two-argument call.

   Producing notes outlast the checking note, since a k-rate opcode with a
   global handle output zeroes that handle when its note ends. */

giSource[] = fillarray(1, 2, 3, 4, 5)

Src@global:CsnArr = csnfromarray(giSource)
Sorted@global:CsnArr = csnfromarray(giSource)
Mov@global:CsnArr = csnfromarray(giSource)
Mask@global:CsnArr = csnfromarray(giSource)

gkTracked init 0
gkHeld init 0
gkExplicit init 0

instr 1
    /* Scalar output, trigger omitted: follows the source. */
    gkTracked = csnmedian(Src)

    /* Same opcode held at zero: keeps whatever the init pass computed. */
    kZero init 0
    gkHeld = csnmedian(Src, kZero)

    /* And the old spelling still means the same thing. */
    kOne init 1
    gkExplicit = csnmedian(Src, kOne)
endin

instr 2
    /* Array output, trigger omitted, one k argument left to mark the rate. */
    kAxis init -1
    Sorted = csnsort(Src, kAxis)
endin

instr 3
    /* Two k arguments plus the omitted trigger. */
    kWindow init 3
    kMovAxis init -1
    Mov = csnmovmean(Src, kWindow, kMovAxis)
endin

instr 4
    /* Comparison against a k scalar, trigger omitted. */
    kThreshold init 3
    Mask = csngt(Src, kThreshold)
endin

instr 5
    /* One i-time write, after the notes above have started. */
    kIndex0[] = fillarray(0)
    csnset Src, kIndex0, 100
endin

instr 10
    i0[] = array(0)
    i4[] = array(4)

    /* Median of 100 2 3 4 5 is 4; the pre-write median was 3. */
    iRef = csnmedian(Src)
    iTracked = i(gkTracked)
    iHeld = i(gkHeld)
    iExplicit = i(gkExplicit)
    assert(iTracked == iRef && iTracked == 4)
    assert(iExplicit == iTracked)
    assert(iHeld == 3)

    /* Sorted 100 2 3 4 5 puts 100 last. */
    iSorted0 = csnget(Sorted, i0)
    iSorted4 = csnget(Sorted, i4)
    assert(iSorted0 == 2 && iSorted4 == 100)

    /* The moving mean has to agree with one computed now. */
    MovRef:CsnArr = csnmovmean(Src, 3, -1)
    iMovRef0 = csnget(MovRef, i0)
    iMov0 = csnget(Mov, i0)
    assert(iMov0 == iMovRef0)

    /* 100 2 3 4 5 against a threshold of 3 gives 1 0 0 1 1. */
    iMask0 = csnget(Mask, i0)
    iMask4 = csnget(Mask, i4)
    assert(iMask0 == 1 && iMask4 == 1)

    /* csnabs keeps a mandatory trigger, so this two-argument call is the only
       way to reach the k-rate form. */
    iAbs0 = csnget(Src, i0)
    assert(iAbs0 == 100)
endin
</CsInstruments>

<CsScore>
i1  0    0.3
i2  0    0.3
i3  0    0.3
i4  0    0.3
i5  0.05 0
i10 0.2  0
</CsScore>
</CsoundSynthesizer>
