<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csntruncate.csd
;
; Without an axis the array is read flat. With one, only that axis is cut and
; the rank is preserved: a 2 x 3 truncated to 2 on axis 1 is a 2 x 2.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    vec:CsnArr    = csnfromarray(array(1, 2, 3, 4, 5, 6))
    short:CsnArr  = csntruncate(vec, 3)
    short_out:i[] = csntoarray(short)
    n:i           = csnsize(short)
    prints("flat n = %d, values = %g %g %g\n", n, short_out[0], short_out[1], short_out[2])

    shape:i[]     = fillarray(2, 3)
    mat:CsnArr    = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6)), shape)
    cut:CsnArr    = csntruncate(mat, 2, 1)
    cut_shape:i[] = csnshape(cut)
    cut_out:i[]   = csntoarray(csnflatten(cut))
    prints("axis 1: %g x %g, values = %g %g %g %g\n", cut_shape[0], cut_shape[1], cut_out[0], cut_out[1], cut_out[2], cut_out[3])

    ; in place
    csntruncate(vec, 2)
    now_n:i       = csnsize(vec)
    now:i[]       = csntoarray(vec)
    prints("in place n = %d, values = %g %g\n", now_n, now[0], now[1])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
