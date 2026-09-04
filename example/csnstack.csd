<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnstack.csd
;
; csnstack inserts a new axis and places each equal-shaped input at one position
; along it. The k-rate overload takes its trigger before the axis.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    a:CsnArr = csnfromarray(array(1, 2))
    b:CsnArr = csnfromarray(array(3, 4))
    c:CsnArr = csnfromarray(array(5, 6))

    by_rows:CsnArr = csnstack(0, a, b, c)
    prints("axis 0:\n")
    csnprint(by_rows)

    kTrig init 1
    kAxis init 1
    kOnce init 1
    by_columns:CsnArr = csnstack(kTrig, kAxis, a, b, c)
    prints("axis 1:\n")
    csnprint(by_columns, kOnce)
    kOnce = 0
endin

</CsInstruments>
<CsScore>
i 1 0 0.01
</CsScore>
</CsoundSynthesizer>
