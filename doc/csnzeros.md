# csnzeros

## Abstract

Create an array of zeros with a given shape.

## Description

`csnzeros` allocates a csnum array of the requested shape and publishes every
element at `0`. The shape is an ordinary Csound array, one extent per dimension,
so the same opcode builds a vector, a matrix, or any rank up to 8.

The optional `itype` argument chooses the element type: `0` (the default) for a
real array, `1` for a complex one. It is always read at init, so a handle keeps
its element type for the whole note; only the shape may follow a k-rate value.

The k-rate form re-allocates only when the requested shape actually changes, and
rewrites the elements on every pass, so a slot that was overwritten downstream
is restored to zero.

## Syntax

```csound
handle:CsnArr = csnzeros(shape:i[])
handle:CsnArr = csnzeros(shape:i[], itype:i)
handle:CsnArr = csnzeros(shape:k[])
handle:CsnArr = csnzeros(shape:k[], itype:i)
```

## Arguments

* `shape:i[] / shape:k[]`: one extent per dimension. `fillarray(4)` is a 4-element vector, `fillarray(2, 3)` a 2×3 matrix.
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
; csnzeros.csd
;
; csnzeros reserves a shape and publishes every element at zero. The shape is an
; ordinary Csound array, so the same call builds a vector or a matrix.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    vec_shape:i[] = fillarray(4)
    vec:CsnArr    = csnzeros(vec_shape)
    vec_out:i[]   = csntoarray(vec)
    prints("vector = %g %g %g %g\n", vec_out[0], vec_out[1], vec_out[2], vec_out[3])

    mat_shape:i[] = fillarray(2, 3)
    mat:CsnArr    = csnzeros(mat_shape)
    size:i        = csnsize(mat)
    dims:i        = csndims(mat)
    prints("matrix size = %d, dims = %d\n", size, dims)

    ; the same shape, complex this time
    cpx:CsnArr    = csnzeros(vec_shape, 1)
    itype:i       = csntype(cpx)
    prints("complex itype = %d\n", itype)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnones](csnones.md)
* [csnfull](csnfull.md)
* [csnempty](csnempty.md)
* [csnlike](csnlike.md)
* [csnidentity](csnidentity.md)

## Credits

Pasquale Mainolfi, 2026
