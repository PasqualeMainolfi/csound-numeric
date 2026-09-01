# csnfull

## Abstract

Create an array of a given shape filled with one value.

## Description

`csnfull` allocates a csnum array of the requested shape and publishes every
element at `value`. It generalises [csnzeros](csnzeros.md) and
[csnones](csnones.md), which are the `value = 0` and `value = 1` cases.

There are two ways to ask for a complex array. Either pass a real `value` and
set `itype` to `1`, in which case every element is `value + 0i`, or pass a
`:Complex;` value directly: its own type then fixes the array's, and there is no
`itype` argument to give.

`itype` is read at init. `value` may follow a k-rate signal, and the k-rate form
rewrites the elements on every pass, so the fill is restored even if something
downstream overwrote the array.

## Syntax

```csound
handle:CsnArr = csnfull(shape:i[], value:i)
handle:CsnArr = csnfull(shape:i[], value:i, itype:i)
handle:CsnArr = csnfull(shape:i[], value:Complex)
handle:CsnArr = csnfull(shape:k[], value:k)
handle:CsnArr = csnfull(shape:k[], value:k, itype:i)
handle:CsnArr = csnfull(shape:k[], value:Complex)
```

## Arguments

* `shape:i[] / shape:k[]`: one extent per dimension.
* `value:i / value:k`: the value every element is set to.
* `value:Complex`: a complex fill value; the array is complex and `itype` is not accepted.
* `itype:i` (optional, default `0`): `0` for a real array, `1` for a complex one.

## Output

* `handle:CsnArr`: handle of the new array.

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
; csnfull.csd
;
; csnfull fills a shape with one value. A complex fill value fixes the element
; type on its own, so no itype argument is needed in that form.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    shape:i[]    = fillarray(4)
    full:CsnArr  = csnfull(shape, -3.5)
    full_out:i[] = csntoarray(full)
    prints("real    = %g %g %g %g\n", full_out[0], full_out[1], full_out[2], full_out[3])

    ; complex fill: the value's type fixes the array's
    z:Complex    = init(1, -2, 0)
    cpx:CsnArr   = csnfull(shape, z)
    itype:i      = csntype(cpx)
    cell:i[]     = fillarray(0)
    w:Complex    = csnget(cpx, cell)
    w_re:i       = real(w)
    w_im:i       = imag(w)
    prints("itype   = %d, cell0 = %g%+gi\n", itype, w_re, w_im)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnzeros](csnzeros.md)
* [csnones](csnones.md)
* [csnlike](csnlike.md)
* [csnempty](csnempty.md)

## Credits

Pasquale Mainolfi, 2026
