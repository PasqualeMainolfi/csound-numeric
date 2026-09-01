# csnflip

## Abstract

Reverse the order of the elements along an axis.

## Description

`csnflip` reverses an array along one axis: with `axis = 0` the rows of a matrix
come back bottom to top, with `axis = 1` each row is read right to left. The
default `axis = -1` means "the whole array, read flat", which for a vector is
simply the reversal.

Two forms share the name. The one with an output publishes a new handle and
leaves the source alone; the one without an output rewrites the source in place
and returns nothing.

[csnreverse](csnreverse.md) is the flat reversal said directly, with no axis
argument to give.

## Syntax

```csound
handle:CsnArr = csnflip(source:CsnArr)
handle:CsnArr = csnflip(source:CsnArr, axis:i)
handle:CsnArr = csnflip(source:CsnArr, axis:k)
csnflip(source:CsnArr)
csnflip(source:CsnArr, axis:i)
csnflip(source:CsnArr, axis:k)
```

## Arguments

* `source:CsnArr`: the array to reverse.
* `axis:i / axis:k` (optional, default `-1`): the axis to reverse along; `-1` reads the array flat. Valid axes run from `0` to `csndims - 1`.

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
; The axis decides what gets reversed: 0 turns a matrix upside down, 1 mirrors
; each row, -1 (the default) reverses the flat order.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    vec:CsnArr      = csnfromarray(array(1, 2, 3, 4))
    flat:CsnArr     = csnflip(vec)
    flat_out:i[]    = csntoarray(flat)
    prints("flat      = %g %g %g %g\n", flat_out[0], flat_out[1], flat_out[2], flat_out[3])

    shape:i[]       = fillarray(2, 3)
    mat:CsnArr      = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6)), shape)

    by_rows:CsnArr  = csnflip(mat, 0)
    by_rows_out:i[] = csntoarray(csnflatten(by_rows))
    prints("axis 0    = %g %g %g %g %g %g\n", by_rows_out[0], by_rows_out[1], by_rows_out[2], by_rows_out[3], by_rows_out[4], by_rows_out[5])

    by_cols:CsnArr  = csnflip(mat, 1)
    by_cols_out:i[] = csntoarray(csnflatten(by_cols))
    prints("axis 1    = %g %g %g %g %g %g\n", by_cols_out[0], by_cols_out[1], by_cols_out[2], by_cols_out[3], by_cols_out[4], by_cols_out[5])

    ; in place
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
