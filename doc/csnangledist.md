# csnangledist

## Abstract

Angle between two vectors.

## Description

`csnangledist` returns the angle between two vectors, in radians, from the
arccosine of their normalised scalar product. The answer runs from `0` for two
vectors pointing the same way to `pi` for two pointing opposite ways, with
`pi/2` for perpendicular ones.

Unlike [csndist](csndist.md) it ignores the lengths entirely: scaling either
operand leaves the answer unchanged. That is what makes it the similarity
measure to use when only direction matters.

Both real and complex arrays are accepted. Use
[csnradtodeg](csnradtodeg.md) to read the answer in degrees.

## Syntax

```csound
value:i = csnangledist(a:CsnArr, b:CsnArr)
value:k = csnangledist(a:CsnArr, b:CsnArr, trig:k)
```

## Arguments

* `a:CsnArr`: first vector.
* `b:CsnArr`: second vector.
* `trig:k`: k-rate trigger. A zero trigger republishes the previous result.

## Output

* `value:i / value:k`: the angle in radians, from `0` to `pi`.

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
; csnangledist.csd
;
; Direction only: scaling either operand leaves the answer alone, which is what
; separates it from csndist.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    x:CsnArr      = csnfromarray(array(1, 0, 0))
    y:CsnArr      = csnfromarray(array(0, 1, 0))
    diag:CsnArr   = csnfromarray(array(1, 1, 0))

    perp:i        = csnangledist(x, y)
    half:i        = csnangledist(x, diag)
    same:i        = csnangledist(x, x)
    prints("perpendicular = %.4f, 45 degrees = %.4f, identical = %g\n", perp, half, same)

    ; in degrees
    ang:CsnArr    = csnfromarray(array(perp, half, same))
    deg:CsnArr    = csnradtodeg(ang)
    deg_out:i[]   = csntoarray(deg)
    prints("degrees = %.1f %.1f %.1f\n", deg_out[0], deg_out[1], deg_out[2])

    ; length does not matter
    long_x:CsnArr = csnmul(x, 100)
    scaled:i      = csnangledist(long_x, diag)
    prints("after scaling one operand = %.4f\n", scaled)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csndot](csndot.md)
* [csndist](csndist.md)
* [csnnormalize](csnnormalize.md)
* [csnradtodeg](csnradtodeg.md)

## Credits

Pasquale Mainolfi, 2026
