# csncntnz

## Abstract

Count the non-zero elements.

## Description

`csncntnz` returns how many elements of an array are not zero, in one pass and
without publishing a mask.

Over a mask from the comparison opcodes it is the count of elements that passed
the test, which makes it the shortest way to answer "how many". Over raw data it
reports the array's density.

Real only. NaN counts as non-zero; use [csncntnan](csncntnan.md) to separate
those out.

## Syntax

```csound
count:i = csncntnz(source:CsnArr)
count:k = csncntnz(source:CsnArr, trig:k)
```

## Arguments

* `source:CsnArr`: the array to scan.
* `trig:k`: k-rate trigger. A zero trigger republishes the previous count.

## Output

* `count:i / count:k`: how many elements are non-zero.

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
; csncntnz.csd
;
; Over raw data it is the density; over a mask it is the number of elements that
; passed the test.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr   = csnfromarray(array(0, 3, 0, 0, 7, 2))

    filled:i      = csncntnz(data)
    total:i       = csnsize(data)
    prints("non-zero = %d of %d\n", filled, total)

    ; over a mask: how many elements passed
    loud:CsnArr   = csngt(data, 2)
    passed:i      = csncntnz(loud)
    prints("above 2  = %d\n", passed)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csncnteq](csncnteq.md)
* [csncntnan](csncntnan.md)
* [csnargnonzero](csnargnonzero.md)
* [csnany](csnany.md)

## Credits

Pasquale Mainolfi, 2026
