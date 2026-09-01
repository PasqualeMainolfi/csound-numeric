# csnconcat

## Abstract

Join two arrays, flat or along an axis.

## Description

`csnconcat` builds a new array holding the elements of both operands.

With no axis argument both arrays are read flat and the result is a 1-D array of
`sizeA + sizeB` elements, whatever their ranks. Given an axis, the two arrays are
stacked along it and must agree on every *other* axis: two `2×3` matrices
concatenated on axis 0 give a `4×3`.

Concatenating with an empty array yields the other operand, so an accumulator can
start from [csnempty](csnempty.md) without a special first case.

If either operand is complex the result is complex, and the real operand is
promoted.

## Syntax

```csound
handle:CsnArr = csnconcat(a:CsnArr, b:CsnArr)
handle:CsnArr = csnconcat(a:CsnArr, b:CsnArr, axis:i)
handle:CsnArr = csnconcat(a:CsnArr, b:CsnArr, trig:k)
handle:CsnArr = csnconcat(a:CsnArr, b:CsnArr, axis:k, trig:k)
```

## Arguments

* `a:CsnArr`: first array; its elements come first.
* `b:CsnArr`: second array.
* `axis:i / axis:k` (optional): the axis to stack along. Omitted, both arrays are read flat and the result is 1-D.
* `trig:k`: k-rate trigger. The join is recomputed on a non-zero trigger; a zero trigger republishes the previous result.

## Output

* `handle:CsnArr`: handle of the joined array.

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
; csnconcat.csd
;
; Without an axis both operands are read flat, so ranks need not agree. With an
; axis they are stacked, and every other axis must match.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    a:CsnArr        = csnfromarray(array(1, 2, 3))
    b:CsnArr        = csnfromarray(array(4, 5))

    joined:CsnArr   = csnconcat(a, b)
    joined_out:i[]  = csntoarray(joined)
    n:i             = csnsize(joined)
    prints("flat n = %d, values = %g %g %g %g %g\n", n, joined_out[0], joined_out[1], joined_out[2], joined_out[3], joined_out[4])

    ; stacked along an axis
    shape:i[]       = fillarray(2, 3)
    mat:CsnArr      = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6)), shape)
    stacked:CsnArr  = csnconcat(mat, mat, 0)
    stacked_shape:i[] = csnshape(stacked)
    prints("axis 0: %g x %g\n", stacked_shape[0], stacked_shape[1])

    ; concatenating with an empty array gives back the other operand
    cap:i[]         = fillarray(4)
    nothing:CsnArr  = csnempty(cap)
    same:CsnArr     = csnconcat(a, nothing)
    same_n:i        = csnsize(same)
    prints("with an empty operand: n = %d\n", same_n)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csninsert](csninsert.md)
* [csnpad](csnpad.md)
* [csnpush](csnpush.md)
* [csnempty](csnempty.md)

## Credits

Pasquale Mainolfi, 2026
