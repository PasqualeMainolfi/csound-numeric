# csncartopol

## Abstract

Cartesian coordinates to polar, in the plane.

## Description

`csncartopol` answers `r = hypot(x, y)` and `phi = atan2(y, x)` in `(-pi, pi]`.

`atan2` keeps the quadrant, which a plain arctangent of `y/x` would lose. The
origin is not a special case here: `r` is `0` and the angle `0` with it — only
[csncartosph](csncartosph.md), which divides by the magnitude, has to refuse it.

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
r:i, phi:i csncartopol x:i, y:i [, degrees:i]
r:k, phi:k csncartopol x:k, y:k [, degrees:i]
```

## Arguments

* `x:i / x:k`, `y:i / y:k`: the cartesian coordinates.
* `degrees:i` (optional, default `0`): `0` reads and writes the angles in radians, `1` in degrees. It is an i-argument in both forms: it picks how the conversion reads and writes, it is not a value that evolves during the note.

## Output

* `r`: the radius, from `hypot`.
* `phi`: the angle from `+x`, in `(-pi, pi]`.

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
; csncartopol.csd
;
; Cartesian to polar, in the plane: r from hypot, phi from atan2 in (-pi, pi].
; The optional trailing 1 asks for the angle in degrees, so a round trip taken
; in degrees comes back in degrees.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    ir1, ip1 csncartopol 3, 4
    prints("(3, 4)   -> r %.4f phi %.4f\n", ir1, ip1)

    ; atan2 keeps the quadrant, which a plain arctangent would lose
    ir2, ip2 csncartopol -3, 4, 1
    prints("(-3, 4)  -> r %.4f phi %.2f deg\n", ir2, ip2)
    ir3, ip3 csncartopol -3, -4, 1
    prints("(-3, -4) -> r %.4f phi %.2f deg\n", ir3, ip3)

    ; the origin is not an error here: r is 0 and the angle is 0 with it
    ir4, ip4 csncartopol 0, 0
    prints("(0, 0)   -> r %.4f phi %.4f\n", ir4, ip4)

    ; round trip in degrees
    ir, ip csncartopol 1, 1, 1
    ibx, iby csnpoltocar ir, ip, 1
    prints("(1, 1) -> r %.4f phi %.2f -> back to %.4f %.4f\n", ir, ip, ibx, iby)
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
* [csncyltocar](csncyltocar.md)
* [csncartocyl](csncartocyl.md)
* [csnsphtocar](csnsphtocar.md)
* [csncartosph](csncartosph.md)
* [csndegtorad](csndegtorad.md)
* [csnradtodeg](csnradtodeg.md)

## Credits

Pasquale Mainolfi, 2026
