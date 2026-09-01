# csnargnonzero

## Abstract

Return the coordinates of the non-zero elements.

## Description

`csnargnonzero` returns the position of every element that is not zero, as a
`matches × ndims` array: one row per element found, each row holding its full
coordinates.

Its usual partner is a comparison. [csngt](csngt.md) and the rest turn a test
into a 0/1 mask, and `csnargnonzero` turns that mask into the list of positions
that passed — the csnum equivalent of NumPy's `nonzero(a > x)`.

Real only. NaN counts as non-zero here; [csnargisnan](csnargisnan.md) is the way
to single those out.

## Syntax

```csound
handle:CsnArr = csnargnonzero(source:CsnArr)
handle:CsnArr = csnargnonzero(source:CsnArr, trig:k)
```

## Arguments

* `source:CsnArr`: the array to scan.
* `trig:k`: k-rate trigger. The scan is redone on a non-zero trigger; a zero trigger republishes the previous result.

## Output

* `handle:CsnArr`: a `matches × ndims` array of coordinates.

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
; csnargnonzero.csd
;
; Comparison first, then csnargnonzero: the mask says which elements passed and
; this turns the mask into the list of positions.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    vec:CsnArr    = csnfromarray(array(0, 7, 0, 9))
    hits:CsnArr   = csnargnonzero(vec)
    hits_out:i[]  = csntoarray(csnflatten(hits))
    count:i       = csnsize(hits)
    prints("non-zero: %d, at %g and %g\n", count, hits_out[0], hits_out[1])

    ; the idiom: compare, then locate
    data:CsnArr   = csnfromarray(array(0.1, 0.8, 0.3, 0.95, 0.2))
    loud:CsnArr   = csngt(data, 0.5)
    where:CsnArr  = csnargnonzero(loud)
    where_out:i[] = csntoarray(csnflatten(where))
    where_n:i     = csnsize(where)
    prints("above 0.5: %d, at %g and %g\n", where_n, where_out[0], where_out[1])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnargwhere](csnargwhere.md)
* [csnargisnan](csnargisnan.md)
* [csncntnz](csncntnz.md)
* [csngt](csngt.md)

## Credits

Pasquale Mainolfi, 2026
