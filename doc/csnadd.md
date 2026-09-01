# csnadd

## Abstract

Elementwise addition of two arrays, or of an array and a scalar.

## Description

`csnadd` adds its two operands elementwise. Two arrays are broadcast against each
other NumPy-style — aligned from the last axis, each pair of extents must either
match or be 1 — so a `2×3` matrix and a `1×3` row add without a reshape. An array
and a scalar add the scalar to every element.

Addition is commutative, so there is no separate "scalar first" overload: write
the array on the left.

Real and complex arrays are both accepted, and an operation mixing the two
promotes the result to complex. A complex scalar is passed as a `:Complex;`.

## Syntax

```csound
handle:CsnArr = csnadd(a:CsnArr, b:CsnArr)
handle:CsnArr = csnadd(a:CsnArr, b:CsnArr, trig:k)
handle:CsnArr = csnadd(a:CsnArr, value:i)
handle:CsnArr = csnadd(a:CsnArr, value:k)
handle:CsnArr = csnadd(a:CsnArr, value:k, trig:k)
handle:CsnArr = csnadd(a:CsnArr, value:Complex)
handle:CsnArr = csnadd(a:CsnArr, value:Complex, trig:k)
```

## Arguments

* `a:CsnArr`: first operand.
* `b:CsnArr`: second operand; broadcast against `a`.
* `value:i / value:k / value:Complex`: a scalar added to every element.
* `trig:k` (optional, default `1`): k-rate trigger. A zero trigger republishes the previous result.

## Output

* `handle:CsnArr`: handle of the sum.

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
; csnadd.csd
;
; Two arrays broadcast NumPy-style, so a 2 x 3 and a 1 x 3 add without a
; reshape. A scalar operand reaches every element.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    a:CsnArr       = csnfromarray(array(1, 2, 3))
    b:CsnArr       = csnfromarray(array(10, 20, 30))

    sum:CsnArr     = csnadd(a, b)
    sum_out:i[]    = csntoarray(sum)
    prints("array + array  = %g %g %g\n", sum_out[0], sum_out[1], sum_out[2])

    biased:CsnArr  = csnadd(a, 0.5)
    biased_out:i[] = csntoarray(biased)
    prints("array + scalar = %g %g %g\n", biased_out[0], biased_out[1], biased_out[2])

    ; broadcasting a row across a matrix
    mat_shape:i[]  = fillarray(2, 3)
    row_shape:i[]  = fillarray(1, 3)
    mat:CsnArr     = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6)), mat_shape)
    row:CsnArr     = csnreshape(b, row_shape)
    wide:CsnArr    = csnadd(mat, row)
    wide_out:i[]   = csntoarray(csnflatten(wide))
    prints("broadcast      = %g %g %g %g %g %g\n", wide_out[0], wide_out[1], wide_out[2], wide_out[3], wide_out[4], wide_out[5])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnsubtract](csnsubtract.md)
* [csnmul](csnmul.md)
* [csndiv](csndiv.md)
* [csnsum](csnsum.md)

## Credits

Pasquale Mainolfi, 2026
