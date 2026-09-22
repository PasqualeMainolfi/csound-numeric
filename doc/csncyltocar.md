# csncyltocar

## Abstract

Cylindrical coordinates to cartesian.

## Description

`csncyltocar` answers `x = r cos(phi)`, `y = r sin(phi)` and passes `z` through
unchanged. It is [csnpoltocar](csnpoltocar.md) with a height, which is the
system a source circling a listener at a fixed elevation lives in.

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
x:i, y:i, z:i csncyltocar r:i, phi:i, h:i [, degrees:i]
x:k, y:k, z:k csncyltocar r:k, phi:k, h:k [, degrees:i]
```

## Arguments

* `r:i / r:k`: the radius in the xy plane.
* `phi:i / phi:k`: the angle from `+x`.
* `h:i / h:k`: the height, carried through untouched.
* `degrees:i` (optional, default `0`): `0` reads and writes the angles in radians, `1` in degrees. It is an i-argument in both forms: it picks how the conversion reads and writes, it is not a value that evolves during the note.

## Output

* `x`, `y`, `z`: the cartesian coordinates.

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
; csncyltocar.csd
;
; Cylindrical to cartesian: (r, phi, z) where r and phi are the polar pair in
; the xy plane and z is carried through untouched. It is the system to reach for
; when a source moves round a listener at a fixed height.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    ; the height passes straight through
    ix1, iy1, iz1 csncyltocar 2, 0, 5
    prints("(r 2, phi 0, z 5)   -> %.4f %.4f %.4f\n", ix1, iy1, iz1)

    ; a quarter turn moves it onto +y, still at the same height
    ix2, iy2, iz2 csncyltocar 2, 90, 5, 1
    prints("(r 2, phi 90deg, z 5) -> %.4f %.4f %.4f\n", ix2, iy2, iz2)

    ; round trip
    ir, ip, ih csncartocyl 3, 4, 7
    prints("cartocyl(3, 4, 7)   -> r %.4f phi %.4f z %.4f\n", ir, ip, ih)
    ibx, iby, ibz csncyltocar ir, ip, ih
    prints("                    -> back to %.4f %.4f %.4f\n", ibx, iby, ibz)

    ; one turn at a fixed height, the path a circular panner traces
    iphi = 0
    while iphi < 360 do
        ipx, ipy, ipz csncyltocar 1, iphi, 1.7, 1
        prints("phi %3.0f deg -> x %.3f y %.3f z %.1f\n", iphi, ipx, ipy, ipz)
        iphi += 90
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

* [csnpoltocar](csnpoltocar.md)
* [csncartopol](csncartopol.md)
* [csncartocyl](csncartocyl.md)
* [csnsphtocar](csnsphtocar.md)
* [csncartosph](csncartosph.md)
* [csndegtorad](csndegtorad.md)
* [csnradtodeg](csnradtodeg.md)

## Credits

Pasquale Mainolfi, 2026
