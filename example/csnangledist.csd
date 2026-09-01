<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnangledist.csd
;
; Direction only: scaling either operand leaves the answer alone, which is what
; separates it from csndist.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    x:CsnArr      = csnfromarray(array(1, 0, 0))
    y:CsnArr      = csnfromarray(array(0, 1, 0))
    diag:CsnArr   = csnfromarray(array(1, 1, 0))

    perp:i        = csnangledist(x, y)
    half:i        = csnangledist(x, diag)
    same:i        = csnangledist(x, x)
    prints("perpendicular = %.4f, 45 degrees = %.4f, identical = %g\n", perp, half, same)

    ; in degrees
    ang:CsnArr    = csnfromarray(array(perp, half, same))
    deg:CsnArr    = csnradtodeg(ang)
    deg_out:i[]   = csntoarray(deg)
    prints("degrees = %.1f %.1f %.1f\n", deg_out[0], deg_out[1], deg_out[2])

    ; length does not matter
    long_x:CsnArr = csnmul(x, 100)
    scaled:i      = csnangledist(long_x, diag)
    prints("after scaling one operand = %.4f\n", scaled)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
