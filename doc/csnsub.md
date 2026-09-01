# csnsub

## Abstract

Running subtraction of every element from the first.

## Description

`csnsub` folds an array by subtraction: it starts from the first element and takes
every later one off it, so `10 1 2 3` gives `10 - 1 - 2 - 3`, that is `4`.

It is the reduction counterpart of [csnsubtract](csnsubtract.md), which works
elementwise between two arrays.

Over the whole array the result is a single number, or a `:Complex;` for a complex source. Given an axis the
reduction runs along that axis instead and the result is an array, with the
reduced axis dropped: a `2×3` matrix reduced on axis 0 gives 3 values, one per
column, and on axis 1 gives 2, one per row.

The fold has no neutral element — there is nothing to start from — so it is
undefined over an empty extent and refused rather than answered with a guess.

Both real and complex arrays are accepted; a complex fold comes back as a
`:Complex;`.

## Syntax

```csound
result:i = csnsub(source:CsnArr)
result:k = csnsub(source:CsnArr)
result:k = csnsub(source:CsnArr, trig:k)
result:Complex = csnsub(source:CsnArr)
result:Complex = csnsub(source:CsnArr, trig:k)
handle:CsnArr = csnsub(source:CsnArr, axis:i)
handle:CsnArr = csnsub(source:CsnArr, axis:k)
handle:CsnArr = csnsub(source:CsnArr, axis:k, trig:k)
```

## Arguments

* `source:CsnArr`: the array to reduce.
* `axis:i / axis:k` (optional): the axis to reduce along. Omitted, the whole array is reduced to one number.
* `trig:k` (optional, default `1`): k-rate trigger. A zero trigger republishes the previous result.

## Output

* `result:i / result:k`: the reduction over the whole array, for a real source.
* `result:Complex`: the reduction over the whole array, for a complex source.
* `handle:CsnArr`: one value per line along `axis`, with that axis dropped.

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
; csnsub.csd
;
; Seeded from the first element, then every later one is taken off it. Undefined
; over an empty extent, unlike csnsum.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    vec:CsnArr    = csnfromarray(array(10, 1, 2, 3))
    whole:i       = csnsub(vec)
    prints("10-1-2-3  = %g\n", whole)

    ; along an axis: one value per column, then one per row
    shape:i[]     = fillarray(2, 3)
    mat:CsnArr    = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6)), shape)

    cols:CsnArr   = csnsub(mat, 0)
    cols_out:i[]  = csntoarray(cols)
    prints("per column = %g %g %g\n", cols_out[0], cols_out[1], cols_out[2])

    rows:CsnArr   = csnsub(mat, 1)
    rows_out:i[]  = csntoarray(rows)
    prints("per row    = %g %g\n", rows_out[0], rows_out[1])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnsubtract](csnsubtract.md)
* [csnsum](csnsum.md)
* [csndiff](csndiff.md)

## Credits

Pasquale Mainolfi, 2026
