# csnpush

## Abstract

Append one element at the end of an array.

## Description

`csnpush` adds a value at the end of a 1-D array and raises its element count by
one. It writes into its source and publishes nothing.

Growing is amortised: an array made by [csnempty](csnempty.md) reserves capacity
up front, and pushing into that reservation costs no allocation at all. Only a
push past the reserved extent reallocates, and the array then grows
geometrically.

A complex array takes a `:Complex;` value. [csnpop](csnpop.md) is the inverse.

## Syntax

```csound
csnpush(handle:CsnArr, value:i)
csnpush(handle:CsnArr, value:Complex)
csnpush(handle:CsnArr, value:k, trig:k)
csnpush(handle:CsnArr, value:Complex, trig:k)
```

## Arguments

* `handle:CsnArr`: the array to append to.
* `value:i / value:k / value:Complex`: the element to append.
* `trig:k`: k-rate trigger. Nothing is appended on a zero trigger.

## Output

None. The array grows by one element.

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
; csnpush.csd
;
; csnempty reserves the capacity, csnpush fills it. Pushing up to the reserved
; extent costs no allocation; past it the array grows geometrically.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    cap:i[]     = fillarray(4)
    buf:CsnArr  = csnempty(cap)

    csnpush(buf, 10)
    csnpush(buf, 20)
    csnpush(buf, 30)

    n:i         = csnsize(buf)
    buf_out:i[] = csntoarray(buf)
    prints("n = %d, values = %g %g %g\n", n, buf_out[0], buf_out[1], buf_out[2])

    ; past the reservation the array simply grows
    csnpush(buf, 40)
    csnpush(buf, 50)
    grown_n:i   = csnsize(buf)
    grown:i[]   = csntoarray(buf)
    prints("n = %d, last = %g\n", grown_n, grown[4])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnpop](csnpop.md)
* [csnempty](csnempty.md)
* [csninsert](csninsert.md)
* [csnconcat](csnconcat.md)

## Credits

Pasquale Mainolfi, 2026
