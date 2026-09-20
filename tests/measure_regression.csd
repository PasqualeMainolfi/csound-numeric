<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>

<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

giShape2x2[] = fillarray(2, 2)
giShape2x3[] = fillarray(2, 3)

; --- Sabine / Eyring ---------------------------------------------------
Vols@global:CsnArr       = csnfromarray(array(100, 200))
Surf1D@global:CsnArr     = csnfromarray(array(10, 20))
Alpha1D@global:CsnArr    = csnfromarray(array(0.1, 0.2))
Surf2D@global:CsnArr     = csnreshape(csnfromarray(array(10, 20, 10, 20)), giShape2x2)
Alpha2D@global:CsnArr    = csnreshape(csnfromarray(array(0.1, 0.2, 0.1, 0.2)), giShape2x2)

; --- required absorption / Schroeder -----------------------------------
T60s@global:CsnArr       = csnfromarray(array(2, 4))

; --- modal frequencies --------------------------------------------------
Room1D@global:CsnArr     = csnfromarray(array(4, 5, 3))
Rooms2D@global:CsnArr    = csnreshape(csnfromarray(array(4, 5, 3, 8, 5, 3)), giShape2x3)
Mode100@global:CsnArr    = csnfromarray(array(1, 0, 0))
Mode200@global:CsnArr    = csnfromarray(array(2, 0, 0))

; --- sample/time conversion & dB sum ------------------------------------
Samps@global:CsnArr      = csnfromarray(array(48, 96))
Levels@global:CsnArr     = csnreshape(csnfromarray(array(0, 0, 0, 0)), giShape2x2)

; --- k-rate results -----------------------------------------------------
SabArrK@global:CsnArr    = csnempty(array(0))
AbsHHK@global:CsnArr     = csnempty(array(0, 0))
AbsSHK@global:CsnArr     = csnempty(array(0))
AbsHSK@global:CsnArr     = csnempty(array(0))
FsHHK@global:CsnArr      = csnempty(array(0, 0))
FsHSK@global:CsnArr      = csnempty(array(0))
FModesK@global:CsnArr    = csnempty(array(0))
MillisK@global:CsnArr    = csnempty(array(0))
DbAxK@global:CsnArr      = csnempty(array(0))
FbgHHK@global:CsnArr     = csnempty(array(0, 0))
FbgSHK@global:CsnArr     = csnempty(array(0))
FbgHSK@global:CsnArr     = csnempty(array(0))
T60HSK@global:CsnArr     = csnempty(array(0))

Delays@global:CsnArr     = csnfromarray(array(0.010, 0.023, 0.041))
T60Targets@global:CsnArr = csnfromarray(array(1.5, 3.0))

gkT60 init 0

gkSab init 0
gkEyr init 0
gkF100 init 0
gkDb init 0

instr 1
    ; ---- Sabine scalar: A = sum(S*alpha) = 1 + 4 = 5 -> 0.161*100/5
    iSab = csnt60sab(100, Surf1D, Alpha1D)
    assert(abs(iSab - 3.22) < 1e-12)

    ; ---- Eyring scalar: S = 30, A = 5 -> -0.161*100/(30*log(1-1/6))
    iEyrRef = -0.161 * 100 / (30 * log(1 - 5/30))
    iEyr = csnt60eyr(100, Surf1D, Alpha1D)
    assert(abs(iEyr - iEyrRef) < 1e-12)

    ; ---- Sabine array: one T60 per volume
    SabArr:CsnArr = csnt60sab(Vols, Surf2D, Alpha2D)
    iSabArr[] = csntoarray(SabArr)
    assert(csndims(SabArr) == 1 && csnsize(SabArr) == 2)
    assert(abs(iSabArr[0] - 3.22) < 1e-12)
    assert(abs(iSabArr[1] - 6.44) < 1e-12)

    ; ---- required absorption, volumes x targets
    AbsHH:CsnArr = csnrt60absp(Vols, T60s)
    iAbsShape[] = csnshape(AbsHH)
    iAbsHH[][] = csntoarray(AbsHH)
    assert(csndims(AbsHH) == 2)
    assert(iAbsShape[0] == 2 && iAbsShape[1] == 2)
    assert(abs(iAbsHH[0][0] - 8.05) < 1e-12)
    assert(abs(iAbsHH[0][1] - 4.025) < 1e-12)
    assert(abs(iAbsHH[1][0] - 16.1) < 1e-12)
    assert(abs(iAbsHH[1][1] - 8.05) < 1e-12)

    ; ---- scalar volume, array of targets
    AbsSH:CsnArr = csnrt60absp(100, T60s)
    iAbsSH[] = csntoarray(AbsSH)
    assert(csndims(AbsSH) == 1 && csnsize(AbsSH) == 2)
    assert(abs(iAbsSH[0] - 8.05) < 1e-12)
    assert(abs(iAbsSH[1] - 4.025) < 1e-12)

    ; ---- array of volumes, scalar target
    AbsHS:CsnArr = csnrt60absp(Vols, 2)
    iAbsHS[] = csntoarray(AbsHS)
    assert(csndims(AbsHS) == 1 && csnsize(AbsHS) == 2)
    assert(abs(iAbsHS[0] - 8.05) < 1e-12)
    assert(abs(iAbsHS[1] - 16.1) < 1e-12)

    ; ---- Schroeder frequency
    iFs = csnfschrd(100, 2)
    assert(abs(iFs - 2000 * sqrt(2 / 100)) < 1e-9)
    FsHS:CsnArr = csnfschrd(Vols, 2)
    iFsHS[] = csntoarray(FsHS)
    assert(abs(iFsHS[0] - 2000 * sqrt(2 / 100)) < 1e-9)
    assert(abs(iFsHS[1] - 2000 * sqrt(2 / 200)) < 1e-9)

    ; ---- modal frequency, scalar room
    iF100 = csnfpqr(Room1D, Mode100, 343)
    assert(abs(iF100 - 343 / 2 * 0.25) < 1e-12)
    ; a second-order mode is a legal index
    iF200 = csnfpqr(Room1D, Mode200, 343)
    assert(abs(iF200 - 343 / 2 * 0.5) < 1e-12)

    ; ---- modal frequency, one row per room
    FModes:CsnArr = csnfpqr(Rooms2D, Mode100, 343)
    iFModes[] = csntoarray(FModes)
    assert(csnsize(FModes) == 2)
    assert(abs(iFModes[0] - 343 / 2 * 0.25) < 1e-12)
    assert(abs(iFModes[1] - 343 / 2 * 0.125) < 1e-12)

    ; ---- sample/time conversion
    Millis:CsnArr = csnsamptomillis(Samps, 48000)
    iMillis[] = csntoarray(Millis)
    assert(abs(iMillis[0] - 1) < 1e-12 && abs(iMillis[1] - 2) < 1e-12)

    ; ---- dB sum
    iDb = csndbsum(Levels)
    assert(abs(iDb - 10 * log10(4)) < 1e-12)
    DbAx:CsnArr = csndbsum(Levels, 0)
    iDbAx[] = csntoarray(DbAx)
    assert(csnsize(DbAx) == 2)
    assert(abs(iDbAx[0] - 10 * log10(2)) < 1e-12)

    prints "i-rate measure checks passed\n"
endin

; The k overloads: every one of these is selected by its trailing trigger,
; so the arguments have to be k variables rather than constants.
instr 2
    kTrig = (timeinstk() == 2 ? 1 : 0)
    kVol init 100
    kTarget init 2
    kAxis0 init 0

    gkSab = csnt60sab(kVol, Surf1D, Alpha1D, kTrig)
    gkEyr = csnt60eyr(kVol, Surf1D, Alpha1D, kTrig)
    SabArrK = csnt60sab(Vols, Surf2D, Alpha2D, kTrig)

    AbsHHK = csnrt60absp(Vols, T60s, kTrig)
    AbsSHK = csnrt60absp(kVol, T60s, kTrig)
    AbsHSK = csnrt60absp(Vols, kTarget, kTrig)

    FsHHK = csnfschrd(Vols, T60s, kTrig)
    FsHSK = csnfschrd(Vols, kTarget, kTrig)

    gkF100 = csnfpqr(Room1D, Mode100, 343, kTrig)
    FModesK = csnfpqr(Rooms2D, Mode100, 343, kTrig)

    MillisK = csnsamptomillis(Samps, 48000, kTrig)

    gkDb = csndbsum(Levels, kTrig)
    DbAxK = csndbsum(Levels, kTrig, kAxis0)

    kDelay init 0.010
    kTarget60 init 1.5
    kGain init 0.7
    gkT60 = csnfbgtot60(kDelay, kGain)
    FbgHHK = csnt60tofbg(Delays, T60Targets, kTrig)
    FbgSHK = csnt60tofbg(kDelay, T60Targets, kTrig)
    FbgHSK = csnt60tofbg(Delays, kTarget60, kTrig)
    T60HSK = csnfbgtot60(Delays, kGain, kTrig)
endin

instr 3
    iEyrRef = -0.161 * 100 / (30 * log(1 - 5/30))
    assert(abs(i(gkSab) - 3.22) < 1e-12)
    assert(abs(i(gkEyr) - iEyrRef) < 1e-12)

    iSabArrK[] = csntoarray(SabArrK)
    assert(csnsize(SabArrK) == 2)
    assert(abs(iSabArrK[0] - 3.22) < 1e-12)
    assert(abs(iSabArrK[1] - 6.44) < 1e-12)

    ; the .hh.k overload had no perf pass at all before
    iAbsHHShape[] = csnshape(AbsHHK)
    iAbsHHK[][] = csntoarray(AbsHHK)
    assert(csndims(AbsHHK) == 2)
    assert(iAbsHHShape[0] == 2 && iAbsHHShape[1] == 2)
    assert(abs(iAbsHHK[0][0] - 8.05) < 1e-12)
    assert(abs(iAbsHHK[1][1] - 8.05) < 1e-12)

    iAbsSHK[] = csntoarray(AbsSHK)
    assert(abs(iAbsSHK[0] - 8.05) < 1e-12)
    assert(abs(iAbsSHK[1] - 4.025) < 1e-12)

    iAbsHSK[] = csntoarray(AbsHSK)
    assert(abs(iAbsHSK[0] - 8.05) < 1e-12)
    assert(abs(iAbsHSK[1] - 16.1) < 1e-12)

    iFsHHK[][] = csntoarray(FsHHK)
    assert(abs(iFsHHK[0][0] - 2000 * sqrt(2 / 100)) < 1e-9)
    assert(abs(iFsHHK[1][1] - 2000 * sqrt(4 / 200)) < 1e-9)

    iFsHSK[] = csntoarray(FsHSK)
    assert(abs(iFsHSK[0] - 2000 * sqrt(2 / 100)) < 1e-9)
    assert(abs(iFsHSK[1] - 2000 * sqrt(2 / 200)) < 1e-9)

    assert(abs(i(gkF100) - 343 / 2 * 0.25) < 1e-12)
    iFModesK[] = csntoarray(FModesK)
    assert(abs(iFModesK[0] - 343 / 2 * 0.25) < 1e-12)
    assert(abs(iFModesK[1] - 343 / 2 * 0.125) < 1e-12)

    iMillisK[] = csntoarray(MillisK)
    assert(abs(iMillisK[0] - 1) < 1e-12 && abs(iMillisK[1] - 2) < 1e-12)

    assert(abs(i(gkDb) - 10 * log10(4)) < 1e-12)
    iDbAxK[] = csntoarray(DbAxK)
    assert(csnsize(DbAxK) == 2)
    assert(abs(iDbAxK[0] - 10 * log10(2)) < 1e-12)

    ; --- comb feedback gain, the k-rate forms -----------------------------
    iWantG = 0.954992586021436
    assert(abs(i(gkT60) + 3 * 0.010 / log10(0.7)) < 1e-12)

    iFbgHHShape[] = csnshape(FbgHHK)
    assert(csndims(FbgHHK) == 2)
    assert(iFbgHHShape[0] == 3 && iFbgHHShape[1] == 2)
    iFbgHHK[][] = csntoarray(FbgHHK)
    assert(abs(iFbgHHK[0][0] - iWantG) < 1e-12)

    iFbgSHK[] = csntoarray(FbgSHK)
    assert(csndims(FbgSHK) == 1 && csnsize(FbgSHK) == 2)
    assert(abs(iFbgSHK[0] - iWantG) < 1e-12)

    ; the hs forms must stay one-dimensional: they used to pair the array
    ; with itself and publish an n x n matrix
    iFbgHSK[] = csntoarray(FbgHSK)
    assert(csndims(FbgHSK) == 1 && csnsize(FbgHSK) == 3)
    assert(abs(iFbgHSK[0] - iWantG) < 1e-12)

    iT60HSK[] = csntoarray(T60HSK)
    assert(csndims(T60HSK) == 1 && csnsize(T60HSK) == 3)
    assert(abs(iT60HSK[0] + 3 * 0.010 / log10(0.7)) < 1e-12)

    prints "k-rate measure checks passed\n"
endin

</CsInstruments>

<CsScore>
i 1 0 0.1
i 2 0 0.4
i 3 0.2 0.1
e
</CsScore>
</CsoundSynthesizer>
