# csnroots

## Abstract

The roots of a polynomial, or of one polynomial per slice along an axis.

## Description

`csnroots` takes the `N` coefficients of a polynomial of degree `N - 1`,
**highest degree first**, the order `np.roots` and `np.poly1d` use:

```
p(z) = c[0] z^(N-1) + c[1] z^(N-2) + ... + c[N-1]
```

and returns its `N - 1` roots as a **complex** vector, even when every root is
real. The roots come in no particular order: sort the real or imaginary parts
if the order matters.

They are found by the Aberth-Ehrlich iteration, all at once, starting from
points evenly spread on a circle whose radius bounds every root. The
coefficients may be real or complex.

With an axis, every 1-D slice along it is a separate polynomial, and the output
has the same shape as the source with that axis one shorter. Without one, the
last axis is used.

The leading coefficient `c[0]` must be non-zero: a leading zero means a lower
degree than the length says, and is refused rather than stripped as `np.roots`
does. Trailing zeros are fine and give roots at `0`. A constant, a slice of
length 1, has no roots and is refused. A polynomial the iteration cannot
settle within 1000 steps is an init error.

Roots of high multiplicity are ill-conditioned, as they are for every method:
expect about `1/m` of the double precision for a root repeated `m` times.

There is no performance-time form.

## Syntax

```csound
roots:CsnArr = csnroots(coefs:CsnArr)
roots:CsnArr = csnroots(coefs:CsnArr, axis:i)
```

## Arguments

* `coefs:CsnArr`: a real or complex array of coefficients, highest degree first, at least 2 along the axis.
* `axis:i` (optional): the axis the coefficients run along. Omit it for the last axis; negative values count from the end.

## Output

* `roots:CsnArr`: a complex array, the source's shape with the axis one shorter.

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
; csnroots.csd
;
; The roots of a polynomial given by its coefficients, highest degree first,
; as np.roots takes them. A polynomial of degree N-1 has N-1 roots, returned
; as a complex vector in no particular order, even when all of them are real.
;
; With an axis, every 1-D slice along it is a separate polynomial: a (3, 2)
; matrix read along axis 0 is two quadratics, and gives a (2, 2) matrix of
; roots.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    ; x^3 - 6x^2 + 11x - 6 = (x - 1)(x - 2)(x - 3)
    ic[] = fillarray(1, -6, 11, -6)
    coefs:CsnArr = csnfromarray(ic)
    roots:CsnArr = csnroots(coefs)
    csnprint(roots)

    ; x^2 + 1 has no real root: +i and -i
    iq[] = fillarray(1, 0, 1)
    quad:CsnArr = csnfromarray(iq)
    croots:CsnArr = csnroots(quad)
    csnprint(croots)

    ; two quadratics side by side, one per column: x^2 - 3x + 2 and x^2 - 4
    iflat[] = fillarray(1, 1, -3, 0, 2, -4)
    ishape[] = fillarray(3, 2)
    flat:CsnArr = csnfromarray(iflat)
    cols:CsnArr = csnreshape(flat, ishape)
    colroots:CsnArr = csnroots(cols, 0)
    csnprint(colroots)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnlagrange](csnlagrange.md)
* [csnreal](csnreal.md)
* [csnsort](csnsort.md)

## Credits

Pasquale Mainolfi, 2026
