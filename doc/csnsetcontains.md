# csnsetcontains

## Abstract

Tests whether a set contains a value.

## Description

`csnsetcontains` uses the sorted set representation to test membership. It
returns `1` when the value occurs and `0` otherwise. The input must be a valid
set array. NaNs compare equal to one another for set membership.

## Syntax

```csound
result:i = csnsetcontains(values:CsnArr, value:i)
result:k = csnsetcontains(values:CsnArr, value:k)
result:k = csnsetcontains(values:CsnArr, value:k, trig:k)
```

## Arguments

* `values:CsnArr`: a valid real set array.
* `value:i` or `value:k`: the value whose membership is tested.
* `trig:k`: optional k-rate trigger. A zero trigger republishes the previous result.

## Output

* `result:i` or `result:k`: `1` when `value` is a member, otherwise `0`.

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
    values:CsnArr = csnlikeset(csnfromarray(array(10, 20, 30)))
    prints("contains 20 = %d\n", csnsetcontains(values, 20))
    prints("contains 25 = %d\n", csnsetcontains(values, 25))
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
* [csnsetremove](csnsetremove.md)
* [csnlikeset](csnlikeset.md)

## Credits

Pasquale Mainolfi, 2026
