# csnatan2

## Abstract

Two-argument arctangent, elementwise.

## Description

`csnatan2` returns the angle of the point `(x, y)` measured from the positive x
axis, in radians, in the range `(-pi, pi]`. The argument order follows numpy's
`arctan2`: the first operand is `y`, the second is `x`.

Unlike `atan(y/x)` it uses the sign of both operands to place the angle in the
right quadrant, and it is defined when either operand is zero: `atan2(1, 0)` is
`pi/2` and `atan2(0, 0)` is `0`.

The two operands broadcast against each other, and either one may be a scalar.
Since the operation is not commutative both scalar orders exist: `csnatan2(y, 2)`
and `csnatan2(2, y)` are different angles.

Real only — for the phase of a complex array use [csnangle](csnangle.md).

## Syntax

```csound
handle:CsnArr = csnatan2(y:CsnArr, x:CsnArr)
handle:CsnArr = csnatan2(y:CsnArr, x:CsnArr, trig:k)
handle:CsnArr = csnatan2(y:CsnArr, x:i)
handle:CsnArr = csnatan2(y:CsnArr, x:k, trig:k)
handle:CsnArr = csnatan2(y:i, x:CsnArr)
handle:CsnArr = csnatan2(y:k, x:CsnArr, trig:k)
```

## Arguments

* `y:CsnArr / y:i / y:k`: the ordinate.
* `x:CsnArr / x:i / x:k`: the abscissa; broadcast against `y`.
* `trig:k` (optional, default `1`): k-rate trigger. A zero trigger republishes the previous result.

## Output

* `handle:CsnArr`: handle of the angles, in radians.

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
; csnatan2.csd
;
; Angles of four points, then the two scalar orders, which are not the same
; angle: atan2(y, 2) is measured from the x axis, atan2(2, y) from the y axis.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    y:CsnArr      = csnfromarray(array(1, 1, -1, 0))
    x:CsnArr      = csnfromarray(array(1, -1, 1, 1))

    ang:CsnArr    = csnatan2(y, x)
    ang_out:i[]   = csntoarray(ang)
    prints("angles = %.4f %.4f %.4f %.4f\n", ang_out[0], ang_out[1], ang_out[2], ang_out[3])

    hs:CsnArr     = csnatan2(y, 2)
    hs_out:i[]    = csntoarray(hs)
    sh:CsnArr     = csnatan2(2, y)
    sh_out:i[]    = csntoarray(sh)
    prints("y,2    = %.4f      2,y = %.4f\n", hs_out[0], sh_out[0])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnangle](csnangle.md)
* [csnhypot](csnhypot.md)
* [csnatan](csnatan.md)
* [csnunwrap](csnunwrap.md)

## Credits

Pasquale Mainolfi, 2026
