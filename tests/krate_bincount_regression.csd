<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>

<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

BinSource@global:CsnArr = csnfromarray(array(0, 1, 1, 3))
BinWeights@global:CsnArr = csnfromarray(array(0.5, 1, 2, 4))
BinCounts@global:CsnArr = csnempty(array(0))
WeightedCounts@global:CsnArr = csnempty(array(0))
EmptySource@global:CsnArr = csnempty(array(0))
EmptyCounts@global:CsnArr = csnempty(array(0))

instr 1
    kTrig init 1
    BinCounts = csnbincount(BinSource, kTrig)
    WeightedCounts = csnbincount(BinSource, BinWeights, kTrig)
    EmptyCounts = csnbincount(EmptySource, kTrig)
endin

instr 2
    iCounts[] = csntoarray(BinCounts)
    iWeighted[] = csntoarray(WeightedCounts)
    assert(csnsize(BinCounts) == 4)
    assert(iCounts[0] == 1 && iCounts[1] == 2 && iCounts[2] == 0 && iCounts[3] == 1)
    assert(iWeighted[0] == 0.5 && iWeighted[1] == 3 && iWeighted[2] == 0 && iWeighted[3] == 4)
    assert(csnsize(EmptyCounts) == 0 && csnisempty(EmptyCounts) == 1)
endin

instr 3
    kIndex0[] = fillarray(0)
    kIndex1[] = fillarray(1)
    csnset BinSource, kIndex0, 2
    csnset BinWeights, kIndex1, 10
endin

instr 4
    /* Both source and weight versions must invalidate the cached outputs.
       The maximum remains three, exercising reuse and clearing of the slot. */
    iCounts[] = csntoarray(BinCounts)
    iWeighted[] = csntoarray(WeightedCounts)
    assert(iCounts[0] == 0 && iCounts[1] == 2 && iCounts[2] == 1 && iCounts[3] == 1)
    assert(iWeighted[0] == 0 && iWeighted[1] == 12 && iWeighted[2] == 0.5 && iWeighted[3] == 4)
endin
</CsInstruments>

<CsScore>
i1 0    0.3
i2 0.05 0
i3 0.10 0.001
i4 0.20 0
</CsScore>
</CsoundSynthesizer>
