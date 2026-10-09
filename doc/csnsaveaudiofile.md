# csnsaveaudiofile

## Abstract

Write an array to a 32-bit float `.wav` file.

## Description

`csnsaveaudiofile` writes a real array as audio: a 1-D array is one channel,
a 2-D array of shape `(channels, frames)` is one channel per row, the layout
[csnloadaudiofile](csnloadaudiofile.md) reads back. With `mono` set to 1 the
rows are averaged into a single channel. Samples are written as 32-bit IEEE
float at the given sample rate, without clipping or scaling, so values
outside [-1, 1] survive the round trip.

Only `.wav` is written (in any case of the extension); `.mp3` and `.flac` are
read-only. A complex array, an array of more than two dimensions, and a
sample rate that is not a whole number of Hz greater than 0 are refused.

The k form writes on a non-zero trigger and only then, the whole array each
time. Writing a file is not real-time safe; trigger it sparingly.

## Syntax

```csound
csnsaveaudiofile(path:S, in:CsnArr, fs:i [, mono:i])
csnsaveaudiofile(path:S, in:CsnArr, fs:i, mono:i, trig:k)
```

## Arguments

* `path:S`: the file to write; must end in `.wav`.
* `in:CsnArr`: a real 1-D array, or a 2-D array of shape `(channels, frames)`.
* `fs:i`: the sample rate to record in the file, in Hz.
* `mono:i`: 0 writes one channel per row (default), 1 averages the rows into one.
* `trig:k`: k-rate trigger. The file is written on a non-zero trigger.

## Execution Time

* Init
* Performance (k-rate)

## Examples

See [csnloadaudiofile](csnloadaudiofile.md).

## See also

* [csnloadaudiofile](csnloadaudiofile.md)
* [csnsave](csnsave.md)
* [csntoftable](csntoftable.md)

## Credits

Pasquale Mainolfi, 2026
