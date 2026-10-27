<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

/* csnzcr and csnamdf against numpy. zcr follows librosa's zero_crossings with
   zero_pos=True and pad=False: |x| <= 1e-10 is 0, 0 is positive, and the rate
   is the crossings over the length. amdf is the curve
   D(tau) = mean(|x[:n - tau] - x[tau:]|) for tau 0..maxlag, in place of the
   axis. The k forms follow a source that changes at perf and start from a
   source nothing has computed at init. */

opcode mismatches(iGot[], iWant[]):i
    iBad = abs(lenarray(iGot) - lenarray(iWant))
    if iBad == 0 then
        iIdx = 0
        while iIdx < lenarray(iGot) do
            if abs(iGot[iIdx] - iWant[iIdx]) > 1e-12 then
                iBad += 1
            endif
            iIdx += 1
        od
    endif
    xout(iBad)
endop

gkZcrFollow init -1
gkAmdfFollow init -1
gkAmdfLate init -1
gkAmdfLateLen init -1

instr 1
    ; zcr, number form on a 1-D source: 7 crossings over 12
    iSig[] = fillarray(1, -1, 0, 2, -3, -1e-12, 4, 0, -2, 5, 6, -7)
    X:CsnArr = csnfromarray(iSig)
    iZ = csnzcr(X)
    assert(abs(iZ - 7 / 12) < 1e-12)

    ; zcr per frame, frames of 4 every 2, along the last axis and along axis 0
    F:CsnArr = csnframe(X, 4, 2)
    iGotZ[] = csntoarray(csnzcr(F))
    iWantZ[] = fillarray(0.5, 0.5, 0.25, 0.5, 0.5)
    iBad = mismatches(iGotZ, iWantZ)
    assert(iBad == 0)
    iGotZ0[] = csntoarray(csnzcr(F, 0))
    iWantZ0[] = fillarray(0.6, 0.2, 0.8, 0.2)
    iBad = mismatches(iGotZ0, iWantZ0)
    assert(iBad == 0)

    ; amdf on a 1-D source, period 4: the curve dips to 0 at lag 4
    iSine[] = fillarray(0, 1, 0, -1, 0, 1, 0, -1)
    V:CsnArr = csnfromarray(iSine)
    iGotA[] = csntoarray(csnamdf(V, 4))
    iWantA[] = fillarray(0, 1, 1, 1, 0)
    iBad = mismatches(iGotA, iWantA)
    assert(iBad == 0)

    ; amdf on 2-D (2, 6) along the last axis: (2, 3)
    iShapeM[] = fillarray(2, 6)
    M:CsnArr = csnreshape(csnfromarray(fillarray(0, 1, 0, -1, 0, 1, 1, 2, 3, 4, 5, 6)), iShapeM)
    AM:CsnArr = csnamdf(M, 2, -1)
    iShapeAM[] = csnshape(AM)
    iWantShapeAM[] = fillarray(2, 3)
    iBad = mismatches(iShapeAM, iWantShapeAM)
    assert(iBad == 0)
    iGotAM[] = csntoarray(csnflatten(AM))
    iWantAM[] = fillarray(0, 1, 1, 0, 1, 2)
    iBad = mismatches(iGotAM, iWantAM)
    assert(iBad == 0)

    ; the same rows as columns, (6, 2) along axis 0, maxlag 1: (2, 2)
    MT:CsnArr = csntranspose(M)
    AT:CsnArr = csnamdf(MT, 1, 0)
    iGotAT[] = csntoarray(csnflatten(AT))
    iWantAT[] = fillarray(0, 0, 1, 1)
    iBad = mismatches(iGotAT, iWantAT)
    assert(iBad == 0)

    prints("features i-time ok\n")
endin

instr 2
    ; zcr k number form: X[1] flips sign on every other pass, 0 then 2 / 4
    X:CsnArr = csnfromarray(fillarray(1, 1, 1, 1))
    kCount init 0
    kTrig init 1
    X[1] = kCount % 2 == 0 ? 1 : -1
    gkZcrFollow = csnzcr(X, kTrig)
    ; amdf k form: X[0] = kCount as well, so the last pass sees [3, -1, 1, 1]
    ; and D(1) = (4 + 2 + 0) / 3
    X[0] = kCount
    D:CsnArr = csnamdf(X, 1, kTrig, -1)
    gkAmdfFollow = D[1]
    kCount += 1
endin

instr 3
    ; the joined source is an empty placeholder at init; the first pass sees
    ; [1, -1, 1, -1], whose D(1) is 2
    kTrig init 1
    kAxis init 0
    A:CsnArr = csnfromarray(fillarray(1, -1))
    J:CsnArr = csnconcat(A, A, kTrig, kAxis)
    D:CsnArr = csnamdf(J, 2, kTrig, -1)
    gkAmdfLate = D[1]
    gkAmdfLateLen = csnsize:k(D)
endin

instr 4
    ; four passes of instr 2: the last had kCount 3, X = [3, -1, 1, 1]
    assert(i(gkZcrFollow) == 0.5)
    assert(i(gkAmdfFollow) == 2)
    assert(i(gkAmdfLate) == 2)
    assert(i(gkAmdfLateLen) == 3)
    prints("features ok\n")
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
