# csnidentity

## Abstract

Create an identity matrix.

## Description

`csnidentity` builds an `n × n` matrix with ones on the main diagonal and zeros
everywhere else. It is the neutral element of [csnmatmul](csnmatmul.md), and the
usual starting point for a transform matrix you then fill in with
[csnset](csnset.md) or [csnsetslice](csnsetslice.md).

`itype` is read at init: `1` gives a complex identity, whose diagonal is `1 + 0i`.

## Syntax

```csound
handle:CsnArr = csnidentity(n:i)
handle:CsnArr = csnidentity(n:i, itype:i)
handle:CsnArr = csnidentity(n:k)
handle:CsnArr = csnidentity(n:k, itype:i)
```

## Arguments

* `n:i / n:k`: the order of the matrix; the result is `n × n`.
* `itype:i` (optional, default `0`): `0` for a real matrix, `1` for a complex one.

## Output

* `handle:CsnArr`: handle of the new `n × n` matrix.

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
; csnidentity.csd
;
; csnidentity builds the n x n identity. Multiplying by it leaves a matrix
; alone, which is the quickest way to check a matmul chain.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    eye:CsnArr   = csnidentity(3)
    eye_out:i[]  = csntoarray(csnflatten(eye))
    prints("row 0 = %g %g %g\n", eye_out[0], eye_out[1], eye_out[2])
    prints("row 1 = %g %g %g\n", eye_out[3], eye_out[4], eye_out[5])
    prints("row 2 = %g %g %g\n", eye_out[6], eye_out[7], eye_out[8])

    tr:i         = csntrace(eye)
    prints("trace = %g\n", tr)

    ; the identity leaves a matrix alone
    shape:i[]    = fillarray(3, 3)
    mat:CsnArr   = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6, 7, 8, 9)), shape)
    same:CsnArr  = csnmatmul(mat, eye)
    same_out:i[] = csntoarray(csnflatten(same))
    prints("mat * I row 2 = %g %g %g\n", same_out[6], same_out[7], same_out[8])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csndiag](csndiag.md)
* [csnmatmul](csnmatmul.md)
* [csntrace](csntrace.md)
* [csnzeros](csnzeros.md)

## Credits

Pasquale Mainolfi, 2026
