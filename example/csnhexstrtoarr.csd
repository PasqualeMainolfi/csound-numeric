<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnhexstrtoarr.csd
;
; A pattern in hex beat notation, one digit for every four steps: the digits,
; and each one back as its four steps.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    digits:CsnStr  = csnhexstrtoarr("0x8A2F")
    csnprint(digits)
    beats:CsnArr   = csnfromhex(digits)
    steps:CsnStr   = csntobinary(beats, 4)
    csnprint(steps)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
