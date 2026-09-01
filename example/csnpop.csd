<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnpop.csd
;
; csnpush and csnpop together make an array a stack. The capacity stays, so the
; room a pop frees is reused by the next push.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    cap:i[]    = fillarray(4)
    stack:CsnArr = csnempty(cap)

    csnpush(stack, 1)
    csnpush(stack, 2)
    csnpush(stack, 3)

    top:i      = csnpop(stack)
    next:i     = csnpop(stack)
    left:i     = csnsize(stack)
    prints("popped %g then %g, %d left\n", top, next, left)

    ; guard the empty case
    last:i     = csnpop(stack)
    empty:i    = csnisempty(stack)
    prints("popped %g, empty = %d\n", last, empty)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
