<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>

<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

; Drives every creation form through allocation, reuse, aliasing, copy and
; release, and the item accessors through csnget/csnset and []. Run it under
; build-asan too: an overrun or a bad free in the string slots shows up there.

giShape[] fillarray 3
Global@global:CsnStr = csnfull(giShape, "global")
Indexed@global:CsnStr = csnfull(giShape, "a")

instr 1
    iShape[] fillarray 2, 3
    Empty:CsnStr = csnempty(iShape)
    Full:CsnStr = csnfull(iShape, "ab")
    Like:CsnStr = csnlike(Full, "cd")
    ; an output that is its own source keeps its layout and replaces its strings
    Full = csnlike(Full, "ef")
    ; assigning copies every string, and the copy is released on its own
    Copy:CsnStr = Full
    Copy = Like
    Global = csnlike(Global, "rewritten")
endin

instr 2
    kShape[] = init(1)
    kShape[0] = (timeinsts() < 0.02 ? 4 : 8)
    ; the k forms rebuild every pass: the slots of the previous pass are released
    EmptyK:CsnStr = csnempty(kShape)
    FullK:CsnStr = csnfull(kShape, "k")
    ; with the trigger the k form of csnlike is chosen and refills on each pass
    LikeK:CsnStr = csnlike(FullK, "like", 1)
    CopyK:CsnStr = LikeK
    ; k bounds pick the k form: the slice is rebuilt as FullK changes size
    kAxis init 0
    kStart init 1
    kStop init 3
    kStep init 1
    SliceK:CsnStr = csngetslice(FullK, kAxis, kStart, kStop, kStep)
endin

instr 3
    ; the global outlives the note that rewrote it
    Local:CsnStr = csnlike(Global, "local")
    SLocal = Local[0]
    iLocalCmp = strcmp(SLocal, "local")
    SGlobal = Global[2]
    iGlobalCmp = strcmp(SGlobal, "rewritten")
    assert(iLocalCmp == 0 && iGlobalCmp == 0)
endin

instr 4
    ; [] at i-rate: set, read back, and init
    Indexed[1] = "b"
    Indexed[2] init "c"
    SB = Indexed[1]
    SC = Indexed[2]
    iBCmp = strcmp(SB, "b")
    iCCmp = strcmp(SC, "c")
    assert(iBCmp == 0 && iCCmp == 0)
    ; [] at k-rate: an index that is a k variable picks the k forms
    kIdx init 0
    Indexed[kIdx] = "k"
    SK = Indexed[kIdx]
endin

instr 6
    ; S[] -> CsnStr -> S[], printing and saving (file contents checked by hand
    ; against numpy: <U5, shape (3,), UTF-8 decoded to code points)
    SItems[] fillarray "x", "yz", "città"
    FromArr:CsnStr = csnfromarray(SItems)
    SBack[] = csntoarray(FromArr)
    SLast = SBack[2]
    iLastCmp = strcmp(SLast, "città")
    iBackSize = lenarray(SBack)
    assert(iLastCmp == 0 && iBackSize == 3)
    csnprint(FromArr)
    csnsave(FromArr, "csnum_strings.npy")
    csnsave(FromArr, "csnum_strings.csn")
    ; the k forms take an explicit trigger, as S[] has no k counterpart
    kTrig init 1
    FromK:CsnStr = csnfromarray(SItems, kTrig)
    SBackK[] = csntoarray(FromK, kTrig)
    csnsave(FromK, "csnum_strings_k.npy", kTrig)
    ; and back from both formats
    LoadedNpy:CsnStr = csnload("csnum_strings.npy")
    LoadedCsn:CsnStr = csnload("csnum_strings.csn")
    SNpy = LoadedNpy[2]
    SCsn = LoadedCsn[1]
    iNpyCmp = strcmp(SNpy, "città")
    iCsnCmp = strcmp(SCsn, "yz")
    iLoadedSize = csnsize(LoadedNpy)
    assert(iNpyCmp == 0 && iCsnCmp == 0 && iLoadedSize == 3)
    LoadedK:CsnStr = csnload("csnum_strings.npy", kTrig)
endin

instr 5
    ; the k-rate write of instr 4 landed and left the other items alone
    SA = Indexed[0]
    SB = Indexed[1]
    iACmp = strcmp(SA, "k")
    iBCmp = strcmp(SB, "b")
    assert(iACmp == 0 && iBCmp == 0)
    prints "strings creation ok\n"
endin
</CsInstruments>

<CsScore>
i1 0 0.01
i2 0 0.05
i3 0.06 0.01
i4 0.08 0.01
i5 0.10 0.01
i6 0.12 0.01
</CsScore>
</CsoundSynthesizer>
