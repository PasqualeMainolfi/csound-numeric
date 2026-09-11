# csnrtlockstart

## Abstract

Starts marking every array the current note creates, until csnrtlockend.

## Description

`csnrtlockstart` opens a marked section: from that line on, every array this
note creates is marked as belonging to a real-time path, exactly as if
[csnrtlock](csnrtlock.md) had been called on each of them. The section ends at
[csnrtlockend](csnrtlockend.md), or on its own when the note ends.

What it does *not* touch is as important as what it does. Arrays created before
the line are untouched, other instruments are untouched, and a second note of
the same instrument running at the same time has its own section: the section
belongs to the instance that opened it. A section left open therefore costs
nothing to anyone else - it closes with the note that forgot it.

The one boundary worth knowing is that a user-defined opcode runs as its own
instance, so arrays created inside a UDO called from within a section are
**not** marked. Where the intent is to cover everything a performance creates,
UDOs included, [csnrtlockall](csnrtlockall.md) in the orchestra header is the
tool for that.

Ending the section stops new arrays from being marked; it does not unmark the
ones already created. [csnrtunlock](csnrtunlock.md) releases those, one handle
at a time.

## Syntax

```csound
csnrtlockstart
csnrtlockstart(trig:k)
```

## Arguments

* `trig:k` (optional, default `1`): k-rate trigger. A zero trigger leaves the section as it was.

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
; csnrtlockstart.csd
;
; Marks every array created after it, for this note only. What was created
; before is untouched, and so is every other instrument: the section belongs to
; the instance that opened it and ends with csnrtlockend or with the note.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 4410
0dbfs = 1

instr 1
    trig:k     = 1
    source:CsnArr = csnarange(0, 64, 1)

    ; created before the section: free to be resized at k-rate
    before_n:k init 4
    before_n   = 40
    before:CsnArr = csnhead(source, before_n, trig)
    before_size:k = csnsize(before)

    csnrtlockstart

    ; created inside the section: marked, and its producer refuses to resize it
    inside:CsnArr = csnzeros(fillarray(8))
    inside_n:k    = csnsize(inside)

    csnrtlockend

    ; created after the section: free again
    after_n:k init 4
    after_n    = 24
    after:CsnArr  = csnhead(source, after_n, trig)
    after_size:k  = csnsize(after)

    printks "before %d, inside %d, after %d\n", 0, before_size, inside_n, after_size
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnrtlockend](csnrtlockend.md)
* [csnrtlockall](csnrtlockall.md)
* [csnrtlock](csnrtlock.md)
* [csnrtunlock](csnrtunlock.md)

## Credits

Pasquale Mainolfi, 2026
