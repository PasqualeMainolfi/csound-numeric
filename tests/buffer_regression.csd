<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

/* csntobuffer and csnfrombuffer against numpy: every dtype's bytes as
   np.array(values).astype(dtype).tobytes() gives them, and back through
   np.frombuffer. The k forms follow a source that changes at perf. */

opcode mismatches(iGot[], iWant[]):i
    iBad = abs(lenarray(iGot) - lenarray(iWant))
    if iBad == 0 then
        iIdx = 0
        while iIdx < lenarray(iGot) do
            if iGot[iIdx] != iWant[iIdx] then
                iBad += 1
            endif
            iIdx += 1
        od
    endif
    xout(iBad)
endop

gkBuf0 init -1
gkBuf1 init -1
gkBack init -1
gkLen init -1

instr 1
    ; |u1
    iSrc0[] = fillarray(0, 1, 127, 255)
    iBytes0[] = fillarray(0, 1, 127, 255)
    Src0:CsnArr = csnfromarray(iSrc0)
    Buf0:CsnArr = csntobuffer(Src0, "|u1")
    iGotBytes0[] = csntoarray(Buf0)
    iBad = mismatches(iGotBytes0, iBytes0)
    assert(iBad == 0)
    Back0:CsnArr = csnfrombuffer(Buf0, "|u1")
    iGotBack0[] = csntoarray(Back0)
    iBad = mismatches(iGotBack0, iSrc0)
    assert(iBad == 0)
    ; |i1
    iSrc1[] = fillarray(0, -1, 127, -128)
    iBytes1[] = fillarray(0, 255, 127, 128)
    Src1:CsnArr = csnfromarray(iSrc1)
    Buf1:CsnArr = csntobuffer(Src1, "|i1")
    iGotBytes1[] = csntoarray(Buf1)
    iBad = mismatches(iGotBytes1, iBytes1)
    assert(iBad == 0)
    Back1:CsnArr = csnfrombuffer(Buf1, "|i1")
    iGotBack1[] = csntoarray(Back1)
    iBad = mismatches(iGotBack1, iSrc1)
    assert(iBad == 0)
    ; <u2
    iSrc2[] = fillarray(0, 1, 258, 65535)
    iBytes2[] = fillarray(0, 0, 1, 0, 2, 1, 255, 255)
    Src2:CsnArr = csnfromarray(iSrc2)
    Buf2:CsnArr = csntobuffer(Src2, "<u2")
    iGotBytes2[] = csntoarray(Buf2)
    iBad = mismatches(iGotBytes2, iBytes2)
    assert(iBad == 0)
    Back2:CsnArr = csnfrombuffer(Buf2, "<u2")
    iGotBack2[] = csntoarray(Back2)
    iBad = mismatches(iGotBack2, iSrc2)
    assert(iBad == 0)
    ; >u2
    iSrc3[] = fillarray(0, 1, 258, 65535)
    iBytes3[] = fillarray(0, 0, 0, 1, 1, 2, 255, 255)
    Src3:CsnArr = csnfromarray(iSrc3)
    Buf3:CsnArr = csntobuffer(Src3, ">u2")
    iGotBytes3[] = csntoarray(Buf3)
    iBad = mismatches(iGotBytes3, iBytes3)
    assert(iBad == 0)
    Back3:CsnArr = csnfrombuffer(Buf3, ">u2")
    iGotBack3[] = csntoarray(Back3)
    iBad = mismatches(iGotBack3, iSrc3)
    assert(iBad == 0)
    ; <i2
    iSrc4[] = fillarray(0, -1, 32767, -32768)
    iBytes4[] = fillarray(0, 0, 255, 255, 255, 127, 0, 128)
    Src4:CsnArr = csnfromarray(iSrc4)
    Buf4:CsnArr = csntobuffer(Src4, "<i2")
    iGotBytes4[] = csntoarray(Buf4)
    iBad = mismatches(iGotBytes4, iBytes4)
    assert(iBad == 0)
    Back4:CsnArr = csnfrombuffer(Buf4, "<i2")
    iGotBack4[] = csntoarray(Back4)
    iBad = mismatches(iGotBack4, iSrc4)
    assert(iBad == 0)
    ; >i2
    iSrc5[] = fillarray(0, -2, 258, -32768)
    iBytes5[] = fillarray(0, 0, 255, 254, 1, 2, 128, 0)
    Src5:CsnArr = csnfromarray(iSrc5)
    Buf5:CsnArr = csntobuffer(Src5, ">i2")
    iGotBytes5[] = csntoarray(Buf5)
    iBad = mismatches(iGotBytes5, iBytes5)
    assert(iBad == 0)
    Back5:CsnArr = csnfrombuffer(Buf5, ">i2")
    iGotBack5[] = csntoarray(Back5)
    iBad = mismatches(iGotBack5, iSrc5)
    assert(iBad == 0)
    ; <u4
    iSrc6[] = fillarray(0, 1, 16909060, 4294967295)
    iBytes6[] = fillarray(0, 0, 0, 0, 1, 0, 0, 0, 4, 3, 2, 1, 255, 255, 255, 255)
    Src6:CsnArr = csnfromarray(iSrc6)
    Buf6:CsnArr = csntobuffer(Src6, "<u4")
    iGotBytes6[] = csntoarray(Buf6)
    iBad = mismatches(iGotBytes6, iBytes6)
    assert(iBad == 0)
    Back6:CsnArr = csnfrombuffer(Buf6, "<u4")
    iGotBack6[] = csntoarray(Back6)
    iBad = mismatches(iGotBack6, iSrc6)
    assert(iBad == 0)
    ; >u4
    iSrc7[] = fillarray(0, 1, 16909060, 4294967295)
    iBytes7[] = fillarray(0, 0, 0, 0, 0, 0, 0, 1, 1, 2, 3, 4, 255, 255, 255, 255)
    Src7:CsnArr = csnfromarray(iSrc7)
    Buf7:CsnArr = csntobuffer(Src7, ">u4")
    iGotBytes7[] = csntoarray(Buf7)
    iBad = mismatches(iGotBytes7, iBytes7)
    assert(iBad == 0)
    Back7:CsnArr = csnfrombuffer(Buf7, ">u4")
    iGotBack7[] = csntoarray(Back7)
    iBad = mismatches(iGotBack7, iSrc7)
    assert(iBad == 0)
    ; <i4
    iSrc8[] = fillarray(0, -1, 2147483647, -2147483648)
    iBytes8[] = fillarray(0, 0, 0, 0, 255, 255, 255, 255, 255, 255, 255, 127, 0, 0, 0, 128)
    Src8:CsnArr = csnfromarray(iSrc8)
    Buf8:CsnArr = csntobuffer(Src8, "<i4")
    iGotBytes8[] = csntoarray(Buf8)
    iBad = mismatches(iGotBytes8, iBytes8)
    assert(iBad == 0)
    Back8:CsnArr = csnfrombuffer(Buf8, "<i4")
    iGotBack8[] = csntoarray(Back8)
    iBad = mismatches(iGotBack8, iSrc8)
    assert(iBad == 0)
    ; >i4
    iSrc9[] = fillarray(0, -16909060, 2147483647, -2147483648)
    iBytes9[] = fillarray(0, 0, 0, 0, 254, 253, 252, 252, 127, 255, 255, 255, 128, 0, 0, 0)
    Src9:CsnArr = csnfromarray(iSrc9)
    Buf9:CsnArr = csntobuffer(Src9, ">i4")
    iGotBytes9[] = csntoarray(Buf9)
    iBad = mismatches(iGotBytes9, iBytes9)
    assert(iBad == 0)
    Back9:CsnArr = csnfrombuffer(Buf9, ">i4")
    iGotBack9[] = csntoarray(Back9)
    iBad = mismatches(iGotBack9, iSrc9)
    assert(iBad == 0)
    ; <f4
    iSrc10[] = fillarray(0.0, -1.5, 0.25, -3.0)
    iBytes10[] = fillarray(0, 0, 0, 0, 0, 0, 192, 191, 0, 0, 128, 62, 0, 0, 64, 192)
    Src10:CsnArr = csnfromarray(iSrc10)
    Buf10:CsnArr = csntobuffer(Src10, "<f4")
    iGotBytes10[] = csntoarray(Buf10)
    iBad = mismatches(iGotBytes10, iBytes10)
    assert(iBad == 0)
    Back10:CsnArr = csnfrombuffer(Buf10, "<f4")
    iGotBack10[] = csntoarray(Back10)
    iBad = mismatches(iGotBack10, iSrc10)
    assert(iBad == 0)
    ; >f4
    iSrc11[] = fillarray(0.0, -1.5, 0.25, -3.0)
    iBytes11[] = fillarray(0, 0, 0, 0, 191, 192, 0, 0, 62, 128, 0, 0, 192, 64, 0, 0)
    Src11:CsnArr = csnfromarray(iSrc11)
    Buf11:CsnArr = csntobuffer(Src11, ">f4")
    iGotBytes11[] = csntoarray(Buf11)
    iBad = mismatches(iGotBytes11, iBytes11)
    assert(iBad == 0)
    Back11:CsnArr = csnfrombuffer(Buf11, ">f4")
    iGotBack11[] = csntoarray(Back11)
    iBad = mismatches(iGotBack11, iSrc11)
    assert(iBad == 0)
    ; <f8
    iSrc12[] = fillarray(0.0, -1.5, 0.1, 1e+300)
    iBytes12[] = fillarray(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 248, 191, 154, 153, 153, 153, 153, 153, 185, 63, 156, 117, 0, 136, 60, 228, 55, 126)
    Src12:CsnArr = csnfromarray(iSrc12)
    Buf12:CsnArr = csntobuffer(Src12, "<f8")
    iGotBytes12[] = csntoarray(Buf12)
    iBad = mismatches(iGotBytes12, iBytes12)
    assert(iBad == 0)
    Back12:CsnArr = csnfrombuffer(Buf12, "<f8")
    iGotBack12[] = csntoarray(Back12)
    iBad = mismatches(iGotBack12, iSrc12)
    assert(iBad == 0)
    ; >f8
    iSrc13[] = fillarray(0.0, -1.5, 0.1, -1e-300)
    iBytes13[] = fillarray(0, 0, 0, 0, 0, 0, 0, 0, 191, 248, 0, 0, 0, 0, 0, 0, 63, 185, 153, 153, 153, 153, 153, 154, 129, 165, 110, 31, 194, 248, 243, 89)
    Src13:CsnArr = csnfromarray(iSrc13)
    Buf13:CsnArr = csntobuffer(Src13, ">f8")
    iGotBytes13[] = csntoarray(Buf13)
    iBad = mismatches(iGotBytes13, iBytes13)
    assert(iBad == 0)
    Back13:CsnArr = csnfrombuffer(Buf13, ">f8")
    iGotBack13[] = csntoarray(Back13)
    iBad = mismatches(iGotBack13, iSrc13)
    assert(iBad == 0)

    ; a 2-D source is flattened in row-major order
    iShape[] = fillarray(2, 2)
    Grid:CsnArr = csnarange(1, 5, 1)
    Grid2:CsnArr = csnreshape(Grid, iShape)
    GridBuf:CsnArr = csntobuffer(Grid2, ">u2")
    iGridBytes[] = fillarray(0, 1, 0, 2, 0, 3, 0, 4)
    iGotGrid[] = csntoarray(GridBuf)
    iBad = mismatches(iGotGrid, iGridBytes)
    assert(iBad == 0)

    ; f4 rounds to the nearest float
    Tenth:CsnArr = csnfromarray(fillarray(0.1))
    TenthBack:CsnArr = csnfrombuffer(csntobuffer(Tenth, "<f4"), "<f4")
    iTenth[] = csntoarray(TenthBack)
    assert(iTenth[0] == 0.10000000149011612)

    ; an empty source gives an empty buffer, and back
    iNone[] = fillarray(0)
    Empty:CsnArr = csnempty(iNone)
    EmptyBuf:CsnArr = csntobuffer(Empty, "<f8")
    EmptyBack:CsnArr = csnfrombuffer(EmptyBuf, "<f8")
    iEmpty = csnisempty(EmptyBack)
    assert(iEmpty == 1)

    prints("buffer i-time ok\n")
endin

instr 2
    ; k forms: Acc grows by one on every pass, the buffer and its decoding follow
    iShape[] = fillarray(2)
    One:CsnArr = csnfull(iShape, 1)
    Acc:CsnArr = csnfull(iShape, 255)
    csnk Acc
    Acc += One
    Buf:CsnArr = csntobuffer(Acc, "<u2", 1)
    Back:CsnArr = csnfrombuffer(Buf, "<u2", 1)
    kBuf[] = csntoarray(Buf)
    kBack[] = csntoarray(Back)
    gkBuf0 = kBuf[0]
    gkBuf1 = kBuf[1]
    gkBack = kBack[0]
    gkLen = lenarray(kBuf)
endin

instr 3
    ; four passes: 255 + 4 = 259 = 0x0103
    assert(i(gkLen) == 4)
    assert(i(gkBuf0) == 3)
    assert(i(gkBuf1) == 1)
    assert(i(gkBack) == 259)
    prints("buffer ok\n")
endin
</CsInstruments>
<CsScore>
i 1 0 0.001
i 2 0 [4 * 32 / 48000]
i 3 0.01 0.001
e
</CsScore>
</CsoundSynthesizer>
