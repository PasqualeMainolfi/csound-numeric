<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnlike.csd
;
; csnlike copies the geometry of an array, not its data: same shape, same
; element type, every element set to the value you give.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    shape:i[]    = fillarray(2, 3)
    src:CsnArr   = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6)), shape)

    mask:CsnArr  = csnlike(src, 0.5)
    dims:i       = csndims(mask)
    size:i       = csnsize(mask)
    mask_out:i[] = csntoarray(csnflatten(mask))
    prints("dims = %d, size = %d, first = %g\n", dims, size, mask_out[0])

    ; the element type follows the source
    cpx:CsnArr   = csntocomplex(src)
    comp:CsnArr  = csnlike(cpx, 1)
    itype:i      = csntype(comp)
    prints("itype of csnlike(complex) = %d\n", itype)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
