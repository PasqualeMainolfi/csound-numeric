# csnouter

## Abstract

Outer product of two vectors.

## Description

`csnouter` multiplies every element of the first operand by every element of the
second and lays the results out as a matrix: element `[i][j]` is `a[i] * b[j]`.
An `n`-element and an `m`-element vector give an `n × m` matrix.

Where [csndot](csndot.md) contracts two vectors down to a number, `csnouter`
expands them into a plane. It is how a separable 2-D window is built from two 1-D
ones, and how a rank-1 matrix is made from a pair of vectors.

Both real and complex arrays are accepted.

## Syntax

```csound
handle:CsnArr = csnouter(a:CsnArr, b:CsnArr)
handle:CsnArr = csnouter(a:CsnArr, b:CsnArr, trig:k)
```

## Arguments

* `a:CsnArr`: first vector; supplies the rows.
* `b:CsnArr`: second vector; supplies the columns.
* `trig:k`: k-rate trigger. A zero trigger republishes the previous result.

## Output

* `handle:CsnArr`: an `n × m` matrix.

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
; csnouter.csd
;
; Two vectors in, a plane out. A separable 2-D window is the outer product of
; two 1-D ones.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    a:CsnArr      = csnfromarray(array(1, 2, 3))
    b:CsnArr      = csnfromarray(array(4, 5, 6))

    plane:CsnArr  = csnouter(a, b)
    plane_shape:i[] = csnshape(plane)
    plane_out:i[] = csntoarray(csnflatten(plane))
    prints("%g x %g\n", plane_shape[0], plane_shape[1])
    prints("row 0 = %g %g %g\n", plane_out[0], plane_out[1], plane_out[2])
    prints("row 2 = %g %g %g\n", plane_out[6], plane_out[7], plane_out[8])

    ; a separable 2-D window
    win:CsnArr    = csnhanning(4)
    win2d:CsnArr  = csnouter(win, win)
    win2d_shape:i[] = csnshape(win2d)
    peak:i        = csnmax(win2d)
    prints("2-D window %g x %g, peak = %.4f\n", win2d_shape[0], win2d_shape[1], peak)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csndot](csndot.md)
* [csninner](csninner.md)
* [csnmatmul](csnmatmul.md)
* [csnhanning](csnhanning.md)

## Credits

Pasquale Mainolfi, 2026
