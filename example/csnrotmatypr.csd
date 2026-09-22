<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnrotmatypr.csd
;
; One 3 x 3 matrix for a yaw, a pitch and a roll taken together:
;
;   R = Rz(yaw) * Ry(-pitch) * Rx(roll)
;
; applied right to left, so a source is rolled first, then pitched, then yawed.
; Handing back the composed matrix rather than the three factors is the whole
; point: a different order is a different rotation, and composing them by hand
; is where it goes wrong.
;
; The convention is the audio one, in the AmbiX frame (+x front, +y left,
; +z up): a positive yaw turns towards the left and a positive pitch lifts the
; front upwards. That pitch is a rotation about -y, not the right-handed Ry
; that csnrotmat builds, which is why this opcode carries its own name.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

opcode applied(m:CsnArr, v:CsnArr, Sn:S):void
    prod:CsnArr = csnmatmul(m, v)
    flat:CsnArr = csnflatten(prod)
    o:i[]       = csntoarray(flat)
    prints("%-26s -> (%6.3f %6.3f %6.3f)\n", Sn, o[0], o[1], o[2])
endop

instr 1
    ; the source sits at the front, as a 3 x 1 column
    ifront:i[]   = fillarray(1, 0, 0)
    ishape:i[]   = fillarray(3, 1)
    front:CsnArr = csnreshape(csnfromarray(ifront), ishape)

    ; one angle at a time, in degrees
    Ry90:CsnArr = csnrotmatypr(90, 0, 0, 1)
    applied(Ry90, front, "yaw 90")
    Rp90:CsnArr = csnrotmatypr(0, 90, 0, 1)
    applied(Rp90, front, "pitch 90")

    ; all three together: rolled, then pitched, then yawed
    Rall:CsnArr = csnrotmatypr(90, 45, 0, 1)
    applied(Rall, front, "yaw 90 pitch 45")

    ; the order is not a detail. Yawing first and pitching after is a
    ; different rotation, and this is what it would look like
    Mz:CsnArr = csnrotmat(90, 2, 1)
    My:CsnArr = csnrotmat(-45, 1, 1)
    swapped:CsnArr = csnmatmul(My, Mz)
    applied(swapped, front, "pitch after yaw")

    ; a listener turning their head is the inverse rotation: for a rotation
    ; the transpose is the inverse, so no flag is needed to ask for one
    head:CsnArr = csnrotmatypr(30, 0, 0, 1)
    applied(csntranspose(head), front, "listener turned 30 left")

    ; and it reads back as a direction through the ambisonics pair
    turned:CsnArr = csnmatmul(csntranspose(head), front)
    tflat:CsnArr  = csnflatten(turned)
    t:i[]         = csntoarray(tflat)
    idist, iaz, iel csncartohoa t[0], t[1], t[2], 1
    prints("%-26s -> az %.2f el %.2f at %.3f\n", "as a direction", iaz, iel, idist)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
