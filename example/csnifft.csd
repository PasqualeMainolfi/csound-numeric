<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>
sr = 48000
ksmps = 32
0dbfs = 1

instr 1
    source:CsnArr = csnfromarray(array(1, 2, 3, 4))
    spectrum:CsnArr = csnfft(source, 4)
    restored:CsnArr = csnifft(spectrum, 4)
    re:CsnArr = csnreal(restored)
    im:CsnArr = csnimag(restored)
    re_out:i[] = csntoarray(re)
    prints("IFFT real lane: %g %g %g %g\n", re_out[0], re_out[1], re_out[2], re_out[3])
    prints("maximum imaginary residue: %g\n", csnmax(csnabs(im)))
    turnoff
endin
</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
