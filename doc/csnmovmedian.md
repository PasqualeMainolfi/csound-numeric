# csnmovmedian

## Abstract

Moving median over a window.

## Description

`csnmovmedian` replaces every element by the median of the elements around it,
within a window of `winsize`.

Unlike [csnmovmean](csnmovmean.md) it removes an isolated spike outright instead
of spreading it over the neighbourhood, and it keeps edges sharp. That makes it
the filter to reach for on data with dropouts or single-sample glitches.

The window is centred on each element and **the array's length is preserved**:
near the two ends the window is simply shorter, so no padding value is invented
and the result lines up with the source element by element.

With no axis the array is read flat. Given an axis each line along it is filtered
on its own.

Real only — ordering has no meaning over the complex field.

Two forms share the name. The one with an output publishes a new handle and
leaves the source alone; the one without an output rewrites the source in place
and returns nothing.

An in-place pass reads a snapshot of the complete source taken before that pass,
so it produces exactly the same values as the output form applied to the same
input. Values written near the start of the array never leak into later
windows. At k-rate, every non-zero trigger applies one new pass to the array's
current values; a zero trigger leaves it untouched.

## Syntax

```csound
handle:CsnArr = csnmovmedian(source:CsnArr, winsize:i)
handle:CsnArr = csnmovmedian(source:CsnArr, winsize:i, axis:i)
handle:CsnArr = csnmovmedian(source:CsnArr, winsize:k, axis:k)
handle:CsnArr = csnmovmedian(source:CsnArr, winsize:k, axis:k, trig:k)
csnmovmedian(source:CsnArr, winsize:i)
csnmovmedian(source:CsnArr, winsize:i, axis:i)
csnmovmedian(source:CsnArr, winsize:k, axis:k)
csnmovmedian(source:CsnArr, winsize:k, axis:k, trig:k)
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
; csnmovmedian.csd
;
; A moving median deletes an isolated spike instead of smearing it, which is
; what separates it from a moving average.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr     = csnfromarray(array(1, 100, 2, 3, 4))

    smoothed:CsnArr = csnmovmedian(data, 3)
    smoothed_out:i[] = csntoarray(smoothed)
    n:i             = csnsize(smoothed)
    prints("window 3, n = %d : %g %g %g %g %g\n", n, smoothed_out[0], smoothed_out[1], smoothed_out[2], smoothed_out[3], smoothed_out[4])

    ; a wider window
    wider:CsnArr    = csnmovmedian(data, 5)
    wider_out:i[]   = csntoarray(wider)
    prints("window 5        : %g %g %g %g %g\n", wider_out[0], wider_out[1], wider_out[2], wider_out[3], wider_out[4])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnmovmean](csnmovmean.md)
* [csnmedian](csnmedian.md)
* [csnmovmin](csnmovmin.md)
* [csnmovmax](csnmovmax.md)

## Credits

Pasquale Mainolfi, 2026
