<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnargnonzero.csd
;
; Comparison first, then csnargnonzero: the mask says which elements passed and
; this turns the mask into the list of positions.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    vec:CsnArr    = csnfromarray(array(0, 7, 0, 9))
    hits:CsnArr   = csnargnonzero(vec)
    hits_out:i[]  = csntoarray(csnflatten(hits))
    count:i       = csnsize(hits)
    prints("non-zero: %d, at %g and %g\n", count, hits_out[0], hits_out[1])

    ; the idiom: compare, then locate
    data:CsnArr   = csnfromarray(array(0.1, 0.8, 0.3, 0.95, 0.2))
    loud:CsnArr   = csngt(data, 0.5)
    where:CsnArr  = csnargnonzero(loud)
    where_out:i[] = csntoarray(csnflatten(where))
    where_n:i     = csnsize(where)
    prints("above 0.5: %d, at %g and %g\n", where_n, where_out[0], where_out[1])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
