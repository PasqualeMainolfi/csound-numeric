<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnnorm.csd
;
; Order 1 is the sum of magnitudes, order 2 the Euclidean length. In the array
; form the axis comes first and the order after it.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    vec:CsnArr   = csnfromarray(array(1, 2, 3))

    l1:i         = csnnorm(vec)
    l2:i         = csnnorm(vec, 2)
    l4:i         = csnnorm(vec, 4)
    prints("order 1 = %g, order 2 = %.4f, order 4 = %.4f\n", l1, l2, l4)

    ; order 1 is the sum of magnitudes the long way
    by_sum:i     = csnsum(csnabs(vec))
    prints("sum of magnitudes = %g\n", by_sum)

    ; per row of a matrix: axis first, then order
    shape:i[]    = fillarray(2, 3)
    mat:CsnArr   = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6)), shape)
    rows:CsnArr  = csnnorm(mat, 1, 2)
    rows_out:i[] = csntoarray(rows)
    prints("row lengths = %.4f %.4f\n", rows_out[0], rows_out[1])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
