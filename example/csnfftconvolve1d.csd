<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnfftconvolve1d.csd
;
; The same convolution csnconvolve1d computes, routed through a pair of FFTs.
; Same operands, same edge modes, same answers: what changes is the cost, which
; stops growing with the kernel once the kernel is long.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    signal:CsnArr = csnfromarray(array(1, 2, 3, 4, 5))
    kernel:CsnArr = csnfromarray(array(1, 0.5, 0.25))

    full:CsnArr   = csnfftconvolve1d(signal, kernel, 0)
    full_out:i[]  = csntoarray(full)
    prints("full  : %g %g %g %g %g %g %g\n", full_out[0], full_out[1], full_out[2], full_out[3], full_out[4], full_out[5], full_out[6])

    same:CsnArr   = csnfftconvolve1d(signal, kernel, 1)
    same_out:i[]  = csntoarray(same)
    prints("same  : %g %g %g %g %g\n", same_out[0], same_out[1], same_out[2], same_out[3], same_out[4])

    valid:CsnArr  = csnfftconvolve1d(signal, kernel, 2)
    valid_out:i[] = csntoarray(valid)
    prints("valid : %g %g %g\n", valid_out[0], valid_out[1], valid_out[2])

    ; the answer is the direct one to within rounding: a 512-sample signal
    ; through a 200-tap kernel, the length where the transform earns its keep
    long_sig:CsnArr = csnsin(csnarange(0, 512, 1))
    long_ker:CsnArr = csnhanning(200)

    direct:CsnArr   = csnconvolve1d(long_sig, long_ker, 0)
    viafft:CsnArr   = csnfftconvolve1d(long_sig, long_ker, 0)
    n_direct:i      = csnsize(direct)
    n_fft:i         = csnsize(viafft)
    worst:i         = csnmax(csnabs(csnsubtract(direct, viafft)))
    prints("512 x 200 taps: n = %d and %d, largest difference %.2g\n", n_direct, n_fft, worst)

    ; along an axis of a matrix, exactly as the direct form does
    shape:i[]     = fillarray(2, 3)
    mat:CsnArr    = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6)), shape)
    ones:CsnArr   = csnfromarray(array(1, 1))
    rows:CsnArr   = csnfftconvolve1d(mat, ones, 0, 1)
    csnprint rows
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
