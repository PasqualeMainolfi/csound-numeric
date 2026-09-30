<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

gkMaxDiff init 0
gkMaxAbs init 0
gkSwitchDiff init 0
gkCompared init 0

opcode track_diff, 0, aa
    aReference, aRealtime xin
    kSample = 0
    while kSample < ksmps do
        kExpected = vaget(kSample, aReference)
        kActual = vaget(kSample, aRealtime)
        gkMaxDiff = max(gkMaxDiff, abs(kActual - kExpected))
        gkCompared += 1
        kSample += 1
    od
endop

; p4 order, p5 type (2 = bandpass, 3 = bandstop), p6 centre, p7 width: the
; reference is the array design on [centre - width/2, centre + width/2]

instr 1 ; Butterworth
    iOrder = p4
    iType = p5
    iBand[] fillarray p6 - p7 / 2, p6 + p7 / 2
    Sos:CsnArr = csnbuttersos(iOrder, iBand, iType, sr)
    ain mpulse 1, 0
    kCentre init p6
    kWidth init p7
    aReference = csnsosfilter(Sos, ain)
    aRealtime = csnrtbutter(ain, iOrder, kCentre, kWidth, iType, sr)
    track_diff(aReference, aRealtime)
endin

instr 2 ; Chebyshev I
    iOrder = p4
    iType = p5
    iBand[] fillarray p6 - p7 / 2, p6 + p7 / 2
    Sos:CsnArr = csncheby1sos(iOrder, iBand, 1, iType, sr)
    ain mpulse 1, 0
    kCentre init p6
    kWidth init p7
    aReference = csnsosfilter(Sos, ain)
    aRealtime = csnrtcheby1(ain, iOrder, kCentre, kWidth, 1, iType, sr)
    track_diff(aReference, aRealtime)
endin

instr 3 ; Chebyshev II
    iOrder = p4
    iType = p5
    iBand[] fillarray p6 - p7 / 2, p6 + p7 / 2
    Sos:CsnArr = csncheby2sos(iOrder, iBand, 40, iType, sr)
    ain mpulse 1, 0
    kCentre init p6
    kWidth init p7
    aReference = csnsosfilter(Sos, ain)
    aRealtime = csnrtcheby2(ain, iOrder, kCentre, kWidth, 40, iType, sr)
    track_diff(aReference, aRealtime)
endin

instr 4 ; elliptic
    iOrder = p4
    iType = p5
    iBand[] fillarray p6 - p7 / 2, p6 + p7 / 2
    Sos:CsnArr = csnellipsos(iOrder, iBand, 1, 60, iType, sr)
    ain mpulse 1, 0
    kCentre init p6
    kWidth init p7
    aReference = csnsosfilter(Sos, ain)
    aRealtime = csnrtellip(ain, iOrder, kCentre, kWidth, 1, 60, iType, sr)
    track_diff(aReference, aRealtime)
endin

instr 5 ; the band moves at k-rate: stays bounded, then settles on the new design
    iLow[] fillarray 400, 600
    LowSos:CsnArr = csnellipsos(4, iLow, 1, 60, 2, sr)
    ain oscili 0.25, 4000
    kCentre = timeinstk() < 3 ? 500 : 4000
    kWidth = timeinstk() < 3 ? 200 : 1000
    aLow = csnsosfilter(LowSos, ain)
    aE = csnrtellip(ain, 4, kCentre, kWidth, 1, 60, 2, sr)
    aB = csnrtbutter(ain, 3, kCentre, kWidth, 3, sr)
    aC1 = csnrtcheby1(ain, 5, kCentre, kWidth, 1, 2, sr)
    aC2 = csnrtcheby2(ain, 5, kCentre, kWidth, 40, 3, sr)
    kSample = 0
    while kSample < ksmps do
        kActual = vaget(kSample, aE)
        gkMaxAbs = max(gkMaxAbs, abs(kActual), abs(vaget(kSample, aB)), abs(vaget(kSample, aC1)), abs(vaget(kSample, aC2)))
        if timeinstk() > 4 then
            kLow = vaget(kSample, aLow)
            gkSwitchDiff = max(gkSwitchDiff, abs(kActual - kLow))
        endif
        kSample += 1
    od
endin

instr 99
    iMaxDiff = i(gkMaxDiff)
    iMaxAbs = i(gkMaxAbs)
    iSwitchDiff = i(gkSwitchDiff)
    iCompared = i(gkCompared)
    prints "rtiir band max diff %g, max abs %g, switch diff %g, compared %d\n", iMaxDiff, iMaxAbs, iSwitchDiff, iCompared
    assert(iMaxDiff < 1e-10)
    assert(iMaxAbs < 10)
    assert(iSwitchDiff > 0.01)
    assert(iCompared > 1000)
endin
</CsInstruments>
<CsScore>
i 1 0     0.004 1 2 1000 400
i 1 0.005 0.004 2 3 1000 400
i 1 0.010 0.004 3 2 6000 3000
i 1 0.015 0.004 6 3 6000 3000
i 2 0.020 0.004 1 2 1000 400
i 2 0.025 0.004 2 3 1000 400
i 2 0.030 0.004 3 2 6000 3000
i 2 0.035 0.004 6 3 6000 3000
i 3 0.040 0.004 1 2 1000 400
i 3 0.045 0.004 2 3 1000 400
i 3 0.050 0.004 3 2 6000 3000
i 3 0.055 0.004 6 3 6000 3000
i 4 0.060 0.004 1 2 1000 400
i 4 0.065 0.004 2 3 1000 400
i 4 0.070 0.004 3 2 6000 3000
i 4 0.075 0.004 6 3 6000 3000
i 5 0.080 0.020
i 99 0.101 0.001
e
</CsScore>
</CsoundSynthesizer>
