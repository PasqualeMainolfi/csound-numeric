# csnzsosfilter

## Abstract

Filters with second-order sections from a given state, and returns the final state.

## Description

`csnzsosfilter` is [csnsosfilter](csnsosfilter.md) with the filter state in the open, as
`scipy.signal.sosfilt(sos, x, axis, zi) -> (y, zf)`: the filter starts from the state `zi` instead of from rest,
and returns its state after the last sample as `zf`.

`zi` has the layout scipy gives it: `(n_sections, ...)` followed by the shape
of the source with the filtered axis replaced by 2, two states per section.
For a 1-D source that is `(n_sections, 2)`; for a `(3, 160)` source filtered
along axis 1, `(n_sections, 3, 2)`.

Nothing is kept between calls. Every call, and every pass of the k-rate and
audio forms, starts from the `zi` it is given and returns the final state as
`zf`; feeding `zf` back in as the next `zi` filters successive blocks as one
continuous signal. `zi` may be the opcode's own `zf` output, which is the
usual way to write that feedback: it is read before either output is
written.

At k-rate a pass whose trigger is 0, or in which neither the source nor `zi`
has changed, publishes the previous results. At audio rate `zi` is read at
the start of every control period and `zf` rewritten at its end, in place.

The coefficients are copied at init and may not change afterwards. The
output and `zf` are complex when the source, `zi` or any coefficient is; the
audio form takes real coefficients and a real `zi`.

## Syntax

```csound
y:CsnArr, zf:CsnArr csnzsosfilter sos:CsnArr, source:CsnArr, zi:CsnArr
y:CsnArr, zf:CsnArr csnzsosfilter sos:CsnArr, source:CsnArr, zi:CsnArr, axis:i
y:CsnArr, zf:CsnArr csnzsosfilter sos:CsnArr, source:CsnArr, zi:CsnArr, trig:k
y:CsnArr, zf:CsnArr csnzsosfilter sos:CsnArr, source:CsnArr, zi:CsnArr, trig:k, axis:i
aout, zf:CsnArr csnzsosfilter sos:CsnArr, ain, zi:CsnArr
```

## Arguments

* `sos:CsnArr`: a real or complex `(n, 6)` array of second-order sections, `sos[:, 3] = 1`.
* `source:CsnArr`: the array to filter, real or complex, of any rank.
* `zi:CsnArr`: the initial state, in the layout above.
* `axis:i` (optional): the axis to filter along. Omit it for the last axis; negative values count from the end.
* `trig:k`: k-rate trigger; a zero trigger republishes the previous results.
* `ain`: the audio signal to filter.

## Output

* `y:CsnArr` / `aout`: the filtered array or signal.
* `zf:CsnArr`: the final state, in the layout of `zi`.

## Execution Time

* Init
* Performance (k-rate)
* Performance (audio)

## Examples

```csound
<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnzsosfilter.csd
;
; sosfilt with an explicit state, as scipy.signal.sosfilt(sos, x, zi=zi):
; zi holds two states per section, shape (n_sections, 2) for a 1-D signal,
; and the final state comes back as zf.
; -----------------------------------------------------------------------------

sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

instr 1
    sos:CsnArr = csnbuttersos(4, 1000, 0, sr)

    iblock[] = fillarray(1, 1, 1, 1, 1, 1, 1, 1)
    x:CsnArr = csnfromarray(iblock)
    izshape[] = fillarray(2, 2)
    z0:CsnArr = csnzeros(izshape)
    y1:CsnArr, z1:CsnArr csnzsosfilter sos, x, z0
    y2:CsnArr, z2:CsnArr csnzsosfilter sos, x, z1
    csnprint(y2)
    csnprint(z2)
    turnoff
endin

instr 2
    sos:CsnArr = csnbuttersos(4, 1000, 0, sr)
    izshape[] = fillarray(2, 2)
    z:CsnArr = csnzeros(izshape)
    anoise = noise(0.3, 0)
    afilt, z csnzsosfilter sos, anoise, z
    out afilt
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
i 2 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnsosfilter](csnsosfilter.md)
* [csnzlfilter](csnzlfilter.md)
* [csnbuttersos](csnbuttersos.md)

## Credits

Pasquale Mainolfi, 2026
