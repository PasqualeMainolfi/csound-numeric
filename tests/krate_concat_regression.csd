<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>

<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

FlatA@global:CsnArr = csnfromarray(array(1, 2))
FlatB@global:CsnArr = csnfromarray(array(3, 4, 5))
FlatMutable@global:CsnArr = csnfromarray(array(1, 2))
EmptyFlat@global:CsnArr = csnempty(array(7))

BlockA@global:CsnArr = csnzeros(array(2, 2))
BlockB@global:CsnArr = csnones(array(2, 2))
EmptyBlockA@global:CsnArr = csnempty(array(2, 2))
EmptyBlockB@global:CsnArr = csnempty(array(2, 2))

ComplexA@global:CsnArr = csntocomplex(FlatA)
ComplexB@global:CsnArr = csntocomplex(FlatB)

FlatZeroOut@global:CsnArr = csnempty(array(0))
FlatPulseOut@global:CsnArr = csnempty(array(0))
FlatEmptyLeftOut@global:CsnArr = csnempty(array(0))
FlatEmptyRightOut@global:CsnArr = csnempty(array(0))
BlockZeroOut@global:CsnArr = csnempty(array(0, 0))
BlockAxis0Out@global:CsnArr = csnempty(array(0, 0))
BlockAxis1Out@global:CsnArr = csnempty(array(0, 0))
BlockEmptyLeftOut@global:CsnArr = csnempty(array(0, 0))
BlockEmptyRightOut@global:CsnArr = csnempty(array(0, 0))
BlockBothEmptyOut@global:CsnArr = csnempty(array(0, 0))
BlockEmptyZeroOut@global:CsnArr = csnempty(array(0, 0))
ComplexOut@global:CsnArr = csnempty(array(0), 1)

instr 1
    kZero = 0
    kPulse = (timeinstk() == 2 ? 1 : 0)
    kAxis0 = 0
    kAxis1 = 1

    ; Change one source value before the pulse. The flat result must be
    ; republished even though its shape and type do not change.
    kIndex[] = init(1)
    kIndex[0] = 0
    kValue = (timeinstk() >= 2 ? 9 : 1)
    csnset(FlatMutable, kIndex, kValue)

    FlatZeroOut = csnconcat(FlatA, FlatB, kZero)
    FlatPulseOut = csnconcat(FlatMutable, FlatB, kPulse)
    FlatEmptyLeftOut = csnconcat(EmptyFlat, FlatB, kPulse)
    FlatEmptyRightOut = csnconcat(FlatA, EmptyFlat, kPulse)

    ; A zero trigger retains the source-copy slot published by block.k init.
    BlockZeroOut = csnconcat(BlockA, BlockB, kAxis1, kZero)
    BlockAxis0Out = csnconcat(BlockA, BlockB, kAxis0, kPulse)
    BlockAxis1Out = csnconcat(BlockA, BlockB, kAxis1, kPulse)
    BlockEmptyLeftOut = csnconcat(EmptyBlockA, BlockB, kAxis0, kPulse)
    BlockEmptyRightOut = csnconcat(BlockA, EmptyBlockB, kAxis1, kPulse)
    BlockBothEmptyOut = csnconcat(EmptyBlockA, EmptyBlockB, kAxis0, kPulse)
    BlockEmptyZeroOut = csnconcat(EmptyBlockA, BlockB, kAxis1, kZero)

    ComplexOut = csnconcat(ComplexA, ComplexB, kPulse)
endin

instr 2
    iFlatZero[] = csntoarray(FlatZeroOut)
    iFlatPulse[] = csntoarray(FlatPulseOut)
    iFlatLeft[] = csntoarray(FlatEmptyLeftOut)
    iFlatRight[] = csntoarray(FlatEmptyRightOut)

    assert(csnsize(FlatZeroOut) == 5)
    assert(iFlatZero[0] == 1 && iFlatZero[1] == 2)
    assert(iFlatZero[2] == 3 && iFlatZero[3] == 4 && iFlatZero[4] == 5)

    assert(csnsize(FlatPulseOut) == 5)
    assert(iFlatPulse[0] == 9 && iFlatPulse[1] == 2)
    assert(iFlatPulse[2] == 3 && iFlatPulse[3] == 4 && iFlatPulse[4] == 5)

    assert(csnsize(FlatEmptyLeftOut) == 3)
    assert(iFlatLeft[0] == 3 && iFlatLeft[1] == 4 && iFlatLeft[2] == 5)
    assert(csnsize(FlatEmptyRightOut) == 2)
    assert(iFlatRight[0] == 1 && iFlatRight[1] == 2)

    iBlockZeroShape[] = csnshape(BlockZeroOut)
    iBlockAxis0Shape[] = csnshape(BlockAxis0Out)
    iBlockAxis1Shape[] = csnshape(BlockAxis1Out)
    iEmptyLeftShape[] = csnshape(BlockEmptyLeftOut)
    iEmptyRightShape[] = csnshape(BlockEmptyRightOut)
    iBothEmptyShape[] = csnshape(BlockBothEmptyOut)
    iEmptyZeroShape[] = csnshape(BlockEmptyZeroOut)

    assert(csnsize(BlockZeroOut) == 4)
    assert(iBlockZeroShape[0] == 2 && iBlockZeroShape[1] == 2)
    assert(csnsize(BlockAxis0Out) == 8)
    assert(iBlockAxis0Shape[0] == 4 && iBlockAxis0Shape[1] == 2)
    assert(csnsize(BlockAxis1Out) == 8)
    assert(iBlockAxis1Shape[0] == 2 && iBlockAxis1Shape[1] == 4)

    i00[] = array(0, 0)
    i02[] = array(0, 2)
    i20[] = array(2, 0)
    iBlockZero00 = csnget(BlockZeroOut, i00)
    iBlockAxis000 = csnget(BlockAxis0Out, i00)
    iBlockAxis020 = csnget(BlockAxis0Out, i20)
    iBlockAxis100 = csnget(BlockAxis1Out, i00)
    iBlockAxis102 = csnget(BlockAxis1Out, i02)
    iBlockEmptyLeft00 = csnget(BlockEmptyLeftOut, i00)
    iBlockEmptyRight00 = csnget(BlockEmptyRightOut, i00)
    assert(iBlockZero00 == 0)
    assert(iBlockAxis000 == 0 && iBlockAxis020 == 1)
    assert(iBlockAxis100 == 0 && iBlockAxis102 == 1)

    assert(csnsize(BlockEmptyLeftOut) == 4)
    assert(iEmptyLeftShape[0] == 2 && iEmptyLeftShape[1] == 2)
    assert(iBlockEmptyLeft00 == 1)
    assert(csnsize(BlockEmptyRightOut) == 4)
    assert(iEmptyRightShape[0] == 2 && iEmptyRightShape[1] == 2)
    assert(iBlockEmptyRight00 == 0)
    assert(csnsize(BlockBothEmptyOut) == 0)
    assert(csnisempty(BlockBothEmptyOut) == 1)
    assert(iBothEmptyShape[0] == 0 && iBothEmptyShape[1] == 2)
    assert(csnsize(BlockEmptyZeroOut) == 0)
    assert(iEmptyZeroShape[0] == 2 && iEmptyZeroShape[1] == 2)

    assert(csntype(ComplexOut) == 1 && csnsize(ComplexOut) == 5)
    iComplexIndex[] = array(2)
    ComplexValue:Complex = csnget(ComplexOut, iComplexIndex)
    iComplexReal = real(ComplexValue)
    iComplexImag = imag(ComplexValue)
    assert(iComplexReal == 3 && iComplexImag == 0)
endin
</CsInstruments>

<CsScore>
i 1 0.000 0.010
i 2 0.004 0.001
e
</CsScore>
</CsoundSynthesizer>
