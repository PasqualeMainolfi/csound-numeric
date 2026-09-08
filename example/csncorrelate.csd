<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csncorrelate.csd
;
; Template matching in two dimensions: VALID gives one score per position the
; patch fits in, and the coordinates of the largest score are where it sits.
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

    scores:CsnArr = csncorrelate(field, patch, 2)
    csnprint scores

    at:CsnArr     = csnargmax(scores)
    at_out:i[]    = csntoarray(csnflatten(at))
    best:i        = csnmax(scores)
    prints("patch found at row %g, column %g, score %g\n", at_out[0], at_out[1], best)

    ; the flip is what separates the two: on a kernel that is not symmetric,
    ; correlation and convolution answer differently
    kernel:CsnArr = csnreshape(csnfromarray(array(1, 2, 3, 4)), kshape)
    corr:CsnArr   = csncorrelate(field, kernel, 2)
    conv:CsnArr   = csnconvolve(field, kernel, 2)
    cell:i[]      = fillarray(0, 0)
    corr_00:i     = csnget(corr, cell)
    conv_00:i     = csnget(conv, cell)
    prints("correlate[0][0] = %g, convolve[0][0] = %g\n", corr_00, conv_00)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
