<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnpow.csd
;
; The scalar on the right is a fixed exponent, which is how curves are shaped;
; the scalar on the left is a fixed base, which is an exponential.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    base:CsnArr    = csnfromarray(array(1, 2, 3, 4))
    expo:CsnArr    = csnfromarray(array(2, 2, 3, 0.5))

    both:CsnArr    = csnpow(base, expo)
    both_out:i[]   = csntoarray(both)
    prints("array pow array = %g %g %g %g\n", both_out[0], both_out[1], both_out[2], both_out[3])

    ; a fixed exponent shapes a curve
    ramp:CsnArr    = csnlinspace(0, 1, 5)
    curve:CsnArr   = csnpow(ramp, 2)
    curve_out:i[]  = csntoarray(curve)
    prints("squared ramp    = %g %g %g %g %g\n", curve_out[0], curve_out[1], curve_out[2], curve_out[3], curve_out[4])

    ; a fixed base is an exponential
    steps:CsnArr   = csnarange(0, 5, 1)
    powers:CsnArr  = csnpow(2, steps)
    powers_out:i[] = csntoarray(powers)
    prints("2 pow array     = %g %g %g %g %g\n", powers_out[0], powers_out[1], powers_out[2], powers_out[3], powers_out[4])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
