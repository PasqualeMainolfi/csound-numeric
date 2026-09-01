<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csncbrt.csd
;
; The cube root is defined for negative numbers too, which is what separates it
; from csnsqrt over the reals.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr   = csnfromarray(array(8, 27, -8, -27))

    roots:CsnArr  = csncbrt(data)
    roots_out:i[] = csntoarray(roots)
    prints("cbrt = %g %g %g %g\n", roots_out[0], roots_out[1], roots_out[2], roots_out[3])

    ; no NaN, unlike the square root
    nan_cbrt:i    = csncntnan(roots)
    nan_sqrt:i    = csncntnan(csnsqrt(data))
    prints("NaN from cbrt = %d, from sqrt = %d\n", nan_cbrt, nan_sqrt)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
