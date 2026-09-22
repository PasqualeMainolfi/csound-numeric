<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnchnlstohoaord.csd
;
; The ambisonics order a channel count stands for:
;
;   order = sqrt(channels) - 1
;
; Only a complete 3-D set has an order: 1, 4, 9, 16, 25 ... channels. Anything
; else is refused rather than rounded, because a fractional order is not a
; smaller set, it is a set that does not exist.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    i1  = csnchnlstohoaord(1)
    i4  = csnchnlstohoaord(4)
    i9  = csnchnlstohoaord(9)
    i16 = csnchnlstohoaord(16)
    prints("1 -> %d, 4 -> %d, 9 -> %d, 16 -> %d\n", i1, i4, i9, i16)

    ; the trip back
    iback = csnhoaordtochnls(i16)
    prints("order %d is %d channels again\n", i16, iback)
    turnoff
endin

instr 2
    ; 5 channels is not a complete set: refused, not rounded down to order 1
    ibad = csnchnlstohoaord(5)
    prints("never reached\n")
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0    0.1
i 2 0.15 0.1
</CsScore>
</CsoundSynthesizer>
