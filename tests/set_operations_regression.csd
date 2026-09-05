<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>

<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

giRawA[] = array(3, 1, 2, 2)
giRawB[] = array(4, 2)
RawA@global:CsnArr = csnfromarray(giRawA)
RawB@global:CsnArr = csnfromarray(giRawB)
A@global:CsnArr = csnlikeset(RawA)
B@global:CsnArr = csnlikeset(RawB)
EmptyRaw@global:CsnArr = csnempty(array(1))
EmptySet@global:CsnArr = csnlikeset(EmptyRaw)

KU@global:CsnArr = csnsetunion(A, B)
KI@global:CsnArr = csnsetintersect(A, B)
KD@global:CsnArr = csnsetdiff(A, B)
KS@global:CsnArr = csnsetsymdiff(A, B)
KMut@global:CsnArr = csnlikeset(RawA)

gkContains init 0
gkContainsHeld init 0
gkSubset init 0
gkSuperset init 0
gkDisjoint init 0
gkEqual init 0

instr 1
    U:CsnArr = csnsetunion(A, B)
    I:CsnArr = csnsetintersect(A, B)
    D:CsnArr = csnsetdiff(A, B)
    S:CsnArr = csnsetsymdiff(A, B)
    iU[] = csntoarray(U)
    iI[] = csntoarray(I)
    iD[] = csntoarray(D)
    iS[] = csntoarray(S)

    assert(lenarray(iU) == 4 && iU[0] == 1 && iU[1] == 2 && iU[2] == 3 && iU[3] == 4)
    assert(lenarray(iI) == 1 && iI[0] == 2)
    assert(lenarray(iD) == 2 && iD[0] == 1 && iD[1] == 3)
    assert(lenarray(iS) == 3 && iS[0] == 1 && iS[1] == 3 && iS[2] == 4)
    assert(csnsetcontains(A, 2) == 1)
    assert(csnsetcontains(A, 5) == 0)
    assert(csnsetissubset(I, A) == 1)
    assert(csnsetissuperset(A, I) == 1)
    assert(csnsetisdisjoint(D, B) == 1)
    assert(csnsetisequal(A, A) == 1)

    EmptyUnion:CsnArr = csnsetunion(EmptySet, A)
    EmptyIntersect:CsnArr = csnsetintersect(EmptySet, A)
    assert(csnsize(EmptySet) == 0)
    assert(csnsetcontains(EmptySet, 1) == 0)
    assert(csnsetisequal(EmptyUnion, A) == 1)
    assert(csnsize(EmptyIntersect) == 0)

    M:CsnArr = csnlikeset(RawA)
    csnsetinsert(M, 0)
    csnsetremove(M, 2)
    iM[] = csntoarray(M)
    assert(lenarray(iM) == 3 && iM[0] == 0 && iM[1] == 1 && iM[2] == 3)

    iNaN = sqrt(-1)
    iNaNRaw1[] = array(iNaN, iNaN)
    iNaNRaw2[] = array(iNaN)
    NaNRaw1:CsnArr = csnfromarray(iNaNRaw1)
    NaNRaw2:CsnArr = csnfromarray(iNaNRaw2)
    NaNSet1:CsnArr = csnlikeset(NaNRaw1)
    NaNSet2:CsnArr = csnlikeset(NaNRaw2)
    NaNUnion:CsnArr = csnsetunion(NaNSet1, NaNSet2)
    NaNIntersect:CsnArr = csnsetintersect(NaNSet1, NaNSet2)
    assert(csnsize(NaNSet1) == 1)
    assert(csnsetcontains(NaNSet1, iNaN) == 1)
    assert(csnsetisequal(NaNSet1, NaNSet2) == 1)
    assert(csnsize(NaNUnion) == 1)
    assert(csnsize(NaNIntersect) == 1)

    Plain:CsnArr = csnlikeset(RawA)
    csnunlikeset(Plain)
    iIndex0[] = array(0)
    csnset(Plain, iIndex0, 99)
    iPlain[] = csntoarray(Plain)
    assert(iPlain[0] == 99)
endin

instr 2
    kTrig init 1
    kZero init 0
    kTwo init 2
    KU = csnsetunion(A, B, kTrig)
    KI = csnsetintersect(A, B, kTrig)
    KD = csnsetdiff(A, B, kTrig)
    KS = csnsetsymdiff(A, B, kTrig)
    gkContains = csnsetcontains(A, kTwo, kTrig)
    gkContainsHeld = csnsetcontains(A, kTwo, kZero)
    gkSubset = csnsetissubset(A, B, kTrig)
    gkSuperset = csnsetissuperset(A, B, kTrig)
    gkDisjoint = csnsetisdisjoint(A, B, kTrig)
    gkEqual = csnsetisequal(A, A, kTrig)
endin

instr 3
    assert(csnsize(KU) == 4)
    assert(csnsize(KI) == 1)
    assert(csnsize(KD) == 2)
    assert(csnsize(KS) == 3)
    assert(i(gkContains) == 1)
    assert(i(gkContainsHeld) == 1)
    assert(i(gkSubset) == 0)
    assert(i(gkSuperset) == 0)
    assert(i(gkDisjoint) == 0)
    assert(i(gkEqual) == 1)
endin

instr 4
    kTrig init 1
    kZero init 0
    csnsetinsert(KMut, kZero, kTrig)
endin

instr 5
    iValues[] = csntoarray(KMut)
    assert(lenarray(iValues) == 4 && iValues[0] == 0 && iValues[1] == 1 && iValues[2] == 2 && iValues[3] == 3)
endin

instr 6
    kTrig init 1
    kTwo init 2
    csnsetremove(KMut, kTwo, kTrig)
endin

instr 7
    iValues[] = csntoarray(KMut)
    assert(lenarray(iValues) == 3 && iValues[0] == 0 && iValues[1] == 1 && iValues[2] == 3)
endin
</CsInstruments>

<CsScore>
i 1 0.00 0.001
i 2 0.01 0.02
i 3 0.02 0.001
i 4 0.04 0.01
i 5 0.045 0.001
i 6 0.06 0.01
i 7 0.065 0.001
e
</CsScore>
</CsoundSynthesizer>
