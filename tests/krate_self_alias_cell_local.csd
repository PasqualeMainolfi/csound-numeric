<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>

<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

/* An opcode whose fill writes each result cell from the input cell sitting at
   the same index may take its own output as input: every cell is read before it
   is written, so X = csnsin(X) is well defined. The permission is conditional
   on the layout holding still, because a reallocation would hand the fill a
   fresh zeroed buffer and drop the data it is about to read — csnabs on a
   complex array is the one member of the family that changes it, and stays
   rejected in krate_self_alias_rejected.csd along with the permuting forms.

   Each note below feeds an opcode on its own handle, and instrument 10 checks
   the results at i-time. The producing notes outlast it on purpose: a k-rate
   opcode with a global handle output zeroes that handle when its note ends. */

giValues[] = fillarray(-4, -1, 2, 9)
giAngles[] = fillarray(0, 1, 2, 3)

Clipped@global:CsnArr = csnfromarray(giValues)
Copied@global:CsnArr = csnfromarray(giValues)
Compared@global:CsnArr = csnfromarray(giValues)
Floored@global:CsnArr = csnfromarray(giValues)
Wrapped@global:CsnArr = csnfromarray(giAngles)
Sorted@global:CsnArr = csnfromarray(giValues)
Normed@global:CsnArr = csnfromarray(giValues)
Ranked@global:CsnArr = csnfromarray(giValues)

instr 1
    /* csnclip on its own handle: idempotent, so it settles at the clipped
       values and stays there however many passes run. */
    kLo init -2
    kHi init 3
    Clipped = csnclip(Clipped, kLo, kHi)
endin

instr 2
    /* csncopy onto itself is a no-op that must not corrupt anything. */
    Copied = csncopy(Copied)
endin

instr 3
    /* A comparison consuming its own output: the first pass turns the values
       into a 0/1 mask, and every later pass compares that mask against the
       same threshold. -4 -1 2 9 against > 0 gives 0 0 1 1; that mask against
       > 0 again gives 0 0 1 1, so it is a fixed point. */
    kThreshold init 0
    Compared = csngt(Compared, kThreshold)
endin

instr 4
    /* Unary maths on its own handle, real in and real out. */
    Floored = csnfloor(Floored)
endin

instr 5
    /* csnwrap folds each angle independently. */
    kPeriod init 2
    kTrig init 1
    Wrapped = csnwrap(Wrapped, kPeriod, kTrig)
endin

instr 6
    /* csnsort stages each slice through a scratch, so it reads the whole slice
       before writing any of it. Sorting an already sorted array is a fixed
       point. */
    kAxis init -1
    Sorted = csnsort(Sorted, kAxis)
endin

instr 7
    /* csnnormalize accumulates the norm over a full read pass, then scales each
       cell in place. After the first pass the norm is 1, so it settles. */
    kAxis init -1
    kOrder init 2
    Normed = csnnormalize(Normed, kAxis, kOrder)
endin

instr 8
    /* csnargsort copies value/index pairs into its scratch before writing any
       index back. */
    kAxis init -1
    Ranked = csnargsort(Ranked, kAxis)
endin

instr 10
    i0[] = array(0)
    i1[] = array(1)
    i2[] = array(2)
    i3[] = array(3)

    iClip0 = csnget(Clipped, i0)
    iClip1 = csnget(Clipped, i1)
    iClip3 = csnget(Clipped, i3)
    assert(iClip0 == -2 && iClip1 == -1 && iClip3 == 3)

    iCopy0 = csnget(Copied, i0)
    iCopy3 = csnget(Copied, i3)
    assert(iCopy0 == -4 && iCopy3 == 9)

    iCmp0 = csnget(Compared, i0)
    iCmp1 = csnget(Compared, i1)
    iCmp2 = csnget(Compared, i2)
    iCmp3 = csnget(Compared, i3)
    assert(iCmp0 == 0 && iCmp1 == 0 && iCmp2 == 1 && iCmp3 == 1)

    iFloor0 = csnget(Floored, i0)
    iFloor3 = csnget(Floored, i3)
    assert(iFloor0 == -4 && iFloor3 == 9)

    /* Angles 0 1 2 3 wrapped to a period of 2 land in [-1, 1): 0 -1 0 -1.
       Already-wrapped values stay put, so re-wrapping every pass is stable. */
    iWrap0 = csnget(Wrapped, i0)
    iWrap1 = csnget(Wrapped, i1)
    iWrap2 = csnget(Wrapped, i2)
    iWrap3 = csnget(Wrapped, i3)
    assert(iWrap0 == 0 && iWrap1 == -1)
    assert(iWrap2 == 0 && iWrap3 == -1)

    /* -4 -1 2 9 sorted ascending. */
    iSort0 = csnget(Sorted, i0)
    iSort3 = csnget(Sorted, i3)
    assert(iSort0 == -4 && iSort3 == 9)

    /* The L2 norm of the normalised vector is 1, so every later pass is a
       no-op and the first element keeps its sign. */
    iNorm0 = csnget(Normed, i0)
    assert(iNorm0 < 0 && iNorm0 > -1)

    /* argsort of -4 -1 2 9 is already 0 1 2 3, and argsort of 0 1 2 3 is the
       same again. */
    iRank0 = csnget(Ranked, i0)
    iRank3 = csnget(Ranked, i3)
    assert(iRank0 == 0 && iRank3 == 3)
endin
</CsInstruments>

<CsScore>
i1  0   0.3
i2  0   0.3
i3  0   0.3
i4  0   0.3
i5  0   0.3
i6  0   0.3
i7  0   0.3
i8  0   0.3
i10 0.2 0
</CsScore>
</CsoundSynthesizer>
