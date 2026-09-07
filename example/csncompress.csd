<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csncompress.csd
;
; Selecting rows out of a matrix, then the same mask read flat over a vector.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    vec:CsnArr    = csnfromarray(array(10, 20, 30, 40))
    keep:CsnArr   = csnfromarray(array(1, 0, 1, 1))

    kept:CsnArr   = csncompress(vec, keep)
    kept_out:i[]  = csntoarray(kept)
    prints("kept = %g %g %g\n", kept_out[0], kept_out[1], kept_out[2])

    ; along axis 0 of a 2x2, the mask picks rows
    shape:i[]     = fillarray(2, 2)
    mat:CsnArr    = csnreshape(vec, shape)
    rows:CsnArr   = csnfromarray(array(0, 1))
    row:CsnArr    = csncompress(mat, rows, 0)
    flat:CsnArr   = csnflatten(row)
    row_out:i[]   = csntoarray(flat)
    prints("row  = %g %g\n", row_out[0], row_out[1])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
