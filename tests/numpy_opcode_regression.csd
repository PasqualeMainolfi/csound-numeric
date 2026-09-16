<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>

<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

Seed@global:CsnArr = csnfromarray(array(1, 2, 3, 4, 5, 6))
KLoaded@global:CsnArr = csnempty(array(0))

instr 1
    iShape[] = fillarray(2, 3)
    iGrid:CsnArr = csnreshape(Seed, iShape)

    csnsave iGrid, "csnum_opcode_grid.csn"
    iCsn:CsnArr = csnload("csnum_opcode_grid.csn")
    iCsnValues[][] = csntoarray(iCsn)
    assert(iCsnValues[1][2] == 6)

    csnsave iGrid, "csnum_opcode_grid.npy"
    iNpy:CsnArr = csnload("csnum_opcode_grid.npy")
    iNpyValues[][] = csntoarray(iNpy)
    assert(csnsize(iNpy) == 6 && iNpyValues[0][1] == 2 && iNpyValues[1][2] == 6)

    iFloat32:CsnArr = csnload("csnum_opcode_float32.npy")
    iFloat32Values[][] = csntoarray(iFloat32)
    assert(csnsize(iFloat32) == 6 && iFloat32Values[0][1] == 2 && iFloat32Values[1][2] == 6)

    Filler:Complex = init(3, -4, 0)
    iComplex:CsnArr = csnfull(iShape, Filler)
    csnsave iComplex, "csnum_opcode_complex.npy"
    iComplexBack:CsnArr = csnload("csnum_opcode_complex.npy")
    iReal:CsnArr = csnreal(iComplexBack)
    iImag:CsnArr = csnimag(iComplexBack)
    iRe[][] = csntoarray(iReal)
    iIm[][] = csntoarray(iImag)
    assert(csntype(iComplexBack) == 1 && iRe[1][2] == 3 && iIm[1][2] == -4)
endin

instr 2
    kTrig = 1
    csnsave.k Seed, "csnum_opcode_k.npy", kTrig
    KLoaded = csnload("csnum_opcode_k.npy", kTrig)
endin

instr 3
    iValues[] = csntoarray(KLoaded)
    assert(csnsize(KLoaded) == 6 && iValues[0] == 1 && iValues[5] == 6)
endin
</CsInstruments>

<CsScore>
i 1 0     0.001
i 2 0.002 0.020
i 3 0.010 0.001
e
</CsScore>
</CsoundSynthesizer>
