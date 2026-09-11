<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

instr 1
    aSignal oscili 0.5, 3000

    ; The analysis size is independent of ksmps. csnsnap buffers the audio and
    ; raises kReady only when a complete, 50%-overlapped frame is available.
    frame:CsnArr, kReady = csnsnap(aSignal, 256, 128)
    spectrum:CsnArr = csnrfft(frame, 256, -1, kReady)
    magnitude:CsnArr = csnabs(spectrum, kReady)
    kPeak = csnmax(magnitude, kReady)

    ; csnsnap marks frame as a real-time path by default; spectrum and magnitude
    ; inherit the mark, so no explicit csnrtlock is needed in this chain.
    printf("new spectrum: %d bins, peak magnitude %.3f\n", kReady, 129, kPeak)
endin
</CsInstruments>
<CsScore>
i 1 0 0.03
</CsScore>
</CsoundSynthesizer>
