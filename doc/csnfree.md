# csnfree

## Abstract

Release the array behind a handle.

## Description

Most csnum arrays need no explicit release: the array a handle names is freed
when the opcode instance that produced it is deallocated, at the end of its note.

Handles declared `@global` are the exception. They outlive their note on purpose
— that is what makes them useful across instruments — so nothing frees them
automatically. `csnfree` is the escape hatch: it releases the array and returns
the registry slot.

After the release the handle names nothing. Reading it again is an error, so free
a global handle from an instrument that runs after every consumer, at the end of
the piece or on an explicit "reset" note.

## Syntax

```csound
csnfree(handle:CsnArr)
```

## Arguments

* `handle:CsnArr`: the handle whose array is released.

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
; csnfree.csd
;
; A @global array outlives its note, so nothing frees it automatically. csnfree
; releases it, from an instrument that runs after every consumer.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

cap@global:i[]       = fillarray(8)
buffer@global:CsnArr = csnzeros(cap)

instr 1
    ; producer: fill the shared buffer
    cell:i[] = fillarray(0)
    csnset(buffer, cell, 42)
    size:i   = csnsize(buffer)
    prints("instr 1: size = %d\n", size)
    turnoff
endin

instr 2
    ; consumer: the array is still there, notes later
    cell:i[] = fillarray(0)
    value:i  = csnget(buffer, cell)
    prints("instr 2: buffer[0] = %g\n", value)
    turnoff
endin

instr 99
    ; after every consumer, release it
    csnfree(buffer)
    prints("instr 99: buffer released\n")
    turnoff
endin

</CsInstruments>
<CsScore>
i 1  0   0.1
i 2  0.2 0.1
i 99 0.4 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csncopy](csncopy.md)
* [csnempty](csnempty.md)
* [csnzeros](csnzeros.md)

## Credits

Pasquale Mainolfi, 2026
