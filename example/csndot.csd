<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csndot.csd
;
; Two vectors give a number, two matrices give a matrix. The type you declare
; for the output is what picks the overload.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    a:CsnArr      = csnfromarray(array(1, 2, 3))
    b:CsnArr      = csnfromarray(array(4, 5, 6))

    scalar:i      = csndot(a, b)
    prints("scalar product = %g\n", scalar)

    ; matrices: the product is a handle
    shape:i[]     = fillarray(2, 3)
    mat:CsnArr    = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6)), shape)
    matt:CsnArr   = csntranspose(mat)
    prod:CsnArr   = csndot(mat, matt)
    prod_shape:i[] = csnshape(prod)
    prod_out:i[]  = csntoarray(csnflatten(prod))
    prints("matrix product %g x %g = %g %g %g %g\n", prod_shape[0], prod_shape[1], prod_out[0], prod_out[1], prod_out[2], prod_out[3])

    ; the scalar product is also the cosine numerator of csnangledist
    len_a:i       = csnnorm(a, 2)
    len_b:i       = csnnorm(b, 2)
    cos_ab:i      = scalar / (len_a * len_b)
    prints("cosine = %.4f\n", cos_ab)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
