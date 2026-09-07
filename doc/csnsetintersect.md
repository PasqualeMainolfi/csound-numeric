# csnsetintersect

## Abstract

Returns the values common to two sets.

## Description

`csnsetintersect` compares two valid set arrays and returns each value belonging
to both. The result is ascending, duplicate-free, and marked as a valid set. It
is empty when the inputs are disjoint.

## Syntax

```csound
result:CsnArr = csnsetintersect(a:CsnArr, b:CsnArr)
result:CsnArr = csnsetintersect(a:CsnArr, b:CsnArr, trig:k)
```

## Arguments

* `a:CsnArr`, `b:CsnArr`: valid real set arrays.
* `trig:k`: optional k-rate trigger. A zero trigger republishes the previous result.

## Output

* `result:CsnArr`: the sorted intersection of `a` and `b`.

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
    a:CsnArr = csnlikeset(csnfromarray(array(1, 2, 3)))
    b:CsnArr = csnlikeset(csnfromarray(array(2, 3, 4)))
    result:CsnArr = csnsetintersect(a, b)
    csnprint result
    turnoff
endin
</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnsetunion](csnsetunion.md)
* [csnsetisdisjoint](csnsetisdisjoint.md)
* [csnsetdiff](csnsetdiff.md)

## Credits

Pasquale Mainolfi, 2026
