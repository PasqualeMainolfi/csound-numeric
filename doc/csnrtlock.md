# csnrtlock

## Abstract

Mark an array as belonging to a real-time path, or clear that mark.

## Description

`csnrtlock` sets the flag that forbids an array from being reallocated during
performance. An array carrying it, and every array derived from it afterwards,
raises an error naming the variable instead of calling the allocator, because a
malloc on the audio thread is what a dropout sounds like.

The audio sources — [csnfromaudio](csnfromaudio.md), [csnpack](csnpack.md),
[csnsnap](csnsnap.md) — set the mark themselves through their `irt` argument, so
an audio chain needs nothing extra. `csnrtlock` is for the chains that never
touch audio and still run under a deadline: array work driving a synth at
k-rate has the same intolerance for an allocation, and no audio opcode to
inherit the mark from.

### The mark travels forward, and only forward

`csnrtlock` runs at **init**. It reaches the arrays created after it in
orchestra order, and no others:

```csound
src:CsnArr    = csnzeros(shape)
csnrtlock src, 1
padded:CsnArr = csnpad(src, grow, grow, fill, trig)   ; inherits the mark
```

```csound
src:CsnArr    = csnzeros(shape)
padded:CsnArr = csnpad(src, grow, grow, fill, trig)   ; created first, unmarked
csnrtlock src, 1                                       ; too late for padded
```

The second form is not an error and is not reported: `padded` was created before
the mark existed and copied what `src` carried at that moment. Put `csnrtlock`
immediately after the array it protects, before anything reads it.

For the same reason `csnrtlock handle, 0` is not retroactive. It clears the mark
on `handle`, so arrays derived from it *from that point on* are free to
reallocate; arrays already derived keep the copy they took.

### What actually trips it

Only a reallocation during performance, which means a k-rate shape or element
type that genuinely changes from one pass to the next — a growing `csnpad`, a
`csnconcat` whose result lengthens, a `csnzeros` driven by a k-rate shape.

A chain whose shapes are settled at init allocates once per note and never
again, so the mark costs nothing there. A change of layout alone does not
trip it either: `csnreshape` preserves the element count and reuses the buffer.

## Syntax

```csound
csnrtlock(handle:CsnArr, iflag:i)
```

## Arguments

* `handle:CsnArr`: the array to mark.
* `iflag:i`: `1` marks the array as a real-time path, `0` clears the mark. Any other value is refused.

## Output

None.

## Execution Time

* Init

## Examples

```csound
<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnrtlock.csd
;
; csnrtlock marks a handle as belonging to a real-time path: neither it nor any
; array derived from it afterwards may reallocate during performance, because a
; malloc on the audio thread is what a dropout sounds like.
;
; The audio sources set that mark themselves. csnrtlock is for the chains that
; never touch audio but still run under a deadline.
;
; It runs at init, so it only reaches arrays created after it in the orchestra.
; -----------------------------------------------------------------------------

sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

instr 1
    shape:i[] = fillarray(32)
    fill:k    init 0
    trig:k    init 1
    grow:k    = timeinstk() * 16

    src:CsnArr = csnzeros(shape)
    csnrtlock src, 1

    ; the padding grows every pass, so this output would have to be
    ; reallocated: refused, with the variable named
    padded:CsnArr = csnpad(src, grow, grow, fill, trig)
endin

; the same chain without the mark: allocating during performance is allowed
instr 2
    shape:i[] = fillarray(32)
    fill:k    init 0
    trig:k    init 1
    grow:k    = timeinstk() * 16

    src:CsnArr    = csnzeros(shape)
    padded:CsnArr = csnpad(src, grow, grow, fill, trig)

    size:k = csnsize(padded)
    if timeinstk() == 8 then
        printf("unmarked chain still growing: %d elements\n", 1, size)
    endif
endin

</CsInstruments>
<CsScore>
i 1 0   0.01
i 2 0.1 0.01
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnfromaudio](csnfromaudio.md)
* [csnpack](csnpack.md)
* [csnsnap](csnsnap.md)

## Credits

Pasquale Mainolfi, 2026
