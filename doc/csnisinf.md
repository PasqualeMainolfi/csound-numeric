# csnisinf

## Abstract

Elementwise infinity test, returned as a 0/1 mask.

## Description

`csnisinf` returns an array with the same shape as its source. Each output
element is `1` for positive or negative infinity and `0` for finite values and
NaN.

The result is an ordinary real mask suitable for [csnwhere](csnwhere.md),
[csnselect](csnselect.md), [csncompress](csncompress.md), or the logical
operators. A logically empty source produces a logically empty mask while
retaining its reserved shape. Real arrays only.

## Syntax

```csound
handle:CsnArr = csnisinf(source:CsnArr)
handle:CsnArr = csnisinf(source:CsnArr, trig:k)
```

## Arguments

* `source:CsnArr`: the real array to classify.
* `trig:k`: k-rate trigger. Zero republishes the previous mask.

## Output

* `handle:CsnArr`: a shape-preserving 0/1 mask.

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

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    inf:i          = exp(1000)
    data:CsnArr    = csnfromarray(array(1, inf, -inf, 0))
    mask:CsnArr    = csnisinf(data)
    mask_out:i[]   = csntoarray(mask)
    prints("is Inf = %g %g %g %g\n", mask_out[0], mask_out[1], mask_out[2], mask_out[3])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnisnan](csnisnan.md)
* [csnisfin](csnisfin.md)
* [csnwhere](csnwhere.md)
* [csnselect](csnselect.md)

## Credits

Pasquale Mainolfi, 2026
