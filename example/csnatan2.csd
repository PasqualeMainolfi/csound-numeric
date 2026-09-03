<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; Angles of four points, then the two scalar orders.

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    y:CsnArr      = csnfromarray(array(1, 1, -1, 0))
    x:CsnArr      = csnfromarray(array(1, -1, 1, 1))

    ang:CsnArr    = csnatan2(y, x)
    ang_out:i[]   = csntoarray(ang)
    prints("angles = %.4f %.4f %.4f %.4f\n", ang_out[0], ang_out[1], ang_out[2], ang_out[3])

    hs:CsnArr     = csnatan2(y, 2)
    hs_out:i[]    = csntoarray(hs)
    sh:CsnArr     = csnatan2(2, y)
    sh_out:i[]    = csntoarray(sh)
    prints("y,2    = %.4f      2,y = %.4f\n", hs_out[0], sh_out[0])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
