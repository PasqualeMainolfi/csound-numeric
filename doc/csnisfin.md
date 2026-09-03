# csnisfin

## Abstract

Elementwise finite-value test, returned as a 0/1 mask.

## Description

`csnisfin` returns an array with the same shape as its source. Each output
element is `1` for a finite value, including positive and negative zero, and `0`
for NaN or either sign of infinity.

The result is an ordinary real mask suitable for [csnwhere](csnwhere.md),
[csnselect](csnselect.md), [csncompress](csncompress.md), or the logical
operators. A logically empty source produces a logically empty mask while
retaining its reserved shape. Real arrays only.

## Syntax

```csound
handle:CsnArr = csnisfin(source:CsnArr)
handle:CsnArr = csnisfin(source:CsnArr, trig:k)
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
    nan:i          = sqrt(-1)
    data:CsnArr    = csnfromarray(array(1, inf, nan, 0))
    mask:CsnArr    = csnisfin(data)
    mask_out:i[]   = csntoarray(mask)
    prints("finite = %g %g %g %g\n", mask_out[0], mask_out[1], mask_out[2], mask_out[3])
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
* [csnisinf](csnisinf.md)
* [csnwhere](csnwhere.md)
* [csnselect](csnselect.md)

## Credits

Pasquale Mainolfi, 2026
