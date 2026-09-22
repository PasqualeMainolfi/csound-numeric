# csnn3dtosn3d

## Abstract

The per-channel gains that carry an N3D-normalised set to SN3D.

## Description

`csnn3dtosn3d` builds

```
gain(n) = 1 / sqrt(2*n + 1)
```

the reciprocal, channel by channel, of what
[csnsn3dton3d](csnsn3dton3d.md) builds: multiplying the two vectors together
gives ones.

The vector is `(order + 1)^2` long, one value per ACN channel, and the factor
depends on the order `n` alone and not on the degree `m`: it is constant over
blocks of `2n+1` consecutive channels, and the `W` channel at ACN 0 is always
left at 1.

**It is a gain vector, not a converter.** It answers what to multiply by;
multiplying is yours to do, with [csnmul](csnmul.md) or whatever else the set
lives in. That keeps it numeric and leaves the channel plumbing where it
belongs.

A negative or fractional order is refused.

There is no performance-time form. A normalisation does not change during a
note: build the vector once at init and reuse the handle.

Ambisonics channel bookkeeping in csnum is ACN ordering only, and SN3D or N3D
normalisation only. Other orderings and normalisations exist; none of them is
what these opcodes answer.

## Syntax

```csound
handle:CsnArr csnn3dtosn3d order:i
```

Note the opcode syntax: a one-argument functional call returning a `CsnArr`,
`csnn3dtosn3d(1)`, is not what the parser expects. It parses inside a larger
expression, but on its own it needs the form above.

## Arguments

* `order:i`: the ambisonics order, zero or more.

## Output

* `handle:CsnArr`: a real vector of `(order + 1)^2` gains, indexed by ACN.

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
; csnn3dtosn3d.csd
;
; The per-channel gains that carry an N3D-normalised set to SN3D:
;
;   gain(n) = 1 / sqrt(2*n + 1)
;
; the reciprocal of what csnsn3dton3d builds, one value per ACN channel, so
; the vector is (order + 1)^2 long. The factor depends on the order n alone,
; not on the degree m.
;
; It is a gain vector, not a converter: multiply your own set by it.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    ; note the opcode syntax rather than csnn3dtosn3d(1): a one-argument
    ; functional call returning a CsnArr is not what the parser expects
    gains:CsnArr csnn3dtosn3d 1
    ig[] = csntoarray(gains)
    prints("order 1, %d channels\n", csnsize(gains))
    prints("  n=0 : %.4f          (W is untouched)\n", ig[0])
    prints("  n=1 : %.4f %.4f %.4f\n", ig[1], ig[2], ig[3])

    ; the two vectors multiply to one, channel by channel
    up:CsnArr = csnsn3dton3d(1)
    unit:CsnArr = csnmul(gains, up)
    iu[] = csntoarray(unit)
    prints("gains * reciprocal: %.4f %.4f %.4f %.4f\n", iu[0], iu[1], iu[2], iu[3])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnsn3dton3d](csnsn3dton3d.md)
* [csnmul](csnmul.md)
* [csnhoaordtochnls](csnhoaordtochnls.md)

## Credits

Pasquale Mainolfi, 2026
