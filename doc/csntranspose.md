# csntranspose

## Abstract

Permute the axes of an array.

## Description

`csntranspose` reorders the dimensions of an array. Called with no axes argument
it reverses them all, which for a matrix is the ordinary transpose: a `2×3`
becomes a `3×2` and element `[i][j]` becomes `[j][i]`.

Given an explicit `axes` array it applies that permutation instead: `axes` must
list every source axis exactly once. Negative values count from the end, so
`-1` names the last source axis; axis `n` of the result is axis `axes[n]` of the
source.

The permutation lives in the array's strides, so this is a layout change, not a
copy of the data.

Two forms share the name. The one with an output writes a new array and
leaves the source alone; the one without an output rewrites the source in place
and returns nothing.

Both forms also take a `:CsnStr;`. The copy duplicates every string, so it
owns its own; the in-place form only moves the strings it already holds to
their new slots, allocating nothing, so it may run on a real-time path. There
the copying k form writes into the room its slots already hold and stops with a
performance error at the first string that does not fit. See [string
arrays](string-arrays.md).

## Syntax

```csound
out:CsnArr = csntranspose(source:CsnArr)
out:CsnArr = csntranspose(source:CsnArr, trig:k)
out:CsnArr = csntranspose(source:CsnArr, axes:i[])
out:CsnArr = csntranspose(source:CsnArr, axes:k[])
csntranspose(source:CsnArr)
csntranspose(source:CsnArr, trig:k)
csntranspose(source:CsnArr, axes:i[])
csntranspose(source:CsnArr, axes:k[])
out:CsnStr = csntranspose(source:CsnStr)
out:CsnStr = csntranspose(source:CsnStr, trig:k)
out:CsnStr = csntranspose(source:CsnStr, axes:i[])
out:CsnStr = csntranspose(source:CsnStr, axes:k[])
csntranspose(source:CsnStr)
csntranspose(source:CsnStr, trig:k)
csntranspose(source:CsnStr, axes:i[])
csntranspose(source:CsnStr, axes:k[])
```

## Arguments

* `source:CsnArr / source:CsnStr`: the array to transpose.
* `trig:k`: k-rate trigger of the form without axes. Without it the call is the init-only form and transposes once; with it the axes are reversed again on every pass where it is non-zero.
* `axes:i[] / axes:k[]` (optional): a permutation of `0 … ndims - 1`. Omitted, the axes are reversed. A `k[]` gives the k-rate form, which re-reads it on every pass; while it does not read as a permutation, as at init before it is written, the output keeps the source's layout, reserved but empty.

## Output

* `out:CsnArr / out:CsnStr`: the transposed array. Omit it for the in-place form.

## Execution Time

* Init
* Performance (k-rate, with `trig:k` or `axes:k[]`)

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

    ; the same permutation using negative axis indices
    axes:i[]       = fillarray(-1, -2)
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
