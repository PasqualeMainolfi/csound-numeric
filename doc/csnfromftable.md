# csnfromftable

## Abstract

Copy a function table into a csnum array handle.

## Description

`csnfromftable` reads the `flen` points of a Csound function table into a fresh
real csnum array and returns its handle. The guard point past the end of the
table is not part of the copy.

The result is always 1-D and always real. Reshape it afterwards with
[csnreshape](csnreshape.md) if the table holds a matrix laid out row by row.

Together with [csnfromarray](csnfromarray.md) this is the way into the suite,
and [csntoftable](csntoftable.md) is the way back to a table an oscillator can
read.

## Syntax

```csound
handle:CsnArr = csnfromftable(ftable:i)
```

## Arguments

* `ftable:i`: the number of an existing function table.

## Output

* `handle:CsnArr`: handle of a new real 1-D array holding the table's `flen` points.

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
; csnfromftable.csd
;
; csnfromftable pulls a function table into the suite. From there the whole
; array vocabulary applies: statistics, windows, normalisation, and back out
; through csntoftable.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    wave:CsnArr = csnfromftable(1)
    n:i         = csnsize(wave)
    peak:i      = csnmax(wave)
    trough:i    = csnmin(wave)
    avg:i       = csnmean(wave)
    prints("points = %d, min = %.3f, max = %.3f, mean = %.6f\n", n, trough, peak, avg)

    ; the copy is 1-D and real, whatever the table held
    dims:i      = csndims(wave)
    itype:i     = csntype(wave)
    prints("dims = %d, itype = %d\n", dims, itype)
    turnoff
endin

</CsInstruments>
<CsScore>
f 1 0 1024 10 1
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csntoftable](csntoftable.md)
* [csnfromarray](csnfromarray.md)
* [csnreshape](csnreshape.md)

## Credits

Pasquale Mainolfi, 2026
