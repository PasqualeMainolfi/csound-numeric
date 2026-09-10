# csninv

## Abstract

Inverse of a square matrix.

## Description

`csninv` returns the matrix that multiplies with its source to give the
identity. It is `np.linalg.inv`, computed the same way: an LU decomposition
with partial pivoting, solved against each column of the identity in turn.

Before reaching for it, consider whether the inverse is the answer or only the
route to one. To apply `A` inverse to a right-hand side, [csnsolve](csnsolve.md)
does it in fewer operations and with less rounding; forming the inverse pays
off when the same one is applied many times, or when the inverse itself is
what you want to look at.

A singular matrix is refused rather than inverted. The pivot that decides this
is compared against a threshold scaled to the matrix, `n * DBL_EPSILON` times
its largest magnitude, so a matrix that is singular only to within rounding is
caught as well as an exactly singular one. An ill-conditioned but invertible
matrix still goes through, with the loss of accuracy its conditioning implies:
a 8x8 Hilbert matrix inverts here, and `A * inv(A)` lands about 1e-8 from the
identity, exactly as its condition number predicts.

Both real and complex matrices are accepted.

## Syntax

```csound
handle:CsnArr = csninv(a:CsnArr)
handle:CsnArr = csninv(a:CsnArr, trig:k)
```

## Arguments

* `a:CsnArr`: the matrix; must be 2-D and square.
* `trig:k` (optional, default `1`): k-rate trigger. A zero trigger republishes the previous result.

## Output

* `handle:CsnArr`: the inverse, shaped like the source.

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
; csninv.csd
;
; The inverse of a matrix, and the check that says whether it is one: multiply
; it back and see how far from the identity the product lands.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    shape:i[] = fillarray(3, 3)
    a:CsnArr  = csnreshape(csnfromarray(array(4, 3, 2, 1, 5, 7, 2, 2, 9)), shape)

    inv:CsnArr = csninv(a)
    csnprint inv

    prod:CsnArr = csnmatmul(a, inv)
    eye:CsnArr  = csnidentity(3)
    diff:CsnArr = csnsubtract(prod, eye)
    worst:i     = csnmax(csnabs(diff))
    prints("A * inv(A) - I: %.2g\n", worst)

    ; solving is the better way to apply an inverse: fewer operations and less
    ; rounding than forming inv(A) and multiplying by it
    bshape:i[] = fillarray(3, 1)
    b:CsnArr   = csnreshape(csnfromarray(array(1, 2, 3)), bshape)
    viasolve:CsnArr = csnsolve(a, b)
    viainv:CsnArr   = csnmatmul(inv, b)
    gap:i = csnmax(csnabs(csnsubtract(viasolve, viainv)))
    prints("solve versus inv-then-multiply: %.2g apart\n", gap)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnsolve](csnsolve.md)
* [csndet](csndet.md)
* [csnmatmul](csnmatmul.md)
* [csnidentity](csnidentity.md)

## Credits

Pasquale Mainolfi, 2026
