<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnsnap.csd
;
; csnsnap slices an audio stream into overlapping frames of a size you choose,
; independent of ksmps. It publishes a frame every hop samples and raises its
; ready flag on the control period where that happens, so a consumer can skip
; the passes in between.
;
; The signal here is a ramp whose value is the sample index, so the first
; element of each frame names the sample the frame starts at.
; -----------------------------------------------------------------------------

sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

instr 1
    first:i[] = fillarray(0)

    aRamp line 0, p3, sr * p3

    ; 128-sample frames, a new one every 64 samples: 50% overlap
    frame:CsnArr, kReady csnsnap aRamp, 128, 64

    kStart = csnget(frame, first)
    if kReady == 1 then
        printf("frame ready, starts at sample %.0f\n", timeinstk(), kStart)
    endif
endin

</CsInstruments>
<CsScore>
i 1 0 0.01
</CsScore>
</CsoundSynthesizer>
