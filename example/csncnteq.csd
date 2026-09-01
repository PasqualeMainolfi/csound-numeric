<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csncnteq.csd
;
; One pass, no mask. csneq plus csnsum says the same thing the long way, and
; csnargwhere says where rather than how many.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr  = csnfromarray(array(1, 5, 3, 5, 2, 5))

    fives:i      = csncnteq(data, 5)
    zeros:i      = csncnteq(data, 0)
    prints("fives = %d, zeros = %d\n", fives, zeros)

    ; the same answer the long way
    mask:CsnArr  = csneq(data, 5)
    by_mask:i    = csnsum(mask)
    prints("via csneq + csnsum = %g\n", by_mask)

    ; and the positions, when those are what is wanted
    wanted:CsnArr = csnfromarray(array(5))
    where:CsnArr  = csnargwhere(data, wanted)
    where_out:i[] = csntoarray(csnflatten(where))
    prints("at %g %g %g\n", where_out[0], where_out[1], where_out[2])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
