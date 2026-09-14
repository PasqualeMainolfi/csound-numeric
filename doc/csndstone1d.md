# csndstone1d

## Abstract

Compute the DST-I of a real array along one axis.

## Description

`csndstone1d` applies the discrete sine transform of type I
along one axis of a real array, matching SciPy's definition with `norm=None`:

```
y[k] = 2 sum(n=0..N-1) x[n] sin(pi (n+1) (k+1) / (N+1))
```

The output has the same shape as the input. The transform is carried by a real
FFT over a symmetric extension of length `2 * (N + 1)`, so the cost grows like a
transform of that size rather than like the direct sum.

Any length is accepted. The extension is always even, which is what the
non-power-of-two branch of the FFT requires, and accuracy is the same either
way; a length whose factorization is ragged only costs more time. A single element is accepted.

The input must be real. The axis is fixed at initialization, so the k-rate form
refuses a source whose rank, axis or extents differ from the ones it was set up
for: the working buffers are sized once and are not reallocated on the
performance path.

## Syntax

```csound
coeffs:CsnArr = csndstone1d(source:CsnArr)
coeffs:CsnArr = csndstone1d(source:CsnArr, axis:i)
coeffs:CsnArr = csndstone1d(source:CsnArr, trig:k)
coeffs:CsnArr = csndstone1d(source:CsnArr, trig:k, axis:i)
```

## Arguments

* `source:CsnArr`: a real array.
* `axis:i` (optional): axis to transform. Omit it to use the last axis; negative values count from the end.
* `trig:k` (optional, default `1`): k-rate trigger. Zero republishes the previous result.

## Output

* `coeffs:CsnArr`: a real array shaped like the source.

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
    ; Any length is accepted, powers of two included but not required.
    source:CsnArr = csnfromarray(array(1, 2, 3, 4, 5, 6))
    coeffs:CsnArr = csndstone1d(source)
    csnprint coeffs
    turnoff
endin
</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csndsttwo1d](csndsttwo1d.md)
* [csnmfcc](csnmfcc.md)
* [csnrfft](csnrfft.md)

## Credits

Pasquale Mainolfi, 2026
