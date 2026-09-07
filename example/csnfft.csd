<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>
sr = 48000
ksmps = 32
0dbfs = 1

instr 1
    ; A unit impulse has a flat complex spectrum.
    source:CsnArr = csnfromarray(array(1, 0, 0, 0, 0, 0, 0, 0))
    spectrum:CsnArr = csnfft(source, 8)
    prints("full FFT: type = %d, bins = %d\n", csntype(spectrum), csnsize(spectrum))
    csnprint spectrum
    turnoff
endin
</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
