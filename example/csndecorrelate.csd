<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csndecorrelate.csd
;
; N-D decorrelation: the inverse of a FULL csncorrelate, with a kernel shaped
; like the source. The pivot is the conjugate of the kernel's last element.
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
    csnprint corr

    back:CsnArr   = csndecorrelate(corr, kernel)
    csnprint back

    ; the same kernel convolved gives a different c: the flip is on every axis
    conv:CsnArr   = csnconvolve(field, kernel, 0)
    csnprint conv
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
