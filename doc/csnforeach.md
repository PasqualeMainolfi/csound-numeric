# csnforeach

## Abstract

Map a user-defined opcode over every element of an array, in place.

## Description

`csnforeach` calls a callback once per element and writes the result back into
the source array. The callback is an `Opcode` object, Csound 7's first-class
handle on an opcode: `opcoderef` (or `init`) names it, `create` allocates its
dataspace, and one `init` call wires the argument cells that `csnforeach`
writes through. All three steps belong to the orchestra, and `csnforeach`
refuses at init time if any of them is missing.

The callback must take exactly one k-rate argument and return exactly one
k-rate result. Any other rate is rejected rather than written through: an `i`
or constant cell would be overwritten with the element value, and a constant is
shared by the whole engine.

The array is read flat, so the rank does not matter and the shape is preserved.
Only real arrays are accepted.

There is no form that publishes a new handle: `csnforeach` rewrites its source
and returns nothing, like the in-place siblings elsewhere in the suite. It also
maps on every control period the trigger is non-zero, and since it rewrites its
own source, an ungated pass applies the callback again to an already mapped
array. Gate it with the trigger, or let it compound on purpose.

The callback runs with the array registry unlocked, so it may call other csnum
opcodes freely. It works on a copy of the elements taken before the first call,
and that copy is written back afterwards: a callback that modifies the same
array through another opcode will see its own writes overwritten. Freeing the
array or changing its element type from inside the callback is an error; a
change of size writes back the common prefix.

The callback's internal state advances once per element, so it should be a pure
function of its argument. NumPy's nearest idea is `np.vectorize`, but the
callback here is a Csound opcode and the result replaces the source.

## Syntax

```csound
csnforeach(source:CsnArr, fn:Opcode)
csnforeach(source:CsnArr, fn:Opcode, trig:k)
```

## Arguments

* `source:CsnArr`: the real array to map over. It is rewritten in place.
* `fn:Opcode`: the callback, created from an `OpcodeDef` and wired with one `init` call before use.
* `trig:k`: k-rate trigger, 1 when omitted. The array is mapped on a non-zero trigger; a zero trigger does nothing.

## Output

None. The source array is rewritten in place.

## Execution Time

* Performance (k-rate)

The init pass validates the callback and reserves the working buffer; the
mapping itself only runs at performance time.

## Examples

```csound
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
```

## See also

* [csnmul](csnmul.md)
* [csnclip](csnclip.md)
* [csnputmask](csnputmask.md)
* [csnreverse](csnreverse.md)

## Credits

Pasquale Mainolfi, 2026
