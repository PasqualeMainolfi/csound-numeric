# csntoarray

## Abstract

Convert a csnum array handle back into a Csound array.

## Description

`csntoarray` copies the array behind a handle into an ordinary Csound array. It
is the only exit point of the suite: every other opcode returns a handle or a
scalar, so this is where the data becomes something the rest of the orchestra can
index.

**The rank of the output is taken from its declaration, not from the handle.** A
2-D array must be received by an output declared with two bracket pairs; a
mismatch is refused at init rather than silently flattened. Call
[csnflatten](csnflatten.md) first when a flat copy is what you want.

The `name:type` annotation carries a single bracket pair, so an output of rank 2
or more is declared the older way, with the rate letter in the name and one
bracket pair per dimension: `out[][] = csntoarray(handle)`.

A complex array must be received by a `:Complex;[]` output; asking for `i[]` from
a complex handle is an error. Use [csnreal](csnreal.md) or
[csntoreal](csntoreal.md) to drop the imaginary lane on purpose.

## Syntax

```csound
out:i[] = csntoarray(handle:CsnArr)
out:k[] = csntoarray(handle:CsnArr)
out:Complex[] = csntoarray(handle:CsnArr)
```

## Arguments

* `handle:CsnArr`: the array to read.

## Output

* `out:i[] / out:k[] / out:Complex[]`: a Csound array with the handle's shape. Declare it with one bracket pair per dimension.

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
; csntoarray.csd
;
; csntoarray is the way out. The rank of the output comes from its declaration,
; so a 2-D handle needs two bracket pairs; csnflatten covers the other case.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    shape:i[]   = fillarray(2, 3)
    mat:CsnArr  = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6)), shape)

    ; Two bracket pairs for a 2-D handle. The name:type annotation carries only
    ; one rank, so a multidimensional output is declared the older way, with the
    ; rate letter in the name.
    i_mat_out[][] = csntoarray(mat)
    prints("mat[1][2] = %g\n", i_mat_out[1][2])

    ; or flatten first, and receive it as a vector
    flat_out:i[] = csntoarray(csnflatten(mat))
    prints("flat      = %g %g %g %g %g %g\n", flat_out[0], flat_out[1], flat_out[2], flat_out[3], flat_out[4], flat_out[5])

    ; a complex handle needs a :Complex;[] output
    cpx:CsnArr        = csntocomplex(csnflatten(mat))
    cpx_out:Complex[] = csntoarray(cpx)
    z_re:i            = real(cpx_out[3])
    z_im:i            = imag(cpx_out[3])
    prints("z[3]      = %g%+gi\n", z_re, z_im)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnfromarray](csnfromarray.md)
* [csntoftable](csntoftable.md)
* [csnflatten](csnflatten.md)
* [csnshape](csnshape.md)

## Credits

Pasquale Mainolfi, 2026
