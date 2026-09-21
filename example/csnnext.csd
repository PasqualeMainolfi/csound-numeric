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
