<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>
sr = 48000
ksmps = 32
0dbfs = 1

instr 1
    values:CsnArr = csnlikeset(csnfromarray(array(1, 3, 5)))
    csnsetinsert values, 4
    csnsetinsert values, 3 ; already present: no change
    csnprint values
    turnoff
endin
</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
