<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnflatten.csd
;
; csnflatten is the shape that always fits. Its usual job is to let a matrix of
; any rank be received by a plain i[] output.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    shape:i[]    = fillarray(2, 3)
    mat:CsnArr   = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6)), shape)

    flat:CsnArr  = csnflatten(mat)
    flat_out:i[] = csntoarray(flat)
    flat_dims:i  = csndims(flat)
    prints("dims = %d, values = %g %g %g %g %g %g\n", flat_dims, flat_out[0], flat_out[1], flat_out[2], flat_out[3], flat_out[4], flat_out[5])

    ; the source keeps its rank
    mat_dims:i   = csndims(mat)
    prints("source dims still = %d\n", mat_dims)

    ; in place
    csnflatten(mat)
    now_dims:i   = csndims(mat)
    prints("source dims after the in-place call = %d\n", now_dims)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
