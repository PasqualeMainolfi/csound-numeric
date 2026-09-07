# csnsetissubset

## Abstract

Tests whether every member of the first set belongs to the second.

## Description

`csnsetissubset` returns `1` when `a` is a subset of `b`, including when both
sets are equal, and `0` otherwise. The empty set is a subset of every set. Both
inputs must be valid set arrays.

## Syntax

```csound
result:i = csnsetissubset(a:CsnArr, b:CsnArr)
result:k = csnsetissubset(a:CsnArr, b:CsnArr)
result:k = csnsetissubset(a:CsnArr, b:CsnArr, trig:k)
```

## Arguments

* `a:CsnArr`: the candidate subset.
* `b:CsnArr`: the candidate containing set.
* `trig:k` (optional, default `1`): k-rate trigger. A zero trigger republishes the previous result.

## Output

* `result:i` or `result:k`: `1` when `a` is a subset of `b`, otherwise `0`.

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
    small:CsnArr = csnlikeset(csnfromarray(array(2, 3)))
    large:CsnArr = csnlikeset(csnfromarray(array(1, 2, 3, 4)))
    prints("small subset of large = %d\n", csnsetissubset(small, large))
    turnoff
endin
</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnsetissuperset](csnsetissuperset.md)
* [csnsetisequal](csnsetisequal.md)
* [csnsetcontains](csnsetcontains.md)

## Credits

Pasquale Mainolfi, 2026
