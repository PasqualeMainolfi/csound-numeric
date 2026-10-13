# csnflatten

## Abstract

Collapse every dimension of an array into one.

## Description

`csnflatten` turns an array of any rank into a 1-D array of the same elements, in
flat order. It is [csnreshape](csnreshape.md) with the only shape that always
fits, and like it a layout change rather than a copy of the data.

The commonest use is right before [csntoarray](csntoarray.md), whose output rank
is taken from its declaration: flattening first lets a matrix of any shape be
received by a plain `i[]`.

Two forms share the name. The one with an output writes a new array and
leaves the source alone; the one without an output rewrites the source in place
and returns nothing.

The in-place form also takes a `:CsnStr;`, since it only changes the layout; the
form that returns a copy is numeric only for now. See [string
arrays](string-arrays.md).

## Syntax

```csound
out:CsnArr = csnflatten(source:CsnArr)
out:CsnArr = csnflatten(source:CsnArr, trig:k)
csnflatten(source:CsnArr)
csnflatten(source:CsnArr, trig:k)
csnflatten(source:CsnStr)
csnflatten(source:CsnStr, trig:k)
```

## Arguments

* `source:CsnArr / source:CsnStr`: the array to flatten.
* `trig:k` (optional): k-rate trigger. Without it the call is the init-only form and flattens once; with it the array is flattened again on every pass where it is non-zero.

## Output

* `out:CsnArr`: the 1-D array. Omit it for the in-place form.

## Execution Time

* Init
* Performance (k-rate, with `trig:k`)

## Examples

```csound
<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnflatten.csd
;
; csnflatten is the shape that always fits. Its usual job is to let a matrix of
; any rank be received by a plain i[] output.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    shape:i[]    = fillarray(2, 3)
    mat:CsnArr   = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6)), shape)

    flat:CsnArr  = csnflatten(mat)
    flat_out:i[] = csntoarray(flat)
    flat_dims:i  = csndims(flat)
    prints("dims = %d, values = %g %g %g %g %g %g\n", flat_dims, flat_out[0], flat_out[1], flat_out[2], flat_out[3], flat_out[4], flat_out[5])

    ; the source keeps its rank
    mat_dims:i   = csndims(mat)
    prints("source dims still = %d\n", mat_dims)

    ; in place
    csnflatten(mat)
    now_dims:i   = csndims(mat)
    prints("source dims after the in-place call = %d\n", now_dims)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnreshape](csnreshape.md)
* [csntoarray](csntoarray.md)
* [csndims](csndims.md)

## Credits

Pasquale Mainolfi, 2026
