<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnsub.csd
;
; Seeded from the first element, then every later one is taken off it. Undefined
; over an empty extent, unlike csnsum.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    vec:CsnArr    = csnfromarray(array(10, 1, 2, 3))
    whole:i       = csnsub(vec)
    prints("10-1-2-3  = %g\n", whole)

    ; along an axis: one value per column, then one per row
    shape:i[]     = fillarray(2, 3)
    mat:CsnArr    = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6)), shape)

    cols:CsnArr   = csnsub(mat, 0)
    cols_out:i[]  = csntoarray(cols)
    prints("per column = %g %g %g\n", cols_out[0], cols_out[1], cols_out[2])

    rows:CsnArr   = csnsub(mat, 1)
    rows_out:i[]  = csntoarray(rows)
    prints("per row    = %g %g\n", rows_out[0], rows_out[1])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
