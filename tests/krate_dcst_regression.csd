<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>

<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

; The transforms run on a real FFT of an extended signal, so the scratch
; buffers are sized once at init for one source layout. These cover the
; k-rate path: the result has to follow a value change in the source while
; that layout stays put.

giFlatValues[] = fillarray(1, 2, 3, 4, 5, 6, 7, 8)
giFlatShape[] = fillarray(8)
giGridShape[] = fillarray(2, 4)

FlatSrc@global:CsnArr = csnfromarray(giFlatValues)
FlatDct@global:CsnArr = csnzeros(giFlatShape)

GridSrc@global:CsnArr = csnreshape(csnfromarray(giFlatValues), giGridShape)
GridDct@global:CsnArr = csnzeros(giGridShape)

SinSrc@global:CsnArr = csnfromarray(giFlatValues)
SinDst@global:CsnArr = csnzeros(giFlatShape)

instr 1
    kTrig init 1
    FlatDct = csndcttwo1d(FlatSrc, kTrig)
    GridDct = csndcttwo1d(GridSrc, kTrig)
    SinDst = csndsttwo1d(SinSrc, kTrig)
endin

instr 2
    ; Same shape, new values: the only mutation the k-rate path accepts.
    i0[] = fillarray(0)
    i11[] = fillarray(1, 1)
    csnset FlatSrc, i0, 101
    csnset GridSrc, i11, 202
endin

instr 10
    i0[] = fillarray(0)
    i10[] = fillarray(1, 0)
    i7[] = fillarray(7)

    ; DCT-II bin 0 is twice the sum of the row, which makes the expected
    ; value independent of everything but the mutation.
    iFlat0 = csnget(FlatDct, i0)
    assert(abs(iFlat0 - 272) < 1e-9)

    ; Row 1 of the grid was 5,6,7,8 and instr 2 turned the 6 into 202.
    iGridRow1 = csnget(GridDct, i10)
    assert(abs(iGridRow1 - 444) < 1e-9)

    ; Untouched source: the full DST-II against its reference value.
    iSin0 = csnget(SinDst, i0)
    iSin7 = csnget(SinDst, i7)
    assert(abs(iSin0 - 46.1324780593) < 1e-9)
    assert(abs(iSin7 + 8) < 1e-9)

    assert(csnsize(FlatDct) == 8)
    assert(csnsize(GridDct) == 8)
endin
</CsInstruments>

<CsScore>
i1 0 0.3
i2 0.05 0
i10 0.2 0
</CsScore>
</CsoundSynthesizer>
