<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csncartopol.csd
;
; Cartesian to polar, in the plane: r from hypot, phi from atan2 in (-pi, pi].
; The optional trailing 1 asks for the angle in degrees, so a round trip taken
; in degrees comes back in degrees.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    ir1, ip1 csncartopol 3, 4
    prints("(3, 4)   -> r %.4f phi %.4f\n", ir1, ip1)

    ; atan2 keeps the quadrant, which a plain arctangent would lose
    ir2, ip2 csncartopol -3, 4, 1
    prints("(-3, 4)  -> r %.4f phi %.2f deg\n", ir2, ip2)
    ir3, ip3 csncartopol -3, -4, 1
    prints("(-3, -4) -> r %.4f phi %.2f deg\n", ir3, ip3)

    ; the origin is not an error here: r is 0 and the angle is 0 with it
    ir4, ip4 csncartopol 0, 0
    prints("(0, 0)   -> r %.4f phi %.4f\n", ir4, ip4)

    ; round trip in degrees
    ir, ip csncartopol 1, 1, 1
    ibx, iby csnpoltocar ir, ip, 1
    prints("(1, 1) -> r %.4f phi %.2f -> back to %.4f %.4f\n", ir, ip, ibx, iby)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
