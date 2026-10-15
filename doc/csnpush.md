# csnpush

## Abstract

Append one element at the end of an array.

## Description

`csnpush` adds a value at the end of a 1-D array and raises its element count by
one. It writes into its source and publishes nothing.

Growing is amortised: an array made by [csnempty](csnempty.md) reserves capacity
up front, and pushing into that reservation costs no allocation at all. Only a
push past the reserved extent reallocates, and the array then grows
geometrically. On a [real-time path](csnrtlock.md) that push is refused with a
performance error instead, since a marked array cannot take new storage during
performance.

A complex array takes a `:Complex;` value. [csnpop](csnpop.md) is the inverse.

A `:CsnStr;` takes an `S` value and stores its own copy of it in the slot just
past the end, where a pop left the string it dropped. On a [real-time
path](csnrtlock.md) the k form runs as long as the value fits that slot (a
push after a pop of the same string always does) and stops with a performance
error, changing nothing, when it does not. See [string
arrays](string-arrays.md).

## Syntax

```csound
csnpush(arr:CsnArr, value:i)
csnpush(arr:CsnArr, value:Complex)
csnpush(arr:CsnArr, value:k, trig:k)
csnpush(arr:CsnArr, value:Complex, trig:k)
csnpush(arr:CsnStr, value:S)
csnpush(arr:CsnStr, value:S, trig:k)
```

## Arguments

* `arr:CsnArr / arr:CsnStr`: the array to append to.
* `value:i / value:k / value:Complex / value:S`: the element to append; a string for a string array.
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
