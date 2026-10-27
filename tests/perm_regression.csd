<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

/* csnperm against a Python rewrite of Heap's algorithm, whose rows are the
   ones of itertools.permutations in another order. -1 asks for all n! rows,
   a count for at most that many. The k form follows a source that changes at
   perf and starts from a source nothing has computed at init. */

opcode mismatches(iGot[], iWant[]):i
    iBad = abs(lenarray(iGot) - lenarray(iWant))
    if iBad == 0 then
        iIdx = 0
        while iIdx < lenarray(iGot) do
            if iGot[iIdx] != iWant[iIdx] then
                iBad += 1
            endif
            iIdx += 1
        od
    endif
    xout(iBad)
endop

gkPermFollow init -1
gkPermLate init -1
gkPermLateLen init -1

instr 1
    ; every permutation of three: (6, 3) in Heap's order
    X:CsnArr = csnfromarray(fillarray(1, 2, 3))
    P:CsnArr = csnperm(X)
    iShape[] = csnshape(P)
    iWantShape[] = fillarray(6, 3)
    iBad = mismatches(iShape, iWantShape)
    assert(iBad == 0)
    iGot[] = csntoarray(csnflatten(P))
    iWant[] = fillarray(1, 2, 3,  2, 1, 3,  3, 1, 2,  1, 3, 2,  2, 3, 1,  3, 2, 1)
    iBad = mismatches(iGot, iWant)
    assert(iBad == 0)

    ; the first five of four: Heap's even step swaps i with the last
    Y:CsnArr = csnfromarray(fillarray(1, 2, 3, 4))
    iGot5[] = csntoarray(csnflatten(csnperm(Y, 5)))
    iWant5[] = fillarray(1, 2, 3, 4,  2, 1, 3, 4,  3, 1, 2, 4,  1, 3, 2, 4,  2, 3, 1, 4)
    iBad = mismatches(iGot5, iWant5)
    assert(iBad == 0)

    ; all 24 permutations of four: Heap's last is (2, 3, 4, 1)
    iGotAll[] = csntoarray(csnflatten(csnperm(Y, -1)))
    assert(lenarray(iGotAll) == 96)
    iLast[] = slicearray(iGotAll, 92, 95)
    iWantLast[] = fillarray(2, 3, 4, 1)
    iBad = mismatches(iLast, iWantLast)
    assert(iBad == 0)

    ; zero rows, and a count past n! stops at n!
    Z:CsnArr = csnperm(Y, 0)
    iShapeZ[] = csnshape(Z)
    iWantShapeZ[] = fillarray(0, 4)
    iBad = mismatches(iShapeZ, iWantShapeZ)
    assert(iBad == 0)
    iShapeC[] = csnshape(csnperm(X, 100))
    iBad = mismatches(iShapeC, iWantShape)
    assert(iBad == 0)

    prints("perm i-time ok\n")
endin

instr 2
    ; X[0] = kCount + 10, so the last pass sees [13, 2, 3], whose second row
    ; is [2, 13, 3]
    X:CsnArr = csnfromarray(fillarray(1, 2, 3))
    kCount init 0
    kTrig init 1
    X[0] = kCount + 10
    P:CsnArr = csnperm(X, 2, kTrig)
    gkPermFollow = P[1][1]
    kCount += 1
endin

instr 3
    ; the joined source is an empty placeholder at init; the first pass sees
    ; [1, 2, 1, 2], whose third row is [1, 1, 2, 2]
    kTrig init 1
    kAxis init 0
    A:CsnArr = csnfromarray(fillarray(1, 2))
    J:CsnArr = csnconcat(A, A, kTrig, kAxis)
    P:CsnArr = csnperm(J, 3, kTrig)
    gkPermLate = P[2][1]
    gkPermLateLen = csnsize:k(P)
endin

instr 4
    assert(i(gkPermFollow) == 13)
    assert(i(gkPermLate) == 1)
    assert(i(gkPermLateLen) == 12)
    prints("perm ok\n")
endin
</CsInstruments>
<CsScore>
i 1 0 0.001
i 2 0 [4 * 32 / 48000]
i 3 0 [2 * 32 / 48000]
i 4 0.01 0.001
e
</CsScore>
</CsoundSynthesizer>
