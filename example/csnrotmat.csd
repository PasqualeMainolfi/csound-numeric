<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnrotmat.csd
;
; A 3 x 3 rotation matrix, ready for csnmatmul. Two forms share the name: one
; turns about a basis axis picked by index, the other about an arbitrary
; direction held in a CsnArr.
;
; The matrices are the mathematical right-handed ones. In the AmbiX frame
; (+x front, +y left, +z up) that makes a positive rotation about +z take a
; source from the front towards the left, which is what a positive yaw does,
; and a positive rotation about +x take it from the left to up. About +y it
; takes the front downwards: the audio pitch convention is the opposite sign,
; a rotation about -y, and is not what this opcode builds.
;
; To rotate the frame rather than the source, csntranspose gives the inverse:
; for a rotation the transpose is the inverse, so no flag is needed to ask.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

opcode showvec(m:CsnArr, v:CsnArr, Sn:S):void
    prod:CsnArr = csnmatmul(m, v)
    flat:CsnArr = csnflatten(prod)
    o:i[]       = csntoarray(flat)
    prints("%-22s -> (%6.3f %6.3f %6.3f)\n", Sn, o[0], o[1], o[2])
endop

instr 1
    ipi2 = 1.5707963267948966

    ; the source sits at the front, as a 3 x 1 column
    ifront:i[] = fillarray(1, 0, 0)
    ishape:i[] = fillarray(3, 1)
    front:CsnArr = csnreshape(csnfromarray(ifront), ishape)

    Rz:CsnArr = csnrotmat(ipi2, 2)
    showvec(Rz, front, "Rz(90) * front")

    Ry:CsnArr = csnrotmat(ipi2, 1)
    showvec(Ry, front, "Ry(90) * front")

    ; the same rotation asked for in degrees
    Rzd:CsnArr = csnrotmat(90, 2, 1)
    showvec(Rzd, front, "Rz(90 deg) * front")

    ; about an arbitrary direction: the length does not matter, only the
    ; direction, so this is the same rotation as Rz above
    iaxis:i[] = fillarray(0, 0, 7)
    axis:CsnArr = csnfromarray(iaxis)
    Ra:CsnArr = csnrotmat(axis, ipi2)
    showvec(Ra, front, "about (0,0,7)")

    ; composition is a matrix product, applied right to left: Rz turns the
    ; front towards the left, then Rx lifts the left up
    Rx:CsnArr = csnrotmat(ipi2, 0)
    RxRz:CsnArr = csnmatmul(Rx, Rz)
    showvec(RxRz, front, "Rx * Rz * front")

    ; and the inverse rotation is the transpose
    back:CsnArr = csnmatmul(csntranspose(Rz), csnmatmul(Rz, front))
    showvec(csnidentity(3), back, "Rz transposed, undone")
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
