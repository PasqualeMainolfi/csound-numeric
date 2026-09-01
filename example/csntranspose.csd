<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csntranspose.csd
;
; With no axes argument csntranspose reverses them all. For a matrix that is
; the ordinary transpose; the explicit form generalises it to any rank.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    shape:i[]      = fillarray(2, 3)
    mat:CsnArr     = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6)), shape)

    tr:CsnArr      = csntranspose(mat)
    tr_shape:i[]   = csnshape(tr)
    tr_out:i[]     = csntoarray(csnflatten(tr))
    prints("shape = %g x %g, flat = %g %g %g %g %g %g\n", tr_shape[0], tr_shape[1], tr_out[0], tr_out[1], tr_out[2], tr_out[3], tr_out[4], tr_out[5])

    ; the same thing said as an explicit permutation
    axes:i[]       = fillarray(1, 0)
    perm:CsnArr    = csntranspose(mat, axes)
    perm_out:i[]   = csntoarray(csnflatten(perm))
    prints("explicit  = %g %g %g %g %g %g\n", perm_out[0], perm_out[1], perm_out[2], perm_out[3], perm_out[4], perm_out[5])

    ; in place
    csntranspose(mat)
    now:i[]        = csnshape(mat)
    prints("source is now %g x %g\n", now[0], now[1])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
