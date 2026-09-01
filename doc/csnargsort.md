# csnargsort

## Abstract

The indices that would sort the array.

## Description

`csnargsort` returns the permutation that puts the array in ascending order:
element `n` of the result is the index of the element that belongs at position
`n`. Sorting `3 1 4 1 5` gives `1 3 0 2 4`.

That indirection is the point. When two arrays run in parallel — magnitudes and
the frequencies they belong to, say — sorting one directly loses the pairing,
while the index array reorders both the same way through
[csntake](csntake.md) or [csnget](csnget.md).

With no axis the array is read flat; given an axis each line along it is ordered
on its own.

Real only — ordering has no meaning over the complex field.

## Syntax

```csound
handle:CsnArr = csnargsort(source:CsnArr)
handle:CsnArr = csnargsort(source:CsnArr, axis:i)
handle:CsnArr = csnargsort(source:CsnArr, axis:k)
handle:CsnArr = csnargsort(source:CsnArr, axis:k, trig:k)
```

## Arguments

* `source:CsnArr`: the array to order.
* `axis:i / axis:k` (optional, default `-1`): the axis to order along; `-1` reads the array flat.
* `trig:k` (optional, default `1`): k-rate trigger. A zero trigger republishes the previous result.

## Output

* `handle:CsnArr`: the indices that would sort the array.

## Execution Time

* Init
* Performance (k-rate)

## Examples

```csound
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
```

## See also

* [csnsort](csnsort.md)
* [csnargmax](csnargmax.md)
* [csntake](csntake.md)
* [csnargunique](csnargunique.md)

## Credits

Pasquale Mainolfi, 2026
