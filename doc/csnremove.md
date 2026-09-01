# csnremove

## Abstract

Remove an element, or a block, at a position in an array.

## Description

`csnremove` is the counterpart of [csninsert](csninsert.md), and the two forms
behave differently on purpose.

* **One element.** `csnremove(array, index)` takes the element at a flat index
  out of the source, shifts the rest down, and **returns the removed value**. The
  source shrinks by one.
* **A block.** `csnremove(array, axis, index)` drops one index along an axis — a
  row out of a matrix, say — and **returns a new array** with that slice missing.
  The source is left alone.

A complex array's one-element form returns a `:Complex;`.

## Syntax

```csound
value:i = csnremove(source:CsnArr, index:i)
value:Complex = csnremove(source:CsnArr, index:i)
value:k = csnremove(source:CsnArr, index:k, trig:k)
value:Complex = csnremove(source:CsnArr, index:k, trig:k)
handle:CsnArr = csnremove(source:CsnArr, axis:i, index:i)
handle:CsnArr = csnremove(source:CsnArr, axis:k, index:k)
handle:CsnArr = csnremove(source:CsnArr, axis:k, index:k, trig:k)
```

## Arguments

* `source:CsnArr`: the array to remove from. The one-element form modifies it; the block form does not.
* `index:i / index:k`: the flat index of the element, or the index along `axis` of the block.
* `axis:i / axis:k`: the axis the block is dropped from.
* `trig:k` (optional, default `1`): k-rate trigger. Nothing is removed on a zero trigger.

## Output

* `value:i / value:k / value:Complex`: the removed element, in the one-element form.
* `handle:CsnArr`: a new array with the block missing, in the block form.

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
; csnremove.csd
;
; Two arguments take one element out of the source and hand it back. Three drop
; a slice along an axis and publish a new array, leaving the source alone.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    vec:CsnArr     = csnfromarray(array(10, 20, 30, 40))
    gone:i         = csnremove(vec, 1)
    vec_out:i[]    = csntoarray(vec)
    n:i            = csnsize(vec)
    prints("removed %g, source n = %d: %g %g %g\n", gone, n, vec_out[0], vec_out[1], vec_out[2])

    ; a row out of a matrix: a new array, the source untouched
    shape:i[]      = fillarray(3, 2)
    mat:CsnArr     = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6)), shape)
    less:CsnArr    = csnremove(mat, 0, 1)
    less_shape:i[] = csnshape(less)
    less_out:i[]   = csntoarray(csnflatten(less))
    mat_size:i     = csnsize(mat)
    prints("block: %g x %g = %g %g %g %g, source size still %d\n", less_shape[0], less_shape[1], less_out[0], less_out[1], less_out[2], less_out[3], mat_size)
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
* [csnpop](csnpop.md)
* [csntake](csntake.md)
* [csntruncate](csntruncate.md)

## Credits

Pasquale Mainolfi, 2026
