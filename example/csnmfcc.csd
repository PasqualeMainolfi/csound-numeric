<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>
sr = 48000
ksmps = 32
0dbfs = 1

instr 1
    ; 128 samples, 32-sample frames hopping by 16: seven frames.
    shape:i[] = fillarray(128)
    source:CsnArr = csnzeros(shape)
    coeffs:CsnArr = csnmfcc(source, 32, 16, 48000, 4, 0, 24000, 2, 2)
    coeff_shape:i[] = csnshape(coeffs)
    prints("MFCC shape = %d coefficients x %d frames\n", coeff_shape[0], coeff_shape[1])
    turnoff
endin
</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
