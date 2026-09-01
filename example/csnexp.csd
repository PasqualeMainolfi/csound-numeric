<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnexp.csd
;
; csnexp is e^x elementwise. An exponential over another base is csnpow with
; the base on the left.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr   = csnfromarray(array(0, 1, 2, 3))

    exp_a:CsnArr  = csnexp(data)
    exp_out:i[]   = csntoarray(exp_a)
    prints("exp   = %.4f %.4f %.4f %.4f\n", exp_out[0], exp_out[1], exp_out[2], exp_out[3])

    ; and back again through the natural logarithm
    back:CsnArr   = csnlog(exp_a, 2.718281828459045)
    back_out:i[]  = csntoarray(back)
    prints("round trip = %g %g %g %g\n", back_out[0], back_out[1], back_out[2], back_out[3])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
