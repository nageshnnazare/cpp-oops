# 18 — Internals: Virtual Dispatch (vtables, vptr, and how a call is lowered)

This is the chapter that makes `virtual` stop being magic. We'll dissect the
**vtable** structure exactly as the Itanium C++ ABI defines it, watch the
compiler **lower** `p->f()` into machine instructions, and cover the linker-level
facts experts must know: the **key function** rule, **vague linkage**, pure
virtuals, and **devirtualization**.

Prereq: [06-polymorphism-vtables.md](06-polymorphism-vtables.md), [17-internals-object-layout.md](17-internals-object-layout.md).

---

## 1. The exact shape of a vtable (Itanium ABI)

A vtable is *not* just an array of function pointers. It's a structure with parts
*above and below* the address the vptr points at:

<!--diagram
title: Itanium vtable structure
box[gray] offset-to-top
  text: signed offset from this subobject back to the most-derived object (0 for primary)
box[gray] RTTI pointer
  text: `&typeinfo` for the class (used by dynamic_cast / typeid)
box[blue] --- vptr points HERE ---
  text: address stored in the object's vptr
box[green] virtual function pointers
  text: slot 0, slot 1, ... in declaration order (this is the "address point")
-->
```
   Memory of the vtable (addresses increasing downward):

     [ offset-to-top   ]   <- (for adjusting 'this' to most-derived; 0 for primary)
     [ RTTI pointer    ]   <- &type_info for this class (dynamic_cast/typeid use it)
     [ &Class::f0      ]   <===  the object's vptr points HERE (the "address point")
     [ &Class::f1      ]
     [ &Class::f2      ]
     ...

   The vptr points AT THE FIRST FUNCTION SLOT. The two housekeeping fields sit at
   NEGATIVE offsets from the vptr:  vptr[-1] = RTTI,  vptr[-2] = offset-to-top.
```

```
   Object:                         Class vtable (shared by all instances):
   +------------------+            [ off-to-top = 0 ]
   | vptr ------------|----------> [ &type_info     ]
   | ...data...       |    +-----> [ &Derived::f0   ]  vptr[0]
   +------------------+    |       [ &Derived::f1   ]  vptr[1]
                          vptr     ...
```

The vtable is emitted **once per class** (in static/read-only memory) and shared
by every object of that class. Each object stores only the small `vptr`.

---

## 2. How the compiler lays out slots

Virtual functions get slots in **declaration order**, with the base class's
virtuals first (so a `Derived` vtable begins with the same slot order as `Base`,
overwritten by overrides). This positional stability is what makes dispatch a
constant-index array load.

```cpp
struct Base    { virtual void a(); virtual void b(); virtual ~Base(); };
struct Derived : Base { void b() override; virtual void c(); };
```

```
   Base vtable slots:        Derived vtable slots:
     [0] &Base::a              [0] &Base::a          (inherited, same slot)
     [1] &Base::b              [1] &Derived::b       (OVERRIDE -> same slot 1!)
     [2] complete dtor (D1)    [2] Derived complete dtor (D1)   (see ch 19)
     [3] deleting dtor (D0)    [3] Derived deleting dtor (D0)
                               [4] &Derived::c       (new virtual appended)

   Key insight: b() keeps slot [1] in BOTH tables. So 'p->b()' compiles to
   "load vptr; call vptr[1]" regardless of the dynamic type -> the override just
   changes WHAT's in slot 1, not WHERE the call looks.
```

---

## 3. Lowering `p->f()` to instructions

Here's the whole trick in ~3 instructions. Given `Base* p; p->b();` where `b` is
the 2nd virtual (slot index 1):

```
   C++:            p->b();

   Pseudo-asm (x86-64, System V), p in RDI:
     mov   rax, [rdi]          ; rax = p->vptr        (load the vtable pointer)
     mov   rax, [rax + 8]      ; rax = vptr[1]        (slot 1; 8 = 1 * 8 bytes)
     call  rax                 ; indirect call to the resolved function
   (this pointer 'p' is already in RDI as the hidden first argument)
```

```
   Non-virtual call:  call Base::b       (direct; address known at compile time;
                                          inlinable)
   Virtual call:      load vptr -> load slot -> indirect call
                      (2 dependent loads + an indirect branch; NOT inlinable
                       unless the compiler proves the dynamic type)

   Cost breakdown of a virtual call:
     * 2 dependent memory loads (vptr, then the slot) — cache/latency bound
     * an INDIRECT branch — the CPU's branch predictor must guess the target
       (monomorphic call sites predict well; megamorphic ones stall)
     * blocks inlining -> loses downstream optimizations (often the bigger cost)
```

Runnable: [`examples/ch18_vtable_dump.cpp`](examples/ch18_vtable_dump.cpp) reads a
live object's vptr and calls through the slots manually (ABI-specific, for
learning).

---

## 4. When is the vptr set? (during construction)

The vptr is **not** set once. The constructor writes the vptr **at each level**
as construction proceeds, so during `Base`'s constructor the object's vptr points
at `Base`'s vtable (this is *why* virtual calls in a base ctor resolve to `Base`
— chapter 06 §9, now explained).

```
   Constructing a Derived:
     1. Base ctor entry   : vptr := &Base::vtable      <-- virtual calls => Base's
        (Base ctor body runs; a virtual call here hits Base's slot)
     2. Derived ctor entry: vptr := &Derived::vtable   <-- now upgraded
        (Derived ctor body runs; virtual calls hit Derived's slot)
   Destruction reverses it: ~Derived sets vptr to Derived, then ~Base sets it to
   Base — so a virtual call in ~Base sees Base's version. (Chapter 19.)
```

```
   This per-stage vptr rewrite is real code the compiler injects. It's the
   mechanism behind "don't call virtuals in ctors/dtors": the vptr genuinely
   points at the CURRENTLY-CONSTRUCTED class's table, not the final one.
```

---

## 5. Pure virtual functions & `__cxa_pure_virtual`

A pure virtual `= 0` still occupies a slot. The ABI fills it with a pointer to a
handler (`__cxa_pure_virtual`) that aborts if ever called — which can happen if
you call a virtual during construction of an abstract base before the override
exists.

```
   Abstract class vtable slot for a pure virtual:
     [i] &__cxa_pure_virtual    ; calling it -> terminate("pure virtual called")

   You can trigger it: call a pure virtual indirectly from the base ctor before
   the derived override is installed -> runtime "pure virtual method called".
```

A pure virtual *with a body* (chapter 07 §5) puts the real function address in
the slot for derived classes to `Base::f()`-call, but the class stays abstract.

---

## 6. The key function rule & vague linkage (linker-level!)

A class's vtable and RTTI must be emitted **somewhere**, but a class defined in a
header is seen by many translation units. Two rules prevent duplicate-symbol
errors and bloat:

```
   KEY FUNCTION (Itanium ABI):
     The vtable is emitted in the ONE object file that defines the class's
     "key function" = the FIRST non-inline, non-pure virtual function.
     -> Put at least one virtual's definition in a .cpp ("anchor") to emit the
        vtable exactly once.

   If there is NO key function (all virtuals inline/pure), the vtable has
   VAGUE LINKAGE: it's emitted in EVERY TU that needs it, marked COMDAT/weak,
   and the LINKER merges the duplicates into one.
```

<!--diagram
title: Where does the vtable live?
box[green] Has a key function
  text: vtable emitted **once**, in the TU that defines the first non-inline virtual
box[orange] No key function (all inline/pure)
  text: vtable emitted in **every** TU as **COMDAT/weak**; linker deduplicates
-->
```
   +--------------------------------------------------------------+
   | class Widget { virtual void f(); ... };   // f NOT inline    |
   |   -> f is the KEY FUNCTION. Define it in widget.cpp.         |
   |   -> the vtable + typeinfo are emitted ONLY in widget.o.     |
   +--------------------------------------------------------------+
   The infamous linker error:
     "undefined reference to `vtable for Widget'"
   almost always means: you declared a non-inline virtual (the key function) but
   never DEFINED it -> the vtable was never emitted. Fix: define that virtual.
```

```
   Practical rule: give a polymorphic class at least one out-of-line virtual
   (often the destructor: 'Widget::~Widget() = default;' in the .cpp). This
   "anchors" the vtable to one object file, avoiding bloat and the classic
   "undefined reference to vtable" error.
```

---

## 7. Devirtualization — when `virtual` costs nothing

The compiler removes the indirect call when it can *prove* the dynamic type:

```cpp
struct Base { virtual int f(); };
struct Derived final : Base { int f() override { return 42; } };

Derived d;
d.f();                 // devirtualized: exact type known -> direct call/inline
Base& b = d;
b.f();                 // may devirtualize: 'b' provably refers to a Derived here

void g(Derived& d) { d.f(); }   // 'final' on the class/method -> direct call
```

```
   Devirtualization triggers:
     * the object's exact type is known at the call site (local of known type)
     * 'final' on the class or the overriding method (no further overrides
       possible -> the target is unique)
     * Link-Time Optimization (LTO) / whole-program devirtualization sees the
       full class hierarchy and proves there's a single override
     * profile-guided speculative devirtualization: "if (vptr==X) call X::f()
       directly; else fall back to indirect" for hot monomorphic call sites

   -> Mark leaf classes / final overrides 'final'. It's a real speed knob.
```

---

## 8. `sizeof` and multiple virtuals (recap with numbers)

```cpp
struct Empty {};                                sizeof == 1
struct OneVirtual { virtual ~OneVirtual(); };   sizeof == 8   (just the vptr)
struct Data { int a, b; virtual void f(); };    sizeof == 16  (vptr + 8 data)
```

```
   Adding a 2nd, 3rd, ... virtual function does NOT grow the object — they all
   share the ONE vptr and just add SLOTS to the (per-class) vtable. The per-
   OBJECT cost of virtual is exactly one pointer, no matter how many virtuals.
```

---

## 9. Summary

```
 +-------------------------------------------------------------------+
 | A vtable = [offset-to-top][RTTI ptr][slot0][slot1]... ; the vptr  |
 |   points at slot0. RTTI at vptr[-1], offset-to-top at vptr[-2].   |
 | Slots are positional (decl order, base first); an override        |
 |   replaces the CONTENTS of a fixed slot -> dispatch is a constant |
 |   index load.                                                     |
 | p->f() lowers to: load vptr; load vptr[i]; indirect call.         |
 |   Cost = 2 loads + indirect branch + lost inlining.               |
 | vptr is (re)written at EACH ctor/dtor stage -> that's why base    |
 |   ctors see base virtuals.                                        |
 | Pure virtual slot = &__cxa_pure_virtual (aborts if called).       |
 | KEY FUNCTION emits the vtable once; else vague/COMDAT linkage.    |
 |   Missing key fn -> "undefined reference to vtable".              |
 | 'final' / known type / LTO -> DEVIRTUALIZATION (virtual is free). |
 +-------------------------------------------------------------------+
```

Next: [19-internals-virtual-destructors.md](19-internals-virtual-destructors.md).
