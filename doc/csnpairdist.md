# csnpairdist

## Abstract

Elementwise distance between two arrays of the same shape.

## Description

`csnpairdist` returns one distance per pair of elements: the magnitude of
`a[i] - b[i]`, in an array with the shape of its operands. Where
[csndist](csndist.md) collapses everything to a single number, this one keeps the
detail.

Both arrays must have exactly the same shape — there is no broadcasting here,
since the pairing is the point.

The result is always **real**, even for complex operands, because a magnitude is:
that is the one case in this family where the element type changes.

## Syntax

```csound
handle:CsnArr = csnpairdist(a:CsnArr, b:CsnArr)
handle:CsnArr = csnpairdist(a:CsnArr, b:CsnArr, trig:k)
```

## Arguments

* `a:CsnArr`: first array.
* `b:CsnArr`: second array; must have exactly the shape of `a`.
* `trig:k`: k-rate trigger. A zero trigger republishes the previous result.

## Output

* `handle:CsnArr`: a real array of per-element distances, with the shape of the operands.

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
```

## See also

* [csndist](csndist.md)
* [csnsubtract](csnsubtract.md)
* [csnabs](csnabs.md)

## Credits

Pasquale Mainolfi, 2026
