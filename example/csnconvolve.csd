<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnconvolve.csd
;
; The N-D form takes a kernel shaped like the source and flips it on every axis
; at once. SAME is the mode a filter usually wants: the result keeps the shape
; of what went in.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    shape:i[]      = fillarray(3, 3)
    kshape:i[]     = fillarray(2, 2)
    source:CsnArr  = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6, 7, 8, 9)), shape)
    kernel:CsnArr  = csnreshape(csnfromarray(array(1, 2, 3, 4)), kshape)

    full:CsnArr    = csnconvolve(source, kernel, 0)
    csnprint full

    ; SAME keeps the source shape, VALID only the fully covered positions
    same:CsnArr    = csnconvolve(source, kernel, 1)
    same_shape:i[] = csnshape(same)
    prints("same  shape : %d x %d\n", same_shape[0], same_shape[1])

    valid:CsnArr   = csnconvolve(source, kernel, 2)
    valid_shape:i[] = csnshape(valid)
    prints("valid shape : %d x %d\n", valid_shape[0], valid_shape[1])
    csnprint valid

    ; a 2x2 box blur: the kernel sums to one, so an interior value comes back as
    ; the average of its neighbourhood. Outside the source counts as zero, which
    ; is why the edges - and with them the overall mean - fall off.
    box:CsnArr     = csnfull(kshape, 0.25)
    blurred:CsnArr = csnconvolve(source, box, 1)
    centre:i[]     = fillarray(1, 1)
    centre_in:i    = csnget(source, centre)
    centre_out:i   = csnget(blurred, centre)
    prints("centre %g -> %g (mean of 1, 2, 4, 5)\n", centre_in, centre_out)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
