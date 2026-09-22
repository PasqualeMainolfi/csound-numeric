<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnsphharm.csd
;
; One real spherical harmonic Y_n^m at one direction, in the AmbiX convention:
; ACN ordering, SN3D normalisation, no global 1/sqrt(4*pi), no Condon-Shortley
; phase. The direction is the one csnhoatocar reads: azimuth from +x (front)
; towards +y (left), elevation from the horizon.
;
;   Y_n^m = sqrt((2 - delta_m) (n-|m|)! / (n+|m|)!) P_n^|m|(sin el) trig(m az)
;   trig  = sin(|m| az) for m < 0, cos(m az) for m >= 0
;
; The first order is the direction itself, which is why a first-order encoder
; is a special case of it: the example checks that against bformenc1.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    iaz = 30
    iel = 20

    ; the first order: W, then Y, Z, X in ACN order
    iW = csnsphharm(0, 0, iaz, iel, 1)
    iY = csnsphharm(1, -1, iaz, iel, 1)
    iZ = csnsphharm(1, 0, iaz, iel, 1)
    iX = csnsphharm(1, 1, iaz, iel, 1)
    prints("azimuth %d, elevation %d degrees\n", iaz, iel)
    prints("  W %.6f  Y %.6f  Z %.6f  X %.6f\n", iW, iY, iZ, iX)

    ; the same numbers are the unit direction
    ix, iy, iz csnhoatocar 1, iaz, iel, 1
    prints("  unit direction x %.6f  y %.6f  z %.6f\n", ix, iy, iz)

    ; a higher harmonic, radians this time
    iY3m2 = csnsphharm(3, -2, iaz * $M_PI / 180, iel * $M_PI / 180)
    prints("  Y(3, -2) %.6f\n", iY3m2)
endin

instr 2
    ; bformenc1 encodes first-order FuMa: W scaled by 1/sqrt(2), then X, Y, Z.
    ; On a constant unit signal its X, Y, Z are exactly the three first-order
    ; harmonics, and W times sqrt(2) is Y_0^0 = 1.
    asig = 1
    aw, ax, ay, az bformenc1 asig, 30, 20
    kn init 0
    if kn == 1 then
        kW = k(aw) * sqrt(2)
        kX = k(ax)
        kY = k(ay)
        kZ = k(az)
        kHX = csnsphharm(1, 1, 30, 20, 1)
        kHY = csnsphharm(1, -1, 30, 20, 1)
        kHZ = csnsphharm(1, 0, 30, 20, 1)
        printks("bformenc1  W %.6f  X %.6f  Y %.6f  Z %.6f\n", 0, kW, kX, kY, kZ)
        printks("csnsphharm W 1.000000  X %.6f  Y %.6f  Z %.6f\n", 0, kHX, kHY, kHZ)
        turnoff
    endif
    kn += 1
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
i 2 0.2 0.1
</CsScore>
</CsoundSynthesizer>
