<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>

<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

giKValues[] = fillarray(7, 8)
KSource@global:CsnArr = csnfromarray(giKValues)

instr 1
    prints("CSN_PRINT_MATRIX_BEGIN\n")
    values:i[] = fillarray(1.234567, 2, 3, 4)
    vec:CsnArr = csnfromarray(values)
    shape:i[] = fillarray(2, 2)
    mat:CsnArr = csnreshape(vec, shape)
    csnprint(mat)
    prints("CSN_PRINT_MATRIX_END\n")

    emptyShape:i[] = fillarray(4)
    empty:CsnArr = csnempty(emptyShape)
    csnprint(empty)

    complexValue:Complex = init(1.234567, -2.345678, 0)
    complexShape:i[] = fillarray(1)
    complexArray:CsnArr = csnfull(complexShape, complexValue)
    csnprint(complexArray)

    summarized:CsnArr = csnarange(0, 1001, 1)
    csnprint(summarized)

    /* Exactly 1000 elements stay below the summary cutoff. Their rendering is
       longer than the initial 4096-byte buffer and exercises reallocation. */
    largeShape:i[] = fillarray(1000)
    large:CsnArr = csnfull(largeShape, 12345.6789)
    prints("CSN_PRINT_LARGE_BEGIN\n")
    csnprint(large)
    prints("CSN_PRINT_LARGE_END\n")
    turnoff
endin

instr 2
    /* Two separated triggers must produce two renderings, not one followed by
       the accumulated history of both. */
    kCycle init 0
    kTrig = (kCycle == 0 || kCycle == 2 ? 1 : 0)
    csnprint(KSource, kTrig)
    kCycle += 1
    if kCycle >= 4 then
        turnoff
    endif
endin
</CsInstruments>

<CsScore>
i 1 0 0.001
i 2 0.002 0.01
e
</CsScore>
</CsoundSynthesizer>
