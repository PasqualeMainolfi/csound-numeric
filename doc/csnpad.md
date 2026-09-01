# csnpad

## Abstract

Add elements before and after an array, filled with a value.

## Description

`csnpad` grows an array by `before` elements at the start and `after` elements at
the end, all set to `value`. The original elements keep their order in the middle.

With no axis argument every axis is padded, so a `2×3` matrix padded by 1 on each
side becomes `4×5`. Given an axis, only that one grows: the same call on axis 0
gives `4×3`.

A complex array must be padded with a `:Complex;` value; a real fill value on a
complex array is refused rather than silently taken as `value + 0i`.

Two forms share the name. The one with an output publishes a new handle and
leaves the source alone; the one without an output rewrites the source in place
and returns nothing.

## Syntax

```csound
handle:CsnArr = csnpad(source:CsnArr, before:i, after:i)
handle:CsnArr = csnpad(source:CsnArr, before:i, after:i, value:i)
handle:CsnArr = csnpad(source:CsnArr, before:i, after:i, value:i, axis:i)
handle:CsnArr = csnpad(source:CsnArr, before:i, after:i, value:Complex)
handle:CsnArr = csnpad(source:CsnArr, before:i, after:i, value:Complex, axis:i)
handle:CsnArr = csnpad(source:CsnArr, before:k, after:k, value:k, trig:k)
handle:CsnArr = csnpad(source:CsnArr, before:k, after:k, value:k, axis:k, trig:k)
handle:CsnArr = csnpad(source:CsnArr, before:k, after:k, value:Complex, trig:k)
handle:CsnArr = csnpad(source:CsnArr, before:k, after:k, value:Complex, axis:k, trig:k)
csnpad(source:CsnArr, before:i, after:i, value:i)
csnpad(source:CsnArr, before:i, after:i, value:i, axis:i)
csnpad(source:CsnArr, before:k, after:k, value:k, trig:k)
csnpad(source:CsnArr, before:k, after:k, value:k, axis:k, trig:k)
```

## Arguments

* `source:CsnArr`: the array to pad.
* `before:i / before:k`: how many elements to add at the start.
* `after:i / after:k`: how many elements to add at the end.
* `value:i / value:k` (optional, default `0`): the fill value.
* `value:Complex`: the fill value for a complex array.
* `axis:i / axis:k` (optional): the axis to pad. Omitted, every axis is padded.
* `trig:k`: k-rate trigger. The padding is recomputed on a non-zero trigger; a zero trigger republishes the previous result.

## Output

* `handle:CsnArr`: handle of the padded array. Omit it for the in-place form.

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
; csnpad.csd
;
; Without an axis every axis grows, which frames a matrix on all four sides.
; With an axis, only that one does.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    vec:CsnArr      = csnfromarray(array(1, 2, 3, 4))
    padded:CsnArr   = csnpad(vec, 1, 2, -1)
    padded_out:i[]  = csntoarray(padded)
    n:i             = csnsize(padded)
    prints("n = %d, values = %g %g %g %g %g %g %g\n", n, padded_out[0], padded_out[1], padded_out[2], padded_out[3], padded_out[4], padded_out[5], padded_out[6])

    shape:i[]       = fillarray(2, 3)
    mat:CsnArr      = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6)), shape)

    frame:CsnArr    = csnpad(mat, 1, 1, 0)
    frame_shape:i[] = csnshape(frame)
    prints("every axis: %g x %g\n", frame_shape[0], frame_shape[1])

    rows:CsnArr     = csnpad(mat, 1, 1, 0, 0)
    rows_shape:i[]  = csnshape(rows)
    prints("axis 0 only: %g x %g\n", rows_shape[0], rows_shape[1])

    ; in place
    csnpad(vec, 0, 1, 9)
    now:i[]         = csntoarray(vec)
    now_n:i         = csnsize(vec)
    prints("in place n = %d, last = %g\n", now_n, now[4])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnresize](csnresize.md)
* [csntruncate](csntruncate.md)
* [csnconcat](csnconcat.md)
* [csninsert](csninsert.md)

## Credits

Pasquale Mainolfi, 2026
