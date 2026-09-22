# csnsphharm

## Abstract

One real spherical harmonic at one direction, AmbiX convention (ACN, SN3D).

## Description

`csnsphharm` computes

```
Y_n^m(az, el) = N_n^|m| P_n^|m|(sin el) trig(m, az)

trig(m, az) = sin(|m| az)   m < 0
              cos(m az)     m >= 0

N_n^|m| = sqrt((2 - delta_m) (n - |m|)! / (n + |m|)!)
```

This is the real harmonic of the AmbiX convention: SN3D normalisation, **no
Condon-Shortley phase**, and no global `1/sqrt(4 pi)`. `P` is the
[csnlegendre](csnlegendre.md) function. The direction is read the way
[csnhoatocar](csnhoatocar.md) reads it: azimuth from `+x` (front) towards `+y`
(left), elevation from the horizon, positive upwards.

The first order is the unit direction itself: `Y_1^-1 = y`, `Y_1^0 = z`,
`Y_1^1 = x`, and `Y_0^0 = 1`. So a first-order encoder is this opcode at `n = 1`,
and it agrees with `bformenc1` apart from the FuMa `1/sqrt(2)` on `W`. With
the Condon-Shortley phase, `Y` and `X` would come out negated.

In SN3D the squares of one order sum to 1 in every direction:
`sum over m of (Y_n^m)^2 = 1`. Multiplying by [csnsn3dton3d](csnsn3dton3d.md)
gives N3D, where the same sum is `2n + 1`.

The normalisation is carried through the recurrence, so the factorial ratio
is never formed and every value stays in `[-1, 1]` at any order up to 1000.

**At the poles**, `el = +-90` degrees, the azimuth is undefined, and nothing
depends on it: every `m != 0` vanishes and every `m = 0` reads 1. An elevation
past the pole is accepted and means the direction on the other side, at
azimuth `+ 180`. The cosine of the elevation is used with its sign for that
reason, not as `sqrt(1 - sin^2)`.

**Distance plays no part.** A harmonic describes a direction. Attenuation,
delay and near-field effects are the encoder's job.

`n` must be an integer in `[0, 1000]` and `m` an integer in `[-n, n]`. For the
whole set at once, indexed by ACN, use [csnsphharmacn](csnsphharmacn.md).

## Syntax

```csound
value:i csnsphharm n:i, m:i, azimuth:i, elevation:i [, degrees:i]
value:k csnsphharm n:k, m:k, azimuth:k, elevation:k [, degrees:i]
```

## Arguments

* `n`: the order, an integer in `[0, 1000]`.
* `m`: the degree, an integer in `[-n, n]`. Its ACN index is `n^2 + n + m` ([csnnmtoacn](csnnmtoacn.md)).
* `azimuth`: from `+x` towards `+y`.
* `elevation`: from the horizon, positive upwards.
* `degrees:i` (optional, default `0`): `0` reads the angles in radians, `1` in degrees. It is an i-argument in both forms.

## Output

* `value`: `Y_n^m` at that direction.

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
; csnsphharm.csd
;
; One real spherical harmonic Y_n^m at one direction, in the AmbiX convention:
; ACN ordering, SN3D normalisation, no global 1/sqrt(4*pi), no Condon-Shortley
; phase. The direction is the one csnhoatocar reads: azimuth from +x (front)
; towards +y (left), elevation from the horizon.
;
;   Y_n^m = sqrt((2 - delta_m) (n-|m|)! / (n+|m|)!) P_n^|m|(sin el) trig(m az)
;   trig  = sin(|m| az) for m < 0, cos(m az) for m >= 0
;
; The first order is the direction itself, which is why a first-order encoder
; is a special case of it: the example checks that against bformenc1.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    iaz = 30
    iel = 20

    ; the first order: W, then Y, Z, X in ACN order
    iW = csnsphharm(0, 0, iaz, iel, 1)
    iY = csnsphharm(1, -1, iaz, iel, 1)
    iZ = csnsphharm(1, 0, iaz, iel, 1)
    iX = csnsphharm(1, 1, iaz, iel, 1)
    prints("azimuth %d, elevation %d degrees\n", iaz, iel)
    prints("  W %.6f  Y %.6f  Z %.6f  X %.6f\n", iW, iY, iZ, iX)

    ; the same numbers are the unit direction
    ix, iy, iz csnhoatocar 1, iaz, iel, 1
    prints("  unit direction x %.6f  y %.6f  z %.6f\n", ix, iy, iz)

    ; a higher harmonic, radians this time
    iY3m2 = csnsphharm(3, -2, iaz * $M_PI / 180, iel * $M_PI / 180)
    prints("  Y(3, -2) %.6f\n", iY3m2)
endin

instr 2
    ; bformenc1 encodes first-order FuMa: W scaled by 1/sqrt(2), then X, Y, Z.
    ; On a constant unit signal its X, Y, Z are exactly the three first-order
    ; harmonics, and W times sqrt(2) is Y_0^0 = 1.
    asig = 1
    aw, ax, ay, az bformenc1 asig, 30, 20
    kn init 0
    if kn == 1 then
        kW = k(aw) * sqrt(2)
        kX = k(ax)
        kY = k(ay)
        kZ = k(az)
        kHX = csnsphharm(1, 1, 30, 20, 1)
        kHY = csnsphharm(1, -1, 30, 20, 1)
        kHZ = csnsphharm(1, 0, 30, 20, 1)
        printks("bformenc1  W %.6f  X %.6f  Y %.6f  Z %.6f\n", 0, kW, kX, kY, kZ)
        printks("csnsphharm W 1.000000  X %.6f  Y %.6f  Z %.6f\n", 0, kHX, kHY, kHZ)
        turnoff
    endif
    kn += 1
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
i 2 0.2 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnsphharmacn](csnsphharmacn.md)
* [csnlegendre](csnlegendre.md)
* [csnhoatocar](csnhoatocar.md)
* [csnnmtoacn](csnnmtoacn.md)
* [csnsn3dton3d](csnsn3dton3d.md)

## Credits

Pasquale Mainolfi, 2026
