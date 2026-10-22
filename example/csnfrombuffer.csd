<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnfrombuffer.csd
;
; The sample rate and byte rate of a WAV header, two little-endian 32-bit
; fields, read from its bytes; and a base64 OSC payload back to 16-bit samples.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    header:i[]     = fillarray(68, 172, 0, 0, 16, 177, 2, 0)
    fields:CsnArr  = csnfrombuffer(csnfromarray(header), "<u4")
    csnprint(fields)
    payload:CsnArr = csnfrombase64("AADoAxj8/38=")
    samples:CsnArr = csnfrombuffer(payload, "<i2")
    csnprint(samples)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
