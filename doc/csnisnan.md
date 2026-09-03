# csnisnan

## Abstract

Elementwise NaN test, returned as a 0/1 mask.

## Description

`csnisnan` returns an array with the same shape as its source. Each output
element is `1` when the corresponding source value is NaN and `0` otherwise.
Finite values and both signs of infinity produce `0`.

The result is an ordinary real mask suitable for [csnwhere](csnwhere.md),
[csnselect](csnselect.md), [csncompress](csncompress.md), or the logical
operators. A logically empty source produces a logically empty mask while
retaining its reserved shape. Real arrays only.

## Syntax

```csound
handle:CsnArr = csnisnan(source:CsnArr)
handle:CsnArr = csnisnan(source:CsnArr, trig:k)
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
    nan:i          = sqrt(-1)
    data:CsnArr    = csnfromarray(array(1, nan, 0, -2))
    mask:CsnArr    = csnisnan(data)
    mask_out:i[]   = csntoarray(mask)
    prints("is NaN = %g %g %g %g\n", mask_out[0], mask_out[1], mask_out[2], mask_out[3])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnisinf](csnisinf.md)
* [csnisfin](csnisfin.md)
* [csnargisnan](csnargisnan.md)
* [csncntnan](csncntnan.md)

## Credits

Pasquale Mainolfi, 2026
