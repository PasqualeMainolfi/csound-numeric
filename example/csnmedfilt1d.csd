<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnmedfilt1d.csd
;
; A median filter with the edges scipy uses: what falls outside the array counts
; as zero, so the window is always full and the ends are pulled towards zero.
; csnmovmedian answers the same filter with a shorter window near the ends.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr      = csnfromarray(array(1, 100, 2, 3, 4))

    filtered:CsnArr  = csnmedfilt1d(data, 3)
    filtered_out:i[] = csntoarray(filtered)
    prints("kernel 3        : %g %g %g %g %g\n", filtered_out[0], filtered_out[1], filtered_out[2], filtered_out[3], filtered_out[4])

    ; a wider kernel: the spike is already gone, the zero padding reaches further
    wide:CsnArr      = csnmedfilt1d(data, 5)
    wide_out:i[]     = csntoarray(wide)
    prints("kernel 5        : %g %g %g %g %g\n", wide_out[0], wide_out[1], wide_out[2], wide_out[3], wide_out[4])

    ; the same window without the padding: at position 0 the window is just
    ; [1, 100], whose median is 50.5, and the spike survives
    moving:CsnArr    = csnmovmedian(data, 3)
    moving_out:i[]   = csntoarray(moving)
    prints("movmedian 3     : %g %g %g %g %g\n", moving_out[0], moving_out[1], moving_out[2], moving_out[3], moving_out[4])

    ; along one axis of a matrix: each row is filtered on its own
    shape:i[]        = fillarray(2, 5)
    flat:CsnArr      = csnfromarray(array(1, 5, 2, 4, 3, 5, 4, 3, 2, 1))
    matrix:CsnArr    = csnreshape(flat, shape)
    rows:CsnArr      = csnmedfilt1d(matrix, 3, 1)
    csnprint rows

    ; in place, no new handle: the source is rewritten
    copy:CsnArr      = csncopy(data)
    csnmedfilt1d(copy, 3)
    copy_out:i[]     = csntoarray(copy)
    prints("in place        : %g %g %g %g %g\n", copy_out[0], copy_out[1], copy_out[2], copy_out[3], copy_out[4])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
