<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csncopy.csd
;
; Assigning a handle aliases the array; csncopy duplicates it. The difference
; shows the moment an in-place opcode writes.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    src:CsnArr    = csnfromarray(array(3, 1, 2))
    alias:CsnArr  = src
    copy:CsnArr   = csncopy(src)

    ; sort src in place: the alias follows, the copy does not
    csnsort(src)

    src_out:i[]   = csntoarray(src)
    alias_out:i[] = csntoarray(alias)
    copy_out:i[]  = csntoarray(copy)
    prints("src   = %g %g %g\n", src_out[0], src_out[1], src_out[2])
    prints("alias = %g %g %g\n", alias_out[0], alias_out[1], alias_out[2])
    prints("copy  = %g %g %g\n", copy_out[0], copy_out[1], copy_out[2])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
