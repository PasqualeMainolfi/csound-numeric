# csnshape

## Abstract

Return the extents of an array, one per dimension.

## Description

`csnshape` hands back the extents of an array as an ordinary Csound array of
`csndims` elements: `fillarray(2, 3)` for a `2×3` matrix.

It is the shape argument the constructors take, so the usual idiom for "another
array laid out like this one" is to read the shape and feed it straight to
[csnzeros](csnzeros.md) or [csnfull](csnfull.md) — or, when the element type
should follow too, to use [csnlike](csnlike.md) instead.

For an array made by [csnempty](csnempty.md) the shape reports the reserved
extents even though [csnsize](csnsize.md) reports `0`.

## Syntax

```csound
shape:i[] = csnshape(handle:CsnArr)
shape:k[] = csnshape(handle:CsnArr)
```

## Arguments

* `handle:CsnArr`: the array to query.

## Output

* `shape:i[] / shape:k[]`: a 1-D Csound array holding one extent per dimension.

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
; csnshape.csd
;
; csnshape returns the extents as a plain Csound array, which is exactly what
; the constructors take: read the shape, build a companion.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    shape:i[]     = fillarray(2, 3)
    mat:CsnArr    = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6)), shape)

    got:i[]       = csnshape(mat)
    rank:i        = lenarray(got)
    prints("rank = %d, extents = %g x %g\n", rank, got[0], got[1])

    ; feed it straight back to a constructor
    same:CsnArr   = csnfull(got, 9)
    same_size:i   = csnsize(same)
    same_dims:i   = csndims(same)
    prints("companion: dims = %d, size = %d\n", same_dims, same_size)

    ; a reserved shape survives even though the size is zero
    cap:i[]       = fillarray(5)
    buf:CsnArr    = csnempty(cap)
    buf_shape:i[] = csnshape(buf)
    buf_size:i    = csnsize(buf)
    prints("empty: reserved = %g, size = %d\n", buf_shape[0], buf_size)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csndims](csndims.md)
* [csnsize](csnsize.md)
* [csnreshape](csnreshape.md)
* [csnlike](csnlike.md)

## Credits

Pasquale Mainolfi, 2026
