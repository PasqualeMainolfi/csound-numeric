<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csninner.csd
;
; The last axis of both operands is contracted. On vectors that is the scalar
; product; above rank 1 it differs from csndot.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    a:CsnArr      = csnfromarray(array(1, 2, 3))
    b:CsnArr      = csnfromarray(array(4, 5, 6))

    scalar:i      = csninner(a, b)
    same:i        = csndot(a, b)
    prints("inner = %g, dot = %g\n", scalar, same)

    ; two matrices: the last axis of each is contracted
    shape:i[]     = fillarray(2, 3)
    mat:CsnArr    = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6)), shape)
    prod:CsnArr   = csninner(mat, mat)
    prod_shape:i[] = csnshape(prod)
    prod_out:i[]  = csntoarray(csnflatten(prod))
    prints("inner of a 2 x 3 with itself: %g x %g = %g %g %g %g\n", prod_shape[0], prod_shape[1], prod_out[0], prod_out[1], prod_out[2], prod_out[3])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
