# C++ Object-Oriented Programming — The One-Stop Mastery Guide

> A complete, example-driven guide to object-oriented C++. It covers the
> language rules (not only the syntax), the ways those rules fail, the design
> consequences, and the Itanium ABI underneath GCC and Clang. Every chapter
> has runnable code. The later chapters open the vtable.

---

## Who this is for

You have **intermediate C++ knowledge** (functions, references, `const`, basic
STL) and you want to *truly* understand object-oriented C++ — not just write
classes, but know **how** they lay out in memory, **why** virtual dispatch works,
**when** to use inheritance vs composition, and **which** design serves you best.

By the end you will be able to:

- Reason about **object layout**, **construction/destruction order**, and
  **vtables** the way a compiler does.
- Write correct **copy/move** semantics (the rule of 0/3/5), including when
  the compiler deletes or suppresses a special member, and when `noexcept`
  changes `vector` reallocation.
- Use **inheritance**, **polymorphism**, **abstract interfaces**, and
  **virtual inheritance** with the actual rules: name hiding, slicing,
  covariant returns, static default arguments, NVI, dominance, and who
  initializes a virtual base.
- Overload operators idiomatically, including defaulted and hand-written `<=>`.
- Apply **SOLID** and the classic **design patterns** in modern C++, and know
  which language feature replaces which pattern.
- Use the modern toolbox: `= default`/`= delete`, `override`/`final`, RAII,
  smart pointers (`enable_shared_from_this`, cycles, `make_shared`),
  `std::variant`, type erasure, deducing `this`, `<=>`.
- Read a mangled name, a vtable dump, and a `dynamic_cast` failure without
  treating them as magic.

---

## How to read this guide

Chapters build on each other. Read in order if you are new. Each file stands
on its own once you have the prerequisites it names. Exercises at the ends of
chapters 00–15 have answers directly underneath.

| # | File | Topic |
|---|------|-------|
| 00 | [00-mental-model.md](00-mental-model.md) | Object model, values, four kinds of polymorphism |
| 01 | [01-classes-and-objects.md](01-classes-and-objects.md) | Members, `this`, ref-qualifiers, layout, aggregates |
| 02 | [02-constructors-destructors.md](02-constructors-destructors.md) | Initialization, RAII, failure, order, triviality |
| 03 | [03-encapsulation.md](03-encapsulation.md) | Access, invariants, const, pimpl |
| 04 | [04-copy-move-rule-of-five.md](04-copy-move-rule-of-five.md) | Special members, elision, exception safety |
| 05 | [05-inheritance.md](05-inheritance.md) | Subobjects, access, hiding, slicing |
| 06 | [06-polymorphism-vtables.md](06-polymorphism-vtables.md) | `virtual`, `override`, covariant return, virtual destructor |
| 07 | [07-abstract-classes-interfaces.md](07-abstract-classes-interfaces.md) | Pure virtual, NVI, concepts |
| 08 | [08-multiple-virtual-inheritance.md](08-multiple-virtual-inheritance.md) | Multiple bases, diamonds, dominance |
| 09 | [09-operator-overloading.md](09-operator-overloading.md) | Operators, hidden friends, `<=>` |
| 10 | [10-composition-vs-inheritance.md](10-composition-vs-inheritance.md) | Ownership, LSP test, bridge |
| 11 | [11-static-friends-nested.md](11-static-friends-nested.md) | `static`, init order, `friend`, nested types |
| 12 | [12-solid-principles.md](12-solid-principles.md) | SOLID with the contracts spelled out |
| 13 | [13-design-patterns.md](13-design-patterns.md) | GoF patterns and the modern replacement |
| 14 | [14-modern-cpp-oop.md](14-modern-cpp-oop.md) | Smart pointers, `variant`, type erasure, deducing `this` |
| 15 | [15-pitfalls-and-best-practices.md](15-pitfalls-and-best-practices.md) | The bugs that compile, and the fixes |
| 16 | [16-cheatsheet.md](16-cheatsheet.md) | Dense reference |

### Expert Internals (the "how it actually works" appendix)

The chapters above are the language and the design. These are the ABI GCC and
Clang use on Linux and macOS (Itanium C++ ABI). MSVC differs in representation.
The concepts transfer.

| # | File | Topic |
|---|------|-------|
| 17 | [17-internals-object-layout.md](17-internals-object-layout.md) | Trivial vs standard-layout, EBO, `[[no_unique_address]]`, tail padding |
| 18 | [18-internals-vtables.md](18-internals-vtables.md) | vtable/vptr, call lowering, key function, devirtualization |
| 19 | [19-internals-virtual-destructors.md](19-internals-virtual-destructors.md) | D0/D1/D2, how `delete p` works, vptr during destruction |
| 20 | [20-internals-multiple-inheritance.md](20-internals-multiple-inheritance.md) | Multiple vptrs, thunks, vbase offsets, VTT |
| 21 | [21-internals-rtti-dynamic-cast.md](21-internals-rtti-dynamic-cast.md) | `type_info`, `typeid`, the `dynamic_cast` walk |
| 22 | [22-internals-mangling-ptm.md](22-internals-mangling-ptm.md) | Name mangling, pointers to members |
| 23 | [23-zero-overhead-and-idioms.md](23-zero-overhead-and-idioms.md) | Unwind tables, CRTP, type erasure, SSO |

Runnable examples live in [`examples/`](examples/). `ch00_checks.cpp` is a set
of `static_assert`s for the language rules (aggregates, defaulted `<=>`,
standard-layout, empty-base size). `ch06_traps.cpp` prints the static-default-
argument surprise. `ch02_init.cpp` prints `vector(n, v)` versus `vector{n, v}`.

---

## Building the examples

All examples are single-file programs. Compile with a recent compiler:

```bash
clang++ -std=c++20 -Wall -Wextra -Wnon-virtual-dtor examples/ch06_vtable.cpp -o /tmp/demo && /tmp/demo

# deducing this needs C++23:
clang++ -std=c++23 -Wall -Wextra examples/ch14_deducing_this.cpp -o /tmp/demo
```

Build everything your compiler supports:

```bash
./examples/build_all.sh
./examples/build_all.sh --run
```

---

## The map

```
                         OBJECT-ORIENTED C++
                                 |
        +------------------------+----------------------------+
        |            |            |            |              |
   ENCAPSULATION  ABSTRACTION  INHERITANCE  POLYMORPHISM      |
   (invariants,   (contracts,  (subobjects, (virtual, variant,|
    access)        NVI)         substitutable) templates,     |
                                              type erasure)   |
        +-----------------------------------------------------+
        |  special members, operators, composition, RAII,
        |  smart pointers, SOLID, patterns
        v
   ABI: layout, vtables, thunks, RTTI, mangling, unwind tables
```

Start with [00-mental-model.md](00-mental-model.md).
The one-page recall is [16-cheatsheet.md](16-cheatsheet.md).
