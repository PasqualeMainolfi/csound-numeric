<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>

<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

; The k forms of csnloadaudiofile and csnsaveaudiofile act when their trigger
; fires and only then, as csnload and csnsave do: no cache in between, since
; the file is not theirs. The load also reports the file's sample rate.
;
; instr 1 reloads on every k-cycle; instr 2 never fires; instr 3 writes once.
; The i-time notes around them rewrite the file and check what the next
; reload or the written file must hold. Every value is exact in single
; precision, the format the files are written in.

giStereo[] = fillarray(0.5, -0.25, 1, 0, -0.5, 0.75, 0, 0.25)
giShape24[] = fillarray(2, 4)
giThird[] = fillarray(0.125, -0.75)

Seed@global:CsnArr = csnreshape(csnfromarray(giStereo), giShape24)
Loaded@global:CsnArr = csnempty(array(0))
Gated@global:CsnArr = csnempty(array(0))
gkLoadedSr@global:k = init(0)
gkGatedSr@global:k = init(0)

; The file has to exist before instr 1's first k-cycle.
csnsaveaudiofile "csnum_kaudio.wav", Seed, 48000

; Reloads on every k-cycle, trigger permanently on.
instr 1
    Loaded, gkLoadedSr csnloadaudiofile "csnum_kaudio.wav", 0, 1
endin

; trig == 0: never reads, so the handle keeps the empty array the init pass
; published and the sample rate stays 0.
instr 2
    kNever = 0
    Gated, gkGatedSr csnloadaudiofile "csnum_kaudio.wav", 0, kNever
endin

; Writes once, on the first k-cycle, averaging the two channels into one.
instr 3
    kFirst = timeinstk() == 1 ? 1 : 0
    csnsaveaudiofile "csnum_kaudio_mixed.wav", Seed, 44100, 1, kFirst
endin

; --- rewrite, at i-time so it lands on an exact k-cycle boundary ----------

instr 20
    Replacement:CsnArr = csnfromarray(giThird)
    csnsaveaudiofile "csnum_kaudio.wav", Replacement, 22050
endin

; --- checkers -------------------------------------------------------------

instr 10
    iShape[] = csnshape(Loaded)
    iValues[][] = csntoarray(Loaded)
    assert(i(gkLoadedSr) == 48000)
    assert(iShape[0] == 2 && iShape[1] == 4)
    assert(iValues[0][0] == 0.5 && iValues[0][1] == -0.25 && iValues[1][1] == 0.75)
endin

; After instr 20: a one-channel file behind the same path loads 1-D, with
; its own sample rate.
instr 11
    iShape[] = csnshape(Loaded)
    iValues[] = csntoarray(Loaded)
    assert(i(gkLoadedSr) == 22050)
    assert(lenarray(iShape) == 1 && iShape[0] == 2)
    assert(iValues[0] == 0.125 && iValues[1] == -0.75)
endin

; The gated load never ran; the single write of instr 3 holds the average.
instr 12
    assert(csnsize(Gated) == 0)
    assert(i(gkGatedSr) == 0)

    Mixed:CsnArr, iMixedSr csnloadaudiofile "csnum_kaudio_mixed.wav"
    iMixedShape[] = csnshape(Mixed)
    iMixedValues[] = csntoarray(Mixed)
    assert(iMixedSr == 44100)
    assert(lenarray(iMixedShape) == 1 && iMixedShape[0] == 4)
    assert(iMixedValues[0] == 0 && iMixedValues[1] == 0.25 && iMixedValues[2] == 0.5 && iMixedValues[3] == 0.125)
endin
</CsInstruments>

<CsScore>
i 1  0.000 0.100
i 2  0.000 0.100
i 3  0.000 0.100
i 10 0.010 0.001
i 20 0.020 0.001
i 11 0.030 0.001
i 12 0.090 0.001
e
</CsScore>
</CsoundSynthesizer>
