<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>
sr = 48000
ksmps = 32
0dbfs = 1

instr 1
    large:CsnArr = csnlikeset(csnfromarray(array(1, 2, 3, 4)))
    small:CsnArr = csnlikeset(csnfromarray(array(2, 3)))
    prints("large superset of small = %d\n", csnsetissuperset(large, small))
    turnoff
endin
</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
