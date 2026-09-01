# csnsetslice

## Abstract

Write an array into a strided slice along an axis.

## Description

`csnsetslice` is the writing counterpart of [csngetslice](csngetslice.md): it
takes the range along `axis` that runs from `start` to `stop` by `step`, and
fills it with the elements of `data`. The destination array is modified in place
and nothing is published.

`data` must have the same rank as the destination and the shape of the slice
itself. Writing one column of a `3×2` matrix therefore takes a `3×1` block, not a
3-element vector; a mismatch is reported with both shapes rather than being
padded or truncated.

The same bounds rules as `csngetslice` apply — non-negative `start` and `stop`
with `start < stop <= extent`, and `step > 0` — and an empty destination is
refused.

## Syntax

```csound
csnsetslice(dest:CsnArr, data:CsnArr, axis:i, start:i, stop:i, step:i)
csnsetslice(dest:CsnArr, data:CsnArr, axis:k, start:k, stop:k, step:k)
```

## Arguments

* `dest:CsnArr`: the array written into.
* `data:CsnArr`: the values to write. Same rank as `dest`, and shaped like the slice.
* `axis:i / axis:k`: the axis to slice along.
* `start:i / start:k`: first index, included.
* `stop:i / stop:k`: last index, excluded.
* `step:i / step:k`: stride; must be > 0.

## Output

None. The destination array is modified.

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
; csnsetslice.csd
;
; csnsetslice fills exactly the range csngetslice would read. Interleaving two
; signals is the same call twice, once on each phase.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    shape:i[]   = fillarray(6)
    dest:CsnArr = csnzeros(shape)

    evens:CsnArr = csnfromarray(array(1, 2, 3))
    odds:CsnArr  = csnfromarray(array(-1, -2, -3))

    csnsetslice(dest, evens, 0, 0, 6, 2)
    csnsetslice(dest, odds,  0, 1, 6, 2)

    dest_out:i[] = csntoarray(dest)
    prints("interleaved = %g %g %g %g %g %g\n", dest_out[0], dest_out[1], dest_out[2], dest_out[3], dest_out[4], dest_out[5])

    ; a whole column of a matrix
    mat_shape:i[] = fillarray(3, 2)
    mat:CsnArr    = csnzeros(mat_shape)
    col_shape:i[] = fillarray(3, 1)
    col:CsnArr    = csnreshape(csnfromarray(array(7, 8, 9)), col_shape)
    csnsetslice(mat, col, 1, 0, 1, 1)
    mat_out:i[]   = csntoarray(csnflatten(mat))
    prints("column 0    = %g %g %g %g %g %g\n", mat_out[0], mat_out[1], mat_out[2], mat_out[3], mat_out[4], mat_out[5])
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
* [csnset](csnset.md)
* [csninsert](csninsert.md)

## Credits

Pasquale Mainolfi, 2026
