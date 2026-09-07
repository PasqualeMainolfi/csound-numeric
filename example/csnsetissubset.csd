<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>
sr = 48000
ksmps = 32
0dbfs = 1

instr 1
    small:CsnArr = csnlikeset(csnfromarray(array(2, 3)))
    large:CsnArr = csnlikeset(csnfromarray(array(1, 2, 3, 4)))
    prints("small subset of large = %d\n", csnsetissubset(small, large))
    turnoff
endin
</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
