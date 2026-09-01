<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csncntnz.csd
;
; Over raw data it is the density; over a mask it is the number of elements that
; passed the test.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr   = csnfromarray(array(0, 3, 0, 0, 7, 2))

    filled:i      = csncntnz(data)
    total:i       = csnsize(data)
    prints("non-zero = %d of %d\n", filled, total)

    ; over a mask: how many elements passed
    loud:CsnArr   = csngt(data, 2)
    passed:i      = csncntnz(loud)
    prints("above 2  = %d\n", passed)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
