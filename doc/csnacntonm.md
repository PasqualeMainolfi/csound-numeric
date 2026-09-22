# csnacntonm

## Abstract

The order and degree an ACN channel index stands for.

## Description

`csnacntonm` answers

```
n = floor(sqrt(ACN))
m = ACN - n*n - n
```

the inverse of [csnnmtoacn](csnnmtoacn.md), as two values rather than one. A
negative index names no component and is refused.

Ambisonics channel bookkeeping in csnum is ACN ordering only, and SN3D or N3D
normalisation only. Other orderings and normalisations exist; none of them is
what these opcodes answer.

## Syntax

```csound
n:i, m:i csnacntonm acn:i
n:k, m:k csnacntonm acn:k
```

## Arguments

* `acn`: the channel index, zero or more.

## Output

* `n`: the order.
* `m`: the degree, in `[-n, n]`.

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
; csnacntonm.csd
;
; The order and degree an ACN channel index stands for:
;
;   n = floor(sqrt(ACN))
;   m = ACN - n*n - n
;
; It is the inverse of csnnmtoacn, and answers two values rather than one.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    ; walk the first two orders, channel by channel
    ik = 0
    while ik < 9 do
        in, im csnacntonm ik
        prints("ACN %d -> order %d degree %2d\n", ik, in, im)
        ik += 1
    od

    ; the round trip closes
    in2, im2 csnacntonm 15
    iback = csnnmtoacn(in2, im2)
    prints("15 -> (%d, %d) -> %d\n", in2, im2, iback)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnnmtoacn](csnnmtoacn.md)
* [csnchnlstohoaord](csnchnlstohoaord.md)

## Credits

Pasquale Mainolfi, 2026
