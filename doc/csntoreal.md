# csntoreal

## Abstract

Convert a complex array to real, keeping the real parts.

## Description

`csntoreal` narrows a complex array to a real one by keeping the real lane and
discarding the imaginary one. The shape is unchanged; the storage halves.

It does the same arithmetic as [csnreal](csnreal.md) — both hand back the real
parts as a real array — and the two differ only in intent: `csnreal` is one of
the pair that takes a complex array apart, while `csntoreal` is the type
conversion, the counterpart of [csntocomplex](csntocomplex.md).

Discarding the imaginary lane loses information. When the magnitude is what is
meant, use [csnabs](csnabs.md) instead.

## Syntax

```csound
handle:CsnArr = csntoreal(source:CsnArr)
handle:CsnArr = csntoreal(source:CsnArr, trig:k)
```

## Arguments

* `source:CsnArr`: a complex array.
* `trig:k`: k-rate trigger. A zero trigger republishes the previous result.

## Output

* `handle:CsnArr`: a real array with the shape of the source.

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
; csntoreal.csd
;
; The imaginary lane is dropped, not folded in. csnabs is the one to use when
; the magnitude is what is meant.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    re:CsnArr     = csnfromarray(array(3, 0, -1))
    im:CsnArr     = csnfromarray(array(4, 2, 0))
    j:Complex     = init(0, 1, 0)
    z:CsnArr      = csnadd(csntocomplex(re), csnmul(csntocomplex(im), j))

    narrowed:CsnArr = csntoreal(z)
    itype:i       = csntype(narrowed)
    out:i[]       = csntoarray(narrowed)
    prints("itype = %d, real parts = %g %g %g\n", itype, out[0], out[1], out[2])

    ; the round trip through csntocomplex loses the imaginary lane
    back:CsnArr   = csntocomplex(narrowed)
    back_im:CsnArr = csnimag(back)
    back_out:i[]  = csntoarray(back_im)
    prints("imaginary after the round trip = %g %g %g\n", back_out[0], back_out[1], back_out[2])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csntocomplex](csntocomplex.md)
* [csnreal](csnreal.md)
* [csnabs](csnabs.md)

## Credits

Pasquale Mainolfi, 2026
