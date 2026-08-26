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
FlatOut@global:CsnArr = csnroll(Source, 1)
AxisOut@global:CsnArr = csnroll(Source, 1, 0)
FlatIn@global:CsnArr = csnfromarray(array(1, 2, 3, 4))
AxisIn@global:CsnArr = csnfromarray(array(1, 2, 3, 4))

Growing@global:CsnArr = csnzeros(array(1))
SameShape@global:CsnArr = csnempty(array(10000))

EmptySource@global:CsnArr = csnempty(array(2, 3))
EmptyFlatOut@global:CsnArr = csnroll(EmptySource, 1)
EmptyAxisOut@global:CsnArr = csnroll(EmptySource, 1, 1)

instr 1
    kShift = 1
    kAxis = 0
    kValues[] = fillarray(1, 2, 3, 4)
    Source = csnfromarray(kValues)
    FlatOut = csnroll(Source, kShift)
    AxisOut = csnroll(Source, kShift, kAxis)
    csnroll(FlatIn, kShift)
    csnroll(AxisIn, kShift, kAxis)

    kLength = (timeinsts() < 0.004 ? 1 : 10000)
    kShape[] = fillarray(kLength)
    Growing = csnzeros(kShape, 0)
    csnroll(Growing, kShift)

    kEmptyAxis = 1
    EmptyFlatOut = csnroll(EmptySource, kShift)
    EmptyAxisOut = csnroll(EmptySource, kShift, kEmptyAxis)
endin

instr 2
    kShape[] = fillarray(10000)
    SameShape = csnzeros(kShape, 0)
endin

instr 3
    kShift = 1
    csnroll(SameShape, kShift)
endin

instr 4
    i0[] = array(0)
    i1[] = array(1)

    assert(csnget(FlatOut, i0) == 4 && csnget(FlatOut, i1) == 1)
    assert(csnget(AxisOut, i0) == 4 && csnget(AxisOut, i1) == 1)
    assert(csnget(FlatIn, i0) == 4 && csnget(FlatIn, i1) == 1)
    assert(csnget(AxisIn, i0) == 4 && csnget(AxisIn, i1) == 1)
    assert(csnsize(Growing) == 10000)
    assert(csnsize(SameShape) == 10000)
    assert(csnsize(EmptyFlatOut) == 0 && csnsize(EmptyAxisOut) == 0)
endin
</CsInstruments>

<CsScore>
i 1 0 0.012
i 2 0.004 0.008
i 3 0 0.012
i 4 0.009 0.001
e
</CsScore>
</CsoundSynthesizer>
