# csnlogicor

## Abstract

Logical or of two arrays, or of an array and a scalar.

## Description

`csnlogicor` returns `1` where **either** operand is non-zero and `0` where both
are zero. Like [csnlogicand](csnlogicand.md) it reads its inputs as truth values,
so any non-zero element counts as true whatever its magnitude.

It widens two masks into their union: everything that passes the first test or the
second. Combined with [csnlogicnot](csnlogicnot.md) it also expresses "outside a
band", the complement of the `csnlogicand` case.

Two arrays are broadcast against each other NumPy-style. Both scalar orders exist,
though the operation is commutative.

Real only.

## Syntax

```csound
handle:CsnArr = csnlogicor(a:CsnArr, b:CsnArr)
handle:CsnArr = csnlogicor(a:CsnArr, b:CsnArr, trig:k)
handle:CsnArr = csnlogicor(a:CsnArr, value:i)
handle:CsnArr = csnlogicor(a:CsnArr, value:k)
handle:CsnArr = csnlogicor(a:CsnArr, value:k, trig:k)
handle:CsnArr = csnlogicor(value:i, b:CsnArr)
handle:CsnArr = csnlogicor(value:k, b:CsnArr)
handle:CsnArr = csnlogicor(value:k, b:CsnArr, trig:k)
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
; csnlogicor.csd
;
; The union of two masks. With csnlogicnot in front of an and, it is also how
; "outside the band" is written.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr      = csnfromarray(array(-2, 0.5, 1, 3, 5))

    very_low:CsnArr  = csnlt(data, 0)
    very_high:CsnArr = csngt(data, 4)
    extremes:CsnArr  = csnlogicor(very_low, very_high)
    extremes_out:i[] = csntoarray(extremes)
    prints("outside [0, 4] = %g %g %g %g %g\n", extremes_out[0], extremes_out[1], extremes_out[2], extremes_out[3], extremes_out[4])

    ; the same set, said as the complement of an and
    above:CsnArr     = csnge(data, 0)
    below:CsnArr     = csnle(data, 4)
    inside:CsnArr    = csnlogicand(above, below)
    outside:CsnArr   = csnlogicnot(inside)
    outside_out:i[]  = csntoarray(outside)
    prints("complement     = %g %g %g %g %g\n", outside_out[0], outside_out[1], outside_out[2], outside_out[3], outside_out[4])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnlogicand](csnlogicand.md)
* [csnlogicnot](csnlogicnot.md)
* [csnany](csnany.md)

## Credits

Pasquale Mainolfi, 2026
