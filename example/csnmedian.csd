<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnmedian.csd
;
; The robust centre: a few extreme values move the mean and barely move the
; median.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr   = csnfromarray(array(2, 4, 4, 4, 5, 5, 7, 9))
    mid:i         = csnmedian(data)
    avg:i         = csnmean(data)
    prints("median = %g, mean = %g\n", mid, avg)

    ; one outlier moves the mean and barely moves the median
    spiked:CsnArr = csnfromarray(array(2, 4, 4, 4, 5, 5, 7, 900))
    mid2:i        = csnmedian(spiked)
    avg2:i        = csnmean(spiked)
    prints("with an outlier: median = %g, mean = %g\n", mid2, avg2)

    ; along an axis
    shape:i[]     = fillarray(2, 3)
    mat:CsnArr    = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6)), shape)
    rows:CsnArr   = csnmedian(mat, 1)
    rows_out:i[]  = csntoarray(rows)
    prints("per row = %g %g\n", rows_out[0], rows_out[1])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
