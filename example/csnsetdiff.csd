<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>
sr = 48000
ksmps = 32
0dbfs = 1

instr 1
    a:CsnArr = csnlikeset(csnfromarray(array(1, 2, 3, 4)))
    b:CsnArr = csnlikeset(csnfromarray(array(2, 4, 6)))
    result:CsnArr = csnsetdiff(a, b)
    csnprint result ; [1, 3]
    turnoff
endin
</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
