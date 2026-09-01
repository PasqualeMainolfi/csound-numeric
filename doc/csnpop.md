# csnpop

## Abstract

Remove the last element of an array and return it.

## Description

`csnpop` takes the last element off a 1-D array, lowers its element count by one,
and hands the removed value back. It is the inverse of
[csnpush](csnpush.md), and together the two make a csnum array usable as a stack.

The reserved capacity is not given back, so pushing again after a pop reuses the
room that is already there.

A complex array's pop returns a `:Complex;`. Popping an empty array is an error;
guard it with [csnisempty](csnisempty.md).

## Syntax

```csound
value:i = csnpop(handle:CsnArr)
value:Complex = csnpop(handle:CsnArr)
value:k = csnpop(handle:CsnArr, trig:k)
value:Complex = csnpop(handle:CsnArr, trig:k)
```

## Arguments

* `handle:CsnArr`: the array to pop from.
* `trig:k`: k-rate trigger. Nothing is popped on a zero trigger.

## Output

* `value:i / value:k / value:Complex`: the removed element.

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
; csnpop.csd
;
; csnpush and csnpop together make an array a stack. The capacity stays, so the
; room a pop frees is reused by the next push.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    cap:i[]    = fillarray(4)
    stack:CsnArr = csnempty(cap)

    csnpush(stack, 1)
    csnpush(stack, 2)
    csnpush(stack, 3)

    top:i      = csnpop(stack)
    next:i     = csnpop(stack)
    left:i     = csnsize(stack)
    prints("popped %g then %g, %d left\n", top, next, left)

    ; guard the empty case
    last:i     = csnpop(stack)
    empty:i    = csnisempty(stack)
    prints("popped %g, empty = %d\n", last, empty)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnpush](csnpush.md)
* [csnremove](csnremove.md)
* [csnisempty](csnisempty.md)

## Credits

Pasquale Mainolfi, 2026
