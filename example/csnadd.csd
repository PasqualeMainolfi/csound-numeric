<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnadd.csd
;
; Two arrays broadcast NumPy-style, so a 2 x 3 and a 1 x 3 add without a
; reshape. A scalar operand reaches every element.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    a:CsnArr       = csnfromarray(array(1, 2, 3))
    b:CsnArr       = csnfromarray(array(10, 20, 30))

    sum:CsnArr     = csnadd(a, b)
    sum_out:i[]    = csntoarray(sum)
    prints("array + array  = %g %g %g\n", sum_out[0], sum_out[1], sum_out[2])

    biased:CsnArr  = csnadd(a, 0.5)
    biased_out:i[] = csntoarray(biased)
    prints("array + scalar = %g %g %g\n", biased_out[0], biased_out[1], biased_out[2])

    ; broadcasting a row across a matrix
    mat_shape:i[]  = fillarray(2, 3)
    row_shape:i[]  = fillarray(1, 3)
    mat:CsnArr     = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6)), mat_shape)
    row:CsnArr     = csnreshape(b, row_shape)
    wide:CsnArr    = csnadd(mat, row)
    wide_out:i[]   = csntoarray(csnflatten(wide))
    prints("broadcast      = %g %g %g %g %g %g\n", wide_out[0], wide_out[1], wide_out[2], wide_out[3], wide_out[4], wide_out[5])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
