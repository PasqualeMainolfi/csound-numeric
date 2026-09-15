<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>

<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

giIndexSource[] = fillarray(10, 20, 30)

IndexSource@global:CsnArr = csnfromarray(giIndexSource)
IndexResult@global:CsnArr = csnfromarray(giIndexSource)

gkIndexWanted init 20

instr 1
    /* The changing k value selects the performance overload; its optional
       trigger is deliberately omitted and therefore defaults to one. */
    IndexResult = csnindexof(IndexSource, gkIndexWanted)
endin

instr 2
    /* A changed scalar must invalidate the cached coordinate. */
    gkIndexWanted = 30
endin

instr 3
    iCoordinate[] = csntoarray(IndexResult)
    assert(csnsize(IndexResult) == 1 && iCoordinate[0] == 2)
endin

instr 4
    /* A source write must invalidate the cache and produce an empty result. */
    kIndex2[] = fillarray(2)
    csnset IndexSource, kIndex2, 99
endin

instr 5
    assert(csnsize(IndexResult) == 0 && csnisempty(IndexResult) == 1)
endin
</CsInstruments>

<CsScore>
i1 0    0.3
i2 0.05 0.001
i3 0.10 0
i4 0.15 0.001
i5 0.20 0
</CsScore>
</CsoundSynthesizer>
