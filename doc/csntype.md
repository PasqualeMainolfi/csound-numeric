# csntype

## Abstract

Return the element type of an array: 0 for real, 1 for complex.

## Description

`csntype` reports how the elements of an array are stored: `0` for a real array,
one double per element, `1` for a complex array, two doubles per element.

Use it to branch on data whose provenance is not fixed — an array read from a
`.csn` file, one handed over by another instrument, or the result of an operation
that promotes a real operand to complex. The opcodes that only make sense over
the reals, ordering comparisons, sorting, rounding, the windows, interpolation,
refuse a complex array, and this is how to check before calling one.

## Syntax

```csound
itype:i = csntype(handle:CsnArr)
itype:k = csntype(handle:CsnArr)
itype:k = csntype(handle:CsnArr, trig:k)
```

## Arguments

* `handle:CsnArr`: the array to query.
* `trig:k` (optional, default `1`): k-rate trigger. A zero trigger republishes the previous answer.

## Output

* `itype:i / itype:k`: `0` for a real array, `1` for a complex one.

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
; csntype.csd
;
; csntype tells real from complex. Mixing the two promotes the result, which is
; the case worth checking before calling a real-only opcode.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr  = csnfromarray(array(1, 2, 3, 4))
    cpx:CsnArr   = csntocomplex(data)

    data_type:i  = csntype(data)
    cpx_type:i   = csntype(cpx)
    prints("real handle = %d, complex handle = %d\n", data_type, cpx_type)

    ; an operation that mixes the two promotes the result
    mixed:CsnArr = csnadd(data, cpx)
    mixed_type:i = csntype(mixed)
    prints("real + complex = %d\n", mixed_type)

    ; branch before calling a real-only opcode
    if mixed_type == 0 then
        sorted:CsnArr = csnsort(mixed)
        prints("sorted\n")
    else
        prints("complex: sorting is undefined, taking the real parts instead\n")
        re:CsnArr  = csnreal(mixed)
        re_out:i[] = csntoarray(re)
        prints("real parts = %g %g %g %g\n", re_out[0], re_out[1], re_out[2], re_out[3])
    endif
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
* [csnshape](csnshape.md)
* [csntocomplex](csntocomplex.md)
* [csntoreal](csntoreal.md)

## Credits

Pasquale Mainolfi, 2026
