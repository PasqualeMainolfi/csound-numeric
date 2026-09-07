# csnlikeset

## Abstract

Creates a normalized set array from a real array.

## Description

`csnlikeset` reads the source flat, sorts its values in ascending order, removes
duplicates, and returns a one-dimensional array marked as a set. The source is
not modified. Empty arrays are valid; NaNs compare equal for set purposes and
sort after all finite and infinite values.

Only arrays produced or maintained by the set API may be passed to other set
operations. See [Set arrays and their invariant](set-arrays.md).

## Syntax

```csound
set:CsnArr = csnlikeset(source:CsnArr)
set:CsnArr = csnlikeset(source:CsnArr, trig:k)
```

## Arguments

* `source:CsnArr`: any real array, read in flat order.
* `trig:k`: optional k-rate trigger. A zero trigger republishes the previous result.

## Output

* `set:CsnArr`: a one-dimensional, ascending, duplicate-free set array.

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
    raw:CsnArr = csnfromarray(array(3, 1, 2, 2, 1))
    values:CsnArr = csnlikeset(raw)
    out:i[] = csntoarray(values)
    prints("set = %g %g %g\n", out[0], out[1], out[2])
    turnoff
endin
</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnunlikeset](csnunlikeset.md)
* [csnsetinsert](csnsetinsert.md)
* [csnunique](csnunique.md)

## Credits

Pasquale Mainolfi, 2026
