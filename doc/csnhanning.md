# csnhanning

## Abstract

Hann window.

## Description

`csnhanning` returns a Hann (raised cosine) window: `0.5 - 0.5·cos(2πn/(N-1))`.

It reaches zero at both ends and rolls off quickly on either side of its main
lobe, which makes it the default choice for spectral analysis when nothing about
the signal argues for something else.

The window is symmetric and `n` elements long, sampled so that the two ends fall
where the definition puts them. Its usual companion is [csnmul](csnmul.md):
multiply a buffer by a window of the same length and the buffer is shaped.

Real only.

## Syntax

```csound
handle:CsnArr = csnhanning(n:i)
handle:CsnArr = csnhanning(n:k)
```

## Arguments

* `n:i / n:k`: the window length, in elements.

## Output

* `handle:CsnArr`: a real 1-D array of `n` window coefficients.

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
; csnhanning.csd
;
; The Hann window: zero at both ends, fast roll-off. The default choice for
; spectral work.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    win:CsnArr   = csnhanning(8)
    win_out:i[]  = csntoarray(win)
    n:i          = csnsize(win)
    prints("n = %d\n", n)
    prints("first half = %.4f %.4f %.4f %.4f\n", win_out[0], win_out[1], win_out[2], win_out[3])
    prints("second half = %.4f %.4f %.4f %.4f\n", win_out[4], win_out[5], win_out[6], win_out[7])

    peak:i       = csnmax(win)
    total:i      = csnsum(win)
    prints("peak = %.4f, sum = %.4f\n", peak, total)

    ; shaping a buffer is one multiplication
    buf:CsnArr   = csnones(fillarray(8))
    shaped:CsnArr = csnmul(buf, win)
    shaped_out:i[] = csntoarray(shaped)
    prints("shaped = %.4f %.4f %.4f %.4f\n", shaped_out[0], shaped_out[1], shaped_out[2], shaped_out[3])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnhamming](csnhamming.md)
* [csnblackman](csnblackman.md)
* [csnbartlett](csnbartlett.md)
* [csnkaiser](csnkaiser.md)

## Credits

Pasquale Mainolfi, 2026
