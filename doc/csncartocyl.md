# csncartocyl

## Abstract

Cartesian coordinates to cylindrical.

## Description

`csncartocyl` answers `r = hypot(x, y)`, `phi = atan2(y, x)` and passes `z`
through unchanged.

Nothing here divides by the magnitude, so a point on the axis is ordinary: `r`
is `0`, the angle `0`, and the height whatever it was.

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
r:i, phi:i, h:i csncartocyl x:i, y:i, z:i [, degrees:i]
r:k, phi:k, h:k csncartocyl x:k, y:k, z:k [, degrees:k]
```

## Arguments

* `x:i / x:k`, `y:i / y:k`, `z:i / z:k`: the cartesian coordinates.
* `degrees:i / degrees:k` (optional, default `0`): `0` reads and writes the angles in radians, `1` in degrees.

## Output

* `r`: the radius in the xy plane.
* `phi`: the angle from `+x`, in `(-pi, pi]`.
* `h`: the height, unchanged.

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
; csncartocyl.csd
;
; Cartesian to cylindrical: (r, phi, z), the polar pair in the xy plane with the
; height carried through untouched. Unlike csncartosph it never has to divide by
; the magnitude, so the origin is not a special case.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    ir1, ip1, iz1 csncartocyl 3, 4, 7
    prints("(3, 4, 7)  -> r %.4f phi %.4f z %.4f\n", ir1, ip1, iz1)

    ; the height is the one component that passes through unchanged
    ir2, ip2, iz2 csncartocyl 3, 4, -2
    prints("(3, 4, -2) -> r %.4f phi %.4f z %.4f\n", ir2, ip2, iz2)

    ; on the axis the radius vanishes but the height does not, and nothing
    ; is undefined: csncartosph would refuse only the origin itself
    ir3, ip3, iz3 csncartocyl 0, 0, 5
    prints("(0, 0, 5)  -> r %.4f phi %.4f z %.4f\n", ir3, ip3, iz3)

    ; in degrees, and back
    ir, ip, iz csncartocyl 1, 1, 3, 1
    ibx, iby, ibz csncyltocar ir, ip, iz, 1
    prints("(1, 1, 3) -> phi %.2f deg -> back to %.4f %.4f %.4f\n", ip, ibx, iby, ibz)
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
* [csnsphtocar](csnsphtocar.md)
* [csncartosph](csncartosph.md)
* [csndegtorad](csndegtorad.md)
* [csnradtodeg](csnradtodeg.md)

## Credits

Pasquale Mainolfi, 2026
