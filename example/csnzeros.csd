<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnzeros.csd
;
; csnzeros reserves a shape and publishes every element at zero. The shape is an
; ordinary Csound array, so the same call builds a vector or a matrix.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    vec_shape:i[] = fillarray(4)
    vec:CsnArr    = csnzeros(vec_shape)
    vec_out:i[]   = csntoarray(vec)
    prints("vector = %g %g %g %g\n", vec_out[0], vec_out[1], vec_out[2], vec_out[3])

    mat_shape:i[] = fillarray(2, 3)
    mat:CsnArr    = csnzeros(mat_shape)
    size:i        = csnsize(mat)
    dims:i        = csndims(mat)
    prints("matrix size = %d, dims = %d\n", size, dims)

    ; the same shape, complex this time
    cpx:CsnArr    = csnzeros(vec_shape, 1)
    itype:i       = csntype(cpx)
    prints("complex itype = %d\n", itype)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
