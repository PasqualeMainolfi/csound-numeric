<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>

<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

Empty@global:CsnArr = csnempty(array(2, 3))
Transposed@global:CsnArr = csntranspose(Empty)
Flipped@global:CsnArr = csnflip(Empty, 1)
Reshaped@global:CsnArr = csnflatten(Empty)

instr 1
    kZero = 0
    kShape[] = fillarray(kZero)
    kAxis = 1

    Reshaped = csnreshape(Empty, kShape)
    Flipped = csnflip(Empty, kAxis)

    ; These assertions run during instrument initialization, immediately after
    ; the k-opcode init callbacks publish their output slots.
    assert(csnsize(Transposed) == 0)
    assert(csnsize(Flipped) == 0)
    assert(csnsize(Reshaped) == 0)
endin

instr 2
    ; The same logical sizes must remain valid after multiple performance cycles.
    assert(csnsize(Transposed) == 0)
    assert(csnsize(Flipped) == 0)
    assert(csnsize(Reshaped) == 0)
endin
</CsInstruments>

<CsScore>
i 1 0 0.005
i 2 0.003 0.001
e
</CsScore>
</CsoundSynthesizer>
