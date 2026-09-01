<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csngrad.csd
;
; Same length in, same length out. That is what separates it from csndiff, and
; what lets the gradient be lined up with the data it came from.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr   = csnfromarray(array(1, 4, 9, 16))

    slope:CsnArr  = csngrad(data)
    slope_out:i[] = csntoarray(slope)
    slope_n:i     = csnsize(slope)
    prints("grad n = %d : %g %g %g %g\n", slope_n, slope_out[0], slope_out[1], slope_out[2], slope_out[3])

    ; csndiff answers the same question one element shorter
    steps:CsnArr  = csndiff(data)
    steps_n:i     = csnsize(steps)
    prints("diff n = %d\n", steps_n)

    ; the gradient of a straight line is constant
    ramp:CsnArr   = csnlinspace(0, 4, 5)
    flat:CsnArr   = csngrad(ramp)
    flat_out:i[]  = csntoarray(flat)
    prints("gradient of a ramp = %g %g %g %g %g\n", flat_out[0], flat_out[1], flat_out[2], flat_out[3], flat_out[4])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
