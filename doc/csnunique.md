# csnunique

## Abstract

The distinct values of an array, in order.

## Description

`csnunique` returns each distinct value of an array exactly once, in ascending
order. `3 1 4 1 5` gives `1 3 4 5`.

The result is shorter than the source whenever anything repeated, so its length
is itself the answer to "how many different values are there".
[csnargunique](csnargunique.md) returns where each of them first appeared.

Equality is exact, as everywhere in the suite: two values that differ in the last
bit are two distinct values. Round with [csnround](csnround.md) first when a
tolerance is what you mean.

Real only.

## Syntax

```csound
handle:CsnArr = csnunique(source:CsnArr)
handle:CsnArr = csnunique(source:CsnArr, trig:k)
```

## Arguments

* `source:CsnArr`: the array to reduce to its distinct values.
* `trig:k`: k-rate trigger. A zero trigger republishes the previous result.

## Output

* `handle:CsnArr`: the distinct values, in ascending order.

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

; -----------------------------------------------------------------------------
; csnunique.csd
;
; Each value once, in order. The length of the result is the count of distinct
; values, which is how a pitch set is reduced to its pitch classes.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr    = csnfromarray(array(3, 1, 4, 1, 5))

    values:CsnArr  = csnunique(data)
    values_out:i[] = csntoarray(values)
    count:i        = csnsize(values)
    prints("distinct = %d : %g %g %g %g\n", count, values_out[0], values_out[1], values_out[2], values_out[3])

    ; midi notes reduced to pitch classes
    notes:CsnArr   = csnfromarray(array(60, 64, 67, 72, 76, 79))
    q:CsnArr, pc:CsnArr = csndivmod(notes, 12)
    classes:CsnArr = csnunique(pc)
    classes_out:i[] = csntoarray(classes)
    classes_n:i    = csnsize(classes)
    prints("pitch classes = %d : %g %g %g\n", classes_n, classes_out[0], classes_out[1], classes_out[2])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnargunique](csnargunique.md)
* [csnsort](csnsort.md)
* [csncnteq](csncnteq.md)

## Credits

Pasquale Mainolfi, 2026
