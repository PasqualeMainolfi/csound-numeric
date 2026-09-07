<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>
sr = 48000
ksmps = 32
0dbfs = 1

instr 1
    ; nfft=4, hop=2, sample rate=8 Hz, rectangular window.
    source:CsnArr = csnfromarray(array(1, 2, 3, 4, 5, 6, 7, 8))
    freqs:CsnArr, times:CsnArr, frames:CsnArr = csnstft(source, 4, 2, 8, 0)
    frame_shape:i[] = csnshape(frames)
    prints("STFT shape = %d bins x %d frames\n", frame_shape[0], frame_shape[1])
    csnprint freqs
    csnprint times
    turnoff
endin
</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
