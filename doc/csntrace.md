# csntrace

## Abstract

Sum of the diagonal of a matrix.

## Description

`csntrace` adds up the elements on the main diagonal — those whose coordinates
are all equal. For an `n × n` matrix that is `m[0][0] + m[1][1] + … `.

The trace is invariant under a change of basis, so it is the cheapest thing to
compare when checking that two matrices describe the same map. Over
[csnidentity](csnidentity.md) it returns the order of the matrix.

Both real and complex arrays are accepted; a complex trace comes back as a
`:Complex;`.

## Syntax

```csound
value:i = csntrace(source:CsnArr)
value:k = csntrace(source:CsnArr)
value:k = csntrace(source:CsnArr, trig:k)
value:Complex = csntrace(source:CsnArr)
value:Complex = csntrace(source:CsnArr, trig:k)
```

## Arguments

* `source:CsnArr`: the matrix to read.
* `trig:k` (optional, default `1`): k-rate trigger. A zero trigger republishes the previous result.

## Output

* `value:i / value:k`: the sum of the diagonal, for a real matrix.
* `value:Complex`: the sum of the diagonal, for a complex matrix.

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
; csntrace.csd
;
; The sum of the diagonal. Over the identity it is the order of the matrix, and
; csndiag is the same diagonal as an array.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    shape:i[]    = fillarray(3, 3)
    mat:CsnArr   = csnreshape(csnfromarray(array(1, 2, 3, 4, 5, 6, 7, 8, 9)), shape)

    tr:i         = csntrace(mat)
    prints("trace = %g\n", tr)

    ; the same thing the long way
    diag:CsnArr  = csndiag(mat)
    diag_out:i[] = csntoarray(diag)
    by_sum:i     = csnsum(diag)
    prints("diagonal = %g %g %g, sum = %g\n", diag_out[0], diag_out[1], diag_out[2], by_sum)

    ; over the identity it is the order
    eye:CsnArr   = csnidentity(4)
    order:i      = csntrace(eye)
    prints("trace of the 4 x 4 identity = %g\n", order)
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
* [csnidentity](csnidentity.md)
* [csnmatmul](csnmatmul.md)
* [csnsum](csnsum.md)

## Credits

Pasquale Mainolfi, 2026
