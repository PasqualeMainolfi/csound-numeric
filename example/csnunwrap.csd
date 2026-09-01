<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnunwrap.csd
;
; A phase that keeps folding back into [-pi, pi) is not differentiable. Unwrap
; it first, then csndiff gives the per-step frequency.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    two_pi:i        = 6.283185307179586
    pi_val:i        = 3.141592653589793

    wrapped:CsnArr  = csnfromarray(array(0, 3, -3, 0, 3))
    cont:CsnArr     = csnunwrap(wrapped, two_pi, pi_val)
    cont_out:i[]    = csntoarray(cont)
    prints("unwrapped = %.4f %.4f %.4f %.4f %.4f\n", cont_out[0], cont_out[1], cont_out[2], cont_out[3], cont_out[4])

    ; now the steps are meaningful
    step:CsnArr     = csndiff(cont)
    step_out:i[]    = csntoarray(step)
    prints("steps     = %.4f %.4f %.4f %.4f\n", step_out[0], step_out[1], step_out[2], step_out[3])

    ; wrapping it again gives the original back
    again:CsnArr    = csnwrap(cont, two_pi)
    again_out:i[]   = csntoarray(again)
    prints("re-wrapped = %.4f %.4f %.4f %.4f %.4f\n", again_out[0], again_out[1], again_out[2], again_out[3], again_out[4])
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
