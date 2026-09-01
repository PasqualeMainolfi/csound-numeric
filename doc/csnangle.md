# csnangle

## Abstract

Argument of each element of a complex array.

## Description

`csnangle` returns the phase of every element — the angle its vector makes with
the positive real axis — in radians, in the range `[-pi, pi]`. The result is a
real array of the same shape.

Together with [csnabs](csnabs.md) it converts a complex array from Cartesian to
polar form: magnitude and phase. Over a spectrum that is the phase curve, and
[csnunwrap](csnunwrap.md) is what makes it continuous enough to differentiate.

**Complex only.** A real array has no phase to report, so passing one is refused.

## Syntax

```csound
handle:CsnArr = csnangle(source:CsnArr)
handle:CsnArr = csnangle(source:CsnArr, trig:k)
```

## Arguments

* `source:CsnArr`: a complex array.
* `trig:k`: k-rate trigger. A zero trigger republishes the previous result.

## Output

* `handle:CsnArr`: a real array of phases, in radians, in `[-pi, pi]`.

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
; csnangle.csd
;
; The phase of each element, in [-pi, pi]. With csnabs it is the polar form of
; a complex array.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    ; 1, i, -1, -i
    re:CsnArr    = csnfromarray(array(1, 0, -1, 0))
    im:CsnArr    = csnfromarray(array(0, 1, 0, -1))
    j:Complex    = init(0, 1, 0)
    z:CsnArr     = csnadd(csntocomplex(re), csnmul(csntocomplex(im), j))

    phase:CsnArr = csnangle(z)
    phase_out:i[] = csntoarray(phase)
    prints("radians = %.4f %.4f %.4f %.4f\n", phase_out[0], phase_out[1], phase_out[2], phase_out[3])

    deg:CsnArr   = csnradtodeg(phase)
    deg_out:i[]  = csntoarray(deg)
    prints("degrees = %.1f %.1f %.1f %.1f\n", deg_out[0], deg_out[1], deg_out[2], deg_out[3])

    ; conjugation mirrors the phase
    conj_phase:CsnArr = csnangle(csnconj(z))
    conj_out:i[] = csntoarray(conj_phase)
    prints("conjugate = %.4f %.4f %.4f %.4f\n", conj_out[0], conj_out[1], conj_out[2], conj_out[3])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnabs](csnabs.md)
* [csnconj](csnconj.md)
* [csnunwrap](csnunwrap.md)
* [csnradtodeg](csnradtodeg.md)

## Credits

Pasquale Mainolfi, 2026
