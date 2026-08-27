<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>

<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

/* The expensive k-rate reductions republish last pass's result when the source
   array has not been written since. The failure mode of that shortcut is a
   stale answer, which no amount of staring at a single k-pass reveals, so each
   note here writes its source part way through and then checks the cached
   result against one computed from scratch at i-time. Comparing against a
   fresh computation rather than a hard-coded number keeps the test about the
   caching and not about the definition of a median or a moving average.

   The producing notes outlast the checking one on purpose: a k-rate opcode
   whose output handle is global zeroes that handle when its note ends, so
   MovOut would read as handle 0 if instrument 2 had already finished. */

giSource[] = fillarray(1, 2, 3, 4, 5)

MedianSrc@global:CsnArr = csnfromarray(giSource)
MovSrc@global:CsnArr = csnfromarray(giSource)
MovOut@global:CsnArr = csnfromarray(giSource)

gkMedian init 0

instr 1
    /* csnmedian sorts a copy of the source on every pass it runs. */
    kTrig init 1
    gkMedian = csnmedian(MedianSrc, kTrig)
endin

instr 2
    /* csnmovmean walks a window over every element. */
    kWindow init 3
    kAxis init -1
    kTrig init 1
    MovOut = csnmovmean(MovSrc, kWindow, kAxis, kTrig)
endin

instr 3
    /* One i-time write into each source, half way through the notes above. */
    kIndex0[] = fillarray(0)
    csnset MedianSrc, kIndex0, 100
    csnset MovSrc, kIndex0, 100
endin

instr 10
    i0[] = array(0)
    i1[] = array(1)
    i4[] = array(4)

    /* The cached median has to agree with one computed now, after the write. */
    iMedianRef = csnmedian(MedianSrc)
    iMedianCached = i(gkMedian)
    assert(iMedianCached == iMedianRef)

    /* And it has to have moved at all: the pre-write median of 1 2 3 4 5 is 3,
       so a cache that never noticed the write would still be reporting it. */
    assert(iMedianCached != 3)

    MovRef:CsnArr = csnmovmean(MovSrc, 3, -1)
    iRef0 = csnget(MovRef, i0)
    iRef1 = csnget(MovRef, i1)
    iRef4 = csnget(MovRef, i4)
    iOut0 = csnget(MovOut, i0)
    iOut1 = csnget(MovOut, i1)
    iOut4 = csnget(MovOut, i4)
    assert(iOut0 == iRef0 && iOut1 == iRef1 && iOut4 == iRef4)
endin
</CsInstruments>

<CsScore>
i1  0    0.3
i2  0    0.3
i3  0.05 0
i10 0.2  0
</CsScore>
</CsoundSynthesizer>
