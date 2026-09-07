# csnsetinsert

## Abstract

Inserts a value into a set while preserving its invariant.

## Description

`csnsetinsert` modifies a valid set array in place. The value is inserted at its
sorted position when absent; inserting an existing value has no effect. The
array remains one-dimensional, ascending, duplicate-free, and marked as a set.

Insertion may grow and reallocate the array. Plan k-rate use accordingly and do
not trigger an insertion on a path that must remain allocation-free.

## Syntax

```csound
csnsetinsert values:CsnArr, value:i
csnsetinsert values:CsnArr, value:k
csnsetinsert values:CsnArr, value:k, trig:k
```

## Arguments

* `values:CsnArr`: a valid real set array, modified in place.
* `value:i` or `value:k`: the value to insert.
* `trig:k`: optional k-rate trigger. A zero trigger performs no operation.

## Output

None. `values` contains the new member if it was not already present.

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
    values:CsnArr = csnlikeset(csnfromarray(array(1, 3, 5)))
    csnsetinsert values, 4
    csnsetinsert values, 3 ; already present: no change
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

* [csnsetremove](csnsetremove.md)
* [csnsetcontains](csnsetcontains.md)
* [csnlikeset](csnlikeset.md)

## Credits

Pasquale Mainolfi, 2026
