<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>

<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

SearchSource@global:CsnArr = csnfromarray(array(1, 3, 3, 5))
SearchQueries@global:CsnArr = csnfromarray(array(0, 3, 4, 6))
SearchLeft@global:CsnArr = csnempty(array(0))
SearchRight@global:CsnArr = csnempty(array(0))
SearchHeld@global:CsnArr = csnempty(array(0))

EmptySearchSource@global:CsnArr = csnempty(array(0))
EmptySearchQueries@global:CsnArr = csnempty(array(0))
SearchInEmpty@global:CsnArr = csnempty(array(0))
SearchNoQueries@global:CsnArr = csnempty(array(0))

SwitchSource@global:CsnArr = csnfromarray(array(1, 3, 3, 5))
SwitchAlternative@global:CsnArr = csnfromarray(array(10, 20, 30, 40))

gkNeedle init 3
gkSwitchNeedle init 3
gkScalarLeft init -1
gkScalarRight init -1
gkScalarHeld init -1
gkScalarSwitched init -1

instr 1
    kAlways init 1
    kNever init 0

    SearchLeft = csnsearchsorted(SearchSource, SearchQueries, 0, kAlways)
    SearchRight = csnsearchsorted(SearchSource, SearchQueries, 1, kAlways)
    SearchHeld = csnsearchsorted(SearchSource, SearchQueries, 0, kNever)

    SearchInEmpty = csnsearchsorted(EmptySearchSource, SearchQueries, 0, kAlways)
    SearchNoQueries = csnsearchsorted(SearchSource, EmptySearchQueries, 0, kAlways)

    gkScalarLeft = csnsearchsorted(SearchSource, gkNeedle, 0, kAlways)
    gkScalarRight = csnsearchsorted(SearchSource, gkNeedle, 1, kAlways)
    gkScalarHeld = csnsearchsorted(SearchSource, gkNeedle, 1, kNever)
    gkScalarSwitched = csnsearchsorted(SwitchSource, gkSwitchNeedle, 0, kAlways)
endin

instr 2
    /* Both operand versions must invalidate the array cache. The source stays
       sorted, while the query mutation changes the first result. */
    kIndex0[] = fillarray(0)
    csnset SearchSource, kIndex0, 3
    csnset SearchQueries, kIndex0, 3

    /* The scalar cache must follow both a changed value and a different array
       identity, even when the replacement has equally young counters. */
    gkNeedle = 5
    SwitchSource = SwitchAlternative
endin

instr 3
    iLeft[] = csntoarray(SearchLeft)
    iRight[] = csntoarray(SearchRight)
    iHeld[] = csntoarray(SearchHeld)
    iInEmpty[] = csntoarray(SearchInEmpty)

    assert(iLeft[0] == 0 && iLeft[1] == 0 && iLeft[2] == 3 && iLeft[3] == 4)
    assert(iRight[0] == 3 && iRight[1] == 3 && iRight[2] == 3 && iRight[3] == 4)

    /* A zero trigger retains the init result, for arrays and scalar outputs. */
    assert(iHeld[0] == 0 && iHeld[1] == 1 && iHeld[2] == 3 && iHeld[3] == 4)
    assert(i(gkScalarHeld) == 3)

    assert(i(gkScalarLeft) == 3 && i(gkScalarRight) == 4)
    assert(i(gkScalarSwitched) == 0)

    assert(csnsize(SearchInEmpty) == 4)
    assert(iInEmpty[0] == 0 && iInEmpty[1] == 0 && iInEmpty[2] == 0 && iInEmpty[3] == 0)
    assert(csndims(SearchNoQueries) == 1 && csnsize(SearchNoQueries) == 0)
endin
</CsInstruments>

<CsScore>
i1 0    0.3
i2 0.05 0.001
i3 0.20 0
</CsScore>
</CsoundSynthesizer>
