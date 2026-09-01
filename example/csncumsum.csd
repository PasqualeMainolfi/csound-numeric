<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csncumsum.csd
;
; The discrete integral. Over an array of durations it gives the onset times,
; which is its commonest use in a score generator.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr    = csnfromarray(array(1, 2, 3, 4))
    running:CsnArr = csncumsum(data)
    running_out:i[] = csntoarray(running)
    prints("cumsum = %g %g %g %g\n", running_out[0], running_out[1], running_out[2], running_out[3])

    ; durations to onsets
    dur:CsnArr     = csnfromarray(array(0.5, 0.25, 0.25, 1))
    onset:CsnArr   = csncumsum(dur)
    onset_out:i[]  = csntoarray(onset)
    prints("onsets = %g %g %g %g\n", onset_out[0], onset_out[1], onset_out[2], onset_out[3])

    ; along an axis: each row accumulates on its own
    shape:i[]      = fillarray(2, 3)
    mat:CsnArr     = csnreshape(csnfromarray(array(1, 9, 3, 4, 5, 6)), shape)
    rows:CsnArr    = csncumsum(mat, 1)
    rows_out:i[]   = csntoarray(csnflatten(rows))
    prints("per row = %g %g %g | %g %g %g\n", rows_out[0], rows_out[1], rows_out[2], rows_out[3], rows_out[4], rows_out[5])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
