<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csntype.csd
;
; csntype tells real from complex. Mixing the two promotes the result, which is
; the case worth checking before calling a real-only opcode.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr  = csnfromarray(array(1, 2, 3, 4))
    cpx:CsnArr   = csntocomplex(data)

    data_type:i  = csntype(data)
    cpx_type:i   = csntype(cpx)
    prints("real handle = %d, complex handle = %d\n", data_type, cpx_type)

    ; an operation that mixes the two promotes the result
    mixed:CsnArr = csnadd(data, cpx)
    mixed_type:i = csntype(mixed)
    prints("real + complex = %d\n", mixed_type)

    ; branch before calling a real-only opcode
    if mixed_type == 0 then
        sorted:CsnArr = csnsort(mixed)
        prints("sorted\n")
    else
        prints("complex: sorting is undefined, taking the real parts instead\n")
        re:CsnArr  = csnreal(mixed)
        re_out:i[] = csntoarray(re)
        prints("real parts = %g %g %g %g\n", re_out[0], re_out[1], re_out[2], re_out[3])
    endif
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
