<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnrtlockend.csd
;
; Ends the section csnrtlockstart opened. It stops new arrays from being
; marked; it does not unmark what was created while the section was open,
; which is what csnrtunlock does, one handle at a time.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 4410
0dbfs = 1

instr 1
    trig:k = 1
    csnrtlockstart
    marked:CsnArr = csnzeros(fillarray(4))
    csnrtlockend

    ; created after the section ended, so it is not marked and can grow
    source:CsnArr = csnarange(0, 64, 1)
    n:k init 4
    n      = 40
    free:CsnArr = csnhead(source, n, trig)
    free_size:k = csnsize(free)

    ; the array from inside the section is still marked: this releases that one
    csnrtunlock marked

    printks "after the section: %d\n", 0, free_size
endin

; a section left open ends on its own when the note ends
instr 2
    csnrtlockstart
    prints("2: section opened and never ended\n")
    turnoff
endin

instr 3
    trig:k = 1
    source:CsnArr = csnarange(0, 64, 1)
    n:k init 4
    n      = 32
    later:CsnArr = csnhead(source, n, trig)
    later_size:k = csnsize(later)
    printks "3: unaffected by instr 2, size %d\n", 0, later_size
endin

</CsInstruments>
<CsScore>
i 1 0   0.1
i 2 0.2 0.05
i 3 0.3 0.1
</CsScore>
</CsoundSynthesizer>
