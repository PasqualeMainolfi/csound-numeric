<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>
sr = 48000
ksmps = 32
0dbfs = 1

instr 1
    ; For eight real samples only DC through Nyquist are returned: five bins.
    source:CsnArr = csnfromarray(array(1, 0, 0, 0, 0, 0, 0, 0))
    spectrum:CsnArr = csnrfft(source, 8)
    prints("real FFT: type = %d, bins = %d\n", csntype(spectrum), csnsize(spectrum))
    csnprint spectrum
    turnoff
endin
</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
