# csnsetdiff

## Abstract

Returns the members of the first set that are absent from the second.

## Description

`csnsetdiff` computes the set difference `a − b`. Both inputs must be valid
set arrays. The result preserves ascending order and is marked as a valid set;
it is empty when every member of `a` also belongs to `b`.

## Syntax

```csound
result:CsnArr = csnsetdiff(a:CsnArr, b:CsnArr)
result:CsnArr = csnsetdiff(a:CsnArr, b:CsnArr, trig:k)
```

## Arguments

* `a:CsnArr`: the valid real set from which members are selected.
* `b:CsnArr`: the valid real set whose members are excluded.
* `trig:k`: optional k-rate trigger. A zero trigger republishes the previous result.

## Output

* `result:CsnArr`: the sorted members that occur in `a` but not in `b`.

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
    a:CsnArr = csnlikeset(csnfromarray(array(1, 2, 3, 4)))
    b:CsnArr = csnlikeset(csnfromarray(array(2, 4, 6)))
    result:CsnArr = csnsetdiff(a, b)
    csnprint result ; [1, 3]
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
* [csnsetsymdiff](csnsetsymdiff.md)

## Credits

Pasquale Mainolfi, 2026
