# csnhilbert1d

## Abstract

Compute the analytic signal of a real array along one axis.

## Description

`csnhilbert1d` returns the analytic signal `a = x + i H(x)`, matching
`scipy.signal.hilbert`. Despite the name, both return the analytic signal and
not the Hilbert transform on its own: the transform is the imaginary part, and
the real part is the source unchanged.

> This is not Csound's `hilbert` opcode. That one is a twelve-pole allpass
> network running on an audio signal, sample by sample, and it returns two
> signals about ninety degrees apart over a usable band. This works on a whole
> array at once, is exact inside the block rather than approximate over a band,
> and carries no state between calls.

It is computed by suppressing the negative half of the spectrum, which is what
makes the result complex:

```
a = ifft(fft(x) * h),   h[0] = h[N/2] = 1,  h[1..N/2-1] = 2,  h[N/2+1..N-1] = 0
```

The transform length is the extent of the axis, never a padded one. Zero-padding
would not extend the answer, it would change every sample of it, because the
Hilbert transform is global: each output depends on the whole input. For the
same reason the axis extent must be even, which is the one condition the
non-power-of-two transform imposes; an odd extent is refused rather than padded.

The input must be real. The analytic signal of a complex array is not defined,
since the negative half of its spectrum is not the redundant mirror of the
positive one and discarding it would destroy information rather than remove
duplication.

The axis is fixed at initialization, and the k-rate form refuses a source whose
rank, axis or extents differ from the ones it was set up for: the working
buffers are sized once and never reallocated on the performance path.

Amplitude envelope, instantaneous phase and instantaneous frequency all follow
from the result with `csnabs`, `csnangle`, `csnunwrap` and `csndiff`.

## Syntax

```csound
analytic:CsnArr = csnhilbert1d(source:CsnArr)
analytic:CsnArr = csnhilbert1d(source:CsnArr, axis:i)
analytic:CsnArr = csnhilbert1d(source:CsnArr, axis:i, trig:k)
```

## Arguments

* `source:CsnArr`: a real array whose transformed axis has an even extent of at least two.
* `axis:i` (optional, default `-1`): axis to transform; `-1` is the last axis.
* `trig:k` (optional, default `1`): k-rate trigger. Zero republishes the previous result.

## Output

* `analytic:CsnArr`: a complex array shaped like the source.

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
    analytic:CsnArr = csnhilbert1d(source)
    ; The real part is the source itself; the imaginary part is the transform.
    csnprint csnreal(analytic)
    csnprint csnimag(analytic)
    ; Amplitude envelope and instantaneous phase come straight off it.
    csnprint csnabs(analytic)
    turnoff
endin
</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnhilbert1dr](csnhilbert1dr.md)
* [csnhilbert2](csnhilbert2.md)
* [csnabs](csnabs.md)
* [csnangle](csnangle.md)

## Credits

Pasquale Mainolfi, 2026
