# csnsetunion

## Abstract

Returns every value belonging to either of two sets.

## Description

`csnsetunion` merges two valid set arrays. Each member appears once in ascending
order, and the result is itself marked as a valid set. Either input may be empty.

## Syntax

```csound
result:CsnArr = csnsetunion(a:CsnArr, b:CsnArr)
result:CsnArr = csnsetunion(a:CsnArr, b:CsnArr, trig:k)
```

## Arguments

* `a:CsnArr`, `b:CsnArr`: valid real set arrays.
* `trig:k`: optional k-rate trigger. A zero trigger republishes the previous result.

## Output

* `result:CsnArr`: the sorted union of `a` and `b`.

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
    b:CsnArr = csnlikeset(csnfromarray(array(2, 4)))
    result:CsnArr = csnsetunion(a, b)
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

* [csnsetintersect](csnsetintersect.md)
* [csnsetdiff](csnsetdiff.md)
* [csnsetsymdiff](csnsetsymdiff.md)

## Credits

Pasquale Mainolfi, 2026
