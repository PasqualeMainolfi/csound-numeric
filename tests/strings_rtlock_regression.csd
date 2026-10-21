<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>

<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

/* On a real-time path a k form writes its strings into the buffers its slots
   already hold. Under csnrtlockall every array is marked from its first
   moment, so the init pass sizes the slots and every perf pass below has to
   run without an error while each string fits: the source changes on every
   pass, by one letter, so each result is made again. A string that would grow
   its slot is refused (instr 2), an in-place write that is refused leaves
   its array unchanged (instr 4), and the report (instr 3) checks the values
   from the last pass.

   Scored on the refusal followed by the final marker, as the refusal counts
   as a failed assertion. The init pass runs both branches of the letter's
   if, the "a" one last, so the slots are sized for one letter from the
   start (sprintfk would leave the string empty there). */

csnrtlockall

gkBad  init 0
gkDone init 0

; an in-place write that is refused leaves the array as it was (instr 4)
gSKeep[] fillarray "keep", "this"
GKeep@global:CsnStr = csnfromarray(gSKeep)

; a padding in place that one slot has no room for leaves every slot as it
; was, the one with room included (instr 5): "ab" sits in a 5-byte buffer
gSPad[] fillarray "abcd", "a"
GPad@global:CsnStr = csnfromarray(gSPad)

; the same for a replace in place (instr 7)
gSRepl[] fillarray "abcd", "a"
GRepl@global:CsnStr = csnfromarray(gSRepl)

instr 1
    kOne init 1
    kTwo init 2
    kZero init 0
    kc init 0

    ; the source alternates "a" and "z" in its first slot
    SSrc[] fillarray "a", "bbbb", "cc"
    Src:CsnStr = csnfromarray(SSrc)
    kIdx0[] fillarray 0
    if kc % 2 == 1 then
        Sx strcpyk "z"
    else
        Sx strcpyk "a"
    endif
    csnset(Src, kIdx0, Sx)

    iMaskVals[] fillarray 1, 0, 1
    Mask:CsnArr = csnfromarray(iMaskVals)

    Flipped:CsnStr = csnflip(Src, kOne)
    Rolled:CsnStr = csnroll(Src, kOne)
    Reversed:CsnStr = csnreverse(Src, kOne)
    Copied:CsnStr = csncopy(Src, kOne)
    Joined:CsnStr = csnconcat(Src, Src, kOne)
    Padded:CsnStr = csnpad(Src, kOne, kOne, "--", kOne)
    Chosen:CsnStr = csnwhere(Mask, Src, "w", kOne)
    Kept:CsnStr = csncompress(Src, Mask, kOne)
    Picked:CsnStr = csnselect(Src, Mask, kOne)
    Cut:CsnStr = csntruncate(Src, kTwo, kOne)
    Sliced:CsnStr = csngetslice(Src, kZero, kZero, kTwo, kOne)
    Joined2:CsnStr = csnstradd(Src, Sx, kOne)
    Doubled:CsnStr = csnstrmul(Src, kTwo)
    Same:CsnStr = csnlike(Src, Sx, kOne)
    SDest[] fillarray "ddd", "ddd", "ddd"
    Dest:CsnStr = csnfromarray(SDest)
    csnputmask(Dest, Mask, Src, kOne)

    Upper:CsnStr = csnstrupper(Src, kOne)
    Capital:CsnStr = csnstrcapitalize(Joined2, kOne)
    Swapped:CsnStr = csnstrswapcase(Joined2, kOne)
    Titled:CsnStr = csnstrtitle(Joined, kOne)
    Spaced:CsnStr = csnstradd("  ", Src, kOne)
    Stripped:CsnStr = csnstrlstrip(Spaced, kOne)
    Spaced = csnstrstrip(Spaced, kOne)
    ; Sx is the chars argument: a new one on a constant source makes the
    ; result again, out of place and in place ("zz" loses its "z" only once
    ; Sx turns "z")
    SFenced[] fillarray "zabaz"
    Fenced:CsnStr = csnfromarray(SFenced)
    Unfenced:CsnStr = csnstrstrip(Fenced, Sx, kOne)
    SEdge[] fillarray "azza"
    Edge:CsnStr = csnfromarray(SEdge)
    csnstrstrip(Edge, Sx, kOne)
    ; a k width lays the strings out at init with its init value, so the
    ; widest one reserves the room the passes fill; a constant width pads at
    ; init in place and onto its own source, and a rewritten slot is padded
    ; again in its buffer
    kWidth init 5
    kWidth = kc % 2 == 1 ? 3 : 5
    Centered:CsnStr = csnstrcenter(Src, kWidth, kOne)
    SOne[] fillarray "q"
    One:CsnStr = csnfromarray(SOne)
    OneRight:CsnStr = csnstrrjust(One, kWidth, kOne)
    SWide[] fillarray "ab"
    Wide:CsnStr = csnfromarray(SWide)
    Wide = csnstrljust(Wide, 4, kOne)
    SRight[] fillarray "b", "c"
    Right:CsnStr = csnfromarray(SRight)
    csnset(Right, kIdx0, Sx)
    csnstrrjust(Right, 4, kOne)
    ; a k width unknown at init pads in place on the first pass, inside the
    ; buffer "abcdefgh" left ("yyyy" gives the slice its room); the in-place
    ; k form marks its target k-rate, or the slice reading it keeps the value
    ; it had at init
    SLate[] fillarray "abcdefgh", "yyyy"
    Late:CsnStr = csnfromarray(SLate)
    iLateIdx[] fillarray 0
    csnset(Late, iLateIdx, "x")
    kLateWidth init 0
    kLateWidth = 4
    csnstrrjust(Late, kLateWidth, kOne)
    LateView:CsnStr = Late[0:1]
    ; a fill that changes on a source that does not makes the result again,
    ; as a copy and in place (the in-place one fills its first pass only:
    ; "9" is 4 bytes long by then)
    if kc % 2 == 1 then
        SFill strcpyk "-"
    else
        SFill strcpyk "."
    endif
    SNine[] fillarray "9"
    Nine:CsnStr = csnfromarray(SNine)
    NineFilled:CsnStr = csnstrrjust(Nine, 4, SFill, kOne)
    SNine2[] fillarray "9", "99999"
    Nine2:CsnStr = csnfromarray(SNine2)
    csnstrljust(Nine2, 4, SFill, kOne)
    ; old is Sx: a new one on a constant source makes the result again, as a
    ; copy and in place
    SRep[] fillarray "azaz"
    Rep:CsnStr = csnfromarray(SRep)
    RepOut:CsnStr = csnstrreplace(Rep, Sx, "-", -1, kOne)
    SRep2[] fillarray "azaz"
    Rep2:CsnStr = csnfromarray(SRep2)
    csnstrreplace(Rep2, Sx, "-", -1, kOne)
    ; replace is not idempotent: onto its own source it replaces again on
    ; every pass, and the k init leaves it to the first pass, so 8 passes
    ; move the "b" 8 places left
    SSlide[] fillarray "aaaaaaaaaab"
    Slide:CsnStr = csnfromarray(SSlide)
    Slide = csnstrreplace(Slide, "ab", "ba", -1, kOne)
    ; a k stop that changes on a constant source cuts again; onto its own
    ; source a cut is cut again on every pass, and the k init leaves it to the
    ; first pass, so 8 passes drop 8 bytes; in place a k range unknown at init
    ; cuts on the first pass, and marks its target k-rate for the slice
    kCutStop init 2
    kCutStop = kc % 2 == 1 ? 3 : 2
    SCut[] fillarray "abcdef"
    Cut0:CsnStr = csnfromarray(SCut)
    Cutted:CsnStr = csnstrsubstr(Cut0, 0, kCutStop, 1, kOne)
    SShrink[] fillarray "abcdefghijk"
    Shrink:CsnStr = csnfromarray(SShrink)
    Shrink = csnstrsubstr(Shrink, 1, 100, 1, kOne)
    SCutIn[] fillarray "abcdef", "uvwxyz"
    CutIn:CsnStr = csnfromarray(SCutIn)
    kCutInStart init 0
    kCutInStart = 2
    csnstrsubstr(CutIn, kCutInStart, 100, 1, kOne)
    CutView:CsnStr = CutIn[0:1]
    ; in place a new range cuts again what the last cut left, once per change:
    ; start 1 on even passes drops one byte each time, 0 on odd ones none, and
    ; the k init, which cannot know the start, cuts nothing
    kTrimStart = kc % 2 == 0 ? 1 : 0
    STrim[] fillarray "abcdefgh"
    Trim:CsnStr = csnfromarray(STrim)
    csnstrsubstr(Trim, kTrimStart, 100, 1, kOne)
    ; the operators: an S on the left and a count on the left run at perf on
    ; a k-rate operand, and an S that changes makes an i-rate operand's result
    ; again, each inside the room the init pass gave it
    OpPre:CsnStr = "<" + Src
    OpTwice:CsnStr = 2 * Src
    SOpFix[] fillarray "ab"
    OpFix:CsnStr = csnfromarray(SOpFix)
    OpFixPlus:CsnStr = OpFix + Sx
    ; the queries: an S or a range that changes on a constant source finds
    ; again, and the numbers land in the result the init pass laid out; on a
    ; constant source with no S nothing moves after init, so the k init
    ; computes. A k form names its start and end before the trigger
    QFind:CsnArr = csnstrfind(Rep, Sx, 0, -1, kOne)
    QRfind:CsnArr = csnstrrfind(Rep, Sx, 0, -1, kOne)
    QStarts:CsnArr = csnstrstartswith(Rep, Sx, 0, -1, kOne)
    QEnds:CsnArr = csnstrendswith(Rep, Sx, 0, -1, kOne)
    kQStart = kc % 2 == 1 ? 2 : 0
    kQEnd = kc % 2 == 1 ? 2 : -1
    QFrom:CsnArr = csnstrfind(Rep, Sx, kQStart, -1, kOne)
    QUpTo:CsnArr = csnstrrfind(Rep, "a", 0, kQEnd, kOne)
    QLen:CsnArr = csnstrlen(Joined2, kOne)
    QAlpha:CsnArr = csnstrisalpha(Src, kOne)
    QConst:CsnArr = csnstrlen(Rep, kOne)
    ; the conversions: the k init sizes the slots of a number-to-string
    ; result for the longest number, so a value that grows from "0.0" at init
    ; to 17 bytes still fits; on a constant source the k init converts
    kNumV[] init 2
    kNumV[0] = kc + 1/3
    kNumV[1] = 0.5
    NumSrc:CsnArr = csnfromarray(kNumV)
    NumStr:CsnStr = csnnumtostr(NumSrc, kOne)
    NumBack:CsnArr = csnstrtonum(NumStr, kOne)
    iNumConst[] fillarray 0.25, 2
    NumConst:CsnArr = csnfromarray(iNumConst)
    NumConstStr:CsnStr = csnnumtostr(NumConst, kOne)
    SNumConst[] fillarray "1.5", "x"
    StrConstSrc:CsnStr = csnfromarray(SNumConst)
    StrConst:CsnArr = csnstrtonum(StrConstSrc, kOne)
    ; the join: a source or a separator that changes joins again into the
    ; buffer the k init sized, and a constant one is joined by the k init
    SJoinSrc = csnstrjoin(Src, "+", kOne)
    SJoinTwo[] fillarray "p", "q"
    JoinTwo:CsnStr = csnfromarray(SJoinTwo)
    SJoinSep = csnstrjoin(JoinTwo, Sx, kOne)
    SJoinConst = csnstrjoin(JoinTwo, "-", kOne)
    ; split and partition: a source that changes splits again into the slots
    ; the k init sized, and a constant one is split by the k init
    SSplitSrc strcatk Sx, ",bb=c"
    SplitSrc:CsnStr = csnstrsplit(SSplitSrc, ",", -1, kOne)
    RPartSrc:CsnStr = csnstrrpartition(SSplitSrc, "=", kOne)
    SplitConst:CsnStr = csnstrrsplit("q,r,s", ",", 1, kOne)
    SplitWhole:CsnStr = csnstrsplit(SSplitSrc, ",", 0, kOne)
    SSplitJoined = csnstrjoin(SplitSrc, "|", kOne)
    SRPartSrc = csnstrjoin(RPartSrc, "|", kOne)
    SSplitConst = csnstrjoin(SplitConst, "|", kOne)
    SSplitWhole = csnstrjoin(SplitWhole, "|", kOne)
    ; a replace on one S: its output grows at perf as strcpyk's does, with
    ; no slot to reserve, so a longer result runs on a real-time path too
    SRepGrow = csnstrreplace(Sx, "z", "zzzz", -1, kOne)
    ; a zero trigger keeps the string the init pass made from "a"
    SRepHeld = csnstrreplace(Sx, "z", "y", -1, kZero)
    ; the strips on one S, whitespace and chars, as k forms: the source pads
    ; the letter, a zero trigger keeps the init result
    SStripPad strcatk Sx, "-- "
    SStripWs = csnstrrstrip(SStripPad, kOne)
    SStripCh = csnstrrstrip(SStripPad, " -", kOne)
    SStripHeld = csnstrstrip(SStripPad, " -", kZero)
    SStripWsHeld = csnstrstrip(SStripPad, kZero)
    ; add and mul on one S, named and as operators, which run on every pass;
    ; a zero trigger keeps the init result, made from "a"
    SAddK = csnstradd(Sx, "+", kOne)
    SAddHeld = csnstradd(Sx, "+", kZero)
    SMulK = csnstrmul(Sx, kTwo)
    SMulHeld = csnstrmul(Sx, kTwo, kZero)
    SOpAdd = Sx + "!"
    SOpMul = Sx * kTwo
    ; queries, substr, case maps, chr, ord and the byte arrays on one S, as k
    ; forms: they follow the letter; a zero trigger keeps the init result
    SUpK = csnstrupper(Sx, kOne)
    kFindK = csnstrfind(SSplitSrc, "b", 0, -1, kOne)
    kOrdK = csnstrord(Sx, kOne)
    kOrdHeld = csnstrord(Sx, kZero)
    SSubK = csnstrsubstr(SSplitSrc, 0, 1, 1, kOne)
    kCode init 0
    kCode = 90
    SChrK = csnstrchr(kCode)
    ; ord of an S a k opcode writes, still empty at init
    kOrdChr = csnstrord(SChrK, kOne)
    BytesK:CsnArr = csnstrtoascii(Sx, kOne)
    SBackK = csnstrfromascii(BytesK, kOne)
    ; a shorter piece ends inside the buffer a longer one sized
    if kc % 2 == 1 then
        SPieceSrc strcpyk "a,b"
    else
        SPieceSrc strcpyk "abc,b"
    endif
    Pieces:CsnStr = csnstrsplit(SPieceSrc, ",", -1, kOne)
    SPiecesJoined = csnstrjoin(Pieces, "|", kOne)
    ; swap undoes itself: onto its own source it swaps again on every pass,
    ; and the k init leaves it to the first pass, so 8 passes give it back
    SToggle[] fillarray "ab"
    Toggle:CsnStr = csnfromarray(SToggle)
    Toggle = csnstrswapcase(Toggle, kOne)
    SCase[] fillarray "Ab", "cD"
    Case:CsnStr = csnfromarray(SCase)
    csnset(Case, kIdx0, Sx)
    csnstrupper(Case, kOne)
    ; nothing else writes Case2 at perf: the in-place k form has to mark it
    ; k-rate, or the slice reading it keeps the value it had at init
    SCase2[] fillarray "ab", "cd"
    Case2:CsnStr = csnfromarray(SCase2)
    csnstrlower(Case2, kOne)
    iIdx0[] fillarray 0
    csnset(Case2, iIdx0, "Q")
    CaseView:CsnStr = Case2[0:1]

    ; pop then push: the string pop leaves past the end comes back in place
    SQueue[] fillarray "p", "q"
    Queue:CsnStr = csnfromarray(SQueue)
    SPopped = csnpop(Queue, kOne)
    csnpush(Queue, SPopped, kOne)

    if kc == 7 then
        kI0 init 0
        kI1 init 1
        kI2 init 2
        kI3 init 3
        kI4 init 4
        S1 = Flipped[kI2]
        kBad = strcmpk(S1, "z") != 0 ? 1 : 0
        S2 = Rolled[kI1]
        kBad += strcmpk(S2, "z") != 0 ? 1 : 0
        S3 = Reversed[kI2]
        kBad += strcmpk(S3, "z") != 0 ? 1 : 0
        S4 = Copied[kI0]
        kBad += strcmpk(S4, "z") != 0 ? 1 : 0
        S5 = Joined[kI3]
        kBad += strcmpk(S5, "z") != 0 ? 1 : 0
        S6 = Padded[kI1]
        kBad += strcmpk(S6, "z") != 0 ? 1 : 0
        S7 = Chosen[kI1]
        kBad += strcmpk(S7, "w") != 0 ? 1 : 0
        S8 = Kept[kI0]
        kBad += strcmpk(S8, "z") != 0 ? 1 : 0
        S9 = Picked[kI1]
        kBad += strcmpk(S9, "cc") != 0 ? 1 : 0
        S10 = Cut[kI0]
        kBad += strcmpk(S10, "z") != 0 ? 1 : 0
        S11 = Sliced[kI0]
        kBad += strcmpk(S11, "z") != 0 ? 1 : 0
        S12 = Joined2[kI1]
        kBad += strcmpk(S12, "bbbbz") != 0 ? 1 : 0
        S13 = Doubled[kI0]
        kBad += strcmpk(S13, "zz") != 0 ? 1 : 0
        S14 = Same[kI2]
        kBad += strcmpk(S14, "z") != 0 ? 1 : 0
        S15 = Dest[kI0]
        kBad += strcmpk(S15, "z") != 0 ? 1 : 0
        S16 = Dest[kI1]
        kBad += strcmpk(S16, "ddd") != 0 ? 1 : 0
        S17 = Queue[kI1]
        kBad += strcmpk(S17, "q") != 0 ? 1 : 0
        S18 = Upper[kI0]
        kBad += strcmpk(S18, "Z") != 0 ? 1 : 0
        S19 = Case[kI0]
        kBad += strcmpk(S19, "Z") != 0 ? 1 : 0
        S20 = Case[kI1]
        kBad += strcmpk(S20, "CD") != 0 ? 1 : 0
        S21 = CaseView[kI0]
        kBad += strcmpk(S21, "q") != 0 ? 1 : 0
        S22 = Capital[kI1]
        kBad += strcmpk(S22, "Bbbbz") != 0 ? 1 : 0
        S23 = Swapped[kI1]
        kBad += strcmpk(S23, "BBBBZ") != 0 ? 1 : 0
        S24 = Toggle[kI0]
        kBad += strcmpk(S24, "ab") != 0 ? 1 : 0
        S25 = Titled[kI3]
        kBad += strcmpk(S25, "Z") != 0 ? 1 : 0
        S26 = Titled[kI4]
        kBad += strcmpk(S26, "Bbbb") != 0 ? 1 : 0
        S27 = Stripped[kI0]
        kBad += strcmpk(S27, "z") != 0 ? 1 : 0
        S28 = Spaced[kI1]
        kBad += strcmpk(S28, "bbbb") != 0 ? 1 : 0
        S29 = Unfenced[kI0]
        kBad += strcmpk(S29, "aba") != 0 ? 1 : 0
        S30 = Edge[kI0]
        kBad += strcmpk(S30, "") != 0 ? 1 : 0
        S31 = Centered[kI0]
        kBad += strcmpk(S31, " z ") != 0 ? 1 : 0
        S35 = OneRight[kI0]
        kBad += strcmpk(S35, "  q") != 0 ? 1 : 0
        S36 = LateView[kI0]
        kBad += strcmpk(S36, "   x") != 0 ? 1 : 0
        S37 = NineFilled[kI0]
        kBad += strcmpk(S37, "---9") != 0 ? 1 : 0
        S38 = Nine2[kI0]
        kBad += strcmpk(S38, "9...") != 0 ? 1 : 0
        S39 = RepOut[kI0]
        kBad += strcmpk(S39, "a-a-") != 0 ? 1 : 0
        S40 = Rep2[kI0]
        kBad += strcmpk(S40, "----") != 0 ? 1 : 0
        S41 = Slide[kI0]
        kBad += strcmpk(S41, "aabaaaaaaaa") != 0 ? 1 : 0
        S42 = Cutted[kI0]
        kBad += strcmpk(S42, "abc") != 0 ? 1 : 0
        S43 = Shrink[kI0]
        kBad += strcmpk(S43, "ijk") != 0 ? 1 : 0
        S44 = CutView[kI0]
        kBad += strcmpk(S44, "cdef") != 0 ? 1 : 0
        S45 = Trim[kI0]
        kBad += strcmpk(S45, "efgh") != 0 ? 1 : 0
        S46 = OpPre[kI0]
        kBad += strcmpk(S46, "<z") != 0 ? 1 : 0
        S47 = OpTwice[kI0]
        kBad += strcmpk(S47, "zz") != 0 ? 1 : 0
        S48 = OpFixPlus[kI0]
        kBad += strcmpk(S48, "abz") != 0 ? 1 : 0
        kBad += QFind[kI0] != 1 ? 1 : 0
        kBad += QRfind[kI0] != 3 ? 1 : 0
        kBad += QStarts[kI0] != 0 ? 1 : 0
        kBad += QEnds[kI0] != 1 ? 1 : 0
        kBad += QLen[kI1] != 5 ? 1 : 0
        kBad += QAlpha[kI0] != 1 ? 1 : 0
        kBad += QConst[kI0] != 4 ? 1 : 0
        kBad += QFrom[kI0] != 3 ? 1 : 0
        kBad += QUpTo[kI0] != 0 ? 1 : 0
        S49 = NumStr[kI0]
        kBad += strcmpk(S49, "7.333333333333333") != 0 ? 1 : 0
        S50 = NumStr[kI1]
        kBad += strcmpk(S50, "0.5") != 0 ? 1 : 0
        kBad += NumBack[kI0] != 7 + 1/3 ? 1 : 0
        S51 = NumConstStr[kI1]
        kBad += strcmpk(S51, "2.0") != 0 ? 1 : 0
        kBad += StrConst[kI0] != 1.5 ? 1 : 0
        kNaN = StrConst[kI1]
        kBad += kNaN == kNaN ? 1 : 0
        kBad += strcmpk(SJoinSrc, "z+bbbb+cc") != 0 ? 1 : 0
        kBad += strcmpk(SJoinSep, "pzq") != 0 ? 1 : 0
        kBad += strcmpk(SJoinConst, "p-q") != 0 ? 1 : 0
        kBad += strcmpk(SSplitJoined, "z|bb=c") != 0 ? 1 : 0
        kBad += strcmpk(SRPartSrc, "z,bb|=|c") != 0 ? 1 : 0
        kBad += strcmpk(SSplitConst, "q,r|s") != 0 ? 1 : 0
        kBad += strcmpk(SSplitWhole, "z,bb=c") != 0 ? 1 : 0
        kBad += strcmpk(SPiecesJoined, "a|b") != 0 ? 1 : 0
        kBad += strcmpk(SRepGrow, "zzzz") != 0 ? 1 : 0
        kBad += strcmpk(SRepHeld, "a") != 0 ? 1 : 0
        kBad += strcmpk(SStripWs, "z--") != 0 ? 1 : 0
        kBad += strcmpk(SStripCh, "z") != 0 ? 1 : 0
        kBad += strcmpk(SStripHeld, "a") != 0 ? 1 : 0
        kBad += strcmpk(SStripWsHeld, "a--") != 0 ? 1 : 0
        kBad += strcmpk(SAddK, "z+") != 0 ? 1 : 0
        kBad += strcmpk(SAddHeld, "a+") != 0 ? 1 : 0
        kBad += strcmpk(SMulK, "zz") != 0 ? 1 : 0
        kBad += strcmpk(SMulHeld, "aa") != 0 ? 1 : 0
        kBad += strcmpk(SOpAdd, "z!") != 0 ? 1 : 0
        kBad += strcmpk(SOpMul, "zz") != 0 ? 1 : 0
        kBad += strcmpk(SUpK, "Z") != 0 ? 1 : 0
        kBad += kFindK != 2 ? 1 : 0
        kBad += kOrdK != 122 ? 1 : 0
        kBad += kOrdHeld != 97 ? 1 : 0
        kBad += strcmpk(SSubK, "z") != 0 ? 1 : 0
        kBad += strcmpk(SChrK, "Z") != 0 ? 1 : 0
        kBad += kOrdChr != 90 ? 1 : 0
        kBad += strcmpk(SBackK, "z") != 0 ? 1 : 0
        S32 = Wide[kI0]
        kBad += strcmpk(S32, "ab  ") != 0 ? 1 : 0
        S33 = Right[kI0]
        kBad += strcmpk(S33, "   z") != 0 ? 1 : 0
        S34 = Right[kI1]
        kBad += strcmpk(S34, "   c") != 0 ? 1 : 0
        gkBad = kBad
        ; the init pass runs this branch too, with timeinstk() still 0
        gkDone = timeinstk()
        printks("string results after the last pass: %d wrong\n", 0, kBad)
        turnoff
    endif
    kc += 1
endin

instr 2
    kOne init 1
    kLen init 2
    SA[] fillarray "a"
    A:CsnStr = csnfromarray(SA)
    Sv strsubk "longlonglonglonglong", 0, kLen
    Grown:CsnStr = csnstradd(A, Sv, kOne)
    kLen = 20
endin

instr 4
    kIdx[] fillarray 0
    ; the init pass runs both branches, the short one last
    if timeinstk() > 0 then
        Sv strcpyk "a string longer than its slot"
    else
        Sv strcpyk "keep"
    endif
    csnset(GKeep, kIdx, Sv)
endin

instr 5
    kOne init 1
    ; nothing is known at init, so the first pass pads, and "a" has no room
    kWidth init 0
    kWidth = 4
    csnstrrjust(GPad, kWidth, kOne)
endin

instr 6
    ; shrink the first string inside its buffer before instr 5 runs
    iIdx[] fillarray 0
    csnset(GPad, iIdx, "ab")
    csnset(GRepl, iIdx, "ab")
    turnoff
endin

instr 7
    kOne init 1
    ; the init pass runs both branches, the one matching nothing last; then
    ; "a" turns "ab" into "xxb", which fits, and "a" into "xx", which does not
    if timeinstk() > 0 then
        SOld strcpyk "a"
    else
        SOld strcpyk "q"
    endif
    csnstrreplace(GRepl, SOld, "xx", -1, kOne)
endin

; a join longer than the one the k init sized is refused: the separator
; grows from "+" at init to 10 bytes on the first pass
instr 8
    kOne init 1
    kc init 0
    kc += 1
    SJoinGrow[] fillarray "zzzz", "y"
    JoinGrow:CsnStr = csnfromarray(SJoinGrow)
    if kc == 1 then
        SGrowSep strcpyk "<-------->"
    else
        SGrowSep strcpyk "+"
    endif
    SJoined = csnstrjoin(JoinGrow, SGrowSep, kOne)
endin

; a piece longer than its slot is refused: "a" at init, "abcdefgh" on the
; first pass
instr 9
    kOne init 1
    if timeinstk() > 0 then
        SSplitGrow strcpyk "abcdefgh,b"
    else
        SSplitGrow strcpyk "a,b"
    endif
    SplitGrow:CsnStr = csnstrsplit(SSplitGrow, ",", -1, kOne)
endin

instr 3
    ; read at i-time: a k-rate if would let prints run at init either way
    iDone = i(gkDone)
    iBad = i(gkBad)
    SKept = GKeep[0]
    iKept = strcmp(SKept, "keep")
    SPadKept = GPad[0]
    iKept += strcmp(SPadKept, "ab")
    SReplKept = GRepl[0]
    iKept += strcmp(SReplKept, "ab")
    if iDone > 0 && iBad == 0 && iKept == 0 then
        prints("csnum string writes stayed in their buffers\n")
    endif
    turnoff
endin
</CsInstruments>

<CsScore>
i 1 0 0.1
i 2 0.2 0.01
i 4 0.25 0.01
i 6 0.26 0.01
i 5 0.27 0.01
i 7 0.28 0.01
i 8 0.29 0.01
i 9 0.295 0.01
i 3 0.3 0.01
</CsScore>
</CsoundSynthesizer>
