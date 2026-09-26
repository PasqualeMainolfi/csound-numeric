# csnsosfilter

## Abstract

Filters an array along an axis, or an audio signal, with a cascade of second-order sections.

## Description

`csnsosfilter` runs every 1-D slice of `source` along `axis` through the
second-order sections of `sos` in cascade, as `scipy.signal.sosfilt` does,
each section in transposed direct form II. `sos` is an `(n, 6)` array with one
`[b0 b1 b2 a0 a1 a2]` row per section, as the `*sos` design opcodes return
it; every `a0` must be exactly 1, as scipy requires.

The output has the shape of the source. It is complex when the source or any
coefficient is complex; the audio form takes real sections only.

This is the form to use for anything beyond the lowest orders: each section
holds one pair of poles, so the cascade stays as accurate as the poles
themselves.

At init the filter starts from rest, every slice along the axis on its own,
as scipy does. The k-rate forms keep the state of every slice from one pass
to the next: each pass filters the source as the next block of one continuous
signal, as `lfilter` with `zi` set to the previous call's `zf` would. A pass
whose trigger is 0, or whose source has not changed, publishes the previous
result without advancing the state. A change in the number of slices restarts
the filter from rest; on a real-time path the state cannot grow past what
init reserved.

The audio form filters one signal sample by sample and keeps its state
across control periods.

The coefficients are copied at init. Rewriting them afterwards is an error,
since the state already holds the old filter; design a new one and start a
new note instead.

## Syntax

```csound
handle:CsnArr = csnsosfilter(sos:CsnArr, source:CsnArr)
handle:CsnArr = csnsosfilter(sos:CsnArr, source:CsnArr, axis:i)
handle:CsnArr = csnsosfilter(sos:CsnArr, source:CsnArr, trig:k)
handle:CsnArr = csnsosfilter(sos:CsnArr, source:CsnArr, trig:k, axis:i)
aout csnsosfilter sos:CsnArr, ain
```

## Arguments

* `sos:CsnArr`: a real or complex `(n, 6)` array of second-order sections, `sos[:, 3] = 1`.
* `source:CsnArr`: the array to filter, real or complex, of any rank.
* `axis:i` (optional): the axis to filter along. Omit it for the last axis; negative values count from the end.
* `trig:k`: k-rate trigger; a zero trigger republishes the previous result and leaves the state where it is.
* `ain`: the audio signal to filter.

## Output

* `handle:CsnArr`: the filtered array, the shape of the source.
* `aout`: the filtered audio signal.

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
; csnsosfilter.csd
;
; A filter given as second-order sections applied to an array along an axis,
; as scipy.signal.sosfilt, and to an audio signal. The sections run in
; cascade, each in transposed direct form II; sos[:, 3] must be 1.
; -----------------------------------------------------------------------------

sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

instr 1
    ; eighth-order elliptic bandpass, 500 to 700 Hz: stable as sections
    iband[] = fillarray(500, 700)
    sos:CsnArr = csnellipsos(8, iband, 1, 60, 2, sr)

    ; two channels of a step, one per row, filtered along the last axis
    istep[] = fillarray(1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0)
    ishape[] = fillarray(2, 8)
    flat:CsnArr = csnfromarray(istep)
    x:CsnArr = csnreshape(flat, ishape)
    y:CsnArr = csnsosfilter(sos, x)
    csnprint(y)
    turnoff
endin

instr 2
    iband[] = fillarray(500, 700)
    sos:CsnArr = csnellipsos(8, iband, 1, 60, 2, sr)
    anoise = noise(0.3, 0)
    afilt = csnsosfilter(sos, anoise)
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

* [csnlfilter](csnlfilter.md)
* [csnbuttersos](csnbuttersos.md)
* [csnellipsos](csnellipsos.md)
* [csnzpktosos](csnzpktosos.md)

## Credits

Pasquale Mainolfi, 2026
