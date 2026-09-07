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
    matrix:CsnArr = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6, 7, 8)), shape)
    spectrum:CsnArr = csnfft2(matrix, 2, 4)
    restored:CsnArr = csnifft2(spectrum, 2, 4)
    realpart:CsnArr = csnreal(restored)
    prints("IFFT2 type = %d\n", csntype(restored))
    csnprint realpart
    turnoff
endin
</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
