<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>
sr = 48000
ksmps = 32
0dbfs = 1

instr 1
    ; Four bands over the 17 bins of a 32-point one-sided spectrum.
    bank:CsnArr = csnmlogfbank(32, 4, 0, 24000, 48000, 0)
    bank_shape:i[] = csnshape(bank)
    prints("filterbank shape = %d bands x %d bins\n", bank_shape[0], bank_shape[1])
    csnprint bank
    turnoff
endin
</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
