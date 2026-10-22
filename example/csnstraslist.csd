<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnstraslist.csd
;
; A melody written as note letters, one per step, and a title whose accented
; letters stay whole.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    melody:CsnStr  = csnstraslist("CEGECEGA")
    csnprint(melody)
    title:CsnStr   = csnstraslist("Ré♭")
    csnprint(title)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
