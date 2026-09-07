<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>
sr = 48000
ksmps = 32
0dbfs = 1

instr 1
    freqs:CsnArr = csnrfftfreq(8, 0.125)
    out:i[] = csntoarray(freqs)
    prints("rfftfreq = %g %g %g %g %g\n", out[0], out[1], out[2], out[3], out[4])
    turnoff
endin
</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
