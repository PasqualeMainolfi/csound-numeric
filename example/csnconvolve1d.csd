<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnconvolve1d.csd
;
; The three edge modes are the same convolution read over different spans: FULL
; keeps every position where the kernel touches the signal, SAME keeps as many
; as the signal had, VALID only the ones where the kernel sits entirely inside.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    signal:CsnArr = csnfromarray(array(1, 2, 3, 4, 5))
    kernel:CsnArr = csnfromarray(array(1, 0.5, 0.25))

    full:CsnArr   = csnconvolve1d(signal, kernel, 0)
    full_out:i[]  = csntoarray(full)
    full_n:i      = csnsize(full)
    prints("full  : n = %d : %g %g %g %g %g %g %g\n", full_n, full_out[0], full_out[1], full_out[2], full_out[3], full_out[4], full_out[5], full_out[6])

    same:CsnArr   = csnconvolve1d(signal, kernel, 1)
    same_out:i[]  = csntoarray(same)
    same_n:i      = csnsize(same)
    prints("same  : n = %d : %g %g %g %g %g\n", same_n, same_out[0], same_out[1], same_out[2], same_out[3], same_out[4])

    valid:CsnArr  = csnconvolve1d(signal, kernel, 2)
    valid_out:i[] = csntoarray(valid)
    valid_n:i     = csnsize(valid)
    prints("valid : n = %d : %g %g %g\n", valid_n, valid_out[0], valid_out[1], valid_out[2])

    ; along an axis of a matrix, every lane is convolved on its own
    shape:i[]     = fillarray(2, 3)
    mat:CsnArr    = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6)), shape)
    ones:CsnArr   = csnfromarray(array(1, 1))

    cols:CsnArr   = csnconvolve1d(mat, ones, 0, 0)
    csnprint cols

    rows:CsnArr   = csnconvolve1d(mat, ones, 0, 1)
    csnprint rows
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
