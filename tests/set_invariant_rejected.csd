<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>

<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

giRaw[] = array(3, 1, 2)
Raw@global:CsnArr = csnfromarray(giRaw)
SetValues@global:CsnArr = csnlikeset(Raw)

instr 1
    iIndex0[] = array(0)
    csnset(SetValues, iIndex0, 99)
    iFound = csnsetcontains(SetValues, 99)
endin
</CsInstruments>

<CsScore>
i 1 0 0.001
e
</CsScore>
</CsoundSynthesizer>
