<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnfull.csd
;
; csnfull fills a shape with one value. A complex fill value fixes the element
; type on its own, so no itype argument is needed in that form.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    shape:i[]    = fillarray(4)
    full:CsnArr  = csnfull(shape, -3.5)
    full_out:i[] = csntoarray(full)
    prints("real    = %g %g %g %g\n", full_out[0], full_out[1], full_out[2], full_out[3])

    ; complex fill: the value's type fixes the array's
    z:Complex    = init(1, -2, 0)
    cpx:CsnArr   = csnfull(shape, z)
    itype:i      = csntype(cpx)
    cell:i[]     = fillarray(0)
    w:Complex    = csnget(cpx, cell)
    w_re:i       = real(w)
    w_im:i       = imag(w)
    prints("itype   = %d, cell0 = %g%+gi\n", itype, w_re, w_im)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
