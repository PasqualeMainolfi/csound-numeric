# csntruncate

## Abstract

Shorten one axis, or every axis, to a length.

## Description

`csntruncate` cuts an array down to `length`, keeping the first elements and
dropping the rest.

With no axis argument the array is read flat and the first `length` elements are
kept. Given an axis, only that axis is shortened and the shape keeps its rank: a
`2×3` matrix truncated to 2 on axis 1 becomes `2×2`.

`length` must not exceed the extent being cut. To grow an array instead, use
[csnresize](csnresize.md), which zero-fills what it adds, or
[csnpad](csnpad.md).

Two forms share the name. The one with an output publishes a new handle and
leaves the source alone; the one without an output rewrites the source in place
and returns nothing.

## Syntax

```csound
handle:CsnArr = csntruncate(source:CsnArr, length:i)
handle:CsnArr = csntruncate(source:CsnArr, length:i, axis:i)
handle:CsnArr = csntruncate(source:CsnArr, length:k, trig:k)
handle:CsnArr = csntruncate(source:CsnArr, length:k, trig:k, axis:k)
csntruncate(source:CsnArr, length:i)
csntruncate(source:CsnArr, length:i, axis:i)
csntruncate(source:CsnArr, length:k, trig:k)
csntruncate(source:CsnArr, length:k, trig:k, axis:k)
```

## Arguments

* `source:CsnArr`: the array to shorten.
* `length:i / length:k`: the number of elements to keep along the axis.
* `axis:i / axis:k` (optional): the axis to cut. Omit it to truncate every axis; `-1` selects the last axis.
* `trig:k` (optional, default `1`): k-rate trigger. A zero trigger republishes the previous result.

## Output

* `handle:CsnArr`: handle of the shortened array. Omit it for the in-place form.

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
; csntruncate.csd
;
; Without an axis every axis is truncated. With an explicit -1 only the last
; axis is cut: a 2 x 3 truncated to 2 becomes a 2 x 2.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    vec:CsnArr    = csnfromarray(array(1, 2, 3, 4, 5, 6))
    short:CsnArr  = csntruncate(vec, 3)
    short_out:i[] = csntoarray(short)
    n:i           = csnsize(short)
    prints("vector n = %d, values = %g %g %g\n", n, short_out[0], short_out[1], short_out[2])

    shape:i[]     = fillarray(2, 3)
    mat:CsnArr    = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6)), shape)

    every:CsnArr  = csntruncate(mat, 1)
    every_shape:i[] = csnshape(every)
    prints("omitted: %g x %g\n", every_shape[0], every_shape[1])

    cut:CsnArr    = csntruncate(mat, 2, -1)
    cut_shape:i[] = csnshape(cut)
    cut_out:i[]   = csntoarray(csnflatten(cut))
    prints("axis -1: %g x %g, values = %g %g %g %g\n", cut_shape[0], cut_shape[1], cut_out[0], cut_out[1], cut_out[2], cut_out[3])

    ; in place
    csntruncate(vec, 2)
    now_n:i       = csnsize(vec)
    now:i[]       = csntoarray(vec)
    prints("in place n = %d, values = %g %g\n", now_n, now[0], now[1])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnhead](csnhead.md)
* [csnresize](csnresize.md)
* [csnpad](csnpad.md)
* [csnremove](csnremove.md)

## Credits

Pasquale Mainolfi, 2026
