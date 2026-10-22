<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csntohex.csd
;
; The three bytes of a MIDI note-on, as hex: status, key and velocity.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    status:S       = csntohex(144)
    prints("status 0x%s\n", status)
    message:i[]    = fillarray(144, 60, 100)
    bytes:CsnArr   = csnfromarray(message)
    text:CsnStr    = csntohex(bytes, 2)
    csnprint(text)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
