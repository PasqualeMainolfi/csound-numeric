# csndims

## Abstract

Return the number of dimensions of an array.

## Description

`csndims` reports the rank of an array: `1` for a vector, `2` for a matrix, and
so on up to the 8 dimensions csnum arrays can carry.

It is the value that decides how many bracket pairs a
[csntoarray](csntoarray.md) output needs, how many coordinates
[csnget](csnget.md) and [csnset](csnset.md) expect, and which axes are legal
arguments elsewhere: a valid axis runs from `0` to `csndims - 1`, with `-1`
meaning "the whole array, read flat" wherever an axis is optional.

## Syntax

```csound
ndim:i = csndims(handle:CsnArr)
ndim:k = csndims(handle:CsnArr)
```

## Arguments

* `handle:CsnArr`: the array to query.

## Output

* `ndim:i / ndim:k`: the number of dimensions.

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
; csndims.csd
;
; csndims is the rank. It bounds the legal axis arguments and fixes how many
; coordinates csnget needs.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    vec:CsnArr     = csnfromarray(array(1, 2, 3, 4, 5, 6))
    vec_dims:i     = csndims(vec)

    mat_shape:i[]  = fillarray(2, 3)
    mat:CsnArr     = csnreshape(vec, mat_shape)
    mat_dims:i     = csndims(mat)

    cube_shape:i[] = fillarray(2, 3, 1)
    cube:CsnArr    = csnreshape(vec, cube_shape)
    cube_dims:i    = csndims(cube)

    prints("vector = %d, matrix = %d, rank 3 = %d\n", vec_dims, mat_dims, cube_dims)

    ; the rank fixes how many coordinates csnget takes
    cell:i[] = fillarray(1, 2)
    value:i  = csnget(mat, cell)
    prints("mat[1][2] = %g\n", value)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnshape](csnshape.md)
* [csnsize](csnsize.md)
* [csntype](csntype.md)
* [csnreshape](csnreshape.md)

## Credits

Pasquale Mainolfi, 2026
