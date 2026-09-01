# csnreverse

## Abstract

Reverse the flat element order of an array.

## Description

`csnreverse` reverses an array read flat: the last element becomes the first,
whatever the rank. It is [csnflip](csnflip.md) with `axis = -1`, said directly
and with no axis argument to give.

Two forms share the name. The one with an output publishes a new handle and
leaves the source alone; the one without an output rewrites the source in place
and returns nothing.

## Syntax

```csound
handle:CsnArr = csnreverse(source:CsnArr)
handle:CsnArr = csnreverse(source:CsnArr, trig:k)
csnreverse(source:CsnArr)
csnreverse(source:CsnArr, trig:k)
```

## Arguments

* `source:CsnArr`: the array to reverse.
* `trig:k`: k-rate trigger. The reversal is recomputed on a non-zero trigger; a zero trigger republishes the previous result.

## Output

* `handle:CsnArr`: handle of the reversed array. Omit it for the in-place form.

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
; csnreverse.csd
;
; csnreverse reads the array flat, so the rank does not matter: a 2 x 3 matrix
; comes back with its six elements in the opposite order, still 2 x 3.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    vec:CsnArr     = csnfromarray(array(1, 2, 3, 4))
    rev:CsnArr     = csnreverse(vec)
    rev_out:i[]    = csntoarray(rev)
    prints("vector = %g %g %g %g\n", rev_out[0], rev_out[1], rev_out[2], rev_out[3])

    shape:i[]      = fillarray(2, 3)
    mat:CsnArr     = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6)), shape)
    mat_rev:CsnArr = csnreverse(mat)
    mat_out:i[]    = csntoarray(csnflatten(mat_rev))
    dims:i         = csndims(mat_rev)
    prints("matrix (dims %d) = %g %g %g %g %g %g\n", dims, mat_out[0], mat_out[1], mat_out[2], mat_out[3], mat_out[4], mat_out[5])

    ; in place
    csnreverse(vec)
    now:i[]        = csntoarray(vec)
    prints("in place = %g %g %g %g\n", now[0], now[1], now[2], now[3])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnflip](csnflip.md)
* [csnroll](csnroll.md)
* [csnsort](csnsort.md)

## Credits

Pasquale Mainolfi, 2026
