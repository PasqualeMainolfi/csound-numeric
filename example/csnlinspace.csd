<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnlinspace.csd
;
; csnlinspace gives a fixed number of points between two bounds, endpoint
; included. It is the usual way to build an axis for csninterp.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    ramp:CsnArr   = csnlinspace(0, 1, 5)
    ramp_out:i[]  = csntoarray(ramp)
    prints("ramp  = %g %g %g %g %g\n", ramp_out[0], ramp_out[1], ramp_out[2], ramp_out[3], ramp_out[4])

    ; a half period of a sine, sampled at 5 points
    phase:CsnArr  = csnlinspace(0, 3.14159265358979, 5)
    wave:CsnArr   = csnsin(phase)
    wave_out:i[]  = csntoarray(wave)
    prints("sin   = %.3f %.3f %.3f %.3f %.3f\n", wave_out[0], wave_out[1], wave_out[2], wave_out[3], wave_out[4])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
