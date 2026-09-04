# csnrandint

## Abstract

Create an array of uniform random integers in a range.

## Description

`csnrandint` allocates a real array of the requested shape and fills it with
integer-valued samples drawn uniformly from `[min, max)`. The lower bound is
included and the upper bound is excluded. Bounds may be negative, but both must
be finite integers in the signed 32-bit range, with `min < max`.

The generator is owned by the Csound instance and shared with [csnrand](csnrand.md)
and [csnshuffle](csnshuffle.md). Use [csnseed](csnseed.md) to make a run
reproducible.

At k-rate, a zero trigger republishes the previous draw unchanged; a non-zero
trigger produces a new array of values.

Real only.

## Syntax

```csound
handle:CsnArr = csnrandint(shape:i[], min:i, max:i)
handle:CsnArr = csnrandint(shape:k[], min:k, max:k)
handle:CsnArr = csnrandint(shape:k[], min:k, max:k, trig:k)
```

## Arguments

* `shape:i[] / shape:k[]`: one extent per dimension.
* `min:i / min:k`: finite integer lower bound, included.
* `max:i / max:k`: finite integer upper bound, excluded.
* `trig:k` (optional, default `1`): k-rate trigger. A non-zero value makes a new draw; zero republishes the previous one.

## Output

* `handle:CsnArr`: handle of the new real array. Its values are integers stored in Csound's numeric element type.

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
; csnrandint.csd
;
; csnrandint draws integer-valued samples from [min, max). Both bounds may be
; negative, and seeding makes the draw reproducible.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    csnseed(12345)

    shape:i[]       = fillarray(8)
    values:CsnArr   = csnrandint(shape, -5, 0)
    rounded:CsnArr  = csnfloor(values)
    difference:CsnArr = csnsubtract(values, rounded)
    all_integer:i   = (csncnteq(difference, 0) == csnsize(values) ? 1 : 0)
    in_range:i      = (csnmin(values) >= -5 && csnmax(values) < 0 ? 1 : 0)

    csnprint(values)
    prints("all integer = %d, all inside [-5, 0) = %d\n", all_integer, in_range)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnrand](csnrand.md)
* [csnshuffle](csnshuffle.md)
* [csnseed](csnseed.md)

## Credits

Pasquale Mainolfi, 2026
