<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>
<CsInstruments>

; -----------------------------------------------------------------------------
; csnforeach.csd
;
; csnforeach maps a user-defined opcode over every element of an array, in
; place. The callback is an Opcode object: 'create' allocates it from the
; opcode name, and one 'init' call wires the argument cells csnforeach writes
; through. The callback must take one k-rate argument and return one.
;
; The array is read flat, so the rank does not matter: the 2 x 3 matrix below
; is mapped element by element and stays 2 x 3.
;
; csnforeach maps on every control period the trigger is non-zero, and it
; rewrites its own source, so an ungated pass applies the callback again to an
; already mapped array. The trigger here fires once.
; -----------------------------------------------------------------------------

sr = 44100
ksmps = 32
0dbfs = 1

; the callback: one k-rate argument in, one k-rate result out
opcode square_plus_one(x:k):k
    xout x * x + 1
endop

instr 1
    ; an i-rate source: csnfromarray on a k[] would re-import the Csound array
    ; every control period and undo the mapping
    src:i[]       = fillarray(0, 1, 2, 3)
    vec:CsnArr    = csnfromarray(src)

    mshape:i[]    = fillarray(2, 3)
    msrc:i[]      = fillarray(1, 2, 3, 4, 5, 6)
    mat:CsnArr    = csnreshape(csnfromarray(msrc), mshape)
    idims:i       = csndims(mat)

    fn:OpcodeDef  init "square_plus_one"
    op:Opcode     create fn
    kin           init 0
    kout:k        init op, kin        ; wires the callback's argument cells

    kpass         init 0
    ktrig         init 1

    csnforeach(vec, op, ktrig)
    csnforeach(mat, op, ktrig)

    if kpass == 0 then
        ktrig = 0                     ; one pass is enough
    elseif kpass == 1 then
        flat:k[]  = csntoarray(vec)
        printf    "vector = %g %g %g %g\n", 1, flat[0], flat[1], flat[2], flat[3]
        mflat:k[] = csntoarray(csnflatten(mat))
        printf    "matrix (dims %d) = %g %g %g %g %g %g\n", 1, idims, mflat[0], mflat[1], mflat[2], mflat[3], mflat[4], mflat[5]
    elseif kpass == 2 then
        held:k[]  = csntoarray(vec)
        printf    "zero trigger = %g %g %g %g\n", 1, held[0], held[1], held[2], held[3]
        turnoff
    endif

    kpass += 1
endin

</CsInstruments>
<CsScore>
i 1 0 0.1
</CsScore>
</CsoundSynthesizer>
