<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnwrap.csd
;
; The range is centred on zero, so a 2*pi period brings an angle back into
; [-pi, pi) rather than [0, 2*pi).
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    two_pi:i      = 6.283185307179586
    phase:CsnArr  = csnfromarray(array(-4, -1, 0, 1, 4, 7))

    wrapped:CsnArr = csnwrap(phase, two_pi)
    wrapped_out:i[] = csntoarray(wrapped)
    prints("period 2*pi = %.4f %.4f %.4f %.4f %.4f %.4f\n", wrapped_out[0], wrapped_out[1], wrapped_out[2], wrapped_out[3], wrapped_out[4], wrapped_out[5])

    ; any period works, not only an angular one
    four:CsnArr   = csnwrap(phase, 4)
    four_out:i[]  = csntoarray(four)
    prints("period 4    = %g %g %g %g %g %g\n", four_out[0], four_out[1], four_out[2], four_out[3], four_out[4], four_out[5])

    ; in place
    csnwrap(phase, two_pi)
    now:i[]       = csntoarray(phase)
    prints("in place    = %.4f %.4f\n", now[0], now[5])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
