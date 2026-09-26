<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnsosfilter.csd
;
; A filter given as second-order sections applied to an array along an axis,
; as scipy.signal.sosfilt, and to an audio signal. The sections run in
; cascade, each in transposed direct form II; sos[:, 3] must be 1.
; -----------------------------------------------------------------------------

sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

instr 1
    ; eighth-order elliptic bandpass, 500 to 700 Hz: stable as sections
    iband[] = fillarray(500, 700)
    sos:CsnArr = csnellipsos(8, iband, 1, 60, 2, sr)

    ; two channels of a step, one per row, filtered along the last axis
    istep[] = fillarray(1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0)
    ishape[] = fillarray(2, 8)
    flat:CsnArr = csnfromarray(istep)
    x:CsnArr = csnreshape(flat, ishape)
    y:CsnArr = csnsosfilter(sos, x)
    csnprint(y)
    turnoff
endin

instr 2
    iband[] = fillarray(500, 700)
    sos:CsnArr = csnellipsos(8, iband, 1, 60, 2, sr)
    anoise = noise(0.3, 0)
    afilt = csnsosfilter(sos, anoise)
    out afilt
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
i 2 0 0.1
</CsScore>
</CsoundSynthesizer>
