# csntocomplex

## Abstract

Convert a real array to complex, imaginary parts at zero.

## Description

`csntocomplex` widens a real array into a complex one: every element becomes
`x + 0i`. The shape is unchanged; only the storage grows, from one double per
element to two.

It is the way in to the complex half of the suite when the data starts out real —
a buffer read from a table, say, that is about to be combined with a complex
spectrum. Most binary opcodes promote automatically when they meet a real and a
complex operand, so this is mainly for the cases where the *result* must be
complex from the start, such as an accumulator declared `@global`.

[csntoreal](csntoreal.md) is the way back.

## Syntax

```csound
handle:CsnArr = csntocomplex(source:CsnArr)
handle:CsnArr = csntocomplex(source:CsnArr, trig:k)
```

## Arguments

* `source:CsnArr`: a real array.
* `trig:k`: k-rate trigger. A zero trigger republishes the previous result.

## Output

* `handle:CsnArr`: a complex array with the shape of the source.

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
; csntocomplex.csd
;
; Real in, complex out, imaginary lane at zero. Building a full complex array
; from two real ones is this plus a multiply by i and an add.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    re:CsnArr    = csnfromarray(array(1, 0, -1, 0))
    im:CsnArr    = csnfromarray(array(0, 1, 0, -1))

    widened:CsnArr = csntocomplex(re)
    itype:i      = csntype(widened)
    size:i       = csnsize(widened)
    prints("itype = %d, size = %d\n", itype, size)

    ; re + i*im, the usual way to assemble a complex array from two real ones
    j:Complex    = init(0, 1, 0)
    scaled:CsnArr = csnmul(csntocomplex(im), j)
    z:CsnArr     = csnadd(widened, scaled)

    z_re:CsnArr  = csnreal(z)
    z_im:CsnArr  = csnimag(z)
    re_out:i[]   = csntoarray(z_re)
    im_out:i[]   = csntoarray(z_im)
    prints("real = %g %g %g %g\n", re_out[0], re_out[1], re_out[2], re_out[3])
    prints("imag = %g %g %g %g\n", im_out[0], im_out[1], im_out[2], im_out[3])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csntoreal](csntoreal.md)
* [csnreal](csnreal.md)
* [csnimag](csnimag.md)
* [csntype](csntype.md)

## Credits

Pasquale Mainolfi, 2026
