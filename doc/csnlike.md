# csnlike

## Abstract

Create an array shaped and typed like another one, filled with a value.

## Description

`csnlike` reads the shape and the element type of an existing handle and builds a
new array with the same layout, every element set to `value`. Nothing of the
source's data is copied — only its geometry.

It is the way to allocate a companion buffer for an array whose shape you do not
know statically, for instance one that came out of a reshape, a slice, or a file.

The element type follows the source, so `csnlike` of a complex array is complex,
and the real `value` fills the real lane with the imaginary lane at zero. Use
[csncopy](csncopy.md) when the contents matter too.

## Syntax

```csound
handle:CsnArr = csnlike(source:CsnArr, value:i)
handle:CsnArr = csnlike(source:CsnArr, value:k)
```

## Arguments

* `source:CsnArr`: the array whose shape and element type are copied.
* `value:i / value:k`: the value every element of the new array is set to.

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
; csnlike.csd
;
; csnlike copies the geometry of an array, not its data: same shape, same
; element type, every element set to the value you give.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    shape:i[]    = fillarray(2, 3)
    src:CsnArr   = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6)), shape)

    mask:CsnArr  = csnlike(src, 0.5)
    dims:i       = csndims(mask)
    size:i       = csnsize(mask)
    mask_out:i[] = csntoarray(csnflatten(mask))
    prints("dims = %d, size = %d, first = %g\n", dims, size, mask_out[0])

    ; the element type follows the source
    cpx:CsnArr   = csntocomplex(src)
    comp:CsnArr  = csnlike(cpx, 1)
    itype:i      = csntype(comp)
    prints("itype of csnlike(complex) = %d\n", itype)
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
* [csncopy](csncopy.md)
* [csnshape](csnshape.md)

## Credits

Pasquale Mainolfi, 2026
