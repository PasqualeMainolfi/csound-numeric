<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnsamptosec.csd
;
; Sample counts into seconds at a chosen rate: n / sr. The same conversion
; csnsamptomillis performs, on the unit Csound scores are written in.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    one:i           = csnsamptosec(44100, 44100)
    prints("44100 sm at 44100: %g s\n", one)

    other:i         = csnsamptosec(44100, 48000)
    prints("44100 sm at 48000: %g s\n", other)

    ; frame boundaries of an analysis, as score times
    frames:CsnArr   = csnfromarray(array(0, 512, 1024, 1536, 2048))
    seconds:CsnArr  = csnsamptosec(frames, 44100)
    csnprint seconds

    ; the hop between two of them
    hop:i           = csnsamptosec(512, 44100)
    prints("hop of 512      : %g s\n", hop)

    ; round trip
    back:CsnArr     = csnsectosamp(seconds, 44100)
    csnprint back
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
