# csnpack

## Abstract

Fold an array of audio signals into one channels x ksmps array.

## Description

`csnpack` is the multichannel counterpart of `csnfromaudio`: it gathers a whole
`a[]` into a single array laid out one channel per row, so multichannel material
can be handled with the matrix opcodes rather than a signal at a time.

The channel count comes from the input array at init and fixes the shape for the
life of the note; a later change is refused rather than silently reallocating
during performance. Samples outside the note's active window are written as zero
in every channel.

Note that an `a[]` stores each channel as a whole `ksmps`-long block, so the
element at index *i* of the Csound array is channel *i*, not sample *i*.

## Syntax

```csound
handle:CsnArr = csnpack(asigs:a[])
handle:CsnArr = csnpack(asigs:a[], irt:i)
```

## Arguments

* `asigs:a[]`: the audio signals, one per channel. The channel count is read at init and may not change afterwards.
* `irt:i` (optional, default 1): marks the result as a realtime audio path. See [csnfromaudio](csnfromaudio.md).

## Output

* `handle:CsnArr`: a two-dimensional real array of shape `channels x ksmps`, republished every control period.

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
; csnpack.csd
;
; csnpack folds an array of audio signals into one CsnArr shaped
; channels x ksmps, so multichannel material can be processed with the matrix
; opcodes. csnunpack takes it apart again.
; -----------------------------------------------------------------------------

sr = 48000
ksmps = 32
nchnls = 2
0dbfs = 1

instr 1
    a1 oscili 0.5, 220
    a2 oscili 0.5, 440
    a3 oscili 0.5, 880

    ins:a[] init 3
    ins[0] = a1
    ins[1] = a2
    ins[2] = a3

    frame:CsnArr = csnpack(ins)

    if timeinstk() == 1 then
        shape:i[] = csnshape(frame)
        prints("packed shape = %d x %d (channels x ksmps)\n", shape[0], shape[1])
    endif

    outs:a[] csnunpack frame
    aMix = (outs[0] + outs[1] + outs[2]) / 3
    kPeak maxk aMix, 1, 1
    if timeinstk() == 40 then
        printf("peak of the three-way mix = %.3f\n", 1, kPeak)
    endif
endin

</CsInstruments>
<CsScore>
i 1 0 0.05
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnunpack](csnunpack.md)
* [csnfromaudio](csnfromaudio.md)
* [csntoaudio](csntoaudio.md)

## Credits

Pasquale Mainolfi, 2026
