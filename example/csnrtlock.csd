<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnrtlock.csd
;
; csnrtlock marks a handle as belonging to a real-time path: neither it nor any
; array derived from it afterwards may reallocate during performance, because a
; malloc on the audio thread is what a dropout sounds like.
;
; The audio sources set that mark themselves. csnrtlock is for the chains that
; never touch audio but still run under a deadline.
;
; It runs at init, so it only reaches arrays created after it in the orchestra.
; -----------------------------------------------------------------------------

sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

instr 1
    shape:i[] = fillarray(32)
    fill:k    init 0
    trig:k    init 1
    grow:k    = timeinstk() * 16

    src:CsnArr = csnzeros(shape)
    csnrtlock src, 1

    ; the padding grows every pass, so this output would have to be
    ; reallocated: refused, with the variable named
    padded:CsnArr = csnpad(src, grow, grow, fill, trig)
endin

; the same chain without the mark: allocating during performance is allowed
instr 2
    shape:i[] = fillarray(32)
    fill:k    init 0
    trig:k    init 1
    grow:k    = timeinstk() * 16

    src:CsnArr    = csnzeros(shape)
    padded:CsnArr = csnpad(src, grow, grow, fill, trig)

    size:k = csnsize(padded)
    if timeinstk() == 8 then
        printf("unmarked chain still growing: %d elements\n", 1, size)
    endif
endin

</CsInstruments>
<CsScore>
i 1 0   0.01
i 2 0.1 0.01
</CsScore>
</CsoundSynthesizer>
