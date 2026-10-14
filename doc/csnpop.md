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

A `:CsnStr;` pops into an `S` variable, which gets a copy. The array only drops
the string from its end without allocating, so the k form is allowed on a
[real-time path](csnrtlock.md). See [string arrays](string-arrays.md).

## Syntax

```csound
value:i = csnpop(arr:CsnArr)
value:Complex = csnpop(arr:CsnArr)
value:k = csnpop(arr:CsnArr, trig:k)
value:Complex = csnpop(arr:CsnArr, trig:k)
value:S = csnpop(arr:CsnStr)
value:S = csnpop(arr:CsnStr, trig:k)
```

## Arguments

* `arr:CsnArr / arr:CsnStr`: the array to pop from.
* `trig:k`: k-rate trigger. Nothing is popped on a zero trigger.

## Output

* `value:i / value:k / value:Complex / value:S`: the removed element; a copy of the string for a string array.

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
