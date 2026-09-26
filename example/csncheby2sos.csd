<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csncheby2sos.csd
;
; Digital Chebyshev type II filters, as scipy.signal.cheby2 with output='sos'.
; One cutoff makes a lowpass (type 0) or a highpass (1), an array of two a
; bandpass (2) or a bandstop (3). Frequencies are in Hz at the sampling rate
; given last.
; -----------------------------------------------------------------------------

sr = 48000
ksmps = 32
0dbfs = 1

instr 1
    ; sixth-order lowpass, 60 dB down from 1 kHz on
    sos:CsnArr = csncheby2sos(6, 1000, 60, 0, sr)
    csnprint(sos)

    ; fourth-order bandpass, 40 dB down outside 300 Hz to 3 kHz: the band goes in a named array
    iband[] = fillarray(300, 3000)
    sosb:CsnArr = csncheby2sos(4, iband, 40, 2, sr)
    csnprint(sosb)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
