<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csndeconvolve.csd
;
; N-D deconvolution: a kernel shaped like the source, undone on every axis at
; once. The answer is x.shape - h.shape + 1 on each axis.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    shape:i[]     = fillarray(3, 3)
    kshape:i[]    = fillarray(2, 2)
    image:CsnArr  = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6, 7, 8, 9)), shape)
    blur:CsnArr   = csnreshape(csnfromarray(array(1, 0.5, 0.5, 0.25)), kshape)

    blurred:CsnArr = csnconvolve(image, blur, 0)
    csnprint blurred

    sharp:CsnArr  = csndeconvolve(blurred, blur)
    csnprint sharp
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
