<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnzlfilter.csd
;
; lfilter with an explicit state, as scipy.signal.lfilter(b, a, x, zi=zi):
; the filter starts from zi and hands back its final state zf. Feeding zf in
; as the next zi filters successive blocks as one continuous signal.
; -----------------------------------------------------------------------------

sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

instr 1
    b:CsnArr, a:CsnArr csnbutterba 2, 2000, 0, sr

    ; two blocks of a step, filtered one after the other
    iblock[] = fillarray(1, 1, 1, 1)
    x:CsnArr = csnfromarray(iblock)
    izshape[] = fillarray(2)
    z0:CsnArr = csnzeros(izshape)
    y1:CsnArr, z1:CsnArr csnzlfilter b, a, x, z0
    y2:CsnArr, z2:CsnArr csnzlfilter b, a, x, z1
    csnprint(y1)
    csnprint(y2)
    csnprint(z2)
    turnoff
endin

instr 2
    ; on audio, zf fed back as zi on every control period
    b:CsnArr, a:CsnArr csnbutterba 2, 2000, 0, sr
    izshape[] = fillarray(2)
    z:CsnArr = csnzeros(izshape)
    anoise = noise(0.3, 0)
    afilt, z csnzlfilter b, a, anoise, z
    out afilt
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
i 2 0 0.1
</CsScore>
</CsoundSynthesizer>
