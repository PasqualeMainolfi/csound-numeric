# csnsphtocar

## Abstract

Spherical coordinates to cartesian.

## Description

`csnsphtocar` answers

```
x = r sin(theta) cos(phi)
y = r sin(theta) sin(phi)
z = r cos(theta)
```

`theta` is the inclination from `+z`, so `theta = 0` is the north pole and
`theta = pi/2` the equator — not an elevation measured from the horizontal.

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
x:i, y:i, z:i csnsphtocar r:i, theta:i, phi:i [, degrees:i]
x:k, y:k, z:k csnsphtocar r:k, theta:k, phi:k [, degrees:k]
```

## Arguments

* `r:i / r:k`: the radius.
* `theta:i / theta:k`: the inclination from `+z`, in `[0, pi]`.
* `phi:i / phi:k`: the azimuth from `+x`.
* `degrees:i / degrees:k` (optional, default `0`): `0` reads and writes the angles in radians, `1` in degrees.

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
; csnsphtocar.csd
;
; Spherical to cartesian. The order is (r, theta, phi): theta is the inclination
; measured from +z, phi the azimuth measured from +x in the xy plane. That is the
; ISO 80000-2 order, and it is the order csncartosph answers in, so feeding one
; into the other returns the point you started from.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    iHalfPi = 1.5707963267948966

    ; the three unit directions
    ix1, iy1, iz1 csnsphtocar 1, iHalfPi, 0
    prints("r 1, theta pi/2, phi 0    -> %.4f %.4f %.4f\n", ix1, iy1, iz1)
    ix2, iy2, iz2 csnsphtocar 1, iHalfPi, iHalfPi
    prints("r 1, theta pi/2, phi pi/2 -> %.4f %.4f %.4f\n", ix2, iy2, iz2)

    ; theta = 0 is the north pole, not the equator
    ix3, iy3, iz3 csnsphtocar 1, 0, 0
    prints("r 1, theta 0              -> %.4f %.4f %.4f\n", ix3, iy3, iz3)

    ; the same three in degrees, which is what the trailing 1 selects
    ix4, iy4, iz4 csnsphtocar 1, 90, 45, 1
    prints("r 1, theta 90, phi 45 deg -> %.4f %.4f %.4f\n", ix4, iy4, iz4)

    ; round trip: csncartosph answers (r, theta, phi) in that same order
    ir, it, ip csncartosph 1, 2, 3
    ibx, iby, ibz csnsphtocar ir, it, ip
    prints("(1, 2, 3) round trip      -> %.4f %.4f %.4f\n", ibx, iby, ibz)

    ; a source on a circle of constant elevation, as a panner would sweep it
    iphi = 0
    while iphi < 6.28 do
        ipx, ipy, ipz csnsphtocar 1, iHalfPi, iphi
        prints("phi %.2f -> x %.3f y %.3f\n", iphi, ipx, ipy)
        iphi += 1.57
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
* [csncyltocar](csncyltocar.md)
* [csncartocyl](csncartocyl.md)
* [csncartosph](csncartosph.md)
* [csndegtorad](csndegtorad.md)
* [csnradtodeg](csnradtodeg.md)

## Credits

Pasquale Mainolfi, 2026
