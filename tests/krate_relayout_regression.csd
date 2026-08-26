<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>

<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

/* An in-place opcode rewrites the layout of an array that another k-rate
   opcode owns as its output slot. The shape the producer asks for never
   changes, so its slot is not reallocated: the producer must still republish
   its own layout on the next pass instead of inheriting the mutated one. */

giValues[] = array(1, 2, 3, 4, 5, 6)
giShape[] = array(2, 3)

Flat@global:CsnArr = csnfromarray(giValues)
Source@global:CsnArr = csnreshape(Flat, giShape)

Flipped@global:CsnArr = csnflip(Source, -1)
Transposed@global:CsnArr = csntranspose(Source)
Zeros@global:CsnArr = csnzeros(giShape)

instr 1
    kAxis = -1
    kShape[] = fillarray(2, 3)

    Flipped = csnflip(Source, kAxis)
    Transposed = csntranspose(Source)
    Zeros = csnzeros(kShape, 0)
endin

instr 2
    csnflatten(Flipped)
    csnflatten(Transposed)
    csnflatten(Zeros)
endin

instr 3
    i01[] = array(0, 1)
    i12[] = array(1, 2)
    i20[] = array(2, 0)

    iFlipShape[] = csnshape(Flipped)
    iTransShape[] = csnshape(Transposed)
    iZeroShape[] = csnshape(Zeros)

    iFlipNdim = lenarray(iFlipShape)
    iTransNdim = lenarray(iTransShape)
    iZeroNdim = lenarray(iZeroShape)

    assert(iFlipNdim == 2 && iTransNdim == 2 && iZeroNdim == 2)
    assert(iFlipShape[0] == 2 && iFlipShape[1] == 3)
    assert(iTransShape[0] == 3 && iTransShape[1] == 2)
    assert(iZeroShape[0] == 2 && iZeroShape[1] == 3)

    iFlipLast = csnget(Flipped, i12)
    iTransUpper = csnget(Transposed, i01)
    iTransLower = csnget(Transposed, i20)
    iZeroLast = csnget(Zeros, i12)

    assert(iFlipLast == 1)
    assert(iTransUpper == 4 && iTransLower == 3)
    assert(iZeroLast == 0)
    assert(csnsize(Flipped) == 6 && csnsize(Transposed) == 6 && csnsize(Zeros) == 6)
endin
</CsInstruments>

<CsScore>
i 1 0 0.012
i 2 0.004 0.004
i 3 0.010 0.001
e
</CsScore>
</CsoundSynthesizer>
