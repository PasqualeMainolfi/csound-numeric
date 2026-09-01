<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnrand.csd
;
; csnrand draws uniformly from [min, max). Seeding first makes the run
; reproducible, which is what lets an example print fixed numbers at all.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    csnseed(12345)

    shape:i[]     = fillarray(6)
    noise:CsnArr  = csnrand(shape, -1, 1)
    lo:i          = csnmin(noise)
    hi:i          = csnmax(noise)
    n:i           = csnsize(noise)
    prints("n = %d, min = %.3f, max = %.3f\n", n, lo, hi)

    in_range:i    = (lo >= -1 && hi < 1 ? 1 : 0)
    prints("all values inside [-1, 1) = %d\n", in_range)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
