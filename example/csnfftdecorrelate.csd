<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnfftdecorrelate.csd
;
; N-D decorrelation through Fourier transforms, checked against the direct
; form on the same operands.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    shape:i[]     = fillarray(3, 3)
    kshape:i[]    = fillarray(2, 2)
    field:CsnArr  = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6, 7, 8, 9)), shape)
    kernel:CsnArr = csnreshape(csnfromarray(array(0.5, 0.25, 1, 2)), kshape)
    corr:CsnArr   = csncorrelate(field, kernel, 0)

    back:CsnArr   = csnfftdecorrelate(corr, kernel)
    csnprint back

    direct:CsnArr = csndecorrelate(corr, kernel)
    worst:i       = csnmax(csnabs(csnsubtract(direct, back)))
    prints("largest difference from csndecorrelate: %.2g\n", worst)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
