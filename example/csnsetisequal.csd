<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>
sr = 48000
ksmps = 32
0dbfs = 1

instr 1
    a:CsnArr = csnlikeset(csnfromarray(array(3, 1, 2, 2)))
    b:CsnArr = csnlikeset(csnfromarray(array(2, 3, 1)))
    prints("same members = %d\n", csnsetisequal(a, b))
    turnoff
endin
</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
