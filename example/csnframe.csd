<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnframe.csd
;
; csnframe cuts an axis into overlapping frames: frame 3, hop 2 over seven
; samples gives three frames. The k-rate overload takes a trigger and then the
; axis, which it requires, and frames again when its source changes.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    x:CsnArr = csnarange(0, 7, 1)

    frames:CsnArr = csnframe(x, 3, 2)
    prints("frames of a vector:\n")
    csnprint(frames)

    iShape[] = fillarray(2, 7)
    m:CsnArr = csnreshape(csnarange(0, 14, 1), iShape)
    rows:CsnArr = csnframe(m, 4, 3, -1)
    prints("each row framed:\n")
    csnprint(rows)

    kTrig init 1
    kOnce init 1
    kPass init 0
    x[0] = kPass
    live:CsnArr = csnframe(x, 3, 2, kTrig, -1)
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
