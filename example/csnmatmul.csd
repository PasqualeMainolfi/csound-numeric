<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnmatmul.csd
;
; An n x k times a k x m gives an n x m. Multiplying by the identity leaves a
; matrix alone, which is the quickest check of a chain.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    shape:i[]     = fillarray(2, 3)
    mat:CsnArr    = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6)), shape)
    matt:CsnArr   = csntranspose(mat)

    prod:CsnArr   = csnmatmul(mat, matt)
    prod_shape:i[] = csnshape(prod)
    prod_out:i[]  = csntoarray(csnflatten(prod))
    prints("2x3 times 3x2 = %g x %g : %g %g %g %g\n", prod_shape[0], prod_shape[1], prod_out[0], prod_out[1], prod_out[2], prod_out[3])

    ; the identity is the neutral element
    eye:CsnArr    = csnidentity(3)
    same:CsnArr   = csnmatmul(mat, eye)
    same_out:i[]  = csntoarray(csnflatten(same))
    prints("mat * I = %g %g %g %g %g %g\n", same_out[0], same_out[1], same_out[2], same_out[3], same_out[4], same_out[5])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
