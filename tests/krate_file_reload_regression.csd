<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>

<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

; csnload.k re-reads the file on every trigger, with no cache in between. That
; is deliberate: csnload reads a file it does not own, so an unchanged path
; proves nothing about the bytes behind it, and a stat stamp would only narrow
; the window — on HFS+, SMB/NFS and FAT the mtime granularity is one to two
; seconds, wide enough for a same-size rewrite to hide in. The trigger is the
; whole contract, and these are the guarantees that follow from it.
;
; instr 1 reloads on every k-cycle for the whole run. The i-time notes around
; it rewrite the file or mutate the published array, and each checker asserts
; what the next reload must have produced.

giFirst[] = fillarray(1, 2, 3)
giSecond[] = fillarray(7, 8, 9, 10, 11)
giThird[] = fillarray(4, 5)

Seed@global:CsnArr = csnfromarray(giFirst)
Loaded@global:CsnArr = csnempty(array(0))
Gated@global:CsnArr = csnempty(array(0))

; The file has to exist before instr 1's first k-cycle.
csnsave Seed, "csnum_reload.csn"

; Reloads on every k-cycle, trigger permanently on.
instr 1
    Loaded = csnload("csnum_reload.csn", 1)
endin

; trig == 0: never reads, so the handle keeps the empty array the init pass
; published. The trigger is the only thing that may cause a read.
instr 2
    kNever = 0
    Gated = csnload("csnum_reload.csn", kNever)
endin

; --- rewrites, at i-time so they land on an exact k-cycle boundary ---------

instr 20
    Replacement:CsnArr = csnfromarray(giSecond)
    csnsave Replacement, "csnum_reload.csn"
endin

instr 21
    Replacement:CsnArr = csnfromarray(giThird)
    csnsave Replacement, "csnum_reload.csn"
endin

; Mutates the array csnload published, leaving the file alone. The next
; trigger has to overwrite it with the file's own contents again.
instr 22
    csnreverse Loaded
endin

; --- checkers -------------------------------------------------------------

instr 10
    iValues[] = csntoarray(Loaded)
    iSize = csnsize(Loaded)
    assert(iSize == 3)
    assert(iValues[0] == 1 && iValues[1] == 2 && iValues[2] == 3)
endin

; After instr 20: a longer file behind the same path must be picked up.
instr 11
    iValues[] = csntoarray(Loaded)
    iSize = csnsize(Loaded)
    assert(iSize == 5)
    assert(iValues[0] == 7 && iValues[4] == 11)
endin

; After instr 21: and a shorter one. The shape shrinks with it.
instr 12
    iValues[] = csntoarray(Loaded)
    iSize = csnsize(Loaded)
    assert(iSize == 2)
    assert(iValues[0] == 4 && iValues[1] == 5)
endin

; After instr 22 reversed it in place: the file never changed, and the reload
; still has to restore the file's own order over the mutation.
instr 13
    iValues[] = csntoarray(Loaded)
    iSize = csnsize(Loaded)
    assert(iSize == 2)
    assert(iValues[0] == 4 && iValues[1] == 5)
endin

; A run of cycles with nothing touched leaves the array where the last reload
; left it, and the gated handle never left its init-pass state.
instr 14
    iValues[] = csntoarray(Loaded)
    iSize = csnsize(Loaded)
    assert(iSize == 2)
    assert(iValues[0] == 4 && iValues[1] == 5)

    iGatedSize = csnsize(Gated)
    assert(iGatedSize == 0)
endin
</CsInstruments>

<CsScore>
i 1  0.000 0.100
i 2  0.000 0.100
i 10 0.010 0.001
i 20 0.020 0.001
i 11 0.030 0.001
i 21 0.040 0.001
i 12 0.050 0.001
i 22 0.060 0.001
i 13 0.070 0.001
i 14 0.090 0.001
e
</CsScore>
</CsoundSynthesizer>
