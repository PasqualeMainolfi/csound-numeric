<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>
sr = 48000
ksmps = 32
0dbfs = 1

instr 1
    source:CsnArr = csnfromarray(array(1, 2, 3, 4, 5, 6, 7, 8))
    spectrum:CsnArr = csnrfft(source, 8)
    restored:CsnArr = csnirfft(spectrum, 8)
    out:i[] = csntoarray(restored)
    prints("IRFFT type = %d, samples = %d\n", csntype(restored), csnsize(restored))
    prints("first/last = %g %g\n", out[0], out[7])
    turnoff
endin
</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
