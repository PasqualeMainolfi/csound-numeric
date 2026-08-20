<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>

<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

Source@global:CsnArr = csnfromarray(array(1, 2, 3, 4))
Reshaped@global:CsnArr = csnreshape(Source, array(2, 2))
ReshapedAlias@global:CsnArr = csnreshape(Source, array(2, 2))
InPlace@global:CsnArr = csnfromarray(array(1, 2, 3, 4))
DynamicSource@global:CsnArr = csnfull(array(1), 0)
DynamicReshaped@global:CsnArr = csnreshape(DynamicSource, array(1))
DynamicAlias@global:CsnArr = csnreshape(DynamicSource, array(1))

instr 1
    kElapsed = timeinsts()
    kRows = (kElapsed < 0.015 ? 2 : 1)
    kCols = (kElapsed < 0.015 ? 2 : 4)
    kType = (kElapsed < 0.015 ? 0 : 1)
    kFour = 4
    kShape[] = fillarray(kRows, kCols)
    kSourceShape[] = fillarray(kFour)

    DynamicSource = csnfull(kSourceShape, 5, kType)
    Reshaped = csnreshape(Source, kShape)
    DynamicReshaped = csnreshape(DynamicSource, kShape)
    csnreshape(InPlace, kShape)
endin

instr 2
    iSourceIndex[] = array(0)
    iInPlaceIndex[] = array(0, 0)
    ReshapedAlias = Reshaped
    DynamicAlias = DynamicReshaped
    csnset(Source, iSourceIndex, 9)
    csnset(InPlace, iInPlaceIndex, 8)
endin

instr 3
    iIndex[] = array(0, 0)
    assert(csnget(ReshapedAlias, iIndex) == 9)
    assert(csnget(InPlace, iIndex) == 8)
    assert(csntype(DynamicAlias) == 0 && csnget(DynamicAlias, iIndex) == 5)
endin

instr 4
    iIndex[] = array(0, 0)
    iLast[] = array(0, 3)
    iReshapedShape[] = csnshape(ReshapedAlias)
    iInPlaceShape[] = csnshape(InPlace)

    assert(csnget(ReshapedAlias, iIndex) == 9)
    assert(csnget(InPlace, iIndex) == 8)
    assert(csnget(ReshapedAlias, iLast) == 4)
    assert(csnget(InPlace, iLast) == 4)
    assert(iReshapedShape[0] == 1 && iReshapedShape[1] == 4)
    assert(iInPlaceShape[0] == 1 && iInPlaceShape[1] == 4)

    D:Complex = csnget(DynamicAlias, iIndex)
    iDReal = real(D)
    iDImag = imag(D)
    assert(csntype(DynamicAlias) == 1)
    assert(iDReal == 5 && iDImag == 0)
endin
</CsInstruments>

<CsScore>
i 1 0 0.03
i 2 0.005 0.001
i 3 0.010 0.001
i 4 0.022 0.001
e
</CsScore>
</CsoundSynthesizer>
