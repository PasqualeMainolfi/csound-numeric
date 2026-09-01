<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnarange.csd
;
; csnarange walks from start to stop by step, stop excluded. The count follows
; from the three bounds, so it is the opcode to reach for when the step matters
; more than the length.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    up:CsnArr    = csnarange(0, 5, 1)
    up_out:i[]   = csntoarray(up)
    up_n:i       = csnsize(up)
    prints("count = %d, values = %g %g %g %g %g\n", up_n, up_out[0], up_out[1], up_out[2], up_out[3], up_out[4])

    by3:CsnArr   = csnarange(2, 12, 3)
    by3_out:i[]  = csntoarray(by3)
    by3_n:i      = csnsize(by3)
    prints("count = %d, values = %g %g %g %g\n", by3_n, by3_out[0], by3_out[1], by3_out[2], by3_out[3])

    fractional:CsnArr = csnarange(0, 1, 0.25)
    fractional_out:i[] = csntoarray(fractional)
    prints("fractional = %g %g %g %g\n", fractional_out[0], fractional_out[1], fractional_out[2], fractional_out[3])

    descending:CsnArr = csnarange(10, 6, -2)
    descending_out:i[] = csntoarray(descending)
    prints("descending = %g %g\n", descending_out[0], descending_out[1])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
