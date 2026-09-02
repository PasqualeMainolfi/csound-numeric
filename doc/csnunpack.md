# csnunpack

## Abstract

Spread a channels x ksmps array back into an array of audio signals.

## Description

`csnunpack` is the inverse of `csnpack`. It reads a `channels x ksmps` array
and writes each row into its own audio signal.

The output array is allocated at init from the channel count found in the source,
and never during performance. A source whose channel count changes afterwards is
refused, since honouring it would mean allocating on the audio thread.

The source handle is resolved afresh on every control period, so an array that
is freed or replaced under this opcode raises an error rather than reading
memory that no longer belongs to it.

## Syntax

```csound
asigs:a[] = csnunpack(source:CsnArr)
```

## Arguments

* `source:CsnArr`: a two-dimensional real array of shape `channels x ksmps`.

## Output

* `asigs:a[]`: one audio signal per row of the source array.

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
; csnunpack.csd
;
; csnunpack spreads a channels x ksmps CsnArr back into an array of audio
; signals. The channel count is read from the array at init and fixed from
; there on, so the output array is allocated once and never during performance.
;
; Note the k-rate gain below: with a bare constant Csound would pick the i-rate
; overload of csnmul, which runs once at init when the frame is still silent.
; -----------------------------------------------------------------------------

sr = 48000
ksmps = 32
nchnls = 2
0dbfs = 1

instr 1
    kGain init 0.25

    aL oscili 0.8, 330
    aR oscili 0.4, 660

    ins:a[] init 2
    ins[0] = aL
    ins[1] = aR

    frame:CsnArr   = csnpack(ins)
    quieter:CsnArr = csnmul(frame, kGain)
    outs:a[] csnunpack quieter

    if timeinstk() == 1 then
        shape:i[] = csnshape(frame)
        prints("unpacking %d channels of %d samples\n", shape[0], shape[1])
    endif

    kInL  maxk ins[0], 1, 1
    kOutL maxk outs[0], 1, 1
    kOutR maxk outs[1], 1, 1
    if timeinstk() == 40 then
        printf("left  in %.3f -> out %.3f\n", 1, kInL, kOutL)
        printf("right out %.3f (its own channel, not the left one)\n", 1, kOutR)
    endif
endin

</CsInstruments>
<CsScore>
i 1 0 0.05
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnpack](csnpack.md)
* [csntoaudio](csntoaudio.md)

## Credits

Pasquale Mainolfi, 2026
