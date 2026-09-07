<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>
sr = 48000
ksmps = 32
0dbfs = 1

instr 1
    ; Eight samples spaced 1/8 second apart: bin spacing is 1 Hz.
    freqs:CsnArr = csnfftfreq(8, 0.125)
    out:i[] = csntoarray(freqs)
    prints("fftfreq = %g %g %g %g %g %g %g %g\n", out[0], out[1], out[2], out[3], out[4], out[5], out[6], out[7])
    turnoff
endin
</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
