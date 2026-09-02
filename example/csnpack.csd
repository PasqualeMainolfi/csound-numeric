<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnpack.csd
;
; csnpack folds an array of audio signals into one CsnArr shaped
; channels x ksmps, so multichannel material can be processed with the matrix
; opcodes. csnunpack takes it apart again.
; -----------------------------------------------------------------------------

sr = 48000
ksmps = 32
nchnls = 2
0dbfs = 1

instr 1
    a1 oscili 0.5, 220
    a2 oscili 0.5, 440
    a3 oscili 0.5, 880

    ins:a[] init 3
    ins[0] = a1
    ins[1] = a2
    ins[2] = a3

    frame:CsnArr = csnpack(ins)

    if timeinstk() == 1 then
        shape:i[] = csnshape(frame)
        prints("packed shape = %d x %d (channels x ksmps)\n", shape[0], shape[1])
    endif

    outs:a[] csnunpack frame
    aMix = (outs[0] + outs[1] + outs[2]) / 3
    kPeak maxk aMix, 1, 1
    if timeinstk() == 40 then
        printf("peak of the three-way mix = %.3f\n", 1, kPeak)
    endif
endin

</CsInstruments>
<CsScore>
i 1 0 0.05
</CsScore>
</CsoundSynthesizer>
