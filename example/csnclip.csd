<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnclip.csd
;
; csnclip is the guard in front of a restricted domain: keep a divisor off zero,
; keep an arcsine argument inside [-1, 1].
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr   = csnfromarray(array(-2, -0.5, 0.5, 3))

    unit:CsnArr   = csnclip(data, 0, 1)
    unit_out:i[]  = csntoarray(unit)
    prints("clipped to [0, 1]  = %g %g %g %g\n", unit_out[0], unit_out[1], unit_out[2], unit_out[3])

    ; keep an arcsine argument inside its domain
    safe:CsnArr   = csnclip(data, -1, 1)
    angle:CsnArr  = csnasin(safe)
    angle_out:i[] = csntoarray(angle)
    prints("asin of clamped    = %.4f %.4f %.4f %.4f\n", angle_out[0], angle_out[1], angle_out[2], angle_out[3])

    ; in place
    csnclip(data, -1, 1)
    now:i[]       = csntoarray(data)
    prints("in place           = %g %g %g %g\n", now[0], now[1], now[2], now[3])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
