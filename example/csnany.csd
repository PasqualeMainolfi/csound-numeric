<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnany.csd
;
; "Did anything clip" is a comparison and a csnany. With an axis the same
; question is answered per row.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    signal:CsnArr  = csnfromarray(array(0.2, 0.9, 1.4, 0.1))
    clipped:i      = csnany(csngt(signal, 1))
    silent:i       = csnany(csneq(signal, 0))
    prints("anything above 1 = %d, any exact zero = %d\n", clipped, silent)

    ; per row
    shape:i[]      = fillarray(2, 3)
    mat:CsnArr     = csnreshape(csnfromarray(array(0, 0, 0, 0, 5, 0)), shape)
    rows:CsnArr    = csnany(mat, 1)
    rows_out:i[]   = csntoarray(rows)
    prints("row 0 has a non-zero = %g, row 1 = %g\n", rows_out[0], rows_out[1])

    ; the neutral answer over nothing
    cap:i[]        = fillarray(4)
    nothing:CsnArr = csnempty(cap)
    over_empty:i   = csnany(nothing)
    prints("over an empty array = %d\n", over_empty)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
