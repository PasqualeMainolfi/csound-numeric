# csnsnap

## Abstract

Slice an audio stream into overlapping frames of a chosen size.

## Description

`csnsnap` decouples the analysis frame from the control period. Samples arrive
`ksmps` at a time and are held in a ring buffer; every `ihop` samples a frame of
`ifsize` samples is published and `kready` goes to `1` for that one control
period. A consumer that only recomputes when `kready` is set does its work once
per hop rather than once per k-cycle.

The default hop is the frame size, which gives contiguous frames with no
overlap. A smaller hop overlaps them.

### Why the hop cannot be smaller than ksmps

Exactly one frame can be published per control period, because the handle names
one array: if two frames completed within the same period, the second would
overwrite the first before any consumer could read it. A hop below `ksmps` would
require exactly that, so it is refused at init rather than silently dropping
frames. The check is applied after the default is substituted, so omitting the
argument is always legal.

This constraint is also what keeps the ring buffer safe: with `ihop >= ksmps` and
`ihop <= ifsize` the buffer is necessarily at least one frame plus one control
period long, and the writer cannot overtake the reader within a period.

## Syntax

```csound
handle:CsnArr, kready:k = csnsnap(asig:a, ifsize:i)
handle:CsnArr, kready:k = csnsnap(asig:a, ifsize:i, ihop:i)
handle:CsnArr, kready:k = csnsnap(asig:a, ifsize:i, ihop:i, irt:i)
```

## Arguments

* `asig:a`: the audio signal to slice.
* `ifsize:i`: frame length in samples. Independent of `ksmps`.
* `ihop:i` (optional, default `ifsize`): samples between the start of one frame and the next. Must be at least `ksmps` and at most `ifsize`.
* `irt:i` (optional, default 1): marks the frames as a realtime audio path. See [csnfromaudio](csnfromaudio.md).

## Output

* `handle:CsnArr`: a one-dimensional real array of `ifsize` elements, republished every time a frame completes.
* `kready:k`: `1` on the control period where a new frame was published, `0` otherwise.

## Execution Time

* Performance (a-rate)

## Examples

```csound
<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnsnap.csd
;
; csnsnap slices an audio stream into overlapping frames of a size you choose,
; independent of ksmps. It publishes a frame every hop samples and raises its
; ready flag on the control period where that happens, so a consumer can skip
; the passes in between.
;
; The signal here is a ramp whose value is the sample index, so the first
; element of each frame names the sample the frame starts at.
; -----------------------------------------------------------------------------

sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

instr 1
    first:i[] = fillarray(0)

    aRamp line 0, p3, sr * p3

    ; 128-sample frames, a new one every 64 samples: 50% overlap
    frame:CsnArr, kReady csnsnap aRamp, 128, 64

    kStart = csnget(frame, first)
    if kReady == 1 then
        printf("frame ready, starts at sample %.0f\n", timeinstk(), kStart)
    endif
endin

</CsInstruments>
<CsScore>
i 1 0 0.01
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnstream](csnstream.md)
* [csnfromaudio](csnfromaudio.md)

## Credits

Pasquale Mainolfi, 2026
