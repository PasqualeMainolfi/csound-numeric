<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnsectosamp.csd
;
; Seconds into sample counts at a chosen rate: s sr. The companion of
; csnsamptosec, and the conversion that turns score times into buffer offsets.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    one:i           = csnsectosamp(1, 44100)
    prints("1 s at 44100    : %g samples\n", one)

    other:i         = csnsectosamp(1, 48000)
    prints("1 s at 48000    : %g samples\n", other)

    ; score times into offsets in a buffer
    times:CsnArr    = csnfromarray(array(0, 0.25, 0.5, 0.75, 1.0))
    offsets:CsnArr  = csnsectosamp(times, 44100)
    csnprint offsets

    ; a buffer long enough to hold the last of them
    total:i         = csnsectosamp(1.0, 44100)
    prints("buffer needed   : %g samples\n", total)

    ; and back
    back:CsnArr     = csnsamptosec(offsets, 44100)
    csnprint back
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
