<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>

<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

Array@global:CsnArr = csnempty(array(1))
Alias@global:CsnArr = csnempty(array(1))

instr 1
    kShape[] = init(1)
    kElapsed = timeinsts()
    kShape[0] = (kElapsed < 0.035 ? 4 : 6)
    kType = (kElapsed < 0.05 ? 0 : 1)
    Array = csnempty(kShape, kType)
endin

; Capture an alias to the original handle. Later probes use both names: if
; csnempty replaced the slot or changed its generation, the alias would stop
; observing the updated layout.
instr 2
    iShape[] = csnshape(Array)
    assert(csnisempty(Array) == 1 && csnsize(Array) == 0)
    assert(iShape[0] == 4)
    assert(csntype(Array) == 0)
    Alias = Array
    csnpush(Array, 17)
    assert(csnsize(Alias) == 1)
endin

instr 3
    ; The next k-cycle must empty the same slot even if its request is unchanged.
    iShape[] = csnshape(Array)
    iAliasShape[] = csnshape(Alias)
    assert(csnsize(Array) == 0 && csnsize(Alias) == 0)
    assert(iShape[0] == 4 && iAliasShape[0] == 4)
endin

instr 4
    ; The slot and generation remain stable while the capacity request is stable.
    iShape[] = csnshape(Array)
    iAliasShape[] = csnshape(Alias)
    assert(csnsize(Array) == 0 && csnsize(Alias) == 0)
    assert(iShape[0] == 4 && iAliasShape[0] == 4)
    assert(csntype(Array) == 0 && csntype(Alias) == 0)
endin

instr 5
    iShape[] = csnshape(Array)
    iAliasShape[] = csnshape(Alias)
    assert(csnsize(Array) == 0 && csnsize(Alias) == 0)
    assert(iShape[0] == 6 && iAliasShape[0] == 6)
    assert(csntype(Array) == 0 && csntype(Alias) == 0)
endin

instr 6
    assert(csnsize(Array) == 0 && csnsize(Alias) == 0)
    assert(csntype(Array) == 1 && csntype(Alias) == 1)
endin
</CsInstruments>

<CsScore>
i 1 0 0.07
i 2 0.01 0.001
i 3 0.015 0.001
i 4 0.025 0.001
i 5 0.04 0.001
i 6 0.055 0.001
e
</CsScore>
</CsoundSynthesizer>
