<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csntobuffer.csd
;
; Four 16-bit PCM samples as the bytes a WAV file stores them, little-endian,
; then as base64 text for an OSC string; and a UDP port in network order.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    samples:i[]    = fillarray(0, 1000, -1000, 32767)
    pcm:CsnArr     = csnfromarray(samples)
    bytes:CsnArr   = csntobuffer(pcm, "<i2")
    csnprint(bytes)
    text:S         = csntobase64(bytes)
    prints("base64 %s\n", text)
    port:CsnArr    = csnfromarray(fillarray(57120))
    wire:CsnArr    = csntobuffer(port, ">u2")
    csnprint(wire)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
