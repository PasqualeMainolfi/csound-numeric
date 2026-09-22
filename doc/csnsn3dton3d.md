# csnsn3dton3d

## Abstract

The per-channel gains that carry an SN3D-normalised set to N3D.

## Description

`csnsn3dton3d` builds

```
gain(n) = sqrt(2*n + 1)
```

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
handle:CsnArr csnsn3dton3d order:i
```

Note the opcode syntax: a one-argument functional call returning a `CsnArr`,
`csnsn3dton3d(2)`, is not what the parser expects. It parses inside a larger
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
; csnsn3dton3d.csd
;
; The per-channel gains that carry an SN3D-normalised set to N3D:
;
;   gain(n) = sqrt(2*n + 1)
;
; one value per ACN channel, so the vector is (order + 1)^2 long. The factor
; depends on the order n alone, not on the degree m, so the vector is constant
; over blocks of 2n+1 consecutive channels.
;
; It is a gain vector, not a converter: multiply your own set by it.
; csnn3dtosn3d builds the reciprocal.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    ; note the opcode syntax rather than csnsn3dton3d(2): a one-argument
    ; functional call returning a CsnArr is not what the parser expects
    gains:CsnArr csnsn3dton3d 2
    ig[] = csntoarray(gains)
    prints("order 2, %d channels\n", csnsize(gains))
    prints("  n=0 : %.4f\n", ig[0])
    prints("  n=1 : %.4f %.4f %.4f\n", ig[1], ig[2], ig[3])
    prints("  n=2 : %.4f %.4f %.4f %.4f %.4f\n", ig[4], ig[5], ig[6], ig[7], ig[8])

    ; applying it, and taking it back with the reciprocal vector
    isrc[] = fillarray(1, 1, 1, 1, 1, 1, 1, 1, 1)
    set:CsnArr = csnfromarray(isrc)
    n3d:CsnArr = csnmul(set, gains)
    back:CsnArr = csnmul(n3d, csnn3dtosn3d(2))
    ib[] = csntoarray(back)
    prints("round trip on a unit set: %.4f %.4f %.4f\n", ib[0], ib[4], ib[8])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnn3dtosn3d](csnn3dtosn3d.md)
* [csnmul](csnmul.md)
* [csnhoaordtochnls](csnhoaordtochnls.md)
* [csnnmtoacn](csnnmtoacn.md)

## Credits

Pasquale Mainolfi, 2026
