# csnstream

## Abstract

Overlap-add a stream of frames back into a continuous audio signal.

## Description

`csnstream` is the other end of `csnsnap`. Each incoming frame is added into an
accumulator at the current write position, the position advances by `ihop`, and
`ksmps` samples are handed out per control period. Each sample is cleared as it
leaves, so the accumulator cell restarts from zero when a later frame reaches it.

With a rectangular window and `ihop` equal to the frame length the frames tile
the signal and the reconstruction is exact. At 50% overlap every output sample
is covered by two frames, so a rectangular window doubles the amplitude; a real
analysis chain uses a window whose overlapped copies sum to one.

### One frame per hop, counted here

A frame is folded in once per `ihop` samples of output, on a phase accumulator
this opcode keeps itself, and only when the source has actually been rewritten.

The clock is the important half. A version test alone would not do: every k-rate
producer bumps its output's data version on every pass it writes, whether or not
the contents changed, so a single `csnmul` between `csnsnap` and `csnstream`
would make every control period look like a new frame and fold each one
`ihop / ksmps` times. The phase counter is immune to what sits in between. The
version test still earns its place at the other extreme: a producer that stops
publishing bumps nothing, and without it the last frame would be folded forever.

The accumulator carries the remainder rather than resetting to zero, so a hop
that is not a multiple of `ksmps` keeps the correct average rate instead of
drifting.

## Syntax

```csound
asig:a, kready:k = csnstream(source:CsnArr, ihop:i)
```

## Arguments

* `source:CsnArr`: a one-dimensional real array of frames, typically from [csnsnap](csnsnap.md) after processing. The frame length is read at init.
* `ihop:i`: samples between one frame and the next. Must be at least `ksmps` and at most the frame length.

## Output

* `asig:a`: the reconstructed audio signal. Silence while the buffer is still filling.
* `kready:k`: `1` once the output is real audio, `0` during the initial fill.

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
; csnstream.csd
;
; csnstream is the other end of csnsnap: it overlap-adds a stream of frames
; back into a continuous audio signal. It folds in one frame per hop samples of
; output, counted on its own clock, so any number of k-rate opcodes may sit
; between the two without changing the result.
;
; With a rectangular window and hop == frame the reconstruction is exact. At
; 50% overlap every sample is covered twice, so the amplitude doubles; a real
; analysis chain applies a window whose overlapped copies sum to one.
; -----------------------------------------------------------------------------

sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

instr 1
    aSig oscili 0.5, 440

    frame:CsnArr, kNew csnsnap aSig, 128, 128
    aOut, kOk csnstream frame, 128

    ; hop == frame: the frames tile the signal without overlapping, so the sum
    ; is the signal itself and the amplitude comes back unchanged. There is a
    ; fixed latency, so compare levels rather than sample against sample.
    kIn  maxk aSig, 1, 1
    kOut maxk aOut, 1, 1
    if timeinstk() == 60 then
        printf("hop == frame: in %.2f -> out %.2f\n", 1, kIn, kOut)
    endif
endin

instr 2
    aSig oscili 0.5, 440

    frame:CsnArr, kNew csnsnap aSig, 128, 64
    aOut, kOk csnstream frame, 64

    kIn  maxk aSig, 1, 1
    kAmp maxk aOut, 1, 1
    if timeinstk() == 60 then
        printf("50%% overlap, rectangular window: in %.2f -> out %.2f\n", 1, kIn, kAmp)
    endif
endin

</CsInstruments>
<CsScore>
i 1 0 0.05
i 2 0.1 0.05
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnsnap](csnsnap.md)
* [csntoaudio](csntoaudio.md)

## Credits

Pasquale Mainolfi, 2026
