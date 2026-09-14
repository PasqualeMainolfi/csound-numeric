<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>
sr = 48000
ksmps = 32
0dbfs = 1

instr 1
    values:i[] = fillarray(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
                           13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24)
    shape:i[] = fillarray(4, 6)
    source:CsnArr = csnreshape(csnfromarray(values), shape)
    analytic:CsnArr = csnhilbert2(source)
    csnprint csnimag(analytic)
    turnoff
endin
</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
