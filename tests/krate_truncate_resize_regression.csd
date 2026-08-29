<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>

<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

; k-rate coverage for csnhead, csntruncate and csnresize, in both the
; publishing and the in-place form. The in-place ones also stand in for the
; version protocol: they rewrite their source, so a pass that did not record
; its own write would run again on the next k-cycle and fail, since the target
; length is no longer shorter than the axis.

giMatShape[] = fillarray(2, 3)

Ramp@global:CsnArr = csnfromarray(array(0, 1, 2, 3, 4, 5))
Grid@global:CsnArr = csnreshape(csnfromarray(array(0, 1, 2, 10, 11, 12)), giMatShape)
ResizeSource@global:CsnArr = csnfromarray(array(1, 2, 3, 4))
InPlaceGrid@global:CsnArr = csnreshape(csnfromarray(array(0, 1, 2, 10, 11, 12)), giMatShape)
InPlaceRamp@global:CsnArr = csnfromarray(array(1, 2, 3, 4))

HeadOut@global:CsnArr = csnempty(array(0))
TruncOut@global:CsnArr = csnempty(array(0))
TruncAllOut@global:CsnArr = csnempty(array(0))
ResizeOut@global:CsnArr = csnempty(array(0))
GatedOut@global:CsnArr = csnempty(array(0))

instr 1
    kThree init 3
    HeadOut = csnhead(Ramp, kThree)
endin

instr 2
    kTwo init 2
    kOne init 1
    TruncOut = csntruncate(Grid, kTwo, 1)
    TruncAllOut = csntruncate(Grid, kOne)
endin

instr 3
    kShape[] init 1
    kShape[0] = 8
    ResizeOut = csnresize(ResizeSource, kShape)

    ; trig == 0: the pass never runs, so the handle keeps the empty array the
    ; init pass published.
    kNever = 0
    GatedOut = csnresize(ResizeSource, kShape, kNever)
endin

instr 4
    kTwo init 2
    csntruncate InPlaceGrid, kTwo, 1

    kShape[] init 1
    kShape[0] = 6
    csnresize InPlaceRamp, kShape
endin

instr 30
    ; --- csnhead.k --------------------------------------------------------
    iHead[] = csntoarray(HeadOut)
    iHeadDims = csndims(HeadOut)
    iHeadSize = csnsize(HeadOut)
    assert(iHeadDims == 1 && iHeadSize == 3)
    assert(iHead[0] == 0 && iHead[1] == 1 && iHead[2] == 2)

    ; --- csntruncate.k ----------------------------------------------------
    iTrunc[][] = csntoarray(TruncOut)
    iTruncShape[] = csnshape(TruncOut)
    iTruncSize = csnsize(TruncOut)
    assert(iTruncSize == 4)
    assert(iTruncShape[0] == 2 && iTruncShape[1] == 2)
    assert(iTrunc[0][0] == 0 && iTrunc[0][1] == 1)
    assert(iTrunc[1][0] == 10 && iTrunc[1][1] == 11)

    ; no axis given: every axis is shortened
    iTruncAll[][] = csntoarray(TruncAllOut)
    iTruncAllShape[] = csnshape(TruncAllOut)
    assert(iTruncAllShape[0] == 1 && iTruncAllShape[1] == 1)
    assert(iTruncAll[0][0] == 0)

    ; the source is untouched by either
    iGridShape[] = csnshape(Grid)
    iGridSize = csnsize(Grid)
    assert(iGridSize == 6 && iGridShape[0] == 2 && iGridShape[1] == 3)

    ; --- csnresize.k ------------------------------------------------------
    iResized[] = csntoarray(ResizeOut)
    iResizedSize = csnsize(ResizeOut)
    assert(iResizedSize == 8)
    assert(iResized[0] == 1 && iResized[3] == 4)
    assert(iResized[4] == 0 && iResized[7] == 0)

    ; held at trig == 0, so still the empty array from the init pass
    iGatedSize = csnsize(GatedOut)
    assert(iGatedSize == 0)

    ; --- in-place forms ---------------------------------------------------
    iInPlaceGrid[][] = csntoarray(InPlaceGrid)
    iInPlaceGridShape[] = csnshape(InPlaceGrid)
    iInPlaceGridSize = csnsize(InPlaceGrid)
    assert(iInPlaceGridSize == 4)
    assert(iInPlaceGridShape[0] == 2 && iInPlaceGridShape[1] == 2)
    assert(iInPlaceGrid[0][0] == 0 && iInPlaceGrid[0][1] == 1)
    assert(iInPlaceGrid[1][0] == 10 && iInPlaceGrid[1][1] == 11)

    iInPlaceRamp[] = csntoarray(InPlaceRamp)
    iInPlaceRampSize = csnsize(InPlaceRamp)
    assert(iInPlaceRampSize == 6)
    assert(iInPlaceRamp[0] == 1 && iInPlaceRamp[3] == 4)
    assert(iInPlaceRamp[4] == 0 && iInPlaceRamp[5] == 0)
endin
</CsInstruments>

<CsScore>
i 1 0.000 0.040
i 2 0.000 0.040
i 3 0.000 0.040
i 4 0.000 0.040
i 30 0.020 0.001
e
</CsScore>
</CsoundSynthesizer>
