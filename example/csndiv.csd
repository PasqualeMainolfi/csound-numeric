<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csndiv.csd
;
; Both scalar orders exist; the scalar on the left is how a reciprocal is
; written. A zero divisor is an error, not an infinity.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    a:CsnArr      = csnfromarray(array(10, 20, 30))
    b:CsnArr      = csnfromarray(array(2, 4, 5))

    quot:CsnArr   = csndiv(a, b)
    quot_out:i[]  = csntoarray(quot)
    prints("array / array  = %g %g %g\n", quot_out[0], quot_out[1], quot_out[2])

    half:CsnArr   = csndiv(a, 10)
    half_out:i[]  = csntoarray(half)
    prints("array / scalar = %g %g %g\n", half_out[0], half_out[1], half_out[2])

    recip:CsnArr  = csndiv(1, b)
    recip_out:i[] = csntoarray(recip)
    prints("reciprocal     = %g %g %g\n", recip_out[0], recip_out[1], recip_out[2])

    ; keep a divisor away from zero before dividing by it
    raw:CsnArr    = csnfromarray(array(0, 0.5, 2))
    safe:CsnArr   = csnclip(raw, 0.001, 1000)
    ratio:CsnArr  = csndiv(a, safe)
    ratio_out:i[] = csntoarray(ratio)
    prints("guarded        = %g %g %g\n", ratio_out[0], ratio_out[1], ratio_out[2])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
