# csnrtlockall

## Abstract

Marks every array created for the rest of the performance. Header only.

## Description

`csnrtlockall` belongs in the orchestra header, and it is refused anywhere
else. From the moment it runs, every array created by any instrument, and by
any user-defined opcode they call, carries the real-time mark and will refuse
to reallocate during performance. It is the whole-orchestra version of what
[csnrtlockblock](csnrtlockblock.md) does for one note.

The restriction to the header is what makes the guarantee worth having. The
header runs once, before any note, so what it declares holds for the entire
performance and no note can be surprised by it. Called from inside an
instrument it would be a per-note intention wearing the name of a global one,
so it says so instead:

```text
csnrtlockall belongs in the orchestra header, where it holds for the whole
performance; inside an instrument use csnrtlockblock, which holds until
csnrtunlockblock or the end of the note
```

There is deliberately **no** way to switch it off again. An orchestra-wide
switch that any one note could turn off would be a guarantee only until
somebody stopped relying on it, and the whole value of declaring it in the
header is that the rest of the file can count on it. Where a particular array
genuinely has to change shape, release that one with
[csnrtunlock](csnrtunlock.md) - a decision about one array, taken where that
array is used, rather than about the policy.

## Syntax

```csound
csnrtlockall
```

## Arguments

* none.

## Output

* none.

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
; csnrtlockall.csd
;
; Declared in the orchestra header, where it holds for the whole performance:
; every array created from here on is marked, in every instrument and inside
; every user-defined opcode. There is no way to switch it back off, and that is
; the point - what one note could turn off, another note would be relying on.
; Release the individual arrays that may move with csnrtunlock.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 4410
0dbfs = 1

csnrtlockall

opcode growing_buffer, k, 0
    trig:k = 1
    source:CsnArr = csnarange(0, 64, 1)
    n:k init 4
    n      = 40
    ; marked like everything else: the mark reaches inside opcodes too
    held:CsnArr = csnhead(source, n, trig)
    csnrtunlock held
    held_n:k = csnsize(held)
    xout held_n
endop

instr 1
    ; a fixed shape is all this asks for, so the mark costs nothing
    buffer:CsnArr = csnzeros(fillarray(16))
    gain:k        = 0.5
    scaled:CsnArr = csnmul(buffer, gain, 1)
    scaled_n:k    = csnsize(scaled)

    ; and one that does need to move, released by hand
    grown:k = growing_buffer()

    printks "fixed %d, released %d\n", 0, scaled_n, grown
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnrtlockblock](csnrtlockblock.md)
* [csnrtunlock](csnrtunlock.md)
* [csnrtlock](csnrtlock.md)
* [csnfromaudio](csnfromaudio.md)

## Credits

Pasquale Mainolfi, 2026
