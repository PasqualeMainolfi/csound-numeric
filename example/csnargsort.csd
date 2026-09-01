<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnargsort.csd
;
; The permutation, not the values. That is what lets two parallel arrays be
; reordered together without losing which belongs to which.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    mags:CsnArr    = csnfromarray(array(0.3, 0.9, 0.1, 0.5))
    freqs:CsnArr   = csnfromarray(array(100, 200, 300, 400))

    order:CsnArr   = csnargsort(mags)
    order_out:i[]  = csntoarray(order)
    prints("order = %g %g %g %g\n", order_out[0], order_out[1], order_out[2], order_out[3])

    ; the loudest partial, and the frequency that goes with it
    loudest:i      = order_out[3]
    peak_mag:i     = csntake(mags, loudest)
    peak_freq:i    = csntake(freqs, loudest)
    prints("loudest: %.1f at %g Hz\n", peak_mag, peak_freq)

    ; reorder both arrays the same way
    n:i = 0
    while n < 4 do
        idx:i  = order_out[n]
        m:i    = csntake(mags, idx)
        f:i    = csntake(freqs, idx)
        prints("rank %d: %.1f at %g Hz\n", n, m, f)
        n += 1
    od
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
