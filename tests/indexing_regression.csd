<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

; v[i][j] goes through ##array_get / ##array_set / ##array_init, and
; v[a:b, step, axis] through slicearray. Every read is bound to a variable
; before assert: a nested call inside assert evaluates as 0.

giShape[] = fillarray(2, 3)
giSix[] = fillarray(0, 1, 2, 3, 4, 5)
G@global:CsnArr = csnfromarray(giSix)

instr 1
    ; One index per dimension.
    A:CsnArr = csnarange(0, 6, 1)
    M:CsnArr = csnreshape(A, giShape)
    iA4 = A[4]
    assert(iA4 == 4)
    iM12 = M[1][2]
    assert(iM12 == 5)
    iM01 = M[0][1]
    assert(iM01 == 1)

    ; = with i arguments and init write once, at init time.
    A[2] = 9
    iA2 = A[2]
    assert(iA2 == 9)
    M[1][0] = 7
    iM10 = M[1][0]
    assert(iM10 == 7)
    M[0][0] init 8
    iM00 = M[0][0]
    assert(iM00 == 8)

    ; csnget keeps working beside [].
    iCell[] = fillarray(1, 2)
    iGet = csnget(M, iCell)
    assert(iGet == 5)

    ; A complex array takes a Complex value through []; csnget reads it.
    Z:CsnArr = csnzeros(giShape, 1)
    C:Complex = init(3, 4, 0)
    Z[1][2] = C
    R:Complex = csnget(Z, iCell)
    iRe = real(R)
    iIm = imag(R)
    assert(iRe == 3 && iIm == 4)
endin

instr 2
    ; The slice syntax ends inclusive, as Csound's own slicearray.
    S:CsnArr = csnarange(0, 6, 1)
    M:CsnArr = csnreshape(S, giShape)

    W1:CsnArr = S[1:3]
    iSize1 = csnsize(W1)
    iW10 = W1[0]
    iW12 = W1[2]
    assert(iSize1 == 3 && iW10 == 1 && iW12 == 3)

    ; An omitted end is the last element.
    W2:CsnArr = S[2:]
    iSize2 = csnsize(W2)
    iW23 = W2[3]
    assert(iSize2 == 4 && iW23 == 5)

    ; An omitted start is 0.
    W3:CsnArr = S[:2]
    iSize3 = csnsize(W3)
    assert(iSize3 == 3)

    ; A step after the comma.
    W4:CsnArr = S[0:5, 2]
    iSize4 = csnsize(W4)
    iW42 = W4[2]
    assert(iSize4 == 3 && iW42 == 4)

    ; Axis 0 by default: rows of a 2-D array.
    W5:CsnArr = M[1:1]
    iSize5 = csnsize(W5)
    iW502 = W5[0][2]
    assert(iSize5 == 3 && iW502 == 5)

    ; An axis after the step; -1 is the last element of that axis.
    W6:CsnArr = M[1:-1, 1, 1]
    iSize6 = csnsize(W6)
    iW610 = W6[1][0]
    assert(iSize6 == 4 && iW610 == 4)

    ; csngetslice keeps its exclusive stop.
    W7:CsnArr = csngetslice(S, 0, 1, 3, 1)
    iSize7 = csnsize(W7)
    assert(iSize7 == 2)
endin

gkRead init 0
gkSum init 0
gkSlice init 0
gkOnce init 0
gkKSliceSize init 0
instr 3
    K:CsnArr = csnarange(0, 6, 1)
    kIdx init 1
    k0 init 0
    kCount init 0
    kCount += 1

    ; A k value writes every pass; an i value only once, at init.
    K[kIdx] = kCount
    K[0] = 50

    ; Operators and an i-bound slice follow the writes made at perf.
    Sum:CsnArr = K + 100
    Sl:CsnArr = K[1:3]
    ; A k bound gives the k form of slicearray.
    Ks:CsnArr = K[kIdx:4]

    gkRead = K[kIdx]
    gkSum = Sum[kIdx]
    gkSlice = Sl[k0]
    gkOnce = K[k0]
    gkKSliceSize = csnsize(Ks)
endin

instr 4
    ; Four passes.
    iRead = i(gkRead)
    iSum = i(gkSum)
    iSlice = i(gkSlice)
    iOnce = i(gkOnce)
    iKSliceSize = i(gkKSliceSize)
    assert(iRead == 4)
    assert(iSum == 104)
    assert(iSlice == 4)
    assert(iOnce == 50)
    assert(iKSliceSize == 4)
endin

instr 5
    ; A global written through [] by one instrument...
    G[1] = 42
endin

instr 6
    ; ...and read by another.
    iG1 = G[1]
    assert(iG1 == 42)
endin
</CsInstruments>
<CsScore>
i 1 0 0.001
i 2 0.001 0.001
i 3 0.01 [4 * 32 / 48000]
i 4 0.02 0.001
i 5 0.03 0.001
i 6 0.04 0.001
e
</CsScore>
</CsoundSynthesizer>
