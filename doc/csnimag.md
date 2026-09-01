# csnimag

## Abstract

Imaginary parts of a complex array, as a real array.

## Description

`csnimag` returns the imaginary lane of a complex array as a real array of the
same shape. With [csnreal](csnreal.md) it takes a complex array apart into the
two halves it was assembled from.

**Complex only.** A real array has no imaginary lane, so passing one is refused
rather than answered with zeros.

## Syntax

```csound
handle:CsnArr = csnimag(source:CsnArr)
handle:CsnArr = csnimag(source:CsnArr, trig:k)
```

## Arguments

* `source:CsnArr`: a complex array.
* `trig:k`: k-rate trigger. A zero trigger republishes the previous result.

## Output

* `handle:CsnArr`: a real array holding the imaginary parts.

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
; csnimag.csd
;
; The imaginary lane on its own. Conjugating flips its sign, which is the
; quickest way to see that csnconj did what it says.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    re:CsnArr    = csnfromarray(array(3, 0, -1))
    im:CsnArr    = csnfromarray(array(4, 2, 0))
    j:Complex    = init(0, 1, 0)
    z:CsnArr     = csnadd(csntocomplex(re), csnmul(csntocomplex(im), j))

    parts:CsnArr = csnimag(z)
    parts_out:i[] = csntoarray(parts)
    prints("imaginary parts = %g %g %g\n", parts_out[0], parts_out[1], parts_out[2])

    ; conjugation flips the sign
    conj:CsnArr  = csnconj(z)
    conj_im:CsnArr = csnimag(conj)
    conj_out:i[] = csntoarray(conj_im)
    prints("after conjugation = %g %g %g\n", conj_out[0], conj_out[1], conj_out[2])

    ; a purely real array has a zero imaginary lane once widened
    widened:CsnArr = csntocomplex(re)
    zero:CsnArr  = csnimag(widened)
    zero_out:i[] = csntoarray(zero)
    prints("after csntocomplex = %g %g %g\n", zero_out[0], zero_out[1], zero_out[2])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnreal](csnreal.md)
* [csnangle](csnangle.md)
* [csnconj](csnconj.md)

## Credits

Pasquale Mainolfi, 2026
