# csnsetisequal

## Abstract

Tests whether two sets contain exactly the same values.

## Description

`csnsetisequal` returns `1` when two valid set arrays have the same members and
`0` otherwise. Because set arrays are normalized, member equality is also an
element-by-element comparison of their stored values. Two empty sets are equal.

## Syntax

```csound
result:i = csnsetisequal(a:CsnArr, b:CsnArr)
result:k = csnsetisequal(a:CsnArr, b:CsnArr)
result:k = csnsetisequal(a:CsnArr, b:CsnArr, trig:k)
```

## Arguments

* `a:CsnArr`, `b:CsnArr`: valid real set arrays.
* `trig:k` (optional, default `1`): k-rate trigger. A zero trigger republishes the previous result.

## Output

* `result:i` or `result:k`: `1` when both sets contain the same values, otherwise `0`.

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
    a:CsnArr = csnlikeset(csnfromarray(array(3, 1, 2, 2)))
    b:CsnArr = csnlikeset(csnfromarray(array(2, 3, 1)))
    prints("same members = %d\n", csnsetisequal(a, b))
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
* [csnsetissuperset](csnsetissuperset.md)
* [csnsetsymdiff](csnsetsymdiff.md)

## Credits

Pasquale Mainolfi, 2026
