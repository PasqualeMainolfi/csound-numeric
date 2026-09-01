# csndiff

## Abstract

Differences between consecutive elements.

## Description

`csndiff` returns the step from each element to the next: element `n` of the
result is `source[n+1] - source[n]`. The result is therefore **one element
shorter** than the source along the axis it works on.

With no axis the array is read flat. Given an axis each line along it is
differenced on its own, and that axis loses one element.

It is the discrete derivative, and the inverse of [csncumsum](csncumsum.md) up to
the first element. Where the result must keep the source's length,
[csngrad](csngrad.md) is the one to use: it takes central differences and pads the
ends with one-sided ones.

At least 2 elements are needed along the axis. Both real and complex arrays are
accepted.

## Syntax

```csound
handle:CsnArr = csndiff(source:CsnArr)
handle:CsnArr = csndiff(source:CsnArr, axis:i)
handle:CsnArr = csndiff(source:CsnArr, axis:k)
handle:CsnArr = csndiff(source:CsnArr, axis:k, trig:k)
```

## Arguments

* `source:CsnArr`: the array to difference; at least 2 elements along the axis.
* `axis:i / axis:k` (optional, default `-1`): the axis to difference along; `-1` reads the array flat.
* `trig:k` (optional, default `1`): k-rate trigger. A zero trigger republishes the previous result.

## Output

* `handle:CsnArr`: the consecutive differences, one element shorter along the axis.

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
; csndiff.csd
;
; The discrete derivative, one element shorter than its source. Over onset times
; it gives the durations back.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr   = csnfromarray(array(1, 4, 9, 16))
    steps:CsnArr  = csndiff(data)
    steps_out:i[] = csntoarray(steps)
    n:i           = csnsize(steps)
    prints("n = %d, diff = %g %g %g\n", n, steps_out[0], steps_out[1], steps_out[2])

    ; onsets back to durations, the inverse of csncumsum
    onset:CsnArr  = csnfromarray(array(0, 0.5, 0.75, 1))
    dur:CsnArr    = csndiff(onset)
    dur_out:i[]   = csntoarray(dur)
    prints("durations = %g %g %g\n", dur_out[0], dur_out[1], dur_out[2])

    ; along an axis
    shape:i[]     = fillarray(2, 3)
    mat:CsnArr    = csnreshape(csnfromarray(array(1, 3, 6, 10, 15, 21)), shape)
    rows:CsnArr   = csndiff(mat, 1)
    rows_shape:i[] = csnshape(rows)
    rows_out:i[]  = csntoarray(csnflatten(rows))
    prints("per row: %g x %g = %g %g %g %g\n", rows_shape[0], rows_shape[1], rows_out[0], rows_out[1], rows_out[2], rows_out[3])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csngrad](csngrad.md)
* [csncumsum](csncumsum.md)
* [csnunwrap](csnunwrap.md)

## Credits

Pasquale Mainolfi, 2026
