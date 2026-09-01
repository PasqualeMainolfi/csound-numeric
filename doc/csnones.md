# csnones

## Abstract

Create an array of ones with a given shape.

## Description

`csnones` allocates a csnum array of the requested shape and publishes every
element at `1`. It is the companion of [csnzeros](csnzeros.md) and takes the
same arguments: a shape array, one extent per dimension, and an optional element
type.

`itype` is read at init, so the element type is fixed for the life of the note.
For a complex array every element is `1 + 0i`.

## Syntax

```csound
handle:CsnArr = csnones(shape:i[])
handle:CsnArr = csnones(shape:i[], itype:i)
handle:CsnArr = csnones(shape:k[])
handle:CsnArr = csnones(shape:k[], itype:i)
```

## Arguments

* `shape:i[] / shape:k[]`: one extent per dimension.
* `itype:i` (optional, default `0`): `0` for a real array, `1` for a complex one.

## Output

* `handle:CsnArr`: handle of the new array.

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
; csnones.csd
;
; csnones fills a shape with ones. Combined with csnmul it is the shortest way
; to a constant array, and it is the natural neutral element for csnprod.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    shape:i[]    = fillarray(5)
    ones:CsnArr  = csnones(shape)
    ones_out:i[] = csntoarray(ones)
    prints("ones    = %g %g %g %g %g\n", ones_out[0], ones_out[1], ones_out[2], ones_out[3], ones_out[4])

    ; a constant array of 0.25, without a second constructor
    gain:CsnArr  = csnmul(ones, 0.25)
    gain_out:i[] = csntoarray(gain)
    prints("scaled  = %g %g %g %g %g\n", gain_out[0], gain_out[1], gain_out[2], gain_out[3], gain_out[4])

    product:i    = csnprod(ones)
    prints("product = %g\n", product)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnzeros](csnzeros.md)
* [csnfull](csnfull.md)
* [csnempty](csnempty.md)
* [csnlike](csnlike.md)

## Credits

Pasquale Mainolfi, 2026
