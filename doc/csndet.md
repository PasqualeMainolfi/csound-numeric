# csndet

## Abstract

Determinant of a square matrix.

## Description

`csndet` returns the determinant, as a real number for a real matrix and as a
`:Complex;` for a complex one. It is `np.linalg.det`, computed from the LU
decomposition: the product of the diagonal, signed by the number of row swaps
the pivoting needed.

Unlike [csninv](csninv.md) and [csnsolve](csnsolve.md), a singular matrix is
**not** an error here - the determinant of a singular matrix is zero, and
asking for it is the classic way to find out whether a matrix can be inverted
at all. NumPy draws the line in the same place: `det` answers, `inv` and
`solve` raise.

Read the answer with the size of the matrix in mind. The determinant scales
with the product of the entries, so it overflows or underflows long before the
matrix becomes hard to invert: an 8x8 Hilbert matrix has a determinant around
2.7e-33 and inverts perfectly well. As a test of invertibility it is a blunt
instrument; where the question is how far from singular a matrix is, the
condition number is the honest measure.

## Syntax

```csound
value:i = csndet(a:CsnArr)
value:k = csndet(a:CsnArr, trig:k)
Value:Complex = csndet(a:CsnArr)
Value:Complex = csndet(a:CsnArr, trig:k)
```

## Arguments

* `a:CsnArr`: the matrix; must be 2-D and square.
* `trig:k` (optional, default `1`): k-rate trigger. A zero trigger republishes the previous result.

## Output

* `value:i / value:k`: the determinant of a real matrix.
* `Value:Complex`: the determinant of a complex matrix.

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
; csndet.csd
;
; The determinant, real and complex. Zero is the answer for a singular matrix,
; not an error, which is what makes this the way to ask whether a matrix can be
; inverted at all.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    shape:i[] = fillarray(3, 3)
    a:CsnArr  = csnreshape(csnfromarray(array(4, 3, 2, 1, 5, 7, 2, 2, 9)), shape)
    d:i       = csndet(a)
    prints("det = %g\n", d)

    ; row 3 is row 1 plus row 2: singular, so the determinant is zero
    dep:CsnArr = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6, 5, 7, 9)), shape)
    d_dep:i    = csndet(dep)
    prints("det of a dependent matrix = %g\n", d_dep)

    ; a complex matrix answers with a complex determinant
    j:Complex  = init(0, 1, 0)
    re:CsnArr  = csnreshape(csnfromarray(array(4, 3, 2, 1, 5, 7, 2, 2, 9)), shape)
    im:CsnArr  = csnreshape(csnfromarray(array(1, 0, 2, 0, 3, 1, 1, 1, 0)), shape)
    z:CsnArr   = csnadd(csntocomplex(re), csnmul(csntocomplex(im), j))
    D:Complex  = csndet(z)
    d_re:i     = real(D)
    d_im:i     = imag(D)
    prints("complex det = %g + %gi\n", d_re, d_im)
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
* [csnsolve](csnsolve.md)
* [csntrace](csntrace.md)
* [csndiag](csndiag.md)

## Credits

Pasquale Mainolfi, 2026
