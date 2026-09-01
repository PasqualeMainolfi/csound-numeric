# csnlogicand

## Abstract

Logical and of two arrays, or of an array and a scalar.

## Description

`csnlogicand` returns `1` where **both** operands are non-zero and `0` otherwise.
It reads its inputs as truth values, not as numbers, so any non-zero element
counts as true whatever its magnitude.

Its natural inputs are the masks the comparison opcodes produce: build one test
with [csngt](csngt.md), another with [csnlt](csnlt.md), and `csnlogicand` narrows
them to the elements that pass both.

Two arrays are broadcast against each other NumPy-style. Both scalar orders exist,
though the operation is commutative, so `csnlogicand(mask, 1)` and
`csnlogicand(1, mask)` agree.

Real only.

## Syntax

```csound
handle:CsnArr = csnlogicand(a:CsnArr, b:CsnArr)
handle:CsnArr = csnlogicand(a:CsnArr, b:CsnArr, trig:k)
handle:CsnArr = csnlogicand(a:CsnArr, value:i)
handle:CsnArr = csnlogicand(a:CsnArr, value:k)
handle:CsnArr = csnlogicand(a:CsnArr, value:k, trig:k)
handle:CsnArr = csnlogicand(value:i, b:CsnArr)
handle:CsnArr = csnlogicand(value:k, b:CsnArr)
handle:CsnArr = csnlogicand(value:k, b:CsnArr, trig:k)
```

## Arguments

* `a:CsnArr`: first operand, read as truth values.
* `b:CsnArr`: second operand; broadcast against `a`.
* `value:i / value:k`: a scalar, on either side of the operation.
* `trig:k` (optional, default `1`): k-rate trigger. A zero trigger republishes the previous result.

## Output

* `handle:CsnArr`: a 0/1 array.

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
; csnlogicand.csd
;
; Two comparisons, one band: csnlogicand keeps only the elements that pass both
; tests, which is how a range filter is written.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr    = csnfromarray(array(-2, 0.5, 1, 3, 5))

    above:CsnArr   = csngt(data, 0)
    below:CsnArr   = csnlt(data, 4)
    band:CsnArr    = csnlogicand(above, below)
    band_out:i[]   = csntoarray(band)
    prints("in (0, 4) = %g %g %g %g %g\n", band_out[0], band_out[1], band_out[2], band_out[3], band_out[4])

    inside:i       = csnsum(band)
    prints("count     = %g\n", inside)

    ; keep only what passed
    kept:CsnArr    = csnmul(data, band)
    kept_out:i[]   = csntoarray(kept)
    prints("kept      = %g %g %g %g %g\n", kept_out[0], kept_out[1], kept_out[2], kept_out[3], kept_out[4])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnlogicor](csnlogicor.md)
* [csnlogicnot](csnlogicnot.md)
* [csngt](csngt.md)
* [csnall](csnall.md)

## Credits

Pasquale Mainolfi, 2026
