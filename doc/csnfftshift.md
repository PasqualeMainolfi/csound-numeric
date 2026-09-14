# csnfftshift

## Abstract

Move the zero-frequency position to the centre of an array axis.

## Description

`csnfftshift` circularly reorders one axis so that the zero-frequency component
of an FFT lies in the centre. It is useful for displaying a full spectrum or
the coordinates from [csnfftfreq](csnfftfreq.md) in negative-to-positive order.

The shape and element type are unchanged, and real and complex arrays are both
accepted. For an odd axis length, the one-element asymmetry follows the usual
FFT-shift convention. [csnifftshift](csnifftshift.md) is the exact inverse for
both even and odd lengths.

## Syntax

```csound
shifted:CsnArr = csnfftshift(source:CsnArr)
shifted:CsnArr = csnfftshift(source:CsnArr, axis:i)
shifted:CsnArr = csnfftshift(source:CsnArr, trig:k)
shifted:CsnArr = csnfftshift(source:CsnArr, trig:k, axis:i)
```

## Arguments

* `source:CsnArr`: real or complex source array.
* `axis:i / axis:k` (optional): axis to transform. Omit it to use the last axis; negative values count from the end.
* `trig:k` (optional, default `1`): k-rate trigger. Zero republishes the previous result.

## Output

* `shifted:CsnArr`: reordered array with the same shape and type as `source`.

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
    native:CsnArr = csnfftfreq(8, 0.125)
    centered:CsnArr = csnfftshift(native)
    out:i[] = csntoarray(centered)
    prints("centered = %g %g %g %g %g %g %g %g\n", out[0], out[1], out[2], out[3], out[4], out[5], out[6], out[7])
    turnoff
endin
</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnifftshift](csnifftshift.md)
* [csnfftfreq](csnfftfreq.md)
* [csnfft](csnfft.md)

## Credits

Pasquale Mainolfi, 2026
