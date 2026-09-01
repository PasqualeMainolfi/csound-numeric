<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnsave.csd
;
; csnsave stores the element type and the shape alongside the payload, so the
; round trip through csnload is lossless: a 2 x 3 complex array comes back a
; 2 x 3 complex array.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    shape:i[]     = fillarray(2, 3)
    mat:CsnArr    = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6)), shape)
    csnsave(mat, "csnsave_example.csn")

    back:CsnArr   = csnload("csnsave_example.csn")
    dims:i        = csndims(back)
    size:i        = csnsize(back)
    back_out:i[]  = csntoarray(csnflatten(back))
    prints("dims = %d, size = %d, values = %g %g %g %g %g %g\n", dims, size, back_out[0], back_out[1], back_out[2], back_out[3], back_out[4], back_out[5])

    ; the element type survives too
    cpx:CsnArr    = csntocomplex(csnflatten(mat))
    csnsave(cpx, "csnsave_example_c.csn")
    back_cpx:CsnArr = csnload("csnsave_example_c.csn")
    itype:i       = csntype(back_cpx)
    prints("complex round trip itype = %d\n", itype)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
