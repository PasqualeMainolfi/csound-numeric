<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnge.csd
;
; The result is a mask, not a selection. Multiply by it to gate, sum it to
; count, csnargnonzero it to get the positions.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr   = csnfromarray(array(-2, 0, 1, 3, 5))

    mask:CsnArr   = csnge(data, 1)
    mask_out:i[]  = csntoarray(mask)
    prints("mask  = %g %g %g %g %g\n", mask_out[0], mask_out[1], mask_out[2], mask_out[3], mask_out[4])

    hits:i        = csnsum(mask)
    prints("hits  = %g\n", hits)

    ; gate the data with its own mask
    gated:CsnArr  = csnmul(data, mask)
    gated_out:i[] = csntoarray(gated)
    prints("gated = %g %g %g %g %g\n", gated_out[0], gated_out[1], gated_out[2], gated_out[3], gated_out[4])

    ; and the positions that passed
    where:CsnArr  = csnargnonzero(mask)
    where_n:i     = csnsize(where)
    prints("passing positions = %d\n", where_n)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
