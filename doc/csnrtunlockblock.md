# csnrtunlockblock

## Abstract

Closes the block opened by csnrtlockblock.

## Description

`csnrtunlockblock` ends the block [csnrtlockblock](csnrtlockblock.md) opened:
arrays created after it are no longer marked. It closes only a block this same
note opened - called by an instrument that never opened one, it does nothing
rather than interfering with somebody else's - and the same thing happens on
its own when the note ends, so a block left open never outlives its note.

It stops arrays from being marked; it does not unmark the arrays created while
the block was open. Those keep the mark for as long as they live, which is
usually the point of having marked them. To release one, use
[csnrtunlock](csnrtunlock.md) on its handle.

## Syntax

```csound
csnrtunlockblock
csnrtunlockblock(trig:k)
```

## Arguments

* `trig:k` (optional, default `1`): k-rate trigger. A zero trigger leaves the block as it was.

## Output

* none.

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
; csnrtunlockblock.csd
;
; Closes the block csnrtlockblock opened. It stops new arrays from being
; marked; it does not unmark what was created while the block was open, which
; is what csnrtunlock does, one handle at a time.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 4410
0dbfs = 1

instr 1
    trig:k = 1
    csnrtlockblock
    marked:CsnArr = csnzeros(fillarray(4))
    csnrtunlockblock

    ; created after the close, so it is not marked and can grow
    source:CsnArr = csnarange(0, 64, 1)
    n:k init 4
    n      = 40
    free:CsnArr = csnhead(source, n, trig)
    free_size:k = csnsize(free)

    ; the array from inside the block is still marked: this releases that one
    csnrtunlock marked

    printks "after the block: %d\n", 0, free_size
endin

; a block left open closes on its own when the note ends
instr 2
    csnrtlockblock
    prints("2: block opened and never closed\n")
    turnoff
endin

instr 3
    trig:k = 1
    source:CsnArr = csnarange(0, 64, 1)
    n:k init 4
    n      = 32
    later:CsnArr = csnhead(source, n, trig)
    later_size:k = csnsize(later)
    printks "3: unaffected by instr 2, size %d\n", 0, later_size
endin

</CsInstruments>
<CsScore>
i 1 0   0.1
i 2 0.2 0.05
i 3 0.3 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnrtlockblock](csnrtlockblock.md)
* [csnrtunlock](csnrtunlock.md)
* [csnrtlockall](csnrtlockall.md)
* [csnrtlock](csnrtlock.md)

## Credits

Pasquale Mainolfi, 2026
