<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnperm.csd
;
; csnperm lists the orderings of a 1-D array, one per row: all n! of them, or
; only the first few. Here the orderings of a three-note motif, then the first
; four of a four-note chord. The k-rate overload takes the count and a trigger,
; and lists again when its source changes.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    motif:CsnArr = csnfromarray(fillarray(60, 64, 67))
    orders:CsnArr = csnperm(motif)
    prints("every ordering of the motif:\n")
    csnprint(orders)

    chord:CsnArr = csnfromarray(fillarray(48, 55, 64, 70))
    first:CsnArr = csnperm(chord, 4)
    prints("the first four of 24:\n")
    csnprint(first)

    kTrig init 1
    kPass init 0
    kOnce init 1
    motif[0] = 60 + kPass
    live:CsnArr = csnperm(motif, 2, kTrig)
    if kPass == 2 then
        prints("after two passes:\n")
        csnprint(live, kOnce)
        kOnce = 0
    endif
    kPass += 1
endin

</CsInstruments>
<CsScore>
i 1 0 0.01
</CsScore>
</CsoundSynthesizer>
