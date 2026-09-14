<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>
sr = 48000
ksmps = 32
0dbfs = 1

instr 1
    ; Any length is accepted, powers of two included but not required.
    source:CsnArr = csnfromarray(array(1, 2, 3, 4, 5, 6))
    coeffs:CsnArr = csndstone1d(source)
    csnprint coeffs
    turnoff
endin
</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
