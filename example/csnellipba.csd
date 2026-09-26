<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnellipba.csd
;
; Digital elliptic (Cauer) filters, as scipy.signal.ellip with output='ba'.
; One cutoff makes a lowpass (type 0) or a highpass (1), an array of two a
; bandpass (2) or a bandstop (3). Frequencies are in Hz at the sampling rate
; given last.
; -----------------------------------------------------------------------------

sr = 48000
ksmps = 32
0dbfs = 1

instr 1
    ; sixth-order lowpass at 1 kHz, 1 dB of ripple, 60 dB down in the stopband
    b:CsnArr, a:CsnArr csnellipba 6, 1000, 1, 60, 0, sr
    csnprint(b)
    csnprint(a)

    ; fourth-order bandpass, 300 Hz to 3 kHz, 0.5 dB ripple, 40 dB stopband: the band goes in a named array
    iband[] = fillarray(300, 3000)
    bb:CsnArr, ab:CsnArr csnellipba 4, iband, 0.5, 40, 2, sr
    csnprint(bb)
    csnprint(ab)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
