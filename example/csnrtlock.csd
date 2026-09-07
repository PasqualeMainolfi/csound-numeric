<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>
sr = 48000
ksmps = 32
0dbfs = 1

instr 1
    source:CsnArr = csnfromarray(array(1, 2, 3, 4))
    csnrtlock source

    ; This result inherits the mark. Its fixed shape allocates at init and
    ; therefore needs no allocation on the performance thread.
    kTrig init 1
    spectrum:CsnArr = csnrfft(source, 4, -1, kTrig)
    kSize = csnsize(spectrum)
    if timeinstk() == 2 then
        printf("locked fixed-size spectrum: %d bins\n", 1, kSize)
    endif
endin
</CsInstruments>
<CsScore>
i 1 0 0.01
</CsScore>
</CsoundSynthesizer>
