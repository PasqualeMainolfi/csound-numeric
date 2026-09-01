<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>

<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

Zeros@global:CsnArr = csnzeros(array(1))
Ones@global:CsnArr = csnones(array(1))
Like@global:CsnArr = csnlike(Zeros, 0)
ZerosAlias@global:CsnArr = csnzeros(array(1))
OnesAlias@global:CsnArr = csnones(array(1))
LikeAlias@global:CsnArr = csnzeros(array(1))
Arange@global:CsnArr = csnzeros(array(1))
Bartlett@global:CsnArr = csnzeros(array(1))

/* itype is an i-argument, so a handle keeps its element type for the whole
   note. The complex constructors get their own slots instead. */
ZerosC@global:CsnArr = csnzeros(array(1), 1)
OnesC@global:CsnArr = csnones(array(1), 1)
LikeC@global:CsnArr = csnlike(ZerosC, 0)

instr 1
    kShape[] = init(1)
    kElapsed = timeinsts()
    kShape[0] = (kElapsed < 0.04 ? 2 : 3)
    kFill = (kElapsed < 0.025 ? 5 : 7)
    kStart = 0
    kStop = 1
    kStep = 0.25
    kLength = 8
    kTrig = 1

    Zeros = csnzeros(kShape, 0)
    Ones = csnones(kShape, 0)
    Like = csnlike(Zeros, kFill)

    ZerosC = csnzeros(kShape, 1)
    OnesC = csnones(kShape, 1)
    LikeC = csnlike(ZerosC, kFill)
    Arange = csnarange(kStart, kStop, kStep, kTrig)
    Bartlett = csnbartlett(kLength)
endin

instr 2
    iIndex0[] = array(0)
    assert(csnsize(Zeros) == 2 && csnsize(Ones) == 2 && csnsize(Like) == 2)
    assert(csnget(Zeros, iIndex0) == 0)
    assert(csnget(Ones, iIndex0) == 1)
    assert(csnget(Like, iIndex0) == 5)

    iIndex1[] = array(1)
    iIndex3[] = array(3)
    iIndex4[] = array(4)
    assert(csnsize(Arange) == 4)
    assert(csnget(Arange, iIndex1) == 0.25 && csnget(Arange, iIndex3) == 0.75)
    assert(abs(csnget(Bartlett, iIndex3) - 0.857142857142857) < 1e-12)
    assert(abs(csnget(Bartlett, iIndex4) - 0.857142857142857) < 1e-12)

    ZerosAlias = Zeros
    OnesAlias = Ones
    LikeAlias = Like
    csnset(Zeros, iIndex0, 91)
    csnset(Ones, iIndex0, 92)
    csnset(Like, iIndex0, 93)
endin

instr 3
    ; Constructors must regenerate content even when their layout is unchanged.
    iIndex0[] = array(0)
    assert(csnget(ZerosAlias, iIndex0) == 0)
    assert(csnget(OnesAlias, iIndex0) == 1)
    assert(csnget(LikeAlias, iIndex0) == 5)
endin

instr 4
    ; A changed fill value must not require a layout change.
    iIndex1[] = array(1)
    assert(csnsize(Like) == 2 && csnsize(LikeAlias) == 2)
    assert(csnget(Like, iIndex1) == 7 && csnget(LikeAlias, iIndex1) == 7)
endin

instr 5
    ; Shape changes update the existing slots observed through all aliases.
    iIndex2[] = array(2)
    assert(csnsize(ZerosAlias) == 3 && csnsize(OnesAlias) == 3 && csnsize(LikeAlias) == 3)
    assert(csnget(ZerosAlias, iIndex2) == 0)
    assert(csnget(OnesAlias, iIndex2) == 1)
    assert(csnget(LikeAlias, iIndex2) == 7)
endin

instr 6
    ; Complex constructors driven at k-rate fill both lanes.
    iIndex0[] = array(0)
    Z:Complex = csnget(ZerosC, iIndex0)
    O:Complex = csnget(OnesC, iIndex0)
    L:Complex = csnget(LikeC, iIndex0)
    iZReal = real(Z)
    iZImag = imag(Z)
    iOReal = real(O)
    iOImag = imag(O)
    iLReal = real(L)
    iLImag = imag(L)
    /* The complex constructors stay complex and the real ones stay real: the
       element type is fixed at init and no k pass can flip it. */
    assert(csntype(ZerosC) == 1 && csntype(OnesC) == 1 && csntype(LikeC) == 1)
    assert(csntype(Zeros) == 0 && csntype(Ones) == 0 && csntype(Like) == 0)
    assert(iZReal == 0 && iZImag == 0)
    assert(iOReal == 1 && iOImag == 0)
    assert(iLReal == 7 && iLImag == 0)
endin
</CsInstruments>

<CsScore>
i 1 0 0.075
i 2 0.01 0.001
i 3 0.015 0.001
i 4 0.03 0.001
i 5 0.045 0.001
i 6 0.06 0.001
e
</CsScore>
</CsoundSynthesizer>
