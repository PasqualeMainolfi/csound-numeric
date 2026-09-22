# csnnmtoacn

## Abstract

The ACN channel index of an ambisonics component, from its order and degree.

## Description

`csnnmtoacn` answers

```
ACN = n*n + n + m
```

the index AmbiX gives the component of order `n` and degree `m`. The components
come out in the order `(0,0), (1,-1), (1,0), (1,1), (2,-2)...`, which is
exactly `0, 1, 2, 3, 4...`, so the index doubles as the position in the
channel set. [csnacntonm](csnacntonm.md) reads the pair back.

`n` must be zero or more and `m` must lie in `[-n, n]`: outside that range the
formula collides with another component's index, so it is refused rather than
answered.

Ambisonics channel bookkeeping in csnum is ACN ordering only, and SN3D or N3D
normalisation only. Other orderings and normalisations exist; none of them is
what these opcodes answer.

## Syntax

```csound
acn:i csnnmtoacn n:i, m:i
acn:k csnnmtoacn n:k, m:k
```

## Arguments

* `n`: the order, zero or more.
* `m`: the degree, in `[-n, n]`.

## Output

* `acn`: the channel index.

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
; csnnmtoacn.csd
;
; The ACN channel index of an ambisonics component, from its order n and its
; degree m:
;
;   ACN = n*n + n + m        with 0 <= n and -n <= m <= n
;
; ACN is the ordering AmbiX uses: the components come out in the order
; (0,0), (1,-1), (1,0), (1,1), (2,-2) ... which is exactly 0, 1, 2, 3, 4 ...
; csnacntonm reads the pair back.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    ; the whole of first order, in ACN order
    i00 = csnnmtoacn(0,  0)
    i1m = csnnmtoacn(1, -1)
    i10 = csnnmtoacn(1,  0)
    i1p = csnnmtoacn(1,  1)
    prints("first order: (0,0)=%d (1,-1)=%d (1,0)=%d (1,1)=%d\n", i00, i1m, i10, i1p)

    ; the corners of second order
    i2m = csnnmtoacn(2, -2)
    i2p = csnnmtoacn(2,  2)
    prints("second order runs from %d to %d\n", i2m, i2p)

    ; and the pair comes back
    in, im csnacntonm i1p
    prints("ACN %d is order %d degree %d\n", i1p, in, im)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnacntonm](csnacntonm.md)
* [csnhoaordtochnls](csnhoaordtochnls.md)
* [csnsn3dton3d](csnsn3dton3d.md)

## Credits

Pasquale Mainolfi, 2026
