<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnfftdecorrelate1d.csd
;
; The same decorrelation through a pair of FFTs: the spectrum is divided by
; conj(H), and the answer is read h - 1 samples into the inverse transform,
; where the reversed kernel left it.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    signal:CsnArr = csnfromarray(array(1, -2, 3, 0.5, 4))
    kernel:CsnArr = csnfromarray(array(0.25, 0.5, 1))
    corr:CsnArr   = csncorrelate1d(signal, kernel, 0)

    back:CsnArr   = csnfftdecorrelate1d(corr, kernel)
    back_out:i[]  = csntoarray(back)
    prints("recovered : %g %g %g %g %g\n", back_out[0], back_out[1], back_out[2], back_out[3], back_out[4])

    ; along an axis of a matrix, against the direct form
    shape:i[]     = fillarray(2, 4)
    mat:CsnArr    = csnreshape(csnfromarray(array(1, 2, 3, 4, -1, 0, 2, 5)), shape)
    lanes:CsnArr  = csncorrelate1d(mat, kernel, 0, -1)
    viafft:CsnArr = csnfftdecorrelate1d(lanes, kernel, -1)
    direct:CsnArr = csndecorrelate1d(lanes, kernel, -1)
    csnprint viafft
    worst:i       = csnmax(csnabs(csnsubtract(direct, viafft)))
    prints("largest difference from csndecorrelate1d: %.2g\n", worst)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
