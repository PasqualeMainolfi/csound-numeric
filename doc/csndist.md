# csndist

## Abstract

Minkowski distance between two arrays, of a given order.

## Description

`csndist` returns a single number: the `p`-th root of the sum of the `p`-th
powers of the elementwise differences. Order `1` is the Manhattan distance, order
`2` the Euclidean one, and larger orders lean further towards the largest single
difference.

The order must be at least 1, and defaults to `1`.

It is [csnnorm](csnnorm.md) of the difference of the two arrays, said in one
call. Where the *per-element* distances are wanted rather than their total,
[csnpairdist](csnpairdist.md) is the one to use.

Both real and complex arrays are accepted; magnitudes are used, so the answer is
a real number either way.

## Syntax

```csound
value:i = csndist(a:CsnArr, b:CsnArr)
value:i = csndist(a:CsnArr, b:CsnArr, order:i)
value:k = csndist(a:CsnArr, b:CsnArr, order:k, trig:k)
```

## Arguments

* `a:CsnArr`: first array.
* `b:CsnArr`: second array.
* `order:i / order:k` (optional, default `1`): the Minkowski order; must be >= 1.
* `trig:k`: k-rate trigger. A zero trigger republishes the previous result.

## Output

* `value:i / value:k`: the distance.

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
; csndist.csd
;
; Order 1 is Manhattan, order 2 Euclidean. It is csnnorm of the difference, in
; one call.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    a:CsnArr      = csnfromarray(array(1, 2, 3))
    b:CsnArr      = csnfromarray(array(4, 5, 6))

    manhattan:i   = csndist(a, b)
    euclid:i      = csndist(a, b, 2)
    prints("order 1 = %g, order 2 = %.4f\n", manhattan, euclid)

    ; the same answer as a norm of the difference
    diff:CsnArr   = csnsubtract(a, b)
    by_norm:i     = csnnorm(diff, 2)
    prints("csnnorm of the difference = %.4f\n", by_norm)

    ; and the per-element distances, when the total is not what is wanted
    each:CsnArr   = csnpairdist(a, b)
    each_out:i[]  = csntoarray(each)
    prints("per element = %g %g %g\n", each_out[0], each_out[1], each_out[2])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnpairdist](csnpairdist.md)
* [csnnorm](csnnorm.md)
* [csnangledist](csnangledist.md)

## Credits

Pasquale Mainolfi, 2026
