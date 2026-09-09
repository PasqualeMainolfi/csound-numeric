<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnfftcorrelate.csd
;
; Template matching in two dimensions, through transforms. The kernel is
; conjugated in the spectrum rather than flipped on every axis, and the answer
; is the one csncorrelate gives.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    shape:i[]     = fillarray(4, 4)
    kshape:i[]    = fillarray(2, 2)

    ; a 4x4 field, empty except for a 2x2 patch at rows 1-2, columns 1-2
    field:CsnArr  = csnreshape(csnfromarray(array(0, 0, 0, 0,
                                                  0, 1, 1, 0,
                                                  0, 1, 1, 0,
                                                  0, 0, 0, 0)), shape)
    patch:CsnArr  = csnones(kshape)

    scores:CsnArr = csnfftcorrelate(field, patch, 2)
    csnprint scores

    at:CsnArr     = csnargmax(scores)
    at_out:i[]    = csntoarray(csnflatten(at))
    prints("patch found at row %g, column %g\n", at_out[0], at_out[1])

    ; a kernel that is not symmetric separates correlation from convolution
    kernel:CsnArr = csnreshape(csnfromarray(array(1, 2, 3, 4)), kshape)
    corr:CsnArr   = csnfftcorrelate(field, kernel, 2)
    conv:CsnArr   = csnfftconvolve(field, kernel, 2)
    cell:i[]      = fillarray(0, 0)
    corr_00:i     = csnget(corr, cell)
    conv_00:i     = csnget(conv, cell)
    prints("correlate[0][0] = %g, convolve[0][0] = %g\n", corr_00, conv_00)

    ; and the direct form answers the same as this one
    direct:CsnArr = csncorrelate(field, kernel, 2)
    worst:i       = csnmax(csnabs(csnsubtract(direct, corr)))
    prints("largest difference from csncorrelate: %.2g\n", worst)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
