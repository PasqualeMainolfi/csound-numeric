<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnouter.csd
;
; Two vectors in, a plane out. A separable 2-D window is the outer product of
; two 1-D ones.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    a:CsnArr      = csnfromarray(array(1, 2, 3))
    b:CsnArr      = csnfromarray(array(4, 5, 6))

    plane:CsnArr  = csnouter(a, b)
    plane_shape:i[] = csnshape(plane)
    plane_out:i[] = csntoarray(csnflatten(plane))
    prints("%g x %g\n", plane_shape[0], plane_shape[1])
    prints("row 0 = %g %g %g\n", plane_out[0], plane_out[1], plane_out[2])
    prints("row 2 = %g %g %g\n", plane_out[6], plane_out[7], plane_out[8])

    ; a separable 2-D window
    win:CsnArr    = csnhanning(4)
    win2d:CsnArr  = csnouter(win, win)
    win2d_shape:i[] = csnshape(win2d)
    peak:i        = csnmax(win2d)
    prints("2-D window %g x %g, peak = %.4f\n", win2d_shape[0], win2d_shape[1], peak)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
