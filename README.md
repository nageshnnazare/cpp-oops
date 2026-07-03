# C++ Object-Oriented Programming — The One-Stop Mastery Guide

> A deep, example-driven, diagram-heavy guide to becoming an **expert** in C++
> OOP. From "what is an object" to vtables, the rule of five, SOLID, design
> patterns, and modern C++ (`override`/`final`, smart pointers, move semantics,
> deducing `this`, the spaceship operator). Every concept is paired with runnable
> code and ASCII diagrams that show what the compiler and memory are *actually*
> doing.

---

## Who this is for

You have **intermediate C++ knowledge** (functions, references, `const`, basic
STL) and you want to *truly* understand object-oriented C++ — not just write
classes, but know **how** they lay out in memory, **why** virtual dispatch works,
**when** to use inheritance vs composition, and **which** design serves you best.

By the end you will be able to:

- Reason about **object layout**, **construction/destruction order**, and
  **vtables** the way a compiler does.
- Write correct **copy/move** semantics (the rule of 0/3/5) every time.
- Use **inheritance**, **polymorphism**, **abstract interfaces**, and even the
  scary **virtual (diamond) inheritance** with confidence.
- Overload operators idiomatically.
- Apply **SOLID** principles and the classic **design patterns** in modern C++.
- Know the modern toolbox: `= default`/`= delete`, `override`/`final`, RAII,
  smart pointers, `std::variant`-based polymorphism, deducing `this`, `<=>`.

---

## How to read this guide

Chapters build on each other; read in order if you're new. Each file is
self-contained and cross-links prerequisites.

| # | File | Topic |
|---|------|-------|
| 00 | [00-mental-model.md](00-mental-model.md) | What OOP *is*: objects, the four pillars, C++'s take |
| 01 | [01-classes-and-objects.md](01-classes-and-objects.md) | Classes, objects, members, `this`, object layout |
| 02 | [02-constructors-destructors.md](02-constructors-destructors.md) | Ctors, dtors, init lists, RAII, order of init |
| 03 | [03-encapsulation.md](03-encapsulation.md) | Access control, invariants, `const`, getters/setters |
| 04 | [04-copy-move-rule-of-five.md](04-copy-move-rule-of-five.md) | Special members, copy/move, rule of 0/3/5 |
| 05 | [05-inheritance.md](05-inheritance.md) | Inheritance, access, construction order, slicing |
| 06 | [06-polymorphism-vtables.md](06-polymorphism-vtables.md) | `virtual`, dynamic dispatch, vtables, `override`/`final` |
| 07 | [07-abstract-classes-interfaces.md](07-abstract-classes-interfaces.md) | Pure virtual, abstract classes, interfaces |
| 08 | [08-multiple-virtual-inheritance.md](08-multiple-virtual-inheritance.md) | Multiple inheritance, the diamond, `virtual` bases |
| 09 | [09-operator-overloading.md](09-operator-overloading.md) | Operator overloading, `<=>`, rules & idioms |
| 10 | [10-composition-vs-inheritance.md](10-composition-vs-inheritance.md) | Relationships: is-a, has-a, composition-first |
| 11 | [11-static-friends-nested.md](11-static-friends-nested.md) | `static` members, `friend`, nested classes |
| 12 | [12-solid-principles.md](12-solid-principles.md) | SOLID with C++ before/after examples |
| 13 | [13-design-patterns.md](13-design-patterns.md) | GoF patterns in modern C++ |
| 14 | [14-modern-cpp-oop.md](14-modern-cpp-oop.md) | RAII, smart pointers, move, `variant`, deducing `this` |
| 15 | [15-pitfalls-and-best-practices.md](15-pitfalls-and-best-practices.md) | Slicing, virtual dtor, self-assign, and more |
| 16 | [16-cheatsheet.md](16-cheatsheet.md) | Quick reference / cheatsheet |

### Expert Internals (the "how it actually works" appendix)

The chapters above make you *fluent*. These make you *dangerous* — the low-level
machinery (ABI, vtables, thunks, unwinding) and expert idioms that separate
senior engineers from the rest. Read them after 00–16.

| # | File | Topic |
|---|------|-------|
| 17 | [17-internals-object-layout.md](17-internals-object-layout.md) | Itanium ABI, standard-layout, EBO, `[[no_unique_address]]`, tail-padding reuse |
| 18 | [18-internals-vtables.md](18-internals-vtables.md) | vtable/vptr anatomy, call lowering to asm, key function & vague linkage, devirtualization |
| 19 | [19-internals-virtual-destructors.md](19-internals-virtual-destructors.md) | D0/D1/D2 destructor variants, how `delete p` works, vtable swapping during construction |
| 20 | [20-internals-multiple-inheritance.md](20-internals-multiple-inheritance.md) | Multiple vptrs, this-adjusting thunks, virtual-base offsets, VTT, construction vtables |
| 21 | [21-internals-rtti-dynamic-cast.md](21-internals-rtti-dynamic-cast.md) | `type_info`, `typeid`, the `dynamic_cast` algorithm (incl. cross-casts) |
| 22 | [22-internals-mangling-ptm.md](22-internals-mangling-ptm.md) | Name mangling, pointer-to-member(-function) representation, linkage |
| 23 | [23-zero-overhead-and-idioms.md](23-zero-overhead-and-idioms.md) | Exception/unwinding & RAII cost model, CRTP, type erasure, SSO, tag dispatch |

Runnable examples live in [`examples/`](examples/). A build helper is provided.
The internals examples (`ch17`–`ch23`) include ABI-poking demos (dumping a live
vtable, observing `this`-pointer adjustment, RTTI, CRTP).

---

## Building the examples

All examples are single-file programs. Compile with a recent compiler:

```bash
clang++ -std=c++20 -Wall -Wextra examples/ch06_vtable.cpp -o /tmp/demo && /tmp/demo

# a couple of examples use C++23 (deducing this) — build those with:
clang++ -std=c++23 -Wall -Wextra examples/ch14_deducing_this.cpp -o /tmp/demo
```

Build everything your compiler supports:

```bash
./examples/build_all.sh
```

---

## The 30-second map of OOP in C++

```
                         OBJECT-ORIENTED C++
                                 |
        +------------------------+----------------------------+
        |            |            |            |              |
   ENCAPSULATION  ABSTRACTION  INHERITANCE  POLYMORPHISM      |
   (hide data,    (model the   (is-a reuse) (one interface,   |
    keep invariants)  essentials)            many forms)      |
        |            |            |            |              |
        +-----+------+     +------+-----+   +--+---------+    |
              |            |            |   |            |    |
        access control  abstract    base/derived  virtual     |
        const, RAII     classes/    ctor order    functions   |
                        interfaces  slicing       vtables     |
                                                              |
        +-----------------------------------------------------+
        |                        SUPPORTED BY
        |   special members (rule of 0/3/5), operator overloading,
        |   composition, static/friend, smart pointers, templates
        v
   DESIGN: SOLID principles + GoF patterns + modern idioms
```

Start with [00-mental-model.md](00-mental-model.md).
