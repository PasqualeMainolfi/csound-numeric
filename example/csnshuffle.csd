<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnshuffle.csd
;
; csnshuffle randomly permutes an array in place. Its shape stays unchanged;
; for a multidimensional array, the operation shuffles the flat element order.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    values:CsnArr = csnfromarray(array(1, 2, 3, 4, 5, 6))

    prints("before:\n")
    csnprint(values)

    csnseed(12345)
    csnshuffle(values)

    prints("after:\n")
    csnprint(values)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
