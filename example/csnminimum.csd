<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    a:CsnArr      = csnfromarray(array(1, 5, -3, 8))
    b:CsnArr      = csnfromarray(array(4, 2, 0, 8))

    lo:CsnArr     = csnminimum(a, b)
    lo_out:i[]    = csntoarray(lo)
    prints("min  = %g %g %g %g\n", lo_out[0], lo_out[1], lo_out[2], lo_out[3])

    cap:CsnArr    = csnminimum(a, 2)
    cap_out:i[]   = csntoarray(cap)
    prints("cap2 = %g %g %g %g\n", cap_out[0], cap_out[1], cap_out[2], cap_out[3])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
