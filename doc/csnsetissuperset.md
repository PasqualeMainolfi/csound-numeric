# csnsetissuperset

## Abstract

Tests whether the first set contains every member of the second.

## Description

`csnsetissuperset` returns `1` when `a` is a superset of `b`, including when
both sets are equal, and `0` otherwise. Every set is a superset of the empty
set. Both inputs must be valid set arrays.

## Syntax

```csound
result:i = csnsetissuperset(a:CsnArr, b:CsnArr)
result:k = csnsetissuperset(a:CsnArr, b:CsnArr)
result:k = csnsetissuperset(a:CsnArr, b:CsnArr, trig:k)
```

## Arguments

* `a:CsnArr`: the candidate containing set.
* `b:CsnArr`: the candidate subset.
* `trig:k` (optional, default `1`): k-rate trigger. A zero trigger republishes the previous result.

## Output

* `result:i` or `result:k`: `1` when `a` is a superset of `b`, otherwise `0`.

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
sr = 48000
ksmps = 32
0dbfs = 1

instr 1
    large:CsnArr = csnlikeset(csnfromarray(array(1, 2, 3, 4)))
    small:CsnArr = csnlikeset(csnfromarray(array(2, 3)))
    prints("large superset of small = %d\n", csnsetissuperset(large, small))
    turnoff
endin
</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnsetissubset](csnsetissubset.md)
* [csnsetisequal](csnsetisequal.md)
* [csnsetcontains](csnsetcontains.md)

## Credits

Pasquale Mainolfi, 2026
