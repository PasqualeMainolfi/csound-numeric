<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csncartocyl.csd
;
; Cartesian to cylindrical: (r, phi, z), the polar pair in the xy plane with the
; height carried through untouched. Unlike csncartosph it never has to divide by
; the magnitude, so the origin is not a special case.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    ir1, ip1, iz1 csncartocyl 3, 4, 7
    prints("(3, 4, 7)  -> r %.4f phi %.4f z %.4f\n", ir1, ip1, iz1)

    ; the height is the one component that passes through unchanged
    ir2, ip2, iz2 csncartocyl 3, 4, -2
    prints("(3, 4, -2) -> r %.4f phi %.4f z %.4f\n", ir2, ip2, iz2)

    ; on the axis the radius vanishes but the height does not, and nothing
    ; is undefined: csncartosph would refuse only the origin itself
    ir3, ip3, iz3 csncartocyl 0, 0, 5
    prints("(0, 0, 5)  -> r %.4f phi %.4f z %.4f\n", ir3, ip3, iz3)

    ; in degrees, and back
    ir, ip, iz csncartocyl 1, 1, 3, 1
    ibx, iby, ibz csncyltocar ir, ip, iz, 1
    prints("(1, 1, 3) -> phi %.2f deg -> back to %.4f %.4f %.4f\n", ip, ibx, iby, ibz)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
