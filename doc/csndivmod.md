# csndivmod

## Abstract

Quotient and remainder in one pass, as two arrays.

## Description

`csndivmod` returns both the floor quotient and the remainder of a division, in a
single pass over the data — the two results a separate division and modulo would
compute twice.

The convention is Python's and NumPy's, not C's: the quotient is floored, so
`-7 divmod 3` gives `-3` and `2`, and the remainder always carries the sign of
the divisor. That is what makes the remainder usable directly as a wrapped index.

Both operands may be arrays, broadcast against each other, and both scalar orders
exist.

Real only.

## Syntax

```csound
quot:CsnArr, rem:CsnArr = csndivmod(a:CsnArr, b:CsnArr)
quot:CsnArr, rem:CsnArr = csndivmod(a:CsnArr, b:CsnArr, trig:k)
quot:CsnArr, rem:CsnArr = csndivmod(a:CsnArr, value:i)
quot:CsnArr, rem:CsnArr = csndivmod(a:CsnArr, value:k)
quot:CsnArr, rem:CsnArr = csndivmod(value:i, b:CsnArr)
quot:CsnArr, rem:CsnArr = csndivmod(value:k, b:CsnArr)
```

## Arguments

* `a:CsnArr`: the dividend.
* `b:CsnArr`: the divisor; broadcast against `a`.
* `value:i / value:k`: a scalar, on either side of the operation.
* `trig:k` (optional, default `1`): k-rate trigger. A zero trigger republishes the previous results.

## Output

* `quot:CsnArr`: handle of the floor quotients.
* `rem:CsnArr`: handle of the remainders.

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
; csndivmod.csd
;
; Floor division, Python-style: -7 divmod 3 is -3 remainder 2, so the remainder
; is always usable as a wrapped index.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    data:CsnArr  = csnfromarray(array(7, -7, 10))

    quot:CsnArr, rem:CsnArr = csndivmod(data, 3)
    quot_out:i[] = csntoarray(quot)
    rem_out:i[]  = csntoarray(rem)
    prints("quotient  = %g %g %g\n", quot_out[0], quot_out[1], quot_out[2])
    prints("remainder = %g %g %g\n", rem_out[0], rem_out[1], rem_out[2])

    ; the remainder wraps an index into range whatever the sign
    idx:CsnArr   = csnfromarray(array(-2, -1, 0, 5, 9))
    q2:CsnArr, wrapped:CsnArr = csndivmod(idx, 4)
    wrapped_out:i[] = csntoarray(wrapped)
    prints("wrapped   = %g %g %g %g %g\n", wrapped_out[0], wrapped_out[1], wrapped_out[2], wrapped_out[3], wrapped_out[4])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csndiv](csndiv.md)
* [csnfloor](csnfloor.md)
* [csnwrap](csnwrap.md)

## Credits

Pasquale Mainolfi, 2026
