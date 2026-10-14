# csnk

## Abstract

Make an array k-rate: in place, or as a k-rate copy inside an expression.

## Description

A `CsnArr` carries its rate. It is i-rate when an i-rate opcode built it and
k-rate once a k-rate opcode produces it or writes it. The operators (`+`, `-`,
`*`, `/`, `^`, `+=`, ...) follow their operands: an operation runs on every
control period only while one of its operands is k-rate. Built from i-rate
arguments, an array is i-rate, so `acc += step` adds once, at init.

`csnk` is the entry point that decides the rate, as `k()` does for a scalar:

* **In place**, `csnk A` marks `A` k-rate from that line on. It copies nothing
  and computes nothing; every operation that takes `A` afterwards runs at k,
  and so does everything derived from them. Use it for accumulators and
  arrays that an operator keeps rewriting.
* **In an expression**, `csnk(A)` returns a k-rate copy of `A` and leaves `A`
  itself alone: only that expression runs at k. The copy is retaken only when
  `A` changes, so an operation on a fixed array is computed once and then
  reused.

Running at k does not mean recomputing on every pass. A k-rate operation skips
its work while its operands and its result are unchanged; only an operation
written over its own operand, as `A += B`, applies on every pass.

Cast before use. The operators decide at init whether to compute there, and an
operator that met `A` still i-rate has already applied once at init when a
later `csnk A` makes it k: `A += B` would then add one extra time.

Assigning the expression form, `D = csnk(F)`, is a k-rate assignment: it
recopies `F` on every pass, as `kx = 5` does, so a following `D += B` gives the
same result on every pass. To accumulate, use the in-place form.

The cast reaches the operators only. A named opcode chooses its rate by its
trigger: without one it is the i-rate form and runs once at init whatever the
rate of its operands; pass a trigger for the k-rate form.

Both forms also take a `:CsnStr;`; the copy holds its own strings. A pass
that retakes it copies them, which allocates, so on a [real-time
path](csnrtlock.md) it runs while the source holds still and is refused with
a performance error once the source changes. See [string
arrays](string-arrays.md).

## Syntax

```csound
csnk source:CsnArr
out:CsnArr = csnk(source:CsnArr)
csnk source:CsnStr
out:CsnStr = csnk(source:CsnStr)
```

## Arguments

* `source:CsnArr / source:CsnStr`: the array to make k-rate (in place) or to copy as k-rate.

## Output

* `out:CsnArr / out:CsnStr`: a k-rate copy of `source`, retaken when `source` changes.

## Execution Time

* Init
* Performance (k-rate), for the expression form

## Examples

```csound
<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnk.csd
;
; An operator runs at k-rate only while one of its operands is k-rate. Arrays
; built from i-rate arguments are i-rate, so `acc += step` would add once, at
; init. csnk marks acc k-rate: the sum then runs on every control period.
; The expression form leaves its source alone and casts only the value it
; returns.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    iShape[] = fillarray(3)
    step:CsnArr  = csnfull(iShape, 0.5)

    acc:CsnArr   = csnzeros(iShape)
    csnk acc                        ; from here on acc lives at k
    acc += step                     ; accumulates on every pass

    still:CsnArr = csnzeros(iShape)
    still += step                   ; i operands only: added once, at init

    ; expression form: only this sum is k-rate, base itself stays i
    base:CsnArr  = csnfull(iShape, 10)
    moved:CsnArr = csnk(base) + step

    kAcc[]   = csntoarray(acc)
    kStill[] = csntoarray(still)
    kMoved[] = csntoarray(moved)
    printks("acc = %g   still = %g   moved = %g\n", 0, kAcc[0], kStill[0], kMoved[0])

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

* [csni](csni.md)
* [csncopy](csncopy.md)

## Credits

Pasquale Mainolfi, 2026
