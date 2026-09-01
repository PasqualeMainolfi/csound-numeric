# csnbartlett

## Abstract

Bartlett (triangular) window.

## Description

`csnbartlett` returns a Bartlett window: a triangle rising linearly from zero at
the first element to the middle and back down to zero at the last. An odd-length
window has one centre coefficient equal to 1. An even-length window has two
equal central coefficients just below 1; for example, both central values of an
8-element window are `6/7`. The window never overshoots 1.

It is the cheapest window that reaches zero at both ends, and the one to use for
a simple cross-fade or a linear taper, where the frequency-domain behaviour
matters less than the shape.

The window is symmetric and `n` elements long, sampled so that the two ends fall
where the definition puts them. Its usual companion is [csnmul](csnmul.md):
multiply a buffer by a window of the same length and the buffer is shaped.

Real only.

## Syntax

```csound
handle:CsnArr = csnbartlett(n:i)
handle:CsnArr = csnbartlett(n:k)
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
; csnbartlett.csd
;
; A linear taper, zero at both ends. The cheapest thing that removes an edge
; discontinuity, and the natural cross-fade shape.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    win:CsnArr   = csnbartlett(8)
    win_out:i[]  = csntoarray(win)
    n:i          = csnsize(win)
    prints("n = %d\n", n)
    prints("rising  = %.4f %.4f %.4f %.4f\n", win_out[0], win_out[1], win_out[2], win_out[3])
    prints("falling = %.4f %.4f %.4f %.4f\n", win_out[4], win_out[5], win_out[6], win_out[7])

    peak:i       = csnmax(win)
    total:i      = csnsum(win)
    prints("peak = %.4f, sum = %.4f\n", peak, total)

    ; shaping a buffer is one multiplication
    buf:CsnArr   = csnones(fillarray(8))
    shaped:CsnArr = csnmul(buf, win)
    shaped_out:i[] = csntoarray(shaped)
    prints("shaped  = %.4f %.4f %.4f %.4f\n", shaped_out[0], shaped_out[1], shaped_out[2], shaped_out[3])

    ; the second half of a Bartlett is a linear fade-out
    fade:CsnArr  = csngetslice(win, 0, 4, 8, 1)
    fade_out:i[] = csntoarray(fade)
    prints("fade    = %.4f %.4f %.4f %.4f\n", fade_out[0], fade_out[1], fade_out[2], fade_out[3])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnhanning](csnhanning.md)
* [csnhamming](csnhamming.md)
* [csnlinspace](csnlinspace.md)

## Credits

Pasquale Mainolfi, 2026
