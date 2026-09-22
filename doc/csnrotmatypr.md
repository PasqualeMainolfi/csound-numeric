# csnrotmatypr

## Abstract

One 3 x 3 rotation matrix for a yaw, a pitch and a roll taken together.

## Description

`csnrotmatypr` composes the three head-orientation angles into a single matrix:

```
R = Rz(yaw) * Ry(-pitch) * Rx(roll)
```

applied right to left, so a source is rolled first, then pitched, then yawed.

**Handing back the composed matrix rather than the three factors is the point.**
A different order is a different rotation, and composing the three by hand is
exactly where it goes wrong. The order is written once, here.

The convention is the audio one, in the AmbiX frame (`+x` front, `+y` left,
`+z` up): a positive yaw turns a source from the front towards the left, a
positive pitch lifts the front upwards, and a positive roll takes the left side
up. That pitch is a rotation about `-y`, **not** the right-handed `Ry` that
[csnrotmat](csnrotmat.md) builds. The sign is why this opcode carries its own
name instead of being a mode of the general constructor: a matrix builder meant
for plain geometry must not quietly hold an audio convention.

The result is an active rotation of a column vector in a fixed frame,
`v2 = R * v`: it rotates the source, not the frame. A listener turning their
own head is the inverse, which for a rotation is the transpose, so
[csntranspose](csntranspose.md) is all it takes and no flag is needed to ask.

In the performance form the matrix is rebuilt on every non-zero trigger. Its
init pass reads the angles at whatever the init chain has already written into
them, which is `0` unless an earlier line set them with `init` — a k-rate
assignment does not write at i-time. So the matrix that reaches the first
control period is normally the identity.

## Syntax

```csound
handle:CsnArr csnrotmatypr yaw:i, pitch:i, roll:i [, degrees:i]
handle:CsnArr csnrotmatypr yaw:k, pitch:k, roll:k [, degrees:i] [, trig:k]
```

## Arguments

* `yaw`: rotation about `+z`, positive from the front towards the left.
* `pitch`: rotation about `-y`, positive from the front upwards. This is the audio sign, the opposite of a right-handed rotation about `+y`.
* `roll`: rotation about `+x`, positive taking the left side up.
* `degrees:i` (optional, default `0`): `0` reads the angles in radians, `1` in degrees. One flag for all three. The output is a matrix and carries no unit, so this applies to the inputs only.
* `trig:k` (optional, default `1`): the matrix is rebuilt on a non-zero trigger; a zero trigger republishes the previous one.

## Output

* `handle:CsnArr`: a `3 x 3` real array, ready for [csnmatmul](csnmatmul.md).

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
; csnrotmatypr.csd
;
; One 3 x 3 matrix for a yaw, a pitch and a roll taken together:
;
;   R = Rz(yaw) * Ry(-pitch) * Rx(roll)
;
; applied right to left, so a source is rolled first, then pitched, then yawed.
; Handing back the composed matrix rather than the three factors is the whole
; point: a different order is a different rotation, and composing them by hand
; is where it goes wrong.
;
; The convention is the audio one, in the AmbiX frame (+x front, +y left,
; +z up): a positive yaw turns towards the left and a positive pitch lifts the
; front upwards. That pitch is a rotation about -y, not the right-handed Ry
; that csnrotmat builds, which is why this opcode carries its own name.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

opcode applied(m:CsnArr, v:CsnArr, Sn:S):void
    prod:CsnArr = csnmatmul(m, v)
    flat:CsnArr = csnflatten(prod)
    o:i[]       = csntoarray(flat)
    prints("%-26s -> (%6.3f %6.3f %6.3f)\n", Sn, o[0], o[1], o[2])
endop

instr 1
    ; the source sits at the front, as a 3 x 1 column
    ifront:i[]   = fillarray(1, 0, 0)
    ishape:i[]   = fillarray(3, 1)
    front:CsnArr = csnreshape(csnfromarray(ifront), ishape)

    ; one angle at a time, in degrees
    Ry90:CsnArr = csnrotmatypr(90, 0, 0, 1)
    applied(Ry90, front, "yaw 90")
    Rp90:CsnArr = csnrotmatypr(0, 90, 0, 1)
    applied(Rp90, front, "pitch 90")

    ; all three together: rolled, then pitched, then yawed
    Rall:CsnArr = csnrotmatypr(90, 45, 0, 1)
    applied(Rall, front, "yaw 90 pitch 45")

    ; the order is not a detail. Yawing first and pitching after is a
    ; different rotation, and this is what it would look like
    Mz:CsnArr = csnrotmat(90, 2, 1)
    My:CsnArr = csnrotmat(-45, 1, 1)
    swapped:CsnArr = csnmatmul(My, Mz)
    applied(swapped, front, "pitch after yaw")

    ; a listener turning their head is the inverse rotation: for a rotation
    ; the transpose is the inverse, so no flag is needed to ask for one
    head:CsnArr = csnrotmatypr(30, 0, 0, 1)
    applied(csntranspose(head), front, "listener turned 30 left")

    ; and it reads back as a direction through the ambisonics pair
    turned:CsnArr = csnmatmul(csntranspose(head), front)
    tflat:CsnArr  = csnflatten(turned)
    t:i[]         = csntoarray(tflat)
    idist, iaz, iel csncartohoa t[0], t[1], t[2], 1
    prints("%-26s -> az %.2f el %.2f at %.3f\n", "as a direction", iaz, iel, idist)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnrotmat](csnrotmat.md)
* [csnmatmul](csnmatmul.md)
* [csntranspose](csntranspose.md)
* [csncartohoa](csncartohoa.md)
* [csnhoatocar](csnhoatocar.md)

## Credits

Pasquale Mainolfi, 2026
