<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csndims.csd
;
; csndims is the rank. It bounds the legal axis arguments and fixes how many
; coordinates csnget needs.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    vec:CsnArr     = csnfromarray(array(1, 2, 3, 4, 5, 6))
    vec_dims:i     = csndims(vec)

    mat_shape:i[]  = fillarray(2, 3)
    mat:CsnArr     = csnreshape(vec, mat_shape)
    mat_dims:i     = csndims(mat)

    cube_shape:i[] = fillarray(2, 3, 1)
    cube:CsnArr    = csnreshape(vec, cube_shape)
    cube_dims:i    = csndims(cube)

    prints("vector = %d, matrix = %d, rank 3 = %d\n", vec_dims, mat_dims, cube_dims)

    ; the rank fixes how many coordinates csnget takes
    cell:i[] = fillarray(1, 2)
    value:i  = csnget(mat, cell)
    prints("mat[1][2] = %g\n", value)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
