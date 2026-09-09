<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnfftconvolve.csd
;
; The N-D convolution through transforms: one pass per axis, a product in the
; spectrum, one inverse pass per axis. Same kernel, same edge modes and same
; answers as csnconvolve, and the same shape rules.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    shape:i[]      = fillarray(3, 3)
    kshape:i[]     = fillarray(2, 2)
    source:CsnArr  = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6, 7, 8, 9)), shape)
    kernel:CsnArr  = csnreshape(csnfromarray(array(1, 2, 3, 4)), kshape)

    full:CsnArr    = csnfftconvolve(source, kernel, 0)
    csnprint full

    valid:CsnArr   = csnfftconvolve(source, kernel, 2)
    csnprint valid

    ; a 32x32 field through a 9x9 blur: every axis is padded to its own power
    ; of two, so the two extents are transformed at different lengths
    field_shape:i[] = fillarray(32, 32)
    blur_shape:i[]  = fillarray(9, 9)
    field:CsnArr    = csnreshape(csnsin(csnarange(0, 1024, 1)), field_shape)
    blur:CsnArr     = csnfull(blur_shape, 1 / 81)

    smooth:CsnArr   = csnfftconvolve(field, blur, 1)
    smooth_shape:i[] = csnshape(smooth)
    prints("SAME keeps the field shape: %d x %d\n", smooth_shape[0], smooth_shape[1])

    direct:CsnArr   = csnconvolve(field, blur, 1)
    worst:i         = csnmax(csnabs(csnsubtract(direct, smooth)))
    prints("largest difference from csnconvolve: %.2g\n", worst)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
