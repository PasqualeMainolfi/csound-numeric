<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csncartosph.csd
;
; Cartesian to spherical, answering (r, theta, phi) in the order csnsphtocar
; reads them back: theta the inclination from +z in [0, pi], phi the azimuth
; from +x in (-pi, pi]. The origin has no direction, so r = 0 is an error rather
; than an arbitrary pair of angles.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    ; a point on each axis
    ir1, it1, ip1 csncartosph 1, 0, 0
    prints("(1, 0, 0) -> r %.4f theta %.4f phi %.4f\n", ir1, it1, ip1)
    ir2, it2, ip2 csncartosph 0, 0, 1
    prints("(0, 0, 1) -> r %.4f theta %.4f phi %.4f\n", ir2, it2, ip2)
    ir3, it3, ip3 csncartosph 0, 0, -1
    prints("(0, 0,-1) -> r %.4f theta %.4f phi %.4f\n", ir3, it3, ip3)

    ; an arbitrary point, and the trip back
    ir, it, ip csncartosph 1, 2, 3
    prints("(1, 2, 3) -> r %.4f theta %.4f phi %.4f\n", ir, it, ip)
    ibx, iby, ibz csnsphtocar ir, it, ip
    prints("          -> back to %.4f %.4f %.4f\n", ibx, iby, ibz)

    ; the same in degrees, which reads better for a listening position
    idr, idt, idp csncartosph 1, 2, 3, 1
    prints("in degrees: r %.4f theta %.2f phi %.2f\n", idr, idt, idp)

    ; a magnitude no sum of squares could hold: hypot carries it
    ibr, ibt, ibp csncartosph 1e200, 1e200, 0
    prints("(1e200, 1e200, 0) -> r %g\n", ibr)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
