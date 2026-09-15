<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>

<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

instr 1
    Source:CsnArr = csnfromarray(array(0, 1.5, 2))
    Result:CsnArr = csnbincount(Source)
endin

instr 2
    iMatrix[][] = init(1, 2)
    Source:CsnArr = csnfromarray(iMatrix)
    Result:CsnArr = csnbincount(Source)
endin

instr 3
    Source:CsnArr = csnfromarray(array(0, 1))
    Weights:CsnArr = csnfromarray(array(1))
    Result:CsnArr = csnbincount(Source, Weights)
endin

instr 4
    /* Every failure above happens while the registry mutex is held. Reaching a
       fresh valid operation proves that error reporting neither crashed nor
       left the registry locked. */
    Source:CsnArr = csnfromarray(array(0, 1, 1))
    Result:CsnArr = csnbincount(Source)
    iValues[] = csntoarray(Result)
    assert(iValues[0] == 1 && iValues[1] == 2)
    prints("csnum registry alive after every bincount validation error\n")
endin
</CsInstruments>

<CsScore>
i1 0    0.01
i2 0.02 0.01
i3 0.04 0.01
i4 0.06 0.01
</CsScore>
</CsoundSynthesizer>
