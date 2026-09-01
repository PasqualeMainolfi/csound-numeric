# csnmovvar

## Abstract

Moving variance over a window.

## Description

`csnmovvar` replaces every element by the variance of the elements around it,
within a window of `winsize` — a local measure of how much the signal is moving,
in the square of the data's units.

It is the square of [csnmovstd](csnmovstd.md), and like [csnvar](csnvar.md) it
divides by `N`.

The window is centred on each element and **the array's length is preserved**:
near the two ends the window is simply shorter, so no padding value is invented
and the result lines up with the source element by element.

With no axis the array is read flat. Given an axis each line along it is filtered
on its own.

Both real and complex arrays are accepted by the output form, whose result is
real. The in-place form accepts only real arrays because it cannot replace a
complex source with real elements without changing its storage layout.

Two forms share the name. The one with an output publishes a new handle and
leaves the source alone; the one without an output rewrites the source in place
and returns nothing.

An in-place pass reads a snapshot of the complete source taken before that pass,
so it produces exactly the same values as the output form applied to the same
real input. Values written near the start of the array never leak into later
windows. At k-rate, every non-zero trigger applies one new pass to the array's
current values; a zero trigger leaves it untouched.

## Syntax

```csound
handle:CsnArr = csnmovvar(source:CsnArr, winsize:i)
handle:CsnArr = csnmovvar(source:CsnArr, winsize:i, axis:i)
handle:CsnArr = csnmovvar(source:CsnArr, winsize:k, axis:k)
handle:CsnArr = csnmovvar(source:CsnArr, winsize:k, axis:k, trig:k)
csnmovvar(source:CsnArr, winsize:i)
csnmovvar(source:CsnArr, winsize:i, axis:i)
csnmovvar(source:CsnArr, winsize:k, axis:k)
csnmovvar(source:CsnArr, winsize:k, axis:k, trig:k)
```

## Arguments

* `source:CsnArr`: the array to filter.
* `winsize:i / winsize:k`: the window length, in elements.
* `axis:i / axis:k` (optional, default `-1`): the axis to slide along; `-1` reads the array flat.
* `trig:k` (optional, default `1`): k-rate trigger. In the output form a zero trigger republishes the previous result; in place it leaves the source untouched.

## Output

* `handle:CsnArr`: the filtered array, with the shape of the source. Omit it for the in-place form.

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
; csnmovvar.csd
;
; Local spread, squared. csnmovstd is the same measure back in the units of the
; data.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr     = csnfromarray(array(1, 1, 1, 8, 1))

    smoothed:CsnArr = csnmovvar(data, 3)
    smoothed_out:i[] = csntoarray(smoothed)
    n:i             = csnsize(smoothed)
    prints("window 3, n = %d : %.4f %.4f %.4f %.4f %.4f\n", n, smoothed_out[0], smoothed_out[1], smoothed_out[2], smoothed_out[3], smoothed_out[4])

    ; a wider window
    wider:CsnArr    = csnmovvar(data, 5)
    wider_out:i[]   = csntoarray(wider)
    prints("window 5        : %.4f %.4f %.4f %.4f %.4f\n", wider_out[0], wider_out[1], wider_out[2], wider_out[3], wider_out[4])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnmovstd](csnmovstd.md)
* [csnvar](csnvar.md)
* [csnmovmean](csnmovmean.md)

## Credits

Pasquale Mainolfi, 2026
