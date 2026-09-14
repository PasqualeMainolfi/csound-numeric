<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>
sr = 48000
ksmps = 32
0dbfs = 1

instr 1
    matrix:CsnArr = csnhilbertmat(4)
    csnprint matrix
    ; Famously ill-conditioned: the determinant is already near zero at n = 4.
    prints("det = %.6e\n", csndet(matrix))
    turnoff
endin
</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
