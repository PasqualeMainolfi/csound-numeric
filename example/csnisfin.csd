<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    inf:i          = exp(1000)
    nan:i          = sqrt(-1)
    data:CsnArr    = csnfromarray(array(1, inf, nan, 0))
    mask:CsnArr    = csnisfin(data)
    mask_out:i[]   = csntoarray(mask)
    prints("finite = %g %g %g %g\n", mask_out[0], mask_out[1], mask_out[2], mask_out[3])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
