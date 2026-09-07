<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>
sr = 48000
ksmps = 32
0dbfs = 1

instr 1
    native:CsnArr = csnfftfreq(7, 1)
    centered:CsnArr = csnfftshift(native)
    restored:CsnArr = csnifftshift(centered)
    out:i[] = csntoarray(restored)
    prints("restored = %g %g %g %g %g %g %g\n", out[0], out[1], out[2], out[3], out[4], out[5], out[6])
    turnoff
endin
</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
