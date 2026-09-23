<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnfftdeconvolve1d.csd
;
; The same deconvolution through a pair of FFTs: the spectrum of y is divided
; by the spectrum of h. It needs no stable recurrence, only a kernel spectrum
; with no zero on the transform grid.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    dry:CsnArr    = csnfromarray(array(1, -2, 3, 0.5, 4, -1))
    echo:CsnArr   = csnfromarray(array(1, 0, 0, 0.5))
    wet:CsnArr    = csnconvolve1d(dry, echo, 0)

    back:CsnArr   = csnfftdeconvolve1d(wet, echo)
    back_out:i[]  = csntoarray(back)
    prints("recovered : %g %g %g %g %g %g\n", back_out[0], back_out[1], back_out[2], back_out[3], back_out[4], back_out[5])

    ; a kernel whose leading tap is not its largest: the direct recurrence
    ; divides by 0.5 at every step and amplifies its own rounding, the
    ; spectral division does not
    long_sig:CsnArr = csnsin(csnarange(0, 256, 1))
    ker:CsnArr      = csnfromarray(array(0.5, 1, 0.3))
    long_wet:CsnArr = csnconvolve1d(long_sig, ker, 0)
    viafft:CsnArr   = csnfftdeconvolve1d(long_wet, ker)
    direct:CsnArr   = csndeconvolve1d(long_wet, ker)
    err_fft:i       = csnmax(csnabs(csnsubtract(viafft, long_sig)))
    err_dir:i       = csnmax(csnabs(csnsubtract(direct, long_sig)))
    prints("256 samples: fft error %.2g, direct error %.2g\n", err_fft, err_dir)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
