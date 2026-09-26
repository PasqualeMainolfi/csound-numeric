<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnbuttersos.csd
;
; Digital Butterworth filters as second-order sections, as
; scipy.signal.butter with output='sos'. Each row is [b0 b1 b2 a0 a1 a2],
; a0 = 1, and the filter is the cascade of the rows. Sections keep a
; high-order or narrow-band design stable where a single b, a loses it: the
; sixth-order bandpass below has a pole outside the unit circle as b, a.
; -----------------------------------------------------------------------------

sr = 48000
ksmps = 32
0dbfs = 1

instr 1
    ; eighth-order lowpass at 1 kHz: four sections, the sharpest last
    sos:CsnArr = csnbuttersos(8, 1000, 0, sr)
    csnprint(sos)

    ; sixth-order bandpass, 500 to 700 Hz: six stable sections
    iband[] = fillarray(500, 700)
    sosb:CsnArr = csnbuttersos(6, iband, 2, sr)
    csnprint(sosb)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
