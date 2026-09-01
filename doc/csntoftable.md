# csntoftable

## Abstract

Write a csnum array into a function table, optionally resizing it.

## Description

`csntoftable` copies the elements of a real array into an existing function
table, in flat order. It is the exit point to use when the data is going to be
read by an oscillator, a `table` opcode or anything else that speaks in table
numbers.

With `resize = 0` (the default) the table must already be at least as long as
the array; a shorter table is refused rather than truncating the data. With
`resize = 1` the table is re-allocated to exactly the array's element count
first.

Table number `0` and `-1` both name Csound's global sine table, so a
non-positive number is refused rather than overwriting the sine every oscillator
reads. Complex arrays and empty arrays are refused too.

## Syntax

```csound
csntoftable(handle:CsnArr, ftable:i)
csntoftable(handle:CsnArr, ftable:i, resize:i)
```

## Arguments

* `handle:CsnArr`: the real array to write out. It is read flat, whatever its rank.
* `ftable:i`: the destination table number; must be greater than 0.
* `resize:i` (optional, default `0`): `0` to write into the table as it is, `1` to re-allocate it to the array's element count first.

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
; csntoftable.csd
;
; csntoftable hands an array back to the table world. resize = 1 sizes the
; table to the array, so a computed window does not need a matching f-statement.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    ; a Kaiser window computed here, then published as table 2
    win:CsnArr = csnkaiser(16, 8)
    csntoftable(win, 2, 1)

    len:i      = ftlen(2)
    centre:i   = table(8, 2)
    edge:i     = table(0, 2)
    prints("table 2: len = %d, centre = %.4f, edge = %.4f\n", len, centre, edge)

    ; writing into a table that already exists, without resizing
    ramp:CsnArr = csnlinspace(0, 1, 8)
    csntoftable(ramp, 3)
    first:i    = table(0, 3)
    last:i     = table(7, 3)
    prints("table 3: first = %g, eighth = %g\n", first, last)
    turnoff
endin

</CsInstruments>
<CsScore>
f 3 0 8 2 0
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnfromftable](csnfromftable.md)
* [csntoarray](csntoarray.md)
* [csnkaiser](csnkaiser.md)

## Credits

Pasquale Mainolfi, 2026
