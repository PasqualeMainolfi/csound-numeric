# csnreflect

## Abstract

Reflect a vector about another.

## Description

`csnreflect` returns `a - 2 · project(a, b)`: the vector `a` mirrored through the
hyperplane perpendicular to `b`. The component along `b` changes sign and the
component across it is kept, so reflecting twice gives `a` back.

Written the other way round, `csnreflect(a, b)` is
`csnsubtract(csnreject(a, b), csnproject(a, b))` — the same decomposition
[csnproject](csnproject.md) and [csnreject](csnreject.md) produce, with one half
negated.

Both operands must have the same shape, and `b` must not be the zero vector.
Both real and complex arrays are accepted.

## Syntax

```csound
handle:CsnArr = csnreflect(a:CsnArr, b:CsnArr)
handle:CsnArr = csnreflect(a:CsnArr, b:CsnArr, trig:k)
```

## Arguments

* `a:CsnArr`: the vector to reflect.
* `b:CsnArr`: the normal of the mirror; same shape as `a`, and not all zero.
* `trig:k`: k-rate trigger. A zero trigger republishes the previous result.

## Output

* `handle:CsnArr`: the reflected vector.

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
; csnreflect.csd
;
; The component along b flips sign, the rest is kept. Reflecting twice gives the
; original back.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    a:CsnArr       = csnfromarray(array(3, 4, 0))
    b:CsnArr       = csnfromarray(array(1, 0, 0))

    mirror:CsnArr  = csnreflect(a, b)
    mirror_out:i[] = csntoarray(mirror)
    prints("reflected = %g %g %g\n", mirror_out[0], mirror_out[1], mirror_out[2])

    ; twice is the identity
    back:CsnArr    = csnreflect(mirror, b)
    back_out:i[]   = csntoarray(back)
    prints("twice     = %g %g %g\n", back_out[0], back_out[1], back_out[2])

    ; the length is preserved
    len_a:i        = csnnorm(a, 2)
    len_m:i        = csnnorm(mirror, 2)
    prints("lengths: %g and %g\n", len_a, len_m)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnproject](csnproject.md)
* [csnreject](csnreject.md)
* [csnnorm](csnnorm.md)

## Credits

Pasquale Mainolfi, 2026
