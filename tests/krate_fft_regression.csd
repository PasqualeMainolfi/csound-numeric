<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>

<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

giOneValues[] = fillarray(1, 2, 3, 4, 5, 6, 7, 8)
OneSrc@global:CsnArr = csnfromarray(giOneValues)
OneSpec@global:CsnArr = csnzeros(array(5), 1)
OneBack@global:CsnArr = csnzeros(array(8))

giTwoShape[] = fillarray(2, 4)
TwoSrc@global:CsnArr = csnreshape(csnfromarray(giOneValues), giTwoShape)
TwoSpec@global:CsnArr = csnzeros(array(4, 3), 1)
TwoBack@global:CsnArr = csnzeros(array(4, 4))

giStftValues[] = fillarray(1, 2, 3, 4, 5, 6, 7, 8,
                           9, 10, 11, 12, 13, 14, 15, 16)
StftSrc@global:CsnArr = csnfromarray(giStftValues)
StftFreq@global:CsnArr = csnzeros(array(5))
StftFramesT@global:CsnArr = csnzeros(array(3))
StftFrames@global:CsnArr = csnzeros(array(5, 3), 1)
StftSamplesT@global:CsnArr = csnzeros(array(16))
StftBack@global:CsnArr = csnzeros(array(16))

instr 1
    kTrig init 1
    OneSpec = csnrfft(OneSrc, 8, kTrig)
    OneBack = csnirfft(OneSpec, 8, kTrig)

    TwoSpec = csnrfft2(TwoSrc, 4, 4, kTrig)
    TwoBack = csnirfft2(TwoSpec, 4, 4, kTrig)

    StftFreq, StftFramesT, StftFrames = csnstft(StftSrc, 8, 4, 48000, 2, kTrig)
    StftSamplesT, StftBack = csnistft(StftFrames, 8, 4, 48000, 2, kTrig)
endin

instr 2
    i0[] = fillarray(0)
    i31[] = fillarray(3, 1)
    i19[] = fillarray(19)
    iTallShape[] = fillarray(4, 2)
    iLongShape[] = fillarray(20)
    csnset OneSrc, i0, 101
    csnreshape TwoSrc, iTallShape
    csnset TwoSrc, i31, 202
    csnresize StftSrc, iLongShape
    csnset StftSrc, i0, 303
    csnset StftSrc, i19, 404
endin

instr 10
    i0[] = fillarray(0)
    i1[] = fillarray(1)
    i31[] = fillarray(3, 1)
    i19[] = fillarray(19)

    iOne = csnget(OneBack, i0)
    iTwo = csnget(TwoBack, i31)
    iStft0 = csnget(StftBack, i0)
    iStft1 = csnget(StftBack, i1)
    iStft19 = csnget(StftBack, i19)
    assert(abs(iOne - 101) < 1e-9)
    assert(abs(iTwo - 202) < 1e-9)
    assert(abs(iStft0 - 303) < 1e-9)
    assert(abs(iStft1 - 2) < 1e-9)
    assert(csnsize(StftBack) == 20)
    assert(abs(iStft19 - 404) < 1e-9)
endin
</CsInstruments>

<CsScore>
i1 0 0.3
i2 0.05 0
i10 0.2 0
</CsScore>
</CsoundSynthesizer>
