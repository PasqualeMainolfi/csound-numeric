<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnrtlockblock.csd
;
; Marks every array created after it, for this note only. What was created
; before is untouched, and so is every other instrument: the block belongs to
; the instance that opened it and closes with csnrtunlockblock or with the note.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 4410
0dbfs = 1

instr 1
    trig:k     = 1
    source:CsnArr = csnarange(0, 64, 1)

    ; created before the block: free to be resized at k-rate
    before_n:k init 4
    before_n   = 40
    before:CsnArr = csnhead(source, before_n, trig)
    before_size:k = csnsize(before)

    csnrtlockblock

    ; created inside the block: marked, and its producer refuses to resize it
    inside:CsnArr = csnzeros(fillarray(8))
    inside_n:k    = csnsize(inside)

    csnrtunlockblock

    ; created after the block: free again
    after_n:k init 4
    after_n    = 24
    after:CsnArr  = csnhead(source, after_n, trig)
    after_size:k  = csnsize(after)

    printks "before %d, inside %d, after %d\n", 0, before_size, inside_n, after_size
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
