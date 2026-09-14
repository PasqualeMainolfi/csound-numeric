# csnifftshift

## Abstract

Undo an FFT shift and restore native FFT ordering.

## Description

`csnifftshift` is the inverse of [csnfftshift](csnfftshift.md). It circularly
reorders the selected axis from centred spectral order back to the native order
expected by inverse FFT operations.

The distinction from `csnfftshift` matters for odd lengths: the two shifts
differ by one element. Shape and item type are preserved, and both real and
complex arrays are accepted.

## Syntax

```csound
restored:CsnArr = csnifftshift(source:CsnArr)
restored:CsnArr = csnifftshift(source:CsnArr, axis:i)
restored:CsnArr = csnifftshift(source:CsnArr, trig:k)
restored:CsnArr = csnifftshift(source:CsnArr, trig:k, axis:i)
```

## Arguments

* `source:CsnArr`: a real or complex array in centred order.
* `axis:i / axis:k` (optional): axis to transform. Omit it to use the last axis; negative values count from the end.
* `trig:k` (optional, default `1`): k-rate trigger. Zero republishes the previous result.

## Output

* `restored:CsnArr`: array in native FFT order, with source shape and type preserved.

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
    native:CsnArr = csnfftfreq(7, 1)
    centered:CsnArr = csnfftshift(native)
    restored:CsnArr = csnifftshift(centered)
    out:i[] = csntoarray(restored)
    prints("restored = %g %g %g %g %g %g %g\n", out[0], out[1], out[2], out[3], out[4], out[5], out[6])
    turnoff
endin
</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnfftshift](csnfftshift.md)
* [csnifft](csnifft.md)
* [csnfftfreq](csnfftfreq.md)

## Credits

Pasquale Mainolfi, 2026
