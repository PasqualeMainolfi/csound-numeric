<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnpoltocar.csd
;
; Polar to cartesian, in the plane: (r, phi) with phi measured from +x. For a
; third component use csncyltocar, which carries a height through, or
; csnsphtocar, which takes two angles.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    ix1, iy1 csnpoltocar 2, 0
    prints("(r 2, phi 0)      -> %.4f %.4f\n", ix1, iy1)

    ; the trailing 1 says the angle is in degrees
    ix2, iy2 csnpoltocar 1, 90, 1
    prints("(r 1, phi 90 deg) -> %.4f %.4f\n", ix2, iy2)

    ; round trip
    ir, ip csncartopol 3, 4
    prints("cartopol(3, 4)    -> r %.4f phi %.4f\n", ir, ip)
    ibx, iby csnpoltocar ir, ip
    prints("                  -> back to %.4f %.4f\n", ibx, iby)

    ; a stereo pair placed by angle rather than by gain
    iangle = -45
    while iangle <= 45 do
        ilx, ily csnpoltocar 1, iangle, 1
        prints("%4.0f deg -> x %+.3f y %.3f\n", iangle, ilx, ily)
        iangle += 45
    od
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
