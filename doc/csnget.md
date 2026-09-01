# csnget

## Abstract

Read one element of an array by its coordinates.

## Description

`csnget` returns a single element, addressed by a Csound array of coordinates —
one per dimension, in order. The number of coordinates must equal
[csndims](csndims.md): a `2×3` matrix is read with two, a vector with one.

A complex array returns a `:Complex;`; a real array returns a plain number, and
the two are distinct overloads chosen by the type you declare for the output.

For a single index into an array read flat, use [csntake](csntake.md); for a
whole strided run, [csngetslice](csngetslice.md).

## Syntax

```csound
value:i = csnget(handle:CsnArr, cell:i[])
value:Complex = csnget(handle:CsnArr, cell:i[])
value:k = csnget(handle:CsnArr, cell:k[])
value:Complex = csnget(handle:CsnArr, cell:k[])
```

## Arguments

* `handle:CsnArr`: the array to read.
* `cell:i[] / cell:k[]`: the coordinates, one per dimension, each within its extent.

## Output

* `value:i / value:k`: the element, for a real array.
* `value:Complex`: the element, for a complex array.

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
; csnget.csd
;
; One coordinate per dimension, in order. The output type picks the overload: a
; complex array is read into a :Complex; and a real one into a number.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    shape:i[]   = fillarray(2, 3)
    mat:CsnArr  = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6)), shape)

    cell:i[]    = fillarray(1, 2)
    value:i     = csnget(mat, cell)
    prints("mat[1][2] = %g\n", value)

    ; one coordinate for a vector
    vec:CsnArr  = csnfromarray(array(10, 20, 30))
    first:i[]   = fillarray(0)
    head:i      = csnget(vec, first)
    prints("vec[0]    = %g\n", head)

    ; a complex array reads back as a :Complex;
    cpx:CsnArr  = csntocomplex(vec)
    z:Complex   = csnget(cpx, first)
    z_re:i      = real(z)
    z_im:i      = imag(z)
    prints("cpx[0]    = %g%+gi\n", z_re, z_im)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnset](csnset.md)
* [csntake](csntake.md)
* [csngetslice](csngetslice.md)
* [csndims](csndims.md)

## Credits

Pasquale Mainolfi, 2026
