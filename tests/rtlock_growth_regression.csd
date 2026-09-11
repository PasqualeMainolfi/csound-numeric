<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>

<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

/* Every buffer that can grow during performance: arrays rewritten in place,
   and the working buffers an opcode keeps for itself. On a marked path none
   of them may reach the allocator at perf time.

   Each note either has to be stopped by the guard (its marker stays 0) or has
   to run to the end (marker 1). The stopped notes cover the paths that used to
   allocate silently: in-place growth checked the wrong flag, and an output
   marked by csnrtlockstart did not reach the convolution's transform buffers. The
   notes that must run cover the other half of the design: buffers reserved at
   init for the worst case, so a window that grows mid-note, a lock applied
   after the opcode, or a source moving within its capacity never trips a
   refusal it should not.

   Refusals are performance errors, which the unit-test runner counts as failed
   assertions, so the file is scored on its final marker. */

giFour[]   = fillarray(1, 2, 3, 4)
giEight[]  = fillarray(1, 5, 2, 4, 3, 9, 7, 8)
giSixteen[] = fillarray(1, 5, 2, 4, 3, 9, 7, 8, 6, 0, 11, 13, 12, 10, 15, 14)
giBlock[]  = fillarray(9, 9)
giFourByTwo[] = fillarray(4, 2)
giKernel[] = fillarray(1, 1)

gkPushLocked   init 0
gkPushFree     init 0
gkInsertLocked init 0
gkSetLocked    init 0
gkPadLocked    init 0
gkBlockLocked  init 0
gkResizeLocked init 0
gkResizeFits   init 0
gkMedLateLock  init 0
gkMedBlock     init 0
gkMedInPlace   init 0
gkLateSource   init 0
gkUnlockedOut  init 0
gkInheritedOut init 0
gkConvBlock    init 0
gkConvFree     init 0
gkSortInPlace  init 0
gkToArray      init 0
gkPadKnown     init 0
gkPadKnownC    init 0
gkPadResize    init 0
gkCompressFits init 0

/* A four-element array holds eight before it has to reallocate, so one push
   per pass reaches the limit on the fifth. */
RtPush@global:CsnArr     = csnfromarray(giFour)
RtPushFree@global:CsnArr = csnfromarray(giFour)
RtInsert@global:CsnArr   = csnfromarray(giFour)
RtSetSrc@global:CsnArr   = csnfromarray(giFour)
RtSet@global:CsnArr      = csnlikeset(RtSetSrc)
RtPad@global:CsnArr      = csnfromarray(giFour)
/* A block drops one axis of the array it goes into, so the block test inserts
   a row into a 4x2 matrix, which holds 16 elements before reallocating. */
RtBlockFlat@global:CsnArr = csnfromarray(giEight)
RtBlock@global:CsnArr     = csnreshape(RtBlockFlat, giFourByTwo)
RtBlockData@global:CsnArr = csnfromarray(giBlock)
RtResize@global:CsnArr   = csnfromarray(giFour)
RtResizeFits@global:CsnArr = csnfromarray(giFour)

instr 1
    csnrtlock RtPush
    kOne init 1
    kValue = timeinstk()
    csnpush RtPush, kValue, kOne
    if timeinstk() == 12 then
        gkPushLocked = 1
    endif
endin

instr 2
    kOne init 1
    kValue = timeinstk()
    csnpush RtPushFree, kValue, kOne
    if timeinstk() == 12 then
        gkPushFree = 1
    endif
endin

instr 3
    csnrtlock RtInsert
    kOne init 1
    kZero init 0
    kValue = timeinstk()
    csninsert RtInsert, kValue, kZero, kOne
    if timeinstk() == 12 then
        gkInsertLocked = 1
    endif
endin

instr 4
    csnrtlock RtSet
    kOne init 1
    kValue = timeinstk() + 100
    csnsetinsert RtSet, kValue, kOne
    if timeinstk() == 12 then
        gkSetLocked = 1
    endif
endin

/* A width that changes every pass, so each pass pads the already padded
   array again. */
instr 5
    csnrtlock RtPad
    kOne init 1
    kZero init 0
    kBefore = timeinstk()
    csnpad RtPad, kBefore, kZero, kZero, kOne
    if timeinstk() == 12 then
        gkPadLocked = 1
    endif
endin

instr 6
    csnrtlock RtBlock
    kOne init 1
    kZero init 0
    csninsert RtBlock, RtBlockData, kZero, kZero, kOne
    if timeinstk() == 12 then
        gkBlockLocked = 1
    endif
endin

instr 7
    csnrtlock RtResize
    kOne init 1
    kShape[] init 1
    kShape[0] = timeinstk() < 2 ? 4 : 20
    csnresize RtResize, kShape, kOne
    if timeinstk() == 12 then
        gkResizeLocked = 1
    endif
endin

/* Within the capacity nothing has to move, so the same resize must pass. */
instr 8
    csnrtlock RtResizeFits
    kOne init 1
    kShape[] init 1
    kShape[0] = timeinstk() < 2 ? 4 : 7
    csnresize RtResizeFits, kShape, kOne
    if timeinstk() == 12 then
        gkResizeFits = 1
    endif
endin

/* Moving median: the lock arrives after the opcode's init, from the output,
   from a csnrtlockstart section and from the source, while the window is
   assigned with `=` (so
   it reads 0 at init) and then grows. Init reserved the widest window the
   source allows, so none of them may be refused. */
RtMedSrc@global:CsnArr = csnfromarray(giSixteen)
RtMedIn@global:CsnArr  = csnfromarray(giSixteen)

instr 9
    kOne init 1
    kAll init -1
    kWin = timeinstk() < 3 ? 2 : 9
    MedOut:CsnArr = csnmovmedian(RtMedSrc, kWin, kAll, kOne)
    csnrtlock MedOut
    if timeinstk() == 12 then
        gkMedLateLock = 1
    endif
endin

instr 10
    kOne init 1
    kAll init -1
    kWin = timeinstk() < 3 ? 2 : 9
    csnrtlockstart
    MedBlock:CsnArr = csnmovmedian(RtMedSrc, kWin, kAll, kOne)
    csnrtlockend
    if timeinstk() == 12 then
        gkMedBlock = 1
    endif
endin

instr 11
    csnrtlock RtMedIn
    kOne init 1
    kAll init -1
    kWin = timeinstk() < 3 ? 2 : 9
    csnmovmedian RtMedIn, kWin, kAll, kOne
    if timeinstk() == 12 then
        gkMedInPlace = 1
    endif
endin

/* The mark belongs to the slot it is set on. Locking a source mid-note does
   not reach an output already derived from it; a lock inherited at init does,
   and stays on the output after the source is released; csnrtunlock on the
   output releases the output. Each output here is made to need more storage
   than it was created with, the only change a mark refuses.

   The late-lock source is shrunk to 8 elements before the output exists, so
   it keeps room for 40 while the output is created with room for 16: once
   marked, the source can still grow to 30 without reallocating, and the
   output it feeds cannot hold that without new storage. */
giSixteenTwenty[] = fillarray(1, 5, 2, 4, 3, 9, 7, 8, 6, 0, 11, 13, 12, 10, 15, 14, 16, 17, 18, 19)
giShapeEight[] = fillarray(8)
RtLateSrc@global:CsnArr    = csnfromarray(giSixteenTwenty)
RtUnlockSrc@global:CsnArr  = csnfromarray(giEight)
RtInheritSrc@global:CsnArr = csnfromarray(giEight)

instr 12
    kOne init 1
    kAll init -1
    kWin init 3
    csnresize RtLateSrc, giShapeEight
    LateOut:CsnArr = csnmovmedian(RtLateSrc, kWin, kAll, kOne)
    kLock = timeinstk() == 2 ? 1 : 0
    csnrtlock RtLateSrc, kLock
    kShape[] init 1
    kShape[0] = timeinstk() < 4 ? 8 : 30
    csnresize RtLateSrc, kShape, kOne
    if timeinstk() == 12 then
        gkLateSource = 1
    endif
endin

instr 13
    csnrtlock RtUnlockSrc
    kOne init 1
    kAll init -1
    kWin init 3
    UnlockedOut:CsnArr = csnmovmedian(RtUnlockSrc, kWin, kAll, kOne)
    csnrtunlock UnlockedOut
    csnrtunlock RtUnlockSrc
    kShape[] init 1
    kShape[0] = timeinstk() < 4 ? 8 : 40
    csnresize RtUnlockSrc, kShape, kOne
    if timeinstk() == 12 then
        gkUnlockedOut = 1
    endif
endin

instr 14
    csnrtlock RtInheritSrc
    kOne init 1
    kAll init -1
    kWin init 3
    InheritedOut:CsnArr = csnmovmedian(RtInheritSrc, kWin, kAll, kOne)
    csnrtunlock RtInheritSrc
    kShape[] init 1
    kShape[0] = timeinstk() < 4 ? 8 : 40
    csnresize RtInheritSrc, kShape, kOne
    if timeinstk() == 12 then
        gkInheritedOut = 1
    endif
endin

/* A convolution marked only through its output: the operands are globals
   created outside the csnrtlockstart section. In SAME mode the output keeps the signal's
   length while the kernel grows, so the slot guard alone never fires; the
   transform size is what changes, and that has to be refused too. The same
   note without the section must run. */
RtConvX@global:CsnArr     = csnfromarray(giEight)
RtConvH@global:CsnArr     = csnfromarray(giKernel)
RtConvFreeX@global:CsnArr = csnfromarray(giEight)
RtConvFreeH@global:CsnArr = csnfromarray(giKernel)

instr 15
    kOne init 1
    csnrtlockstart
    ConvOut:CsnArr = csnfftconvolve1d(RtConvX, RtConvH, 1, -1, kOne)
    csnrtlockend
    kTap init 1
    csnpush RtConvH, kTap, kOne
    if timeinstk() == 12 then
        gkConvBlock = 1
    endif
endin

instr 16
    kOne init 1
    ConvFree:CsnArr = csnfftconvolve1d(RtConvFreeX, RtConvFreeH, 1, -1, kOne)
    kTap init 1
    csnpush RtConvFreeH, kTap, kOne
    if timeinstk() == 12 then
        gkConvFree = 1
    endif
endin

/* Working buffers reserved for the source's capacity: an in-place sort and a
   k-rate conversion to a Csound array follow a marked source that moves within
   its capacity without being refused. */
RtSortSrc@global:CsnArr = csnfromarray(giEight)
RtToArraySrc@global:CsnArr = csnfromarray(giEight)

instr 17
    csnrtlock RtSortSrc
    kOne init 1
    kAll init -1
    csnsort RtSortSrc, kAll, kOne
    kShape[] init 1
    kShape[0] = timeinstk() < 3 ? 8 : 15
    csnresize RtSortSrc, kShape, kOne
    if timeinstk() == 12 then
        gkSortInPlace = 1
    endif
endin

instr 18
    csnrtlock RtToArraySrc
    kOne init 1
    kValues[] = csntoarray(RtToArraySrc)
    kShape[] init 1
    kShape[0] = timeinstk() < 3 ? 8 : 15
    csnresize RtToArraySrc, kShape, kOne
    if timeinstk() == 12 then
        gkToArray = 1
    endif
endin

/* A k-rate pad whose widths are already known at init creates its output at
   the padded shape, so on a marked source the passes that keep those widths
   need no new storage. It used to start from the source's shape and was
   refused on the first pass. */
RtPadKnownSrc@global:CsnArr  = csnfromarray(giEight)
RtPadKnownSrcC@global:CsnArr = csntocomplex(RtPadKnownSrc)

instr 19
    csnrtlock RtPadKnownSrc
    kOne init 1
    kGrow init 4
    kFill init 0
    PadKnown:CsnArr = csnpad(RtPadKnownSrc, kGrow, kGrow, kFill, kOne)
    if timeinstk() == 12 then
        gkPadKnown = 1
    endif
endin

instr 20
    csnrtlock RtPadKnownSrcC
    kOne init 1
    kGrow init 4
    Fill:Complex init 0, 0
    PadKnownC:CsnArr = csnpad(RtPadKnownSrcC, kGrow, kGrow, Fill, kOne)
    if timeinstk() == 12 then
        gkPadKnownC = 1
    endif
endin

/* A marked output keeps the storage it was created with, twice its initial
   element count, and reuses it for any later shape that fits: the pad shrinks
   and then grows again within that room, and a compress whose count follows a
   moving threshold stays within it too. */
RtPadResizeSrc@global:CsnArr = csnfromarray(giEight)
RtCompressFitsSrc@global:CsnArr = csnfromarray(giEight)

instr 21
    csnrtlock RtPadResizeSrc
    kOne init 1
    kFill init 0
    kGrow init 4
    if timeinstk() >= 3 then
        kGrow = 2
    endif
    if timeinstk() >= 6 then
        kGrow = 6
    endif
    PadResize:CsnArr = csnpad(RtPadResizeSrc, kGrow, kGrow, kFill, kOne)
    if timeinstk() == 12 then
        gkPadResize = 1
    endif
endin

instr 22
    csnrtlock RtCompressFitsSrc
    kOne init 1
    kThresh init 4.5
    if timeinstk() >= 3 then
        kThresh = 6.5
    endif
    if timeinstk() >= 6 then
        kThresh = 1.5
    endif
    Mask:CsnArr = csngt(RtCompressFitsSrc, kThresh, kOne)
    Kept:CsnArr = csncompress(RtCompressFitsSrc, Mask, -1, kOne)
    if timeinstk() == 12 then
        gkCompressFits = 1
    endif
endin

instr 100
    iPushLocked   = i(gkPushLocked)
    iPushFree     = i(gkPushFree)
    iInsertLocked = i(gkInsertLocked)
    iSetLocked    = i(gkSetLocked)
    iPadLocked    = i(gkPadLocked)
    iBlockLocked  = i(gkBlockLocked)
    iResizeLocked = i(gkResizeLocked)
    iResizeFits   = i(gkResizeFits)
    iMedLateLock  = i(gkMedLateLock)
    iMedBlock     = i(gkMedBlock)
    iMedInPlace   = i(gkMedInPlace)
    iLateSource   = i(gkLateSource)
    iUnlockedOut  = i(gkUnlockedOut)
    iInheritedOut = i(gkInheritedOut)
    iConvBlock    = i(gkConvBlock)
    iConvFree     = i(gkConvFree)
    iSortInPlace  = i(gkSortInPlace)
    iToArray      = i(gkToArray)
    iPadKnown     = i(gkPadKnown)
    iPadKnownC    = i(gkPadKnownC)
    iPadResize    = i(gkPadResize)
    iCompressFits = i(gkCompressFits)

    assert(iPushLocked == 0)
    assert(iPushFree == 1)
    assert(iInsertLocked == 0)
    assert(iSetLocked == 0)
    assert(iPadLocked == 0)
    assert(iBlockLocked == 0)
    assert(iResizeLocked == 0)
    assert(iResizeFits == 1)
    assert(iMedLateLock == 1)
    assert(iMedBlock == 1)
    assert(iMedInPlace == 1)
    assert(iLateSource == 1)
    assert(iUnlockedOut == 1)
    assert(iInheritedOut == 0)
    assert(iConvBlock == 0)
    assert(iConvFree == 1)
    assert(iSortInPlace == 1)
    assert(iToArray == 1)
    assert(iPadKnown == 1)
    assert(iPadKnownC == 1)
    assert(iPadResize == 1)
    assert(iCompressFits == 1)

    iRefused = iPushLocked + iInsertLocked + iSetLocked + iPadLocked + iBlockLocked \
             + iResizeLocked + iInheritedOut + iConvBlock
    iRan = iPushFree + iResizeFits + iMedLateLock + iMedBlock + iMedInPlace \
         + iLateSource + iUnlockedOut + iConvFree + iSortInPlace + iToArray \
         + iPadKnown + iPadKnownC + iPadResize + iCompressFits
    if iRefused == 0 && iRan == 14 then
        prints("csnum growth guards held on every real-time path\n")
    endif
endin
</CsInstruments>

<CsScore>
i 1  0.0 0.02
i 2  0.1 0.02
i 3  0.2 0.02
i 4  0.3 0.02
i 5  0.4 0.02
i 6  0.5 0.02
i 7  0.6 0.02
i 8  0.7 0.02
i 9  0.8 0.02
i 10 0.9 0.02
i 11 1.0 0.02
i 12 1.1 0.02
i 13 1.2 0.02
i 14 1.3 0.02
i 15 1.4 0.02
i 16 1.5 0.02
i 17 1.6 0.02
i 18 1.7 0.02
i 19 1.8 0.02
i 20 1.9 0.02
i 21 2.0 0.02
i 22 2.1 0.02
i 100 2.2 0.01
e
</CsScore>
</CsoundSynthesizer>
