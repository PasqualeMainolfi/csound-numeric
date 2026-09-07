<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>
sr = 48000
ksmps = 32
0dbfs = 1

instr 1
    a:CsnArr = csnlikeset(csnfromarray(array(1, 3, 5)))
    b:CsnArr = csnlikeset(csnfromarray(array(2, 4, 6)))
    prints("sets are disjoint = %d\n", csnsetisdisjoint(a, b))
    turnoff
endin
</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
