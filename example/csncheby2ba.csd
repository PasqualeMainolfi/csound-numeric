<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csncheby2ba.csd
;
; Digital Chebyshev type II filters, as scipy.signal.cheby2 with output='ba'.
; One cutoff makes a lowpass (type 0) or a highpass (1), an array of two a
; bandpass (2) or a bandstop (3). Frequencies are in Hz at the sampling rate
; given last.
; -----------------------------------------------------------------------------

sr = 48000
ksmps = 32
0dbfs = 1

instr 1
    ; sixth-order lowpass, 60 dB down from 1 kHz on
    b:CsnArr, a:CsnArr csncheby2ba 6, 1000, 60, 0, sr
    csnprint(b)
    csnprint(a)

    ; fourth-order bandpass, 40 dB down outside 300 Hz to 3 kHz: the band goes in a named array
    iband[] = fillarray(300, 3000)
    bb:CsnArr, ab:CsnArr csncheby2ba 4, iband, 40, 2, sr
    csnprint(bb)
    csnprint(ab)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
