<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csndecorrelate1d.csd
;
; Decorrelation undoes a FULL correlation: given c = correlate(x, h) and h, it
; gives x back. The correlation reads the kernel reversed and conjugated, so
; the pivot of the recurrence is conj(h[last]) rather than h[0].
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    signal:CsnArr = csnfromarray(array(1, -2, 3, 0.5, 4))
    kernel:CsnArr = csnfromarray(array(0.25, 0.5, 1))

    corr:CsnArr   = csncorrelate1d(signal, kernel, 0)
    corr_out:i[]  = csntoarray(corr)
    prints("correlated : %g %g %g %g %g %g %g\n", corr_out[0], corr_out[1], corr_out[2], corr_out[3], corr_out[4], corr_out[5], corr_out[6])

    back:CsnArr   = csndecorrelate1d(corr, kernel)
    back_out:i[]  = csntoarray(back)
    back_n:i      = csnsize(back)
    prints("recovered  : n = %d : %g %g %g %g %g\n", back_n, back_out[0], back_out[1], back_out[2], back_out[3], back_out[4])

    ; deconvolving the same c instead treats the kernel the wrong way round
    wrong:CsnArr  = csndeconvolve1d(corr, kernel)
    wrong_out:i[] = csntoarray(wrong)
    prints("deconvolved: %g %g %g %g %g\n", wrong_out[0], wrong_out[1], wrong_out[2], wrong_out[3], wrong_out[4])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
