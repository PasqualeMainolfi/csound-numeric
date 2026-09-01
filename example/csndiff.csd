<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csndiff.csd
;
; The discrete derivative, one element shorter than its source. Over onset times
; it gives the durations back.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr   = csnfromarray(array(1, 4, 9, 16))
    steps:CsnArr  = csndiff(data)
    steps_out:i[] = csntoarray(steps)
    n:i           = csnsize(steps)
    prints("n = %d, diff = %g %g %g\n", n, steps_out[0], steps_out[1], steps_out[2])

    ; onsets back to durations, the inverse of csncumsum
    onset:CsnArr  = csnfromarray(array(0, 0.5, 0.75, 1))
    dur:CsnArr    = csndiff(onset)
    dur_out:i[]   = csntoarray(dur)
    prints("durations = %g %g %g\n", dur_out[0], dur_out[1], dur_out[2])

    ; along an axis
    shape:i[]     = fillarray(2, 3)
    mat:CsnArr    = csnreshape(csnfromarray(array(1, 3, 6, 10, 15, 21)), shape)
    rows:CsnArr   = csndiff(mat, 1)
    rows_shape:i[] = csnshape(rows)
    rows_out:i[]  = csntoarray(csnflatten(rows))
    prints("per row: %g x %g = %g %g %g %g\n", rows_shape[0], rows_shape[1], rows_out[0], rows_out[1], rows_out[2], rows_out[3])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
