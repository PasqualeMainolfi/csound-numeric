<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>

<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

/* An init pass re-run inside a loop re-initialises the same opcode instance.
   Each pass used to take a fresh slot for a local output and only the last
   one reached the deinit, so the registry filled across notes.
   Three notes of 3000 passes each overflow it unless every pass recycles the
   slot its own instance created. */
instr 1
    ii = 0
    while ii < 3000 do
        sos:CsnArr = csnbuttersos(4, 1000 + ii, 0, sr)
        ii += 1
    od
    iShape[] = csnshape(sos)
    assert(iShape[0] == 2 && iShape[1] == 6)
    turnoff
endin

/* A handle aliased in with `=` belongs to the opcode that created it:
   overwriting the local that holds the alias must not free it. */
instr 2
    src:CsnArr = csnfromarray(array(1, 2, 3))
    dst:CsnArr = src
    dst = csnfromarray(array(9, 9))
    iSrc = csnsize(src)
    iDst = csnsize(dst)
    assert(iSrc == 3 && iDst == 2)
    iVals[] = csntoarray(src)
    assert(iVals[0] == 1 && iVals[1] == 2 && iVals[2] == 3)
    turnoff
endin

/* The same local as output and operand: from the second pass on the previous
   array is this instance's own, and it is still being read. */
instr 3
    x:CsnArr = csnfromarray(array(1, 2, 3, 4, 5, 6))
    iNewShape[] = fillarray(2, 3)
    x = csnreshape(x, iNewShape)
    ii = 0
    while ii < 3 do
        x = csntranspose(x)
        ii += 1
    od
    iShape[] = csnshape(x)
    assert(iShape[0] == 3 && iShape[1] == 2)
    flat:CsnArr = csnflatten(x)
    iVals[] = csntoarray(flat)
    assert(iVals[0] == 1 && iVals[1] == 4 && iVals[2] == 2 && iVals[5] == 6)
    turnoff
endin

/* A global output that is also the operand keeps its previous array alive
   until the op has read it. */
instr 4
    G@global:CsnArr = csnfromarray(array(1, 2, 3, 4))
    G@global:CsnArr = csncopy(G)
    iVals[] = csntoarray(G)
    assert(iVals[0] == 1 && iVals[1] == 2 && iVals[2] == 3 && iVals[3] == 4)
    turnoff
endin

/* A looped local that is its own operand: each pass must keep the array it
   reads, so it is released one pass later instead of never. Three notes of
   3000 passes each overflow the registry if it is not. */
instr 5
    acc:CsnArr = csnfromarray(array(1, 2))
    ii = 0
    while ii < 3000 do
        acc = csnadd(acc, 1)
        ii += 1
    od
    iVals[] = csntoarray(acc)
    assert(iVals[0] == 3001 && iVals[1] == 3002)
    turnoff
endin

/* The same through a global output, which never released an operand either. */
instr 6
    Acc@global:CsnArr = csnfromarray(array(1, 2))
    ii = 0
    while ii < 3000 do
        Acc@global:CsnArr = csnadd(Acc, 1)
        ii += 1
    od
    iVals[] = csntoarray(Acc)
    assert(iVals[0] == 3001 && iVals[1] == 3002)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.01
i 1 0.02 0.01
i 1 0.04 0.01
i 2 0.06 0.01
i 3 0.08 0.01
i 4 0.10 0.01
i 5 0.12 0.01
i 5 0.14 0.01
i 5 0.16 0.01
i 6 0.18 0.01
i 6 0.20 0.01
i 6 0.22 0.01
e 0.3
</CsScore>
</CsoundSynthesizer>
