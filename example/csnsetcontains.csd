<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>
sr = 48000
ksmps = 32
0dbfs = 1

instr 1
    values:CsnArr = csnlikeset(csnfromarray(array(10, 20, 30)))
    prints("contains 20 = %d\n", csnsetcontains(values, 20))
    prints("contains 25 = %d\n", csnsetcontains(values, 25))
    turnoff
endin
</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
