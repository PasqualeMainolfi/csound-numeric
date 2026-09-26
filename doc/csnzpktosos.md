# csnzpktosos

## Abstract

Splits a digital filter given as zeros, poles and gain into second-order sections.

## Description

`csnzpktosos` turns a digital filter given as zeros, poles and gain into a
cascade of second-order sections, as `scipy.signal.zpk2sos` does with its
default `'nearest'` pairing. The result is an `(n, 6)` array, one section per
row:

```
[b0 b1 b2 a0 a1 a2]        H_i(z) = (b0 + b1 z^-1 + b2 z^-2) / (a0 + a1 z^-1 + a2 z^-2)
```

with `a0 = 1`, and the whole filter is their product. A cascade of low-order
sections keeps its precision at orders where the coefficients of a single
`b / a` would lose it.

The sections are built the way scipy builds them:

1. the shorter of the zeros and poles is padded with roots at the origin, and
   an odd count gets one more zero and one more pole at the origin, so every
   section is second order and there are `ceil(n / 2)` of them;
2. complex roots must come in conjugate pairs, within `100 eps`; each pair
   stays in one section, so every coefficient is real;
3. starting from the pole closest to the unit circle, each pole is paired
   with its conjugate, or with the real pole nearest the unit circle, and
   with the zeros nearest to it;
4. the sections are reversed, so the poles closest to the unit circle come
   last, and the gain multiplies the numerator of the first section.

A complex zero or pole without its conjugate is refused: a section with real
coefficients cannot hold it. No zeros and no poles gives the single row
`[k 0 0 1 0 0]`.

The `'keep_odd'` and `'minimal'` pairings of scipy, and with them analog
filters, are not available.

There is no performance-time form.

## Syntax

```csound
sos:CsnArr = csnzpktosos(zeros:CsnArr, poles:CsnArr, k:i)
```

## Arguments

* `zeros:CsnArr`: a 1-D real or complex array of zeros, possibly empty.
* `poles:CsnArr`: a 1-D real or complex array of poles, possibly empty.
* `k:i`: the gain, finite.

## Output

* `sos:CsnArr`: a real `(n, 6)` array of second-order sections.

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
; csnzpktosos.csd
;
; A digital filter given as zeros, poles and gain, split into second-order
; sections, as scipy.signal.zpk2sos does with its default 'nearest' pairing.
; Each row is [b0 b1 b2 a0 a1 a2], a0 = 1; the gain sits in the first row and
; the poles closest to the unit circle in the last one. A cascade of sections
; keeps its precision at orders where a single b / a would lose it.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    ; three zeros at Nyquist, a real pole at 0.5 and the pair 0.3 +- 0.4j
    iz[] = fillarray(-1, -1, -1)
    ipr[] = fillarray(0.5, 0.3, 0.3)
    ipi[] = fillarray(0, 0.4, -0.4)
    z:CsnArr = csnfromarray(iz)
    pr:CsnArr = csnfromarray(ipr)
    pi:CsnArr = csnfromarray(ipi)
    prc:CsnArr = csntocomplex(pr)
    pic:CsnArr = csntocomplex(pi)
    j:Complex = init(0, 1, 0)
    pij:CsnArr = csnmul(pic, j)
    p:CsnArr = csnadd(prc, pij)

    ; an odd order is padded with a zero and a pole at the origin:
    ; [[1 2 1 1 -0.5 0]
    ;  [1 1 0 1 -0.6 0.25]]
    sos:CsnArr = csnzpktosos(z, p, 1)
    csnprint(sos)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnzpktotf](csnzpktotf.md)
* [csntftozpk](csntftozpk.md)
* [csnbilinearzpk](csnbilinearzpk.md)

## Credits

Pasquale Mainolfi, 2026
