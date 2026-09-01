<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csninsert.csd
;
; csninsert writes into its source rather than publishing a new handle. Three
; arguments insert one element at a flat index; four insert a block along an axis.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    vec:CsnArr    = csnfromarray(array(1, 2, 3, 4))
    csninsert(vec, 99, 2)
    vec_out:i[]   = csntoarray(vec)
    n:i           = csnsize(vec)
    prints("one element, n = %d: %g %g %g %g %g\n", n, vec_out[0], vec_out[1], vec_out[2], vec_out[3], vec_out[4])

    ; a whole row into a matrix
    shape:i[]     = fillarray(3, 2)
    mat:CsnArr    = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6)), shape)
    row:CsnArr    = csnfromarray(array(7, 8))
    csninsert(mat, row, 0, 1)
    mat_shape:i[] = csnshape(mat)
    mat_out:i[]   = csntoarray(csnflatten(mat))
    prints("block: %g x %g, flat = %g %g %g %g %g %g %g %g\n", mat_shape[0], mat_shape[1], mat_out[0], mat_out[1], mat_out[2], mat_out[3], mat_out[4], mat_out[5], mat_out[6], mat_out[7])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
