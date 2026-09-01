<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnpairdist.csd
;
; One distance per pair, with the shape kept. csndist collapses the same data to
; a single number.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    a:CsnArr      = csnfromarray(array(1, 2, 3))
    b:CsnArr      = csnfromarray(array(4, 1, 9))

    each:CsnArr   = csnpairdist(a, b)
    each_out:i[]  = csntoarray(each)
    prints("per element = %g %g %g\n", each_out[0], each_out[1], each_out[2])

    ; where the two arrays differ most
    at:CsnArr     = csnargmax(each)
    at_out:i[]    = csntoarray(csnflatten(at))
    worst:i       = csnmax(each)
    prints("largest difference %g at index %g\n", worst, at_out[0])

    ; summing the pairs gives the order-1 distance
    total:i       = csnsum(each)
    manhattan:i   = csndist(a, b)
    prints("sum = %g, csndist order 1 = %g\n", total, manhattan)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
