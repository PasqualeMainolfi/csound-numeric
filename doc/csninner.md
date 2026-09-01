# csninner

## Abstract

Inner product: sum over the last axis of both operands.

## Description

`csninner` contracts the **last** axis of each operand, which must match in
length, and pairs every remaining line of the first with every remaining line of
the second.

For two vectors that is the ordinary scalar product, the same answer
[csndot](csndot.md) gives. Above rank 1 the two part company: `csndot` contracts
the last axis of the first operand against the *second-to-last* of the second,
while `csninner` uses the last of both.

Declare the output as a number, or a `:Complex;`, for the vector case, and as a
handle for the higher-rank case.

Both real and complex arrays are accepted.

## Syntax

```csound
value:i = csninner(a:CsnArr, b:CsnArr)
value:k = csninner(a:CsnArr, b:CsnArr, trig:k)
value:Complex = csninner(a:CsnArr, b:CsnArr)
value:Complex = csninner(a:CsnArr, b:CsnArr, trig:k)
handle:CsnArr = csninner(a:CsnArr, b:CsnArr)
handle:CsnArr = csninner(a:CsnArr, b:CsnArr, trig:k)
```

## Arguments

* `a:CsnArr`: first operand.
* `b:CsnArr`: second operand; its last axis must match the last axis of `a`.
* `trig:k`: k-rate trigger. A zero trigger republishes the previous result.

## Output

* `value:i / value:k / value:Complex`: the scalar product, for two vectors.
* `handle:CsnArr`: the contracted array, for higher ranks.

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
; csninner.csd
;
; The last axis of both operands is contracted. On vectors that is the scalar
; product; above rank 1 it differs from csndot.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    a:CsnArr      = csnfromarray(array(1, 2, 3))
    b:CsnArr      = csnfromarray(array(4, 5, 6))

    scalar:i      = csninner(a, b)
    same:i        = csndot(a, b)
    prints("inner = %g, dot = %g\n", scalar, same)

    ; two matrices: the last axis of each is contracted
    shape:i[]     = fillarray(2, 3)
    mat:CsnArr    = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6)), shape)
    prod:CsnArr   = csninner(mat, mat)
    prod_shape:i[] = csnshape(prod)
    prod_out:i[]  = csntoarray(csnflatten(prod))
    prints("inner of a 2 x 3 with itself: %g x %g = %g %g %g %g\n", prod_shape[0], prod_shape[1], prod_out[0], prod_out[1], prod_out[2], prod_out[3])
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
* [csnouter](csnouter.md)
* [csnmatmul](csnmatmul.md)

## Credits

Pasquale Mainolfi, 2026
