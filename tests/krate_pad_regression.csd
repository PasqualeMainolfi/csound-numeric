<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>

<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

Flat@global:CsnArr = csnfromarray(array(1, 2, 3))
Block@global:CsnArr = csnones(array(2, 3))
EmptyFlat@global:CsnArr = csnempty(array(4))
ComplexFlat@global:CsnArr = csntocomplex(Flat)
ComplexBlock@global:CsnArr = csntocomplex(Block)

PadZeroOut@global:CsnArr = csnempty(array(0))
PadFlatOut@global:CsnArr = csnempty(array(0))
PadAxis0Out@global:CsnArr = csnempty(array(0, 0))
PadAxis1Out@global:CsnArr = csnempty(array(0, 0))
PadAllAxesOut@global:CsnArr = csnempty(array(0, 0))
PadEmptyOut@global:CsnArr = csnempty(array(0))
PadComplexOut@global:CsnArr = csnempty(array(0), 1)
PadComplexAxisOut@global:CsnArr = csnempty(array(0, 0), 1)

PadInFlat@global:CsnArr = csnfromarray(array(1, 2, 3))
PadInBlock@global:CsnArr = csnones(array(2, 3))
PadInComplex@global:CsnArr = csntocomplex(Flat)

instr 1
    kZero = 0
    kPulse = (timeinstk() == 2 ? 1 : 0)
    kBefore = 1
    kAfter = 2
    kValue = -7
    kAxis0 = 0
    kAxis1 = 1
    kAllAxes = -1
    CValue:Complex = init(5, 6, 0)

    ; A zero trigger keeps the source copy published by the .k init pass.
    PadZeroOut = csnpad(Flat, kBefore, kAfter, kValue, kZero)

    ; 1-D pad: the single axis grows by before + after.
    PadFlatOut = csnpad(Flat, kBefore, kAfter, kValue, kPulse)

    ; 2-D pads on one axis, and the -1 request that pads every axis.
    PadAxis0Out = csnpad(Block, kBefore, kAfter, kValue, kAxis0, kPulse)
    PadAxis1Out = csnpad(Block, kBefore, kAfter, kValue, kAxis1, kPulse)
    PadAllAxesOut = csnpad(Block, kBefore, kAfter, kValue, kAllAxes, kPulse)

    ; A logically empty source contributes no extent, so only padding is left.
    PadEmptyOut = csnpad(EmptyFlat, kBefore, kAfter, kValue, kPulse)

    PadComplexOut = csnpad(ComplexFlat, kBefore, kAfter, CValue, kPulse)
    PadComplexAxisOut = csnpad(ComplexBlock, kBefore, kAfter, CValue, kAxis1, kPulse)

    ; In-place forms rewrite the source array itself, once per pulse.
    csnpad(PadInFlat, kBefore, kAfter, kValue, kPulse)
    csnpad(PadInBlock, kBefore, kAfter, kValue, kAxis0, kPulse)
    csnpad(PadInComplex, kBefore, kAfter, CValue, kPulse)
endin

instr 2
    ; --- untriggered output keeps the init-time copy of the source ---------
    iZero[] = csntoarray(PadZeroOut)
    assert(csnsize(PadZeroOut) == 3)
    assert(iZero[0] == 1 && iZero[1] == 2 && iZero[2] == 3)

    ; --- 1-D ---------------------------------------------------------------
    iFlat[] = csntoarray(PadFlatOut)
    assert(csnsize(PadFlatOut) == 6)
    assert(iFlat[0] == -7)
    assert(iFlat[1] == 1 && iFlat[2] == 2 && iFlat[3] == 3)
    assert(iFlat[4] == -7 && iFlat[5] == -7)

    ; --- 2-D, axis 0 -------------------------------------------------------
    iAxis0Shape[] = csnshape(PadAxis0Out)
    assert(iAxis0Shape[0] == 5 && iAxis0Shape[1] == 3)
    assert(csnsize(PadAxis0Out) == 15)
    i00[] = array(0, 0)
    i10[] = array(1, 0)
    i30[] = array(3, 0)
    assert(csnget(PadAxis0Out, i00) == -7)
    assert(csnget(PadAxis0Out, i10) == 1)
    assert(csnget(PadAxis0Out, i30) == -7)

    ; --- 2-D, axis 1 -------------------------------------------------------
    iAxis1Shape[] = csnshape(PadAxis1Out)
    assert(iAxis1Shape[0] == 2 && iAxis1Shape[1] == 6)
    assert(csnsize(PadAxis1Out) == 12)
    i01[] = array(0, 1)
    i04[] = array(0, 4)
    assert(csnget(PadAxis1Out, i00) == -7)
    assert(csnget(PadAxis1Out, i01) == 1)
    assert(csnget(PadAxis1Out, i04) == -7)

    ; --- 2-D, every axis ---------------------------------------------------
    iAllShape[] = csnshape(PadAllAxesOut)
    assert(iAllShape[0] == 5 && iAllShape[1] == 6)
    assert(csnsize(PadAllAxesOut) == 30)
    i11[] = array(1, 1)
    i23[] = array(2, 3)
    assert(csnget(PadAllAxesOut, i00) == -7)
    assert(csnget(PadAllAxesOut, i11) == 1)
    assert(csnget(PadAllAxesOut, i23) == 1)
    assert(csnget(PadAllAxesOut, i30) == -7)

    ; --- empty source ------------------------------------------------------
    iEmpty[] = csntoarray(PadEmptyOut)
    assert(csnsize(PadEmptyOut) == 3)
    assert(iEmpty[0] == -7 && iEmpty[1] == -7 && iEmpty[2] == -7)

    ; --- complex -----------------------------------------------------------
    assert(csntype(PadComplexOut) == 1 && csnsize(PadComplexOut) == 6)
    iC0[] = array(0)
    iC1[] = array(1)
    CPadFill:Complex = csnget(PadComplexOut, iC0)
    CPadKept:Complex = csnget(PadComplexOut, iC1)
    iPadFillReal = real(CPadFill)
    iPadFillImag = imag(CPadFill)
    iPadKeptReal = real(CPadKept)
    iPadKeptImag = imag(CPadKept)
    assert(iPadFillReal == 5 && iPadFillImag == 6)
    assert(iPadKeptReal == 1 && iPadKeptImag == 0)

    iComplexAxisShape[] = csnshape(PadComplexAxisOut)
    assert(csntype(PadComplexAxisOut) == 1)
    assert(iComplexAxisShape[0] == 2 && iComplexAxisShape[1] == 6)
    CAxisFill:Complex = csnget(PadComplexAxisOut, i00)
    CAxisKept:Complex = csnget(PadComplexAxisOut, i01)
    iAxisFillReal = real(CAxisFill)
    iAxisFillImag = imag(CAxisFill)
    iAxisKeptReal = real(CAxisKept)
    iAxisKeptImag = imag(CAxisKept)
    assert(iAxisFillReal == 5 && iAxisFillImag == 6)
    assert(iAxisKeptReal == 1 && iAxisKeptImag == 0)

    ; --- in place ----------------------------------------------------------
    iInFlat[] = csntoarray(PadInFlat)
    assert(csnsize(PadInFlat) == 6)
    assert(iInFlat[0] == -7)
    assert(iInFlat[1] == 1 && iInFlat[2] == 2 && iInFlat[3] == 3)
    assert(iInFlat[4] == -7 && iInFlat[5] == -7)

    iInBlockShape[] = csnshape(PadInBlock)
    assert(iInBlockShape[0] == 5 && iInBlockShape[1] == 3)
    assert(csnsize(PadInBlock) == 15)
    assert(csnget(PadInBlock, i00) == -7)
    assert(csnget(PadInBlock, i10) == 1)
    assert(csnget(PadInBlock, i30) == -7)

    assert(csntype(PadInComplex) == 1 && csnsize(PadInComplex) == 6)
    CInFill:Complex = csnget(PadInComplex, iC0)
    CInKept:Complex = csnget(PadInComplex, iC1)
    iInFillReal = real(CInFill)
    iInFillImag = imag(CInFill)
    iInKeptReal = real(CInKept)
    iInKeptImag = imag(CInKept)
    assert(iInFillReal == 5 && iInFillImag == 6)
    assert(iInKeptReal == 1 && iInKeptImag == 0)

    ; The sources themselves are untouched by the out-of-place forms.
    iSource[] = csntoarray(Flat)
    assert(csnsize(Flat) == 3)
    assert(iSource[0] == 1 && iSource[1] == 2 && iSource[2] == 3)
endin
</CsInstruments>

<CsScore>
i 1 0.000 0.010
i 2 0.004 0.001
e
</CsScore>
</CsoundSynthesizer>
