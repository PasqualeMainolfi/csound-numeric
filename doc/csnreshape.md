# csnreshape

## Abstract

Return the same elements under a new shape.

## Description

`csnreshape` reinterprets an array's flat element order under a different set of
extents. The element count must match: a 6-element array can become `2×3`,
`3×2`, `6` or `1×6`, but not `2×4`.

Shape and strides are kept in the array itself, so this is a layout change, not a
copy of the data.

Two forms share the name. The one with an output publishes a new handle and
leaves the source alone; the one without an output rewrites the source in place
and returns nothing.

## Syntax

```csound
handle:CsnArr = csnreshape(source:CsnArr, shape:i[])
handle:CsnArr = csnreshape(source:CsnArr, shape:k[])
csnreshape(source:CsnArr, shape:i[])
csnreshape(source:CsnArr, shape:k[])
```

## Arguments

* `source:CsnArr`: the array to reshape.
* `shape:i[] / shape:k[]`: the new extents, one per dimension. Their product must equal the array's element count.

## Output

* `handle:CsnArr`: handle of the reshaped array. Omit it for the in-place form.

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
; csnreshape.csd
;
; csnreshape changes the layout, not the data: the flat order is preserved and
; only the extents move. The form without an output rewrites its source.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    vec:CsnArr    = csnfromarray(array(1, 2, 3, 4, 5, 6))

    two_by_three:i[] = fillarray(2, 3)
    mat:CsnArr    = csnreshape(vec, two_by_three)
    mat_out:i[]   = csntoarray(csnflatten(mat))
    mat_shape:i[] = csnshape(mat)
    prints("%g x %g: %g %g %g %g %g %g\n", mat_shape[0], mat_shape[1], mat_out[0], mat_out[1], mat_out[2], mat_out[3], mat_out[4], mat_out[5])

    ; the source is untouched
    vec_dims:i    = csndims(vec)
    prints("source dims still = %d\n", vec_dims)

    ; in place: no output, the source itself becomes 3 x 2
    three_by_two:i[] = fillarray(3, 2)
    csnreshape(vec, three_by_two)
    now:i[]       = csnshape(vec)
    prints("source is now %g x %g\n", now[0], now[1])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnflatten](csnflatten.md)
* [csnresize](csnresize.md)
* [csntranspose](csntranspose.md)
* [csnshape](csnshape.md)

## Credits

Pasquale Mainolfi, 2026
