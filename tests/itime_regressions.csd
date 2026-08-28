<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>

<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 2
0dbfs = 1

instr 1
    iIndex0[] = fillarray(0)
    iIndex1[] = fillarray(1)
    iShape1[] = fillarray(1)

    ; ------------------------------------------------------------------
    ; Previously fixed scalar/complex regressions.
    ; ------------------------------------------------------------------
    iLogspace:CsnArr = csnlogspace(2, 8, 1, 10)
    iLogspaceValue = csnget(iLogspace, iIndex0)
    assert(abs(iLogspaceValue - 100) < 1e-12)

    A:Complex = init(1, 2, 0)
    B:Complex = init(3, 4, 0)
    Ten:Complex = init(10, 0, 0)

    iArrA:CsnArr = csnfull(iShape1, A)
    iArrB:CsnArr = csnfull(iShape1, B)
    iReal10:CsnArr = csnfull(iShape1, 10)

    ; (1 + 2i) * (3 + 4i) = -5 + 10i.
    iProduct:CsnArr = csnmul(iArrA, iArrB)
    Product:Complex = csnget(iProduct, iIndex0)
    iProductReal = real(Product)
    iProductImag = imag(Product)
    assert(abs(iProductReal + 5) < 1e-12)
    assert(abs(iProductImag - 10) < 1e-12)

    ; A real left operand must remain on the left during promotion.
    iMixed:CsnArr = csnadd(iReal10, iArrB)
    Mixed:Complex = csnget(iMixed, iIndex0)
    iMixedReal = real(Mixed)
    iMixedImag = imag(Mixed)
    assert(abs(iMixedReal - 13) < 1e-12)
    assert(abs(iMixedImag - 4) < 1e-12)

    ; (10 + 0i) / (3 + 4i) = 1.2 - 1.6i.
    iDivision:CsnArr = csndiv(Ten, iArrB)
    Division:Complex = csnget(iDivision, iIndex0)
    iDivisionReal = real(Division)
    iDivisionImag = imag(Division)
    assert(abs(iDivisionReal - 1.2) < 1e-12)
    assert(abs(iDivisionImag + 1.6) < 1e-12)

    iCosh:CsnArr = csncosh(iArrA)
    CoshValue:Complex = csnget(iCosh, iIndex0)
    iCoshReal = real(CoshValue)
    iCoshImag = imag(CoshValue)
    assert(abs(iCoshReal - cosh(1) * cos(2)) < 1e-12)
    assert(abs(iCoshImag - sinh(1) * sin(2)) < 1e-12)

    ; ------------------------------------------------------------------
    ; Comparisons return shape-preserving 0/1 masks.
    ; ------------------------------------------------------------------
    iValues[] = fillarray(40, 10, 30, 20)
    iValuesArr:CsnArr = csnfromarray(iValues)

    iGt:CsnArr = csngt(iValuesArr, 30)
    iGe:CsnArr = csnge(iValuesArr, 30)
    iLt:CsnArr = csnlt(iValuesArr, 30)
    iLe:CsnArr = csnle(iValuesArr, 30)
    iEq:CsnArr = csneq(iValuesArr, 30)
    iNe:CsnArr = csnne(iValuesArr, 30)
    iGtValues[] = csntoarray(iGt)
    iGeValues[] = csntoarray(iGe)
    iLtValues[] = csntoarray(iLt)
    iLeValues[] = csntoarray(iLe)
    iEqValues[] = csntoarray(iEq)
    iNeValues[] = csntoarray(iNe)

    assert(iGtValues[0] == 1 && iGtValues[1] == 0 && iGtValues[2] == 0 && iGtValues[3] == 0)
    assert(iGeValues[0] == 1 && iGeValues[1] == 0 && iGeValues[2] == 1 && iGeValues[3] == 0)
    assert(iLtValues[0] == 0 && iLtValues[1] == 1 && iLtValues[2] == 0 && iLtValues[3] == 1)
    assert(iLeValues[0] == 0 && iLeValues[1] == 1 && iLeValues[2] == 1 && iLeValues[3] == 1)
    assert(iEqValues[0] == 0 && iEqValues[1] == 0 && iEqValues[2] == 1 && iEqValues[3] == 0)
    assert(iNeValues[0] == 1 && iNeValues[1] == 1 && iNeValues[2] == 0 && iNeValues[3] == 1)

    ; ------------------------------------------------------------------
    ; Logical overloads: handle-handle, handle-scalar, scalar-handle, not.
    ; ------------------------------------------------------------------
    iTruthA[] = fillarray(0, 1, 2, 0)
    iTruthB[] = fillarray(0, 0, -3, 4)
    iTruthArrA:CsnArr = csnfromarray(iTruthA)
    iTruthArrB:CsnArr = csnfromarray(iTruthB)

    iAndHH:CsnArr = csnlogicand(iTruthArrA, iTruthArrB)
    iOrHH:CsnArr = csnlogicor(iTruthArrA, iTruthArrB)
    iAndHS:CsnArr = csnlogicand(iTruthArrA, 1)
    iOrHS:CsnArr = csnlogicor(iTruthArrA, 0)
    iAndSH:CsnArr = csnlogicand(1, iTruthArrA)
    iOrSH:CsnArr = csnlogicor(0, iTruthArrA)
    iNot:CsnArr = csnlogicnot(iTruthArrA)
    iAndHHValues[] = csntoarray(iAndHH)
    iOrHHValues[] = csntoarray(iOrHH)
    iAndHSValues[] = csntoarray(iAndHS)
    iOrHSValues[] = csntoarray(iOrHS)
    iAndSHValues[] = csntoarray(iAndSH)
    iOrSHValues[] = csntoarray(iOrSH)
    iNotValues[] = csntoarray(iNot)

    assert(iAndHHValues[0] == 0 && iAndHHValues[1] == 0 && iAndHHValues[2] == 1 && iAndHHValues[3] == 0)
    assert(iOrHHValues[0] == 0 && iOrHHValues[1] == 1 && iOrHHValues[2] == 1 && iOrHHValues[3] == 1)
    assert(iAndHSValues[0] == 0 && iAndHSValues[1] == 1 && iAndHSValues[2] == 1 && iAndHSValues[3] == 0)
    assert(iOrHSValues[0] == 0 && iOrHSValues[1] == 1 && iOrHSValues[2] == 1 && iOrHSValues[3] == 0)
    assert(iAndSHValues[0] == 0 && iAndSHValues[1] == 1 && iAndSHValues[2] == 1 && iAndSHValues[3] == 0)
    assert(iOrSHValues[0] == 0 && iOrSHValues[1] == 1 && iOrSHValues[2] == 1 && iOrSHValues[3] == 0)
    assert(iNotValues[0] == 1 && iNotValues[1] == 0 && iNotValues[2] == 0 && iNotValues[3] == 1)

    ; ------------------------------------------------------------------
    ; Sort/argsort: flat, both axes, and in-place overload.
    ; ------------------------------------------------------------------
    iSorted:CsnArr = csnsort(iValuesArr, -1)
    iArgsorted:CsnArr = csnargsort(iValuesArr, -1)
    iSortedValues[] = csntoarray(iSorted)
    iArgsortedValues[] = csntoarray(iArgsorted)
    assert(iSortedValues[0] == 10 && iSortedValues[1] == 20 && iSortedValues[2] == 30 && iSortedValues[3] == 40)
    assert(iArgsortedValues[0] == 1 && iArgsortedValues[1] == 3 && iArgsortedValues[2] == 2 && iArgsortedValues[3] == 0)

    iMatrix[][] = init(2, 3)
    iMatrix[0][0] = 4
    iMatrix[0][1] = 1
    iMatrix[0][2] = 3
    iMatrix[1][0] = 2
    iMatrix[1][1] = 6
    iMatrix[1][2] = 5
    iMatrixArr:CsnArr = csnfromarray(iMatrix)

    iSortAxis0:CsnArr = csnsort(iMatrixArr, 0)
    iSortAxis1:CsnArr = csnsort(iMatrixArr, 1)
    iArgsortAxis0:CsnArr = csnargsort(iMatrixArr, 0)
    iArgsortAxis1:CsnArr = csnargsort(iMatrixArr, 1)
    iSortAxis0Values[][] = csntoarray(iSortAxis0)
    iSortAxis1Values[][] = csntoarray(iSortAxis1)
    iArgsortAxis0Values[][] = csntoarray(iArgsortAxis0)
    iArgsortAxis1Values[][] = csntoarray(iArgsortAxis1)

    assert(iSortAxis0Values[0][0] == 2 && iSortAxis0Values[0][1] == 1 && iSortAxis0Values[0][2] == 3)
    assert(iSortAxis0Values[1][0] == 4 && iSortAxis0Values[1][1] == 6 && iSortAxis0Values[1][2] == 5)
    assert(iSortAxis1Values[0][0] == 1 && iSortAxis1Values[0][1] == 3 && iSortAxis1Values[0][2] == 4)
    assert(iSortAxis1Values[1][0] == 2 && iSortAxis1Values[1][1] == 5 && iSortAxis1Values[1][2] == 6)
    assert(iArgsortAxis0Values[0][0] == 1 && iArgsortAxis0Values[0][1] == 0 && iArgsortAxis0Values[0][2] == 0)
    assert(iArgsortAxis0Values[1][0] == 0 && iArgsortAxis0Values[1][1] == 1 && iArgsortAxis0Values[1][2] == 1)
    assert(iArgsortAxis1Values[0][0] == 1 && iArgsortAxis1Values[0][1] == 2 && iArgsortAxis1Values[0][2] == 0)
    assert(iArgsortAxis1Values[1][0] == 0 && iArgsortAxis1Values[1][1] == 2 && iArgsortAxis1Values[1][2] == 1)

    iSortIn:CsnArr = csncopy(iMatrixArr)
    csnsort(iSortIn, 0)
    iSortInValues[][] = csntoarray(iSortIn)
    assert(iSortInValues[0][0] == 2 && iSortInValues[0][1] == 1 && iSortInValues[0][2] == 3)
    assert(iSortInValues[1][0] == 4 && iSortInValues[1][1] == 6 && iSortInValues[1][2] == 5)

    ; Empty sort/argsort must return usable empty arrays.
    iEmptyShape[] = fillarray(0)
    iEmpty:CsnArr = csnempty(iEmptyShape)
    iEmptySort:CsnArr = csnsort(iEmpty, -1)
    iEmptyArgsort:CsnArr = csnargsort(iEmpty, -1)
    assert(csnsize(iEmptySort) == 0)
    assert(csnsize(iEmptyArgsort) == 0)

    ; ------------------------------------------------------------------
    ; Percentile/quantile: sorted private buffers, interpolation and axes.
    ; ------------------------------------------------------------------
    assert(abs(csnpercentile(iValuesArr, 0) - 10) < 1e-12)
    assert(abs(csnpercentile(iValuesArr, 25) - 17.5) < 1e-12)
    assert(abs(csnpercentile(iValuesArr, 50) - 25) < 1e-12)
    assert(abs(csnpercentile(iValuesArr, 75) - 32.5) < 1e-12)
    assert(abs(csnpercentile(iValuesArr, 100) - 40) < 1e-12)
    assert(abs(csnquantile(iValuesArr, 0.25) - 17.5) < 1e-12)
    assert(abs(csnquantile(iValuesArr, 0.5) - 25) < 1e-12)
    assert(abs(csnquantile(iValuesArr, 0.75) - 32.5) < 1e-12)

    iNaN = sqrt(-1)
    iNaNValues[] = fillarray(1, iNaN, 3)
    iNaNArr:CsnArr = csnfromarray(iNaNValues)
    iNaNPercentile = csnpercentile(iNaNArr, 50)
    iNaNQuantile = csnquantile(iNaNArr, 0.5)
    assert(iNaNPercentile != iNaNPercentile)
    assert(iNaNQuantile != iNaNQuantile)

    iPercentileAxis0:CsnArr = csnpercentile(iMatrixArr, 50, 0)
    iPercentileAxis1:CsnArr = csnpercentile(iMatrixArr, 75, 1)
    iQuantileAxis0:CsnArr = csnquantile(iMatrixArr, 0.5, 0)
    iQuantileAxis1:CsnArr = csnquantile(iMatrixArr, 0.25, 1)
    iPercentileAxis0Values[] = csntoarray(iPercentileAxis0)
    iPercentileAxis1Values[] = csntoarray(iPercentileAxis1)
    iQuantileAxis0Values[] = csntoarray(iQuantileAxis0)
    iQuantileAxis1Values[] = csntoarray(iQuantileAxis1)
    assert(abs(iPercentileAxis0Values[0] - 3) < 1e-12 && abs(iPercentileAxis0Values[1] - 3.5) < 1e-12 && abs(iPercentileAxis0Values[2] - 4) < 1e-12)
    assert(abs(iPercentileAxis1Values[0] - 3.5) < 1e-12 && abs(iPercentileAxis1Values[1] - 5.5) < 1e-12)
    assert(abs(iQuantileAxis0Values[0] - 3) < 1e-12 && abs(iQuantileAxis0Values[1] - 3.5) < 1e-12 && abs(iQuantileAxis0Values[2] - 4) < 1e-12)
    assert(abs(iQuantileAxis1Values[0] - 2) < 1e-12 && abs(iQuantileAxis1Values[1] - 3.5) < 1e-12)

    ; ------------------------------------------------------------------
    ; Copy/reverse preserve shape, values, complex type and element pairs.
    ; ------------------------------------------------------------------
    iRealCopy:CsnArr = csncopy(iValuesArr)
    iRealReverse:CsnArr = csnreverse(iValuesArr)
    iRealCopyValues[] = csntoarray(iRealCopy)
    iRealReverseValues[] = csntoarray(iRealReverse)
    assert(iRealCopyValues[0] == 40 && iRealCopyValues[1] == 10 && iRealCopyValues[2] == 30 && iRealCopyValues[3] == 20)
    assert(iRealReverseValues[0] == 20 && iRealReverseValues[1] == 30 && iRealReverseValues[2] == 10 && iRealReverseValues[3] == 40)

    iRealReverseIn:CsnArr = csncopy(iValuesArr)
    csnreverse(iRealReverseIn)
    iRealReverseInValues[] = csntoarray(iRealReverseIn)
    assert(iRealReverseInValues[0] == 20 && iRealReverseInValues[1] == 30 && iRealReverseInValues[2] == 10 && iRealReverseInValues[3] == 40)
    ; Mutating the copy must not modify its source.
    iSourceAfterReverse[] = csntoarray(iValuesArr)
    assert(iSourceAfterReverse[0] == 40 && iSourceAfterReverse[1] == 10 && iSourceAfterReverse[2] == 30 && iSourceAfterReverse[3] == 20)

    iComplexPair:CsnArr = csnconcat(iArrA, iArrB)
    iComplexCopy:CsnArr = csncopy(iComplexPair)
    iComplexReverse:CsnArr = csnreverse(iComplexPair)
    iComplexReverseIn:CsnArr = csncopy(iComplexPair)
    csnreverse(iComplexReverseIn)
    assert(csntype(iComplexCopy) == 1)
    assert(csntype(iComplexReverse) == 1)

    Copy0:Complex = csnget(iComplexCopy, iIndex0)
    Copy1:Complex = csnget(iComplexCopy, iIndex1)
    Reverse0:Complex = csnget(iComplexReverse, iIndex0)
    Reverse1:Complex = csnget(iComplexReverse, iIndex1)
    ReverseIn0:Complex = csnget(iComplexReverseIn, iIndex0)
    ReverseIn1:Complex = csnget(iComplexReverseIn, iIndex1)
    iCopy0Real = real(Copy0)
    iCopy0Imag = imag(Copy0)
    iCopy1Real = real(Copy1)
    iCopy1Imag = imag(Copy1)
    iReverse0Real = real(Reverse0)
    iReverse0Imag = imag(Reverse0)
    iReverse1Real = real(Reverse1)
    iReverse1Imag = imag(Reverse1)
    iReverseIn0Real = real(ReverseIn0)
    iReverseIn0Imag = imag(ReverseIn0)
    iReverseIn1Real = real(ReverseIn1)
    iReverseIn1Imag = imag(ReverseIn1)
    assert(iCopy0Real == 1 && iCopy0Imag == 2 && iCopy1Real == 3 && iCopy1Imag == 4)
    assert(iReverse0Real == 3 && iReverse0Imag == 4 && iReverse1Real == 1 && iReverse1Imag == 2)
    assert(iReverseIn0Real == 3 && iReverseIn0Imag == 4 && iReverseIn1Real == 1 && iReverseIn1Imag == 2)

    ; ------------------------------------------------------------------
    ; numpy.unique semantics: sorted, one-dimensional unique values.
    ; ------------------------------------------------------------------
    iDuplicates[] = fillarray(3, 1, 3, 2, 1)
    iDuplicatesArr:CsnArr = csnfromarray(iDuplicates)
    iUnique:CsnArr = csnunique(iDuplicatesArr)
    iUniqueValues[] = csntoarray(iUnique)
    assert(csndims(iUnique) == 1)
    assert(csnsize(iUnique) == 3)
    assert(iUniqueValues[0] == 1 && iUniqueValues[1] == 2 && iUniqueValues[2] == 3)
endin

instr 2
    iIndex0[] = fillarray(0)
    iShape4[] = fillarray(4)

    ; Generators and clipping.
    iRandom:CsnArr = csnrand(iShape4, -2, 2)
    iArange:CsnArr = csnarange(0, 4, 1)
    iLinspace:CsnArr = csnlinspace(0, 1, 3)
    iGeomspace:CsnArr = csngeomspace(1, 16, 5)
    assert(csnsize(iRandom) == 4)
    iArangeValues[] = csntoarray(iArange)
    iLinspaceValues[] = csntoarray(iLinspace)
    iGeomspaceValues[] = csntoarray(iGeomspace)
    assert(iArangeValues[0] == 0 && iArangeValues[1] == 1 && iArangeValues[2] == 2 && iArangeValues[3] == 3)
    assert(iLinspaceValues[0] == 0 && iLinspaceValues[1] == 0.5 && iLinspaceValues[2] == 1)
    assert(abs(iGeomspaceValues[0] - 1) < 1e-12 && abs(iGeomspaceValues[1] - 2) < 1e-12 && abs(iGeomspaceValues[2] - 4) < 1e-12 && abs(iGeomspaceValues[3] - 8) < 1e-12 && abs(iGeomspaceValues[4] - 16) < 1e-12)

    ; A non-zero seed replays the same stream, so csnrand becomes reproducible.
    ; The registry generator is global state: reseeding here also decides what
    ; every later csnrand draws.
    csnseed 12345
    iSeededA:CsnArr = csnrand(iShape4, 0, 1)
    iSeededAValues[] = csntoarray(iSeededA)
    csnseed 12345
    iSeededB:CsnArr = csnrand(iShape4, 0, 1)
    iSeededBValues[] = csntoarray(iSeededB)
    assert(iSeededAValues[0] == iSeededBValues[0] && iSeededAValues[1] == iSeededBValues[1] && iSeededAValues[2] == iSeededBValues[2] && iSeededAValues[3] == iSeededBValues[3])

    ; A different seed selects a different stream.
    csnseed 54321
    iSeededC:CsnArr = csnrand(iShape4, 0, 1)
    iSeededCValues[] = csntoarray(iSeededC)
    assert(iSeededCValues[0] != iSeededAValues[0])

    iClipSourceValues[] = fillarray(-2, -0.5, 0.5, 2)
    iClipSource:CsnArr = csnfromarray(iClipSourceValues)
    iClipped:CsnArr = csnclip(iClipSource, -1, 1)
    iClippedValues[] = csntoarray(iClipped)
    assert(iClippedValues[0] == -1 && iClippedValues[1] == -0.5 && iClippedValues[2] == 0.5 && iClippedValues[3] == 1)
    iClipIn:CsnArr = csncopy(iClipSource)
    csnclip(iClipIn, -1, 1)
    iClipInValues[] = csntoarray(iClipIn)
    assert(iClipInValues[0] == -1 && iClipInValues[1] == -0.5 && iClipInValues[2] == 0.5 && iClipInValues[3] == 1)

    ; Coordinate selectors and counts.
    iSelectMatrix[][] = init(2, 2)
    iSelectMatrix[0][0] = 1
    iSelectMatrix[0][1] = 2
    iSelectMatrix[1][0] = 3
    iSelectMatrix[1][1] = 4
    iSelectSource:CsnArr = csnfromarray(iSelectMatrix)
    iNeedlesValues[] = fillarray(1, 4)
    iNeedles:CsnArr = csnfromarray(iNeedlesValues)
    iArgwhere:CsnArr = csnargwhere(iSelectSource, iNeedles)
    iArgwhereValues[][] = csntoarray(iArgwhere)
    assert(csnsize(iArgwhere) == 4)
    assert(iArgwhereValues[0][0] == 0 && iArgwhereValues[0][1] == 0)
    assert(iArgwhereValues[1][0] == 1 && iArgwhereValues[1][1] == 1)

    iSparseValues[] = fillarray(0, 2, 0, 3)
    iSparse:CsnArr = csnfromarray(iSparseValues)
    iArgnonzero:CsnArr = csnargnonzero(iSparse)
    iArgnonzeroValues[][] = csntoarray(iArgnonzero)
    assert(iArgnonzeroValues[0][0] == 1 && iArgnonzeroValues[1][0] == 3)

    iNaN = sqrt(-1)
    iWithNaNValues[] = fillarray(1, iNaN, 2)
    iWithNaN:CsnArr = csnfromarray(iWithNaNValues)
    iArgisnan:CsnArr = csnargisnan(iWithNaN)
    iArgisnanValues[][] = csntoarray(iArgisnan)
    assert(csnsize(iArgisnan) == 1 && iArgisnanValues[0][0] == 1)

    iRepeatedValues[] = fillarray(3, 1, 3, 2)
    iRepeated:CsnArr = csnfromarray(iRepeatedValues)
    iArgunique:CsnArr = csnargunique(iRepeated)
    assert(csndims(iArgunique) == 2 && csnsize(iArgunique) == 3)
    assert(csncnteq(iRepeated, 3) == 2)
    assert(csncntnz(iSparse) == 2)
    assert(csncntnan(iWithNaN) == 1)

    ; Scalar and axis reductions.
    assert(csnmin(iRepeated) == 1)
    assert(csnmax(iRepeated) == 3)
    assert(csnmedian(iRepeated) == 2.5)
    iReduceMatrix[][] = init(2, 3)
    iReduceMatrix[0][0] = 1
    iReduceMatrix[0][1] = 5
    iReduceMatrix[0][2] = 3
    iReduceMatrix[1][0] = 4
    iReduceMatrix[1][1] = 2
    iReduceMatrix[1][2] = 6
    iReduceSource:CsnArr = csnfromarray(iReduceMatrix)
    iMinAxis:CsnArr = csnmin(iReduceSource, 0)
    iMaxAxis:CsnArr = csnmax(iReduceSource, 1)
    iMedianAxis:CsnArr = csnmedian(iReduceSource, 0)
    iArgmin:CsnArr = csnargmin(iReduceSource, 1)
    iArgmax:CsnArr = csnargmax(iReduceSource, 1)
    iMinAxisValues[] = csntoarray(iMinAxis)
    iMaxAxisValues[] = csntoarray(iMaxAxis)
    iMedianAxisValues[] = csntoarray(iMedianAxis)
    iArgminValues[][] = csntoarray(iArgmin)
    iArgmaxValues[][] = csntoarray(iArgmax)
    assert(iMinAxisValues[0] == 1 && iMinAxisValues[1] == 2 && iMinAxisValues[2] == 3)
    assert(iMaxAxisValues[0] == 5 && iMaxAxisValues[1] == 6)
    assert(iMedianAxisValues[0] == 2.5 && iMedianAxisValues[1] == 3.5 && iMedianAxisValues[2] == 4.5)
    assert(iArgminValues[0][0] == 0 && iArgminValues[0][1] == 0 && iArgminValues[1][0] == 1 && iArgminValues[1][1] == 1)
    assert(iArgmaxValues[0][0] == 0 && iArgmaxValues[0][1] == 1 && iArgmaxValues[1][0] == 1 && iArgmaxValues[1][1] == 2)

    ; Rounding and vector geometry.
    iRoundInputValues[] = fillarray(-1.7, -1.2, 1.2, 1.7, 2.5)
    iRoundInput:CsnArr = csnfromarray(iRoundInputValues)
    iFloor:CsnArr = csnfloor(iRoundInput)
    iCeil:CsnArr = csnceil(iRoundInput)
    iRound:CsnArr = csnround(iRoundInput)
    iFloorValues[] = csntoarray(iFloor)
    iCeilValues[] = csntoarray(iCeil)
    iRoundValues[] = csntoarray(iRound)
    assert(iFloorValues[0] == -2 && iFloorValues[1] == -2 && iFloorValues[2] == 1 && iFloorValues[3] == 1)
    assert(iCeilValues[0] == -1 && iCeilValues[1] == -1 && iCeilValues[2] == 2 && iCeilValues[3] == 2)
    assert(iRoundValues[0] == -2 && iRoundValues[1] == -1 && iRoundValues[2] == 1 && iRoundValues[3] == 2 && iRoundValues[4] == 2)

    iVecAValues[] = fillarray(3, 4, 0)
    iVecBValues[] = fillarray(1, 0, 0)
    iVecYValues[] = fillarray(0, 1, 0)
    iVecA:CsnArr = csnfromarray(iVecAValues)
    iVecB:CsnArr = csnfromarray(iVecBValues)
    iVecY:CsnArr = csnfromarray(iVecYValues)
    iProject:CsnArr = csnproject(iVecA, iVecB)
    iReject:CsnArr = csnreject(iVecA, iVecB)
    iCross:CsnArr = csncross(iVecB, iVecY)
    iProjectValues[] = csntoarray(iProject)
    iRejectValues[] = csntoarray(iReject)
    iCrossValues[] = csntoarray(iCross)
    assert(iProjectValues[0] == 3 && iProjectValues[1] == 0 && iProjectValues[2] == 0)
    assert(iRejectValues[0] == 0 && iRejectValues[1] == 4 && iRejectValues[2] == 0)
    assert(iCrossValues[0] == 0 && iCrossValues[1] == 0 && iCrossValues[2] == 1)

    iGradientInputValues[] = fillarray(1, 4, 9)
    iGradientInput:CsnArr = csnfromarray(iGradientInputValues)
    iGradient:CsnArr = csngrad(iGradientInput, -1)
    iGradientValues[] = csntoarray(iGradient)
    assert(iGradientValues[0] == 3 && iGradientValues[1] == 4 && iGradientValues[2] == 5)

    ; Moving median/min/max, output and in-place signatures.
    iMovingValues[] = fillarray(1, 5, 2, 4, 3)
    iMoving:CsnArr = csnfromarray(iMovingValues)
    iMovMedian:CsnArr = csnmovmedian(iMoving, 3, -1)
    iMovMin:CsnArr = csnmovmin(iMoving, 3, -1)
    iMovMax:CsnArr = csnmovmax(iMoving, 3, -1)
    assert(csnsize(iMovMedian) == 5 && csnsize(iMovMin) == 5 && csnsize(iMovMax) == 5)
    iMovMedianIn:CsnArr = csncopy(iMoving)
    iMovMinIn:CsnArr = csncopy(iMoving)
    iMovMaxIn:CsnArr = csncopy(iMoving)
    csnmovmedian(iMovMedianIn, 3, -1)
    csnmovmin(iMovMinIn, 3, -1)
    csnmovmax(iMovMaxIn, 3, -1)
    assert(csnsize(iMovMedianIn) == 5 && csnsize(iMovMinIn) == 5 && csnsize(iMovMaxIn) == 5)
endin

instr 3
    iIndex0[] = fillarray(0)
    iIndex1[] = fillarray(1)
    iIndex2[] = fillarray(2)
    iIndex00[] = fillarray(0, 0)
    iIndex01[] = fillarray(0, 1)
    iShape1[] = fillarray(1)
    iShape2[] = fillarray(2)
    iShape22[] = fillarray(2, 2)
    iShape4[] = fillarray(4)
    iAxes10[] = fillarray(1, 0)

    ; Creation, type selection, conversion and metadata.
    iEmpty:CsnArr = csnempty(iShape2)
    iEmpty2D:CsnArr = csnempty(iShape22)
    iZeros:CsnArr = csnzeros(iShape22)
    iOnes:CsnArr = csnones(iShape22)
    iFull:CsnArr = csnfull(iShape22, 7)
    iComplexSeedValues[] = fillarray(1, 3, 9)
    iComplexSeedReal:CsnArr = csnfromarray(iComplexSeedValues)
    iComplexSeed:CsnArr = csntocomplex(iComplexSeedReal)
    iA:Complex = csnget(iComplexSeed, iIndex0)
    iB:Complex = csnget(iComplexSeed, iIndex1)
    iC:Complex = csnget(iComplexSeed, iIndex2)
    iFullComplex:CsnArr = csnfull(iShape2, iA)
    iLike:CsnArr = csnlike(iFull, 3)

    iNative[][] = init(2, 2)
    iNative[0][0] = 1
    iNative[0][1] = 2
    iNative[1][0] = 3
    iNative[1][1] = 4
    iFromReal:CsnArr = csnfromarray(iNative)
    iToReal[][] = csntoarray(iFromReal)

    iNativeComplex:Complex[] = csntoarray(iFullComplex)
    iFromComplex:CsnArr = csnfromarray(iNativeComplex)
    csnset(iFromComplex, iIndex1, iB)
    iToComplex:Complex[] = csntoarray(iFromComplex)
    iFullComplexReal:CsnArr = csntoreal(iFullComplex)
    iFromComplexReal:CsnArr = csnreal(iFromComplex)
    iFullComplexRealValues[] = csntoarray(iFullComplexReal)
    iFromComplexRealValues[] = csntoarray(iFromComplexReal)
    iToComplex0Real = real(iToComplex[0])
    iToComplex0Imag = imag(iToComplex[0])
    iToComplex1Real = real(iToComplex[1])
    iToComplex1Imag = imag(iToComplex[1])

    iCreatedEmptyShape[] = csnshape(iEmpty)
    iCreatedEmpty2DShape[] = csnshape(iEmpty2D)
    assert(csnisempty(iEmpty) == 1 && csnsize(iEmpty) == 0)
    assert(iCreatedEmptyShape[0] == 2)
    assert(csndims(iEmpty2D) == 2 && csnsize(iEmpty2D) == 0)
    assert(iCreatedEmpty2DShape[0] == 2 && iCreatedEmpty2DShape[1] == 2)
    csnpush(iEmpty, 9)
    assert(csnsize(iEmpty) == 1 && csnget(iEmpty, iIndex0) == 9)
    assert(csndims(iFull) == 2 && csnsize(iFull) == 4)
    iFullShape[] = csnshape(iFull)
    assert(iFullShape[0] == 2 && iFullShape[1] == 2)
    assert(csnget(iZeros, iIndex01) == 0 && csnget(iOnes, iIndex01) == 1)
    assert(csnget(iFull, iIndex01) == 7 && csnget(iLike, iIndex01) == 3)
    assert(iToReal[0][0] == 1 && iToReal[1][1] == 4)
    assert(iToComplex0Real == 1 && iToComplex0Imag == 0)
    assert(iToComplex1Real == 3 && iToComplex1Imag == 0)
    assert(iFullComplexRealValues[0] == 1 && iFromComplexRealValues[1] == 3)

    iIdentity:CsnArr = csnidentity(3)
    assert(csnsize(iIdentity) == 9)
    assert(csnget(iIdentity, iIndex01) == 0)
    iIndex11[] = fillarray(1, 1)
    assert(csnget(iIdentity, iIndex11) == 1)

    ; Reshape, flatten, transpose, flip and roll: output and in-place forms.
    iReshaped:CsnArr = csnreshape(iFromReal, iShape4)
    iReshapeIn:CsnArr = csncopy(iFromReal)
    csnreshape(iReshapeIn, iShape4)
    assert(csndims(iReshaped) == 1 && csndims(iReshapeIn) == 1)

    iFlattened:CsnArr = csnflatten(iFromReal)
    iFlattenIn:CsnArr = csncopy(iFromReal)
    csnflatten(iFlattenIn)
    assert(csndims(iFlattened) == 1 && csndims(iFlattenIn) == 1)

    iTranspose:CsnArr = csntranspose(iFromReal)
    iTransposeAxes:CsnArr = csntranspose(iFromReal, iAxes10)
    iTransposeIn:CsnArr = csncopy(iFromReal)
    iTransposeAxesIn:CsnArr = csncopy(iFromReal)
    csntranspose(iTransposeIn)
    csntranspose(iTransposeAxesIn, iAxes10)
    iTransposeValues[][] = csntoarray(iTranspose)
    assert(iTransposeValues[0][1] == 3 && iTransposeValues[1][0] == 2)
    assert(csnget(iTransposeAxes, iIndex01) == 3)
    assert(csnget(iTransposeIn, iIndex01) == 3)
    assert(csnget(iTransposeAxesIn, iIndex01) == 3)

    iTransposeEmpty:CsnArr = csntranspose(iEmpty2D)
    iFlipEmpty:CsnArr = csnflip(iEmpty2D, 1)
    assert(csnsize(iTransposeEmpty) == 0 && csnsize(iFlipEmpty) == 0)

    iFlip:CsnArr = csnflip(iFromReal, 1)
    iFlipIn:CsnArr = csncopy(iFromReal)
    csnflip(iFlipIn, 1)
    assert(csnget(iFlip, iIndex01) == 1 && csnget(iFlipIn, iIndex01) == 1)

    iRoll:CsnArr = csnroll(iFromReal, 1)
    iRollIn:CsnArr = csncopy(iFromReal)
    csnroll(iRollIn, 1)
    iRollAxis:CsnArr = csnroll(iFromReal, 1, 1)
    iRollAxisIn:CsnArr = csncopy(iFromReal)
    csnroll(iRollAxisIn, 1, 1)
    iRollValues[][] = csntoarray(iRoll)
    iRollAxisValues[][] = csntoarray(iRollAxis)
    assert(iRollValues[0][0] == 4 && iRollValues[0][1] == 1)
    assert(iRollAxisValues[0][0] == 2 && iRollAxisValues[0][1] == 1)
    assert(csnget(iRollIn, iIndex01) == 1 && csnget(iRollAxisIn, iIndex01) == 1)

    iRollEmpty:CsnArr = csnroll(iEmpty2D, 1)
    iRollAxisEmpty:CsnArr = csnroll(iEmpty2D, 1, 1)
    csnroll(iEmpty2D, 1)
    csnroll(iEmpty2D, 1, 1)
    assert(csnsize(iEmpty2D) == 0)
    assert(csnsize(iRollEmpty) == 0 && csnsize(iRollAxisEmpty) == 0)

    ; Scalar/complex get and set, take, slices and one-dimensional mutation.
    iSetReal:CsnArr = csncopy(iFromReal)
    csnset(iSetReal, iIndex01, 20)
    assert(csnget(iSetReal, iIndex01) == 20)
    iSetComplex:CsnArr = csncopy(iFullComplex)
    csnset(iSetComplex, iIndex1, iB)
    SetComplex:Complex = csnget(iSetComplex, iIndex1)
    iSetComplexReal = real(SetComplex)
    iSetComplexImag = imag(SetComplex)
    assert(iSetComplexReal == 3 && iSetComplexImag == 0)

    iTaken:CsnArr = csntake(iFromReal, 0, 1)
    iTakenValues[] = csntoarray(iTaken)
    iTakenFlat = csntake(iFromReal, 2)
    TakenComplex:Complex = csntake(iFromComplex, 1)
    iTakenComplexReal = real(TakenComplex)
    iTakenComplexImag = imag(TakenComplex)
    assert(iTakenValues[0] == 3 && iTakenValues[1] == 4)
    assert(iTakenFlat == 3)
    assert(iTakenComplexReal == 3 && iTakenComplexImag == 0)

    iSlice:CsnArr = csngetslice(iFromReal, 1, 0, 2, 1)
    assert(csnsize(iSlice) == 4 && csnget(iSlice, iIndex11) == 4)
    iSliceTarget:CsnArr = csncopy(iFromReal)
    iSliceDataValues[][] = init(2, 1)
    iSliceDataValues[0][0] = 8
    iSliceDataValues[1][0] = 9
    iSliceData:CsnArr = csnfromarray(iSliceDataValues)
    csnsetslice(iSliceTarget, iSliceData, 1, 0, 1, 1)
    assert(csnget(iSliceTarget, iIndex00) == 8)
    iIndex10[] = fillarray(1, 0)
    assert(csnget(iSliceTarget, iIndex10) == 9)

    ; A non-unit step must map destination coordinates back to the selected
    ; source positions, in both the get and set directions.
    iStepValues[] = fillarray(0, 1, 2, 3, 4, 5)
    iStepSource:CsnArr = csnfromarray(iStepValues)
    iStepSlice:CsnArr = csngetslice(iStepSource, 0, 1, 6, 2)
    iStepSliceValues[] = csntoarray(iStepSlice)
    assert(csnsize(iStepSlice) == 3)
    assert(iStepSliceValues[0] == 1 && iStepSliceValues[1] == 3 && iStepSliceValues[2] == 5)

    iStepDataValues[] = fillarray(10, 30, 50)
    iStepData:CsnArr = csnfromarray(iStepDataValues)
    iStepTarget:CsnArr = csncopy(iStepSource)
    csnsetslice(iStepTarget, iStepData, 0, 1, 6, 2)
    iStepTargetValues[] = csntoarray(iStepTarget)
    assert(iStepTargetValues[0] == 0 && iStepTargetValues[1] == 10)
    assert(iStepTargetValues[2] == 2 && iStepTargetValues[3] == 30)
    assert(iStepTargetValues[4] == 4 && iStepTargetValues[5] == 50)

    ; The same helpers must copy both components of complex elements.
    iComplexSlice:CsnArr = csngetslice(iFromComplex, 0, 0, 2, 1)
    ComplexSliceValue:Complex = csnget(iComplexSlice, iIndex1)
    iComplexSliceReal = real(ComplexSliceValue)
    iComplexSliceImag = imag(ComplexSliceValue)
    assert(iComplexSliceReal == 3 && iComplexSliceImag == 0)

    iComplexSliceData:CsnArr = csnfull(iShape1, iC)
    iComplexSliceTarget:CsnArr = csncopy(iFromComplex)
    csnsetslice(iComplexSliceTarget, iComplexSliceData, 0, 1, 2, 1)
    ComplexSetSliceValue:Complex = csnget(iComplexSliceTarget, iIndex1)
    iComplexSetSliceReal = real(ComplexSetSliceValue)
    iComplexSetSliceImag = imag(ComplexSetSliceValue)
    iExpectedComplexSetSliceReal = real(iC)
    iExpectedComplexSetSliceImag = imag(iC)
    assert(iComplexSetSliceReal == iExpectedComplexSetSliceReal)
    assert(iComplexSetSliceImag == iExpectedComplexSetSliceImag)

    iMutReal:CsnArr = csncopy(iReshaped)
    csnpush(iMutReal, 5)
    iPoppedReal = csnpop(iMutReal)
    assert(iPoppedReal == 5)
    csninsert(iMutReal, 9, 1)
    iRemovedReal = csnremove(iMutReal, 1)
    assert(iRemovedReal == 9)
    assert(csnsize(iMutReal) == 4)

    iMutComplex:CsnArr = csncopy(iFromComplex)
    csnpush(iMutComplex, iC)
    PoppedComplex:Complex = csnpop(iMutComplex)
    csninsert(iMutComplex, iC, 1)
    RemovedComplex:Complex = csnremove(iMutComplex, 1)
    iPoppedComplexReal = real(PoppedComplex)
    iPoppedComplexImag = imag(PoppedComplex)
    iRemovedComplexReal = real(RemovedComplex)
    iRemovedComplexImag = imag(RemovedComplex)
    assert(iPoppedComplexReal == 9 && iPoppedComplexImag == 0)
    assert(iRemovedComplexReal == 9 && iRemovedComplexImag == 0)

    iBlockTarget:CsnArr = csncopy(iFromReal)
    iBlockValues[] = fillarray(8, 9)
    iBlock:CsnArr = csnfromarray(iBlockValues)
    csninsert(iBlockTarget, iBlock, 1, 0)
    iRemovedBlock:CsnArr = csnremove(iBlockTarget, 1, 0)
    iBlockShape[] = csnshape(iBlockTarget)
    assert(iBlockShape[0] == 2 && iBlockShape[1] == 3)
    assert(csnsize(iRemovedBlock) == 4)

    iConcatFlat:CsnArr = csnconcat(iReshaped, iReshaped)
    iConcatBlock:CsnArr = csnconcat(iFromReal, iFromReal, 0)
    iConcatBlockShape[] = csnshape(iConcatBlock)
    assert(csnsize(iConcatFlat) == 8)
    assert(iConcatBlockShape[0] == 4 && iConcatBlockShape[1] == 2)

    ; Real and complex padding, with all-axes/one-axis and output/in-place forms.
    iPad:CsnArr = csnpad(iFromReal, 1, 1, -1)
    iPadAxis:CsnArr = csnpad(iFromReal, 1, 0, -2, 1)
    iPadIn:CsnArr = csncopy(iFromReal)
    iPadAxisIn:CsnArr = csncopy(iFromReal)
    csnpad(iPadIn, 1, 1, -1)
    csnpad(iPadAxisIn, 1, 0, -2, 1)
    assert(csnsize(iPad) == 16 && csnsize(iPadIn) == 16)
    assert(csnsize(iPadAxis) == 6 && csnsize(iPadAxisIn) == 6)

    iPadComplex:CsnArr = csnpad(iFromComplex, 1, 1, iC)
    iPadAxisComplex:CsnArr = csnpad(iFromComplex, 1, 0, iC, 0)
    iPadComplexIn:CsnArr = csncopy(iFromComplex)
    iPadAxisComplexIn:CsnArr = csncopy(iFromComplex)
    csnpad(iPadComplexIn, 1, 1, iC)
    csnpad(iPadAxisComplexIn, 1, 0, iC, 0)
    assert(csnsize(iPadComplex) == 4 && csnsize(iPadComplexIn) == 4)
    assert(csnsize(iPadAxisComplex) == 3 && csnsize(iPadAxisComplexIn) == 3)

    ; Explicit release has its own no-output signature.
    iToFree:CsnArr = csnzeros(iShape2)
    csnfree(iToFree)
endin

instr 4
    iIndex0[] = fillarray(0)
    iIndex1[] = fillarray(1)
    iIndex2[] = fillarray(2)

    iValues[] = fillarray(1, 2, 4)
    iTwosValues[] = fillarray(2, 2, 2)
    iDomainValues[] = fillarray(0.25, 0.5, 0.75)
    iLargeValues[] = fillarray(1, 2, 4)
    iValuesArr:CsnArr = csnfromarray(iValues)
    iTwos:CsnArr = csnfromarray(iTwosValues)
    iDomain:CsnArr = csnfromarray(iDomainValues)
    iLarge:CsnArr = csnfromarray(iLargeValues)

    iMatrixValues[][] = init(2, 2)
    iMatrixValues[0][0] = 1
    iMatrixValues[0][1] = 2
    iMatrixValues[1][0] = 3
    iMatrixValues[1][1] = 4
    iMatrix:CsnArr = csnfromarray(iMatrixValues)

    iComplexValues:CsnArr = csntocomplex(iValuesArr)
    iComplexTwos:CsnArr = csntocomplex(iTwos)
    iComplexTwo:Complex = csnget(iComplexTwos, iIndex0)

    ; Scalar reductions, including their complex-output overloads.
    assert(csnsum(iValuesArr) == 7)
    assert(csnprod(iValuesArr) == 8)
    assert(csnsub(iValuesArr) == -5)
    assert(abs(csnmean(iValuesArr) - 7 / 3) < 1e-12)
    assert(csnall(iValuesArr) == 1 && csnany(iValuesArr) == 1)
    assert(abs(csnvar(iValuesArr) - 14 / 9) < 1e-12)
    assert(abs(csnstd(iValuesArr) - sqrt(14 / 9)) < 1e-12)

    iComplexSum:Complex = csnsum(iComplexValues)
    iComplexProd:Complex = csnprod(iComplexValues)
    iComplexSub:Complex = csnsub(iComplexValues)
    iComplexMean:Complex = csnmean(iComplexValues)
    iComplexSumReal = real(iComplexSum)
    iComplexProdReal = real(iComplexProd)
    iComplexSubReal = real(iComplexSub)
    iComplexMeanReal = real(iComplexMean)
    assert(iComplexSumReal == 7 && iComplexProdReal == 8)
    assert(iComplexSubReal == -5 && abs(iComplexMeanReal - 7 / 3) < 1e-12)

    ; Axis reductions.
    iSumAxis:CsnArr = csnsum(iMatrix, 0)
    iProdAxis:CsnArr = csnprod(iMatrix, 0)
    iSubAxis:CsnArr = csnsub(iMatrix, 0)
    iMeanAxis:CsnArr = csnmean(iMatrix, 0)
    iAnyAxis:CsnArr = csnany(iMatrix, 1)
    iAllAxis:CsnArr = csnall(iMatrix, 1)
    iStdAxis:CsnArr = csnstd(iMatrix, 0)
    iVarAxis:CsnArr = csnvar(iMatrix, 0)
    iSumAxisValues[] = csntoarray(iSumAxis)
    iProdAxisValues[] = csntoarray(iProdAxis)
    iSubAxisValues[] = csntoarray(iSubAxis)
    iMeanAxisValues[] = csntoarray(iMeanAxis)
    iStdAxisValues[] = csntoarray(iStdAxis)
    iVarAxisValues[] = csntoarray(iVarAxis)
    assert(iSumAxisValues[0] == 4 && iSumAxisValues[1] == 6)
    assert(iProdAxisValues[0] == 3 && iProdAxisValues[1] == 8)
    assert(iSubAxisValues[0] == -2 && iSubAxisValues[1] == -2)
    assert(iMeanAxisValues[0] == 2 && iMeanAxisValues[1] == 3)
    assert(csnsize(iAnyAxis) == 2 && csnsize(iAllAxis) == 2)
    assert(iStdAxisValues[0] == 1 && iStdAxisValues[1] == 1)
    assert(iVarAxisValues[0] == 1 && iVarAxisValues[1] == 1)

    ; Binary arithmetic: HH, HS, SH and complex-scalar dispatch.
    iAddHH:CsnArr = csnadd(iValuesArr, iTwos)
    iAddHS:CsnArr = csnadd(iValuesArr, 2)
    iAddHC:CsnArr = csnadd(iComplexValues, iComplexTwo)

    iSubtractHH:CsnArr = csnsubtract(iValuesArr, iTwos)
    iSubtractHS:CsnArr = csnsubtract(iValuesArr, 2)
    iSubtractHC:CsnArr = csnsubtract(iComplexValues, iComplexTwo)
    iSubtractSH:CsnArr = csnsubtract(8, iValuesArr)
    iSubtractCH:CsnArr = csnsubtract(iComplexTwo, iComplexValues)

    iMulHH:CsnArr = csnmul(iValuesArr, iTwos)
    iMulHS:CsnArr = csnmul(iValuesArr, 2)
    iMulHC:CsnArr = csnmul(iComplexValues, iComplexTwo)

    iDivHH:CsnArr = csndiv(iValuesArr, iTwos)
    iDivHS:CsnArr = csndiv(iValuesArr, 2)
    iDivSH:CsnArr = csndiv(8, iValuesArr)
    iDivHC:CsnArr = csndiv(iComplexValues, iComplexTwo)
    iDivCH:CsnArr = csndiv(iComplexTwo, iComplexValues)

    iPowHH:CsnArr = csnpow(iValuesArr, iTwos)
    iPowHS:CsnArr = csnpow(iValuesArr, 2)
    iPowSH:CsnArr = csnpow(2, iValuesArr)
    iPowHC:CsnArr = csnpow(iComplexValues, iComplexTwo)
    iPowCH:CsnArr = csnpow(iComplexTwo, iComplexValues)

    iLogHH:CsnArr = csnlog(iValuesArr, iTwos)
    iLogHS:CsnArr = csnlog(iValuesArr, 2)
    iLogSH:CsnArr = csnlog(8, iTwos)
    iLogHC:CsnArr = csnlog(iComplexValues, iComplexTwo)
    iLogCH:CsnArr = csnlog(iComplexTwo, iComplexTwos)

    iAddHHValues[] = csntoarray(iAddHH)
    iSubtractSHValues[] = csntoarray(iSubtractSH)
    iMulHSValues[] = csntoarray(iMulHS)
    iDivSHValues[] = csntoarray(iDivSH)
    iPowHSValues[] = csntoarray(iPowHS)
    assert(iAddHHValues[0] == 3 && iAddHHValues[2] == 6)
    assert(iSubtractSHValues[0] == 7 && iSubtractSHValues[2] == 4)
    assert(iMulHSValues[0] == 2 && iMulHSValues[2] == 8)
    assert(iDivSHValues[0] == 8 && iDivSHValues[2] == 2)
    assert(iPowHSValues[0] == 1 && iPowHSValues[2] == 16)
    assert(csnsize(iAddHS) == 3 && csntype(iAddHC) == 1)
    assert(csnsize(iSubtractHH) == 3 && csntype(iSubtractHC) == 1 && csntype(iSubtractCH) == 1)
    assert(csnsize(iMulHH) == 3 && csntype(iMulHC) == 1)
    assert(csnsize(iDivHH) == 3 && csntype(iDivHC) == 1 && csntype(iDivCH) == 1)
    assert(csnsize(iPowHH) == 3 && csntype(iPowHC) == 1 && csntype(iPowCH) == 1)
    assert(csnsize(iLogHH) == 3 && csnsize(iLogHS) == 3 && csnsize(iLogSH) == 3)
    assert(csntype(iLogHC) == 1 && csntype(iLogCH) == 1)

    /* csnhypot is real-only, and its operands are Pythagorean triples so the
       results come out exact rather than needing a tolerance. */
    iHypotAValues[] = fillarray(3, 6, 8)
    iHypotBValues[] = fillarray(4, 8, 15)
    iHypotA:CsnArr = csnfromarray(iHypotAValues)
    iHypotB:CsnArr = csnfromarray(iHypotBValues)

    iHypotHH:CsnArr = csnhypot(iHypotA, iHypotB)
    iHypotHS:CsnArr = csnhypot(iHypotA, 4)
    iHypotHHValues[] = csntoarray(iHypotHH)
    iHypotHSValues[] = csntoarray(iHypotHS)
    assert(iHypotHHValues[0] == 5 && iHypotHHValues[1] == 10 && iHypotHHValues[2] == 17)
    assert(iHypotHSValues[0] == 5)
    assert(csnsize(iHypotHS) == 3)

    ; Every unary numerical signature, on inputs inside its real domain.
    iAbs:CsnArr = csnabs(iValuesArr)
    iExp:CsnArr = csnexp(iDomain)
    iSqrt:CsnArr = csnsqrt(iValuesArr)
    iCbrt:CsnArr = csncbrt(iValuesArr)
    iSin:CsnArr = csnsin(iDomain)
    iCos:CsnArr = csncos(iDomain)
    iTan:CsnArr = csntan(iDomain)
    iAsin:CsnArr = csnasin(iDomain)
    iAcos:CsnArr = csnacos(iDomain)
    iAtan:CsnArr = csnatan(iDomain)
    iSinh:CsnArr = csnsinh(iDomain)
    iCosh:CsnArr = csncosh(iDomain)
    iTanh:CsnArr = csntanh(iDomain)
    iAsinh:CsnArr = csnasinh(iDomain)
    iAcosh:CsnArr = csnacosh(iLarge)
    iAtanh:CsnArr = csnatanh(iDomain)
    iSign:CsnArr = csnsign(iValuesArr)
    assert(csnsize(iAbs) + csnsize(iExp) + csnsize(iSqrt) + csnsize(iCbrt) == 12)
    assert(csnsize(iSin) + csnsize(iCos) + csnsize(iTan) + csnsize(iAsin) == 12)
    assert(csnsize(iAcos) + csnsize(iAtan) + csnsize(iSinh) + csnsize(iCosh) == 12)
    assert(csnsize(iTanh) + csnsize(iAsinh) + csnsize(iAcosh) + csnsize(iAtanh) + csnsize(iSign) == 15)

    ; Products, norms and vector distances: array/scalar/complex scalar forms.
    iDot:CsnArr = csndot(iMatrix, iMatrix)
    iDotScalar = csndot(iValuesArr, iTwos)
    iDotComplex:Complex = csndot(iComplexValues, iComplexTwos)
    iDotComplexReal = real(iDotComplex)
    iInner:CsnArr = csninner(iMatrix, iMatrix)
    iInnerScalar = csninner(iValuesArr, iTwos)
    iInnerComplex:Complex = csninner(iComplexValues, iComplexTwos)
    iInnerComplexReal = real(iInnerComplex)
    iOuter:CsnArr = csnouter(iValuesArr, iTwos)
    assert(csnsize(iDot) == 4 && iDotScalar == 14 && iDotComplexReal == 14)
    assert(csnsize(iInner) == 4 && iInnerScalar == 14 && iInnerComplexReal == 14)
    assert(csnsize(iOuter) == 9)

    iNormAxis:CsnArr = csnnorm(iMatrix, 1, 2)
    iNormScalar = csnnorm(iValuesArr, 2)
    iNormalized:CsnArr = csnnormalize(iValuesArr, -1, 2)
    iNormalizeIn:CsnArr = csncopy(iValuesArr)
    csnnormalize(iNormalizeIn, -1, 2)
    assert(csnsize(iNormAxis) == 2 && abs(iNormScalar - sqrt(21)) < 1e-12)
    assert(csnsize(iNormalized) == 3 && csnsize(iNormalizeIn) == 3)

    iPairDist:CsnArr = csnpairdist(iMatrix, iMatrix)
    iDistance = csndist(iValuesArr, iTwos, 2)
    iAngleDistance = csnangledist(iValuesArr, iTwos)
    iReflected:CsnArr = csnreflect(iValuesArr, iTwos)
    assert(csnsize(iPairDist) > 0 && iDistance >= 0 && iAngleDistance >= 0)
    assert(csnsize(iReflected) == 3)

    iDiff:CsnArr = csndiff(iValuesArr, -1)
    iCumSum:CsnArr = csncumsum(iValuesArr, -1)
    iCumProd:CsnArr = csncumprod(iValuesArr, -1)
    iDiffValues[] = csntoarray(iDiff)
    iCumSumValues[] = csntoarray(iCumSum)
    iCumProdValues[] = csntoarray(iCumProd)
    assert(iDiffValues[0] == 1 && iDiffValues[1] == 2)
    assert(iCumSumValues[0] == 1 && iCumSumValues[2] == 7)
    assert(iCumProdValues[0] == 1 && iCumProdValues[2] == 8)

    iMatmul:CsnArr = csnmatmul(iMatrix, iMatrix)
    iMatmulScalar = csnmatmul(iValuesArr, iTwos)
    iTrace = csntrace(iMatrix)
    iComplexMatrix:CsnArr = csntocomplex(iMatrix)
    iTraceComplex:Complex = csntrace(iComplexMatrix)
    iTraceComplexReal = real(iTraceComplex)
    iDiag:CsnArr = csndiag(iValuesArr)
    assert(csnsize(iMatmul) == 4 && iMatmulScalar == 14)
    assert(iTrace == 5 && iTraceComplexReal == 5 && csnsize(iDiag) == 9)

    ; Moving mean/std/variance, output and in-place forms.
    iMovMean:CsnArr = csnmovmean(iValuesArr, 2, -1)
    iMovStd:CsnArr = csnmovstd(iValuesArr, 2, -1)
    iMovVar:CsnArr = csnmovvar(iValuesArr, 2, -1)
    iMovMeanIn:CsnArr = csncopy(iValuesArr)
    iMovStdIn:CsnArr = csncopy(iValuesArr)
    iMovVarIn:CsnArr = csncopy(iValuesArr)
    csnmovmean(iMovMeanIn, 2, -1)
    csnmovstd(iMovStdIn, 2, -1)
    csnmovvar(iMovVarIn, 2, -1)
    assert(csnsize(iMovMean) == 3 && csnsize(iMovStd) == 3 && csnsize(iMovVar) == 3)
    assert(csnsize(iMovMeanIn) == 3 && csnsize(iMovStdIn) == 3 && csnsize(iMovVarIn) == 3)
endin

instr 5
    iAnglesValues[] = fillarray(-4, 0, 4, 7)
    iAngles:CsnArr = csnfromarray(iAnglesValues)
    iComplexAngles:CsnArr = csntocomplex(iAngles)

    iRealPart:CsnArr = csnreal(iComplexAngles)
    iImagPart:CsnArr = csnimag(iComplexAngles)
    iToReal:CsnArr = csntoreal(iComplexAngles)
    iConjugate:CsnArr = csnconj(iComplexAngles)
    iPhase:CsnArr = csnangle(iComplexAngles)
    assert(csnsize(iRealPart) == 4 && csnsize(iImagPart) == 4 && csnsize(iToReal) == 4)
    assert(csntype(iConjugate) == 1 && csnsize(iPhase) == 4)

    iWrapped:CsnArr = csnwrap(iAngles, 6.283185307179586)
    iWrapIn:CsnArr = csncopy(iAngles)
    csnwrap(iWrapIn, 6.283185307179586)
    iWrappedValues[] = csntoarray(iWrapped)
    iWrapInValues[] = csntoarray(iWrapIn)
    assert(iWrappedValues[0] > -3.141592653589793 && iWrappedValues[0] <= 3.141592653589793)
    assert(iWrapInValues[3] > -3.141592653589793 && iWrapInValues[3] <= 3.141592653589793)

    iUnwrapInputValues[] = fillarray(0, 3, -3, -2)
    iUnwrapInput:CsnArr = csnfromarray(iUnwrapInputValues)
    iUnwrapped:CsnArr = csnunwrap(iUnwrapInput, 6.283185307179586, 3.141592653589793, -1)
    iUnwrapIn:CsnArr = csncopy(iUnwrapInput)
    csnunwrap(iUnwrapIn, 6.283185307179586, 3.141592653589793, -1)
    iUnwrappedValues[] = csntoarray(iUnwrapped)
    iUnwrapInValues[] = csntoarray(iUnwrapIn)
    assert(abs(iUnwrappedValues[2] - 3.283185307179586) < 1e-12)
    assert(abs(iUnwrapInValues[3] - 4.283185307179586) < 1e-12)
endin

instr 6
    ; ------------------------------------------------------------------
    ; Angle conversion, both the copying and the in-place overload. The
    ; in-place form has to leave the source it was handed converted, and
    ; the copying form has to leave it alone.
    ; ------------------------------------------------------------------
    iDegreeValues[] = fillarray(0, 90, 180, 270, 360, -45)
    iDegrees:CsnArr = csnfromarray(iDegreeValues)

    iRadians:CsnArr = csndegtorad(iDegrees)
    iRadianValues[] = csntoarray(iRadians)
    assert(abs(iRadianValues[1] - 1.5707963267948966) < 1e-12)
    assert(abs(iRadianValues[3] - 4.71238898038469) < 1e-12)
    assert(abs(iRadianValues[5] + 0.7853981633974483) < 1e-12)

    iBackToDegrees:CsnArr = csnradtodeg(iRadians)
    iBackValues[] = csntoarray(iBackToDegrees)
    assert(abs(iBackValues[1] - 90) < 1e-10 && abs(iBackValues[4] - 360) < 1e-10)

    iSourceAfterAngle[] = csntoarray(iDegrees)
    assert(iSourceAfterAngle[1] == 90 && iSourceAfterAngle[4] == 360)

    iAngleIn:CsnArr = csncopy(iDegrees)
    csndegtorad(iAngleIn)
    iAngleInValues[] = csntoarray(iAngleIn)
    assert(abs(iAngleInValues[2] - 3.141592653589793) < 1e-12)
    csnradtodeg(iAngleIn)
    iAngleBackValues[] = csntoarray(iAngleIn)
    assert(abs(iAngleBackValues[2] - 180) < 1e-10)

    ; ------------------------------------------------------------------
    ; Window functions, against NumPy's values for the same length. Kaiser
    ; needs a looser bound than the others: its modified Bessel term is a
    ; polynomial approximation good to roughly 1e-7, not to the last bit.
    ; ------------------------------------------------------------------
    iHanning:CsnArr = csnhanning(8)
    iHamming:CsnArr = csnhamming(8)
    iBartlett:CsnArr = csnbartlett(8)
    iBlackman:CsnArr = csnblackman(8)
    iKaiser:CsnArr = csnkaiser(8, 4)

    iHanningValues[] = csntoarray(iHanning)
    iHammingValues[] = csntoarray(iHamming)
    iBartlettValues[] = csntoarray(iBartlett)
    iBlackmanValues[] = csntoarray(iBlackman)
    iKaiserValues[] = csntoarray(iKaiser)

    assert(abs(iHanningValues[1] - 0.188255099070633) < 1e-12 && abs(iHanningValues[5] - 0.611260466978157) < 1e-12)
    assert(abs(iHammingValues[1] - 0.253194691144983) < 1e-12 && abs(iHammingValues[5] - 0.642359629619905) < 1e-12)
    assert(abs(iBartlettValues[1] - 0.285714285714286) < 1e-12 && abs(iBartlettValues[5] - 0.571428571428571) < 1e-12)
    assert(abs(iBlackmanValues[1] - 0.090453424354128) < 1e-12 && abs(iBlackmanValues[5] - 0.459182957545964) < 1e-12)
    assert(abs(iKaiserValues[1] - 0.367669606379961) < 1e-7 && abs(iKaiserValues[5] - 0.718780790355119) < 1e-7)

    /* A window of one is the degenerate case: the usual n/(size - 1) ramp
       divides by zero there, so every window has to answer 1 instead. A
       window of zero reserves nothing at all. */
    iSinglePoint:CsnArr = csnkaiser(1, 4)
    iSinglePointValues[] = csntoarray(iSinglePoint)
    assert(iSinglePointValues[0] == 1)
    iNoPoints:CsnArr = csnblackman(0)
    assert(csnsize(iNoPoints) == 0)

    ; ------------------------------------------------------------------
    ; divmod publishes two handles at once, and its quotient follows the
    ; floor convention rather than truncation: -7 divided by 3 is -3 with a
    ; remainder of 2, not -2 with a remainder of -1.
    ; ------------------------------------------------------------------
    iDividendValues[] = fillarray(7, -7, 13, 5)
    iDivisorValues[] = fillarray(3, 3, 5, 5)
    iDividend:CsnArr = csnfromarray(iDividendValues)
    iDivisor:CsnArr = csnfromarray(iDivisorValues)

    iQuotient:CsnArr, iRemainder:CsnArr csndivmod iDividend, iDivisor
    iQuotientValues[] = csntoarray(iQuotient)
    iRemainderValues[] = csntoarray(iRemainder)
    assert(iQuotientValues[0] == 2 && iQuotientValues[1] == -3 && iQuotientValues[2] == 2 && iQuotientValues[3] == 1)
    assert(iRemainderValues[0] == 1 && iRemainderValues[1] == 2 && iRemainderValues[2] == 3 && iRemainderValues[3] == 0)

    iScalarQuotient:CsnArr, iScalarRemainder:CsnArr csndivmod iDividend, 3
    iScalarQuotientValues[] = csntoarray(iScalarQuotient)
    iScalarRemainderValues[] = csntoarray(iScalarRemainder)
    assert(iScalarQuotientValues[1] == -3 && iScalarQuotientValues[2] == 4)
    assert(iScalarRemainderValues[1] == 2 && iScalarRemainderValues[2] == 1)

    /* The scalar on the left divides each element, so the roles of the two
       operands swap: the remainder is taken from the scalar, not the array. */
    iLeftQuotient:CsnArr, iLeftRemainder:CsnArr csndivmod 3, iDividend
    iLeftQuotientValues[] = csntoarray(iLeftQuotient)
    iLeftRemainderValues[] = csntoarray(iLeftRemainder)
    assert(iLeftQuotientValues[0] == 0 && iLeftQuotientValues[1] == -1)
    assert(iLeftRemainderValues[0] == 3 && iLeftRemainderValues[1] == -4)

    /* Broadcasting reaches divmod through the same path as the other binary
       operations: a 2x3 grid against a length-3 row. */
    iGridValues[] = fillarray(1, 2, 3, 4, 5, 6)
    iGridShape[] = fillarray(2, 3)
    iGridFlat:CsnArr = csnfromarray(iGridValues)
    iGrid:CsnArr = csnreshape(iGridFlat, iGridShape)
    iRowValues[] = fillarray(2, 3, 4)
    iRow:CsnArr = csnfromarray(iRowValues)

    iGridQuotient:CsnArr, iGridRemainder:CsnArr csndivmod iGrid, iRow
    iCell00[] = fillarray(0, 0)
    iCell11[] = fillarray(1, 1)
    iGridQuotient00 = csnget(iGridQuotient, iCell00)
    iGridRemainder00 = csnget(iGridRemainder, iCell00)
    iGridQuotient11 = csnget(iGridQuotient, iCell11)
    iGridRemainder11 = csnget(iGridRemainder, iCell11)
    assert(iGridQuotient00 == 0 && iGridRemainder00 == 1)
    assert(iGridQuotient11 == 1 && iGridRemainder11 == 2)
    assert(csnsize(iGridQuotient) == 6 && csnsize(iGridRemainder) == 6)
endin
</CsInstruments>

<CsScore>
i 1 0 0.01
i 2 0.02 0.01
i 3 0.04 0.01
i 4 0.06 0.01
i 5 0.08 0.01
i 6 0.10 0.01
e
</CsScore>

; This inventory names the OENTRY selected by each public, suffix-free call
; above.  tests/check_itime_signatures.cmake keeps it exactly synchronized
; with src/csnum.c and rejects dotted opcode calls in executable Csound code.
; @covers-begin
; csnrand csnarange csnlinspace csnlogspace csngeomspace csnclip csnclip.in csnargwhere
; csnargnonzero csnargisnan csnargunique csnunique csngt csnlt csnne csnge
; csnle csneq csncnteq csncntnz csncntnan csnmin csnmax csnmedian
; csnmin.ax csnmax.ax csnmedian.ax csnargmin csnargmax csnfloor csnceil csnround
; csnproject csnreject csncross csngrad csnmovmedian csnmovmedian.in csnmovmin csnmovmin.in
; csnmovmax csnmovmax.in csnsort csnsort.in csnargsort csnpercentile csnpercentile.ax csnquantile
; csnquantile.ax csnlogicand.hh csnlogicor.hh csnlogicand.hs csnlogicor.hs csnlogicand.sh csnlogicor.sh csnlogicnot
; csnempty csnzeros csnones csnfull csnfull.c csnlike csnfromarray csnfromarray.c
; csntoarray csntoarray.c csnfree csndims csnsize csnisempty csnshape csnidentity
; csnreshape csnreshape.in csnflatten csnflatten.in csntranspose csntranspose.ax csntranspose.in csntranspose.ax.in
; csnflip csnflip.in csnroll csnroll.in csnroll.ax csnroll.ax.in csnget csnget.c
; csnset csnset.c csntake csntake.flat csntake.flat.c csngetslice csnsetslice csnpush
; csnpush.c csnpop csnpop.c csninsert.flat csninsert.flat.c csnremove.flat csnremove.flat.c csninsert.block
; csnremove.block csnconcat.flat csnconcat.block csnpad csnpad.ax csnpad.in csnpad.ax.in csnpad.c
; csnpad.ax.c csnpad.in.c csnpad.ax.in.c csnsum csnprod csnsub csnmean csnall
; csnany csnstd csnvar csnsum.c csnprod.c csnsub.c csnmean.c csnsum.ax
; csnprod.ax csnsub.ax csnmean.ax csnany.ax csnall.ax csnstd.ax csnvar.ax csnadd
; csnadd.hs csnadd.hs.c csnsubtract.hh csnsubtract.hs csnsubtract.hs.c csnsubtract.sh csnsubtract.sh.c csnmul.hh
; csnmul.hs csnmul.hs.c csndiv.hh csndiv.hs csndiv.sh csndiv.hs.c csndiv.sh.c csnpow.hh
; csnpow.hs csnpow.sh csnpow.hs.c csnpow.sh.c csnlog.hh csnlog.hs csnlog.sh csnlog.hs.c
; csnlog.sh.c csnabs csnexp csnsqrt csncbrt csnsin csncos csntan
; csnasin csnacos csnatan csnsinh csncosh csntanh csnasinh csnacosh
; csnatanh csnsign csndot csndot.s csndot.s.c csninner csninner.s csninner.s.c
; csnouter csnnorm csnnorm.s csnnormalize csnnormalize.in csnpairdist csndist csnangledist
; csnreflect csndiff csncumsum csncumprod csnmatmul csnmatmul.s csntrace csntrace.c
; csndiag csnmovmean csnmovmean.in csnmovstd csnmovstd.in csnmovvar csnmovvar.in csnreal
; csnimag csntoreal csntocomplex csnconj csnangle csnwrap csnwrap.in csnunwrap
; csnunwrap.in csntype csncopy csnreverse csnreverse.in csnseed csnhypot csnhypot.hs
; csndegtorad csndegtorad.in csnradtodeg csnradtodeg.in csnhanning csnhamming csnbartlett csnblackman
; csnkaiser csndivmod.hh csndivmod.hs csndivmod.sh
; @covers-end
</CsoundSynthesizer>
