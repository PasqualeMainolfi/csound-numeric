# csnsetsymdiff

## Abstract

Returns the values belonging to exactly one of two sets.

## Description

`csnsetsymdiff` computes the symmetric difference of two valid set arrays. A
value is included when it occurs in `a` or `b`, but not both. The result is
ascending, duplicate-free, and marked as a valid set.

## Syntax

```csound
result:CsnArr = csnsetsymdiff(a:CsnArr, b:CsnArr)
result:CsnArr = csnsetsymdiff(a:CsnArr, b:CsnArr, trig:k)
```

## Arguments

* `a:CsnArr`, `b:CsnArr`: valid real set arrays.
* `trig:k`: optional k-rate trigger. A zero trigger republishes the previous result.

## Output

* `result:CsnArr`: the sorted symmetric difference of `a` and `b`.

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
    result:CsnArr = csnsetsymdiff(a, b)
    csnprint result ; [1, 3, 4]
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
* [csnsetintersect](csnsetintersect.md)
* [csnsetdiff](csnsetdiff.md)

## Credits

Pasquale Mainolfi, 2026
