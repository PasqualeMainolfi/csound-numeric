# csnacosh

## Abstract

Inverse hyperbolic cosine of every element.

## Description

`csnacosh` returns the inverse hyperbolic cosine of every element.

Over the reals the argument must be at least 1; anything below gives a NaN, so
[csnclip](csnclip.md) is the usual guard. A complex array has no such restriction.

Real and complex arrays are both accepted: a complex source is carried through
the complex form of the function and the result stays complex.

## Syntax

```csound
handle:CsnArr = csnacosh(source:CsnArr)
handle:CsnArr = csnacosh(source:CsnArr, trig:k)
```

## Arguments

* `source:CsnArr`: the array to read.
* `trig:k`: k-rate trigger. The result is recomputed on a non-zero trigger; a zero trigger republishes the previous one.

## Output

* `handle:CsnArr`: handle of the result, with the shape and element type of the source.

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
; csnacosh.csd
;
; Elementwise, over the whole array. Where the real domain is restricted,
; csnclip is the guard to put in front.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr   = csnfromarray(array(1, 1.5, 2, 5, 10))

    out_a:CsnArr  = csnacosh(data)
    out_arr:i[]   = csntoarray(out_a)
    prints("acosh = %.4f %.4f %.4f %.4f %.4f\n", out_arr[0], out_arr[1], out_arr[2], out_arr[3], out_arr[4])

    nan_count:i   = csncntnan(out_a)
    prints("NaN elements = %d\n", nan_count)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csncosh](csncosh.md)
* [csnasinh](csnasinh.md)
* [csnatanh](csnatanh.md)
* [csnclip](csnclip.md)

## Credits

Pasquale Mainolfi, 2026
