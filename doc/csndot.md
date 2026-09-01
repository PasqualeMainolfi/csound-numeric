# csndot

## Abstract

NumPy-style dot product: scalar product of vectors, matrix product of matrices.

## Description

`csndot` follows NumPy's `dot`, whose meaning depends on the ranks of its
operands.

* **Two vectors** give their scalar product, a single number — the sum of the
  elementwise products. Declare the output as a number, or as a `:Complex;` when
  the operands are complex.
* **Two matrices** give their matrix product, and the output is a handle. The
  last axis of the first operand must match the second-to-last of the second.

[csnmatmul](csnmatmul.md) is the matrix product said unambiguously, and
[csninner](csninner.md) is the sum over the *last* axis of both operands, which
differs from `csndot` as soon as the rank goes above 1.

Both real and complex arrays are accepted.

## Syntax

```csound
value:i = csndot(a:CsnArr, b:CsnArr)
value:k = csndot(a:CsnArr, b:CsnArr, trig:k)
value:Complex = csndot(a:CsnArr, b:CsnArr)
value:Complex = csndot(a:CsnArr, b:CsnArr, trig:k)
handle:CsnArr = csndot(a:CsnArr, b:CsnArr)
handle:CsnArr = csndot(a:CsnArr, b:CsnArr, trig:k)
```

## Arguments

* `a:CsnArr`: first operand.
* `b:CsnArr`: second operand; its second-to-last axis must match the last axis of `a`.
* `trig:k`: k-rate trigger. A zero trigger republishes the previous result.

## Output

* `value:i / value:k / value:Complex`: the scalar product, for two vectors.
* `handle:CsnArr`: the product array, for higher ranks.

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
; csndot.csd
;
; Two vectors give a number, two matrices give a matrix. The type you declare
; for the output is what picks the overload.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    a:CsnArr      = csnfromarray(array(1, 2, 3))
    b:CsnArr      = csnfromarray(array(4, 5, 6))

    scalar:i      = csndot(a, b)
    prints("scalar product = %g\n", scalar)

    ; matrices: the product is a handle
    shape:i[]     = fillarray(2, 3)
    mat:CsnArr    = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6)), shape)
    matt:CsnArr   = csntranspose(mat)
    prod:CsnArr   = csndot(mat, matt)
    prod_shape:i[] = csnshape(prod)
    prod_out:i[]  = csntoarray(csnflatten(prod))
    prints("matrix product %g x %g = %g %g %g %g\n", prod_shape[0], prod_shape[1], prod_out[0], prod_out[1], prod_out[2], prod_out[3])

    ; the scalar product is also the cosine numerator of csnangledist
    len_a:i       = csnnorm(a, 2)
    len_b:i       = csnnorm(b, 2)
    cos_ab:i      = scalar / (len_a * len_b)
    prints("cosine = %.4f\n", cos_ab)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnmatmul](csnmatmul.md)
* [csninner](csninner.md)
* [csnouter](csnouter.md)
* [csnangledist](csnangledist.md)

## Credits

Pasquale Mainolfi, 2026
