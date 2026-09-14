<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>
sr = 48000
ksmps = 32
0dbfs = 1

instr 1
    source:CsnArr = csnfromarray(array(1, 2, 3, 4, 5, 6, 7, 8))
    shifted:CsnArr = csnhilbert1dr(source)
    csnprint shifted
    turnoff
endin
</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
