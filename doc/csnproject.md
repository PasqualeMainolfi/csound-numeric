# csnproject

## Abstract

Component of one vector along another.

## Description

`csnproject` returns the part of `a` that lies along `b`: the vector
`(a·b / b·b) · b`, which is the shadow `a` casts on the line through `b`.

It is one half of a decomposition — [csnreject](csnreject.md) is the other, the
part of `a` perpendicular to `b` — and adding the two back together gives `a`.

Both operands must have the same shape, and `b` must not be the zero vector:
there is no line to project onto, so the call is refused rather than dividing by
zero.

Real only: a complex operand is refused rather than being treated as two real
halves.

## Syntax

```csound
handle:CsnArr = csnproject(a:CsnArr, b:CsnArr)
handle:CsnArr = csnproject(a:CsnArr, b:CsnArr, trig:k)
```

## Arguments

* `a:CsnArr`: the vector being decomposed.
* `b:CsnArr`: the direction to project onto; same shape as `a`, and not all zero.
* `trig:k`: k-rate trigger. A zero trigger republishes the previous result.

## Output

* `handle:CsnArr`: the component of `a` along `b`.

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
; csnproject.csd
;
; The shadow of a on the line through b. Add csnreject to it and a comes back.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    a:CsnArr      = csnfromarray(array(3, 4, 0))
    b:CsnArr      = csnfromarray(array(1, 0, 0))

    along:CsnArr  = csnproject(a, b)
    along_out:i[] = csntoarray(along)
    prints("along b = %g %g %g\n", along_out[0], along_out[1], along_out[2])

    across:CsnArr = csnreject(a, b)
    across_out:i[] = csntoarray(across)
    prints("across  = %g %g %g\n", across_out[0], across_out[1], across_out[2])

    ; the two halves add back up to a
    back:CsnArr   = csnadd(along, across)
    back_out:i[]  = csntoarray(back)
    prints("sum     = %g %g %g\n", back_out[0], back_out[1], back_out[2])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnreject](csnreject.md)
* [csnreflect](csnreflect.md)
* [csndot](csndot.md)
* [csnnormalize](csnnormalize.md)

## Credits

Pasquale Mainolfi, 2026
