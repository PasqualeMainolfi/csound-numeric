<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnreverse.csd
;
; csnreverse reads the array flat, so the rank does not matter: a 2 x 3 matrix
; comes back with its six elements in the opposite order, still 2 x 3.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    vec:CsnArr     = csnfromarray(array(1, 2, 3, 4))
    rev:CsnArr     = csnreverse(vec)
    rev_out:i[]    = csntoarray(rev)
    prints("vector = %g %g %g %g\n", rev_out[0], rev_out[1], rev_out[2], rev_out[3])

    shape:i[]      = fillarray(2, 3)
    mat:CsnArr     = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6)), shape)
    mat_rev:CsnArr = csnreverse(mat)
    mat_out:i[]    = csntoarray(csnflatten(mat_rev))
    dims:i         = csndims(mat_rev)
    prints("matrix (dims %d) = %g %g %g %g %g %g\n", dims, mat_out[0], mat_out[1], mat_out[2], mat_out[3], mat_out[4], mat_out[5])

    ; in place
    csnreverse(vec)
    now:i[]        = csntoarray(vec)
    prints("in place = %g %g %g %g\n", now[0], now[1], now[2], now[3])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
