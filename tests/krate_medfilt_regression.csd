<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>

<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

/* The k-rate median filters, in every form.

   Three things are checked, and each of them has been broken at least once:

   - an output form fed by a source that never changes must publish the same
     values on every pass, through the reuse path;
   - an in-place form applies one pass per external write. Filtering again what
     it has already filtered is visible here because these kernels are not
     idempotent on this data: [2, 9, 1, 8, 3, 7, 4] filtered once is
     [2, 2, 8, 3, 7, 4, 4] and twice [2, 2, 3, 7, 4, 4, 4];
   - a write from another opcode must be seen: clipping the filtered array
     during a pass makes the filter run once more, on the clipped values.

   The expectations are scipy.signal.medfilt's. */

giY[]        = fillarray(2, 9, 1, 8, 3, 7, 4)
giMflat[]    = fillarray(1, 9, 2, 3, 4, 1, 8, 2, 3, 5, 1, 7)
giShape[]    = fillarray(3, 4)
giKcol[]     = fillarray(3, 1)
giEmpty[]    = fillarray(0)
giEmpty2[]   = fillarray(0, 0)

Src1d@global:CsnArr     = csnfromarray(giY)
In1d@global:CsnArr      = csnfromarray(giY)
In1dW@global:CsnArr     = csnfromarray(giY)

MatFlat@global:CsnArr   = csnfromarray(giMflat)
Mat@global:CsnArr       = csnreshape(MatFlat, giShape)
MatInFlat@global:CsnArr = csnfromarray(giMflat)
MatIn@global:CsnArr     = csnreshape(MatInFlat, giShape)
MatInSFlat@global:CsnArr = csnfromarray(giMflat)
MatInS@global:CsnArr    = csnreshape(MatInSFlat, giShape)
MatInWFlat@global:CsnArr = csnfromarray(giMflat)
MatInW@global:CsnArr    = csnreshape(MatInWFlat, giShape)

Out1d@global:CsnArr     = csnempty(giEmpty)
Zero1d@global:CsnArr    = csnempty(giEmpty)
OutNd@global:CsnArr     = csnempty(giEmpty2)
OutNdS@global:CsnArr    = csnempty(giEmpty2)

instr 1
    kAlways init 1
    kNever  init 0
    kLow    init 0

    ; inert at init, and a real clip from the third pass on
    kHigh1d = (timeinstk() >= 3 ? 5 : 1000)
    kHighNd = (timeinstk() >= 3 ? 1 : 1000)
    kTouch  = (timeinstk() == 3 ? 1 : 0)

    ; --- output forms: the source never changes, so neither may the result ---
    Out1d  = csnmedfilt1d(Src1d, 3, -1, kAlways)
    OutNd  = csnmedfilt(Mat, 3, kAlways)
    OutNdS = csnmedfilt(Mat, giKcol, kAlways)

    ; a zero trigger republishes what the init pass computed
    Zero1d = csnmedfilt1d(Src1d, 3, -1, kNever)

    ; --- in place, triggered on every pass: filter once, then hold ---
    csnmedfilt1d(In1d, 3, -1, kAlways)
    csnmedfilt(MatIn, 3, kAlways)
    csnmedfilt(MatInS, giKcol, kAlways)

    ; --- in place, with a write from another opcode on the third pass ---
    csnclip(In1dW, kLow, kHigh1d, kTouch)
    csnmedfilt1d(In1dW, 3, -1, kAlways)

    csnclip(MatInW, kLow, kHighNd, kTouch)
    csnmedfilt(MatInW, 3, kAlways)
endin

instr 2
    ; --- output form, filtered on every pass from an unchanging source -------
    iOutSize = csnsize(Out1d)
    assert(iOutSize == 7)
    iOut[] = csntoarray(Out1d)
    assert(iOut[0] == 2 && iOut[1] == 2 && iOut[2] == 8 && iOut[3] == 3)
    assert(iOut[4] == 7 && iOut[5] == 4 && iOut[6] == 4)

    ; a zero trigger keeps the result the init pass published
    iZero[] = csntoarray(Zero1d)
    assert(iZero[2] == 8 && iZero[3] == 3 && iZero[4] == 7)

    ; --- in place: one pass, not one per k-period --------------------------
    ; filtered twice this would read 3, 7, 4 at the same positions
    iIn[] = csntoarray(In1d)
    assert(iIn[0] == 2 && iIn[1] == 2 && iIn[2] == 8)
    assert(iIn[3] == 3 && iIn[4] == 7 && iIn[5] == 4 && iIn[6] == 4)

    ; --- in place, after an external write ---------------------------------
    ; clipped to 5 the array reads 2 2 5 3 5 4 4; filtering that once gives
    ; 2 2 3 5 4 4 4, which is what a write the filter noticed looks like
    iInW[] = csntoarray(In1dW)
    assert(iInW[0] == 2 && iInW[1] == 2 && iInW[2] == 3)
    assert(iInW[3] == 5 && iInW[4] == 4 && iInW[5] == 4 && iInW[6] == 4)

    ; --- N-D output form, one size on every axis ---------------------------
    iNdSize = csnsize(OutNd)
    assert(iNdSize == 12)
    iNdShape[] = csnshape(OutNd)
    assert(iNdShape[0] == 3 && iNdShape[1] == 4)
    iNd[][] = csntoarray(OutNd)
    assert(iNd[0][0] == 0 && iNd[0][1] == 1 && iNd[0][2] == 2 && iNd[0][3] == 0)
    assert(iNd[1][0] == 1 && iNd[1][1] == 3 && iNd[1][2] == 3 && iNd[1][3] == 2)
    assert(iNd[2][0] == 0 && iNd[2][1] == 1 && iNd[2][2] == 1 && iNd[2][3] == 0)

    ; --- N-D output form, one size per axis: 3 down, 1 across --------------
    iNdS[][] = csntoarray(OutNdS)
    assert(iNdS[0][0] == 1 && iNdS[0][1] == 1 && iNdS[0][2] == 2 && iNdS[0][3] == 2)
    assert(iNdS[1][0] == 3 && iNdS[1][1] == 5 && iNdS[1][2] == 2 && iNdS[1][3] == 3)
    assert(iNdS[2][0] == 3 && iNdS[2][1] == 1 && iNdS[2][2] == 1 && iNdS[2][3] == 2)

    ; --- N-D in place: filtered twice the centre row would read 1, 1 -------
    iMatIn[][] = csntoarray(MatIn)
    assert(iMatIn[0][1] == 1 && iMatIn[0][2] == 2)
    assert(iMatIn[1][0] == 1 && iMatIn[1][1] == 3 && iMatIn[1][2] == 3 && iMatIn[1][3] == 2)

    ; the same in place with a kernel shaped per axis; twice this reads 1 at [1][1]
    iMatInS[][] = csntoarray(MatInS)
    assert(iMatInS[1][1] == 5 && iMatInS[1][3] == 3 && iMatInS[2][0] == 3)

    ; --- N-D in place, after an external write -----------------------------
    ; clipped to 1 the centre row reads 1 1 1 1; filtering that once takes it
    ; back to 0 1 1 0, which the unclipped array would never reach here
    iMatInW[][] = csntoarray(MatInW)
    assert(iMatInW[1][0] == 0 && iMatInW[1][1] == 1)
    assert(iMatInW[1][2] == 1 && iMatInW[1][3] == 0)

    ; --- the sources the output forms read are untouched --------------------
    iSrc[] = csntoarray(Src1d)
    assert(iSrc[0] == 2 && iSrc[1] == 9 && iSrc[3] == 8 && iSrc[6] == 4)
    iMat[][] = csntoarray(Mat)
    assert(iMat[0][1] == 9 && iMat[1][2] == 8 && iMat[2][3] == 7)

    prints("csnum k-rate medfilt paths hold\n")
endin
</CsInstruments>

<CsScore>
i 1 0.000 0.010
i 2 0.006 0.001
e
</CsScore>
</CsoundSynthesizer>
