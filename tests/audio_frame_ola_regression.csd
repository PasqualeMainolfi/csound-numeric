<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>

<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

/* The signal is a ramp whose value is the sample index, so every number that
   comes out names the sample it came from. That turns "is the framing right"
   and "is the overlap-add summing" into arithmetic on two readings. */

gkSnapA   init -1
gkSnapB   init -1
gkSnapDef init -1

gkDirectIn1  init -1
gkDirectOut1 init -1
gkDirectIn2  init -1
gkDirectOut2 init -1

gkOverIn1  init -1
gkOverOut1 init -1
gkOverIn2  init -1
gkOverOut2 init -1

gkChainOut2 init -1

/* csnsnap with hop == frame advances one whole frame per emission. */
instr 1
    iIdx[] fillarray 0
    aRamp line 0, p3, sr * p3
    F:CsnArr, kReady csnsnap aRamp, 64, 64
    kFirst = csnget(F, iIdx)
    if kReady == 1 && timeinstk() == 4 then
        gkSnapA = kFirst
    endif
    if kReady == 1 && timeinstk() == 6 then
        gkSnapB = kFirst
    endif
endin

/* The hop argument is optional and defaults to the frame size, so this note
   must behave exactly like instr 1. Csound rejects a hop below ksmps, and the
   default has to be substituted before that check or omitting it would fail. */
instr 2
    iIdx[] fillarray 0
    aRamp line 0, p3, sr * p3
    F:CsnArr, kReady csnsnap aRamp, 64
    kFirst = csnget(F, iIdx)
    if kReady == 1 && timeinstk() == 6 then
        gkSnapDef = kFirst
    endif
endin

/* Rectangular window, hop == frame: overlap-add reconstructs the input, so
   output and input advance together and the slope is 1. */
instr 10
    aRamp line 0, p3, sr * p3
    F:CsnArr, kNew csnsnap aRamp, 64, 64
    aOut, kOk csnstream F, 64
    kIn  downsamp aRamp
    kOut downsamp aOut
    if timeinstk() == 20 then
        gkDirectIn1 = kIn
        gkDirectOut1 = kOut
    endif
    if timeinstk() == 40 then
        gkDirectIn2 = kIn
        gkDirectOut2 = kOut
    endif
endin

/* Same chain with a k-rate opcode wedged in. csnmul by one is the identity, so
   the result must match instr 10 exactly. It does not come for free: every
   k-rate producer bumps its output's data version on every pass it writes, so
   a consumer that decided "is this frame new?" from the version alone would
   fold one frame per pass here instead of one per hop. */
instr 11
    kOne init 1
    aRamp line 0, p3, sr * p3
    F:CsnArr, kNew csnsnap aRamp, 64, 64
    G:CsnArr = csnmul(F, kOne)
    aOut, kOk csnstream G, 64
    kOut downsamp aOut
    if timeinstk() == 40 then
        gkChainOut2 = kOut
    endif
endin

/* Half a frame of hop: every output sample is covered by two frames, and a
   rectangular window makes them sum, so the slope doubles. This is the case
   that tells overlap-add apart from plain replacement. */
instr 12
    aRamp line 0, p3, sr * p3
    F:CsnArr, kNew csnsnap aRamp, 64, 32
    aOut, kOk csnstream F, 32
    kIn  downsamp aRamp
    kOut downsamp aOut
    if timeinstk() == 20 then
        gkOverIn1 = kIn
        gkOverOut1 = kOut
    endif
    if timeinstk() == 40 then
        gkOverIn2 = kIn
        gkOverOut2 = kOut
    endif
endin

instr 100
    iSnapA   = i(gkSnapA)
    iSnapB   = i(gkSnapB)
    iSnapDef = i(gkSnapDef)

    iDIn1  = i(gkDirectIn1)
    iDOut1 = i(gkDirectOut1)
    iDIn2  = i(gkDirectIn2)
    iDOut2 = i(gkDirectOut2)

    iOIn1  = i(gkOverIn1)
    iOOut1 = i(gkOverOut1)
    iOIn2  = i(gkOverIn2)
    iOOut2 = i(gkOverOut2)

    iChainOut2 = i(gkChainOut2)

    ; consecutive frames start one hop apart
    assert(iSnapB - iSnapA == 64)
    ; the default hop is the frame size, so it lands where instr 1 landed
    assert(iSnapDef == iSnapB)

    ; no overlap: output tracks input one for one
    iDirectSlope = (iDOut2 - iDOut1) / (iDIn2 - iDIn1)
    assert(iDirectSlope == 1)

    ; an identity opcode in the chain changes nothing
    assert(iChainOut2 == iDOut2)

    ; 50% overlap with a rectangular window sums two frames per sample
    iOverSlope = (iOOut2 - iOOut1) / (iOIn2 - iOIn1)
    assert(iOverSlope == 2)

    prints("csnum audio frame/ola regression passed\n")
endin
</CsInstruments>

<CsScore>
i 1  0.0 0.02
i 2  0.0 0.02
i 10 0.0 0.04
i 11 0.0 0.04
i 12 0.0 0.04
i 100 0.06 0.01
e
</CsScore>
</CsoundSynthesizer>
