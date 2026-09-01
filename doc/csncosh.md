# csncosh

## Abstract

Hyperbolic cosine of every element.

## Description

`csncosh` returns the hyperbolic cosine of every element. It is even and never
below 1, and like the hyperbolic sine it grows exponentially.

Real and complex arrays are both accepted: a complex source is carried through
the complex form of the function and the result stays complex.

## Syntax

```csound
handle:CsnArr = csncosh(source:CsnArr)
handle:CsnArr = csncosh(source:CsnArr, trig:k)
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
; csncosh.csd
;
; Elementwise, over the whole array. csntanh is the bounded one, which is why
; it is the usual soft-clipper.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr   = csnfromarray(array(-2, -1, 0, 1, 2))

    out_a:CsnArr  = csncosh(data)
    out_arr:i[]   = csntoarray(out_a)
    prints("cosh = %.4f %.4f %.4f %.4f %.4f\n", out_arr[0], out_arr[1], out_arr[2], out_arr[3], out_arr[4])

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
* [csntanh](csntanh.md)
* [csnacosh](csnacosh.md)

## Credits

Pasquale Mainolfi, 2026
