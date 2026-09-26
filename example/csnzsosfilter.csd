<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnzsosfilter.csd
;
; sosfilt with an explicit state, as scipy.signal.sosfilt(sos, x, zi=zi):
; zi holds two states per section, shape (n_sections, 2) for a 1-D signal,
; and the final state comes back as zf.
; -----------------------------------------------------------------------------

sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

instr 1
    sos:CsnArr = csnbuttersos(4, 1000, 0, sr)

    iblock[] = fillarray(1, 1, 1, 1, 1, 1, 1, 1)
    x:CsnArr = csnfromarray(iblock)
    izshape[] = fillarray(2, 2)
    z0:CsnArr = csnzeros(izshape)
    y1:CsnArr, z1:CsnArr csnzsosfilter sos, x, z0
    y2:CsnArr, z2:CsnArr csnzsosfilter sos, x, z1
    csnprint(y2)
    csnprint(z2)
    turnoff
endin

instr 2
    sos:CsnArr = csnbuttersos(4, 1000, 0, sr)
    izshape[] = fillarray(2, 2)
    z:CsnArr = csnzeros(izshape)
    anoise = noise(0.3, 0)
    afilt, z csnzsosfilter sos, anoise, z
    out afilt
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
i 2 0 0.1
</CsScore>
</CsoundSynthesizer>
