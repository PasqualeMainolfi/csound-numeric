<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnpercentile.csd
;
; 50 is the median, 0 the minimum, 100 the maximum. Positions between two
; elements are interpolated, so the answer need not be an element of the array.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr  = csnfromarray(array(2, 4, 4, 4, 5, 5, 7, 9))

    pc0:i         = csnpercentile(data, 0)
    pc25:i        = csnpercentile(data, 25)
    pc50:i        = csnpercentile(data, 50)
    pc75:i        = csnpercentile(data, 75)
    pc100:i       = csnpercentile(data, 100)
    prints("0=%g 25=%g 50=%g 75=%g 100=%g\n", pc0, pc25, pc50, pc75, pc100)

    ; the ends agree with csnmin and csnmax, the middle with csnmedian
    lo:i         = csnmin(data)
    hi:i         = csnmax(data)
    mid:i        = csnmedian(data)
    prints("min=%g max=%g median=%g\n", lo, hi, mid)

    ; the interquartile range, a robust measure of spread
    iqr:i        = pc75 - pc25
    prints("interquartile range = %g\n", iqr)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
