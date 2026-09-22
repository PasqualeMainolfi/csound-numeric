# csnchnlstohoaord

## Abstract

The ambisonics order a channel count stands for.

## Description

`csnchnlstohoaord` answers

```
order = sqrt(channels) - 1
```

the inverse of [csnhoaordtochnls](csnhoaordtochnls.md).

**Only a complete 3-D set has an order**: `1, 4, 9, 16, 25...` channels.
Anything else is refused rather than rounded, because a fractional order is not
a smaller set, it is a set that does not exist. A count of 5 is an error, not
order 1 with a channel to spare.

Ambisonics channel bookkeeping in csnum is ACN ordering only, and SN3D or N3D
normalisation only. Other orderings and normalisations exist; none of them is
what these opcodes answer.

## Syntax

```csound
order:i csnchnlstohoaord channels:i
order:k csnchnlstohoaord channels:k
```

## Arguments

* `channels`: the number of channels, a perfect square of at least 1.

## Output

* `order`: the ambisonics order.

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
; csnchnlstohoaord.csd
;
; The ambisonics order a channel count stands for:
;
;   order = sqrt(channels) - 1
;
; Only a complete 3-D set has an order: 1, 4, 9, 16, 25 ... channels. Anything
; else is refused rather than rounded, because a fractional order is not a
; smaller set, it is a set that does not exist.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    i1  = csnchnlstohoaord(1)
    i4  = csnchnlstohoaord(4)
    i9  = csnchnlstohoaord(9)
    i16 = csnchnlstohoaord(16)
    prints("1 -> %d, 4 -> %d, 9 -> %d, 16 -> %d\n", i1, i4, i9, i16)

    ; the trip back
    iback = csnhoaordtochnls(i16)
    prints("order %d is %d channels again\n", i16, iback)
    turnoff
endin

instr 2
    ; 5 channels is not a complete set: refused, not rounded down to order 1
    ibad = csnchnlstohoaord(5)
    prints("never reached\n")
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0    0.1
i 2 0.15 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnhoaordtochnls](csnhoaordtochnls.md)
* [csnacntonm](csnacntonm.md)

## Credits

Pasquale Mainolfi, 2026
