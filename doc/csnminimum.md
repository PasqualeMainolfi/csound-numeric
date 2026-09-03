# csnminimum

## Abstract

Elementwise minimum of two arrays, or of an array and a scalar.

## Description

`csnminimum` compares position by position and keeps the smaller of the two
values. It is the elementwise pair of [csnmin](csnmin.md), which instead reduces
one array to its smallest element.

The two operands broadcast against each other, so a row can be compared against a
matrix without being materialised first. The second operand may be a scalar.

Real only. NaN is not propagated: where one operand is NaN the other is returned,
following C's `fmin`.

## Syntax

```csound
handle:CsnArr = csnminimum(a:CsnArr, b:CsnArr)
handle:CsnArr = csnminimum(a:CsnArr, b:CsnArr, trig:k)
handle:CsnArr = csnminimum(a:CsnArr, value:i)
handle:CsnArr = csnminimum(a:CsnArr, value:k, trig:k)
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
; csnminimum.csd
;
; Elementwise against another array, then against a scalar ceiling.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    a:CsnArr      = csnfromarray(array(1, 5, -3, 8))
    b:CsnArr      = csnfromarray(array(4, 2, 0, 8))

    lo:CsnArr     = csnminimum(a, b)
    lo_out:i[]    = csntoarray(lo)
    prints("min  = %g %g %g %g\n", lo_out[0], lo_out[1], lo_out[2], lo_out[3])

    cap:CsnArr    = csnminimum(a, 2)
    cap_out:i[]   = csntoarray(cap)
    prints("cap2 = %g %g %g %g\n", cap_out[0], cap_out[1], cap_out[2], cap_out[3])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnmaximum](csnmaximum.md)
* [csnmin](csnmin.md)
* [csnclip](csnclip.md)

## Credits

Pasquale Mainolfi, 2026
