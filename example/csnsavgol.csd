<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnsavgol.csd
;
; The coefficient matrix of a Savitzky-Golay filter, one row per derivative
; order, applied with csncorrelate1d. The test signal is a parabola, which a
; second-order fit reproduces exactly, so smoothing gives the signal back and
; the first derivative comes out as 2t + 3 with no error to speak of.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    coeffs:CsnArr = csnsavgol(7, 2, 1)
    csnprint coeffs

    smooth:CsnArr = csngetrow(coeffs, 0)
    slope:CsnArr  = csngetrow(coeffs, 1)

    t:CsnArr      = csnarange(0, 12, 1)
    signal:CsnArr = csnadd(csnmul(t, t), csnmul(t, 3))

    ; VALID keeps only the positions where the window is full, which is where
    ; the fit has all of its points
    fitted:CsnArr = csncorrelate1d(signal, smooth, 2)
    csnprint fitted

    derivative:CsnArr = csncorrelate1d(signal, slope, 2)
    csnprint derivative

    ; correlate applies the row as it stands; convolve reverses it, which flips
    ; the sign of every odd-order row - the even ones are symmetric and agree
    flipped:CsnArr = csnconvolve1d(signal, slope, 2)
    csnprint flipped
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
