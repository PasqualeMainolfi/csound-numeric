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
    freqs:CsnArr, frame_times:CsnArr, frames:CsnArr = csnstft(source, 4, 2, 8, 0)
    sample_times:CsnArr, restored:CsnArr = csnistft(frames, 4, 2, 8, 0)
    out:i[] = csntoarray(restored)
    prints("ISTFT type = %d, samples = %d\n", csntype(restored), csnsize(restored))
    prints("first/last = %g %g\n", out[0], out[7])
    csnprint sample_times
    turnoff
endin
</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
