<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csntobase64.csd
;
; A patch name and a MIDI identity request, as base64 text that travels in
; JSON or an OSC string.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    name:S         = csntobase64("bass pad")
    prints("name %s\n", name)
    request:i[]    = fillarray(240, 126, 127, 6, 1, 247)
    sysex:CsnArr   = csnfromarray(request)
    text:S         = csntobase64(sysex)
    prints("sysex %s\n", text)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
