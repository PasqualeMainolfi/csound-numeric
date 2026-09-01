# csnnorm

## Abstract

Vector or matrix norm of a given order.

## Description

`csnnorm` returns the p-norm of an array: the `p`-th root of the sum of the
`p`-th powers of the magnitudes. Order `1` is the sum of magnitudes, order `2`
the Euclidean length, and larger orders lean further towards the largest element.

The order must be at least 1, and defaults to `1`.

Over the whole array the result is a single number. Given an axis the norm is
taken along that axis and the result is an array, with the reduced axis dropped —
one length per row or per column. In the array form the axis is required and
comes before the order.

Both real and complex arrays are accepted; magnitudes are used, so the answer is
a real number either way.

## Syntax

```csound
value:i = csnnorm(source:CsnArr)
value:i = csnnorm(source:CsnArr, order:i)
value:k = csnnorm(source:CsnArr, order:k)
value:k = csnnorm(source:CsnArr, order:k, trig:k)
handle:CsnArr = csnnorm(source:CsnArr, axis:i)
handle:CsnArr = csnnorm(source:CsnArr, axis:i, order:i)
handle:CsnArr = csnnorm(source:CsnArr, axis:k, order:k)
handle:CsnArr = csnnorm(source:CsnArr, axis:k, order:k, trig:k)
```

## Arguments

* `source:CsnArr`: the array to measure.
* `order:i / order:k` (optional, default `1`): the norm order; must be >= 1.
* `axis:i / axis:k`: the axis to measure along. Required for the array form, and it comes before `order`.
* `trig:k` (optional, default `1`): k-rate trigger. A zero trigger republishes the previous result.

## Output

* `value:i / value:k`: the norm over the whole array.
* `handle:CsnArr`: one norm per line along `axis`, with that axis dropped.

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
; csnnorm.csd
;
; Order 1 is the sum of magnitudes, order 2 the Euclidean length. In the array
; form the axis comes first and the order after it.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    vec:CsnArr   = csnfromarray(array(1, 2, 3))

    l1:i         = csnnorm(vec)
    l2:i         = csnnorm(vec, 2)
    l4:i         = csnnorm(vec, 4)
    prints("order 1 = %g, order 2 = %.4f, order 4 = %.4f\n", l1, l2, l4)

    ; order 1 is the sum of magnitudes the long way
    by_sum:i     = csnsum(csnabs(vec))
    prints("sum of magnitudes = %g\n", by_sum)

    ; per row of a matrix: axis first, then order
    shape:i[]    = fillarray(2, 3)
    mat:CsnArr   = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6)), shape)
    rows:CsnArr  = csnnorm(mat, 1, 2)
    rows_out:i[] = csntoarray(rows)
    prints("row lengths = %.4f %.4f\n", rows_out[0], rows_out[1])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnnormalize](csnnormalize.md)
* [csndist](csndist.md)
* [csnabs](csnabs.md)
* [csnhypot](csnhypot.md)

## Credits

Pasquale Mainolfi, 2026
