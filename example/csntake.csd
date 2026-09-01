<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csntake.csd
;
; Three arguments drop the axis, so a row of a matrix comes back as a vector.
; Two arguments read the array flat and hand back one number.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    shape:i[]     = fillarray(3, 4)
    mat:CsnArr    = csnreshape(csnarange(0, 12, 1), shape)

    ; row 1: the axis disappears
    row:CsnArr    = csntake(mat, 0, 1)
    row_dims:i    = csndims(row)
    row_out:i[]   = csntoarray(row)
    prints("row 1 (dims %d) = %g %g %g %g\n", row_dims, row_out[0], row_out[1], row_out[2], row_out[3])

    ; column 2, likewise
    col:CsnArr    = csntake(mat, 1, 2)
    col_out:i[]   = csntoarray(col)
    prints("col 2           = %g %g %g\n", col_out[0], col_out[1], col_out[2])

    ; two arguments: one element, read flat
    flat:i        = csntake(mat, 5)
    prints("flat index 5    = %g\n", flat)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
