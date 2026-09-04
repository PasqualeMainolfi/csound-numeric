<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>

<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

/* csnfromaudio marks its slot as a realtime audio path, and the mark travels
   along the operand edges every derived array is created from. A slot carrying
   it refuses to reallocate during performance, because a malloc on the audio
   thread is what a dropout sounds like.

   Both notes below grow their output without bound. The first inherits the
   mark and must be stopped; the second opted out with irt=0 and must run to
   the end. The pair matters: an inert guard passes the second check on its own.

   The refusal is a performance error, and csound's unit-test runner counts one
   of those as a failed assertion, so this file is scored on its final marker
   rather than on the assertion tally. A failed assert stops the note before the
   marker prints, and the refusal itself is raised from under the registry
   mutex, so a wedged lock shows up as a hung test rather than a wrong value. */

gkLockedReached   init 0
gkUnlockedReached init 0

instr 1
    kFill init 0
    kTrig init 1
    kN = timeinstk() * 16
    aSig oscili 0.5, 440
    A:CsnArr = csnfromaudio(aSig)
    B:CsnArr = csnpad(A, kN, kN, kFill, kTrig)
    if timeinstk() == 12 then
        gkLockedReached = 1
    endif
endin

instr 2
    kFill init 0
    kTrig init 1
    kN = timeinstk() * 16
    aSig oscili 0.5, 440
    A:CsnArr = csnfromaudio(aSig, 0)
    B:CsnArr = csnpad(A, kN, kN, kFill, kTrig)
    if timeinstk() == 12 then
        gkUnlockedReached = 1
    endif
endin

/* A shape-only change never needs new storage, so this one must not trip the
   guard even though it is on a locked path: reshape preserves the element
   count and reuses the buffer. It guards the opposite mistake, a check that
   fires on every layout change. */
gkReshapeReached init 0

instr 3
    kShape[] init 2
    kFlip = (timeinstk() % 2 == 0) ? 1 : 0
    kShape[0] = (kFlip == 1) ? 32 : 16
    kShape[1] = (kFlip == 1) ?  1 :  2
    aSig oscili 0.5, 440
    A:CsnArr = csnfromaudio(aSig)
    B:CsnArr = csnreshape(A, kShape)
    if timeinstk() == 12 then
        gkReshapeReached = 1
    endif
endin

/* These four producers used to create an unmarked one-element placeholder at
   init. Each source is marked before the derived output is initialized; the
   first pass fits the preallocated layout, while the later change would force
   the derived slot to be republished and must therefore stop the note. */
gkInterpReached   init 0
gkResampleReached init 0
gkCompressReached init 0
gkSelectReached   init 0

RtInterpQuery@global:CsnArr = csnfromarray(array(0.5, 1.5))
RtInterpX@global:CsnArr     = csnfromarray(array(0, 1, 2))
RtInterpY@global:CsnArr     = csnfromarray(array(0, 10, 20))
RtInterpOut@global:CsnArr   = csnempty(array(0))

instr 4
    csnrtlock RtInterpX, 1
    kShape[] init 1
    kShape[0] = timeinstk() < 2 ? 2 : 3
    csnresize RtInterpQuery, kShape
    RtInterpOut = csninterp(RtInterpQuery, RtInterpX, RtInterpY, 0, 1)
    if timeinstk() == 12 then
        gkInterpReached = 1
    endif
endin

RtResampleSrc@global:CsnArr = csnfromarray(array(0, 10, 20, 30))
RtResampleOut@global:CsnArr = csnempty(array(0))

instr 5
    csnrtlock RtResampleSrc, 1
    kLength init 4
    if timeinstk() >= 2 then
        kLength = 5
    endif
    RtResampleOut = csnresample(RtResampleSrc, kLength, 0, 1)
    if timeinstk() == 12 then
        gkResampleReached = 1
    endif
endin

RtCompressSrc@global:CsnArr  = csnfromarray(array(10, 20, 30, 40))
RtCompressMask@global:CsnArr = csnfromarray(array(1, 0, 0, 0))
RtCompressOut@global:CsnArr  = csnempty(array(0))

instr 6
    csnrtlock RtCompressSrc, 1
    kTrig init 1
    RtCompressOut = csncompress(RtCompressSrc, RtCompressMask, -1, kTrig)
    if timeinstk() == 12 then
        gkCompressReached = 1
    endif
endin

instr 7
    iIndex[] = fillarray(1)
    csnset RtCompressMask, iIndex, 1
endin

RtSelectSrc@global:CsnArr  = csnfromarray(array(10, 20, 30, 40))
RtSelectMask@global:CsnArr = csnfromarray(array(1, 0, 0, 0))
RtSelectOut@global:CsnArr  = csnempty(array(0))

instr 8
    csnrtlock RtSelectSrc, 1
    kTrig init 1
    RtSelectOut = csnselect(RtSelectSrc, RtSelectMask, kTrig)
    if timeinstk() == 12 then
        gkSelectReached = 1
    endif
endin

instr 9
    iIndex[] = fillarray(1)
    csnset RtSelectMask, iIndex, 1
endin

instr 100
    iLocked   = i(gkLockedReached)
    iUnlocked = i(gkUnlockedReached)
    iReshape  = i(gkReshapeReached)
    iInterp   = i(gkInterpReached)
    iResample = i(gkResampleReached)
    iCompress = i(gkCompressReached)
    iSelect   = i(gkSelectReached)

    assert(iLocked == 0)
    assert(iUnlocked == 1)
    assert(iReshape == 1)
    assert(iInterp == 0)
    assert(iResample == 0)
    assert(iCompress == 0)
    assert(iSelect == 0)
    prints("csnum registry alive after the realtime-lock refusal\n")
endin
</CsInstruments>

<CsScore>
i 1 0.0 0.02
i 2 0.1 0.02
i 3 0.2 0.02
i 4 0.3 0.02
i 5 0.4 0.02
i 6 0.5 0.02
i 7 0.505 0
i 8 0.6 0.02
i 9 0.605 0
i 100 0.7 0.01
e
</CsScore>
</CsoundSynthesizer>
