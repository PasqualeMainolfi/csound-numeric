<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>
sr = 48000
ksmps = 32
0dbfs = 1

Source@global:CsnArr = csnfromarray(array(0, 10, 20, 30))
Output@global:CsnArr = csnempty(array(0))

instr 1
    csnrtlock Source
    kLength init 4
    if timeinstk() >= 3 then
        kLength = 6
    endif
    Output = csnresample(Source, kLength, 0, 1)
    ; Output inherited the mark, but this analysis branch may resize.
    csnrtunlock Output
    kSize = csnsize(Output)
    if timeinstk() == 5 then
        printf("unlocked output grew to %d elements\n", 1, kSize)
    endif
endin
</CsInstruments>
<CsScore>
i 1 0 0.01
</CsScore>
</CsoundSynthesizer>
