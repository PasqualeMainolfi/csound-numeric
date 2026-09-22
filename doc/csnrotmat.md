# csnrotmat

## Abstract

A 3 x 3 rotation matrix, about a basis axis or about an arbitrary direction.

## Description

`csnrotmat` publishes a `3 x 3` handle ready for [csnmatmul](csnmatmul.md).
Two forms share the name. One turns about a basis axis picked by index:

```
axis 0   Rx = [1  0  0 ; 0  c -s ; 0  s  c]
axis 1   Ry = [c  0  s ; 0  1  0 ;-s  0  c]
axis 2   Rz = [c -s  0 ; s  c  0 ; 0  0  1]
```

The other turns about an arbitrary direction, by Rodrigues' formula, which for
a basis direction gives exactly the matrix above.

These are **active rotations of a column vector in a fixed right-handed
frame**: `v2 = R * v` rotates the source, not the frame. In the AmbiX frame
(`+x` front, `+y` left, `+z` up) a positive rotation about `+z` takes a source
from the front towards the left, which is what a positive yaw does, and a
positive rotation about `+x` takes it from the left to up.

**About `+y` it takes the front downwards.** The audio pitch convention is the
opposite sign, a rotation about `-y`, and is deliberately not what this opcode
builds: a general matrix constructor that quietly carried an audio convention
would mislead anyone using it for plain geometry. Compose the audio convention
from these, or negate the angle.

To rotate the frame rather than the source, [csntranspose](csntranspose.md)
gives the inverse: for a rotation the transpose is the inverse, so no flag is
needed to ask for one. That is also how a listener's own orientation is
compensated.

The direction of the arbitrary-axis form is normalised internally, so a
direction that arrives straight from [csncross](csncross.md) works whatever its
length. The null vector is the one refusal: it names no axis.

In the performance form the matrix is rebuilt on every non-zero trigger. Its
init pass reads the angle at whatever the init chain has already written into
it, which is `0` unless an earlier line set it with `init` — a k-rate
assignment does not write at i-time. So the matrix that reaches the first
control period is normally the identity, which is the right answer for a
rotation nobody has set yet and is still a proper rotation to multiply by.

## Syntax

```csound
handle:CsnArr csnrotmat angle:i, axis:i [, degrees:i]
handle:CsnArr csnrotmat angle:k, axis:i [, degrees:i] [, trig:k]
handle:CsnArr csnrotmat direction:CsnArr, angle:i [, degrees:i]
handle:CsnArr csnrotmat direction:CsnArr, angle:k [, degrees:i] [, trig:k]
```

## Arguments

* `angle`: the rotation angle, positive by the right-hand rule about the axis.
* `axis:i`: `0` turns about `+x`, `1` about `+y`, `2` about `+z`. Anything else is an error. It is an i-argument in both forms: it picks which matrix is built, not what it is built from.
* `direction:CsnArr`: a 1-D real array of exactly 3 elements, the axis to turn about. Any length; it is normalised internally. The null vector is an error.
* `degrees:i` (optional, default `0`): `0` reads the angle in radians, `1` in degrees. The output is a matrix and carries no unit, so this applies to the input only.
* `trig:k` (optional, default `1`): the matrix is rebuilt on a non-zero trigger; a zero trigger republishes the previous one.

## Output

* `handle:CsnArr`: a `3 x 3` real array.

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
; csnrotmat.csd
;
; A 3 x 3 rotation matrix, ready for csnmatmul. Two forms share the name: one
; turns about a basis axis picked by index, the other about an arbitrary
; direction held in a CsnArr.
;
; The matrices are the mathematical right-handed ones. In the AmbiX frame
; (+x front, +y left, +z up) that makes a positive rotation about +z take a
; source from the front towards the left, which is what a positive yaw does,
; and a positive rotation about +x take it from the left to up. About +y it
; takes the front downwards: the audio pitch convention is the opposite sign,
; a rotation about -y, and is not what this opcode builds.
;
; To rotate the frame rather than the source, csntranspose gives the inverse:
; for a rotation the transpose is the inverse, so no flag is needed to ask.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

opcode showvec(m:CsnArr, v:CsnArr, Sn:S):void
    prod:CsnArr = csnmatmul(m, v)
    flat:CsnArr = csnflatten(prod)
    o:i[]       = csntoarray(flat)
    prints("%-22s -> (%6.3f %6.3f %6.3f)\n", Sn, o[0], o[1], o[2])
endop

instr 1
    ipi2 = 1.5707963267948966

    ; the source sits at the front, as a 3 x 1 column
    ifront:i[] = fillarray(1, 0, 0)
    ishape:i[] = fillarray(3, 1)
    front:CsnArr = csnreshape(csnfromarray(ifront), ishape)

    Rz:CsnArr = csnrotmat(ipi2, 2)
    showvec(Rz, front, "Rz(90) * front")

    Ry:CsnArr = csnrotmat(ipi2, 1)
    showvec(Ry, front, "Ry(90) * front")

    ; the same rotation asked for in degrees
    Rzd:CsnArr = csnrotmat(90, 2, 1)
    showvec(Rzd, front, "Rz(90 deg) * front")

    ; about an arbitrary direction: the length does not matter, only the
    ; direction, so this is the same rotation as Rz above
    iaxis:i[] = fillarray(0, 0, 7)
    axis:CsnArr = csnfromarray(iaxis)
    Ra:CsnArr = csnrotmat(axis, ipi2)
    showvec(Ra, front, "about (0,0,7)")

    ; composition is a matrix product, applied right to left: Rz turns the
    ; front towards the left, then Rx lifts the left up
    Rx:CsnArr = csnrotmat(ipi2, 0)
    RxRz:CsnArr = csnmatmul(Rx, Rz)
    showvec(RxRz, front, "Rx * Rz * front")

    ; and the inverse rotation is the transpose
    back:CsnArr = csnmatmul(csntranspose(Rz), csnmatmul(Rz, front))
    showvec(csnidentity(3), back, "Rz transposed, undone")
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnmatmul](csnmatmul.md)
* [csntranspose](csntranspose.md)
* [csnidentity](csnidentity.md)
* [csncross](csncross.md)
* [csnhoatocar](csnhoatocar.md)

## Credits

Pasquale Mainolfi, 2026
