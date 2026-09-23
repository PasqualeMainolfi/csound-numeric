<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csndeconvolve1d.csd
;
; Deconvolution undoes a FULL convolution: given y = x * h and h, it gives x
; back, one sample per position the kernel fits in, len(y) - len(h) + 1. Here
; it takes an echo back out of a signal.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    dry:CsnArr    = csnfromarray(array(1, -2, 3, 0.5, 4, -1))
    echo:CsnArr   = csnfromarray(array(1, 0, 0, 0.5))
    wet:CsnArr    = csnconvolve1d(dry, echo, 0)
    wet_out:i[]   = csntoarray(wet)
    prints("with echo : %g %g %g %g %g %g %g %g %g\n", wet_out[0], wet_out[1], wet_out[2], wet_out[3], wet_out[4], wet_out[5], wet_out[6], wet_out[7], wet_out[8])

    back:CsnArr   = csndeconvolve1d(wet, echo)
    back_out:i[]  = csntoarray(back)
    back_n:i      = csnsize(back)
    prints("recovered : n = %d : %g %g %g %g %g %g\n", back_n, back_out[0], back_out[1], back_out[2], back_out[3], back_out[4], back_out[5])

    ; along an axis, every lane is recovered on its own
    shape:i[]     = fillarray(2, 3)
    mat:CsnArr    = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6)), shape)
    kernel:CsnArr = csnfromarray(array(2, 1))
    rows:CsnArr   = csnconvolve1d(mat, kernel, 0, -1)
    csnprint rows
    undone:CsnArr = csndeconvolve1d(rows, kernel, -1)
    csnprint undone
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
