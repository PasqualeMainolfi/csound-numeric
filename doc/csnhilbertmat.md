# csnhilbertmat

## Abstract

Build the Hilbert matrix of a given size.

## Description

`csnhilbertmat` returns the `n` by `n` matrix whose entries are

```
H[i][j] = 1 / (i + j + 1)
```

matching `scipy.linalg.hilbert`. It shares a name with `csnhilbert1d` and
nothing else: this is a matrix of constants, not a transform, and it takes a
size rather than an array.

It is the textbook example of an ill-conditioned matrix. The determinant falls
away almost immediately, reaching about `1.65e-07` at `n = 4` and sinking below
what double precision can carry not long after, which makes it the standard
probe for how a solver behaves when a system is barely solvable. Fed to
`csnsolve`, `csninv` or `csndet` it is a test, not a workload.

A size of zero yields an empty zero by zero array rather than an error.

## Syntax

```csound
matrix:CsnArr = csnhilbertmat(n:i)
matrix:CsnArr = csnhilbertmat(n:k, trig:k)
```

## Arguments

* `n:i` / `n:k`: side of the matrix. The k-rate form rebuilds it whenever this changes.
* `trig:k` (optional, default `1`): k-rate trigger. Zero republishes the previous result.

## Output

* `matrix:CsnArr`: a real `(n, n)` array.

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
sr = 48000
ksmps = 32
0dbfs = 1

instr 1
    matrix:CsnArr = csnhilbertmat(4)
    csnprint matrix
    ; Famously ill-conditioned: the determinant is already near zero at n = 4.
    prints("det = %.6e\n", csndet(matrix))
    turnoff
endin
</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnidentity](csnidentity.md)
* [csndet](csndet.md)
* [csninv](csninv.md)
* [csnsolve](csnsolve.md)

## Credits

Pasquale Mainolfi, 2026
