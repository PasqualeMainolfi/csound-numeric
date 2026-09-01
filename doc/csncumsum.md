# csncumsum

## Abstract

Cumulative sum of the elements.

## Description

`csncumsum` returns a running total: element `n` of the result is the sum of
elements `0 … n` of the source. The shape is unchanged, so `1 2 3 4` gives
`1 3 6 10`.

With no axis the array is read flat. Given an axis each line along it accumulates
on its own.

It is the discrete integral, and the inverse of [csndiff](csndiff.md) up to the
first element: a difference of a cumulative sum gives the original back, minus its
head.

Both real and complex arrays are accepted.

## Syntax

```csound
handle:CsnArr = csncumsum(source:CsnArr)
handle:CsnArr = csncumsum(source:CsnArr, axis:i)
handle:CsnArr = csncumsum(source:CsnArr, axis:k)
handle:CsnArr = csncumsum(source:CsnArr, axis:k, trig:k)
```

## Arguments

* `source:CsnArr`: the array to accumulate.
* `axis:i / axis:k` (optional, default `-1`): the axis to accumulate along; `-1` reads the array flat.
* `trig:k` (optional, default `1`): k-rate trigger. A zero trigger republishes the previous result.

## Output

* `handle:CsnArr`: the running totals, with the shape of the source.

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
; csncumsum.csd
;
; The discrete integral. Over an array of durations it gives the onset times,
; which is its commonest use in a score generator.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr    = csnfromarray(array(1, 2, 3, 4))
    running:CsnArr = csncumsum(data)
    running_out:i[] = csntoarray(running)
    prints("cumsum = %g %g %g %g\n", running_out[0], running_out[1], running_out[2], running_out[3])

    ; durations to onsets
    dur:CsnArr     = csnfromarray(array(0.5, 0.25, 0.25, 1))
    onset:CsnArr   = csncumsum(dur)
    onset_out:i[]  = csntoarray(onset)
    prints("onsets = %g %g %g %g\n", onset_out[0], onset_out[1], onset_out[2], onset_out[3])

    ; along an axis: each row accumulates on its own
    shape:i[]      = fillarray(2, 3)
    mat:CsnArr     = csnreshape(csnfromarray(array(1, 9, 3, 4, 5, 6)), shape)
    rows:CsnArr    = csncumsum(mat, 1)
    rows_out:i[]   = csntoarray(csnflatten(rows))
    prints("per row = %g %g %g | %g %g %g\n", rows_out[0], rows_out[1], rows_out[2], rows_out[3], rows_out[4], rows_out[5])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csncumprod](csncumprod.md)
* [csndiff](csndiff.md)
* [csnsum](csnsum.md)

## Credits

Pasquale Mainolfi, 2026
