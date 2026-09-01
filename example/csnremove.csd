<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnremove.csd
;
; Two arguments take one element out of the source and hand it back. Three drop
; a slice along an axis and publish a new array, leaving the source alone.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    vec:CsnArr     = csnfromarray(array(10, 20, 30, 40))
    gone:i         = csnremove(vec, 1)
    vec_out:i[]    = csntoarray(vec)
    n:i            = csnsize(vec)
    prints("removed %g, source n = %d: %g %g %g\n", gone, n, vec_out[0], vec_out[1], vec_out[2])

    ; a row out of a matrix: a new array, the source untouched
    shape:i[]      = fillarray(3, 2)
    mat:CsnArr     = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6)), shape)
    less:CsnArr    = csnremove(mat, 0, 1)
    less_shape:i[] = csnshape(less)
    less_out:i[]   = csntoarray(csnflatten(less))
    mat_size:i     = csnsize(mat)
    prints("block: %g x %g = %g %g %g %g, source size still %d\n", less_shape[0], less_shape[1], less_out[0], less_out[1], less_out[2], less_out[3], mat_size)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
