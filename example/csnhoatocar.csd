<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnhoatocar.csd
;
; The ambisonics description of a direction back to cartesian coordinates:
;
;   x = distance * cos(el) * cos(az)
;   y = distance * cos(el) * sin(az)
;   z = distance * sin(el)
;
; The frame is the AmbiX one, +x front, +y left, +z up. A positive azimuth
; turns towards the left, a positive elevation lifts off the horizontal plane,
; and a distance of 1 gives the unit direction vector.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    ipi2 = 1.5707963267948966

    ; one direction per axis, at unit distance
    ix1, iy1, iz1 csnhoatocar 1, 0, 0
    prints("az 0     el 0    -> %.4f %.4f %.4f  front\n", ix1, iy1, iz1)
    ix2, iy2, iz2 csnhoatocar 1, ipi2, 0
    prints("az pi/2  el 0    -> %.4f %.4f %.4f  left\n", ix2, iy2, iz2)
    ix3, iy3, iz3 csnhoatocar 1, 0, ipi2
    prints("az 0     el pi/2 -> %.4f %.4f %.4f  up\n", ix3, iy3, iz3)

    ; the distance scales the direction, it does not bend it
    ix4, iy4, iz4 csnhoatocar 4, ipi2, 0
    prints("distance 4, left -> %.4f %.4f %.4f\n", ix4, iy4, iz4)

    ; degrees, which is how a listening position is usually written down
    ix5, iy5, iz5 csnhoatocar 1, 45, 30, 1
    prints("az 45 el 30 deg  -> %.4f %.4f %.4f\n", ix5, iy5, iz5)

    ; and the trip back through csncartohoa
    id, ia, ie csncartohoa ix5, iy5, iz5, 1
    prints("                 -> back to az %.2f el %.2f at %.4f\n", ia, ie, id)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
