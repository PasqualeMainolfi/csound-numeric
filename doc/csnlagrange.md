# csnlagrange

## Abstract

The coefficients of the Lagrange interpolating polynomial through a set of points.

## Description

`csnlagrange` builds the unique polynomial of degree at most `N - 1` that passes
through the `N` points `(x[i], y[i])`, and returns its `N` coefficients,
**highest degree first**. That is the order `scipy.interpolate.lagrange` hands
to `np.poly1d`:

```
p(t) = c[0] t^(N-1) + c[1] t^(N-2) + ... + c[N-1]
```

It is computed through the Newton divided differences, expanded into the
power basis by Horner's scheme, in `O(N^2)`.

The `x` values must be distinct: two points with the same `x` have no
polynomial through them, and are refused rather than answered with infinities.
They need not be sorted or evenly spaced.

**Keep `N` small.** A high-degree interpolant oscillates between its points
(Runge's phenomenon), and the power-basis coefficients lose precision fast as
`N` grows, as scipy's do. For more than a handful of points, piecewise
interpolation with [csninterp](csninterp.md) is the better tool.

There is no performance-time form.

## Syntax

```csound
coefs:CsnArr csnlagrange x:CsnArr, y:CsnArr
```

## Arguments

* `x:CsnArr`: a 1-D real array of `N >= 1` distinct abscissae.
* `y:CsnArr`: a 1-D real array of `N` ordinates.

## Output

* `coefs:CsnArr`: a real vector of `N` coefficients, highest degree first.

## Execution Time

* Init

## Examples

```csound
<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnlagrange.csd
;
; The Lagrange interpolating polynomial through N points: the unique
; polynomial of degree at most N-1 that passes through all of them. The result
; is its N coefficients, highest degree first, as scipy.interpolate.lagrange
; hands them to np.poly1d.
;
; The x values must be distinct. With many points the polynomial oscillates
; between them (Runge's phenomenon) and the coefficients lose precision: keep N
; small.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    ; four points on 2x^3 - x + 4
    ix[] = fillarray(-1, 0.5, 2, 3)
    iy[] = fillarray(3, 3.75, 18, 55)
    hx:CsnArr = csnfromarray(ix)
    hy:CsnArr = csnfromarray(iy)
    coefs:CsnArr = csnlagrange(hx, hy)
    ic[] = csntoarray(coefs)
    prints("coefficients, highest degree first: %.4f %.4f %.4f %.4f\n", ic[0], ic[1], ic[2], ic[3])

    ; evaluate it between the points, by Horner
    it = 1
    iv = ((ic[0] * it + ic[1]) * it + ic[2]) * it + ic[3]
    prints("p(1) = %.4f\n", iv)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csninterp](csninterp.md)
* [csnresample](csnresample.md)

## Credits

Pasquale Mainolfi, 2026
