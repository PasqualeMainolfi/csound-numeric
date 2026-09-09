<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnfftcorrelate1d.csd
;
; Correlation through a pair of FFTs. The kernel is conjugated in the spectrum
; instead of being read backwards in time, which is the same operation said the
; other way round: the answers match csncorrelate1d to within rounding.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    signal:CsnArr = csnfromarray(array(1, 2, 3, 4, 5))
    kernel:CsnArr = csnfromarray(array(1, 0.5, 0.25))

    corr:CsnArr   = csnfftcorrelate1d(signal, kernel, 0)
    corr_out:i[]  = csntoarray(corr)
    conv:CsnArr   = csnfftconvolve1d(signal, kernel, 0)
    conv_out:i[]  = csntoarray(conv)
    prints("correlate : %g %g %g %g %g %g %g\n", corr_out[0], corr_out[1], corr_out[2], corr_out[3], corr_out[4], corr_out[5], corr_out[6])
    prints("convolve  : %g %g %g %g %g %g %g\n", conv_out[0], conv_out[1], conv_out[2], conv_out[3], conv_out[4], conv_out[5], conv_out[6])

    ; matched filter over a long signal: VALID scores one position per place
    ; the template fits, and the largest is where it sits
    fieldshape:i[]  = fillarray(256)
    template:CsnArr = csnhanning(64)
    field:CsnArr    = csnzeros(fieldshape)
    csnsetslice(field, template, 0, 100, 164, 1)

    scores:CsnArr   = csnfftcorrelate1d(field, template, 2)
    at:CsnArr       = csnargmax(scores)
    at_out:i[]      = csntoarray(csnflatten(at))
    prints("template planted at 100, found at %g\n", at_out[0])

    ; the direct form answers the same thing
    direct:CsnArr   = csncorrelate1d(field, template, 2)
    worst:i         = csnmax(csnabs(csnsubtract(direct, scores)))
    prints("largest difference from csncorrelate1d: %.2g\n", worst)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
