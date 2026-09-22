# csncartosph

## Abstract

Cartesian coordinates to spherical.

## Description

`csncartosph` answers

```
r     = hypot(hypot(x, y), z)
theta = acos(z / r)
phi   = atan2(y, x)
```

in the order [csnsphtocar](csnsphtocar.md) reads them back. `theta` lands in
`[0, pi]` and `phi` in `(-pi, pi]`.

The magnitude is taken with two nested `hypot` calls rather than the square
root of a sum of squares: the sum overflows for components the magnitude
itself can hold. `z / r` is clamped to `[-1, 1]` before `acos`, since rounding
can push it an ulp outside and `acos` answers NaN there rather than the pole.

The origin is the one refusal: with `r = 0` there is no direction to report,
and any pair of angles would be invented. [csncartocyl](csncartocyl.md) never
divides by the magnitude and so accepts it.

Three systems, each with its own pair of opcodes, so nothing is inferred from
how many arguments were passed:

| system | components | |
| --- | --- | --- |
| polar | `r`, `phi` | planar; `phi` from `+x` |
| cylindrical | `r`, `phi`, `z` | the polar pair with the height carried through |
| spherical | `r`, `theta`, `phi` | `theta` the inclination from `+z`, `phi` the azimuth from `+x` |

The spherical order is ISO 80000-2's, and both directions of the pair use it, so
`csncartosph` feeds straight back into `csnsphtocar` and returns the point it
started from.

The optional trailing flag selects the unit of the angles: `0` radians, the
default, `1` degrees. It applies to whichever side of the conversion carries
them — arguments on the way to cartesian, results on the way back — so a round
trip taken in degrees comes back in degrees.

## Syntax

```csound
r:i, theta:i, phi:i csncartosph x:i, y:i, z:i [, degrees:i]
r:k, theta:k, phi:k csncartosph x:k, y:k, z:k [, degrees:i]
```

## Arguments

* `x:i / x:k`, `y:i / y:k`, `z:i / z:k`: the cartesian coordinates; not all three zero.
* `degrees:i` (optional, default `0`): `0` reads and writes the angles in radians, `1` in degrees. It is an i-argument in both forms: it picks how the conversion reads and writes, it is not a value that evolves during the note.

## Output

* `r`: the magnitude.
* `theta`: the inclination from `+z`, in `[0, pi]`.
* `phi`: the azimuth from `+x`, in `(-pi, pi]`.

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
; csncartosph.csd
;
; Cartesian to spherical, answering (r, theta, phi) in the order csnsphtocar
; reads them back: theta the inclination from +z in [0, pi], phi the azimuth
; from +x in (-pi, pi]. The origin has no direction, so r = 0 is an error rather
; than an arbitrary pair of angles.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    ; a point on each axis
    ir1, it1, ip1 csncartosph 1, 0, 0
    prints("(1, 0, 0) -> r %.4f theta %.4f phi %.4f\n", ir1, it1, ip1)
    ir2, it2, ip2 csncartosph 0, 0, 1
    prints("(0, 0, 1) -> r %.4f theta %.4f phi %.4f\n", ir2, it2, ip2)
    ir3, it3, ip3 csncartosph 0, 0, -1
    prints("(0, 0,-1) -> r %.4f theta %.4f phi %.4f\n", ir3, it3, ip3)

    ; an arbitrary point, and the trip back
    ir, it, ip csncartosph 1, 2, 3
    prints("(1, 2, 3) -> r %.4f theta %.4f phi %.4f\n", ir, it, ip)
    ibx, iby, ibz csnsphtocar ir, it, ip
    prints("          -> back to %.4f %.4f %.4f\n", ibx, iby, ibz)

    ; the same in degrees, which reads better for a listening position
    idr, idt, idp csncartosph 1, 2, 3, 1
    prints("in degrees: r %.4f theta %.2f phi %.2f\n", idr, idt, idp)

    ; a magnitude no sum of squares could hold: hypot carries it
    ibr, ibt, ibp csncartosph 1e200, 1e200, 0
    prints("(1e200, 1e200, 0) -> r %g\n", ibr)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnpoltocar](csnpoltocar.md)
* [csncartopol](csncartopol.md)
* [csncyltocar](csncyltocar.md)
* [csncartocyl](csncartocyl.md)
* [csnsphtocar](csnsphtocar.md)
* [csndegtorad](csndegtorad.md)
* [csnradtodeg](csnradtodeg.md)

## Credits

Pasquale Mainolfi, 2026
