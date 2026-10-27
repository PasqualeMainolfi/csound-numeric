<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

/* csnframe against numpy: sliding_window_view(x, frame, axis)[..., ::hop, ...]
   with the window axis moved next to the framed one, so a source of shape
   (..., n, ...) gives (..., 1 + (n - frame) // hop, frame, ...). The k forms
   follow a source that changes at perf, keep their result on a zero trigger,
   and start from a source nothing has computed at init. */

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

gkFollow init -1
gkKept init -1
gkLate0 init -1
gkLate1 init -1
gkLateRows init -1

instr 1
    ; 1-D, frame 3, hop 2, the last axis by default
    V:CsnArr = csnarange(0, 7, 1)
    F1:CsnArr = csnframe(V, 3, 2)
    iShape1[] = csnshape(F1)
    iWantShape1[] = fillarray(3, 3)
    iBad = mismatches(iShape1, iWantShape1)
    assert(iBad == 0)
    iGot1[] = csntoarray(csnflatten(F1))
    iWant1[] = fillarray(0, 1, 2, 2, 3, 4, 4, 5, 6)
    iBad = mismatches(iGot1, iWant1)
    assert(iBad == 0)

    ; a tail shorter than a hop is dropped: 10 samples, frame 4, hop 3
    W:CsnArr = csnarange(0, 10, 1)
    F2:CsnArr = csnframe(W, 4, 3)
    iGot2[] = csntoarray(csnflatten(F2))
    iWant2[] = fillarray(0, 1, 2, 3, 3, 4, 5, 6, 6, 7, 8, 9)
    iBad = mismatches(iGot2, iWant2)
    assert(iBad == 0)

    ; 2-D (2, 7) along the last axis: each row framed on its own
    iShapeM[] = fillarray(2, 7)
    M:CsnArr = csnreshape(csnarange(0, 14, 1), iShapeM)
    F3:CsnArr = csnframe(M, 3, 2, -1)
    iShape3[] = csnshape(F3)
    iWantShape3[] = fillarray(2, 3, 3)
    iBad = mismatches(iShape3, iWantShape3)
    assert(iBad == 0)
    iGot3[] = csntoarray(csnflatten(F3))
    iWant3[] = fillarray(0, 1, 2, 2, 3, 4, 4, 5, 6, 7, 8, 9, 9, 10, 11, 11, 12, 13)
    iBad = mismatches(iGot3, iWant3)
    assert(iBad == 0)

    ; 2-D (7, 2) along axis 0: frames of rows, the columns after them
    iShapeN[] = fillarray(7, 2)
    N:CsnArr = csnreshape(csnarange(0, 14, 1), iShapeN)
    F4:CsnArr = csnframe(N, 3, 2, 0)
    iShape4[] = csnshape(F4)
    iWantShape4[] = fillarray(3, 3, 2)
    iBad = mismatches(iShape4, iWantShape4)
    assert(iBad == 0)
    iGot4[] = csntoarray(csnflatten(F4))
    iWant4[] = fillarray(0, 1, 2, 3, 4, 5, 4, 5, 6, 7, 8, 9, 8, 9, 10, 11, 12, 13)
    iBad = mismatches(iGot4, iWant4)
    assert(iBad == 0)

    ; 3-D (2, 6, 2) along the middle axis, frame 2, hop 3
    iShapeT[] = fillarray(2, 6, 2)
    T:CsnArr = csnreshape(csnarange(0, 24, 1), iShapeT)
    F5:CsnArr = csnframe(T, 2, 3, 1)
    iShape5[] = csnshape(F5)
    iWantShape5[] = fillarray(2, 2, 2, 2)
    iBad = mismatches(iShape5, iWantShape5)
    assert(iBad == 0)
    iGot5[] = csntoarray(csnflatten(F5))
    iWant5[] = fillarray(0, 1, 2, 3, 6, 7, 8, 9, 12, 13, 14, 15, 18, 19, 20, 21)
    iBad = mismatches(iGot5, iWant5)
    assert(iBad == 0)

    ; a frame as long as the source gives one frame
    F6:CsnArr = csnframe(V, 7, 5)
    iShape6[] = csnshape(F6)
    iWantShape6[] = fillarray(1, 7)
    iBad = mismatches(iShape6, iWantShape6)
    assert(iBad == 0)

    ; complex: both parts travel together
    C:CsnArr = csntocomplex(csnarange(0, 4, 1))
    C *= 2
    FC:CsnArr = csnframe(C, 2, 1)
    iGotRe[] = csntoarray(csnflatten(csnreal(FC)))
    iWantRe[] = fillarray(0, 2, 2, 4, 4, 6)
    iBad = mismatches(iGotRe, iWantRe)
    assert(iBad == 0)

    ; strings: copies, frame 2, hop 2
    S:CsnStr = csnstrsplit("a b c d e", " ")
    FS:CsnStr = csnframe(S, 2, 2)
    SJoined = csnstrjoin(FS, ",")
    iBad = strcmp(SJoined, "a,b,c,d") == 0 ? 0 : 1
    assert(iBad == 0)

    prints("frame i-time ok\n")
endin

instr 2
    ; k form: V[0] counts the passes and the first frame follows it
    V:CsnArr = csnarange(0, 5, 1)
    kCount init 0
    kTrig init 1
    V[0] = kCount
    F:CsnArr = csnframe(V, 2, 2, kTrig, -1)
    gkFollow = F[0][0]
    kCount += 1
endin

instr 3
    ; a zero trigger keeps the last result: two passes framed, then none
    V:CsnArr = csnarange(0, 4, 1)
    kCount init 0
    kTrig init 1
    V[0] = kCount
    F:CsnArr = csnframe(V, 2, 2, kTrig, -1)
    gkKept = F[0][0]
    kCount += 1
    kTrig = kCount < 2 ? 1 : 0
endin

instr 4
    ; the joined source is an empty placeholder at init, shorter than a frame;
    ; the first pass frames [0, 1, 0, 1]
    kTrig init 1
    kAxis init 0
    A:CsnArr = csnarange(0, 2, 1)
    J:CsnArr = csnconcat(A, A, kTrig, kAxis)
    F:CsnArr = csnframe(J, 3, 1, kTrig, 0)
    kShape[] = csnshape(F)
    gkLateRows = kShape[0]
    gkLate0 = F[0][2]
    gkLate1 = F[1][2]
endin

instr 5
    ; four passes of instr 2: the counter reached 3
    assert(i(gkFollow) == 3)
    ; instr 3 framed on its first two passes only
    assert(i(gkKept) == 1)
    assert(i(gkLateRows) == 2)
    assert(i(gkLate0) == 0)
    assert(i(gkLate1) == 1)
    prints("frame ok\n")
endin
</CsInstruments>
<CsScore>
i 1 0 0.001
i 2 0 [4 * 32 / 48000]
i 3 0 [4 * 32 / 48000]
i 4 0 [2 * 32 / 48000]
i 5 0.01 0.001
e
</CsScore>
</CsoundSynthesizer>
