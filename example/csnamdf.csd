<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnamdf.csd
;
; csnamdf gives the average magnitude difference of a signal with itself
; delayed by 0 to maxlag samples. A periodic signal dips to 0 at its period:
; here 4 samples. The period is the first dip after lag 0, found with
; csnargmin on the lags worth searching; the k-rate overload takes maxlag,
; a trigger, then the axis.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    x:CsnArr = csnfromarray(fillarray(0, 1, 0, -1, 0, 1, 0, -1, 0, 1, 0, -1))

    curve:CsnArr = csnamdf(x, 6)
    prints("D(tau), tau 0..6:\n")
    csnprint(curve)

    ; search lags 2..6: lag 0 is always 0
    search:CsnArr = curve[2:6]
    at:CsnArr = csnargmin(search)
    period:i = at[0][0] + 2
    prints("period: %d samples\n", period)

    iShape[] = fillarray(2, 6)
    rows:CsnArr = csnreshape(csnfromarray(fillarray(0, 1, 0, -1, 0, 1, 1, 2, 3, 4, 5, 6)), iShape)
    kTrig init 1
    kOnce init 1
    per_row:CsnArr = csnamdf(rows, 2, kTrig, -1)
    prints("k-rate, one curve per row:\n")
    csnprint(per_row, kOnce)
    kOnce = 0
endin

</CsInstruments>
<CsScore>
i 1 0 0.01
</CsScore>
</CsoundSynthesizer>
