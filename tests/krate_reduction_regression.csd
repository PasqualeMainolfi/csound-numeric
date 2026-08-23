<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>

<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

giMatShape[] = fillarray(2, 3)

Flat@global:CsnArr = csnfromarray(array(1, 2, 3, 4))
Odd@global:CsnArr = csnfromarray(array(5, 1, 9, 3, 7))
Long@global:CsnArr = csnfromarray(array(1, 2, 3, 4, 5, 6))
Mat@global:CsnArr = csnreshape(Long, giMatShape)
Peaks@global:CsnArr = csnreshape(csnfromarray(array(1, 9, 3, 4, 5, 2)), giMatShape)
ComplexFlat@global:CsnArr = csntocomplex(Flat)

SumAxis0@global:CsnArr = csnempty(array(0))
SumAxis1@global:CsnArr = csnempty(array(0))
ProdAxis0@global:CsnArr = csnempty(array(0))
MinAxis0@global:CsnArr = csnempty(array(0))
MaxAxis0@global:CsnArr = csnempty(array(0))
VarAxis0@global:CsnArr = csnempty(array(0))
StdAxis0@global:CsnArr = csnempty(array(0))
MedianAxis0@global:CsnArr = csnempty(array(0))
GatedAxis0@global:CsnArr = csnempty(array(0))
ArgMinAxis0@global:CsnArr = csnempty(array(0, 0))
ArgMaxAxis0@global:CsnArr = csnempty(array(0, 0))

gkSum init 0
gkProd init 0
gkSub init 0
gkMean init 0
gkMin init 0
gkMax init 0
gkMedian init 0
gkAll init 0
gkAny init 0
gkStd init 0
gkVar init 0
gkGatedStd init 0
gkGatedMedian init 0
gkComplexRe init 0
gkComplexIm init 0
gkGatedRe init 0
gkGatedIm init 0

; Scalar reductions, all gated by the trigger.
instr 1
    kTrig = (timeinstk() == 2 ? 1 : 0)
    gkSum = csnsum(Flat, kTrig)
    gkProd = csnprod(Flat, kTrig)
    gkSub = csnsub(Flat, kTrig)
    gkMean = csnmean(Flat, kTrig)
    gkMin = csnmin(Flat, kTrig)
    gkMax = csnmax(Flat, kTrig)
    gkMedian = csnmedian(Odd, kTrig)
    gkAll = csnall(Flat, kTrig)
    gkAny = csnany(Flat, kTrig)
    gkStd = csnstd(Flat, kTrig)
    gkVar = csnvar(Flat, kTrig)
endin

; A zero trigger must leave the value the init pass published, not zero.
instr 2
    kZero = 0
    gkGatedStd = csnstd(Flat, kZero)
    gkGatedMedian = csnmedian(Odd, kZero)
endin

; Axis reductions: the k overload drops the reduced axis and republishes its
; own handle on every triggered pass.
instr 3
    kTrig = (timeinstk() == 2 ? 1 : 0)
    kZero = 0
    kAxis0 = 0
    kAxis1 = 1
    SumAxis0 = csnsum(Mat, kAxis0, kTrig)
    SumAxis1 = csnsum(Mat, kAxis1, kTrig)
    ProdAxis0 = csnprod(Mat, kAxis0, kTrig)
    MinAxis0 = csnmin(Mat, kAxis0, kTrig)
    MaxAxis0 = csnmax(Mat, kAxis0, kTrig)
    VarAxis0 = csnvar(Mat, kAxis0, kTrig)
    StdAxis0 = csnstd(Mat, kAxis0, kTrig)
    MedianAxis0 = csnmedian(Mat, kAxis0, kTrig)
    ; Never triggered: keeps the source copy the init pass published.
    GatedAxis0 = csnsum(Mat, kAxis0, kZero)

    ; argmin/argmax yield one row of source coordinates per reduced position,
    ; so the result is 2-D whatever the source's rank.
    ArgMinAxis0 = csnargmin(Peaks, kAxis0, kTrig)
    ArgMaxAxis0 = csnargmax(Peaks, kAxis0, kTrig)
endin

; Complex scalar reductions: same in/out types as the i-rate .c overloads, so
; the trailing trigger is what selects the k form.
instr 4
    kTrig = (timeinstk() == 2 ? 1 : 0)
    kZero = 0
    CSum:Complex = csnsum(ComplexFlat, kTrig)
    CGated:Complex = csnmean(ComplexFlat, kZero)
    gkComplexRe = real(CSum)
    gkComplexIm = imag(CSum)
    gkGatedRe = real(CGated)
    gkGatedIm = imag(CGated)
endin

instr 5
    ; --- scalar -----------------------------------------------------------
    iSum = i(gkSum)
    iProd = i(gkProd)
    iSub = i(gkSub)
    iMean = i(gkMean)
    iMin = i(gkMin)
    iMax = i(gkMax)
    iMedian = i(gkMedian)
    iAll = i(gkAll)
    iAny = i(gkAny)
    iStd = i(gkStd)
    iVar = i(gkVar)

    assert(iSum == 10)
    assert(iProd == 24)
    assert(iSub == -8)
    assert(iMean == 2.5)
    assert(iMin == 1 && iMax == 4)
    assert(iMedian == 5)
    assert(iAll == 1 && iAny == 1)
    assert(abs(iVar - 1.25) < 1e-12)
    assert(abs(iStd - sqrt(1.25)) < 1e-12)

    ; --- gated scalars keep the i-time value -------------------------------
    iGatedStd = i(gkGatedStd)
    iGatedMedian = i(gkGatedMedian)
    assert(abs(iGatedStd - sqrt(1.25)) < 1e-12)
    assert(iGatedMedian == 5)

    ; The source is untouched by every reduction above.
    iSourceValues[] = csntoarray(Flat)
    assert(csnsize(Flat) == 4 && csndims(Flat) == 1)
    assert(iSourceValues[0] == 1 && iSourceValues[3] == 4)

    ; --- axis 0 (2x3 -> 3) -------------------------------------------------
    iSum0[] = csntoarray(SumAxis0)
    iSum0Shape[] = csnshape(SumAxis0)
    assert(csndims(SumAxis0) == 1 && csnsize(SumAxis0) == 3)
    assert(iSum0Shape[0] == 3)
    assert(iSum0[0] == 5 && iSum0[1] == 7 && iSum0[2] == 9)

    iProd0[] = csntoarray(ProdAxis0)
    assert(iProd0[0] == 4 && iProd0[1] == 10 && iProd0[2] == 18)

    iMin0[] = csntoarray(MinAxis0)
    iMax0[] = csntoarray(MaxAxis0)
    assert(iMin0[0] == 1 && iMin0[1] == 2 && iMin0[2] == 3)
    assert(iMax0[0] == 4 && iMax0[1] == 5 && iMax0[2] == 6)

    ; median of a 2-element run is the mean of the pair
    iMed0[] = csntoarray(MedianAxis0)
    assert(csnsize(MedianAxis0) == 3)
    assert(iMed0[0] == 2.5 && iMed0[1] == 3.5 && iMed0[2] == 4.5)

    ; var over axis 0 of [[1,2,3],[4,5,6]] is 2.25 on every column
    iVar0[] = csntoarray(VarAxis0)
    iStd0[] = csntoarray(StdAxis0)
    assert(csnsize(VarAxis0) == 3)
    assert(abs(iVar0[0] - 2.25) < 1e-12)
    assert(abs(iVar0[2] - 2.25) < 1e-12)
    assert(abs(iStd0[0] - 1.5) < 1e-12)

    ; --- axis 1 (2x3 -> 2) -------------------------------------------------
    iSum1[] = csntoarray(SumAxis1)
    assert(csndims(SumAxis1) == 1 && csnsize(SumAxis1) == 2)
    assert(iSum1[0] == 6 && iSum1[1] == 15)

    ; --- untriggered axis form keeps the init-time source copy -------------
    iGatedShape[] = csnshape(GatedAxis0)
    assert(csndims(GatedAxis0) == 2 && csnsize(GatedAxis0) == 6)
    assert(iGatedShape[0] == 2 && iGatedShape[1] == 3)

    ; --- argmin / argmax: (count, source ndim) coordinate rows -------------
    ; Peaks is [[1,9,3],[4,5,2]], so along axis 0 the minima sit at
    ; (0,0) (1,1) (1,2) and the maxima at (1,0) (0,1) (0,2).
    iArgMinShape[] = csnshape(ArgMinAxis0)
    assert(csndims(ArgMinAxis0) == 2 && csnsize(ArgMinAxis0) == 6)
    assert(iArgMinShape[0] == 3 && iArgMinShape[1] == 2)

    iR0C0[] = array(0, 0)
    iR0C1[] = array(0, 1)
    iR1C0[] = array(1, 0)
    iR1C1[] = array(1, 1)
    iR2C0[] = array(2, 0)
    iR2C1[] = array(2, 1)

    assert(csnget(ArgMinAxis0, iR0C0) == 0 && csnget(ArgMinAxis0, iR0C1) == 0)
    assert(csnget(ArgMinAxis0, iR1C0) == 1 && csnget(ArgMinAxis0, iR1C1) == 1)
    assert(csnget(ArgMinAxis0, iR2C0) == 1 && csnget(ArgMinAxis0, iR2C1) == 2)

    assert(csndims(ArgMaxAxis0) == 2 && csnsize(ArgMaxAxis0) == 6)
    assert(csnget(ArgMaxAxis0, iR0C0) == 1 && csnget(ArgMaxAxis0, iR0C1) == 0)
    assert(csnget(ArgMaxAxis0, iR1C0) == 0 && csnget(ArgMaxAxis0, iR1C1) == 1)
    assert(csnget(ArgMaxAxis0, iR2C0) == 0 && csnget(ArgMaxAxis0, iR2C1) == 2)

    ; The k form must agree with the i-rate one on rank and size.
    iRefArgMin:CsnArr = csnargmin(Peaks, 0)
    assert(csndims(iRefArgMin) == 2 && csnsize(iRefArgMin) == 6)

    ; The 2-D source keeps its rank and contents.
    iMatShape[] = csnshape(Mat)
    assert(csndims(Mat) == 2 && csnsize(Mat) == 6)
    assert(iMatShape[0] == 2 && iMatShape[1] == 3)

    ; --- complex -----------------------------------------------------------
    iCre = i(gkComplexRe)
    iCim = i(gkComplexIm)
    assert(iCre == 10 && iCim == 0)

    iGre = i(gkGatedRe)
    iGim = i(gkGatedIm)
    assert(iGre == 2.5 && iGim == 0)

    ; The i-rate overloads still resolve and carry no perf routine.
    iISum = csnsum(Flat)
    iIMedian = csnmedian(Odd)
    ICSum:Complex = csnsum(ComplexFlat)
    iICre = real(ICSum)
    iICim = imag(ICSum)
    assert(iISum == 10 && iIMedian == 5)
    assert(iICre == 10 && iICim == 0)
endin
</CsInstruments>

<CsScore>
i 1 0.000 0.010
i 2 0.000 0.010
i 3 0.000 0.010
i 4 0.000 0.010
i 5 0.006 0.001
e
</CsScore>
</CsoundSynthesizer>
