<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnn3dtosn3d.csd
;
; The per-channel gains that carry an N3D-normalised set to SN3D:
;
;   gain(n) = 1 / sqrt(2*n + 1)
;
; the reciprocal of what csnsn3dton3d builds, one value per ACN channel, so
; the vector is (order + 1)^2 long. The factor depends on the order n alone,
; not on the degree m.
;
; It is a gain vector, not a converter: multiply your own set by it.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    ; note the opcode syntax rather than csnn3dtosn3d(1): a one-argument
    ; functional call returning a CsnArr is not what the parser expects
    gains:CsnArr csnn3dtosn3d 1
    ig[] = csntoarray(gains)
    prints("order 1, %d channels\n", csnsize(gains))
    prints("  n=0 : %.4f          (W is untouched)\n", ig[0])
    prints("  n=1 : %.4f %.4f %.4f\n", ig[1], ig[2], ig[3])

    ; the two vectors multiply to one, channel by channel
    up:CsnArr = csnsn3dton3d(1)
    unit:CsnArr = csnmul(gains, up)
    iu[] = csntoarray(unit)
    prints("gains * reciprocal: %.4f %.4f %.4f %.4f\n", iu[0], iu[1], iu[2], iu[3])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
