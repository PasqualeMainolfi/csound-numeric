<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnfromhex.csd
;
; A MIDI note-on written in hex, read back as its three bytes.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    top:i          = csnfromhex("0x7F")
    prints("0x7F is %d\n", top)
    words:S[]      = fillarray("90", "3C", "64")
    message:CsnStr = csnfromarray(words)
    bytes:CsnArr   = csnfromhex(message)
    csnprint(bytes)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
