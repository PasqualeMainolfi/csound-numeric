# csncnteq

## Abstract

Count the elements equal to a value.

## Description

`csncnteq` returns how many elements of an array are exactly equal to a given
value. It is [csneq](csneq.md) followed by [csnsum](csnsum.md), done in one pass
and without publishing a mask.

Use it when only the count matters. When the *positions* matter,
[csnargwhere](csnargwhere.md) is the opcode to reach for.

Real only. Equality is exact, so a NaN is never counted, and a value that is the
result of floating-point arithmetic may not compare equal to the literal you
expect.

## Syntax

```csound
count:i = csncnteq(source:CsnArr, value:i)
count:k = csncnteq(source:CsnArr, value:k, trig:k)
```

## Arguments

* `source:CsnArr`: the array to scan.
* `value:i / value:k`: the value to count.
* `trig:k`: k-rate trigger. A zero trigger republishes the previous count.

## Output

* `count:i / count:k`: how many elements equal `value`.

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
; csncnteq.csd
;
; One pass, no mask. csneq plus csnsum says the same thing the long way, and
; csnargwhere says where rather than how many.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr  = csnfromarray(array(1, 5, 3, 5, 2, 5))

    fives:i      = csncnteq(data, 5)
    zeros:i      = csncnteq(data, 0)
    prints("fives = %d, zeros = %d\n", fives, zeros)

    ; the same answer the long way
    mask:CsnArr  = csneq(data, 5)
    by_mask:i    = csnsum(mask)
    prints("via csneq + csnsum = %g\n", by_mask)

    ; and the positions, when those are what is wanted
    wanted:CsnArr = csnfromarray(array(5))
    where:CsnArr  = csnargwhere(data, wanted)
    where_out:i[] = csntoarray(csnflatten(where))
    prints("at %g %g %g\n", where_out[0], where_out[1], where_out[2])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csncntnz](csncntnz.md)
* [csncntnan](csncntnan.md)
* [csneq](csneq.md)
* [csnargwhere](csnargwhere.md)

## Credits

Pasquale Mainolfi, 2026
