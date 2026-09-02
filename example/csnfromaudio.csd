<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnfromaudio.csd
;
; csnfromaudio captures one control period of an audio signal into a CsnArr of
; ksmps elements, once per k-cycle. From there the whole suite applies, and
; csntoaudio takes the result back out.
; -----------------------------------------------------------------------------

sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

instr 1
    kHalf init 0.5

    aSig  oscili 0.5, 440
    block:CsnArr = csnfromaudio(aSig)

    ; an ordinary k-rate array from here on
    quieter:CsnArr = csnmul(block, kHalf)
    aOut  csntoaudio quieter

    ; the block really does hold ksmps samples
    if timeinstk() == 1 then
        shape:i[] = csnshape(block)
        prints("one block = %d samples (ksmps = %d)\n", shape[0], ksmps)
    endif

    kPeak maxk aOut, 1, 1
    if timeinstk() == 40 then
        printf("peak after halving = %.3f\n", 1, kPeak)
    endif
endin

</CsInstruments>
<CsScore>
i 1 0 0.05
</CsScore>
</CsoundSynthesizer>
