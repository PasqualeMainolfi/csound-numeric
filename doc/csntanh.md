# csntanh

## Abstract

Hyperbolic tangent of every element.

## Description

`csntanh` returns the hyperbolic tangent of every element. It is bounded to
`(-1, 1)` and flattens out smoothly on both sides, which makes it the usual
soft-clipping curve: pass a signal through it and loud values compress instead of
folding.

Real and complex arrays are both accepted: a complex source is carried through
the complex form of the function and the result stays complex.

## Syntax

```csound
handle:CsnArr = csntanh(source:CsnArr)
handle:CsnArr = csntanh(source:CsnArr, trig:k)
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
; csntanh.csd
;
; Elementwise, over the whole array. csntanh is the bounded one, which is why
; it is the usual soft-clipper.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr   = csnfromarray(array(-2, -1, 0, 1, 2))

    out_a:CsnArr  = csntanh(data)
    out_arr:i[]   = csntoarray(out_a)
    prints("tanh = %.4f %.4f %.4f %.4f %.4f\n", out_arr[0], out_arr[1], out_arr[2], out_arr[3], out_arr[4])

    lo:i          = csnmin(out_a)
    hi:i          = csnmax(out_a)
    prints("range = %.4f .. %.4f\n", lo, hi)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnsinh](csnsinh.md)
* [csncosh](csncosh.md)
* [csnatanh](csnatanh.md)
* [csnclip](csnclip.md)

## Credits

Pasquale Mainolfi, 2026
