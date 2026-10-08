# csni

## Abstract

Freeze an array at its init value, as `i()` does for a scalar.

## Description

`csni(A)` returns a copy of `A` taken at init and never updated afterwards,
the counterpart of `i(kx)`. It is i-rate, so an operation on it alone is an
init-time computation, even when `A` keeps moving at k-rate.

A k-rate result that depends on k values is not computed at init, so its
frozen copy is what it holds at that moment: empty, or the value it started
from. `i(kx)` reads a k variable the same way.

In place, `csni A` makes no change: an array is i-rate until a k-rate opcode
produces or writes it, and the rate only rises, so `csni A` on an array that
is already k-rate is an init error. Lowering it would leave it rewritten at
performance time by the opcodes that made it k while the operators ignored
it. Use the expression form to take a frozen copy.

## Syntax

```csound
csni source:CsnArr
out:CsnArr = csni(source:CsnArr)
```

## Arguments

* `source:CsnArr`: the array to freeze (expression form), or to check is not
  k-rate (in place).

## Output

* `out:CsnArr`: an i-rate copy of `source`, taken at init.

## Execution Time

* Init

## Examples

```csound
<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csni.csd
;
; csni(A) is to an array what i(kx) is to a scalar: a copy taken at init and
; never updated. live accumulates at k-rate; frozen is its value at init (0,
; since the first sum happens at performance time), and an operation on
; frozen alone stays an init-time computation.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    iShape[] = fillarray(2)
    one:CsnArr    = csnfull(iShape, 1)

    live:CsnArr   = csnzeros(iShape)
    csnk live
    live += one                     ; 1, 2, 3, ... on every pass

    frozen:CsnArr = csni(live)      ; the value of live at init
    shifted:CsnArr = csni(live) + one

    kLive[]    = csntoarray(live)
    kFrozen[]  = csntoarray(frozen)
    kShifted[] = csntoarray(shifted)
    printks("live = %g   frozen = %g   shifted = %g\n", 0, kLive[0], kFrozen[0], kShifted[0])

    kPass init 0
    kPass += 1
    if kPass == 4 then
        turnoff
    endif
endin

</CsInstruments>
<CsScore>
i 1 0 1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnk](csnk.md)
* [csncopy](csncopy.md)

## Credits

Pasquale Mainolfi, 2026
