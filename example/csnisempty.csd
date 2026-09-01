<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnisempty.csd
;
; An empty array travels through the suite instead of stopping it, but min, max
; and the variance family have no answer over an empty extent. Guard those.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    cap:i[]      = fillarray(4)
    buf:CsnArr   = csnempty(cap)
    before:i     = csnisempty(buf)

    ; safe over an empty array
    total:i      = csnsum(buf)
    prints("empty = %d, sum = %g\n", before, total)

    ; guard the ones that are not
    if before == 0 then
        peak:i = csnmax(buf)
        prints("peak = %g\n", peak)
    else
        prints("no peak: the array is empty\n")
    endif

    csnpush(buf, 7)
    csnpush(buf, 3)
    after:i      = csnisempty(buf)
    peak_now:i   = csnmax(buf)
    prints("after two pushes: empty = %d, peak = %g\n", after, peak_now)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
