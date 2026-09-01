<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csndegtorad.csd
;
; The trigonometric opcodes take radians, so this is the step in front of them
; whenever the data arrives in degrees.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    deg:CsnArr   = csnfromarray(array(0, 90, 180, 270, 360))

    rad:CsnArr   = csndegtorad(deg)
    rad_out:i[]  = csntoarray(rad)
    prints("radians = %.4f %.4f %.4f %.4f %.4f\n", rad_out[0], rad_out[1], rad_out[2], rad_out[3], rad_out[4])

    wave:CsnArr  = csnsin(rad)
    wave_out:i[] = csntoarray(wave)
    prints("sine    = %.4f %.4f %.4f %.4f %.4f\n", wave_out[0], wave_out[1], wave_out[2], wave_out[3], wave_out[4])

    ; in place
    csndegtorad(deg)
    now:i[]      = csntoarray(deg)
    prints("in place = %.4f %.4f\n", now[0], now[1])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
