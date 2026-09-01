# csnmul

## Abstract

Elementwise multiplication of two arrays, or of an array and a scalar.

## Description

`csnmul` multiplies its two operands elementwise — the Hadamard product, not the
matrix product; [csnmatmul](csnmatmul.md) is that. Two arrays are broadcast
against each other NumPy-style: aligned from the last axis, each pair of extents
must either match or be 1.

An array times a scalar scales every element, which is the usual way to apply a
gain to a buffer.

Multiplication is commutative, so there is no separate "scalar first" overload:
write the array on the left.

Real and complex arrays are both accepted, and an operation mixing the two
promotes the result to complex. A complex scalar is passed as a `:Complex;`, and
multiplying a complex array by one is a rotation and a scaling at once.

## Syntax

```csound
handle:CsnArr = csnmul(a:CsnArr, b:CsnArr)
handle:CsnArr = csnmul(a:CsnArr, b:CsnArr, trig:k)
handle:CsnArr = csnmul(a:CsnArr, value:i)
handle:CsnArr = csnmul(a:CsnArr, value:k)
handle:CsnArr = csnmul(a:CsnArr, value:k, trig:k)
handle:CsnArr = csnmul(a:CsnArr, value:Complex)
handle:CsnArr = csnmul(a:CsnArr, value:Complex, trig:k)
```

## Arguments

* `a:CsnArr`: first operand.
* `b:CsnArr`: second operand; broadcast against `a`.
* `value:i / value:k / value:Complex`: a scalar multiplying every element.
* `trig:k` (optional, default `1`): k-rate trigger. A zero trigger republishes the previous result.

## Output

* `handle:CsnArr`: handle of the product.

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
; csnmul.csd
;
; Elementwise, not matrix: csnmul scales, masks and windows. csnmatmul is the
; linear-algebra product.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    a:CsnArr       = csnfromarray(array(1, 2, 3, 4))
    b:CsnArr       = csnfromarray(array(10, 100, 1000, 10000))

    prod:CsnArr    = csnmul(a, b)
    prod_out:i[]   = csntoarray(prod)
    prints("elementwise = %g %g %g %g\n", prod_out[0], prod_out[1], prod_out[2], prod_out[3])

    gain:CsnArr    = csnmul(a, 0.5)
    gain_out:i[]   = csntoarray(gain)
    prints("scaled      = %g %g %g %g\n", gain_out[0], gain_out[1], gain_out[2], gain_out[3])

    ; windowing a buffer is one multiplication
    buf:CsnArr     = csnones(fillarray(8))
    win:CsnArr     = csnhanning(8)
    shaped:CsnArr  = csnmul(buf, win)
    shaped_out:i[] = csntoarray(shaped)
    prints("windowed    = %.3f %.3f %.3f %.3f\n", shaped_out[0], shaped_out[1], shaped_out[2], shaped_out[3])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csndiv](csndiv.md)
* [csnmatmul](csnmatmul.md)
* [csnadd](csnadd.md)
* [csnprod](csnprod.md)

## Credits

Pasquale Mainolfi, 2026
