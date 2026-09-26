<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnlfilter.csd
;
; A filter given as b, a applied to an array along an axis, as
; scipy.signal.lfilter, and to an audio signal. Transposed direct form II;
; a[0] need not be 1, the coefficients are divided by it.
; -----------------------------------------------------------------------------

sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

instr 1
    ; second-order Butterworth lowpass at 2 kHz
    b:CsnArr, a:CsnArr csnbutterba 2, 2000, 0, sr

    ; an impulse, filtered at init: its first samples are the impulse response
    iimp[] = fillarray(1, 0, 0, 0, 0, 0, 0, 0)
    x:CsnArr = csnfromarray(iimp)
    y:CsnArr = csnlfilter(b, a, x)
    csnprint(y)
    turnoff
endin

instr 2
    ; the same filter on noise, sample by sample
    b:CsnArr, a:CsnArr csnbutterba 2, 2000, 0, sr
    anoise = noise(0.3, 0)
    afilt = csnlfilter(b, a, anoise)
    out afilt
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
i 2 0 0.1
</CsScore>
</CsoundSynthesizer>
