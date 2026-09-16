<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnsave.csd
;
; csnsave chooses csnum's format or NumPy's format by extension. Both preserve
; shape and real/complex type through csnload.
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

    csnsave(mat, "csnsave_example.npy")
    back_npy:CsnArr = csnload("csnsave_example.npy")
    npy_out:i[] = csntoarray(csnflatten(back_npy))
    prints("NumPy round trip: first = %g, last = %g\n", npy_out[0], npy_out[5])

    ; the element type survives too
    cpx:CsnArr    = csntocomplex(csnflatten(mat))
    csnsave(cpx, "csnsave_example_c.csn")
    back_cpx:CsnArr = csnload("csnsave_example_c.csn")
    itype:i       = csntype(back_cpx)
    prints("complex round trip itype = %d\n", itype)
    csnsave(cpx, "csnsave_example_c.npy")
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
