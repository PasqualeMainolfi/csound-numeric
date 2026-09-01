<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnsubtract.csd
;
; Both scalar orders exist because subtraction is not commutative. The scalar
; on the left is also how an array is negated.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    a:CsnArr        = csnfromarray(array(10, 20, 30))
    b:CsnArr        = csnfromarray(array(1, 2, 3))

    diff:CsnArr     = csnsubtract(a, b)
    diff_out:i[]    = csntoarray(diff)
    prints("array - array  = %g %g %g\n", diff_out[0], diff_out[1], diff_out[2])

    less:CsnArr     = csnsubtract(a, 5)
    less_out:i[]    = csntoarray(less)
    prints("array - scalar = %g %g %g\n", less_out[0], less_out[1], less_out[2])

    from:CsnArr     = csnsubtract(100, a)
    from_out:i[]    = csntoarray(from)
    prints("scalar - array = %g %g %g\n", from_out[0], from_out[1], from_out[2])

    ; negation
    neg:CsnArr      = csnsubtract(0, a)
    neg_out:i[]     = csntoarray(neg)
    prints("negated        = %g %g %g\n", neg_out[0], neg_out[1], neg_out[2])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
