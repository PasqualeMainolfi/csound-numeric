<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csngetslice.csd
;
; The slice runs along one axis and the rank is preserved. Every other axis
; comes through whole, so a 3 x 4 sliced on axis 1 is still a matrix.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    shape:i[]      = fillarray(3, 4)
    mat:CsnArr     = csnreshape(csnarange(0, 12, 1), shape)

    ; every other column
    even:CsnArr    = csngetslice(mat, 1, 0, 4, 2)
    even_shape:i[] = csnshape(even)
    even_out:i[]   = csntoarray(csnflatten(even))
    prints("axis 1, step 2: %g x %g = %g %g %g %g %g %g\n", even_shape[0], even_shape[1], even_out[0], even_out[1], even_out[2], even_out[3], even_out[4], even_out[5])

    ; the first two rows
    top:CsnArr     = csngetslice(mat, 0, 0, 2, 1)
    top_shape:i[]  = csnshape(top)
    top_out:i[]    = csntoarray(csnflatten(top))
    prints("axis 0, rows 0-1: %g x %g, first = %g, last = %g\n", top_shape[0], top_shape[1], top_out[0], top_out[7])

    ; on a vector it is the plain strided range
    vec:CsnArr     = csnfromarray(array(10, 20, 30, 40, 50, 60))
    odd:CsnArr     = csngetslice(vec, 0, 1, 6, 2)
    odd_out:i[]    = csntoarray(odd)
    prints("vector: %g %g %g\n", odd_out[0], odd_out[1], odd_out[2])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
