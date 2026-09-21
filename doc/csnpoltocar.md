# csnpoltocar

## Abstract

Polar coordinates to cartesian, in the plane.

## Description

`csnpoltocar` answers `x = r cos(phi)` and `y = r sin(phi)`.

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
x:i, y:i csnpoltocar r:i, phi:i [, degrees:i]
x:k, y:k csnpoltocar r:k, phi:k [, degrees:k]
```

## Arguments

* `r:i / r:k`: the radius.
* `phi:i / phi:k`: the angle from `+x`.
* `degrees:i / degrees:k` (optional, default `0`): `0` reads and writes the angles in radians, `1` in degrees.

## Output

* `x`, `y`: the cartesian coordinates.

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
; csnpoltocar.csd
;
; Polar to cartesian, in the plane: (r, phi) with phi measured from +x. For a
; third component use csncyltocar, which carries a height through, or
; csnsphtocar, which takes two angles.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    ix1, iy1 csnpoltocar 2, 0
    prints("(r 2, phi 0)      -> %.4f %.4f\n", ix1, iy1)

    ; the trailing 1 says the angle is in degrees
    ix2, iy2 csnpoltocar 1, 90, 1
    prints("(r 1, phi 90 deg) -> %.4f %.4f\n", ix2, iy2)

    ; round trip
    ir, ip csncartopol 3, 4
    prints("cartopol(3, 4)    -> r %.4f phi %.4f\n", ir, ip)
    ibx, iby csnpoltocar ir, ip
    prints("                  -> back to %.4f %.4f\n", ibx, iby)

    ; a stereo pair placed by angle rather than by gain
    iangle = -45
    while iangle <= 45 do
        ilx, ily csnpoltocar 1, iangle, 1
        prints("%4.0f deg -> x %+.3f y %.3f\n", iangle, ilx, ily)
        iangle += 45
    od
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csncartopol](csncartopol.md)
* [csncyltocar](csncyltocar.md)
* [csncartocyl](csncartocyl.md)
* [csnsphtocar](csnsphtocar.md)
* [csncartosph](csncartosph.md)
* [csndegtorad](csndegtorad.md)
* [csnradtodeg](csnradtodeg.md)

## Credits

Pasquale Mainolfi, 2026
