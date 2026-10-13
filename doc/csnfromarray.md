# csnfromarray

## Abstract

Convert a Csound array into a `CsnArr`.

## Description

`csnfromarray` copies a Csound `i[]`, `k[]` or `:Complex;[]` array into a fresh
`CsnArr` and returns it. Together with
[csnfromftable](csnfromftable.md) it is the way *into* the suite: every other
opcode exchanges `CsnArr` values, never Csound arrays.

The rank and the extents of the Csound array are preserved, so a `2×3` Csound
array becomes a `2×3` csnum array. A `:Complex;[]` source produces a complex
array; an `i[]` or `k[]` source produces a real one.

The copy is taken at init for the i-rate form, and on every pass for the k-rate
form, so a `k[]` that keeps moving keeps the `CsnArr` in step with it.

From a Csound `S[]` the output is a `:CsnStr;` of the same shape, each element a
copy. An `S[]` has no k counterpart, so the k form takes a trigger; it
allocates, so it is refused on a [real-time path](csnrtlock.md). See [string
arrays](string-arrays.md).

## Syntax

```csound
out:CsnArr = csnfromarray(source:i[])
out:CsnArr = csnfromarray(source:k[])
out:CsnArr = csnfromarray(source:Complex[])
out:CsnStr = csnfromarray(source:S[])
out:CsnStr = csnfromarray(source:S[], trig:k)
```

## Arguments

* `source:i[] / source:k[] / source:Complex[] / source:S[]`: the Csound array to copy.
* `trig:k`: `S[]` form only; the array is copied again on a non-zero trigger.

## Output

* `out:CsnArr / out:CsnStr`: the new csnum array, a `:CsnStr;` from an `S[]`.

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
; CsnArr, and csntoarray is the way back out.
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
