<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csntobinary.csd
;
; A 16-step drum pattern kept as one number, written out as its steps, and
; the four bit masks of a 4-step bar.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    steps:S        = csntobinary(34952, 16)
    prints("pattern %s\n", steps)
    masks:i[]      = fillarray(1, 2, 4, 8)
    bits:CsnArr    = csnfromarray(masks)
    rows:CsnStr    = csntobinary(bits, 4)
    csnprint(rows)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
