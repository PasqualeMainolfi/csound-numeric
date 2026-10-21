<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>

<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

/* An S has no rate, so the operators on S (S1 + S2, Sx += Sy, S * n,
   Sx *= n) compute at init and, at perf, again only when an operand
   changed. Sx += "b" appends once, not on every pass; an init-time edit
   after S1 = S2 + S3 stays; an operand written at perf by strcpyk is
   followed on every pass it changes. A k count gives the operator a rate, so
   Sx *= kTwo doubles on every pass, as kx *= 2 does. */

instr 1
    kTwo init 2
    kc init 0
    kc += 1
    SOnce = "a"
    SOnce += "b"
    SEdited = "kick" + ".wav"
    SEdited strcat SEdited, "!"
    SRep = "ab"
    SRep *= 2
    ; the init pass runs both branches, the "q" one last
    if kc % 2 == 1 then
        SWriter strcpyk "zz"
    else
        SWriter strcpyk "q"
    endif
    SCopy strcpyk SWriter
    SCopy += "!"
    STimes = SWriter * 3
    SJoined = SWriter + "-" + SWriter
    SSuffixed = "x" + SWriter
    SDoubled = "a"
    SDoubled *= kTwo
    ; on pass 3 the writer holds "zz", not the "q" of init
    if kc == 3 then
        kBad = 0
        kBad += strcmpk(SOnce, "ab") != 0 ? 1 : 0
        kBad += strcmpk(SEdited, "kick.wav!") != 0 ? 1 : 0
        kBad += strcmpk(SRep, "abab") != 0 ? 1 : 0
        kBad += strcmpk(SCopy, "zz!") != 0 ? 1 : 0
        kBad += strcmpk(STimes, "zzzzzz") != 0 ? 1 : 0
        kBad += strcmpk(SJoined, "zz-zz") != 0 ? 1 : 0
        kBad += strcmpk(SSuffixed, "xzz") != 0 ? 1 : 0
        kBad += strlenk(SDoubled) != 16 ? 1 : 0
        printks("S operator results after 3 passes: %d wrong\n", 0, kBad)
        if kBad == 0 then
            printks("csnum S operators follow their operands\n", 0)
        endif
        turnoff
    endif
endin
</CsInstruments>

<CsScore>
i1 0 0.1
</CsScore>
</CsoundSynthesizer>
