# csnsubtract

## Abstract

Elementwise subtraction of two arrays, or of an array and a scalar.

## Description

`csnsubtract` subtracts its second operand from its first, elementwise. Two
arrays are broadcast against each other NumPy-style — aligned from the last axis,
each pair of extents must either match or be 1.

Subtraction is not commutative, so both scalar orders exist:
`csnsubtract(array, value)` takes the scalar off every element, and
`csnsubtract(value, array)` subtracts every element from the scalar. The second
is how to negate an array: `csnsubtract(0, data)`.

Real and complex arrays are both accepted, and an operation mixing the two
promotes the result to complex. A complex scalar is passed as a `:Complex;`.

## Syntax

```csound
handle:CsnArr = csnsubtract(a:CsnArr, b:CsnArr)
handle:CsnArr = csnsubtract(a:CsnArr, b:CsnArr, trig:k)
handle:CsnArr = csnsubtract(a:CsnArr, value:i)
handle:CsnArr = csnsubtract(a:CsnArr, value:k)
handle:CsnArr = csnsubtract(a:CsnArr, value:k, trig:k)
handle:CsnArr = csnsubtract(a:CsnArr, value:Complex)
handle:CsnArr = csnsubtract(a:CsnArr, value:Complex, trig:k)
handle:CsnArr = csnsubtract(value:i, b:CsnArr)
handle:CsnArr = csnsubtract(value:k, b:CsnArr)
handle:CsnArr = csnsubtract(value:k, b:CsnArr, trig:k)
handle:CsnArr = csnsubtract(value:Complex, b:CsnArr)
handle:CsnArr = csnsubtract(value:Complex, b:CsnArr, trig:k)
```

## Arguments

* `a:CsnArr`: the array subtracted from.
* `b:CsnArr`: the array subtracted; broadcast against `a`.
* `value:i / value:k / value:Complex`: a scalar, on either side of the operation.
* `trig:k` (optional, default `1`): k-rate trigger. A zero trigger republishes the previous result.

## Output

* `handle:CsnArr`: handle of the difference.

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
; csnsubtract.csd
;
; Both scalar orders exist because subtraction is not commutative. The scalar
; on the left is also how an array is negated.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    a:CsnArr        = csnfromarray(array(10, 20, 30))
    b:CsnArr        = csnfromarray(array(1, 2, 3))

    diff:CsnArr     = csnsubtract(a, b)
    diff_out:i[]    = csntoarray(diff)
    prints("array - array  = %g %g %g\n", diff_out[0], diff_out[1], diff_out[2])

    less:CsnArr     = csnsubtract(a, 5)
    less_out:i[]    = csntoarray(less)
    prints("array - scalar = %g %g %g\n", less_out[0], less_out[1], less_out[2])

    from:CsnArr     = csnsubtract(100, a)
    from_out:i[]    = csntoarray(from)
    prints("scalar - array = %g %g %g\n", from_out[0], from_out[1], from_out[2])

    ; negation
    neg:CsnArr      = csnsubtract(0, a)
    neg_out:i[]     = csntoarray(neg)
    prints("negated        = %g %g %g\n", neg_out[0], neg_out[1], neg_out[2])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnadd](csnadd.md)
* [csnmul](csnmul.md)
* [csndiv](csndiv.md)
* [csnsub](csnsub.md)

## Credits

Pasquale Mainolfi, 2026
