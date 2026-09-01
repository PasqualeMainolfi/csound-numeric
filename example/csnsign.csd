<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnsign.csd
;
; Split a signal into sign and magnitude, shape the magnitude, put the sign
; back: csnsign and csnabs are the two halves of that idiom.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr    = csnfromarray(array(-3, 0, 2, -0.5))

    sgn:CsnArr     = csnsign(data)
    sgn_out:i[]    = csntoarray(sgn)
    prints("sign = %g %g %g %g\n", sgn_out[0], sgn_out[1], sgn_out[2], sgn_out[3])

    ; soft-clip the magnitude, then restore the sign
    mag:CsnArr     = csnabs(data)
    shaped:CsnArr  = csntanh(mag)
    signed:CsnArr  = csnmul(shaped, sgn)
    signed_out:i[] = csntoarray(signed)
    prints("shaped = %.4f %.4f %.4f %.4f\n", signed_out[0], signed_out[1], signed_out[2], signed_out[3])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
