# 20 — Internals: Multiple & Virtual Inheritance (thunks, VTT, vbase offsets)

Multiple inheritance forces the compiler to solve a hard problem: an object now
has **several base subobjects at different addresses**, yet a `Base*` must always
point at *its* subobject and `this` must be correct inside every method. The
machinery — **multiple vptrs**, **this-adjusting thunks**, **virtual-base
offsets**, the **VTT**, and **construction vtables** — is the deepest part of the
C++ ABI. This chapter demystifies it.

Prereq: [08-multiple-virtual-inheritance.md](08-multiple-virtual-inheritance.md), [18-internals-vtables.md](18-internals-vtables.md).

---

## 1. Multiple bases → multiple subobjects → multiple vptrs

```cpp
struct A { int a; virtual void fa(); };
struct B { int b; virtual void fb(); };
struct C : A, B { int c; void fa() override; void fb() override; };
```

<!--diagram
title: Layout of C : A, B
box[blue] A subobject (primary base) @ offset 0
  text: `vptr_A` + `int a` — shares offset 0 with C; C* == A* (no adjustment)
box[green] B subobject (secondary base) @ offset 8/16
  text: `vptr_B` + `int b` — a DIFFERENT address; C*->B* needs a pointer ADD
box[gray] C's own members
  text: `int c`
-->
```
   C object in memory:
   offset 0   +----------------+  <- C* and A* both point HERE (primary base)
              | vptr_A         |
              | int a          |
        16    +----------------+  <- B* points HERE (secondary base): C*+16
              | vptr_B         |
              | int b          |
              +----------------+
              | int c          |
              +----------------+

   TWO vptrs! One for the A-part, one for the B-part. Each secondary base with
   virtuals gets its own vptr into its own (sub-)vtable.
```

```
   Upcast adjusts the pointer VALUE:
     C* pc = &c;
     A* pa = pc;      // same address (offset 0) — no adjustment
     B* pb = pc;      // pb = (char*)pc + 16 — the compiler ADDS the offset!
   And downcast subtracts it. This is why 'static_cast' between related types can
   change the numeric pointer value (surprising if you thought pointers are just
   addresses).
```

Runnable: [`examples/ch20_this_adjust.cpp`](examples/ch20_this_adjust.cpp) prints
the differing subobject addresses.

---

## 2. `this`-pointer adjustment and thunks

Problem: `C::fb()` overrides `B::fb()`. When called through a `B*` (which points
at the B-subobject at offset +16), the vtable slot must deliver a `this` that
points at the **C** object (offset 0) so `C::fb` can see all of C. The ABI solves
this with a **thunk**: a tiny trampoline that adjusts `this` then jumps to the
real function.

```
   Call through B*:  pb->fb();
     pb points at the B-subobject (C address + 16).
     B-subobject's vtable slot for fb() does NOT point straight at C::fb.
     It points at a THUNK:

        _ZThn16_N1C2fbEv:              ; "thunk, this minus 16, C::fb"
            sub   rdi, 16              ; adjust this: B-subobj -> C object
            jmp   C::fb               ; tail-call the real override

   So the vtable slot = &thunk; the thunk fixes 'this' and jumps. Inside C::fb,
   'this' correctly points at the whole C.
```

<!--diagram
title: this-adjusting thunk
box[green] pb->fb() via B subobject
  text: vtable slot points at a **thunk**
  box[orange] thunk _ZThn16_N1C2fbEv
    text: `this -= 16` (B-subobj -> C), then `jmp C::fb`
  box[blue] C::fb
    text: runs with a correct C-based `this`
-->
```
   Mangled thunk names look like:  _ZThn16_N1C2fbEv
                                    ^^^ Th = thunk, n16 = this adjust -16 bytes
   You'll spot these in disassembly/nm output of any multiple-inheritance class.
   They're the runtime cost of MI: an extra pointer add (usually negligible).
```

---

## 3. `offset-to-top`: getting back to the most-derived object

Recall (chapter 18) each vtable has an **offset-to-top** field. For a secondary
base's vtable, it's non-zero — it tells the runtime how far the subobject is from
the start of the complete object. `dynamic_cast<void*>(p)` uses it to recover the
most-derived address.

```
   C object:
     A-subobj vtable: offset-to-top =   0   (A is primary; already at top)
     B-subobj vtable: offset-to-top = -16   (B sits +16 from top -> subtract 16)

   dynamic_cast<void*>(pb):
     read pb->vptr[-2] (offset-to-top = -16) -> most-derived = (char*)pb - 16
   -> yields the address of the whole C object. (More on RTTI in chapter 21.)
```

---

## 4. Virtual inheritance: the virtual-base offset

With virtual inheritance (the diamond fix, chapter 08), the shared base's
**location is not fixed at compile time** — it depends on the most-derived type.
So the compiler can't hardcode the offset to the virtual base; it stores a
**vbase offset** in the vtable and looks it up at runtime.

```cpp
struct V { int v; virtual void f(); };
struct L : virtual V { int l; };
struct R : virtual V { int r; };
struct D : L, R { int d; };     // ONE shared V, located via vbase offsets
```

<!--diagram
title: Diamond D : L, R (both virtually inherit V)
box[blue] L subobject
  text: `vptr_L` (its vtable holds the **vbase offset** to reach V), `int l`
box[green] R subobject
  text: `vptr_R` (holds ITS vbase offset to reach V), `int r`
box[gray] D's own member
  text: `int d`
box[orange] V subobject (SHARED, at the end)
  text: `vptr_V`, `int v` — one copy; found via runtime vbase offset, not a fixed delta
-->
```
   D object (one possible Itanium layout):
     +----------------+  <- D*, L*
     | vptr_L         |     (L's vtable stores "vbase offset to V")
     | int l          |
     +----------------+  <- R*
     | vptr_R         |     (R's vtable stores "vbase offset to V")
     | int r          |
     +----------------+
     | int d          |
     +----------------+  <- the shared V lives here
     | vptr_V         |
     | int v          |
     +----------------+

   To access V's members from an L*, the compiler:
     1. loads L's vptr -> reads the VBASE OFFSET stored in L's vtable
     2. adds it to the L* to reach the shared V
   -> virtual-base access is an EXTRA indirection (load offset, then add). This
      is the runtime cost of virtual inheritance you were warned about (ch 08).
```

```
   Why runtime? Because a plain L standalone puts V at a different place than an L
   inside a D. The SAME L code must work in both -> it can't hardcode the offset;
   it reads it from the vtable, which differs per most-derived type.
```

---

## 5. The VTT (Virtual Table Table) and construction vtables

Here's the subtlest piece. While a `D` is under construction, its base `L` is
being constructed too — and during `L`'s constructor, the object is "an `L`", not
yet a `D`. But the shared virtual base `V` is at the *D* layout position. So `L`'s
constructor needs a *different* vtable (with the *D*-appropriate vbase offset)
than a standalone `L` would use.

The ABI solves this with **construction vtables**, indexed by the **VTT** — a
table of vtables passed to base constructors.

```
   VTT (Virtual Table Table) for D: an array of vtable pointers used ONLY during
   construction/destruction, so each base ctor installs the correct (construction)
   vtable for the current stage.

   Construction of D:
     1. D's ctor constructs the shared virtual base V first (most-derived rule).
     2. D's ctor calls L's ctor, passing the VTT entry so L installs a
        CONSTRUCTION vtable whose vbase offset matches D's layout (not standalone L).
     3. Same for R.
     4. Finally D installs the real D vtables.

   Symbols you'll see:  _ZTT1D  = "VTT for D",  _ZTC...  = construction vtable.
```

<!--diagram
title: Why construction vtables exist
box[red] The problem
  text: during L's ctor inside a D, the shared V is at D's position, not standalone-L's
box[green] The fix: VTT + construction vtables
  text: D's ctor passes VTT entries so each base ctor installs a stage-correct vtable
-->
```
   This is why virtual inheritance makes constructors more expensive and why the
   ABI emits extra symbols (VTT, construction vtables) for classes with virtual
   bases. You rarely see it directly — but "why is there a _ZTT symbol?" and
   "why is virtual inheritance construction slower?" both answer here.
```

---

## 6. Cost summary of the inheritance flavors

```
   +-------------------------+------------------+---------------------------------+
   | Flavor                  | per-object cost  | per-call / access cost          |
   |-------------------------+------------------+---------------------------------|
   | single inheritance      | 1 vptr (if poly) | vtable load + indirect call     |
   | multiple inheritance    | N vptrs (per     | + possible this-adjust THUNK    |
   |                         |   polymorphic    |   (one add) on secondary bases  |
   |                         |   base)          |                                 |
   | virtual inheritance     | vptrs + shared   | + VBASE-OFFSET indirection to   |
   |                         | vbase; VTT/ctor  |  reach the shared base; costlier|
   |                         | vtables          |   construction                  |
   +-------------------------+------------------+---------------------------------+
```

```
   Practical takeaways for an expert:
     * Multiple inheritance of INTERFACES (no data) -> extra vptr(s) + occasional
       thunk. Cheap. Fine.
     * Virtual (diamond) inheritance -> extra indirection on base access + heavier
       ctors + VTT machinery. Use only for genuine shared-state diamonds.
     * Pointer casts between MI bases change the address value -> never
       reinterpret_cast across MI; use static_cast/dynamic_cast which adjust.
```

---

## 7. Summary

```
 +-------------------------------------------------------------------+
 | Multiple inheritance -> multiple base SUBOBJECTS at different     |
 |   offsets -> multiple VPTRs. Upcast/downcast ADJUSTS the pointer  |
 |   value (static_cast does it; reinterpret_cast would be wrong).   |
 | Overrides called via a secondary base go through a THIS-ADJUSTING |
 |   THUNK (sub offset; jmp real fn). Mangled _ZThn<offset>_...      |
 | offset-to-top in each (sub)vtable recovers the most-derived addr  |
 |   (used by dynamic_cast<void*>).                                  |
 | Virtual inheritance: the shared base's offset is RUNTIME -> stored|
 |   as a VBASE OFFSET in the vtable; access = load offset + add.    |
 | VTT + construction vtables give base ctors a stage-correct vtable |
 |   so virtual-base offsets are right mid-construction.             |
 +-------------------------------------------------------------------+
```

Next: [21-internals-rtti-dynamic-cast.md](21-internals-rtti-dynamic-cast.md).
