# csncopy

## Abstract

Make an independent copy of an array.

## Description

`csncopy` allocates a new array with the same shape, element type and contents as
its source, and returns an independent `CsnArr`. The two arrays share nothing afterwards:
writing into one through [csnset](csnset.md), [csnpush](csnpush.md) or any
in-place opcode leaves the other alone.

Assignment also copies: `B = A` and `B = csncopy(A)` both make `B` independent
of `A`. Use `csncopy` when an explicit copy or its triggered k-rate form makes
the intent clearer.

Use it before an in-place operation you want to keep the original for, and to
snapshot an array that is still being written.

A `:CsnStr;` gives a `:CsnStr;` holding its own copies of the strings. Copying
strings allocates, so the k form is refused on a [real-time
path](csnrtlock.md). See [string arrays](string-arrays.md).

## Syntax

```csound
out:CsnArr = csncopy(source:CsnArr)
out:CsnArr = csncopy(source:CsnArr, trig:k)
out:CsnStr = csncopy(source:CsnStr)
out:CsnStr = csncopy(source:CsnStr, trig:k)
```

## Arguments

* `source:CsnArr / source:CsnStr`: the array to copy.
* `trig:k`: k-rate trigger. The copy is retaken on a non-zero trigger; a zero trigger republishes the previous one.

## Output

* `out:CsnArr / out:CsnStr`: the new, independent array.

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
; csncopy.csd
;
; Assignment and csncopy both make independent arrays. An in-place write to
; the source leaves both copies unchanged.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    iValues[] = fillarray(3, 1, 2)
    src:CsnArr      = csnfromarray(iValues)
    assigned:CsnArr = src
    copied:CsnArr   = csncopy(src)

    ; sort src in place: neither copy follows the mutation
    csnsort(src)

    src_out:i[]      = csntoarray(src)
    assigned_out:i[] = csntoarray(assigned)
    copied_out:i[]   = csntoarray(copied)
    prints("src      = %g %g %g\n", src_out[0], src_out[1], src_out[2])
    prints("assigned = %g %g %g\n", assigned_out[0], assigned_out[1], assigned_out[2])
    prints("copied   = %g %g %g\n", copied_out[0], copied_out[1], copied_out[2])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnlike](csnlike.md)
* [csnfromarray](csnfromarray.md)
* [csnk](csnk.md), [csni](csni.md): copies that also set the rate

## Credits

Pasquale Mainolfi, 2026
