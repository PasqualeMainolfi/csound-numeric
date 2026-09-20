# csndbsum

## Abstract

Sums levels in decibels by adding their powers, over a pair, a whole array, or one axis.

## Description

Decibels are logarithmic, so they do not add. Two sources at the same level are
not twice the number: their powers add and the sum goes back to dB.

```
L = 10 * log10( sum over the elements of 10^(L_i / 10) )
```

Two equal levels give +3.0103 dB, four give +6.0206 dB, and a source 20 dB below
another contributes about 0.04 dB. That asymmetry is the whole practical content
of the operation: in a sum of octave bands or of noise sources, only the loudest
few matter.

The formula is the power one, `10 log10`, which is what sound pressure *levels*,
sound power levels and band levels in dB SPL are. If your numbers are amplitude
ratios expressed with `20 log10` — Csound's `dbamp` and `ampdb` work that way —
they are not levels in this sense and summing them here is not the operation you
want.

Three ways to call it. Two scalars sum one pair. A handle alone sums every
element of the array, whatever its shape, to one number. A handle with an axis
reduces along that axis and drops it, exactly as [csnsum](csnsum.md) does, so a
`sources x bands` matrix summed along axis 0 gives one level per band and along
axis 1 one level per source.

An empty array, or one whose powers sum to zero, has no level to report; the
logarithm yields negative infinity, which is the honest answer and the one NumPy
gives.

Real only: ordering and logarithms of complex values are not this operation.

At k-rate the pair form carries no trigger — it is two exponentials — while the
reducing forms take one, and a zero trigger republishes the previous result. In
the k-rate reducing form the axis follows the trigger, as it does in every csnum
reduction.

## Syntax

```csound
value:i = csndbsum(a:i, b:i)
value:i = csndbsum(source:CsnArr)
handle:CsnArr = csndbsum(source:CsnArr, axis:i)
value:k = csndbsum(a:k, b:k)
value:k = csndbsum(source:CsnArr, trig:k)
handle:CsnArr = csndbsum(source:CsnArr, trig:k, axis:k)
```

## Arguments

* `a:i / a:k`, `b:i / b:k`: two levels in dB, for the pair form.
* `source:CsnArr`: the array of levels in dB.
* `axis:i / axis:k` (optional): the axis to sum along. Omit it to sum every element to one number; `-1` selects the last axis.
* `trig:k` (optional, default `1`): k-rate trigger on the forms that read an array. A zero trigger republishes the previous result.

## Output

* `value:i / value:k`: the summed level in dB, for the pair form and for the whole-array form.
* `handle:CsnArr`: the summed levels with the reduced axis dropped, for the axis form.

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
; csndbsum.csd
;
; Decibels do not add. Two equal sources make one 3.01 dB louder, not twice the
; number: the powers add, and the sum goes back to dB. csndbsum does that on a
; pair of levels, over a whole array, or along one axis of a matrix.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    ; two equal sources: +3.0103 dB, the classic doubling
    pair:i          = csndbsum(80, 80)
    prints("80 and 80       : %.4f dB\n", pair)

    ; 6 dB apart: the quieter one barely counts
    uneven:i        = csndbsum(85, 79)
    prints("85 and 79       : %.4f dB\n", uneven)

    ; 20 dB apart: it counts for almost nothing at all
    far:i           = csndbsum(85, 65)
    prints("85 and 65       : %.4f dB\n", far)

    ; a whole array of sources at once: four equal ones make +6.0206 dB
    four:CsnArr     = csnfromarray(array(80, 80, 80, 80))
    total:i         = csndbsum(four)
    prints("four times 80   : %.4f dB\n", total)

    ; an octave-band spectrum, summed to one broadband level
    bands:CsnArr    = csnfromarray(array(72, 78, 81, 76, 69, 61))
    broadband:i     = csndbsum(bands)
    prints("broadband       : %.4f dB\n", broadband)

    ; one row per source, one column per band: axis 0 sums the sources band by
    ; band, axis 1 sums the bands source by source
    shape:i[]       = fillarray(2, 3)
    matrix:CsnArr   = csnreshape(csnfromarray(array(70, 76, 80, 70, 70, 74)), shape)
    per_band:CsnArr = csndbsum(matrix, 0)
    per_source:CsnArr = csndbsum(matrix, 1)
    csnprint per_band
    csnprint per_source
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnsum](csnsum.md)
* [csnrms](csnrms.md)
* [csnmean](csnmean.md)
* [csnt60sab](csnt60sab.md)

## Credits

Pasquale Mainolfi, 2026
