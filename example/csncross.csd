<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csncross.csd
;
; Perpendicular to both, right-hand rule, length equal to the area they span.
; Zero when the two are parallel.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    x:CsnArr     = csnfromarray(array(1, 0, 0))
    y:CsnArr     = csnfromarray(array(0, 1, 0))

    z:CsnArr     = csncross(x, y)
    z_out:i[]    = csntoarray(z)
    prints("x cross y = %g %g %g\n", z_out[0], z_out[1], z_out[2])

    ; the order matters: swapping them flips the result
    back:CsnArr  = csncross(y, x)
    back_out:i[] = csntoarray(back)
    prints("y cross x = %g %g %g\n", back_out[0], back_out[1], back_out[2])

    ; parallel vectors give zero
    twice:CsnArr = csnmul(x, 3)
    none:CsnArr  = csncross(x, twice)
    none_out:i[] = csntoarray(none)
    prints("parallel  = %g %g %g\n", none_out[0], none_out[1], none_out[2])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
