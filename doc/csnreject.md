# csnreject

## Abstract

Component of one vector orthogonal to another.

## Description

`csnreject` returns the part of `a` perpendicular to `b`: `a` minus its
projection onto `b`. It is the other half of the decomposition
[csnproject](csnproject.md) starts, and the two add back up to `a`.

The result is orthogonal to `b` by construction, so its scalar product with `b`
is zero. When `a` and `b` are parallel the rejection is the zero vector.

Both operands must have the same shape, and `b` must not be the zero vector.

Real only: a complex operand is refused rather than being treated as two real
halves.

## Syntax

```csound
handle:CsnArr = csnreject(a:CsnArr, b:CsnArr)
handle:CsnArr = csnreject(a:CsnArr, b:CsnArr, trig:k)
```

## Arguments

* `a:CsnArr`: the vector being decomposed.
* `b:CsnArr`: the direction to reject; same shape as `a`, and not all zero.
* `trig:k`: k-rate trigger. A zero trigger republishes the previous result.

## Output

* `handle:CsnArr`: the component of `a` orthogonal to `b`.

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
; csnreject.csd
;
; What is left of a once its component along b is taken out. Orthogonal to b by
; construction, so the scalar product with b is zero.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    a:CsnArr       = csnfromarray(array(3, 4, 0))
    b:CsnArr       = csnfromarray(array(1, 0, 0))

    across:CsnArr  = csnreject(a, b)
    across_out:i[] = csntoarray(across)
    prints("rejection = %g %g %g\n", across_out[0], across_out[1], across_out[2])

    ; orthogonal by construction
    check:i        = csndot(across, b)
    prints("dot with b = %g\n", check)

    ; parallel vectors leave nothing behind
    twice:CsnArr   = csnmul(b, 5)
    none:CsnArr    = csnreject(twice, b)
    none_out:i[]   = csntoarray(none)
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

* [csnproject](csnproject.md)
* [csnreflect](csnreflect.md)
* [csndot](csndot.md)

## Credits

Pasquale Mainolfi, 2026
