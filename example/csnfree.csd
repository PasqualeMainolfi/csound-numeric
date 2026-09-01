<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnfree.csd
;
; A @global array outlives its note, so nothing frees it automatically. csnfree
; releases it, from an instrument that runs after every consumer.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

cap@global:i[]       = fillarray(8)
buffer@global:CsnArr = csnzeros(cap)

instr 1
    ; producer: fill the shared buffer
    cell:i[] = fillarray(0)
    csnset(buffer, cell, 42)
    size:i   = csnsize(buffer)
    prints("instr 1: size = %d\n", size)
    turnoff
endin

instr 2
    ; consumer: the array is still there, notes later
    cell:i[] = fillarray(0)
    value:i  = csnget(buffer, cell)
    prints("instr 2: buffer[0] = %g\n", value)
    turnoff
endin

instr 99
    ; after every consumer, release it
    csnfree(buffer)
    prints("instr 99: buffer released\n")
    turnoff
endin

</CsInstruments>
<CsScore>
i 1  0   0.1
i 2  0.2 0.1
i 99 0.4 0.1
</CsScore>
</CsoundSynthesizer>
