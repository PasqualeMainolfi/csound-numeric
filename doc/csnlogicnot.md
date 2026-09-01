# csnlogicnot

## Abstract

Logical negation of an array.

## Description

`csnlogicnot` returns `1` where an element is zero and `0` where it is not. It
reads its input as truth values, so it turns any array into a mask of "where this
is zero".

Applied to a mask it is the complement: the elements a test rejected. Applied to
raw data it is the "is exactly zero" test, which
[csneq](csneq.md) would say the long way.

Real only.

## Syntax

```csound
handle:CsnArr = csnlogicnot(source:CsnArr)
handle:CsnArr = csnlogicnot(source:CsnArr, trig:k)
```

## Arguments

* `source:CsnArr`: the array to negate, read as truth values.
* `trig:k`: k-rate trigger. A zero trigger republishes the previous result.

## Output

* `handle:CsnArr`: a 0/1 array.

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
; csnlogicnot.csd
;
; On a mask it is the complement; on raw data it is the "is exactly zero" test.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr     = csnfromarray(array(-2, 0, 1, 0, 5))

    is_zero:CsnArr  = csnlogicnot(data)
    is_zero_out:i[] = csntoarray(is_zero)
    prints("is zero    = %g %g %g %g %g\n", is_zero_out[0], is_zero_out[1], is_zero_out[2], is_zero_out[3], is_zero_out[4])

    ; the complement of a mask
    loud:CsnArr     = csngt(data, 0)
    quiet:CsnArr    = csnlogicnot(loud)
    quiet_out:i[]   = csntoarray(quiet)
    prints("not > 0    = %g %g %g %g %g\n", quiet_out[0], quiet_out[1], quiet_out[2], quiet_out[3], quiet_out[4])

    ; double negation is the "is non-zero" mask
    non_zero:CsnArr = csnlogicnot(is_zero)
    count:i         = csnsum(non_zero)
    nz:i            = csncntnz(data)
    prints("non-zero: %g by mask, %d by csncntnz\n", count, nz)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnlogicand](csnlogicand.md)
* [csnlogicor](csnlogicor.md)
* [csncntnz](csncntnz.md)
* [csneq](csneq.md)

## Credits

Pasquale Mainolfi, 2026
