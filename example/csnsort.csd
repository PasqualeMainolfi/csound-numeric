<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnsort.csd
;
; Ascending order, duplicates kept. With an axis every row is sorted on its own
; and the rows stay where they are.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr    = csnfromarray(array(3, 1, 4, 1, 5))

    sorted:CsnArr  = csnsort(data)
    sorted_out:i[] = csntoarray(sorted)
    prints("sorted = %g %g %g %g %g\n", sorted_out[0], sorted_out[1], sorted_out[2], sorted_out[3], sorted_out[4])

    ; the source is untouched
    src_out:i[]    = csntoarray(data)
    prints("source = %g %g %g %g %g\n", src_out[0], src_out[1], src_out[2], src_out[3], src_out[4])

    ; along an axis: every row sorted, rows in place
    shape:i[]      = fillarray(2, 3)
    mat:CsnArr     = csnreshape(csnfromarray(array(3, 1, 2, 9, 7, 8)), shape)
    rows:CsnArr    = csnsort(mat, 1)
    rows_out:i[]   = csntoarray(csnflatten(rows))
    prints("per row = %g %g %g | %g %g %g\n", rows_out[0], rows_out[1], rows_out[2], rows_out[3], rows_out[4], rows_out[5])

    ; in place
    csnsort(data)
    now:i[]        = csntoarray(data)
    prints("in place = %g %g %g %g %g\n", now[0], now[1], now[2], now[3], now[4])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
