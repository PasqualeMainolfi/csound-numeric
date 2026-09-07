<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>
sr = 48000
ksmps = 32
0dbfs = 1

instr 1
    shape:i[] = fillarray(2, 4)
    matrix:CsnArr = csnreshape(csnfromarray(array(1, 0, 0, 0, 0, 0, 0, 0)), shape)
    spectrum:CsnArr = csnfft2(matrix, 2, 4)
    prints("FFT2: type = %d, elements = %d\n", csntype(spectrum), csnsize(spectrum))
    csnprint spectrum
    turnoff
endin
</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
