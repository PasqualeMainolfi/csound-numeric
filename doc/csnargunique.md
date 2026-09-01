# csnargunique

## Abstract

Coordinates of the first occurrence of each distinct value.

## Description

`csnargunique` returns, for every distinct value of an array, the position where
it first appeared. The values themselves come in the same order from
[csnunique](csnunique.md), so the two results line up element by element: the
`n`-th coordinate here belongs to the `n`-th value there.

That is what makes it usable as a de-duplicating index: keep only the first
occurrence of each value, and use the same positions to pick the matching
elements out of a parallel array.

Real only.

## Syntax

```csound
handle:CsnArr = csnargunique(source:CsnArr)
handle:CsnArr = csnargunique(source:CsnArr, trig:k)
```

## Arguments

* `source:CsnArr`: the array to scan.
* `trig:k`: k-rate trigger. A zero trigger republishes the previous result.

## Output

* `handle:CsnArr`: the coordinates of the first occurrence of each distinct value, in the order [csnunique](csnunique.md) reports them.

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
; csnargunique.csd
;
; The positions line up with csnunique's values, which is what lets a parallel
; array be de-duplicated the same way.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr    = csnfromarray(array(3, 1, 4, 1, 5))

    values:CsnArr  = csnunique(data)
    where:CsnArr   = csnargunique(data)
    values_out:i[] = csntoarray(values)
    where_out:i[]  = csntoarray(csnflatten(where))
    count:i        = csnsize(values)

    prints("distinct = %d\n", count)
    n:i = 0
    while n < count do
        prints("value %g first seen at index %g\n", values_out[n], where_out[n])
        n += 1
    od

    ; the same positions pick out of a parallel array
    tags:CsnArr    = csnfromarray(array(10, 20, 30, 40, 50))
    first_tag:i    = csntake(tags, where_out[0])
    prints("tag of the first distinct value = %g\n", first_tag)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnunique](csnunique.md)
* [csnargsort](csnargsort.md)
* [csnargwhere](csnargwhere.md)

## Credits

Pasquale Mainolfi, 2026
