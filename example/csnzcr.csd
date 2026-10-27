<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnzcr.csd
;
; csnzcr counts the sign changes of a signal over its length: one number for a
; 1-D source, one value per frame for a framed one. The k-rate overloads take a
; trigger, then the axis for the array output.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    x:CsnArr = csnfromarray(fillarray(1, -1, 0, 2, -3, -1e-12, 4, 0, -2, 5, 6, -7))

    rate:i = csnzcr(x)
    prints("whole signal: %.4f\n", rate)

    frames:CsnArr = csnframe(x, 4, 2)
    per_frame:CsnArr = csnzcr(frames)
    prints("per frame:\n")
    csnprint(per_frame)

    kTrig init 1
    kOnce init 1
    live:CsnArr = csnzcr(frames, kTrig, -1)
    prints("k-rate, per frame:\n")
    csnprint(live, kOnce)
    kOnce = 0
endin

</CsInstruments>
<CsScore>
i 1 0 0.01
</CsScore>
</CsoundSynthesizer>
