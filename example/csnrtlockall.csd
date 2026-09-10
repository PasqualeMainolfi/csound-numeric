<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnrtlockall.csd
;
; Declared in the orchestra header, where it holds for the whole performance:
; every array created from here on is marked, in every instrument and inside
; every user-defined opcode. There is no way to switch it back off, and that is
; the point - what one note could turn off, another note would be relying on.
; Release the individual arrays that may move with csnrtunlock.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 4410
0dbfs = 1

csnrtlockall

opcode growing_buffer, k, 0
    trig:k = 1
    source:CsnArr = csnarange(0, 64, 1)
    n:k init 4
    n      = 40
    ; marked like everything else: the mark reaches inside opcodes too
    held:CsnArr = csnhead(source, n, trig)
    csnrtunlock held
    held_n:k = csnsize(held)
    xout held_n
endop

instr 1
    ; a fixed shape is all this asks for, so the mark costs nothing
    buffer:CsnArr = csnzeros(fillarray(16))
    gain:k        = 0.5
    scaled:CsnArr = csnmul(buffer, gain, 1)
    scaled_n:k    = csnsize(scaled)

    ; and one that does need to move, released by hand
    grown:k = growing_buffer()

    printks "fixed %d, released %d\n", 0, scaled_n, grown
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
