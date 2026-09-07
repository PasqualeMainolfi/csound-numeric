# csnunlikeset

## Abstract

Removes the set classification without changing an array's contents.

## Description

`csnunlikeset` modifies the source handle in place, clearing its set
classification while preserving its values, shape, and order. It does not undo
the sorting or duplicate removal performed by [csnlikeset](csnlikeset.md).

After this call, set operations reject the array until `csnlikeset` normalizes
and marks it again. This is useful before intentionally editing a set through
the generic array API.

## Syntax

```csound
csnunlikeset values:CsnArr
csnunlikeset values:CsnArr, trig:k
```

## Arguments

* `values:CsnArr`: the set array whose classification is cleared in place.
* `trig:k`: optional k-rate trigger. A zero trigger performs no operation.

## Output

None. `values` keeps the same payload but is no longer a set array.

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
    values:CsnArr = csnlikeset(csnfromarray(array(3, 1, 2, 2)))
    csnunlikeset values
    first:i[] = array(0)
    csnset values, first, 99
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

* [csnlikeset](csnlikeset.md)
* [csnset](csnset.md)
* [Set arrays and their invariant](set-arrays.md)

## Credits

Pasquale Mainolfi, 2026
