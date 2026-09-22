# csnhoatocar

## Abstract

The ambisonics description of a direction to cartesian coordinates.

## Description

`csnhoatocar` answers

```
x = distance * cos(elevation) * cos(azimuth)
y = distance * cos(elevation) * sin(azimuth)
z = distance * sin(elevation)
```

in the AmbiX frame, `+x` front, `+y` left, `+z` up. A positive azimuth turns a
source from the front towards the left, a positive elevation lifts it off the
horizontal plane, and `distance = 1` gives the unit direction vector. It is the
inverse of [csncartohoa](csncartohoa.md), which reads the three back in the
same order.

**The elevation is measured from the horizon, not from `+z`.** Its counterpart
in [csnsphtocar](csnsphtocar.md) is `theta`, the inclination from the zenith,
and the two are related by `elevation = pi/2 - theta`. Feeding one where the
other is expected mirrors a source about the horizontal plane and raises
nothing.

The distance scales the direction, it does not bend it, and no angle is
undefined here: unlike its inverse, this direction has no refusal.

## Syntax

```csound
x:i, y:i, z:i csnhoatocar distance:i, azimuth:i, elevation:i [, degrees:i]
x:k, y:k, z:k csnhoatocar distance:k, azimuth:k, elevation:k [, degrees:i]
```

## Arguments

* `distance`: the length of the vector. Pass `1` for a unit direction.
* `azimuth`: the angle from `+x` towards `+y`.
* `elevation`: the angle from the horizontal plane, positive upwards.
* `degrees:i` (optional, default `0`): `0` reads the angles in radians, `1` in degrees. It is an i-argument in both forms: it picks how the conversion reads and writes, it is not a value that evolves during the note.

## Output

* `x, y, z`: the cartesian coordinates, in the AmbiX frame.

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
; csnhoatocar.csd
;
; The ambisonics description of a direction back to cartesian coordinates:
;
;   x = distance * cos(el) * cos(az)
;   y = distance * cos(el) * sin(az)
;   z = distance * sin(el)
;
; The frame is the AmbiX one, +x front, +y left, +z up. A positive azimuth
; turns towards the left, a positive elevation lifts off the horizontal plane,
; and a distance of 1 gives the unit direction vector.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    ipi2 = 1.5707963267948966

    ; one direction per axis, at unit distance
    ix1, iy1, iz1 csnhoatocar 1, 0, 0
    prints("az 0     el 0    -> %.4f %.4f %.4f  front\n", ix1, iy1, iz1)
    ix2, iy2, iz2 csnhoatocar 1, ipi2, 0
    prints("az pi/2  el 0    -> %.4f %.4f %.4f  left\n", ix2, iy2, iz2)
    ix3, iy3, iz3 csnhoatocar 1, 0, ipi2
    prints("az 0     el pi/2 -> %.4f %.4f %.4f  up\n", ix3, iy3, iz3)

    ; the distance scales the direction, it does not bend it
    ix4, iy4, iz4 csnhoatocar 4, ipi2, 0
    prints("distance 4, left -> %.4f %.4f %.4f\n", ix4, iy4, iz4)

    ; degrees, which is how a listening position is usually written down
    ix5, iy5, iz5 csnhoatocar 1, 45, 30, 1
    prints("az 45 el 30 deg  -> %.4f %.4f %.4f\n", ix5, iy5, iz5)

    ; and the trip back through csncartohoa
    id, ia, ie csncartohoa ix5, iy5, iz5, 1
    prints("                 -> back to az %.2f el %.2f at %.4f\n", ia, ie, id)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csncartohoa](csncartohoa.md)
* [csnsphtocar](csnsphtocar.md)
* [csnrotmat](csnrotmat.md)

## Credits

Pasquale Mainolfi, 2026
