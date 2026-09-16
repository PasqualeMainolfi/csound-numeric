# csnsave

## Abstract

Write an array to a `.csn` or NumPy `.npy` file.

## Description

`csnsave` selects the format from the path extension. A `.csn` path writes
csnum's own format: a fixed 64-byte `CSDN` header with version, real/complex
type, dimension count, shape, element count and payload byte count, followed by
the raw `double` payload. A `.npy` path writes a NumPy 3.0 file with C-order
`float64` or `complex128` values. NumPy's `np.load` can read that file.

Both formats preserve shape and real/complex type on a round trip through
[csnload](csnload.md) when the element count matches the product of the shape.
The `.npy` writer checks this before saving. A logically empty array with a
nonzero reserved shape cannot be saved as `.npy`; an array with a zero-sized
extent can.

Only `.csn` and `.npy` paths are accepted. At k-rate a zero trigger leaves the
disk untouched. On a nonzero trigger, a write can be skipped when the same
array version was already written to the same path by this opcode instance.

## Syntax

```csound
csnsave(handle:CsnArr, path:S)
csnsave(handle:CsnArr, path:S, trig:k)
```

## Arguments

* `handle:CsnArr`: the array to write.
* `path:S`: destination path; must end in `.csn` or `.npy`.
* `trig:k`: k-rate trigger. A zero trigger skips the write; an unchanged array and path may also skip a repeat write.

## Output

None.

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
; csnsave.csd
;
; csnsave chooses csnum's format or NumPy's format by extension. Both preserve
; shape and real/complex type through csnload.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    shape:i[]     = fillarray(2, 3)
    mat:CsnArr    = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6)), shape)
    csnsave(mat, "csnsave_example.csn")

    back:CsnArr   = csnload("csnsave_example.csn")
    dims:i        = csndims(back)
    size:i        = csnsize(back)
    back_out:i[]  = csntoarray(csnflatten(back))
    prints("dims = %d, size = %d, values = %g %g %g %g %g %g\n", dims, size, back_out[0], back_out[1], back_out[2], back_out[3], back_out[4], back_out[5])

    csnsave(mat, "csnsave_example.npy")
    back_npy:CsnArr = csnload("csnsave_example.npy")
    npy_out:i[] = csntoarray(csnflatten(back_npy))
    prints("NumPy round trip: first = %g, last = %g\n", npy_out[0], npy_out[5])

    ; the element type survives too
    cpx:CsnArr    = csntocomplex(csnflatten(mat))
    csnsave(cpx, "csnsave_example_c.csn")
    back_cpx:CsnArr = csnload("csnsave_example_c.csn")
    itype:i       = csntype(back_cpx)
    prints("complex round trip itype = %d\n", itype)
    csnsave(cpx, "csnsave_example_c.npy")
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnload](csnload.md)
* [csntoftable](csntoftable.md)
* [csntoarray](csntoarray.md)

## Credits

Pasquale Mainolfi, 2026
