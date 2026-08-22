<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>

<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

Real@global:CsnArr = csnfromarray(array(1, 2, 3))
ComplexArray@global:CsnArr = csnempty(array(0), 1)
BlockSource@global:CsnArr = csnzeros(array(2, 4))
BlockRemoved@global:CsnArr = csnremove(BlockSource, 1, 1)
EmptyBlockSource@global:CsnArr = csnempty(array(2, 4))
EmptyBlockRemoved@global:CsnArr = csnremove(EmptyBlockSource, 1, 1)
UnitExtentSource@global:CsnArr = csnzeros(array(2, 1))
UnitExtentRemoved@global:CsnArr = csnremove(UnitExtentSource, 1, 0)

; A zero trigger must leave the array untouched on every control cycle.
instr 1
    kValue = 9
    kIndex = 1
    kTrig = 0
    csninsert(Real, kValue, kIndex, kTrig)
endin

instr 2
    iValues[] = csntoarray(Real)
    assert(csnsize(Real) == 3)
    assert(iValues[0] == 1 && iValues[1] == 2 && iValues[2] == 3)
endin

; A one-cycle pulse inserts exactly once at the current k-rate index.
instr 3
    kValue = 9
    kIndex = 1
    kTrig = (timeinstk() == 2 ? 1 : 0)
    csninsert(Real, kValue, kIndex, kTrig)
endin

instr 4
    iValues[] = csntoarray(Real)
    assert(csnsize(Real) == 4)
    assert(iValues[0] == 1 && iValues[1] == 9)
    assert(iValues[2] == 2 && iValues[3] == 3)
endin

instr 5
    kIndex = 2
    kTrig = 0
    kRemoved = csnremove(Real, kIndex, kTrig)
endin

instr 6
    iValues[] = csntoarray(Real)
    assert(csnsize(Real) == 4)
    assert(iValues[0] == 1 && iValues[1] == 9)
    assert(iValues[2] == 2 && iValues[3] == 3)
endin

; Removal compacts the selected position exactly once per pulse.
instr 7
    kIndex = 2
    kTrig = (timeinstk() == 2 ? 1 : 0)
    kRemoved = csnremove(Real, kIndex, kTrig)
endin

instr 8
    iValues[] = csntoarray(Real)
    assert(csnsize(Real) == 3)
    assert(iValues[0] == 1 && iValues[1] == 9 && iValues[2] == 3)
endin

; Build a complex array through the k-rate insertion path.
instr 9
    Value:Complex = init(0, 0, 0)
    kIndex = 0
    kTrig = (timeinstk() == 2 ? 1 : 0)
    csninsert(ComplexArray, Value, kIndex, kTrig)
endin

instr 10
    Value:Complex = init(0, 0, 0)
    kIndex = 1
    kTrig = (timeinstk() == 2 ? 1 : 0)
    csninsert(ComplexArray, Value, kIndex, kTrig)
endin

instr 11
    i0[] = array(0)
    i1[] = array(1)
    First:Complex = csnget(ComplexArray, i0)
    Second:Complex = csnget(ComplexArray, i1)
    iFirstReal = real(First)
    iFirstImag = imag(First)
    iSecondReal = real(Second)
    iSecondImag = imag(Second)
    assert(csnsize(ComplexArray) == 2)
    assert(iFirstReal == 0 && iFirstImag == 0)
    assert(iSecondReal == 0 && iSecondImag == 0)
endin

instr 12
    kIndex = 0
    kTrig = (timeinstk() == 2 ? 1 : 0)
    Removed:Complex = csnremove(ComplexArray, kIndex, kTrig)
endin

instr 13
    i0[] = array(0)
    Remaining:Complex = csnget(ComplexArray, i0)
    iRemainingReal = real(Remaining)
    iRemainingImag = imag(Remaining)
    assert(csnsize(ComplexArray) == 1)
    assert(iRemainingReal == 0 && iRemainingImag == 0)
endin

; A zero trigger preserves the initialized block-removal result.
instr 14
    kAxis = 1
    kIndex = 0
    kTrig = 0
    BlockRemoved = csnremove(BlockSource, kAxis, kIndex, kTrig)
endin

instr 15
    iShape[] = csnshape(BlockRemoved)
    assert(csnsize(BlockRemoved) == 8)
    assert(iShape[0] == 2 && iShape[1] == 4)
endin

; A pulse recomputes the output using the current axis and index.
instr 16
    kAxis = 1
    kIndex = 2
    kTrig = (timeinstk() == 2 ? 1 : 0)
    BlockRemoved = csnremove(BlockSource, kAxis, kIndex, kTrig)
endin

instr 17
    iShape[] = csnshape(BlockRemoved)
    assert(csnsize(BlockRemoved) == 6)
    assert(iShape[0] == 2 && iShape[1] == 3)
endin

; Physical shape is retained while logical emptiness propagates.
instr 18
    kAxis = 1
    kIndex = 1
    kTrig = (timeinstk() == 2 ? 1 : 0)
    EmptyBlockRemoved = csnremove(EmptyBlockSource, kAxis, kIndex, kTrig)
endin

instr 19
    iShape[] = csnshape(EmptyBlockRemoved)
    assert(csnsize(EmptyBlockRemoved) == 0)
    assert(csnisempty(EmptyBlockRemoved) == 1)
    assert(iShape[0] == 2 && iShape[1] == 3)
endin

; Removing the sole block is valid and produces a zero extent.
instr 20
    kAxis = 1
    kIndex = 0
    kTrig = (timeinstk() == 2 ? 1 : 0)
    UnitExtentRemoved = csnremove(UnitExtentSource, kAxis, kIndex, kTrig)
endin

instr 21
    iShape[] = csnshape(UnitExtentRemoved)
    assert(csnsize(UnitExtentRemoved) == 0)
    assert(csnisempty(UnitExtentRemoved) == 1)
    assert(iShape[0] == 2 && iShape[1] == 0)
endin
</CsInstruments>

<CsScore>
i 1  0.000 0.003
i 2  0.004 0.001
i 3  0.005 0.003
i 4  0.009 0.001
i 5  0.010 0.003
i 6  0.014 0.001
i 7  0.015 0.003
i 8  0.019 0.001
i 9  0.000 0.003
i 10 0.000 0.003
i 11 0.004 0.001
i 12 0.005 0.003
i 13 0.009 0.001
i 14 0.020 0.006
i 15 0.024 0.001
i 16 0.027 0.006
i 17 0.031 0.001
i 18 0.020 0.006
i 19 0.024 0.001
i 20 0.027 0.006
i 21 0.031 0.001
e
</CsScore>
</CsoundSynthesizer>
