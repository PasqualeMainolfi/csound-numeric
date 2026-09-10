# csnsolve

## Abstract

Solves a linear system A x = B for x.

## Description

`csnsolve` answers the question `csninv` is usually asked for by mistake: given
a square matrix `A` and a right-hand side `B`, it returns the `x` that satisfies
`A x = B`. It is `np.linalg.solve`.

The matrix is factored once, by LU decomposition with partial pivoting, and the
factors are then applied to every column of `B`, so several right-hand sides in
one call cost far less than several calls. `B` may be a vector or a matrix; it
must have as many rows as `A`, and the result comes back shaped like `B`.

Solving is also the right way to *apply* an inverse. Forming `inv(A)` and
multiplying by it does more arithmetic and accumulates more rounding than
solving directly, which is why `np.linalg.solve` exists alongside
`np.linalg.inv` and why the example below compares the two.

A singular matrix is refused rather than answered: the pivot that decides this
is compared against a threshold scaled to the matrix, `n * DBL_EPSILON` times
its largest magnitude, so a matrix that is singular only to within rounding is
caught as well as an exactly singular one. Where the question is whether a
matrix is invertible at all, [csndet](csndet.md) answers with zero instead of
refusing.

Both real and complex matrices are accepted, and a real operand is promoted
when the other is complex.

## Syntax

```csound
handle:CsnArr = csnsolve(a:CsnArr, b:CsnArr)
handle:CsnArr = csnsolve(a:CsnArr, b:CsnArr, trig:k)
```

## Arguments

* `a:CsnArr`: the matrix; must be 2-D and square.
* `b:CsnArr`: the right-hand side; as many rows as `a`, any number of columns.
* `trig:k` (optional, default `1`): k-rate trigger. A zero trigger republishes the previous result.

## Output

* `handle:CsnArr`: the solution, shaped like `b`.

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
; csnsolve.csd
;
; Solves A x = B for x. The right-hand side may carry several columns at once,
; and the answer is checked the way it should be: by multiplying it back.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    shape:i[]  = fillarray(3, 3)
    bshape:i[] = fillarray(3, 1)
    a:CsnArr   = csnreshape(csnfromarray(array(4, 3, 2, 1, 5, 7, 2, 2, 9)), shape)
    b:CsnArr   = csnreshape(csnfromarray(array(1, 2, 3)), bshape)

    x:CsnArr   = csnsolve(a, b)
    csnprint x

    ; A x should give B back
    back:CsnArr = csnmatmul(a, x)
    diff:CsnArr = csnsubtract(back, b)
    worst:i     = csnmax(csnabs(diff))
    prints("largest residual: %.2g\n", worst)

    ; two right-hand sides in one call, one column each
    b2shape:i[] = fillarray(3, 2)
    b2:CsnArr   = csnreshape(csnfromarray(array(1, 0,  2, 1,  3, 0)), b2shape)
    x2:CsnArr   = csnsolve(a, b2)
    x2_shape:i[] = csnshape(x2)
    prints("two columns in, %d x %d out\n", x2_shape[0], x2_shape[1])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csninv](csninv.md)
* [csndet](csndet.md)
* [csnmatmul](csnmatmul.md)
* [csndot](csndot.md)

## Credits

Pasquale Mainolfi, 2026
