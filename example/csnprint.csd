<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnprint.csd
;
; csnprint writes the shape, element type and NumPy-style array body. Long
; arrays keep the first and last three elements, with an ellipsis between them.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    values:i[] = fillarray(1.234567, 2, 3, 4)
    shape:i[]  = fillarray(2, 2)
    mat:CsnArr = csnreshape(csnfromarray(values), shape)
    csnprint(mat)

    long:CsnArr = csnarange(0, 1001, 1)
    csnprint(long)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
