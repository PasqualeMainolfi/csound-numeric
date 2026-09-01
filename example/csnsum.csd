<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnsum.csd
;
; Whole-array first, then per column and per row. The sum over an empty array is
; 0, so an accumulator needs no special first case.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    vec:CsnArr    = csnfromarray(array(1, 2, 3, 4))
    whole:i       = csnsum(vec)
    prints("sum       = %g\n", whole)

    ; along an axis: one value per column, then one per row
    shape:i[]     = fillarray(2, 3)
    mat:CsnArr    = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6)), shape)

    cols:CsnArr   = csnsum(mat, 0)
    cols_out:i[]  = csntoarray(cols)
    prints("per column = %g %g %g\n", cols_out[0], cols_out[1], cols_out[2])

    rows:CsnArr   = csnsum(mat, 1)
    rows_out:i[]  = csntoarray(rows)
    prints("per row    = %g %g\n", rows_out[0], rows_out[1])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
