# csnreal

## Abstract

Real parts of a complex array, as a real array.

## Description

`csnreal` returns the real lane of a complex array as a real array of the same
shape. With [csnimag](csnimag.md) it takes a complex array apart into the two
halves it was assembled from.

**Complex only.** A real array has no imaginary lane to leave behind, so passing
one is refused rather than answered with a copy.

## Syntax

```csound
handle:CsnArr = csnreal(source:CsnArr)
handle:CsnArr = csnreal(source:CsnArr, trig:k)
```

## Arguments

* `source:CsnArr`: a complex array.
* `trig:k`: k-rate trigger. A zero trigger republishes the previous result.

## Output

* `handle:CsnArr`: a real array holding the real parts.

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
; csnreal.csd
;
; csnreal and csnimag take a complex array apart into the two real arrays it
; was built from.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    re:CsnArr    = csnfromarray(array(3, 0, -1))
    im:CsnArr    = csnfromarray(array(4, 2, 0))
    j:Complex    = init(0, 1, 0)
    z:CsnArr     = csnadd(csntocomplex(re), csnmul(csntocomplex(im), j))

    parts:CsnArr = csnreal(z)
    parts_out:i[] = csntoarray(parts)
    itype:i      = csntype(parts)
    prints("itype = %d, real parts = %g %g %g\n", itype, parts_out[0], parts_out[1], parts_out[2])

    ; the conjugate leaves the real parts alone
    conj:CsnArr  = csnconj(z)
    conj_re:CsnArr = csnreal(conj)
    conj_out:i[] = csntoarray(conj_re)
    prints("after conjugation = %g %g %g\n", conj_out[0], conj_out[1], conj_out[2])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnimag](csnimag.md)
* [csnangle](csnangle.md)
* [csnconj](csnconj.md)
* [csntoreal](csntoreal.md)

## Credits

Pasquale Mainolfi, 2026
