# csnload

## Abstract

Read an array from a `.csn` or NumPy `.npy` file.

## Description

`csnload` selects the format from the path extension and publishes a handle
with the shape and real/complex type read from the file. A `.csn` path reads
csnum's own format. A `.npy` path reads NumPy format versions 1.0, 2.0 and 3.0.
Only these two extensions are accepted.

For `.npy`, the supported dtypes are `bool`, signed/unsigned integers of
1/2/4/8 bytes, `float16`/`float32`/`float64`, and `complex64`/`complex128`.
Little, big and native byte order are handled. Real values become `double`;
complex values become pairs of `double`. Integer values outside the exact
range of `double` may lose precision. A Fortran-order file is reordered into
csnum's C-order layout without changing its shape.

Structured and object dtypes, scalar arrays (`shape=()`), and headers over
10,000 bytes are unsupported. csnum allows at most eight dimensions and
2^28 elements. Malformed headers, unsupported types and incomplete payloads
are rejected. See [csnsave](csnsave.md) for the `.npy` type and order written
by csnum.

At k-rate the trigger is the whole contract: it fires, the file is read. There is
deliberately no caching between triggers, not even on an unchanged path. `csnload`
reads a file it does not own, so the path proves nothing about the bytes behind
it, and a stat-based stamp would only narrow the window — on HFS+, SMB/NFS and
FAT the mtime granularity is one to two seconds, wide enough for a same-size
rewrite to hide in.

Until the first trigger fires the handle still holds the empty array the init
pass published, so a consumer that cannot read an empty extent belongs behind the
trigger too.

## Syntax

```csound
handle:CsnArr = csnload(path:S)
handle:CsnArr = csnload(path:S, trig:k)
```

## Arguments

* `path:S`: the file to read; must end in `.csn` or `.npy`.
* `trig:k`: k-rate trigger. The file is read on a non-zero trigger and left alone on a zero one.

## Output

* `handle:CsnArr`: handle of the array read from the file. Empty until the first k-rate trigger fires.

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
; csnload.csd
;
; csnload restores shape and element type from .csn or .npy. At k-rate the
; trigger is the whole contract, and the handle is empty until it first fires.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    shape:i[]  = fillarray(3, 2)
    src:CsnArr = csnreshape(csnfromarray(array(10, 20, 30, 40, 50, 60)), shape)
    csnsave(src, "csnload_example.csn")
    csnsave(src, "csnload_example.npy")
    prints("written\n")
    turnoff
endin

instr 2
    back:CsnArr     = csnload("csnload_example.csn")
    back_shape:i[]  = csnshape(back)
    back_out:i[]    = csntoarray(csnflatten(back))
    prints("shape = %g x %g, values = %g %g %g %g %g %g\n", back_shape[0], back_shape[1], back_out[0], back_out[1], back_out[2], back_out[3], back_out[4], back_out[5])
    from_npy:CsnArr = csnload("csnload_example.npy")
    npy_out:i[] = csntoarray(csnflatten(from_npy))
    prints("NumPy file: first = %g, last = %g\n", npy_out[0], npy_out[5])
    turnoff
endin

instr 3
    ; k-rate: empty until the trigger fires
    elapsed:k   = timeinsts()
    trig:k      = (elapsed > 0.02 ? 1 : 0)
    live:CsnArr = csnload("csnload_example.npy", trig)
    n:k         = csnsize(live)
    printf("size after trigger = %d\n", trig, n)
endin

</CsInstruments>
<CsScore>
i 1 0   0.1
i 2 0.2 0.1
i 3 0.4 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnsave](csnsave.md)
* [csnisempty](csnisempty.md)
* [csnfromftable](csnfromftable.md)

## Credits

Pasquale Mainolfi, 2026
