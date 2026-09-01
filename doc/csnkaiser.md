# csnkaiser

## Abstract

Kaiser window with a beta parameter.

## Description

`csnkaiser` returns a Kaiser window, built from the zeroth-order modified Bessel
function of the first kind. Unlike the fixed windows it takes a shape parameter,
`beta`, which trades main-lobe width against side-lobe level continuously:

* `beta = 0` is a rectangular window;
* `beta` around `5` is close to a Hamming;
* `beta` around `6` is close to a Hann;
* `beta` around `8.6` is close to a Blackman;
* larger values keep lowering the side lobes and widening the main one.

That one knob is why it is the window to reach for when the requirement is stated
as a stop-band attenuation.

The window is symmetric and `n` elements long, sampled so that the two ends fall
where the definition puts them. Its usual companion is [csnmul](csnmul.md):
multiply a buffer by a window of the same length and the buffer is shaped.

Real only.

## Syntax

```csound
handle:CsnArr = csnkaiser(n:i, beta:i)
handle:CsnArr = csnkaiser(n:i, beta:k)
handle:CsnArr = csnkaiser(n:k, beta:k)
```

## Arguments

* `n:i / n:k`: the window length, in elements.
* `beta:i / beta:k`: the shape parameter; 0 is rectangular, larger values lower the side lobes and widen the main lobe.

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
; csnkaiser.csd
;
; One knob, beta, trades main-lobe width against side-lobe level. Around 6 it
; is close to a Hann; higher values push the side lobes down further.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    win:CsnArr   = csnkaiser(8, 6)
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

    ; beta is the knob: 0 is rectangular, larger values taper harder
    flat:CsnArr  = csnkaiser(8, 0)
    tight:CsnArr = csnkaiser(8, 12)
    flat_out:i[]  = csntoarray(flat)
    tight_out:i[] = csntoarray(tight)
    prints("beta 0  = %.4f %.4f %.4f %.4f\n", flat_out[0], flat_out[1], flat_out[2], flat_out[3])
    prints("beta 12 = %.4f %.4f %.4f %.4f\n", tight_out[0], tight_out[1], tight_out[2], tight_out[3])
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
* [csnblackman](csnblackman.md)
* [csntoftable](csntoftable.md)

## Credits

Pasquale Mainolfi, 2026
