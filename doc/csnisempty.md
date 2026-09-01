# csnisempty

## Abstract

Return 1 when an array holds no element.

## Description

`csnisempty` reports `1` when an array has no elements and `0` otherwise. It is
the same test as `csnsize(handle) == 0`, said directly.

An empty array is an ordinary value in csnum, not an error case: the shape
transforms return an empty result of the right rank, concatenation with an empty
operand yields the other one, and [csnsum](csnsum.md) over an empty array is `0`.
A few operations have no answer over an empty extent — `min`, `max`, `sub` and
the variance family — and this is the guard to put in front of them.

It is also the state a k-rate [csnload](csnload.md) sits in until its first
trigger fires.

## Syntax

```csound
empty:i = csnisempty(handle:CsnArr)
empty:k = csnisempty(handle:CsnArr)
```

## Arguments

* `handle:CsnArr`: the array to query.

## Output

* `empty:i / empty:k`: `1` when the array holds no element, `0` otherwise.

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
; csnisempty.csd
;
; An empty array travels through the suite instead of stopping it, but min, max
; and the variance family have no answer over an empty extent. Guard those.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    cap:i[]      = fillarray(4)
    buf:CsnArr   = csnempty(cap)
    before:i     = csnisempty(buf)

    ; safe over an empty array
    total:i      = csnsum(buf)
    prints("empty = %d, sum = %g\n", before, total)

    ; guard the ones that are not
    if before == 0 then
        peak:i = csnmax(buf)
        prints("peak = %g\n", peak)
    else
        prints("no peak: the array is empty\n")
    endif

    csnpush(buf, 7)
    csnpush(buf, 3)
    after:i      = csnisempty(buf)
    peak_now:i   = csnmax(buf)
    prints("after two pushes: empty = %d, peak = %g\n", after, peak_now)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnsize](csnsize.md)
* [csnempty](csnempty.md)
* [csnpush](csnpush.md)

## Credits

Pasquale Mainolfi, 2026
