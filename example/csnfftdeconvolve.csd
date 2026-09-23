<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnfftdeconvolve.csd
;
; N-D deconvolution through Fourier transforms: every axis padded to a power
; of two at or above the source's length, spectra divided, transformed back.
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

    sharp:CsnArr  = csnfftdeconvolve(blurred, blur)
    csnprint sharp

    direct:CsnArr = csndeconvolve(blurred, blur)
    worst:i       = csnmax(csnabs(csnsubtract(direct, sharp)))
    prints("largest difference from csndeconvolve: %.2g\n", worst)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
