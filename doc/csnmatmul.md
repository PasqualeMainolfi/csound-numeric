# csnmatmul

## Abstract

Matrix multiplication.

## Description

`csnmatmul` is the matrix product: element `[i][j]` of the result is the scalar
product of row `i` of the first operand with column `j` of the second. The last
axis of the first must match the second-to-last of the second, so an `n × k`
times a `k × m` gives an `n × m`.

It is [csndot](csndot.md) restricted to the matrix reading, which is what makes
it the one to use when the intent is a linear map: no rank-dependent behaviour to
keep track of.

Multiplying by [csnidentity](csnidentity.md) leaves a matrix unchanged, which is
the quickest way to check a chain.

Both real and complex arrays are accepted.

## Syntax

```csound
handle:CsnArr = csnmatmul(a:CsnArr, b:CsnArr)
handle:CsnArr = csnmatmul(a:CsnArr, b:CsnArr, trig:k)
value:i = csnmatmul(a:CsnArr, b:CsnArr)
value:k = csnmatmul(a:CsnArr, b:CsnArr, trig:k)
```

## Arguments

* `a:CsnArr`: left operand.
* `b:CsnArr`: right operand; its second-to-last axis must match the last axis of `a`.
* `trig:k`: k-rate trigger. A zero trigger republishes the previous result.

## Output

* `handle:CsnArr`: the product matrix.
* `value:i / value:k`: the single number, when both operands are vectors.

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
; csnmatmul.csd
;
; An n x k times a k x m gives an n x m. Multiplying by the identity leaves a
; matrix alone, which is the quickest check of a chain.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    shape:i[]     = fillarray(2, 3)
    mat:CsnArr    = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6)), shape)
    matt:CsnArr   = csntranspose(mat)

    prod:CsnArr   = csnmatmul(mat, matt)
    prod_shape:i[] = csnshape(prod)
    prod_out:i[]  = csntoarray(csnflatten(prod))
    prints("2x3 times 3x2 = %g x %g : %g %g %g %g\n", prod_shape[0], prod_shape[1], prod_out[0], prod_out[1], prod_out[2], prod_out[3])

    ; the identity is the neutral element
    eye:CsnArr    = csnidentity(3)
    same:CsnArr   = csnmatmul(mat, eye)
    same_out:i[]  = csntoarray(csnflatten(same))
    prints("mat * I = %g %g %g %g %g %g\n", same_out[0], same_out[1], same_out[2], same_out[3], same_out[4], same_out[5])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csndot](csndot.md)
* [csntranspose](csntranspose.md)
* [csnidentity](csnidentity.md)
* [csntrace](csntrace.md)

## Credits

Pasquale Mainolfi, 2026
