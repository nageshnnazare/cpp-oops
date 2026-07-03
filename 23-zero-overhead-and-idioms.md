# 23 — Zero-Overhead Abstractions & Expert Idioms

The final internals chapter zooms back out. Now that you know *how* the machinery
works, this chapter is about **using it like an expert**: the "zero-overhead
principle," the real cost model of exceptions/RAII, and the idioms that give you
polymorphism *without* the vtable — **CRTP**, **type erasure**, **SSO**, **tag
dispatch**, and knowing when abstraction is genuinely free.

Prereqs: chapters 14, 17–22.

---

## 1. The zero-overhead principle (Stroustrup's law)

> "What you don't use, you don't pay for. And what you do use, you couldn't hand-
> code any better."

```
   * A class with no virtuals has NO vptr and NO vtable — same layout/cost as a C
     struct (chapter 17).
   * A non-virtual member call is a plain direct call — identical to a free
     function taking the object as the first arg.
   * RAII destructors that are trivial emit no code.
   * Templates monomorphize -> a std::sort<int> is as fast as a hand-written int
     sort; no runtime dispatch (see the cpp-templates guide).

   The COST you pay for OOP is exactly the feature you asked for:
     virtual  -> vptr + indirect call     (chapter 18)
     RTTI     -> type_info + dynamic_cast  (chapter 21)
     exceptions -> unwinding tables (but zero cost on the happy path — §2)
```

---

## 2. Exceptions, stack unwinding & RAII — the real cost model

RAII (chapter 02) relies on destructors running during **stack unwinding** when
an exception propagates. Modern ABIs implement this with the **"zero-cost"
(table-driven)** model.

<!--diagram
title: Zero-cost exception model
box[green] Happy path (no throw)
  text: **no extra instructions** — no setup, no checks; normal code speed
box[red] Throw path (rare)
  text: the runtime consults side tables to find handlers & run destructors (slow, but rare)
-->
```
   "Zero-cost" = zero cost WHEN NOTHING THROWS:
     * The compiler emits read-only side tables (.eh_frame / .gcc_except_table)
       mapping each PC range -> which destructors to run ("cleanup landing pads")
       and which catch handlers apply.
     * On the normal path: NO try/finally instructions, NO flag checks. Free.
     * On throw: libunwind walks the stack frame by frame, consulting the tables,
       running each frame's destructors (RAII cleanup) until a matching catch.

   Trade-off: throwing is EXPENSIVE (table lookup + unwind); not throwing is FREE.
   => Use exceptions for EXCEPTIONAL cases, not control flow.
```

```
   noexcept — a correctness AND performance tool:
     * Promises a function won't throw -> the compiler can OMIT unwinding setup
       around calls to it and enables optimizations (e.g. vector move, ch 04).
     * If a noexcept function DOES throw -> std::terminate (no unwinding).
     * Move ctors/dtors should be noexcept so containers move instead of copy.

   Destructors are IMPLICITLY noexcept. Throwing from a destructor during
   unwinding -> std::terminate. Never let a destructor throw.
```

Runnable (RAII during unwinding): [`examples/ch02_raii.cpp`](examples/ch02_raii.cpp).

---

## 3. CRTP — static polymorphism (virtual dispatch, zero runtime cost)

The **Curiously Recurring Template Pattern**: a base templated on its derived
type. It gives you the "call derived behavior from base code" superpower —
resolved entirely at **compile time**, so no vptr, no indirect call, fully
inlinable. (Deep-dive lives in the `cpp-templates` guide; here's the OOP framing.)

```cpp
template <class Derived>
struct Shape {                         // CRTP base
    double area() const {
        return static_cast<const Derived*>(this)->area_impl();  // static dispatch
    }
    void describe() const {
        std::cout << "area = " << area() << '\n';   // base code calls derived impl
    }
};

struct Circle : Shape<Circle> {        // note: inherits Shape<Circle>
    double r;
    explicit Circle(double r) : r(r) {}
    double area_impl() const { return 3.14159265 * r * r; }
};
```

<!--diagram
title: Runtime polymorphism vs CRTP (static)
box[blue] virtual (runtime)
  text: vptr per object; indirect call; OPEN set; heterogeneous containers OK
box[green] CRTP (compile-time)
  text: **no vptr**, direct inlinable call; closed/known type; no base-pointer container
-->
```
   virtual:   Shape* s;  s->area();   // vtable lookup, not inlined, works in a
                                      //   vector<Shape*> of mixed types
   CRTP:      Circle c;  c.area();    // static_cast + direct call -> INLINED to
                                      //   3.14*r*r; zero overhead; but you CANNOT
                                      //   store Circle and Square in one container
                                      //   as "Shape" (different base types)
```

```
   Use CRTP when: you want interface reuse / policy injection with ZERO runtime
   cost and you DON'T need a heterogeneous runtime collection. Common for
   mixins, static interfaces, and expression templates.
   NOTE: C++23 "deducing this" (chapter 14) replaces many CRTP uses more cleanly.
```

Runnable: [`examples/ch23_crtp.cpp`](examples/ch23_crtp.cpp).

---

## 4. Type erasure — runtime polymorphism WITHOUT inheritance

Type erasure gives you a *value* that can hold **any** type satisfying an
interface, with virtual dispatch under the hood but **no base class required** by
the stored types (non-intrusive). `std::function`, `std::any`, and
`std::shared_ptr`'s deleter all use it.

```cpp
class Drawable {                       // a type-erasing wrapper (value semantics)
    struct Concept {                   // internal interface (the "erased" API)
        virtual void draw() const = 0;
        virtual std::unique_ptr<Concept> clone() const = 0;
        virtual ~Concept() = default;
    };
    template <class T>
    struct Model : Concept {           // adapts ANY T with a draw() free/method
        T obj;
        explicit Model(T o) : obj(std::move(o)) {}
        void draw() const override { obj.draw(); }                 // duck-typed
        std::unique_ptr<Concept> clone() const override {
            return std::make_unique<Model>(*this);
        }
    };
    std::unique_ptr<Concept> self_;    // the hidden vtable lives here
public:
    template <class T>
    Drawable(T obj) : self_(std::make_unique<Model<T>>(std::move(obj))) {}
    Drawable(const Drawable& o) : self_(o.self_->clone()) {}       // value copy
    void draw() const { self_->draw(); }
};

struct Button { void draw() const { /*...*/ } };   // NO base class needed!
struct Icon   { void draw() const { /*...*/ } };

std::vector<Drawable> ui{ Button{}, Icon{} };      // heterogeneous, value-based
```

<!--diagram
title: Type erasure (the Concept/Model sandwich)
box[blue] Drawable (public value type)
  text: copyable value with `.draw()`; users store it in containers by value
  box[orange] Concept (private abstract interface)
    text: the erased virtual API: `draw()`, `clone()`, virtual dtor
    box[green] Model<T> (private template)
      text: wraps a concrete T and forwards `draw()` to it — the ONLY place T is known
-->
```
   Stored types (Button, Icon) need NO common base — they just need a draw().
   The wrapper synthesizes a vtable via Concept/Model. Benefits:
     * value semantics (copy, store in vector<Drawable>) unlike raw base pointers
     * non-intrusive: works with types you can't modify (int, third-party)
   Cost: a heap allocation + an indirect call (like virtual) — but flexible.
   This is EXACTLY how std::function<Sig> stores any callable. (Full mechanics:
   cpp-templates guide, chapter 10.)
```

---

## 5. Small-object / Small-buffer optimization (SSO/SBO)

Experts know that `std::string` (and often `std::function`) avoids heap
allocation for small values by storing them **inline** in the object itself.

```
   std::string layout (typical libstdc++/libc++, ~24-32 bytes):
     small string ("hi"):   [ len | 'h''i''\0'... stored INLINE | cap ]  no heap!
     large string:          [ ptr -> heap buffer | len | cap ]           heap

   The object reserves a small inline buffer; if the value fits, it lives there
   (no malloc). This is why copying short strings is nearly free and cache-
   friendly. std::function does the same for small callables (SBO).
```

```
   Design lesson: for your own wrappers/handles, consider a small-buffer
   optimization when values are usually tiny -> dodge the allocator on the hot
   path. (This is a big reason std::string beats naive char* management.)
```

---

## 6. Tag dispatch & `if constexpr` — compile-time branching

Selecting behavior at compile time (no runtime branch, no virtual) via
overloading on tag types — the classic pre-concepts technique still worth knowing:

```cpp
// pick an algorithm based on the iterator category, at COMPILE time:
template <class It>
void advance_impl(It& it, int n, std::random_access_iterator_tag) { it += n; }
template <class It>
void advance_impl(It& it, int n, std::input_iterator_tag) { while (n--) ++it; }

template <class It>
void my_advance(It& it, int n) {
    advance_impl(it, n, typename std::iterator_traits<It>::iterator_category{});
}
```

```
   Tag dispatch: overload resolution picks the right impl from a TAG type — zero
   runtime cost, chosen by the type system. Modern C++ often replaces it with
   'if constexpr' or concepts (see cpp-templates guide), but you'll see tag
   dispatch throughout the STL internals.
```

---

## 7. Choosing your polymorphism (the expert decision table)

```
   +--------------------+-----------+----------+-----------+------------------+
   | Technique          | dispatch  | per-obj  | container | when             |
   |                    | cost      | overhead | of mixed  |                  |
   |--------------------+-----------+----------+-----------+------------------|
   | virtual functions  | indirect  | 1 vptr   | yes       | OPEN set, need   |
   |                    | call      |          | (via ptr) | runtime extension|
   | CRTP               | none      | none     | no        | static reuse,    |
   |                    | (inlined) |          |           | zero overhead    |
   | std::variant+visit | ~switch   | tag byte | yes       | CLOSED set, value|
   |                    |           |          | (value)   | semantics        |
   | type erasure       | indirect  | ptr+heap | yes       | non-intrusive,   |
   |                    | call      |          | (value)   | value semantics  |
   | templates/overload | none      | none     | no        | full compile-time|
   +--------------------+-----------+----------+-----------+------------------+
```

```
   The expert instinct: reach for the LEAST powerful tool that solves the problem.
   Free-function/template < CRTP < variant < virtual < type erasure < dynamic_cast,
   roughly in order of increasing runtime machinery. Don't pay for dynamism you
   don't need — but don't contort code to avoid a cheap virtual, either.
```

---

## 8. Final expert mental model

```
 +-------------------------------------------------------------------+
 | Zero-overhead: you pay ONLY for the dynamism you use. Non-virtual |
 |   classes cost like C structs; virtual adds a vptr + indirect     |
 |   call; RTTI adds type_info; exceptions are free until thrown.    |
 | noexcept + trivial dtors + devirtualization make abstractions     |
 |   genuinely free in the common case.                              |
 | Polymorphism without vtables:                                     |
 |   CRTP        -> compile-time, inlined, no heterogeneous container|
 |   variant     -> closed set, value semantics                      |
 |   type erasure-> open set, value semantics, non-intrusive (heap)  |
 | SSO/SBO stores small values inline (no malloc). Tag dispatch /    |
 |   if constexpr branch at compile time.                            |
 | Pick the least powerful tool that works.                          |
 +-------------------------------------------------------------------+
```

You've now seen C++ OOP from the four pillars down to thunks, VTTs, and unwind
tables. Back to the [README](README.md) or the [cheatsheet](16-cheatsheet.md).
