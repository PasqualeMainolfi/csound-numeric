<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnbutterba.csd
;
; Digital Butterworth filters, as scipy.signal.butter with output='ba':
; maximally flat in the passband and 3 dB down exactly at the cutoff. One
; cutoff makes a lowpass (type 0) or a highpass (1), an array of two a
; bandpass (2) or a bandstop (3). Frequencies are in Hz at the sampling rate
; given last.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    ; second-order lowpass at a quarter of the sampling rate:
    ; b = [0.2929 0.5858 0.2929], a = [1 0 0.1716]
    b:CsnArr, a:CsnArr csnbutterba 2, sr / 4, 0, sr
    csnprint(b)
    csnprint(a)

    ; fourth-order highpass at 100 Hz
    bh:CsnArr, ah:CsnArr csnbutterba 4, 100, 1, sr
    csnprint(bh)
    csnprint(ah)

    ; second-order bandpass, 300 Hz to 3 kHz: the band goes in a named array
    iband[] = fillarray(300, 3000)
    bb:CsnArr, ab:CsnArr csnbutterba 2, iband, 2, sr
    csnprint(bb)
    csnprint(ab)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
