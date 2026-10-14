# csntake

## Abstract

Pick one index along an axis, dropping that axis; or one element by flat index.

## Description

`csntake` has two forms.

* **Along an axis.** `csntake(array, axis, index)` selects one position along
  `axis` and returns everything at it, with that axis **dropped**: taking row 1
  of a `3×4` matrix on axis 0 gives a 4-element vector, not a `1×4`. That is what
  distinguishes it from [csngetslice](csngetslice.md), which keeps the rank. The
  source must be 2-D or higher.
* **By flat index.** `csntake(array, index)` reads the array flat and returns the
  single element at that position, as a number or, for a complex array, as a
  `:Complex;`.

On a `:CsnStr;` the axis form returns a `:CsnStr;` holding its own copies of
the strings, and the flat form returns one element as a copy in an `S`
variable; a slot that was never written reads as `""`. The k axis form copies
strings, which allocates, so it is refused on a [real-time path](csnrtlock.md);
the flat form writes only its `S` output, as [csnget](csnget.md) does. See
[string arrays](string-arrays.md).

## Syntax

```csound
out:CsnArr = csntake(source:CsnArr, axis:i, index:i)
out:CsnArr = csntake(source:CsnArr, axis:k, index:k)
value:i = csntake(source:CsnArr, index:i)
value:Complex = csntake(source:CsnArr, index:i)
value:k = csntake(source:CsnArr, index:k)
value:Complex = csntake(source:CsnArr, index:k)
out:CsnStr = csntake(source:CsnStr, axis:i, index:i)
out:CsnStr = csntake(source:CsnStr, axis:k, index:k)
value:S = csntake(source:CsnStr, index:i)
value:S = csntake(source:CsnStr, index:k)
```

## Arguments

* `source:CsnArr / source:CsnStr`: the array to read.
* `axis:i / axis:k`: the axis to select along, from `-csndims` to `csndims - 1`; negative values count from the end. Only in the three-argument form, which needs a 2-D or higher array.
* `index:i / index:k`: the position along `axis`, or the flat index in the two-argument form.

## Output

* `out:CsnArr / out:CsnStr`: the selection, with `axis` dropped, in the three-argument form.
* `value:i / value:k / value:Complex / value:S`: the single element, in the two-argument form; a copy of the string for a string array.

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
; csntake.csd
;
; Three arguments drop the axis, so a row of a matrix comes back as a vector.
; Two arguments read the array flat and hand back one number.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    shape:i[]     = fillarray(3, 4)
    mat:CsnArr    = csnreshape(csnarange(0, 12, 1), shape)

    ; row 1: the axis disappears
    row:CsnArr    = csntake(mat, 0, 1)
    row_dims:i    = csndims(row)
    row_out:i[]   = csntoarray(row)
    prints("row 1 (dims %d) = %g %g %g %g\n", row_dims, row_out[0], row_out[1], row_out[2], row_out[3])

    ; column 2, likewise
    col:CsnArr    = csntake(mat, 1, 2)
    col_out:i[]   = csntoarray(col)
    prints("col 2           = %g %g %g\n", col_out[0], col_out[1], col_out[2])

    ; two arguments: one element, read flat
    flat:i        = csntake(mat, 5)
    prints("flat index 5    = %g\n", flat)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csngetslice](csngetslice.md)
* [csnget](csnget.md)
* [csnremove](csnremove.md)

## Credits

Pasquale Mainolfi, 2026
