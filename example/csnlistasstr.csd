<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnlistasstr.csd
;
; The partials of a harmonic tone written as one line, ready for a log or an
; OSC message.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    ratios:CsnArr  = csnlinspace(1, 4, 4)
    partials:CsnArr = ratios * 110
    line:S         = csnlistasstr(partials, ", ")
    prints("partials %s\n", line)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
