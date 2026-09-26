<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnbuttap.csd
;
; The analog Butterworth lowpass prototype of order N, cutoff 1 rad/s, as
; zeros, poles and gain, like scipy.signal.buttap: no zeros, N poles evenly
; spaced on the left half of the unit circle, gain 1. The zpk transforms move
; it to the band wanted, and the bilinear transform makes it digital.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    ; order 3: poles at -0.5 +- 0.866j and -1
    z:CsnArr, p:CsnArr, ik csnbuttap 3
    csnprint(z)
    csnprint(p)
    prints("gain = %g\n", ik)

    ; a digital lowpass at 1 kHz: move the cutoff, pre-warped, then bilinear
    ifs = 44100
    iwc = 2 * ifs * tan($M_PI * 1000 / ifs)
    zl:CsnArr, pl:CsnArr, ikl csnlptolpzpk z, p, ik, iwc
    zd:CsnArr, pd:CsnArr, ikd csnbilinearzpk zl, pl, ikl, ifs
    sos:CsnArr = csnzpktosos(zd, pd, ikd)
    csnprint(sos)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
