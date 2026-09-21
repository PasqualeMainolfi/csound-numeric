# csnnext

## Abstract

Read one array element per control period, walking the array flat.

## Description

`csnnext` is an iterator. Each control period on a non-zero trigger it hands
back the next element of the array and raises its state output; once the array
is exhausted the state falls to 0 and the value output holds the last element
read. The array is walked flat, so the rank does not matter.

The walk is taken over a copy of the elements made at init, which is what lets
the iterator answer without locking the registry once the array is exhausted.
That copy must not go stale: a write to the source while the walk is in
progress raises an error rather than serving the old values. Reading the source
with other opcodes is fine.

`reset` is **level triggered**, not edge triggered. It rewinds to the first
element on every control period it is high, so it has to be pulsed: holding it
at 1 pins the iterator on element 0 and the value output never advances, with
no error to say so. It is 0 when omitted, so the default walk runs once and
then reports exhaustion.

Real and complex arrays are two distinct overloads, chosen by the type of the
value output: `k` for a real array, `:Complex;` for a complex one. Handing
either the other's element type is an init error, not a performance one.

Once the array is exhausted the handle is no longer resolved, so an array freed
after the walk ended leaves `csnnext` republishing its last element with a zero
state instead of reporting a dead handle.

## Syntax

```csound
value:k, state:k csnnext source:CsnArr [, trig:k] [, reset:k]
value:Complex, state:k csnnext source:CsnArr [, trig:k] [, reset:k]
```

## Arguments

* `source:CsnArr`: the array to walk. It is only read.
* `trig:k`: k-rate trigger, 1 when omitted. The iterator advances on a non-zero trigger; a zero trigger holds the outputs where they are.
* `reset:k`: k-rate rewind, 0 when omitted. While it is 1 the iterator restarts from the first element, so pulse it rather than holding it.

## Output

* `value:k` or `value:Complex`: the element just read, or the last element once the array is exhausted. The type selects the overload.
* `state:k`: 1 when an element was produced this period, 0 once the array is exhausted. An empty array reports 0 from the start.

## Execution Time

* Performance (k-rate)

The init pass validates the handle and the element type and takes the copy the
walk reads; the iteration itself only runs at performance time.

## Examples

```csound
<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnnext.csd
;
; csnnext hands back one element per control period, walking the array flat.
; The second output is the state: 1 while an element was produced, 0 once the
; array is exhausted, and the value output then holds the last element.
;
; It iterates a copy taken at init, so it is not disturbed by a reader; but a
; write to the source while the walk is in progress is refused rather than
; silently served from the stale copy.
;
; reset is level triggered: it rewinds to the first element on every control
; period it is high, so it has to be pulsed. Holding it at 1 pins the iterator
; on element 0 forever.
;
; Real and complex arrays are two distinct overloads, selected by the type of
; the value output. Feeding one the other's element type is an init error.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    ; an i-rate source: csnfromarray on a k[] carries a performance pass that
    ; re-imports the Csound array every control period, which would count as a
    ; write and abort the walk
    src:i[]      = fillarray(10, 20, 30)
    vec:CsnArr   = csnfromarray(src)

    kon          init 1
    kreset       init 0
    kpass        init 0

    kvalue, kstate csnnext vec, kon, kreset

    printf       "pass %d: state %d value %g\n", kpass + 1, kpass, kstate, kvalue

    ; pulse the reset once the walk has ended, to start it over
    if kpass == 3 then
        kreset = 1
    else
        kreset = 0
    endif

    kpass += 1
    if kpass == 7 then
        turnoff
    endif
endin

instr 2
    ; the complex overload: the value output is a :Complex;, the state stays k
    src:i[]      = fillarray(1, 2)
    base:CsnArr  = csnfromarray(src)
    j:Complex    = init(2, 5, 0)
    cpx:CsnArr   = csnmul(csntocomplex(base), j)

    kon          init 1
    kpass        init 0

    cvalue:Complex, kstate csnnext cpx, kon

    printf       "complex pass %d: state %d value %g+%gj\n", kpass + 1, kpass, kstate, real(cvalue), imag(cvalue)

    kpass += 1
    if kpass == 3 then
        turnoff
    endif
endin

</CsInstruments>
<CsScore>
i 1 0   0.1
i 2 0.2 0.1
</CsScore>
</CsoundSynthesizer>
```

## See also

* [csnget](csnget.md)
* [csnforeach](csnforeach.md)
* [csnsize](csnsize.md)
* [csntoarray](csntoarray.md)

## Credits

Pasquale Mainolfi, 2026
