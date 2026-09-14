# csnflip

## Abstract

Reverse the order of the elements along an axis.

## Description

`csnflip` reverses an array along one or more axes: with `axis = 0` the rows of a matrix
come back bottom to top, with `axis = 1` each row is read right to left. The
omitted axis reverses every axis. An explicit `-1` selects only the last axis.

Two forms share the name. The one with an output publishes a new handle and
leaves the source alone; the one without an output rewrites the source in place
and returns nothing.

[csnreverse](csnreverse.md) is the flat reversal said directly, with no axis
argument to give.

## Syntax

```csound
handle:CsnArr = csnflip(source:CsnArr)
handle:CsnArr = csnflip(source:CsnArr, axis:i)
handle:CsnArr = csnflip(source:CsnArr, trig:k)
handle:CsnArr = csnflip(source:CsnArr, trig:k, axis:k)
csnflip(source:CsnArr)
csnflip(source:CsnArr, axis:i)
csnflip(source:CsnArr, trig:k)
csnflip(source:CsnArr, trig:k, axis:k)
```

## Arguments

* `source:CsnArr`: the array to reverse.
* `axis:i / axis:k` (optional): the axis to reverse along. Omit it to reverse every axis; explicit negative values count from the end, so `-1` is the last axis.
* `trig:k` (optional, default `1`): k-rate trigger, placed before an explicit axis.

## Output

* `handle:CsnArr`: handle of the reversed array. Omit it for the in-place form.

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
; csnflip.csd
;
; Omitting the axis reverses every axis. An explicit -1 reverses only the last
; axis, while -2 selects the penultimate one.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    shape:i[]       = fillarray(2, 3)
    mat:CsnArr      = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6)), shape)

    every:CsnArr     = csnflip(mat)
    every_out:i[]    = csntoarray(csnflatten(every))
    prints("omitted   = %g %g %g %g %g %g\n", every_out[0], every_out[1], every_out[2], every_out[3], every_out[4], every_out[5])

    by_cols:CsnArr  = csnflip(mat, -1)
    by_cols_out:i[] = csntoarray(csnflatten(by_cols))
    prints("axis -1   = %g %g %g %g %g %g\n", by_cols_out[0], by_cols_out[1], by_cols_out[2], by_cols_out[3], by_cols_out[4], by_cols_out[5])

    by_rows:CsnArr  = csnflip(mat, -2)
    by_rows_out:i[] = csntoarray(csnflatten(by_rows))
    prints("axis -2   = %g %g %g %g %g %g\n", by_rows_out[0], by_rows_out[1], by_rows_out[2], by_rows_out[3], by_rows_out[4], by_rows_out[5])

    ; in place
    vec:CsnArr      = csnfromarray(array(1, 2, 3, 4))
    csnflip(vec)
    now:i[]         = csntoarray(vec)
    prints("in place  = %g %g %g %g\n", now[0], now[1], now[2], now[3])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnreverse](csnreverse.md)
* [csnroll](csnroll.md)
* [csntranspose](csntranspose.md)

## Credits

Pasquale Mainolfi, 2026
