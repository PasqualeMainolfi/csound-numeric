<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnsize.csd
;
; csnsize counts elements, not extents. It is the count that moves as an array
; is pushed and popped, while csnshape reports what was reserved.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

instr 1
    shape:i[]  = fillarray(2, 3)
    mat:CsnArr = csnzeros(shape)
    mat_size:i = csnsize(mat)
    prints("2 x 3 matrix: size = %d\n", mat_size)

    ; reserved capacity is not content
    cap:i[]    = fillarray(4)
    buf:CsnArr = csnempty(cap)
    empty:i    = csnsize(buf)
    csnpush(buf, 1)
    csnpush(buf, 2)
    csnpush(buf, 3)
    filled:i   = csnsize(buf)
    popped:i   = csnpop(buf)
    after:i    = csnsize(buf)
    prints("empty = %d, after 3 pushes = %d, after 1 pop = %d (popped %g)\n", empty, filled, after, popped)
    turnoff
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
