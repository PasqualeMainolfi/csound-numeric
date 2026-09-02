<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnstream.csd
;
; csnstream is the other end of csnsnap: it overlap-adds a stream of frames
; back into a continuous audio signal. It folds in one frame per hop samples of
; output, counted on its own clock, so any number of k-rate opcodes may sit
; between the two without changing the result.
;
; With a rectangular window and hop == frame the reconstruction is exact. At
; 50% overlap every sample is covered twice, so the amplitude doubles; a real
; analysis chain applies a window whose overlapped copies sum to one.
; -----------------------------------------------------------------------------

sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

instr 1
    aSig oscili 0.5, 440

    frame:CsnArr, kNew csnsnap aSig, 128, 128
    aOut, kOk csnstream frame, 128

    ; hop == frame: the frames tile the signal without overlapping, so the sum
    ; is the signal itself and the amplitude comes back unchanged. There is a
    ; fixed latency, so compare levels rather than sample against sample.
    kIn  maxk aSig, 1, 1
    kOut maxk aOut, 1, 1
    if timeinstk() == 60 then
        printf("hop == frame: in %.2f -> out %.2f\n", 1, kIn, kOut)
    endif
endin

instr 2
    aSig oscili 0.5, 440

    frame:CsnArr, kNew csnsnap aSig, 128, 64
    aOut, kOk csnstream frame, 64

    kIn  maxk aSig, 1, 1
    kAmp maxk aOut, 1, 1
    if timeinstk() == 60 then
        printf("50%% overlap, rectangular window: in %.2f -> out %.2f\n", 1, kIn, kAmp)
    endif
endin

</CsInstruments>
<CsScore>
i 1 0 0.05
i 2 0.1 0.05
</CsScore>
</CsoundSynthesizer>
