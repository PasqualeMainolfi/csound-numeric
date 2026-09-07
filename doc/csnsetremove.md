# csnsetremove

## Abstract

Removes a value from a set while preserving its invariant.

## Description

`csnsetremove` modifies a valid set array in place. When the value is present it
is removed and the logical length decreases by one; removing an absent value has
no effect. The result remains an ascending, duplicate-free set array.

## Syntax

```csound
csnsetremove values:CsnArr, value:i
csnsetremove values:CsnArr, value:k
csnsetremove values:CsnArr, value:k, trig:k
```

## Arguments

* `values:CsnArr`: a valid real set array, modified in place.
* `value:i` or `value:k`: the value to remove.
* `trig:k`: optional k-rate trigger. A zero trigger performs no operation.

## Output

None. `values` no longer contains the requested member when it was present.

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
    values:CsnArr = csnlikeset(csnfromarray(array(1, 2, 3, 4)))
    csnsetremove values, 2
    csnsetremove values, 9 ; absent: no change
    csnprint values
    turnoff
endin
</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnsetinsert](csnsetinsert.md)
* [csnsetcontains](csnsetcontains.md)
* [csnlikeset](csnlikeset.md)

## Credits

Pasquale Mainolfi, 2026
