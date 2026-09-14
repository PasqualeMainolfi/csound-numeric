# csnhilbert1dr

## Abstract

Compute the Hilbert transform of a real array along one axis, as a real array.

## Description

`csnhilbert1dr` returns `H(x)` itself, the ninety-degree phase shift of the
source, rather than the analytic signal `csnhilbert1d` builds around it. It is
exactly the imaginary part of that signal, and the two agree to the last bit.

> This is not Csound's `hilbert` opcode. That one is a twelve-pole allpass
> network running on an audio signal, sample by sample, and it returns two
> signals about ninety degrees apart over a usable band. This works on a whole
> array at once, is exact inside the block rather than approximate over a band,
> and carries no state between calls.

Taking it directly is cheaper than taking the analytic signal and discarding the
real part. The spectrum is rotated by `-i` and inverted with a real transform,
which stays real because the rotation preserves the conjugate symmetry a real
source has:

```
Y = -i rfft(x),   Y[0] = Y[N/2] = 0,   H = irfft(Y, N)
```

That is one real transform each way instead of a real one and a complex one,
about forty per cent less work from a few hundred samples up, and the result is
a real array rather than a complex one of the same length.

The transform length is the extent of the axis and is never padded, since the
Hilbert transform is global and padding would change every output sample rather
than extend the answer. The extent must be even. The input must be real.

The axis is fixed at initialization, and the k-rate form refuses a source whose
layout differs from the one it was set up for.

## Syntax

```csound
shifted:CsnArr = csnhilbert1dr(source:CsnArr)
shifted:CsnArr = csnhilbert1dr(source:CsnArr, axis:i)
shifted:CsnArr = csnhilbert1dr(source:CsnArr, axis:i, trig:k)
```

## Arguments

* `source:CsnArr`: a real array whose transformed axis has an even extent of at least two.
* `axis:i` (optional, default `-1`): axis to transform; `-1` is the last axis.
* `trig:k` (optional, default `1`): k-rate trigger. Zero republishes the previous result.

## Output

* `shifted:CsnArr`: a real array shaped like the source.

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
sr = 48000
ksmps = 32
0dbfs = 1

instr 1
    source:CsnArr = csnfromarray(array(1, 2, 3, 4, 5, 6, 7, 8))
    shifted:CsnArr = csnhilbert1dr(source)
    csnprint shifted
    turnoff
endin
</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnhilbert1d](csnhilbert1d.md)
* [csnhilbert2](csnhilbert2.md)

## Credits

Pasquale Mainolfi, 2026
