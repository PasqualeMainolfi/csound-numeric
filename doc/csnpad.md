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

A `:CsnStr;` is padded with a string, which has no default:
`csnpad(labels, 1, 1, "")` pads with empty strings. Every cell of the result
owns its string, the fill value included.

Two forms share the name. The one with an output writes a new array and
leaves the source alone; the one without an output rewrites the source in place
and returns nothing.

At k-rate the output is padded at init only when the widths, the value and the
axis are known there: constants or i variables. With a k width the init pads
nothing, the output keeps the source's layout, reserved but empty, and the first
triggered pass pads; the width's init value (set with `init`, since `=` still
reads 0 at init) only sizes the room the output is created with, so passes that
keep those widths need no new storage and the pad can run on a
[real-time path](csnrtlock.md). Widths that
change during performance reshape the output within the room it was created
with, twice its initial element count; only a shape that needs more than that
is refused on a marked output.
The in-place form rewrites its source within the source's capacity and only
reallocates when the padded array outgrows it, which a marked source refuses
too.

A string array writes its strings into the room its slots already hold. With
an output, the k init gives every slot room for the fill and for the longest
string of the source, so on a marked output the k form runs as long as the
strings fit that room and stops with a performance error at the first that
does not. In place the padded image is built in a working copy whose slots
then trade places with the source's, so the string k form in place is refused
on a marked array. A pass that runs with an unchanged fill value and widths,
from an unchanged source, is reused and writes nothing.

## Syntax

```csound
out:CsnArr = csnpad(source:CsnArr, before:i, after:i)
out:CsnArr = csnpad(source:CsnArr, before:i, after:i, value:i)
out:CsnArr = csnpad(source:CsnArr, before:i, after:i, value:i, axis:i)
out:CsnArr = csnpad(source:CsnArr, before:i, after:i, value:Complex)
out:CsnArr = csnpad(source:CsnArr, before:i, after:i, value:Complex, axis:i)
out:CsnArr = csnpad(source:CsnArr, before:k, after:k, value:k, trig:k)
out:CsnArr = csnpad(source:CsnArr, before:k, after:k, value:k, trig:k, axis:k)
out:CsnArr = csnpad(source:CsnArr, before:k, after:k, value:Complex, trig:k)
out:CsnArr = csnpad(source:CsnArr, before:k, after:k, value:Complex, trig:k, axis:k)
csnpad(source:CsnArr, before:i, after:i, value:i)
csnpad(source:CsnArr, before:i, after:i, value:i, axis:i)
csnpad(source:CsnArr, before:k, after:k, value:k, trig:k)
csnpad(source:CsnArr, before:k, after:k, value:k, trig:k, axis:k)
out:CsnStr = csnpad(source:CsnStr, before:i, after:i, value:S)
out:CsnStr = csnpad(source:CsnStr, before:i, after:i, value:S, axis:i)
out:CsnStr = csnpad(source:CsnStr, before:k, after:k, value:S, trig:k)
out:CsnStr = csnpad(source:CsnStr, before:k, after:k, value:S, trig:k, axis:k)
csnpad(source:CsnStr, before:i, after:i, value:S)
csnpad(source:CsnStr, before:i, after:i, value:S, axis:i)
csnpad(source:CsnStr, before:k, after:k, value:S, trig:k)
csnpad(source:CsnStr, before:k, after:k, value:S, trig:k, axis:k)
```

## Arguments

* `source:CsnArr / source:CsnStr`: the array to pad.
* `before:i / before:k`: how many elements to add at the start.
* `after:i / after:k`: how many elements to add at the end.
* `value:i / value:k` (optional, default `0`): the fill value.
* `value:Complex`: the fill value for a complex array.
* `value:S`: the fill value for a string array; required.
* `axis:i / axis:k` (optional): the axis to pad. Omitted, every axis is padded; `-1` selects the last axis.
* `trig:k`: k-rate trigger. The padding is recomputed on a non-zero trigger; a zero trigger republishes the previous result.

## Output

* `out:CsnArr / out:CsnStr`: the padded array. Omit it for the in-place form.

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

    rows:CsnArr     = csnpad(mat, 1, 1, 0, -2)
    rows_shape:i[]  = csnshape(rows)
    prints("axis -2 only: %g x %g\n", rows_shape[0], rows_shape[1])

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
