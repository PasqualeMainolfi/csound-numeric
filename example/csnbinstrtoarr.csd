<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnbinstrtoarr.csd
;
; A drum pattern written as binary digits, one step each, and the hits it
; holds.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    kick:CsnArr    = csnbinstrtoarr("0b1000100010001000")
    csnprint(kick)
    hits:i         = csnsum(kick)
    prints("kick hits %d of %d steps\n", hits, csnsize(kick))
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
