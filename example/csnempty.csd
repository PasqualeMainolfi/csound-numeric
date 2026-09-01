<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnempty.csd
;
; csnempty reserves a shape without publishing any element. The reservation is
; the capacity csnpush fills, so pushing up to it never reallocates.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    cap:i[]     = fillarray(4)
    buf:CsnArr  = csnempty(cap)

    size:i      = csnsize(buf)
    empty:i     = csnisempty(buf)
    shape:i[]   = csnshape(buf)
    prints("size = %d, isempty = %d, reserved extent = %g\n", size, empty, shape[0])

    csnpush(buf, 10)
    csnpush(buf, 20)
    filled:i    = csnsize(buf)
    buf_out:i[] = csntoarray(buf)
    prints("after two pushes: size = %d, values = %g %g\n", filled, buf_out[0], buf_out[1])

    ; an empty array travels through the suite instead of stopping it
    fresh:CsnArr = csnempty(cap)
    total:i      = csnsum(fresh)
    prints("sum over an empty array = %g\n", total)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
