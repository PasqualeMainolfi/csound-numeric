# csnloadaudiofile

## Abstract

Read a `.wav`, `.mp3` or `.flac` audio file into an array.

## Description

`csnloadaudiofile` decodes the whole file and returns its samples as a real
`CsnArr`, with one row per channel: a file with one channel gives a 1-D array
of its samples, a file with more gives a 2-D array of shape
`(channels, frames)`, the layout `librosa.load(..., mono=False)` uses. With
`mono` set to 1 the channels are averaged into a single 1-D row instead. The
second output is the file's sample rate; nothing is resampled, so it may
differ from the orchestra's `sr`.

The format is chosen from the extension, `.wav`, `.mp3` or `.flac` in any
case. Every sample format the decoders read becomes a `double` in [-1, 1]
(integer PCM is scaled, float PCM is kept). The decoders are
[dr_wav, dr_mp3 and dr_flac](https://github.com/mackron/dr_libs), compiled
into csnum. A file the decoder cannot open or read is refused by name.

At k-rate the trigger is the whole contract, as for [csnload](csnload.md): it
fires, the file is read, with no caching between triggers. Until the first
trigger fires the output holds the empty array the init pass published and
the sample rate reads 0. Decoding a file is not real-time safe; trigger it
sparingly, or load at init.

## Syntax

```csound
out:CsnArr, fs:i = csnloadaudiofile(path:S [, mono:i])
out:CsnArr, fs:k = csnloadaudiofile(path:S, mono:i, trig:k)
```

## Arguments

* `path:S`: the file to read; must end in `.wav`, `.mp3` or `.flac`.
* `mono:i`: 0 keeps the channels (default), 1 averages them into one.
* `trig:k`: k-rate trigger. The file is read on a non-zero trigger and left alone on a zero one.

## Output

* `out:CsnArr`: the samples, 1-D for one channel, `(channels, frames)` for more. Empty until the first k-rate trigger fires.
* `fs`: the file's sample rate in Hz; 0 until the first k-rate trigger fires.

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
; csnloadaudiofile.csd
;
; csnsaveaudiofile writes a two-channel array as a .wav file and
; csnloadaudiofile reads it back, one row per channel, with its sample rate,
; or averaged into one channel. At k-rate the trigger is the whole contract.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    shape:i[]  = fillarray(2, 4)
    stereo:CsnArr = csnreshape(csnfromarray(array(0.5, -0.25, 1, 0, -0.5, 0.75, 0, 0.25)), shape)
    csnsaveaudiofile("audiofile_example.wav", stereo, 48000)
    prints("written\n")
    turnoff
endin

instr 2
    back:CsnArr, fs:i = csnloadaudiofile("audiofile_example.wav")
    back_shape:i[] = csnshape(back)
    prints("%g channels x %g frames at %g Hz\n", back_shape[0], back_shape[1], fs)
    mixed:CsnArr, mixed_fs:i = csnloadaudiofile("audiofile_example.wav", 1)
    mixed_out:i[] = csntoarray(mixed)
    prints("averaged: %g %g %g %g\n", mixed_out[0], mixed_out[1], mixed_out[2], mixed_out[3])
    turnoff
endin

instr 3
    ; k-rate: empty, and the rate 0, until the trigger fires
    trig:k = (timeinsts() > 0.02 ? 1 : 0)
    live:CsnArr, live_fs:k = csnloadaudiofile("audiofile_example.wav", 0, trig)
    n:k = csnsize(live)
    printf("size after trigger = %d at %d Hz\n", trig, n, live_fs)
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

* [csnsaveaudiofile](csnsaveaudiofile.md)
* [csnload](csnload.md)
* [csnfromftable](csnfromftable.md)

## Credits

Pasquale Mainolfi, 2026
