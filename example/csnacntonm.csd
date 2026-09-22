<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnacntonm.csd
;
; The order and degree an ACN channel index stands for:
;
;   n = floor(sqrt(ACN))
;   m = ACN - n*n - n
;
; It is the inverse of csnnmtoacn, and answers two values rather than one.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    ; walk the first two orders, channel by channel
    ik = 0
    while ik < 9 do
        in, im csnacntonm ik
        prints("ACN %d -> order %d degree %2d\n", ik, in, im)
        ik += 1
    od

    ; the round trip closes
    in2, im2 csnacntonm 15
    iback = csnnmtoacn(in2, im2)
    prints("15 -> (%d, %d) -> %d\n", in2, im2, iback)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
