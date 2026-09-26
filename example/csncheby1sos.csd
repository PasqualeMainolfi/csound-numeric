<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csncheby1sos.csd
;
; Digital Chebyshev type I filters, as scipy.signal.cheby1 with output='sos'.
; One cutoff makes a lowpass (type 0) or a highpass (1), an array of two a
; bandpass (2) or a bandstop (3). Frequencies are in Hz at the sampling rate
; given last.
; -----------------------------------------------------------------------------

sr = 48000
ksmps = 32
0dbfs = 1

instr 1
    ; sixth-order lowpass at 1 kHz, 1 dB of ripple
    sos:CsnArr = csncheby1sos(6, 1000, 1, 0, sr)
    csnprint(sos)

    ; fourth-order bandpass, 300 Hz to 3 kHz, 0.5 dB of ripple: the band goes in a named array
    iband[] = fillarray(300, 3000)
    sosb:CsnArr = csncheby1sos(4, iband, 0.5, 2, sr)
    csnprint(sosb)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
