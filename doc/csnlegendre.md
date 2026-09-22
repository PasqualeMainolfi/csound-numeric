# csnlegendre

## Abstract

The associated Legendre function `P_n^m(x)`, without the Condon-Shortley phase.

## Description

`csnlegendre` computes `P_n^m(x)` for `0 <= m <= n` and `x` in `[-1, 1]` by the
upward recurrence in `n`:

```
P_m^m     = (2m - 1)!! (1 - x^2)^(m/2)
P_{m+1}^m = x (2m + 1) P_m^m
P_n^m     = ((2n - 1) x P_{n-1}^m - (n + m - 1) P_{n-2}^m) / (n - m)
```

**There is no Condon-Shortley phase.** The `(-1)^m` factor that many physics
references and `scipy.special.lpmv` include is left out, because ambisonics does
not use it and [csnsphharm](csnsphharm.md) is built on this same function. The
relation is

```
scipy.special.lpmv(m, n, x) == (-1)^m * csnlegendre(n, m, x)
```

The argument order is the one the harmonics use: degree `n` first, then order
`m`. scipy puts `m` first.

`n` and `m` must be integers, `n` in `[0, 1000]` and `m` in `[0, n]`. A
negative `m` is refused rather than computed: `P_n^{-m}` carries a factorial
ratio of its own, and half supporting it would be a trap. An `x` outside
`[-1, 1]` is refused rather than clamped, since it is a wrong argument and not a
rounding error.

The function is unnormalised, so it grows fast with `m`: `(2m - 1)!!` passes
the double range near `m = 150`. A value that overflows is an error, not an
infinity. The harmonics do not have that limit, because they carry their
normalisation through the recurrence instead of dividing by it afterwards.

The handle form maps a real array of any shape elementwise and keeps its shape.
Every element is checked before anything is written. `n` and `m` are
i-arguments there, in both the init and the performance form, because they pick
the function rather than feed it. At k-rate the result is recomputed only when
the source has been written since the last pass, and assigning it back to its
own input is allowed.

## Syntax

```csound
value:i csnlegendre n:i, m:i, x:i
value:k csnlegendre n:k, m:k, x:k
handle:CsnArr csnlegendre n:i, m:i, source:CsnArr
handle:CsnArr csnlegendre n:i, m:i, source:CsnArr, trig:k
```

## Arguments

* `n`: the degree, an integer in `[0, 1000]`.
* `m`: the order, an integer in `[0, n]`.
* `x`: the argument, in `[-1, 1]`.
* `source:CsnArr`: a real array of any shape, every element in `[-1, 1]`.
* `trig:k` (optional, default `1`): the result is recomputed on a non-zero trigger. A zero trigger republishes the previous one.

## Output

* `value`: `P_n^m(x)`.
* `handle:CsnArr`: a real array shaped like `source`, holding `P_n^m` of each element.

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
; csnlegendre.csd
;
; The associated Legendre function P_n^m(x), for 0 <= m <= n and x in [-1, 1],
; WITHOUT the Condon-Shortley phase:
;
;   scipy.special.lpmv(m, n, x) == (-1)^m * csnlegendre(n, m, x)
;
; Note the argument order: degree n first, then order m, as the harmonics
; write it, not scipy's (m, n).
;
; One form is scalar, at init or k-rate; the other maps a handle elementwise
; and keeps its shape.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    ix = 0.3
    prints("x = %.2f\n", ix)
    in = 0
    while in <= 3 do
        im = 0
        while im <= in do
            iP = csnlegendre(in, im, ix)
            prints("  P(%d, %d) = %+.6f\n", in, im, iP)
            im += 1
        od
        in += 1
    od

    ; P_2^1 is 3x sqrt(1 - x^2) here; scipy's lpmv(1, 2, x) is its negative
    iP21 = csnlegendre(2, 1, ix)
    iCs = -iP21
    prints("P(2, 1) = %.6f, with the Condon-Shortley phase %.6f\n", iP21, iCs)

    ; elementwise over a handle: P_3^0 on a grid over [-1, 1]
    grid:CsnArr = csnlinspace(-1, 1, 5)
    p30:CsnArr = csnlegendre(3, 0, grid)
    ig[] = csntoarray(grid)
    ip[] = csntoarray(p30)
    prints("P(3, 0) at %.1f %.1f %.1f %.1f %.1f\n", ig[0], ig[1], ig[2], ig[3], ig[4])
    prints("         = %.4f %.4f %.4f %.4f %.4f\n", ip[0], ip[1], ip[2], ip[3], ip[4])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnsphharm](csnsphharm.md)
* [csnsphharmacn](csnsphharmacn.md)

## Credits

Pasquale Mainolfi, 2026
