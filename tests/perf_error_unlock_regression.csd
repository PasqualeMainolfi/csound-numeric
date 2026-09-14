<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>

<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

/* A performance error raised while an opcode holds the registry mutex.

   csoundPerfError runs the note's deinit chain before it returns, and the
   deinit takes the same mutex, which is not recursive: an opcode that reports
   with the lock held wedges Csound for good. Each note below provokes one such
   error on a k-rate pass, most by having a later note free an operand with
   csnfree, the moving median with an axis that turns invalid. Every note must
   be stopped by its error, which leaves its marker at 0, and the performance
   must go on to the final marker. A regression shows up as a hung run, which
   the ctest timeout turns into a failure.

   The errors count as failed assertions in the unit-test runner, so the file
   is scored on its final marker. */

giX[]      = fillarray(1, 2, 3, 4, 5, 6, 7, 8)
giH[]      = fillarray(1, 1, 1)
giA[]      = fillarray(2, 1, 1, 3)
giB[]      = fillarray(1, 2)
giShape22[] = fillarray(2, 2)

ConvX@global:CsnArr   = csnfromarray(giX)
ConvH@global:CsnArr   = csnfromarray(giH)
NdX@global:CsnArr     = csnfromarray(giX)
NdH@global:CsnArr     = csnfromarray(giH)
FftX@global:CsnArr    = csnfromarray(giX)
FftH@global:CsnArr    = csnfromarray(giH)
FftNdX@global:CsnArr  = csnfromarray(giX)
FftNdH@global:CsnArr  = csnfromarray(giH)
SolveFlat@global:CsnArr = csnfromarray(giA)
SolveA@global:CsnArr  = csnreshape(SolveFlat, giShape22)
SolveB@global:CsnArr  = csnfromarray(giB)
InvFlat@global:CsnArr = csnfromarray(giA)
InvA@global:CsnArr    = csnreshape(InvFlat, giShape22)
DetFlat@global:CsnArr = csnfromarray(giA)
DetA@global:CsnArr    = csnreshape(DetFlat, giShape22)
MedSrc@global:CsnArr  = csnfromarray(giX)

gkConv1d  init 0
gkConvNd  init 0
gkFft1d   init 0
gkFftNd   init 0
gkSolve   init 0
gkInverse init 0
gkDet     init 0
gkMedfilt init 0

instr 1
    kOne init 1
    Out:CsnArr = csnconvolve1d(ConvX, ConvH, 0, kOne)
    if timeinstk() == 12 then
        gkConv1d = 1
    endif
endin

instr 2
    kOne init 1
    Out:CsnArr = csnconvolve(NdX, NdH, 0, kOne)
    if timeinstk() == 12 then
        gkConvNd = 1
    endif
endin

instr 3
    kOne init 1
    Out:CsnArr = csnfftconvolve1d(FftX, FftH, 0, -1, kOne)
    if timeinstk() == 12 then
        gkFft1d = 1
    endif
endin

instr 4
    kOne init 1
    Out:CsnArr = csnfftconvolve(FftNdX, FftNdH, 0, kOne)
    if timeinstk() == 12 then
        gkFftNd = 1
    endif
endin

instr 5
    kOne init 1
    Out:CsnArr = csnsolve(SolveA, SolveB, kOne)
    if timeinstk() == 12 then
        gkSolve = 1
    endif
endin

instr 6
    kOne init 1
    Out:CsnArr = csninv(InvA, kOne)
    if timeinstk() == 12 then
        gkInverse = 1
    endif
endin

instr 7
    kOne init 1
    kDet = csndet(DetA, kOne)
    if timeinstk() == 12 then
        gkDet = 1
    endif
endin

instr 8
    kOne init 1
    kAxis init 0
    if timeinstk() >= 4 then
        kAxis = 5
    endif
    Out:CsnArr = csnmedfilt1d(MedSrc, 3, kOne, kAxis)
    if timeinstk() == 12 then
        gkMedfilt = 1
    endif
endin

/* Frees one operand of each running note between its passes. */
instr 20
    csnfree ConvX
    csnfree NdH
    csnfree FftX
    csnfree FftNdH
    csnfree SolveA
    csnfree InvA
    csnfree DetA
endin

instr 100
    iConv1d  = i(gkConv1d)
    iConvNd  = i(gkConvNd)
    iFft1d   = i(gkFft1d)
    iFftNd   = i(gkFftNd)
    iSolve   = i(gkSolve)
    iInverse = i(gkInverse)
    iDet     = i(gkDet)
    iMedfilt = i(gkMedfilt)
    assert(iConv1d == 0)
    assert(iConvNd == 0)
    assert(iFft1d == 0)
    assert(iFftNd == 0)
    assert(iSolve == 0)
    assert(iInverse == 0)
    assert(iDet == 0)
    assert(iMedfilt == 0)
    iRan = iConv1d + iConvNd + iFft1d + iFftNd + iSolve + iInverse + iDet + iMedfilt
    if iRan == 0 then
        prints("csnum performance continued after every locked perf error\n")
    endif
endin
</CsInstruments>

<CsScore>
i 1   0.0   0.02
i 2   0.0   0.02
i 3   0.0   0.02
i 4   0.0   0.02
i 5   0.0   0.02
i 6   0.0   0.02
i 7   0.0   0.02
i 8   0.0   0.02
i 20  0.004 0
i 100 0.05  0.01
e
</CsScore>
</CsoundSynthesizer>
