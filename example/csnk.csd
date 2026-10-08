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
