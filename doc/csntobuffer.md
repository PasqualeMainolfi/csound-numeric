# csntobuffer

## Abstract

The bytes of a real array in a binary dtype, as NumPy's `a.astype(dtype).tobytes()`.

## Description

`csntobuffer` writes every value of a real `:CsnArr;` as the bytes the
dtype stores it in, and gives them as a real one-dimensional array of whole
numbers from 0 to 255: `csntobuffer(a, "<i2")` on `[1000]` gives `[232 3]`.
A source of more dimensions is flattened in row-major order, as `tobytes`
does, and an empty one gives an empty buffer. The bytes go where numbers
cannot: through [csntobase64](csntobase64.md) into an OSC string, or into a
file header or a network packet.

The dtype is a NumPy dtype string with its byte order spelled out: `|u1` and
`|i1` for one byte, `<` for little-endian and `>` for big-endian before
`u2`, `i2`, `u4`, `i4` (unsigned and signed integers of 2 and 4 bytes),
`f4` and `f8` (IEEE 754 floats of 4 and 8 bytes). The native order `=`, the
shorthands without a byte order (`u1`, `i2`) and the 8-byte integers and
complex types are not read.

Each value must fit the dtype: an integer dtype takes whole numbers in its
range, so `-1` as `|i1` gives `255` and `-1` as `|u1` is refused, and `f4`
takes finite values up to its largest, rounded to the nearest float. NaN and
infinities are refused for every dtype. Where `astype` wraps or truncates
(`300` to `|u1`, `3.5` to `<i2`), the message names the first bad value and
its index, and the output is left as it was. A complex array is refused.
[csnfrombuffer](csnfrombuffer.md) reads the bytes back.

Both k forms take a trigger, and compute at init too. They write the buffer
again only when the source, the dtype or the output changed. The buffer
follows the length of the source, and on a [real-time path](csnrtlock.md) a
pass that needs more room than the init pass gave it stops with a performance
error.

## Syntax

```csound
out:CsnArr = csntobuffer(source:CsnArr, dtype:S)
out:CsnArr = csntobuffer(source:CsnArr, dtype:S, trig:k)
```

## Arguments

* `source:CsnArr`: a real array of any shape.
* `dtype:S`: `|u1`, `|i1`, `<u2`, `>u2`, `<i2`, `>i2`, `<u4`, `>u4`, `<i4`, `>i4`, `<f4`, `>f4`, `<f8` or `>f8`.
* `trig:k`: k-rate trigger, which selects the k form. A zero trigger keeps the previous buffer.

## Output

* `out:CsnArr`: the bytes, a real 1-D array of whole numbers from 0 to 255, `size(source)` times the dtype's width long.

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
; csntobuffer.csd
;
; Four 16-bit PCM samples as the bytes a WAV file stores them, little-endian,
; then as base64 text for an OSC string; and a UDP port in network order.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    samples:i[]    = fillarray(0, 1000, -1000, 32767)
    pcm:CsnArr     = csnfromarray(samples)
    bytes:CsnArr   = csntobuffer(pcm, "<i2")
    csnprint(bytes)
    text:S         = csntobase64(bytes)
    prints("base64 %s\n", text)
    port:CsnArr    = csnfromarray(fillarray(57120))
    wire:CsnArr    = csntobuffer(port, ">u2")
    csnprint(wire)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

```text
CsnArr(shape=(8,), dtype=float64)
[0 0 232 3 24 252 255 127]
base64 AADoAxj8/38=
CsnArr(shape=(2,), dtype=float64)
[223 32]
```

## See also

* [csnfrombuffer](csnfrombuffer.md)
* [csntobase64](csntobase64.md)
* [csnstrencode](csnstrencode.md)
* [csnsave](csnsave.md)

## Credits

Pasquale Mainolfi, 2026
