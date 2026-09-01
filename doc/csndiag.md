# csndiag

## Abstract

Diagonal of a matrix, or a matrix from a diagonal.

## Description

`csndiag` reads its argument's rank and does the matching thing, as NumPy's
`diag` does.

* Given a **matrix** it extracts the main diagonal and returns it as a vector.
* Given a **vector** of `n` elements it builds an `n × n` matrix with those
  values on the main diagonal and zeros everywhere else.

The two are inverse up to the off-diagonal elements, which the round trip
discards. Building a diagonal matrix is how a per-element gain becomes a linear
map that [csnmatmul](csnmatmul.md) can apply.

Both real and complex arrays are accepted.

## Syntax

```csound
handle:CsnArr = csndiag(source:CsnArr)
handle:CsnArr = csndiag(source:CsnArr, trig:k)
```

## Arguments

* `source:CsnArr`: a matrix, to extract its diagonal, or a vector, to build a matrix from it.
* `trig:k`: k-rate trigger. A zero trigger republishes the previous result.

## Output

* `handle:CsnArr`: the diagonal as a vector, or the matrix built from it.

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
; csndiag.csd
;
; A matrix in gives its diagonal; a vector in gives a diagonal matrix. That is
; how a per-element gain becomes a linear map.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    shape:i[]     = fillarray(3, 3)
    mat:CsnArr    = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6, 7, 8, 9)), shape)

    diag:CsnArr   = csndiag(mat)
    diag_out:i[]  = csntoarray(diag)
    diag_dims:i   = csndims(diag)
    prints("diagonal (dims %d) = %g %g %g\n", diag_dims, diag_out[0], diag_out[1], diag_out[2])

    ; the other direction: a vector becomes a diagonal matrix
    gains:CsnArr  = csnfromarray(array(0.5, 1, 2))
    gain_map:CsnArr = csndiag(gains)
    map_shape:i[] = csnshape(gain_map)
    map_out:i[]   = csntoarray(csnflatten(gain_map))
    prints("gain map %g x %g, row 1 = %g %g %g\n", map_shape[0], map_shape[1], map_out[3], map_out[4], map_out[5])

    ; and it applies as a linear map
    scaled:CsnArr = csnmatmul(mat, gain_map)
    scaled_out:i[] = csntoarray(csnflatten(scaled))
    prints("scaled row 0 = %g %g %g\n", scaled_out[0], scaled_out[1], scaled_out[2])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csntrace](csntrace.md)
* [csnidentity](csnidentity.md)
* [csnmatmul](csnmatmul.md)

## Credits

Pasquale Mainolfi, 2026
