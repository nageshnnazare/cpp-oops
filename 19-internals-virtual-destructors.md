# 19 — Internals: How Virtual Destructors *Actually* Work

Everyone learns the rule "give polymorphic base classes a virtual destructor."
Few know *why it works at the machine level* — that the compiler emits **up to
three** distinct destructor functions per class, that `delete p` calls through a
special vtable slot, and exactly what goes wrong without `virtual`. This chapter
makes it concrete.

Prereq: [18-internals-vtables.md](18-internals-vtables.md).

---

## 1. The Itanium ABI emits THREE destructors (D0/D1/D2)

For a class with a destructor, the compiler can generate three variants:

<!--diagram
title: The three destructor variants (Itanium ABI)
box[green] D2 - base object destructor
  text: destroys members + non-virtual bases only; does NOT touch virtual bases
box[blue] D1 - complete object destructor
  text: D2 + destroys virtual bases; the "normal" destructor for a full object
box[red] D0 - deleting destructor
  text: runs D1, then calls `operator delete` to free the storage
-->
```
   +--------+-------------------------+------------------------------------------+
   | Symbol | Name                    | What it does                             |
   |--------+-------------------------+------------------------------------------|
   | D2     | base object destructor  | destroy members + direct non-virtual     |
   |        |                         | bases (used as a subobject dtor)         |
   | D1     | complete object dtor    | D2 + destroy virtual bases (a whole obj) |
   | D0     | deleting destructor     | D1 + operator delete(this) (frees memory)|
   +--------+-------------------------+------------------------------------------+

   Mangled names (you'll see these in the linker/debugger):
     _ZN6WidgetD2Ev  = Widget::~Widget [base object]
     _ZN6WidgetD1Ev  = Widget::~Widget [complete object]
     _ZN6WidgetD0Ev  = Widget::~Widget [deleting]
```

Why three? Because "destroy this subobject," "destroy this whole object," and
"destroy AND free this heap object" are genuinely different jobs, especially once
virtual bases (chapter 20) are involved.

---

## 2. The vtable holds the destructor(s)

When a destructor is `virtual`, **two** slots are reserved in the vtable: the
**complete-object (D1)** and the **deleting (D0)** destructors, adjacent.

```
   Widget vtable (virtual dtor present):
     [ offset-to-top ]
     [ RTTI          ]
     [ &Widget::~D1  ]   <- complete object destructor  (vptr[k])
     [ &Widget::~D0  ]   <- deleting destructor          (vptr[k+1])
     [ &Widget::f    ]   ... other virtuals follow
```

```
   So a virtual destructor consumes TWO vtable slots (D1 and D0). A subclass
   provides its OWN D1/D0 in the same slots -> deleting through a base pointer
   finds the DERIVED deleting destructor. That's the whole mechanism.
```

---

## 3. How `delete p` is lowered

This is the crux. `delete p` where `p` is a pointer to a class with a **virtual**
destructor does NOT call a fixed function — it dispatches through the vtable's
**deleting-destructor (D0)** slot.

```
   C++:   Base* p = new Derived();  delete p;

   With a VIRTUAL destructor, 'delete p' lowers to roughly:
     mov  rax, [p]            ; load vptr
     mov  rax, [rax + D0off]  ; load the DELETING destructor slot (D0)
     mov  rdi, p              ; this = p
     call rax                 ; -> Derived::~D0 : runs ~Derived (D1) then frees
   Result: ~Derived runs, THEN ~Base (D1 chains to base D2), THEN operator delete.

   With a NON-VIRTUAL destructor, 'delete p' lowers to:
     ; static type is Base -> call Base's dtor DIRECTLY, no vtable
     call Base::~D0           ; only ~Base runs; frees sizeof(Base) worth
   Result: ~Derived is NEVER called. Derived's members leak; UB by the standard.
```

<!--diagram
title: delete p  (p : Base*, points at a Derived)
box[green] Virtual destructor (correct)
  text: `delete p` -> vtable **D0** slot -> Derived deleting dtor -> ~Derived, ~Base, free
box[red] Non-virtual destructor (BUG / UB)
  text: `delete p` -> **static** call to Base::~ only -> ~Derived skipped -> leak / UB
-->
```
   VIRTUAL:                                   NON-VIRTUAL:
     p->vptr -> Derived vtable                 (no vtable consulted for dtor)
       D0 slot = Derived deleting dtor         delete calls Base::~Base directly
         runs ~Derived (D1)                      runs ONLY ~Base
           chains to ~Base                        Derived part never destroyed
             operator delete(p)                   -> UB: leak / wrong free size
```

---

## 4. The destructor chain (D1 → base D2)

A complete-object destructor (D1) does its own cleanup, then **calls each base's
base-object destructor (D2)** in reverse order. This is how "destroy the whole
hierarchy" is assembled from per-class pieces.

```
   Derived::~D1:
     1. run ~Derived body
     2. destroy Derived's members (reverse declaration order)
     3. call Base::~D2   (base object destructor — NOT D1, to avoid re-destroying
                          virtual bases twice)
   Base::~D2:
     4. run ~Base body
     5. destroy Base's members (reverse order)
   (virtual bases, if any, destroyed by the MOST-DERIVED D1 at the very end)
```

```
   Note the ABI subtlety: D1 chains to the base's D2 (base-object), not D1. Only
   the most-derived object's D1 is responsible for the shared virtual bases, so
   intermediates must NOT re-destroy them. (Ties back to chapter 08 / chapter 20.)
```

Runnable: [`examples/ch19_dtor_variants.cpp`](examples/ch19_dtor_variants.cpp)
prints the chain order for virtual vs non-virtual bases.

---

## 5. vptr during destruction (mirror of construction)

Just as construction upgrades the vptr level by level (chapter 18 §4),
destruction **downgrades** it. Before running `~Base`'s body, the vptr is reset to
`Base`'s vtable — so a virtual call inside `~Base` resolves to `Base`'s version,
and the (already-destroyed) `Derived` override is never reached.

```
   Destroying a Derived:
     ~Derived body      : vptr = &Derived::vtable   (virtual call -> Derived's)
     [destroy Derived members]
     vptr := &Base::vtable                          <-- DOWNGRADED
     ~Base body         : vptr = &Base::vtable       (virtual call -> Base's)

   This is the machine reason a virtual call in a destructor NEVER dispatches to
   an already-destroyed derived override. Safe, but rarely what people expect ->
   just don't call virtuals in dtors.
```

---

## 6. Optimization: when destructors are trivial or devirtualized

```
   * TRIVIAL destructor: if a class (and all members/bases) has no custom dtor,
     the destructor is trivial -> the compiler emits NOTHING to run (and such
     types are memcpy-able / can skip destruction entirely, e.g. in containers).
   * DEVIRTUALIZED delete: if the exact type is known ('delete static_cast
     <Derived*>(p)' or a local Derived), the compiler calls the D0/D1 directly,
     no vtable lookup.
   * A non-polymorphic class needs NO deleting destructor in a vtable at all
     (it has no vtable). The two-slot cost applies only to virtual dtors.
```

---

## 7. The protected-non-virtual-destructor alternative

If a base is used polymorphically but you *never* `delete` through it (e.g. it's
always owned by value or by the derived type), you can make the destructor
**protected and non-virtual** to *forbid* `delete base_ptr` at compile time —
avoiding the vtable slots while staying safe.

```cpp
class Interface {
public:
    virtual void f() = 0;
protected:
    ~Interface() = default;   // protected + non-virtual: can't 'delete Interface*'
};
```

```
   * protected dtor -> external code cannot 'delete p' (compile error) -> no
     accidental slicing-of-destruction bug.
   * non-virtual -> no D0/D1 vtable slots, marginally smaller vtable.
   Use when the base is a pure interface managed by the owner, never deleted
   polymorphically. (Guideline C.35 in the C++ Core Guidelines.)
```

---

## 8. Summary

```
 +------------------------------------------------------------------+
 | The ABI emits up to 3 destructors per class:                     |
 |   D2 base-object (subobject cleanup),                            |
 |   D1 complete-object (D2 + virtual bases),                       |
 |   D0 deleting (D1 + operator delete).                            |
 | A VIRTUAL dtor occupies TWO vtable slots (D1 & D0).              |
 | 'delete p' with a virtual dtor dispatches through the D0 slot -> |
 |   runs the DERIVED dtor then frees. Without virtual, it calls    |
 |   Base's dtor directly -> Derived never destroyed -> UB/leak.    |
 | D1 chains to each base's D2 (reverse order); most-derived D1     |
 |   handles shared virtual bases.                                  |
 | vptr is downgraded per level during destruction (mirror of ctor).|
 | Trivial dtors emit nothing; known types devirtualize delete.     |
 | Alternative: protected non-virtual dtor forbids delete-via-base. |
 +------------------------------------------------------------------+
```

Next: [20-internals-multiple-inheritance.md](20-internals-multiple-inheritance.md).
