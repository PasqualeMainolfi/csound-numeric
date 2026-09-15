<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnbincount.csd
;
; Count integer bins, leave gaps at zero, and optionally accumulate one weight
; per source element.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    source:CsnArr     = csnfromarray(array(0, 1, 1, 3))

    counts:CsnArr     = csnbincount(source)
    counts_out:i[]    = csntoarray(counts)
    prints("counts  = %g %g %g %g\n", counts_out[0], counts_out[1], counts_out[2], counts_out[3])

    weights:CsnArr    = csnfromarray(array(0.5, 1, 2, 4))
    weighted:CsnArr   = csnbincount(source, weights)
    weighted_out:i[]  = csntoarray(weighted)
    prints("weighted = %g %g %g %g\n", weighted_out[0], weighted_out[1], weighted_out[2], weighted_out[3])

    empty:CsnArr      = csnbincount(csnempty(array(0)))
    prints("empty size = %d\n", csnsize(empty))
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
