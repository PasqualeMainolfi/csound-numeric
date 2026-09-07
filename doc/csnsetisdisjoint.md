# csnsetisdisjoint

## Abstract

Tests whether two sets have no member in common.

## Description

`csnsetisdisjoint` returns `1` when the intersection of two valid set arrays is
empty, and `0` otherwise. An empty set is disjoint from every set, including
another empty set.

## Syntax

```csound
result:i = csnsetisdisjoint(a:CsnArr, b:CsnArr)
result:k = csnsetisdisjoint(a:CsnArr, b:CsnArr)
result:k = csnsetisdisjoint(a:CsnArr, b:CsnArr, trig:k)
```

## Arguments

* `a:CsnArr`, `b:CsnArr`: valid real set arrays.
* `trig:k` (optional, default `1`): k-rate trigger. A zero trigger republishes the previous result.

## Output

* `result:i` or `result:k`: `1` when the sets share no value, otherwise `0`.

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
    a:CsnArr = csnlikeset(csnfromarray(array(1, 3, 5)))
    b:CsnArr = csnlikeset(csnfromarray(array(2, 4, 6)))
    prints("sets are disjoint = %d\n", csnsetisdisjoint(a, b))
    turnoff
endin
</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnsetintersect](csnsetintersect.md)
* [csnsetisequal](csnsetisequal.md)
* [csnsetsymdiff](csnsetsymdiff.md)

## Credits

Pasquale Mainolfi, 2026
