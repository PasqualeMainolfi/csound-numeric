<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnhoaordtochnls.csd
;
; How many channels a complete 3-D ambisonics set of a given order carries:
;
;   channels = (order + 1)^2
;
; so 1, 4, 9, 16, 25 ... csnchnlstohoaord reads the order back, and refuses a
; count that is not one of those.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    iord = 0
    while iord <= 4 do
        ich = csnhoaordtochnls(iord)
        iback = csnchnlstohoaord(ich)
        prints("order %d -> %2d channels -> order %d\n", iord, ich, iback)
        iord += 1
    od

    ; the count is also the length of the SN3D gain vector, and the index one
    ; past the last valid ACN
    ich3 = csnhoaordtochnls(3)
    in, im csnacntonm ich3 - 1
    prints("order 3: %d channels, last ACN %d is order %d degree %d\n", ich3, ich3 - 1, in, im)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
