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

opcode track_diff, 0, aa
    aReference, aRealtime xin
    kSample = 0
    while kSample < ksmps do
        kExpected = vaget(kSample, aReference)
        kActual = vaget(kSample, aRealtime)
        gkMaxDiff = max(gkMaxDiff, abs(kActual - kExpected))
        kSample += 1
    od
endop

instr 1 ; Chebyshev I
    iOrder = p4
    iType = p5
    iCutoff = p6
    Sos:CsnArr = csncheby1sos(iOrder, iCutoff, 1, iType, sr)
    ain mpulse 1, 0
    kCutoff init iCutoff
    aReference = csnsosfilter(Sos, ain)
    aRealtime = csnrtcheby1(ain, iOrder, kCutoff, 1, iType, sr)
    track_diff(aReference, aRealtime)
endin

instr 2 ; Chebyshev II
    iOrder = p4
    iType = p5
    iCutoff = p6
    Sos:CsnArr = csncheby2sos(iOrder, iCutoff, 40, iType, sr)
    ain mpulse 1, 0
    kCutoff init iCutoff
    aReference = csnsosfilter(Sos, ain)
    aRealtime = csnrtcheby2(ain, iOrder, kCutoff, 40, iType, sr)
    track_diff(aReference, aRealtime)
endin

instr 3 ; elliptic
    iOrder = p4
    iType = p5
    iCutoff = p6
    Sos:CsnArr = csnellipsos(iOrder, iCutoff, 1, 60, iType, sr)
    ain mpulse 1, 0
    kCutoff init iCutoff
    aReference = csnsosfilter(Sos, ain)
    aRealtime = csnrtellip(ain, iOrder, kCutoff, 1, 60, iType, sr)
    track_diff(aReference, aRealtime)
endin

instr 4 ; cutoff moves at k-rate: stays bounded, then settles on the new design
    LowSos:CsnArr = csnellipsos(4, 500, 1, 60, 0, sr)
    ain oscili 0.25, 4000
    kCutoff = timeinstk() < 3 ? 500 : 4000
    aLow = csnsosfilter(LowSos, ain)
    aE = csnrtellip(ain, 4, kCutoff, 1, 60, 0, sr)
    aC1 = csnrtcheby1(ain, 5, kCutoff, 1, 0, sr)
    aC2 = csnrtcheby2(ain, 5, kCutoff, 40, 0, sr)
    kSample = 0
    while kSample < ksmps do
        kActual = vaget(kSample, aE)
        kC1 = vaget(kSample, aC1)
        kC2 = vaget(kSample, aC2)
        gkMaxAbs = max(gkMaxAbs, abs(kActual), abs(kC1), abs(kC2))
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
    prints "rtiir max diff %g, max abs %g, switch diff %g\n", iMaxDiff, iMaxAbs, iSwitchDiff
    assert(iMaxDiff < 1e-10)
    assert(iMaxAbs < 10)
    assert(iSwitchDiff > 0.01)
endin
</CsInstruments>
<CsScore>
i 1 0     0.004 1 0 1000
i 1 0.005 0.004 2 1 1000
i 1 0.010 0.004 3 0 4000
i 1 0.015 0.004 8 1 4000
i 2 0.020 0.004 1 0 1000
i 2 0.025 0.004 2 1 1000
i 2 0.030 0.004 5 0 4000
i 2 0.035 0.004 8 1 4000
i 3 0.040 0.004 1 0 1000
i 3 0.045 0.004 2 1 1000
i 3 0.050 0.004 5 0 4000
i 3 0.055 0.004 8 1 4000
i 4 0.060 0.020
i 99 0.081 0.001
e
</CsScore>
</CsoundSynthesizer>
