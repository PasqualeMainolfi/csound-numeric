<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnsn3dton3d.csd
;
; The per-channel gains that carry an SN3D-normalised set to N3D:
;
;   gain(n) = sqrt(2*n + 1)
;
; one value per ACN channel, so the vector is (order + 1)^2 long. The factor
; depends on the order n alone, not on the degree m, so the vector is constant
; over blocks of 2n+1 consecutive channels.
;
; It is a gain vector, not a converter: multiply your own set by it.
; csnn3dtosn3d builds the reciprocal.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    ; note the opcode syntax rather than csnsn3dton3d(2): a one-argument
    ; functional call returning a CsnArr is not what the parser expects
    gains:CsnArr csnsn3dton3d 2
    ig[] = csntoarray(gains)
    prints("order 2, %d channels\n", csnsize(gains))
    prints("  n=0 : %.4f\n", ig[0])
    prints("  n=1 : %.4f %.4f %.4f\n", ig[1], ig[2], ig[3])
    prints("  n=2 : %.4f %.4f %.4f %.4f %.4f\n", ig[4], ig[5], ig[6], ig[7], ig[8])

    ; applying it, and taking it back with the reciprocal vector
    isrc[] = fillarray(1, 1, 1, 1, 1, 1, 1, 1, 1)
    set:CsnArr = csnfromarray(isrc)
    n3d:CsnArr = csnmul(set, gains)
    back:CsnArr = csnmul(n3d, csnn3dtosn3d(2))
    ib[] = csntoarray(back)
    prints("round trip on a unit set: %.4f %.4f %.4f\n", ib[0], ib[4], ib[8])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
