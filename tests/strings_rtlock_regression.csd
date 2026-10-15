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

instr 3
    ; read at i-time: a k-rate if would let prints run at init either way
    iDone = i(gkDone)
    iBad = i(gkBad)
    SKept = GKeep[0]
    iKept = strcmp(SKept, "keep")
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
i 3 0.3 0.01
</CsScore>
</CsoundSynthesizer>
