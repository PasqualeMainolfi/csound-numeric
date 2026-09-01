# csnnormalize

## Abstract

Divide an array by its own norm of a given order.

## Description

`csnnormalize` scales an array so that its norm becomes 1. The order is the same
one [csnnorm](csnnorm.md) takes and defaults to `1`, so by default the elements
end up summing (in magnitude) to 1 — a probability-like distribution. Order `2`
gives a unit-length vector instead, which is what a direction wants.

With no axis the whole array is scaled by one factor. Given an axis each line
along it is normalised on its own, so every row of a matrix comes out with the
same norm.

Both real and complex arrays are accepted.

Two forms share the name. The one with an output publishes a new handle and
leaves the source alone; the one without an output rewrites the source in place
and returns nothing.

## Syntax

```csound
handle:CsnArr = csnnormalize(source:CsnArr)
handle:CsnArr = csnnormalize(source:CsnArr, axis:i)
handle:CsnArr = csnnormalize(source:CsnArr, axis:i, order:i)
handle:CsnArr = csnnormalize(source:CsnArr, axis:k, order:k)
handle:CsnArr = csnnormalize(source:CsnArr, axis:k, order:k, trig:k)
csnnormalize(source:CsnArr)
csnnormalize(source:CsnArr, axis:i)
csnnormalize(source:CsnArr, axis:i, order:i)
csnnormalize(source:CsnArr, axis:k, order:k)
csnnormalize(source:CsnArr, axis:k, order:k, trig:k)
```

## Arguments

* `source:CsnArr`: the array to normalise.
* `axis:i / axis:k` (optional, default `-1`): the axis to normalise along; `-1` scales the whole array by one factor.
* `order:i / order:k` (optional, default `1`): the norm order; must be >= 1.
* `trig:k` (optional, default `1`): k-rate trigger. A zero trigger republishes the previous result.

## Output

* `handle:CsnArr`: the normalised array. Omit it for the in-place form.

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
; csnnormalize.csd
;
; Order 1 makes the magnitudes sum to 1, which is what a weight vector wants.
; Order 2 makes the length 1, which is what a direction wants.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    vec:CsnArr    = csnfromarray(array(1, 2, 3))

    weights:CsnArr = csnnormalize(vec)
    weights_out:i[] = csntoarray(weights)
    total:i       = csnsum(weights)
    prints("order 1 = %.4f %.4f %.4f, sum = %g\n", weights_out[0], weights_out[1], weights_out[2], total)

    unit:CsnArr   = csnnormalize(vec, -1, 2)
    unit_out:i[]  = csntoarray(unit)
    length:i      = csnnorm(unit, 2)
    prints("order 2 = %.4f %.4f %.4f, length = %g\n", unit_out[0], unit_out[1], unit_out[2], length)

    ; per row, so every row comes out the same length
    shape:i[]     = fillarray(2, 3)
    mat:CsnArr    = csnreshape(csnfromarray(array(1, 2, 3, 40, 50, 60)), shape)
    rows:CsnArr   = csnnormalize(mat, 1, 2)
    row_norms:CsnArr = csnnorm(rows, 1, 2)
    row_out:i[]   = csntoarray(row_norms)
    prints("row lengths after = %g %g\n", row_out[0], row_out[1])

    ; in place
    csnnormalize(vec)
    now:i[]       = csntoarray(vec)
    prints("in place = %.4f %.4f %.4f\n", now[0], now[1], now[2])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnnorm](csnnorm.md)
* [csnproject](csnproject.md)
* [csnangledist](csnangledist.md)

## Credits

Pasquale Mainolfi, 2026
