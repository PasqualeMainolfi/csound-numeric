<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csngeomspace.csd
;
; csngeomspace fixes the two endpoints and finds the constant ratio between
; them. Five points from 100 Hz to 1600 Hz is four octaves, one per step.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    geo:CsnArr   = csngeomspace(100, 1600, 5)
    geo_out:i[]  = csntoarray(geo)
    prints("values = %g %g %g %g %g\n", geo_out[0], geo_out[1], geo_out[2], geo_out[3], geo_out[4])

    ratio:i      = geo_out[1] / geo_out[0]
    prints("ratio  = %g\n", ratio)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
