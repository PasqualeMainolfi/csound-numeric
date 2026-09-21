<CsoundSynthesizer>
<CsOptions>
-n -d
</CsOptions>

<CsInstruments>
sr = 48000
ksmps = 32
nchnls = 1
0dbfs = 1

/* csnforeach maps a callback over an array in place, once per control period
   the trigger is non-zero. Four things are pinned here.

   It maps, flat, whatever the rank, and keeps the shape. A zero trigger leaves
   the array alone. It is deliberately not idempotent: it rewrites its own
   source, so a second non-zero trigger applies the callback to the already
   mapped values. And it advances the array's data version, so a k-rate
   consumer downstream recomputes instead of serving what it cached before the
   first mapping.

   That last one is why instrument 1 sums through csnsum rather than reading
   the elements back: csnsum's init pass runs before any mapping and would keep
   answering 6 for the untouched source if the version never moved.

   The k-rate work runs in the early instruments and the element assertions run
   at i-time in instrument 10, which the score starts after they have ended.

   The source arrays are i-rate: csnfromarray on a k[] carries a performance
   pass that re-imports the Csound array every control period, which would undo
   the mapping between one pass and the next. */

/* The k-rate sum travels to instrument 10 through a global: a k-rate assert
   would be evaluated once at i-time too, before anything has been mapped. */
gkOnceSum init 0

giSource[] = fillarray(0, 1, 2, 3)
giGrid[] = fillarray(1, 2, 3, 4, 5, 6)
giGridShape[] = fillarray(2, 3)

Once@global:CsnArr = csnfromarray(giSource)
Twice@global:CsnArr = csnfromarray(giSource)
Held@global:CsnArr = csnfromarray(giSource)
GridFlat@global:CsnArr = csnfromarray(giGrid)
Grid@global:CsnArr = csnreshape(GridFlat, giGridShape)

opcode square_plus_one(x:k):k
    xout x * x + 1
endop

instr 1
    /* One mapping: 0 1 2 3 becomes 1 2 5 10, and the k-rate sum that follows
       has to see 18 rather than the 6 its init pass computed. */
    fn:OpcodeDef init "square_plus_one"
    op:Opcode create fn
    kin init 0
    kout:k init op, kin

    kpass init 0
    ktrig init 0
    kon init 1

    csnforeach(Once, op, ktrig)
    gkOnceSum = csnsum(Once, kon)

    /* The mapping is held back one control period on purpose. csnsum caches
       the sum of the untouched source on pass 0; the mapping lands on pass 1,
       and only a data version that moved makes csnsum walk the array again.
       Mapping on pass 0 would have made this untestable, because csnsum would
       then compute 18 the first time it ever ran. */
    if kpass == 0 then
        ktrig = 1
    elseif kpass == 1 then
        ktrig = 0
    endif
    kpass += 1
endin

instr 2
    /* Two mappings, to pin the compounding: 0 1 2 3 becomes 1 2 5 10 and then
       2 5 26 101. An opcode that skipped the second pass because nothing else
       had written the array would leave 1 2 5 10 here. */
    fn:OpcodeDef init "square_plus_one"
    op:Opcode create fn
    kin init 0
    kout:k init op, kin

    kpass init 0
    ktrig init 1

    csnforeach(Twice, op, ktrig)

    if kpass == 1 then
        ktrig = 0
    endif
    kpass += 1
endin

instr 3
    /* A zero trigger never maps, on any pass. */
    fn:OpcodeDef init "square_plus_one"
    op:Opcode create fn
    kin init 0
    kout:k init op, kin

    koff init 0
    csnforeach(Held, op, koff)
endin

instr 4
    /* The array is read flat, so a 2 x 3 matrix is mapped element by element
       and comes back 2 x 3. */
    fn:OpcodeDef init "square_plus_one"
    op:Opcode create fn
    kin init 0
    kout:k init op, kin

    kpass init 0
    ktrig init 1

    csnforeach(Grid, op, ktrig)

    if kpass == 0 then
        ktrig = 0
    endif
    kpass += 1
endin

instr 10
    i0[] = array(0)
    i1[] = array(1)
    i2[] = array(2)
    i3[] = array(3)

    /* csnsum's init pass ran before the mapping and answered 6 for the
       untouched source; 18 here means csnforeach advanced the data version and
       the consumer recomputed. */
    iOnceSum = i(gkOnceSum)
    assert(iOnceSum == 18)

    iOnce0 = csnget(Once, i0)
    iOnce1 = csnget(Once, i1)
    iOnce2 = csnget(Once, i2)
    iOnce3 = csnget(Once, i3)
    assert(iOnce0 == 1 && iOnce1 == 2 && iOnce2 == 5 && iOnce3 == 10)

    iTwice0 = csnget(Twice, i0)
    iTwice1 = csnget(Twice, i1)
    iTwice2 = csnget(Twice, i2)
    iTwice3 = csnget(Twice, i3)
    assert(iTwice0 == 2 && iTwice1 == 5 && iTwice2 == 26 && iTwice3 == 101)

    iHeld0 = csnget(Held, i0)
    iHeld1 = csnget(Held, i1)
    iHeld2 = csnget(Held, i2)
    iHeld3 = csnget(Held, i3)
    assert(iHeld0 == 0 && iHeld1 == 1 && iHeld2 == 2 && iHeld3 == 3)

    /* the source of the reshape is untouched: csnreshape publishes its own
       array, so mapping Grid must not reach back into GridFlat */
    iFlat0 = csnget(GridFlat, i0)
    iFlat3 = csnget(GridFlat, i3)
    assert(iFlat0 == 1 && iFlat3 == 4)

    iGridShape[] = csnshape(Grid)
    assert(iGridShape[0] == 2 && iGridShape[1] == 3)

    i00[] = array(0, 0)
    i02[] = array(0, 2)
    i10[] = array(1, 0)
    i12[] = array(1, 2)
    iGrid00 = csnget(Grid, i00)
    iGrid02 = csnget(Grid, i02)
    iGrid10 = csnget(Grid, i10)
    iGrid12 = csnget(Grid, i12)
    assert(iGrid00 == 2 && iGrid02 == 10 && iGrid10 == 17 && iGrid12 == 37)
endin
</CsInstruments>

<CsScore>
i1  0   0.1
i2  0   0.1
i3  0   0.1
i4  0   0.1
i10 0.2 0
</CsScore>
</CsoundSynthesizer>
