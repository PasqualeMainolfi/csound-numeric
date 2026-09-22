<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csncartohoa.csd
;
; Cartesian to the ambisonics description of a direction: distance, azimuth and
; elevation, in the order csnhoatocar reads them back. The frame is the AmbiX
; one, +x front, +y left, +z up, so a positive azimuth turns towards the left
; and a positive elevation lifts off the horizontal plane.
;
; The elevation is measured from the horizon, not from +z. That is the whole
; reason this pair sits next to csncartosph, whose theta is the inclination:
; the two differ by el = pi/2 - theta, and confusing them mirrors a source
; about the horizontal plane without raising anything.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    ; one direction per axis
    id1, ia1, ie1 csncartohoa 1, 0, 0
    prints("(1, 0, 0) front -> d %.4f az %.4f el %.4f\n", id1, ia1, ie1)
    id2, ia2, ie2 csncartohoa 0, 1, 0
    prints("(0, 1, 0) left  -> d %.4f az %.4f el %.4f\n", id2, ia2, ie2)
    id3, ia3, ie3 csncartohoa 0, 0, 1
    prints("(0, 0, 1) up    -> d %.4f az %.4f el %.4f\n", id3, ia3, ie3)

    ; elevation against the inclination csncartosph answers for the same point
    ir, it, ip csncartosph 0, 0, 1
    prints("up: elevation %.4f, inclination %.4f, sum %.4f = pi/2\n", ie3, it, ie3 + it)

    ; an arbitrary point, and the trip back
    id, ia, ie csncartohoa 3, -4, 5
    prints("(3,-4, 5) -> d %.4f az %.4f el %.4f\n", id, ia, ie)
    ibx, iby, ibz csnhoatocar id, ia, ie
    prints("          -> back to %.4f %.4f %.4f\n", ibx, iby, ibz)

    ; degrees read better for a listening position: both angles convert
    idd, ida, ide csncartohoa 3, -4, 5, 1
    prints("in degrees: d %.4f az %.2f el %.2f\n", idd, ida, ide)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
