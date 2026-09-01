# csnhypot

## Abstract

Hypotenuse of two arrays, elementwise.

## Description

`csnhypot` returns `sqrt(a² + b²)` for every pair of elements, computed the way C
does it: without the overflow or underflow a literal squaring would risk.

It is the magnitude of a vector given by its two components, so it converts a
pair of real and imaginary lanes, or a pair of x and y coordinates, into a length
in one call.

The second operand may be a scalar. Real only — for the magnitude of a complex
array use [csnabs](csnabs.md).

## Syntax

```csound
handle:CsnArr = csnhypot(a:CsnArr, b:CsnArr)
handle:CsnArr = csnhypot(a:CsnArr, b:CsnArr, trig:k)
handle:CsnArr = csnhypot(a:CsnArr, value:i)
handle:CsnArr = csnhypot(a:CsnArr, value:k)
handle:CsnArr = csnhypot(a:CsnArr, value:k, trig:k)
```

## Arguments

* `a:CsnArr`: first component.
* `b:CsnArr`: second component; broadcast against `a`.
* `value:i / value:k`: a scalar second component.
* `trig:k` (optional, default `1`): k-rate trigger. A zero trigger republishes the previous result.

## Output

* `handle:CsnArr`: handle of the elementwise hypotenuse.

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
; csnhypot.csd
;
; Two component arrays in, one length array out. Computed without the overflow
; a literal sqrt(a*a + b*b) would risk on large values.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    x:CsnArr     = csnfromarray(array(3, 5, 8))
    y:CsnArr     = csnfromarray(array(4, 12, 15))

    len:CsnArr   = csnhypot(x, y)
    len_out:i[]  = csntoarray(len)
    prints("lengths = %g %g %g\n", len_out[0], len_out[1], len_out[2])

    ; a scalar second component: distance from a fixed offset
    off:CsnArr   = csnhypot(x, 4)
    off_out:i[]  = csntoarray(off)
    prints("with 4  = %g %.4f %.4f\n", off_out[0], off_out[1], off_out[2])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnabs](csnabs.md)
* [csnsqrt](csnsqrt.md)
* [csnnorm](csnnorm.md)
* [csnpairdist](csnpairdist.md)

## Credits

Pasquale Mainolfi, 2026
