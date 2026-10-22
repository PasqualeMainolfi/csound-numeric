<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnfrombase64.csd
;
; base64 text back to a string, and to the bytes of a MIDI identity
; request.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    name:S         = csnfrombase64("YmFzcyBwYWQ=")
    prints("name %s\n", name)
    sysex:CsnArr   = csnfrombase64("8H5/BgH3")
    csnprint(sysex)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
