<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnrtcheby2.csd
;
; A digital Chebyshev type II filter on audio whose cutoff, or band centre and
; width, follow a k-rate signal. The prototype is designed once at init; each
; control period moves it to the current frequencies and filters the block
; through the second-order sections, keeping the state across the change.
; -----------------------------------------------------------------------------

sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

instr 1
    ; lowpass sweeping from 200 Hz to 5 kHz and back
    anoise = noise(0.3, 0)
    kfc = expseg(200, p3 / 2, 5000, p3 / 2, 200)
    afilt = csnrtcheby2(anoise, 5, kfc, 40, 0, sr)
    out afilt
endin

instr 2
    ; bandpass whose centre glides up while the band narrows
    anoise = noise(0.3, 0)
    kcentre = expseg(300, p3, 3000)
    kwidth = linseg(400, p3, 100)
    afilt = csnrtcheby2(anoise, 4, kcentre, kwidth, 40, 2, sr)
    out afilt
endin

</CsInstruments>
<CsScore>
i 1 0 2
i 2 2 2
</CsScore>
</CsoundSynthesizer>
