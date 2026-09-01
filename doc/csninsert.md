# csninsert

## Abstract

Insert an element, or a block, at a position in an array.

## Description

`csninsert` writes into its source array: it makes room at `index` and puts the
new data there, shifting everything after it along. It publishes no handle — the
source itself grows.

Two shapes of insertion share the name.

* **One element.** `csninsert(array, value, index)` inserts a single value at a
  flat index. `index` may run from `0` to the element count, end included, so
  inserting at the end is an append.
* **A block.** `csninsert(array, data, axis, index)` inserts a whole array along
  an axis: a row into a matrix, say. The block must match the source on every
  other axis.

A complex array takes a `:Complex;` value in the one-element form.

## Syntax

```csound
csninsert(source:CsnArr, value:i, index:i)
csninsert(source:CsnArr, value:Complex, index:i)
csninsert(source:CsnArr, value:k, index:k, trig:k)
csninsert(source:CsnArr, value:Complex, index:k, trig:k)
csninsert(source:CsnArr, data:CsnArr, axis:i, index:i)
csninsert(source:CsnArr, data:CsnArr, axis:k, index:k)
csninsert(source:CsnArr, data:CsnArr, axis:k, index:k, trig:k)
```

## Arguments

* `source:CsnArr`: the array that is written into.
* `value:i / value:k / value:Complex`: the single element to insert.
* `data:CsnArr`: the block to insert; must match `source` on every axis other than `axis`.
* `axis:i / axis:k`: the axis the block is inserted along.
* `index:i / index:k`: where to insert. Valid from `0` to the extent, end included.
* `trig:k` (optional, default `1`): k-rate trigger. Nothing is inserted on a zero trigger.

## Output

None. The source array is modified.

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
; csninsert.csd
;
; csninsert writes into its source rather than publishing a new handle. Three
; arguments insert one element at a flat index; four insert a block along an axis.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    vec:CsnArr    = csnfromarray(array(1, 2, 3, 4))
    csninsert(vec, 99, 2)
    vec_out:i[]   = csntoarray(vec)
    n:i           = csnsize(vec)
    prints("one element, n = %d: %g %g %g %g %g\n", n, vec_out[0], vec_out[1], vec_out[2], vec_out[3], vec_out[4])

    ; a whole row into a matrix
    shape:i[]     = fillarray(3, 2)
    mat:CsnArr    = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6)), shape)
    row:CsnArr    = csnfromarray(array(7, 8))
    csninsert(mat, row, 0, 1)
    mat_shape:i[] = csnshape(mat)
    mat_out:i[]   = csntoarray(csnflatten(mat))
    prints("block: %g x %g, flat = %g %g %g %g %g %g %g %g\n", mat_shape[0], mat_shape[1], mat_out[0], mat_out[1], mat_out[2], mat_out[3], mat_out[4], mat_out[5], mat_out[6], mat_out[7])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnremove](csnremove.md)
* [csnpush](csnpush.md)
* [csnconcat](csnconcat.md)
* [csnsetslice](csnsetslice.md)

## Credits

Pasquale Mainolfi, 2026
