# csnlfilter

## Abstract

Filters an array along an axis, or an audio signal, with the transfer function b / a.

## Description

`csnlfilter` applies the filter

```
a[0] y[n] = b[0] x[n] + b[1] x[n-1] + ... - a[1] y[n-1] - a[2] y[n-2] - ...
```

to every 1-D slice of `source` along `axis`, as `scipy.signal.lfilter` does,
in transposed direct form II. `b` and `a` may have different lengths; both
are divided by `a[0]`, which must not be zero.

The output has the shape of the source. It is complex when the source or any
coefficient is complex; the audio form takes real coefficients only.

**Prefer [csnsosfilter](csnsosfilter.md) beyond the lowest orders.** A single
`b / a` loses precision quickly as the order grows, and a filter that is
stable as sections can blow up as one polynomial.

At init the filter starts from rest, every slice along the axis on its own,
as scipy does. The k-rate forms keep the state of every slice from one pass
to the next: each pass filters the source as the next block of one continuous
signal, as `lfilter` with `zi` set to the previous call's `zf` would. A pass
whose trigger is 0, or whose source has not changed, publishes the previous
result without advancing the state. A change in the number of slices restarts
the filter from rest; on a real-time path the state cannot grow past what
init reserved.

The audio form filters one signal sample by sample and keeps its state
across control periods.

The coefficients are copied at init. Rewriting them afterwards is an error,
since the state already holds the old filter; design a new one and start a
new note instead.

## Syntax

```csound
handle:CsnArr = csnlfilter(b:CsnArr, a:CsnArr, source:CsnArr)
handle:CsnArr = csnlfilter(b:CsnArr, a:CsnArr, source:CsnArr, axis:i)
handle:CsnArr = csnlfilter(b:CsnArr, a:CsnArr, source:CsnArr, trig:k)
handle:CsnArr = csnlfilter(b:CsnArr, a:CsnArr, source:CsnArr, trig:k, axis:i)
aout csnlfilter b:CsnArr, a:CsnArr, ain
```

## Arguments

* `b:CsnArr`: a 1-D real or complex array, the numerator coefficients.
* `a:CsnArr`: a 1-D real or complex array, the denominator coefficients, `a[0]` non-zero.
* `source:CsnArr`: the array to filter, real or complex, of any rank.
* `axis:i` (optional): the axis to filter along. Omit it for the last axis; negative values count from the end.
* `trig:k`: k-rate trigger; a zero trigger republishes the previous result and leaves the state where it is.
* `ain`: the audio signal to filter.

## Output

* `handle:CsnArr`: the filtered array, the shape of the source.
* `aout`: the filtered audio signal.

## Execution Time

* Init
* Performance (k-rate)
* Performance (audio)

## Examples

```csound
<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnlfilter.csd
;
; A filter given as b, a applied to an array along an axis, as
; scipy.signal.lfilter, and to an audio signal. Transposed direct form II;
; a[0] need not be 1, the coefficients are divided by it.
; -----------------------------------------------------------------------------

sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

instr 1
    ; second-order Butterworth lowpass at 2 kHz
    b:CsnArr, a:CsnArr csnbutterba 2, 2000, 0, sr

    ; an impulse, filtered at init: its first samples are the impulse response
    iimp[] = fillarray(1, 0, 0, 0, 0, 0, 0, 0)
    x:CsnArr = csnfromarray(iimp)
    y:CsnArr = csnlfilter(b, a, x)
    csnprint(y)
    turnoff
endin

instr 2
    ; the same filter on noise, sample by sample
    b:CsnArr, a:CsnArr csnbutterba 2, 2000, 0, sr
    anoise = noise(0.3, 0)
    afilt = csnlfilter(b, a, anoise)
    out afilt
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
i 2 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnsosfilter](csnsosfilter.md)
* [csnbutterba](csnbutterba.md)
* [csnzpktotf](csnzpktotf.md)

## Credits

Pasquale Mainolfi, 2026
