<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnfrombinary.csd
;
; Drum patterns written as steps, read back as the numbers that hold them.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    kick:i         = csnfrombinary("1000100010001000")
    snare:i        = csnfrombinary("0b0000100000001000")
    prints("kick %d, snare %d\n", kick, snare)
    lines:S[]      = fillarray("1010", "0110", "1111")
    patterns:CsnStr = csnfromarray(lines)
    values:CsnArr  = csnfrombinary(patterns)
    csnprint(values)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
