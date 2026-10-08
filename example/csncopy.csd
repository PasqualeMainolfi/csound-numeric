<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csncopy.csd
;
; Assignment and csncopy both make independent arrays. An in-place write to
; the source leaves both copies unchanged.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    iValues[] = fillarray(3, 1, 2)
    src:CsnArr      = csnfromarray(iValues)
    assigned:CsnArr = src
    copied:CsnArr   = csncopy(src)

    ; sort src in place: neither copy follows the mutation
    csnsort(src)

    src_out:i[]      = csntoarray(src)
    assigned_out:i[] = csntoarray(assigned)
    copied_out:i[]   = csntoarray(copied)
    prints("src      = %g %g %g\n", src_out[0], src_out[1], src_out[2])
    prints("assigned = %g %g %g\n", assigned_out[0], assigned_out[1], assigned_out[2])
    prints("copied   = %g %g %g\n", copied_out[0], copied_out[1], copied_out[2])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
