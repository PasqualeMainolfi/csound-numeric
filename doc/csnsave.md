# csnsave

## Abstract

Write an array to a `.csn` file.

## Description

`csnsave` writes an array to disk in csnum's own format: a fixed 64-byte header —
a `CSDN` magic, a major and minor version, the element type, the dimension count,
the element count, the shape, and the payload length — followed by the raw
payload.

Everything that matters is therefore stored, not inferred. A `2×3` array comes
back `2×3` from [csnload](csnload.md), and a complex array comes back complex
rather than as twice as many reals.

The path must end in `.csn`; anything else is refused before a file is opened.

At k-rate the trigger is the whole contract: it fires, the file is written.
Unlike the rest of the suite a zero trigger here does not republish a previous
result — there is nothing to republish, so it simply does not touch the disk.

## Syntax

```csound
csnsave(handle:CsnArr, path:S)
csnsave(handle:CsnArr, path:S, trig:k)
```

## Arguments

* `handle:CsnArr`: the array to write.
* `path:S`: destination path; must end in `.csn`.
* `trig:k`: k-rate trigger. The file is written on a non-zero trigger and left alone on a zero one.

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
; csnsave stores the element type and the shape alongside the payload, so the
; round trip through csnload is lossless: a 2 x 3 complex array comes back a
; 2 x 3 complex array.
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

    ; the element type survives too
    cpx:CsnArr    = csntocomplex(csnflatten(mat))
    csnsave(cpx, "csnsave_example_c.csn")
    back_cpx:CsnArr = csnload("csnsave_example_c.csn")
    itype:i       = csntype(back_cpx)
    prints("complex round trip itype = %d\n", itype)
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
