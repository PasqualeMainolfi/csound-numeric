# csnmaximum

## Abstract

Elementwise maximum of two arrays, or of an array and a scalar.

## Description

`csnmaximum` compares position by position and keeps the larger of the two
values. It is the elementwise pair of [csnmax](csnmax.md), which instead reduces
one array to its largest element.

The two operands broadcast against each other, so a row can be compared against a
matrix without being materialised first. The second operand may be a scalar.

Real only. NaN is not propagated: where one operand is NaN the other is returned,
following C's `fmax`.

## Syntax

```csound
handle:CsnArr = csnmaximum(a:CsnArr, b:CsnArr)
handle:CsnArr = csnmaximum(a:CsnArr, b:CsnArr, trig:k)
handle:CsnArr = csnmaximum(a:CsnArr, value:i)
handle:CsnArr = csnmaximum(a:CsnArr, value:k, trig:k)
```

## Arguments

* `a:CsnArr`: first operand.
* `b:CsnArr`: second operand; broadcast against `a`.
* `value:i / value:k`: a scalar second operand.
* `trig:k` (optional, default `1`): k-rate trigger. A zero trigger republishes the previous result.

## Output

* `handle:CsnArr`: handle of the elementwise minimum.

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
; csnmaximum.csd
;
; Elementwise against another array, then against a scalar floor.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    a:CsnArr      = csnfromarray(array(1, 5, -3, 8))
    b:CsnArr      = csnfromarray(array(4, 2, 0, 8))

    hi:CsnArr     = csnmaximum(a, b)
    hi_out:i[]    = csntoarray(hi)
    prints("max  = %g %g %g %g\n", hi_out[0], hi_out[1], hi_out[2], hi_out[3])

    flr:CsnArr    = csnmaximum(a, 2)
    flr_out:i[]   = csntoarray(flr)
    prints("flr2 = %g %g %g %g\n", flr_out[0], flr_out[1], flr_out[2], flr_out[3])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnminimum](csnminimum.md)
* [csnmax](csnmax.md)
* [csnclip](csnclip.md)

## Credits

Pasquale Mainolfi, 2026
