# csnfrombuffer

## Abstract

A real array from its bytes in a binary dtype, as NumPy's `np.frombuffer(b, dtype)`.

## Description

`csnfrombuffer` reads a real one-dimensional `:CsnArr;` of bytes, whole
numbers from 0 to 255, as the elements of a dtype, and gives their values as
a real one-dimensional array: `csnfrombuffer(b, "<i2")` on `[232 3]` gives
`[1000]`. It reads what [csntobuffer](csntobuffer.md) writes, and the bytes
of a file header, a network packet or an OSC blob decoded with
[csnfrombase64](csnfrombase64.md). An empty source gives an empty array.

The dtype is a NumPy dtype string with its byte order spelled out: `|u1` and
`|i1` for one byte, `<` for little-endian and `>` for big-endian before
`u2`, `i2`, `u4`, `i4` (unsigned and signed integers of 2 and 4 bytes),
`f4` and `f8` (IEEE 754 floats of 4 and 8 bytes). The native order `=`, the
shorthands without a byte order and the 8-byte integers and complex types
are not read.

The length of the source must be a whole number of elements, a multiple of
the dtype's width; NumPy raises there too. A value that is not a byte, a
complex array or one of more dimensions is refused, the message naming the
first bad value and its index, and the output is left as it was. Every
element decodes to a value: a float dtype gives NaN and infinities as they
are, and a signed integer its two's-complement value.

Both k forms take a trigger, and compute at init too. They read the bytes
again only when the source, the dtype or the output changed. The array
follows the length of the source, and on a [real-time path](csnrtlock.md) a
pass that needs more room than the init pass gave it stops with a performance
error.

## Syntax

```csound
out:CsnArr = csnfrombuffer(source:CsnArr, dtype:S)
out:CsnArr = csnfrombuffer(source:CsnArr, dtype:S, trig:k)
```

## Arguments

* `source:CsnArr`: the bytes, a real 1-D array of whole numbers from 0 to 255.
* `dtype:S`: `|u1`, `|i1`, `<u2`, `>u2`, `<i2`, `>i2`, `<u4`, `>u4`, `<i4`, `>i4`, `<f4`, `>f4`, `<f8` or `>f8`.
* `trig:k`: k-rate trigger, which selects the k form. A zero trigger keeps the previous result.

## Output

* `out:CsnArr`: the values, a real 1-D array, `size(source)` divided by the dtype's width long.

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
; csnfrombuffer.csd
;
; The sample rate and byte rate of a WAV header, two little-endian 32-bit
; fields, read from its bytes; and a base64 OSC payload back to 16-bit samples.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    header:i[]     = fillarray(68, 172, 0, 0, 16, 177, 2, 0)
    fields:CsnArr  = csnfrombuffer(csnfromarray(header), "<u4")
    csnprint(fields)
    payload:CsnArr = csnfrombase64("AADoAxj8/38=")
    samples:CsnArr = csnfrombuffer(payload, "<i2")
    csnprint(samples)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

```text
CsnArr(shape=(2,), dtype=float64)
[44100 1.764e+05]
CsnArr(shape=(4,), dtype=float64)
[0 1000 -1000 32767]
```

## See also

* [csntobuffer](csntobuffer.md)
* [csnfrombase64](csnfrombase64.md)
* [csnstrdecode](csnstrdecode.md)
* [csnload](csnload.md)

## Credits

Pasquale Mainolfi, 2026
