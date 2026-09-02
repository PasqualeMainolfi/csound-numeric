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

instr 100
    iLocked   = i(gkLockedReached)
    iUnlocked = i(gkUnlockedReached)
    iReshape  = i(gkReshapeReached)

    assert(iLocked == 0)
    assert(iUnlocked == 1)
    assert(iReshape == 1)
    prints("csnum registry alive after the realtime-lock refusal\n")
endin
</CsInstruments>

<CsScore>
i 1 0.0 0.02
i 2 0.1 0.02
i 3 0.2 0.02
i 100 0.3 0.01
e
</CsScore>
</CsoundSynthesizer>
