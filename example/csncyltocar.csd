<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csncyltocar.csd
;
; Cylindrical to cartesian: (r, phi, z) where r and phi are the polar pair in
; the xy plane and z is carried through untouched. It is the system to reach for
; when a source moves round a listener at a fixed height.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    ; the height passes straight through
    ix1, iy1, iz1 csncyltocar 2, 0, 5
    prints("(r 2, phi 0, z 5)   -> %.4f %.4f %.4f\n", ix1, iy1, iz1)

    ; a quarter turn moves it onto +y, still at the same height
    ix2, iy2, iz2 csncyltocar 2, 90, 5, 1
    prints("(r 2, phi 90deg, z 5) -> %.4f %.4f %.4f\n", ix2, iy2, iz2)

    ; round trip
    ir, ip, ih csncartocyl 3, 4, 7
    prints("cartocyl(3, 4, 7)   -> r %.4f phi %.4f z %.4f\n", ir, ip, ih)
    ibx, iby, ibz csncyltocar ir, ip, ih
    prints("                    -> back to %.4f %.4f %.4f\n", ibx, iby, ibz)

    ; one turn at a fixed height, the path a circular panner traces
    iphi = 0
    while iphi < 360 do
        ipx, ipy, ipz csncyltocar 1, iphi, 1.7, 1
        prints("phi %3.0f deg -> x %.3f y %.3f z %.1f\n", iphi, ipx, ipy, ipz)
        iphi += 90
    od
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
