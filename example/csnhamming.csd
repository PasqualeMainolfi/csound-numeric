<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnhamming.csd
;
; Like the Hann but with the first side lobe cancelled. It does not reach zero
; at the ends, which is the trade.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    win:CsnArr   = csnhamming(8)
    win_out:i[]  = csntoarray(win)
    n:i          = csnsize(win)
    prints("n = %d\n", n)
    prints("first half = %.4f %.4f %.4f %.4f\n", win_out[0], win_out[1], win_out[2], win_out[3])
    prints("second half = %.4f %.4f %.4f %.4f\n", win_out[4], win_out[5], win_out[6], win_out[7])

    peak:i       = csnmax(win)
    total:i      = csnsum(win)
    prints("peak = %.4f, sum = %.4f\n", peak, total)

    ; shaping a buffer is one multiplication
    buf:CsnArr   = csnones(fillarray(8))
    shaped:CsnArr = csnmul(buf, win)
    shaped_out:i[] = csntoarray(shaped)
    prints("shaped = %.4f %.4f %.4f %.4f\n", shaped_out[0], shaped_out[1], shaped_out[2], shaped_out[3])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
