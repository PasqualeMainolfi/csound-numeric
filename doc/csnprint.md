# csnprint

## Abstract

Print an array with its shape and element type in a NumPy-style layout.

## Description

`csnprint` writes a `CsnArr` directly to Csound's message stream. The first line
reports its shape and dtype; the following lines show the values in nested
brackets, with newlines and indentation separating higher dimensions.

Real and complex components are formatted with five significant digits
(`%.5g`). Complex values use `j` for the imaginary unit. A one-dimensional
shape keeps its tuple comma, for example `shape=(4,)`, while an empty array
prints `[]` below its metadata.

Arrays of at most 1000 elements are printed in full. Above that threshold,
dimensions longer than six items retain their first and last three entries and
put `...` between them, following NumPy's edge-item convention. The shape in the
header always remains complete.

The k-rate overload prints on every non-zero trigger. A zero trigger emits no
message. Its trigger is required because it is the argument that distinguishes
the performance-time overload from the init-time one.

## Syntax

```csound
csnprint(handle:CsnArr)
csnprint(handle:CsnArr, trig:k)
```

## Arguments

* `handle:CsnArr`: the real or complex array to print.
* `trig:k`: required k-rate trigger; non-zero prints the current array, zero does nothing.

## Output

None. The formatted array is written to Csound's message stream.

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
; csnprint.csd
;
; csnprint writes the shape, element type and NumPy-style array body. Long
; arrays keep the first and last three elements, with an ellipsis between them.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    values:i[] = fillarray(1.234567, 2, 3, 4)
    shape:i[]  = fillarray(2, 2)
    mat:CsnArr = csnreshape(csnfromarray(values), shape)
    csnprint(mat)

    long:CsnArr = csnarange(0, 1001, 1)
    csnprint(long)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

The two calls print:

```text
CsnArr(shape=(2, 2), dtype=float64)
[[1.2346 2]
 [3 4]]
CsnArr(shape=(1001,), dtype=float64)
[0 1 2 ... 998 999 1000]
```

## See also

* [csnshape](csnshape.md)
* [csntype](csntype.md)
* [csnsize](csnsize.md)
* [csntoarray](csntoarray.md)

## Credits

Pasquale Mainolfi, 2026
