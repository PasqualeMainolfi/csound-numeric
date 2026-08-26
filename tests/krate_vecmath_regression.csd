<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>

<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

/* k-rate elementwise math, sorts, vector products and the axis reductions.
   Every opcode here republishes an existing slot on each triggered pass, so the
   cases that matter are: the value is actually computed (not left as the init
   copy of the source), the logical size is the full array, the scratch-backed
   sorts work on both the flat and the axis path, and the trigger gates. */

giValues[] = array(4, 1, 3, 2)
giA[] = array(3, 4, 0)
giB[] = array(1, 0, 0)
giYAxis[] = array(0, 1, 0)
giSigned[] = array(-2.5, 2.5, -1, 1)
giShape23[] = array(2, 3)
giShape32[] = array(3, 2)
giFlat6[] = array(3, 4, 0, 1, 0, 0)
giMatA[] = array(1, 2, 3, 4, 5, 6)
giMatB[] = array(7, 8, 9, 10, 11, 12)
giDiag[] = array(1, 1, 0)
giSeq[] = array(1, 5, 2, 4, 3)
giShape22[] = array(2, 2)
giQuad[] = array(1, 2, 3, 4)

V@global:CsnArr = csnfromarray(giValues)
A@global:CsnArr = csnfromarray(giA)
B@global:CsnArr = csnfromarray(giB)
YAxis@global:CsnArr = csnfromarray(giYAxis)
Signed@global:CsnArr = csnfromarray(giSigned)
Flat6@global:CsnArr = csnfromarray(giFlat6)
Mat@global:CsnArr = csnreshape(Flat6, giShape23)
FlatMatA@global:CsnArr = csnfromarray(giMatA)
FlatMatB@global:CsnArr = csnfromarray(giMatB)
MatA@global:CsnArr = csnreshape(FlatMatA, giShape23)
MatB@global:CsnArr = csnreshape(FlatMatB, giShape32)
Diag@global:CsnArr = csnfromarray(giDiag)
Product@global:CsnArr = csnmatmul(MatA, MatB)
Seq@global:CsnArr = csnfromarray(giSeq)
Quad@global:CsnArr = csnfromarray(giQuad)
FlatQuad@global:CsnArr = csnfromarray(giQuad)
Square@global:CsnArr = csnreshape(FlatQuad, giShape22)
ComplexQuad@global:CsnArr = csntocomplex(FlatQuad)
ComplexSquare@global:CsnArr = csnreshape(ComplexQuad, giShape22)

MovMedian@global:CsnArr = csnmovmedian(Seq, 3)
MovMin@global:CsnArr = csnmovmin(Seq, 3)
MovMax@global:CsnArr = csnmovmax(Seq, 3)
MovMean@global:CsnArr = csnmovmean(Seq, 3)
MovVar@global:CsnArr = csnmovvar(Seq, 3)
Spread@global:CsnArr = csndiag(Quad)
Extracted@global:CsnArr = csndiag(Square)
Sliced@global:CsnArr = csngetslice(Quad, 0, 0, 2, 1)

/* In-place reverse reached through the family name, and the axis reductions
   whose axis is mandatory. */
Reversed@global:CsnArr = csnfromarray(giQuad)
GatedReverse@global:CsnArr = csnreverse(Quad)
GatedReverseComplex@global:CsnArr = csnreverse(ComplexQuad)
PercAxis@global:CsnArr = csnpercentile(Square, 50, 0)
QuantAxis@global:CsnArr = csnquantile(Square, 0.5, 1)

/* Complex lane ops change the element type of the result, so their k form has
   to republish the slot with the destination's type, not the source's. */
RealPart@global:CsnArr = csnreal(ComplexQuad)
ImagPart@global:CsnArr = csnimag(ComplexQuad)
Conjugate@global:CsnArr = csnconj(ComplexQuad)
Promoted@global:CsnArr = csntocomplex(Quad)

/* In-place moving filter, never triggered: must keep the original sequence. */
MovGated@global:CsnArr = csnfromarray(giSeq)

Sorted@global:CsnArr = csnsort(V, -1)
Argsorted@global:CsnArr = csnargsort(V, -1)
SortedAxis@global:CsnArr = csnsort(Mat, 1)
Projected@global:CsnArr = csnproject(A, B)
Rejected@global:CsnArr = csnreject(A, B)
Crossed@global:CsnArr = csncross(B, YAxis)
Reflected@global:CsnArr = csnreflect(A, B)
PairDist@global:CsnArr = csnpairdist(A, B)
Outer@global:CsnArr = csnouter(B, YAxis)
Grad@global:CsnArr = csngrad(V, -1)
Diff@global:CsnArr = csndiff(V, -1)
CumSum@global:CsnArr = csncumsum(V, -1)
CumProd@global:CsnArr = csncumprod(V, -1)
Absolute@global:CsnArr = csnabs(Signed)
Floored@global:CsnArr = csnfloor(Signed)
Ceiled@global:CsnArr = csnceil(Signed)
Signs@global:CsnArr = csnsign(Signed)
NotV@global:CsnArr = csnlogicnot(V)
Normalized@global:CsnArr = csnnormalize(A, -1, 2)
NormAxis@global:CsnArr = csnnorm(Mat, 1, 2)

/* In-place overloads: no output handle, the source is rewritten. */
SortInPlace@global:CsnArr = csnfromarray(giValues)
NormInPlace@global:CsnArr = csnfromarray(giA)

gkTraceReal init 0
gkTraceComplexRe init 0
gkMatDot init 0
gkAngleRight init 0
gkAngleHalf init 0
gkNorm init 0
gkDot init 0
gkInner init 0
gkDist init 0

/* Gated off for the whole note. */
Frozen@global:CsnArr = csnabs(Signed)

instr 1
    kTrig init 1
    kOff init 0
    kAllAxes init -1
    kAxis1 init 1
    /* Plain assignments on purpose: order and window still read 0 during the
       init pass, and the k init has to tolerate that instead of rejecting it. */
    kOrder = 2

    Sorted = csnsort(V, kAllAxes, kTrig)
    Argsorted = csnargsort(V, kAllAxes, kTrig)
    SortedAxis = csnsort(Mat, kAxis1, kTrig)
    Projected = csnproject(A, B, kTrig)
    Rejected = csnreject(A, B, kTrig)
    Crossed = csncross(B, YAxis, kTrig)
    Reflected = csnreflect(A, B, kTrig)
    PairDist = csnpairdist(A, B, kTrig)
    Outer = csnouter(B, YAxis, kTrig)
    Grad = csngrad(V, kAllAxes, kTrig)
    Diff = csndiff(V, kAllAxes, kTrig)
    CumSum = csncumsum(V, kAllAxes, kTrig)
    CumProd = csncumprod(V, kAllAxes, kTrig)
    Absolute = csnabs(Signed, kTrig)
    Floored = csnfloor(Signed, kTrig)
    Ceiled = csnceil(Signed, kTrig)
    Signs = csnsign(Signed, kTrig)
    NotV = csnlogicnot(V, kTrig)
    Normalized = csnnormalize(A, kAllAxes, kOrder, kTrig)
    NormAxis = csnnorm(Mat, kAxis1, kOrder, kTrig)
    Product = csnmatmul(MatA, MatB, kTrig)

    /* Deliberately a plain k assignment, not `init`: the window still reads 0
       during the init pass, which the k init must tolerate. */
    kWindow = 3
    MovMedian = csnmovmedian(Seq, kWindow, kAllAxes, kTrig)
    MovMin = csnmovmin(Seq, kWindow, kAllAxes, kTrig)
    MovMax = csnmovmax(Seq, kWindow, kAllAxes, kTrig)
    MovMean = csnmovmean(Seq, kWindow, kAllAxes, kTrig)
    MovVar = csnmovvar(Seq, kWindow, kAllAxes, kTrig)
    csnmovmedian(MovGated, kWindow, kAllAxes, kOff)

    kSliceAxis = 0
    kSliceStart = 1
    kSliceStop = 3
    kSliceStep = 1
    Sliced = csngetslice(Quad, kSliceAxis, kSliceStart, kSliceStop, kSliceStep)

    RealPart = csnreal(ComplexQuad, kTrig)
    ImagPart = csnimag(ComplexQuad, kTrig)
    Conjugate = csnconj(ComplexQuad, kTrig)
    Promoted = csntocomplex(Quad, kTrig)

    csnreverse(Reversed, kTrig)
    GatedReverse = csnreverse(Quad, kOff)
    GatedReverseComplex = csnreverse(ComplexQuad, kOff)
    kPercent = 50
    kQuant = 0.5
    kAxis0 = 0
    PercAxis = csnpercentile(Square, kPercent, kAxis0, kTrig)
    QuantAxis = csnquantile(Square, kQuant, kAxis1, kTrig)

    Spread = csndiag(Quad, kTrig)
    Extracted = csndiag(Square, kTrig)

    csnsort(SortInPlace, kAllAxes, kTrig)
    csnnormalize(NormInPlace, kAllAxes, kOrder, kTrig)

    Frozen = csnabs(Signed, kOff)
endin

instr 2
    i0[] = array(0)
    i1[] = array(1)
    i2[] = array(2)
    i3[] = array(3)
    i01[] = array(0, 1)
    i12[] = array(1, 2)

    /* Sorts: flat, the index permutation, and one row of the axis form. */
    iSorted0 = csnget(Sorted, i0)
    iSorted3 = csnget(Sorted, i3)
    iArg0 = csnget(Argsorted, i0)
    iSortAxis = csnget(SortedAxis, i12)
    assert(iSorted0 == 1 && iSorted3 == 4)
    assert(iArg0 == 1)
    /* Row 1 of the matrix is [1, 0, 0]; sorted along axis 1 it ends [0, 0, 1]. */
    assert(iSortAxis == 1)
    assert(csnsize(Sorted) == 4 && csnsize(SortedAxis) == 6)

    /* Vector products: a onto b, the remainder, and the right-handed cross. */
    iProj0 = csnget(Projected, i0)
    iProj1 = csnget(Projected, i1)
    iRej1 = csnget(Rejected, i1)
    iCross2 = csnget(Crossed, i2)
    iRefl1 = csnget(Reflected, i1)
    iPair1 = csnget(PairDist, i1)
    assert(iProj0 == 3 && iProj1 == 0)
    assert(iRej1 == 4)
    assert(iCross2 == 1)
    assert(iRefl1 == 4)
    assert(iPair1 == 4)
    assert(csnsize(Outer) == 9)

    /* Axis walks over [4, 1, 3, 2]. */
    iGrad0 = csnget(Grad, i0)
    iDiff0 = csnget(Diff, i0)
    iCumSum1 = csnget(CumSum, i1)
    iCumProd2 = csnget(CumProd, i2)
    assert(iGrad0 == -3 && iDiff0 == -3)
    assert(iCumSum1 == 5 && iCumProd2 == 12)

    /* Elementwise math over [-2.5, 2.5, -1, 1]. */
    iAbs0 = csnget(Absolute, i0)
    iFloor0 = csnget(Floored, i0)
    iCeil0 = csnget(Ceiled, i0)
    iSign0 = csnget(Signs, i0)
    iNot0 = csnget(NotV, i0)
    assert(iAbs0 == 2.5 && iFloor0 == -3 && iCeil0 == -2)
    assert(iSign0 == -1 && iNot0 == 0)
    assert(csnsize(Absolute) == 4)

    /* Norms: unit vector along a, and the per-row norms of the matrix. */
    iNorm0 = csnget(Normalized, i0)
    iNorm1 = csnget(Normalized, i1)
    iNormAxis0 = csnget(NormAxis, i0)
    assert(iNorm0 == 0.6 && iNorm1 == 0.8)
    assert(iNormAxis0 == 5)

    /* In-place forms rewrite their own source. */
    iSortIn0 = csnget(SortInPlace, i0)
    iNormIn0 = csnget(NormInPlace, i0)
    assert(iSortIn0 == 1)
    assert(iNormIn0 == 0.6)

    /* Never triggered. Unlike the binops, whose k init publishes a copy of the
       operand, the unary k init already applies the op, so the gated output is
       the absolute value and not the raw source. */
    iFrozen0 = csnget(Frozen, i0)
    assert(iFrozen0 == 2.5)

    /* [[1, 2, 3], [4, 5, 6]] times [[7, 8], [9, 10], [11, 12]]. */
    i00[] = array(0, 0)
    i11[] = array(1, 1)
    iProdShape[] = csnshape(Product)
    iProd00 = csnget(Product, i00)
    iProd11 = csnget(Product, i11)
    assert(iProd00 == 58 && iProd11 == 154)
    assert(iProdShape[0] == 2 && iProdShape[1] == 2 && csnsize(Product) == 4)

    iMatDot = i(gkMatDot)
    iAngleRight = i(gkAngleRight)
    iAngleHalf = i(gkAngleHalf)
    assert(iMatDot == 0)
    assert(iAngleRight > 1.5707 && iAngleRight < 1.5709)
    assert(iAngleHalf > 0.7853 && iAngleHalf < 0.7855)

    /* Window of 3 over [1, 5, 2, 4, 3]: position 1 sees [1, 5, 2]. */
    iMed1 = csnget(MovMedian, i1)
    iMin1 = csnget(MovMin, i1)
    iMax1 = csnget(MovMax, i1)
    iMean1 = csnget(MovMean, i1)
    iVar1 = csnget(MovVar, i1)
    assert(iMed1 == 2 && iMin1 == 1 && iMax1 == 5)
    assert(iMean1 > 2.66 && iMean1 < 2.67)
    assert(iVar1 > 2.888 && iVar1 < 2.89)

    /* Gated off: the in-place filter must not have touched the sequence. */
    iGated1 = csnget(MovGated, i1)
    assert(iGated1 == 5)

    /* csndiag both ways: a vector becomes the diagonal of a square matrix, a
       matrix gives its diagonal back. */
    i22[] = array(2, 2)
    iSpreadShape[] = csnshape(Spread)
    iSpread22 = csnget(Spread, i22)
    iSpread01 = csnget(Spread, i01)
    iExtract1 = csnget(Extracted, i1)
    assert(iSpreadShape[0] == 4 && iSpreadShape[1] == 4)
    assert(iSpread22 == 3 && iSpread01 == 0)
    assert(iExtract1 == 4 && csnsize(Extracted) == 2)

    /* Slice 1..3 of [1, 2, 3, 4], with the whole spec arriving at k-rate. */
    iSliced0 = csnget(Sliced, i0)
    iSliced1 = csnget(Sliced, i1)
    assert(iSliced0 == 2 && iSliced1 == 3)
    assert(csnsize(Sliced) == 2)

    /* csnreal and csnimag hand back real arrays, csnconj and csntocomplex
       complex ones, whatever the source was. */
    iRealPart0 = csnget(RealPart, i0)
    iImagPart0 = csnget(ImagPart, i0)
    assert(csntype(RealPart) == 0 && csntype(ImagPart) == 0)
    assert(csntype(Conjugate) == 1 && csntype(Promoted) == 1)
    assert(iRealPart0 == 1 && iImagPart0 == 0)

    /* [1, 2, 3, 4] reversed in place an odd number of times. */
    iReversed0 = csnget(Reversed, i0)
    assert(iReversed0 == 4 || iReversed0 == 1)

    /* Never triggered: the k init of csnreverse must already have reversed the
       copy it publishes, complex lane included. */
    iGated0 = csnget(GatedReverse, i0)
    iGated3 = csnget(GatedReverse, i3)
    GatedC0:Complex = csnget(GatedReverseComplex, i0)
    iGatedC0 = real(GatedC0)
    assert(iGated0 == 4 && iGated3 == 1)
    assert(iGatedC0 == 4)

    /* [[1, 2], [3, 4]]: median down the columns, then across the rows. */
    iPercAxis0 = csnget(PercAxis, i0)
    iQuantAxis0 = csnget(QuantAxis, i0)
    assert(iPercAxis0 == 2 && iQuantAxis0 == 1.5)

    /* Trace of [[1, 2], [3, 4]], real and complex overloads. */
    iTrace = i(gkTraceReal)
    iTraceComplex = i(gkTraceComplexRe)
    assert(iTrace == 5 && iTraceComplex == 5)

    iNormS = i(gkNorm)
    iDotS = i(gkDot)
    iInnerS = i(gkInner)
    iDistS = i(gkDist)
    assert(iNormS == 5)
    assert(iDotS == 3 && iInnerS == 3)
    assert(iDistS > 4.47 && iDistS < 4.48)
endin

/* The scalar forms write a k-value; it is parked in a global so instr 2 can
   check it at i-time, where a failed assert is reported once instead of once
   per control period. */
instr 3
    kTrig init 1
    kOrder = 2
    gkNorm = csnnorm(A, kOrder, kTrig)
    gkDot = csndot(A, B, kTrig)
    gkInner = csninner(A, B, kTrig)
    gkDist = csndist(A, B, kOrder, kTrig)

    /* Two 1-D arrays: the matrix product degenerates to the dot product. */
    gkMatDot = csnmatmul(B, YAxis, kTrig)
    gkAngleRight = csnangledist(B, YAxis, kTrig)
    gkAngleHalf = csnangledist(B, Diag, kTrig)

    gkTraceReal = csntrace(Square, kTrig)
    kComplexTrace:Complex = csntrace(ComplexSquare, kTrig)
    gkTraceComplexRe = real(kComplexTrace)
endin
</CsInstruments>

<CsScore>
i 1 0 0.006
i 3 0 0.006
i 2 0.004 0.001
e
</CsScore>
</CsoundSynthesizer>
