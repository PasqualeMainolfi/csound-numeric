<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnfromftable.csd
;
; csnfromftable pulls a function table into the suite. From there the whole
; array vocabulary applies: statistics, windows, normalisation, and back out
; through csntoftable.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    wave:CsnArr = csnfromftable(1)
    n:i         = csnsize(wave)
    peak:i      = csnmax(wave)
    trough:i    = csnmin(wave)
    avg:i       = csnmean(wave)
    prints("points = %d, min = %.3f, max = %.3f, mean = %.6f\n", n, trough, peak, avg)

    ; the copy is 1-D and real, whatever the table held
    dims:i      = csndims(wave)
    itype:i     = csntype(wave)
    prints("dims = %d, itype = %d\n", dims, itype)
    turnoff
endin

</CsInstruments>
<CsScore>
f 1 0 1024 10 1
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
