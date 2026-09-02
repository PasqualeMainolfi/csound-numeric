<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>

<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

/* The bridge is exercised at performance time and checked at i-time from a
   later note: the values travel through k-rate globals, which read back at
   init as whatever the producing notes left behind. */
gkRoundTrip init -1
gkScaled    init -1
gkPack1     init -1
gkPackCh0   init -1
gkPackCh1   init -1
gkPackCh2   init -1
gkStride0   init -1
gkStride1   init -1
gkStride2   init -1

/* csnfromaudio -> csntoaudio is the identity, sample for sample: no
   resampling, no windowing, nothing that could round. */
instr 1
    aSig  oscili 0.5, 440
    A:CsnArr = csnfromaudio(aSig)
    aOut  csntoaudio A
    aDiff = abs(aOut - aSig)
    kMax  maxk aDiff, 1, 1
    gkRoundTrip = kMax
endin

/* A k-rate opcode in the middle must not disturb the identity. The multiplier
   is bound to a k variable on purpose: with a bare constant Csound picks the
   i-rate overload, which computes once at init and would freeze the result. */
instr 2
    kTwo init 2
    aSig  oscili 0.5, 440
    A:CsnArr = csnfromaudio(aSig)
    B:CsnArr = csnmul(A, kTwo)
    aOut  csntoaudio B
    aDiff = abs(aOut - 2 * aSig)
    kMax  maxk aDiff, 1, 1
    gkScaled = kMax
endin

/* csnpack -> csnunpack, single channel. */
instr 3
    aIn[] init 1
    aIn[0] oscili 0.5, 440
    A:CsnArr = csnpack(aIn)
    aOut[] csnunpack A
    aDiff = abs(aOut[0] - aIn[0])
    kMax  maxk aDiff, 1, 1
    gkPack1 = kMax
endin

/* Three channels carrying distinct DC levels. An a[] stores each channel as a
   whole ksmps-long block, so a wrong per-channel stride scrambles the
   channels into one another instead of failing loudly. */
instr 4
    aIn[] init 3
    aIn[0] = 0.1
    aIn[1] = 0.2
    aIn[2] = 0.3
    A:CsnArr = csnpack(aIn)
    aOut[] csnunpack A
    gkPackCh0 downsamp aOut[0]
    gkPackCh1 downsamp aOut[1]
    gkPackCh2 downsamp aOut[2]
endin

/* csnunpack on an array built without csnpack: 0..95 laid out as 3 x 32, so
   channel i must start exactly at i * ksmps. */
instr 5
    iShape[] fillarray 3, 32
    R:CsnArr = csnarange(0, 96, 1)
    M:CsnArr = csnreshape(R, iShape)
    aOut[] csnunpack M
    gkStride0 downsamp aOut[0]
    gkStride1 downsamp aOut[1]
    gkStride2 downsamp aOut[2]
endin

instr 100
    /* assert only reads i-rate operands: the k-rate globals are pulled across
       with i() first, or the comparison inside assert evaluates as 0. */
    iRoundTrip = i(gkRoundTrip)
    iScaled    = i(gkScaled)
    iPack1     = i(gkPack1)
    iCh0       = i(gkPackCh0)
    iCh1       = i(gkPackCh1)
    iCh2       = i(gkPackCh2)
    iSt0       = i(gkStride0)
    iSt1       = i(gkStride1)
    iSt2       = i(gkStride2)

    assert(iRoundTrip == 0)
    assert(iScaled == 0)
    assert(iPack1 == 0)
    assert(iCh0 == 0.1 && iCh1 == 0.2 && iCh2 == 0.3)
    assert(iSt0 == 0 && iSt1 == 32 && iSt2 == 64)
    prints("csnum audio bridge regression passed\n")
endin
</CsInstruments>

<CsScore>
i 1 0.0 0.02
i 2 0.0 0.02
i 3 0.0 0.02
i 4 0.0 0.02
i 5 0.0 0.02
i 100 0.05 0.01
e
</CsScore>
</CsoundSynthesizer>
