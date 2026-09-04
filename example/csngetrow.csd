<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csngetrow.csd
;
; csngetrow extracts one zero-based row from a two-dimensional array and
; returns it as a one-dimensional array.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    shape:i[]  = fillarray(2, 3)
    matrix:CsnArr = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6)), shape)
    row:CsnArr = csngetrow(matrix, 1)

    prints("row 1:\n")
    csnprint(row)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
