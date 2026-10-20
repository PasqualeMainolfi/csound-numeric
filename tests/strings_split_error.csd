<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>

<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

; an empty separator is an error, as in Python: at init, and in a k pass
; whose separator turns empty; a maxsplit that is not whole is refused too,
; and nothing is left locked

instr 1
    Result:CsnStr = csnstrsplit("a,b", "", -1)
endin

instr 2
    kOne init 1
    ; the init pass runs both branches, the "," one last
    if timeinstk() > 0 then
        SSep strcpyk ""
    else
        SSep strcpyk ","
    endif
    Result:CsnStr = csnstrrpartition("a,b", SSep, kOne)
endin

instr 3
    Result:CsnStr = csnstrrsplit("a,b", ",", 1.5)
endin

instr 4
    Result:CsnStr = csnstrsplit("a,b", ",", -1)
    SJoined = csnstrjoin(Result, "|")
    iCmp = strcmp(SJoined, "a|b")
    assert(iCmp == 0)
    prints("csnum split alive after every refusal\n")
endin
</CsInstruments>

<CsScore>
i1 0    0.01
i2 0.02 0.01
i3 0.04 0.01
i4 0.06 0.01
</CsScore>
</CsoundSynthesizer>
