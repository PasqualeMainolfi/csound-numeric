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

    hi:CsnArr     = csnmaximum(a, b)
    hi_out:i[]    = csntoarray(hi)
    prints("max  = %g %g %g %g\n", hi_out[0], hi_out[1], hi_out[2], hi_out[3])

    flr:CsnArr    = csnmaximum(a, 2)
    flr_out:i[]   = csntoarray(flr)
    prints("flr2 = %g %g %g %g\n", flr_out[0], flr_out[1], flr_out[2], flr_out[3])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
