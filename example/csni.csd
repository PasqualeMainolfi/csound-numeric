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
