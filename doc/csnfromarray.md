# csnfromarray

## Abstract

Convert a Csound array into a csnum array handle.

## Description

`csnfromarray` copies a Csound `i[]`, `k[]` or `:Complex;[]` array into a fresh
csnum array and returns its handle. Together with
[csnfromftable](csnfromftable.md) it is the way *into* the suite: every other
opcode exchanges handles, never Csound arrays.

The rank and the extents of the Csound array are preserved, so a `2×3` Csound
array becomes a `2×3` csnum array. A `:Complex;[]` source produces a complex
array; an `i[]` or `k[]` source produces a real one.

The copy is taken at init for the i-rate form, and on every pass for the k-rate
form, so a `k[]` that keeps moving keeps the handle in step with it.

## Syntax

```csound
handle:CsnArr = csnfromarray(source:i[])
handle:CsnArr = csnfromarray(source:k[])
handle:CsnArr = csnfromarray(source:Complex[])
```

## Arguments

* `source:i[] / source:k[] / source:Complex[]`: the Csound array to copy.

## Output

* `handle:CsnArr`: handle of the new csnum array.

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
; csnfromarray.csd
;
; csnfromarray is the way into the suite. From there on everything travels as a
; handle, and csntoarray is the way back out.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr    = csnfromarray(array(4, 1, 3, 2))
    sorted:CsnArr  = csnsort(data)
    sorted_out:i[] = csntoarray(sorted)
    prints("sorted = %g %g %g %g\n", sorted_out[0], sorted_out[1], sorted_out[2], sorted_out[3])

    ; rank and extents survive the trip
    src:i[][]   = init(2, 3)
    src[0][0]   = 1
    src[1][2]   = 6
    mat:CsnArr  = csnfromarray(src)
    dims:i      = csndims(mat)
    size:i      = csnsize(mat)
    prints("dims = %d, size = %d\n", dims, size)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csntoarray](csntoarray.md)
* [csnfromftable](csnfromftable.md)
* [csncopy](csncopy.md)

## Credits

Pasquale Mainolfi, 2026
