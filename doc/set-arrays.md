# Set arrays and their invariant

A set array is a one-dimensional real `CsnArr` whose elements are in ascending
order and occur exactly once. Set operations rely on this invariant to use
binary search and merge-like algorithms.

`csnlikeset` establishes the invariant: it reads an ordinary real array, sorts
its values, removes duplicates and returns a new array marked as a set. The set
operations (`csnsetunion`, `csnsetintersect`, `csnsetdiff`, `csnsetsymdiff` and
the set predicates) preserve it. The in-place `csnsetinsert` and
`csnsetremove` operations also preserve it by inserting and removing at the
sorted position.

Generic array operations do **not** promise to preserve the set invariant.
This includes generic in-place writes such as `csnset`, layout operations and
ordinary arithmetic or transformation opcodes. After a set array has been
modified outside the set API, set operations reject it instead of applying
binary search to data that may no longer be sorted or unique.

Pass the resulting array through `csnlikeset` again to restore the invariant:

```csound
raw:CsnArr      = csnfromarray(array(3, 1, 2, 2))
values:CsnArr   = csnlikeset(raw)       ; [1, 2, 3]

cell:i[] = array(0)
csnset(values, cell, 10)                ; generic write: no set guarantee

restored:CsnArr = csnlikeset(values)    ; [2, 3, 10], valid set again
```

`csnunlikeset` deliberately removes the set classification without changing
the payload. Use it when subsequent code should treat the values as an
ordinary array.

NaN values follow the set comparator rather than IEEE scalar equality: all NaN
values compare as the same set element and are ordered after finite values and
infinities. Consequently a normalized set contains at most one NaN.
