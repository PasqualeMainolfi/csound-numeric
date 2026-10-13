# csntoarray

## Abstract

Convert a `CsnArr` back into a Csound array.

## Description

`csntoarray` copies a `CsnArr` into an ordinary Csound array. It
is the only exit point of the suite: every other opcode returns a `CsnArr` or a
scalar, so this is where the data becomes something the rest of the orchestra can
index.

**The rank of the output is taken from its declaration, not from the `CsnArr`.** A
2-D array must be received by an output declared with two bracket pairs; a
mismatch is refused at init rather than silently flattened. Call
[csnflatten](csnflatten.md) first when a flat copy is what you want.

The `name:type` annotation carries a single bracket pair: `out:i[]` declares an
array of `i`, and its rank comes from what sizes it. An output of rank 2 or more
is therefore either declared the older way, with the rate letter in the name and
one bracket pair per dimension, `iout[][] = csntoarray(arr)`, or declared with
the annotation and given its rank with `init` before the assignment:
`out:i[] = init(2, 3)`, then `out = csntoarray(arr)`. The extents `init` sets
need not match, `csntoarray` replaces them; the number of them does.

A complex array must be received by a `:Complex;[]` output; asking for `i[]` from
a complex array is an error. Use [csnreal](csnreal.md) or
[csntoreal](csntoreal.md) to drop the imaginary lane on purpose.

From a `:CsnStr;` the output is an `S[]` declared with the array's rank. The k
form takes a trigger, since `S[]` has no k counterpart to select it. See [string
arrays](string-arrays.md).

## Syntax

```csound
out:i[] = csntoarray(arr:CsnArr)
out:k[] = csntoarray(arr:CsnArr)
out:Complex[] = csntoarray(arr:CsnArr)
out:S[] = csntoarray(arr:CsnStr)
out:S[] = csntoarray(arr:CsnStr, trig:k)
```

## Arguments

* `arr:CsnArr / arr:CsnStr`: the array to read.
* `trig:k`: string form only; the Csound array is refreshed on a non-zero trigger.

## Output

* `out:i[] / out:k[] / out:Complex[] / out:S[]`: a Csound array with the source's shape. Declare it with one bracket pair per dimension, or give it its rank with `init` first.

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
; so a 2-D array needs two bracket pairs; csnflatten covers the other case.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    shape:i[]   = fillarray(2, 3)
    mat:CsnArr  = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6)), shape)

    ; Two bracket pairs for a 2-D array. The name:type annotation carries only
    ; one rank, so a multidimensional output is declared the older way, with the
    ; rate letter in the name.
    i_mat_out[][] = csntoarray(mat)
    prints("mat[1][2] = %g\n", i_mat_out[1][2])

    ; or keep the annotation, which declares an array of i, and give it its
    ; rank with init before the assignment
    typed_out:i[] = init(2, 3)
    typed_out     = csntoarray(mat)
    prints("typed[1][2] = %g\n", typed_out[1][2])

    ; or flatten first, and receive it as a vector
    flat_out:i[] = csntoarray(csnflatten(mat))
    prints("flat      = %g %g %g %g %g %g\n", flat_out[0], flat_out[1], flat_out[2], flat_out[3], flat_out[4], flat_out[5])

    ; a complex array needs a :Complex;[] output
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
