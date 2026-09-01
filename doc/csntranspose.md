# csntranspose

## Abstract

Permute the axes of an array.

## Description

`csntranspose` reorders the dimensions of an array. Called with no axes argument
it reverses them all, which for a matrix is the ordinary transpose: a `2×3`
becomes a `3×2` and element `[i][j]` becomes `[j][i]`.

Given an explicit `axes` array it applies that permutation instead: `axes` must
list every axis index from `0` to `ndims - 1` exactly once, and axis `n` of the
result is axis `axes[n]` of the source.

The permutation lives in the array's strides, so this is a layout change, not a
copy of the data.

Two forms share the name. The one with an output publishes a new handle and
leaves the source alone; the one without an output rewrites the source in place
and returns nothing.

## Syntax

```csound
handle:CsnArr = csntranspose(source:CsnArr)
handle:CsnArr = csntranspose(source:CsnArr, axes:i[])
handle:CsnArr = csntranspose(source:CsnArr, axes:k[])
csntranspose(source:CsnArr)
csntranspose(source:CsnArr, axes:i[])
csntranspose(source:CsnArr, axes:k[])
```

## Arguments

* `source:CsnArr`: the array to transpose.
* `axes:i[] / axes:k[]` (optional): a permutation of `0 … ndims - 1`. Omitted, the axes are reversed.

## Output

* `handle:CsnArr`: handle of the transposed array. Omit it for the in-place form.

## Execution Time

* Init
* Performance (k-rate, with an explicit `axes` argument)

## Examples

```csound
<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csntranspose.csd
;
; With no axes argument csntranspose reverses them all. For a matrix that is
; the ordinary transpose; the explicit form generalises it to any rank.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    shape:i[]      = fillarray(2, 3)
    mat:CsnArr     = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6)), shape)

    tr:CsnArr      = csntranspose(mat)
    tr_shape:i[]   = csnshape(tr)
    tr_out:i[]     = csntoarray(csnflatten(tr))
    prints("shape = %g x %g, flat = %g %g %g %g %g %g\n", tr_shape[0], tr_shape[1], tr_out[0], tr_out[1], tr_out[2], tr_out[3], tr_out[4], tr_out[5])

    ; the same thing said as an explicit permutation
    axes:i[]       = fillarray(1, 0)
    perm:CsnArr    = csntranspose(mat, axes)
    perm_out:i[]   = csntoarray(csnflatten(perm))
    prints("explicit  = %g %g %g %g %g %g\n", perm_out[0], perm_out[1], perm_out[2], perm_out[3], perm_out[4], perm_out[5])

    ; in place
    csntranspose(mat)
    now:i[]        = csnshape(mat)
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

* [csnreshape](csnreshape.md)
* [csnflip](csnflip.md)
* [csnmatmul](csnmatmul.md)

## Credits

Pasquale Mainolfi, 2026
