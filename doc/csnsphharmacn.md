# csnsphharmacn

## Abstract

Every real spherical harmonic up to an order, indexed by ACN, at one direction
or at many.

## Description

`csnsphharmacn` computes the same harmonics as [csnsphharm](csnsphharm.md)
(AmbiX: SN3D, no Condon-Shortley phase, no `1/sqrt(4 pi)`), for every
`0 <= n <= order` and `-n <= m <= n` together, in ACN order. That is
`(order + 1)^2` values. The set costs `O(order^2)` per direction: one column of
fixed `m` is walked upwards in `n` from the diagonal, and the diagonal is carried
from one column to the next.

Two forms share the name.

**One direction** gives a vector of `(order + 1)^2` values. These are the
encoding gains of a mono source at that direction: multiply a sample by it and
you have the set. The encoding itself is left to you. Distance plays no part,
because a harmonic describes a direction only.

**D directions**, as two 1-D arrays of azimuths and elevations of the same
length, give a `((order + 1)^2, D)` matrix with one direction per column. That
is the column-vector layout [csnmatmul](csnmatmul.md) uses, and a mode-matching
decoder is built from it. For as many loudspeakers as channels the decoder is
its inverse ([csninv](csninv.md)). For more loudspeakers it is the
pseudo-inverse, `transpose(Y) * inv(Y * transpose(Y))`. The example builds the
square case for a tetrahedron.

The order must be an integer in `[0, 1000]`. It fixes the output length, so it
is an i-argument in every form, and so is the angle unit. In the one-direction
performance form the angles are read on every non-zero trigger. In the
many-direction performance form the matrix is recomputed only when either
direction array has been written since the last pass. The direction arrays
cannot be this opcode's own output.

At the poles, and past them, the harmonics behave as in
[csnsphharm](csnsphharm.md): the azimuth drops out at the pole itself, and an
elevation past it means the direction on the other side.

## Syntax

```csound
handle:CsnArr csnsphharmacn order:i, azimuth:i, elevation:i [, degrees:i]
handle:CsnArr csnsphharmacn order:i, azimuth:k, elevation:k [, degrees:i] [, trig:k]
handle:CsnArr csnsphharmacn order:i, azimuths:CsnArr, elevations:CsnArr [, degrees:i]
handle:CsnArr csnsphharmacn order:i, azimuths:CsnArr, elevations:CsnArr [, degrees:i] [, trig:k]
```

## Arguments

* `order:i`: the highest order, an integer in `[0, 1000]`.
* `azimuth`, `elevation`: one direction. Azimuth from `+x` towards `+y`, elevation from the horizon.
* `azimuths:CsnArr`, `elevations:CsnArr`: 1-D real arrays of the same non-zero length `D`, one direction per element.
* `degrees:i` (optional, default `0`): `0` reads the angles in radians, `1` in degrees.
* `trig:k` (optional, default `1`): the result is recomputed on a non-zero trigger. A zero trigger republishes the previous one.

## Output

* `handle:CsnArr`: a real vector of `(order + 1)^2` values indexed by ACN, or a `((order + 1)^2, D)` matrix with one direction per column.

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
; csnsphharmacn.csd
;
; Every real spherical harmonic up to an order, indexed by ACN, in the AmbiX
; convention (SN3D, no Condon-Shortley phase). Two forms:
;
;   one direction    -> a vector of (order + 1)^2 values: the encoding gains
;                       of a mono source at that direction
;   D directions     -> a ((order + 1)^2, D) matrix, one direction per column
;
; The matrix is what a mode-matching decoder is built from: for a layout of
; as many loudspeakers as channels, the decoder is its inverse (csninv), and
; for more loudspeakers the pseudo-inverse. The example builds a first-order
; decoder for a tetrahedron and checks that a source encoded exactly at one
; loudspeaker comes out of that loudspeaker alone.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    ; the encoding gains of a source at azimuth 30, elevation 20, order 2
    gains:CsnArr = csnsphharmacn(2, 30, 20, 1)
    ig[] = csntoarray(gains)
    prints("order 2, %d channels\n", lenarray(ig))
    prints("  n=0 : %.4f\n", ig[0])
    prints("  n=1 : %.4f %.4f %.4f\n", ig[1], ig[2], ig[3])
    prints("  n=2 : %.4f %.4f %.4f %.4f %.4f\n", ig[4], ig[5], ig[6], ig[7], ig[8])

    ; SN3D: the squares of each order sum to 1
    isq = ig[1] ^ 2 + ig[2] ^ 2 + ig[3] ^ 2
    prints("  sum of squares of order 1: %.6f\n", isq)

    ; a tetrahedron, four loudspeakers for four first-order channels
    iel = 35.26438968275466
    iaz[] = fillarray(45, 135, -135, -45)
    ielv[] = fillarray(iel, -iel, iel, -iel)
    az:CsnArr = csnfromarray(iaz)
    el:CsnArr = csnfromarray(ielv)
    Y:CsnArr = csnsphharmacn(1, az, el, 1)
    ishape[] = csnshape(Y)
    prints("direction matrix %d x %d (channels x loudspeakers)\n", ishape[0], ishape[1])

    ; mode matching: decoder = inverse of the direction matrix
    D:CsnArr = csninv(Y)

    ; a source encoded at the second loudspeaker
    icol[] = fillarray(4, 1)
    enc:CsnArr = csnsphharmacn(1, 135, -iel, 1)
    src:CsnArr = csnreshape(enc, icol)
    feeds:CsnArr = csnmatmul(D, src)
    flat:CsnArr = csnflatten(feeds)
    iF[] = csntoarray(flat)
    prints("loudspeaker feeds: %.4f %.4f %.4f %.4f\n", iF[0], iF[1], iF[2], iF[3])
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
* [csnlegendre](csnlegendre.md)
* [csnnmtoacn](csnnmtoacn.md)
* [csnhoaordtochnls](csnhoaordtochnls.md)
* [csnsn3dton3d](csnsn3dton3d.md)
* [csninv](csninv.md)
* [csnmatmul](csnmatmul.md)

## Credits

Pasquale Mainolfi, 2026
