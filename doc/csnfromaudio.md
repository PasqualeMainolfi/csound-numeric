# csnfromaudio

## Abstract

Capture one control period of an audio signal into an array.

## Description

`csnfromaudio` is the entry point from audio into the array world. It runs once
per control period, not once per sample, and writes the whole block of `ksmps`
samples into an array whose shape is fixed at init. From there every opcode in
the suite applies, and `csntoaudio` takes the result back out.

Samples outside the note's active window — the ones covered by `ksmps_offset`
at the start and `ksmps_no_end` at the end — are written as zero, so a
sample-accurate note start does not leak the previous block's tail.

### Realtime paths

A malloc on the audio thread is what a dropout sounds like. By default the array
this opcode publishes is marked as belonging to a realtime path, and the mark
travels along the operand edges of every array derived from it. A marked array
refuses to reallocate during performance and raises an error naming the variable
instead.

That refusal only bites where a shape actually changes at k-rate. A chain whose
shapes are settled at init — the usual case — allocates once and never again, so
the mark costs nothing. Where the frames are being harvested for analysis rather
than sent back out to audio, pass `irt = 0` and the derived arrays behave like
any other data array.

## Syntax

```csound
handle:CsnArr = csnfromaudio(asig:a)
handle:CsnArr = csnfromaudio(asig:a, irt:i)
```

## Arguments

* `asig:a`: the audio signal to capture.
* `irt:i` (optional, default 1): `1` marks the array as a realtime audio path, so neither it nor anything derived from it may reallocate during performance; `0` lifts that restriction for arrays derived from it. See *Realtime paths* below.

## Output

* `handle:CsnArr`: a one-dimensional real array of `ksmps` elements, republished every control period.

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
; csnfromaudio.csd
;
; csnfromaudio captures one control period of an audio signal into a CsnArr of
; ksmps elements, once per k-cycle. From there the whole suite applies, and
; csntoaudio takes the result back out.
; -----------------------------------------------------------------------------

sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

instr 1
    kHalf init 0.5

    aSig  oscili 0.5, 440
    block:CsnArr = csnfromaudio(aSig)

    ; an ordinary k-rate array from here on
    quieter:CsnArr = csnmul(block, kHalf)
    aOut  csntoaudio quieter

    ; the block really does hold ksmps samples
    if timeinstk() == 1 then
        shape:i[] = csnshape(block)
        prints("one block = %d samples (ksmps = %d)\n", shape[0], ksmps)
    endif

    kPeak maxk aOut, 1, 1
    if timeinstk() == 40 then
        printf("peak after halving = %.3f\n", 1, kPeak)
    endif
endin

</CsInstruments>
<CsScore>
i 1 0 0.05
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csntoaudio](csntoaudio.md)
* [csnpack](csnpack.md)
* [csnsnap](csnsnap.md)

## Credits

Pasquale Mainolfi, 2026
