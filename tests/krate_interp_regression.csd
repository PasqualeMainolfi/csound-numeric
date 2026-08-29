<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>

<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

; k-rate coverage for csninterp and csnresample: every interpolation mode,
; every bounds mode, the flattened and the per-axis forms, and the version
; cache that has to notice a source, a table or a length changing mid-note.

giMatShape[] = fillarray(2, 3)

Xd@global:CsnArr = csnfromarray(array(0, 1, 2, 3))
Yd@global:CsnArr = csnfromarray(array(0, 10, 20, 30))
Query@global:CsnArr = csnfromarray(array(0.5, 1.5, 2.5))
QueryMat@global:CsnArr = csnreshape(csnfromarray(array(0.5, 1.5, 2.5, 0.2, 1.2, 2.2)), giMatShape)
Ramp@global:CsnArr = csnfromarray(array(0, 10, 20, 30))
Grid@global:CsnArr = csnreshape(csnfromarray(array(0, 1, 2, 10, 11, 12)), giMatShape)

; Mutated mid-note to prove the caches invalidate.
CacheQuery@global:CsnArr = csnfromarray(array(0.5, 1.5, 2.5))
CacheTable@global:CsnArr = csnfromarray(array(0, 10, 20, 30))

InterpFlat@global:CsnArr = csnempty(array(0))
InterpAxis0@global:CsnArr = csnempty(array(0))
InterpAxis1@global:CsnArr = csnempty(array(0))
InterpCache@global:CsnArr = csnempty(array(0))
ResUp@global:CsnArr = csnempty(array(0))
ResDown@global:CsnArr = csnempty(array(0))
ResOne@global:CsnArr = csnempty(array(0))
ResNearest@global:CsnArr = csnempty(array(0))
ResCubic@global:CsnArr = csnempty(array(0))
ResAxis0@global:CsnArr = csnempty(array(0))
ResAxis1@global:CsnArr = csnempty(array(0))
ResFlat@global:CsnArr = csnempty(array(0))
ResLen@global:CsnArr = csnempty(array(0))

gkLinear init -1
gkNearest init -1
gkPrevious init -1
gkNext init -1
gkCubic init -1
gkClampHi init -1
gkClampLo init -1
gkFill init -1
gkExtrapHi init -1
gkExtrapLo init -1
gkNodeLow init -1
gkNodeHigh init -1
gkLength init 4

; Scalar k form: one query point, every mode and every bounds policy.
instr 1
    kAt = 1.5
    gkLinear = csninterp(kAt, Xd, Yd, 0, 1)
    gkNearest = csninterp(kAt, Xd, Yd, 1, 1)
    gkPrevious = csninterp(kAt, Xd, Yd, 2, 1)
    gkNext = csninterp(kAt, Xd, Yd, 3, 1)
    gkCubic = csninterp(kAt, Xd, Yd, 4, 1)

    kAbove = 5
    kBelow = -2
    gkClampHi = csninterp(kAbove, Xd, Yd, 0, 1)
    gkClampLo = csninterp(kBelow, Xd, Yd, 0, 1)
    gkFill = csninterp(kAbove, Xd, Yd, 0, 2, -7)
    gkExtrapHi = csninterp(kAbove, Xd, Yd, 0, 3)
    gkExtrapLo = csninterp(kBelow, Xd, Yd, 0, 3)

    ; The breakpoints themselves must come back exactly.
    kFirst = 0
    kLast = 3
    gkNodeLow = csninterp(kFirst, Xd, Yd, 0, 1)
    gkNodeHigh = csninterp(kLast, Xd, Yd, 0, 1)
endin

; Vector k form. The mapping is elementwise, so the flattened pass and both
; axis passes have to agree value by value.
instr 2
    InterpFlat = csninterp(Query, Xd, Yd, 0, 1)
    InterpAxis0 = csninterp(QueryMat, Xd, Yd, 0, 1, 0)
    InterpAxis1 = csninterp(QueryMat, Xd, Yd, 0, 1, 1)
    InterpCache = csninterp(CacheQuery, Xd, CacheTable, 0, 1)
endin

; csnresample at k-rate: length is a k argument, axis defaults to the
; flattened form.
instr 3
    ResUp = csnresample(Ramp, 7, 0, 1)
    ResDown = csnresample(Ramp, 2, 0, 1)
    ResOne = csnresample(Ramp, 1, 0, 1)
    ResNearest = csnresample(Ramp, 7, 1, 1)
    ResCubic = csnresample(Ramp, 7, 4, 1)
    ResAxis0 = csnresample(Grid, 3, 0, 1, 0, 0)
    ResAxis1 = csnresample(Grid, 5, 0, 1, 0, 1)
    ResFlat = csnresample(Grid, 3, 0, 1)
    ResLen = csnresample(Ramp, gkLength, 0, 1)
endin

; Checker A: everything as first published.
instr 30
    iLinear = i(gkLinear)
    iNearest = i(gkNearest)
    iPrevious = i(gkPrevious)
    iNext = i(gkNext)
    iCubic = i(gkCubic)
    assert(iLinear == 15)
    ; 1.5 sits exactly halfway, and a tie takes the left breakpoint.
    assert(iNearest == 10)
    assert(iPrevious == 10)
    assert(iNext == 20)
    ; PCHIP through collinear breakpoints is the line itself.
    assert(abs(iCubic - 15) < 1e-12)

    iClampHi = i(gkClampHi)
    iClampLo = i(gkClampLo)
    iFill = i(gkFill)
    iExtrapHi = i(gkExtrapHi)
    iExtrapLo = i(gkExtrapLo)
    assert(iClampHi == 30 && iClampLo == 0)
    assert(iFill == -7)
    assert(iExtrapHi == 50 && iExtrapLo == -20)

    iNodeLow = i(gkNodeLow)
    iNodeHigh = i(gkNodeHigh)
    assert(iNodeLow == 0 && iNodeHigh == 30)

    ; --- csninterp, vector -------------------------------------------------
    iFlat[] = csntoarray(InterpFlat)
    iFlatDims = csndims(InterpFlat)
    iFlatSize = csnsize(InterpFlat)
    assert(iFlatDims == 1 && iFlatSize == 3)
    assert(iFlat[0] == 5 && iFlat[1] == 15 && iFlat[2] == 25)

    ; The source shape survives: 2x3 in, 2x3 out, on either axis.
    iAxis0[][] = csntoarray(InterpAxis0)
    iAxis1[][] = csntoarray(InterpAxis1)
    iAxis0Shape[] = csnshape(InterpAxis0)
    assert(iAxis0Shape[0] == 2 && iAxis0Shape[1] == 3)
    assert(iAxis0[0][0] == 5 && iAxis0[0][2] == 25)
    assert(abs(iAxis0[1][0] - 2) < 1e-12 && abs(iAxis0[1][2] - 22) < 1e-12)
    assert(iAxis1[0][0] == 5 && iAxis1[0][2] == 25)
    assert(abs(iAxis1[1][0] - 2) < 1e-12 && abs(iAxis1[1][2] - 22) < 1e-12)

    iCache[] = csntoarray(InterpCache)
    assert(iCache[0] == 5 && iCache[2] == 25)

    ; --- csnresample -------------------------------------------------------
    ; 4 -> 7 spans the whole source, so the endpoints are kept and the step
    ; halves: 0 5 10 ... 30.
    iUp[] = csntoarray(ResUp)
    iUpSize = csnsize(ResUp)
    assert(iUpSize == 7)
    assert(iUp[0] == 0 && iUp[6] == 30)
    assert(abs(iUp[1] - 5) < 1e-12 && abs(iUp[3] - 15) < 1e-12 && abs(iUp[5] - 25) < 1e-12)

    iDown[] = csntoarray(ResDown)
    iDownSize = csnsize(ResDown)
    assert(iDownSize == 2)
    assert(iDown[0] == 0 && iDown[1] == 30)

    ; A single output point has no span to divide and takes the first sample.
    iOne[] = csntoarray(ResOne)
    iOneDims = csndims(ResOne)
    iOneSize = csnsize(ResOne)
    assert(iOneDims == 1 && iOneSize == 1)
    assert(iOne[0] == 0)

    iNear[] = csntoarray(ResNearest)
    assert(iNear[0] == 0 && iNear[1] == 0 && iNear[2] == 10)
    assert(iNear[5] == 20 && iNear[6] == 30)

    iRCubic[] = csntoarray(ResCubic)
    assert(abs(iRCubic[1] - 5) < 1e-12 && abs(iRCubic[5] - 25) < 1e-12)

    ; 2x3 resampled along axis 0: the slices are strided, so this also covers
    ; the gather into the scratch buffer.
    iRAxis0[][] = csntoarray(ResAxis0)
    iRAxis0Shape[] = csnshape(ResAxis0)
    assert(iRAxis0Shape[0] == 3 && iRAxis0Shape[1] == 3)
    assert(iRAxis0[0][0] == 0 && iRAxis0[0][2] == 2)
    assert(abs(iRAxis0[1][0] - 5) < 1e-12 && abs(iRAxis0[1][2] - 7) < 1e-12)
    assert(iRAxis0[2][0] == 10 && iRAxis0[2][2] == 12)

    iRAxis1[][] = csntoarray(ResAxis1)
    iRAxis1Shape[] = csnshape(ResAxis1)
    assert(iRAxis1Shape[0] == 2 && iRAxis1Shape[1] == 5)
    assert(iRAxis1[0][0] == 0 && iRAxis1[0][4] == 2)
    assert(abs(iRAxis1[0][1] - 0.5) < 1e-12)
    assert(iRAxis1[1][0] == 10 && iRAxis1[1][4] == 12)
    assert(abs(iRAxis1[1][3] - 11.5) < 1e-12)

    ; Flattening collapses the rank: the 2x3 is read as one 6-point vector.
    iRFlat[] = csntoarray(ResFlat)
    iRFlatDims = csndims(ResFlat)
    iRFlatSize = csnsize(ResFlat)
    assert(iRFlatDims == 1 && iRFlatSize == 3)
    assert(iRFlat[0] == 0 && iRFlat[2] == 12)
    assert(abs(iRFlat[1] - 6) < 1e-12)

    iLen[] = csntoarray(ResLen)
    iLenSize = csnsize(ResLen)
    assert(iLenSize == 4)
    assert(iLen[0] == 0 && iLen[1] == 10 && iLen[3] == 30)
endin

; Mutators: a query point, a y table and the resample length, each of which
; must invalidate the cached pass.
instr 40
    kCell[] init 1
    kCell[0] = 0
    csnset CacheQuery, kCell, 2.5
    turnoff
endin

instr 41
    kCell[] init 1
    kCell[0] = 3
    csnset CacheTable, kCell, 300
    gkLength = 7
    turnoff
endin

; Checker B: the same handles after the mutations.
instr 50
    ; CacheQuery[0] moved from 0.5 to 2.5, so the first output follows it.
    iCache[] = csntoarray(InterpCache)
    assert(iCache[0] == 25 && iCache[2] == 25)
endin

instr 51
    ; CacheTable[3] moved from 30 to 300: 2.5 now interpolates 20 and 300.
    iCache[] = csntoarray(InterpCache)
    assert(abs(iCache[0] - 160) < 1e-12 && abs(iCache[2] - 160) < 1e-12)

    ; The k-rate length grew from 4 to 7 and the array was republished.
    iLen[] = csntoarray(ResLen)
    iLenSize = csnsize(ResLen)
    assert(iLenSize == 7)
    assert(iLen[0] == 0 && iLen[6] == 30)
    assert(abs(iLen[1] - 5) < 1e-12 && abs(iLen[3] - 15) < 1e-12)
endin
</CsInstruments>

<CsScore>
i 1 0.000 0.040
i 2 0.000 0.040
i 3 0.000 0.040
i 30 0.004 0.001
i 40 0.008 0.001
i 50 0.014 0.001
i 41 0.018 0.001
i 51 0.024 0.001
e
</CsScore>
</CsoundSynthesizer>
