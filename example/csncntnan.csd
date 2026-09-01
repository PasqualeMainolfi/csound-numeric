<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csncntnan.csd
;
; A cheap validity check: run it after anything that can leave the real domain,
; before the statistics that a NaN would poison.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr  = csnfromarray(array(4, -1, 9, -16))
    roots:CsnArr = csnsqrt(data)

    bad:i        = csncntnan(roots)
    prints("NaN elements = %d\n", bad)

    ; a NaN propagates through the reductions, so check before trusting them
    if bad == 0 then
        avg:i = csnmean(roots)
        prints("mean = %g\n", avg)
    else
        prints("skipping the mean: %d NaN elements\n", bad)
        safe:CsnArr = csnsqrt(csnclip(data, 0, 1000))
        avg2:i      = csnmean(safe)
        prints("mean after clipping = %g\n", avg2)
    endif
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
