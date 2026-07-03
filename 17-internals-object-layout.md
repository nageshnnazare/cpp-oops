# 17 — Internals: Object Layout & the Itanium C++ ABI

Time to go under the hood. This chapter explains **exactly** how C++ lays objects
out in memory — the rules the compiler follows, the **Itanium C++ ABI** (used by
GCC/Clang on Linux/macOS; MSVC differs in details but the concepts transfer), and
the optimizations experts exploit: **standard-layout**, **empty base
optimization (EBO)**, `[[no_unique_address]]`, and **tail-padding reuse**.

Prereq: [01-classes-and-objects.md](01-classes-and-objects.md), [05-inheritance.md](05-inheritance.md).

> Most of this is *implementation-defined* by the language but *nailed down* by
> the platform ABI. That's why two compilers targeting the same ABI produce
> layout-compatible objects — it's a contract, not a language guarantee.

---

## 1. The three layout categories

The standard classifies class types by how constrained their layout is:

<!--diagram
title: Class layout categories
box[blue] Trivial
  text: trivial ctors/dtor/copy; memcpy-able; no user-provided special members
box[green] Standard-layout
  text: C-compatible layout; one access control; no virtuals; no mixed-access data
box[orange] POD (Plain Old Data)
  text: trivial **AND** standard-layout (the old C-struct-like guarantee)
-->
```
   +-----------------------------------------------------------------+
   | TRIVIAL          : can be copied with memcpy; no custom ctor/   |
   |                    dtor/copy; trivially default-constructible.  |
   |-----------------------------------------------------------------|
   | STANDARD-LAYOUT  : layout matches an equivalent C struct.       |
   |   * no virtual functions or virtual bases                       |
   |   * all non-static data members have the SAME access control    |
   |   * no non-standard-layout bases; at most one class in the      |
   |     hierarchy has data members                                  |
   |   -> offsetof() is legal; safe to share with C; reinterpret ok  |
   |-----------------------------------------------------------------|
   | POD = TRIVIAL and STANDARD-LAYOUT (the classic "C struct").     |
   +-----------------------------------------------------------------+
```

```cpp
#include <type_traits>
struct A { int x; double y; };            // trivial + standard-layout (POD-ish)
struct B { private: int a; public: int b; };  // NOT standard-layout (mixed access)
struct C { virtual ~C(); };               // NOT standard-layout (has vtable)

static_assert(std::is_standard_layout_v<A>);
static_assert(std::is_trivial_v<A>);
static_assert(!std::is_standard_layout_v<B>);
static_assert(!std::is_standard_layout_v<C>);
```

Why you care: only **standard-layout** types give you `offsetof`, guaranteed
"first member at offset 0", and safe interop with C / `memcpy` / serialization.

---

## 2. Alignment, size, and padding (the exact rules)

Every type has a **size** (`sizeof`) and an **alignment** (`alignof`). The
compiler places each member at an offset that is a multiple of its alignment,
inserting **padding** as needed, and rounds the total size up to a multiple of
the class's alignment (so arrays stay aligned).

```cpp
struct S {
    char  c;      // alignof 1
    double d;     // alignof 8
    int   i;      // alignof 4
};
```

```
   Alignment of S = max(alignof members) = 8. Offsets computed left-to-right:

   offset 0        1 .. 7            8 ............ 15   16 .. 19   20 .. 23
          +--------+-----------------+------------------+----------+----------+
          |  c(1)  | padding (7)     |     d (8)        |  i (4)   | pad (4)  |
          +--------+-----------------+------------------+----------+----------+
   * c at 0. d needs offset %8 -> jump to 8 (7 bytes pad).
   * i at 16. size so far 20, but alignof(S)=8 -> round up to 24 (4 tail pad).
   sizeof(S) == 24, alignof(S) == 8.
```

```
   REORDER members largest-alignment-first to shrink padding:
     struct S2 { double d; int i; char c; };  // 8 + 4 + 1 + 3pad = 16 (not 24!)
   Data members are laid out in DECLARATION order (the ABI forbids reordering),
   so the ORDER YOU WRITE is the order in memory. This is a real tuning knob.
```

Runnable: [`examples/ch17_ebo.cpp`](examples/ch17_ebo.cpp) prints these sizes.

---

## 3. Where the vptr goes (Itanium ABI)

For a polymorphic class, the Itanium ABI places the **vptr at offset 0** (before
the data members). This is why a `Derived*` → `Base*` upcast to the *primary*
base needs **no pointer adjustment** (the vptr and primary-base subobject share
offset 0).

```
   struct Poly { int x; virtual void f(); };   // 64-bit

   offset 0 ................ 7   8 .. 11   12 .. 15
          +--------------------+----------+----------+
          |  vptr (8 bytes)    |  x (4)   | pad (4)  |
          +--------------------+----------+----------+
   sizeof(Poly) == 16, alignof == 8. The vptr is an INVISIBLE first member.
```

```
   Consequence: a "just add a virtual function" change grows every object by one
   pointer AND makes it non-standard-layout (offsetof no longer valid, not
   memcpy-safe). Don't add virtuals to a type you memcpy or share with C.
```

---

## 4. Empty Base Optimization (EBO)

An object must have `sizeof >= 1` so distinct objects have distinct addresses.
But an **empty base subobject** is allowed to occupy **zero** bytes — the ABI
lets the derived class reuse that space. This is **EBO**, and it's why
`std::vector`, `std::unique_ptr`, etc., can carry stateless allocators/deleters
for *free*.

```cpp
struct Empty {};                       // sizeof(Empty) == 1 (as a standalone object)

struct AsMember { Empty e; int x; };   // e still needs an address -> 1 + pad + 4
struct AsBase : Empty { int x; };      // EBO: Empty contributes 0 bytes -> just 4
```

<!--diagram
title: Empty Base Optimization
box[red] As a MEMBER (no EBO)
  text: `struct { Empty e; int x; }`  ->  **sizeof 8** (e occupies a byte + pad)
box[green] As a BASE (EBO applies)
  text: `struct : Empty { int x; }`  ->  **sizeof 4** (Empty takes 0 bytes)
-->
```
   struct AsMember { Empty e; int x; }        struct AsBase : Empty { int x; }
   +--------+--------+----------------+        +----------------+
   | e (1)  | pad(3) |    x (4)       |        |     x (4)      |
   +--------+--------+----------------+        +----------------+
   sizeof == 8  (empty member needs an addr)   sizeof == 4  (EBO!)
```

```
   Rule: an empty BASE can overlap the derived object (0 size) as long as it
   doesn't collide with another subobject of the SAME TYPE (which would break
   "distinct objects of the same type have distinct addresses").
   This is THE reason libraries put stateless policies (Deleter, Compare,
   Allocator) in a BASE (or in a compressed pair), not a member.
```

---

## 5. `[[no_unique_address]]` (C++20) — EBO for members

C++20 lets you get the EBO benefit *without* inheritance, by marking a
(potentially empty) member with `[[no_unique_address]]`. The compiler may then
overlay it with other members.

```cpp
struct Empty {};

struct Widget {
    [[no_unique_address]] Empty e;   // may occupy 0 bytes
    int x;
};
static_assert(sizeof(Widget) == sizeof(int));   // e folded away
```

```
   [[no_unique_address]] : "this member need not have a unique address; if it's
   empty, reuse the space." Modern replacement for the EBO-via-base trick.
   Two [[no_unique_address]] members of the SAME empty type still need distinct
   addresses (so they can't BOTH be zero-sized simultaneously).
```

This is how modern standard libraries implement things like `std::ranges` view
adaptors and stateless comparators with zero overhead.

---

## 6. Tail-padding reuse (a surprising layout trick)

The padding at the *end* of a base subobject can be **reused** to store the
derived class's members — but *only* for non-standard-layout types. This means a
base subobject inside a derived object can be *smaller* than the base as a
standalone object.

```cpp
struct Base {                  // non-standard-layout (mixed access) so reuse is allowed
    int  i;                    // offset 0..3
    char c;                    // offset 4        -> tail padding 5,6,7
private:
    int secret;                // makes it non-standard-layout
};                             // sizeof(Base) == 12 (with tail padding)

struct Derived : Base {
    char d;                    // may sit in Base's TAIL PADDING (offset 5)!
};
```

```
   Base standalone:   [ i(4) | c(1) | pad(3) | secret? ... ]  sizeof includes pad
   Derived:           [ ...Base part... d ]  <- 'd' tucked into Base's tail pad
   -> sizeof(Derived) can EQUAL sizeof(Base) (no growth), reusing the padding.

   CATCH: this is why memcpy'ing a BASE subobject inside a derived object is
   dangerous — you might clobber the derived member living in the tail padding.
   Standard-layout types do NOT reuse tail padding (safe for offsetof/C interop).
```

This is the deep reason the standard says: don't `memcpy` into a base subobject;
use assignment. It's also why `std::is_standard_layout` matters for serialization.

---

## 7. Reference & bitfield layout notes

```
   * References are NOT objects; a T& member is typically stored as a pointer
     (implementation detail) and makes the class non-standard-layout-ish for
     copy purposes (reference members can't be reseated -> no copy assignment).
   * Bitfields pack multiple fields into one storage unit:
       struct Flags { unsigned a:1, b:1, c:6; };  // often 1 byte total
     Bitfield layout (order within the unit, straddling) is largely
     implementation/ABI-defined — don't rely on exact bit positions across
     compilers.
   * alignas(N) raises alignment: 'struct alignas(64) CacheLine {...};' pads to
     64-byte alignment (used to avoid false sharing — see the cpp-atomics guide).
```

---

## 8. Inspecting layout yourself

```
   sizeof(T)            : total bytes (incl. padding & vptr)
   alignof(T)           : required alignment
   offsetof(T, member)  : byte offset (STANDARD-LAYOUT types only!)
   __builtin_offsetof   : compiler builtin behind offsetof

   Clang/GCC dumps (invaluable for real work):
     clang++ -Xclang -fdump-record-layouts -c foo.cpp   # exact field offsets, vptr
     clang++ -Xclang -fdump-vtable-layouts -c foo.cpp    # vtable contents (ch 18)
     g++ -fdump-lang-class foo.cpp                        # GCC class/vtable dump
```

```
   *** RUN THIS ***  the record-layout dump prints something like:
     0 | struct Poly
     0 |   (Poly vtable pointer)
     8 |   int x
       | [sizeof=16, dsize=12, align=8, nvsize=12, nvalign=8]
   'dsize' = data size (without tail padding) — the value used for tail-padding
   reuse in derived classes. This is the ground truth for layout questions.
```

Runnable: [`examples/ch17_ebo.cpp`](examples/ch17_ebo.cpp).

---

## 9. Summary

```
 +------------------------------------------------------------------+
 | Layout categories: trivial (memcpy-able), standard-layout (C     |
 |   compatible, offsetof-legal), POD = both.                       |
 | Members laid out in DECLARATION order; padding aligns each; size |
 |   rounds up to alignof. Reorder big->small to cut padding.       |
 | Itanium ABI: vptr at offset 0; adding a virtual grows the object |
 |   by a pointer AND breaks standard-layout.                       |
 | EBO: empty BASE subobjects take 0 bytes (libraries exploit this).|
 | C++20 [[no_unique_address]]: EBO for MEMBERS.                    |
 | Tail-padding reuse: derived members may live in a base's trailing|
 |   padding (non-standard-layout only) -> don't memcpy base parts. |
 | Inspect with sizeof/alignof/offsetof and -fdump-record-layouts.  |
 +------------------------------------------------------------------+
```

Next: [18-internals-vtables.md](18-internals-vtables.md).
