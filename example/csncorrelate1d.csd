<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csncorrelate1d.csd
;
; Correlation is convolution with the kernel read back to front, which is what
; makes it the matched filter: slide a template along a signal and the largest
; answer is where the template sits.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    signal:CsnArr = csnfromarray(array(1, 2, 3, 4, 5))
    kernel:CsnArr = csnfromarray(array(1, 0.5, 0.25))

    ; the same operands csnconvolve1d uses, so the two can be compared directly
    corr:CsnArr   = csncorrelate1d(signal, kernel, 0)
    corr_out:i[]  = csntoarray(corr)
    conv:CsnArr   = csnconvolve1d(signal, kernel, 0)
    conv_out:i[]  = csntoarray(conv)
    prints("correlate : %g %g %g %g %g %g %g\n", corr_out[0], corr_out[1], corr_out[2], corr_out[3], corr_out[4], corr_out[5], corr_out[6])
    prints("convolve  : %g %g %g %g %g %g %g\n", conv_out[0], conv_out[1], conv_out[2], conv_out[3], conv_out[4], conv_out[5], conv_out[6])

    ; matched filter: VALID gives one answer per position the template fits in,
    ; so the index of the largest is where the template starts
    field:CsnArr    = csnfromarray(array(0, 0, 1, 2, 1, 0, 0))
    template:CsnArr = csnfromarray(array(1, 2, 1))

    scores:CsnArr   = csncorrelate1d(field, template, 2)
    scores_out:i[]  = csntoarray(scores)
    scores_n:i      = csnsize(scores)
    prints("scores : n = %d : %g %g %g %g %g\n", scores_n, scores_out[0], scores_out[1], scores_out[2], scores_out[3], scores_out[4])

    at:CsnArr       = csnargmax(scores)
    at_out:i[]      = csntoarray(csnflatten(at))
    peak:i          = csnmax(scores)
    prints("template starts at index %g, score %g\n", at_out[0], peak)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
