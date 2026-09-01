# csncross

## Abstract

Cross product of two 3-element vectors.

## Description

`csncross` returns the vector perpendicular to both operands, with a length equal
to the area of the parallelogram they span, and a direction given by the
right-hand rule.

Both operands must be 1-D arrays of exactly 3 elements — the cross product is
defined only in three dimensions — and both must be real: this is a real-vector
operation, and a complex operand is refused rather than treated as two real
halves.

The result is zero when the two vectors are parallel, which is the cheap test for
collinearity.

## Syntax

```csound
handle:CsnArr = csncross(a:CsnArr, b:CsnArr)
handle:CsnArr = csncross(a:CsnArr, b:CsnArr, trig:k)
```

## Arguments

* `a:CsnArr`: first vector; 1-D, exactly 3 real elements.
* `b:CsnArr`: second vector; 1-D, exactly 3 real elements.
* `trig:k`: k-rate trigger. A zero trigger republishes the previous result.

## Output

* `handle:CsnArr`: a 3-element vector perpendicular to both operands.

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
; csncross.csd
;
; Perpendicular to both, right-hand rule, length equal to the area they span.
; Zero when the two are parallel.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    x:CsnArr     = csnfromarray(array(1, 0, 0))
    y:CsnArr     = csnfromarray(array(0, 1, 0))

    z:CsnArr     = csncross(x, y)
    z_out:i[]    = csntoarray(z)
    prints("x cross y = %g %g %g\n", z_out[0], z_out[1], z_out[2])

    ; the order matters: swapping them flips the result
    back:CsnArr  = csncross(y, x)
    back_out:i[] = csntoarray(back)
    prints("y cross x = %g %g %g\n", back_out[0], back_out[1], back_out[2])

    ; parallel vectors give zero
    twice:CsnArr = csnmul(x, 3)
    none:CsnArr  = csncross(x, twice)
    none_out:i[] = csntoarray(none)
    prints("parallel  = %g %g %g\n", none_out[0], none_out[1], none_out[2])
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
* [csnangledist](csnangledist.md)
* [csnproject](csnproject.md)

## Credits

Pasquale Mainolfi, 2026
