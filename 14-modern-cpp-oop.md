# 14 — Modern C++ OOP (RAII, Smart Pointers, and Beyond)

Modern C++ (11 through 26) reshaped how idiomatic OOP is written: automatic
resource management with smart pointers, move semantics, closed-set polymorphism
with `std::variant`, and syntax sugar like deducing `this` and the spaceship
operator. This chapter is the "how OOP is actually written today" chapter.

Prereqs: chapters 04, 06, 09, 10.

---

## 1. Smart pointers: ownership made explicit (never `new`/`delete`)

Raw owning pointers are a bug magnet (leaks, double-free, dangling). Smart
pointers encode ownership in the type and clean up via RAII (chapter 02).

```cpp
#include <memory>

std::unique_ptr<Widget> a = std::make_unique<Widget>(args);   // sole owner
std::shared_ptr<Widget> b = std::make_shared<Widget>(args);   // shared owner
std::weak_ptr<Widget>   w = b;                                 // non-owning observer
```

```
   unique_ptr<T> : EXCLUSIVE ownership. Move-only. Zero overhead vs raw ptr.
                   Frees automatically when it goes out of scope. THE default.
   shared_ptr<T> : SHARED ownership via an atomic reference count. Frees when
                   the LAST shared_ptr dies. Heavier (control block, atomics).
   weak_ptr<T>   : observes a shared_ptr WITHOUT keeping it alive; breaks cycles.
                   .lock() -> shared_ptr if still alive, else null.
```

```
   unique_ptr:                        shared_ptr:
     up ──owns──► [Widget]             sp1 ─┐
     (dies -> Widget deleted)          sp2 ─┼─► [ctrl: refcount=3] ─► [Widget]
                                       sp3 ─┘   (deleted when count hits 0)
                                       wp ····► (observes; doesn't count)
```

### Ownership in interfaces (rules of thumb)

```
   Function takes...            Meaning
   -------------------------    ------------------------------------------
   unique_ptr<T> (by value)     "I take ownership" (sink)
   shared_ptr<T> (by value)     "I share ownership" (keep it alive)
   T*  or  T&                   "I just USE it, no ownership" (non-owning)
   const T&                     read-only use, no ownership

   Return unique_ptr<T> from factories (transfers ownership to the caller).
   Prefer make_unique/make_shared (exception-safe, single allocation for shared).
```

Runnable: [`examples/ch14_smart_pointers.cpp`](examples/ch14_smart_pointers.cpp).

---

## 2. Polymorphism with smart pointers (the modern base-collection)

```cpp
std::vector<std::unique_ptr<Shape>> shapes;
shapes.push_back(std::make_unique<Circle>(2.0));
shapes.push_back(std::make_unique<Square>(3.0));
for (const auto& s : shapes)
    std::cout << s->area() << '\n';      // virtual dispatch; no leaks; no slicing
// vector's destructor deletes every Shape automatically.
```

```
   This replaces the old  std::vector<Shape*>  + manual new/delete loop.
   No slicing (we store pointers), no leaks (unique_ptr frees), exception-safe.
   Requires Shape to have a virtual destructor (chapter 06 §7).
```

---

## 3. Move semantics in class design (recap + practice)

```
   * Make move ctor/assign 'noexcept' -> containers move (not copy) on realloc.
   * Rule of Zero: prefer members that already move well (string, vector, unique_ptr).
   * Return big objects BY VALUE — move/RVO makes it cheap (no out-parameters).
   * Accept sink parameters by value and std::move into place:
       Widget(std::string name) : name_(std::move(name)) {}
```

```
   Old style (out-param, ugly):        Modern (return by value, cheap):
     void make(BigThing& out);           BigThing make();
     BigThing b; make(b);                auto b = make();   // move/RVO, no copy
```

---

## 4. Closed-set polymorphism: `std::variant` + `std::visit`

For a **fixed, known** set of types, `std::variant` gives type-safe polymorphism
with **value semantics** — no inheritance, no heap, no vtable.

```cpp
#include <variant>

struct Circle { double r; double area() const { return 3.14159*r*r; } };
struct Square { double s; double area() const { return s*s; } };
using Shape = std::variant<Circle, Square>;

double area(const Shape& sh) {
    return std::visit([](const auto& x){ return x.area(); }, sh);  // dispatch
}

std::vector<Shape> shapes{ Circle{2}, Square{3} };   // stored BY VALUE, contiguous
```

```
   virtual/inheritance             std::variant + visit
   ---------------------------     -----------------------------------
   OPEN set (add types anywhere)   CLOSED set (all types known here)
   heap + base pointers            value semantics, cache-friendly, no heap
   vtable indirect call            visit (often devirtualized/inlined)
   new subtype = new class         new type = edit the variant + visitors

   Use variant for a small FIXED set (AST nodes, states, results). Use
   inheritance for an OPEN, extensible set (plugins).
```

Runnable: [`examples/ch14_variant.cpp`](examples/ch14_variant.cpp).

---

## 5. Type erasure: runtime polymorphism WITHOUT inheritance

Sometimes you want polymorphism but don't want callers to inherit from your base
(non-intrusive). Type erasure (`std::function` is a famous example) wraps any
conforming type behind a value interface. (Full mechanics are in the
`cpp-templates` guide, chapter 10; here's the OOP framing.)

```cpp
// std::function IS type erasure over "callable":
std::function<int(int)> f = [](int x){ return x + 1; };   // holds a lambda
f = &some_free_function;                                    // ...or a function ptr
f = SomeFunctor{};                                          // ...or a functor
f(41);   // 42 — dispatches to whatever it holds, no common base class required
```

```
   Type erasure: one value type (std::function / std::any / a custom wrapper)
   stores ANY type that satisfies a duck-typed interface, dispatching via a
   hidden virtual under the hood. Non-intrusive: stored types need NO base class.
```

---

## 6. `override`, `final`, `= default`, `= delete` (write intent)

Modern class declarations state intent explicitly (chapters 04, 06):

```cpp
class Base {
public:
    virtual void f();
    virtual ~Base() = default;             // virtual dtor, defaulted
};
class Derived final : public Base {         // no further subclassing
public:
    void f() override;                      // compiler-checked override
    Derived(const Derived&) = delete;       // non-copyable
    Derived(Derived&&) noexcept = default;  // movable
};
```

```
   override  -> catches signature mistakes at compile time.
   final     -> locks classes/methods; enables devirtualization.
   = default -> "generate the standard version" (keeps it trivial where possible).
   = delete  -> forbid an operation (compile error on use).
   Together they make the class's contract explicit and self-documenting.
```

---

## 7. Deducing `this` (C++23) — deduplicate cv/ref overloads

An explicit object parameter lets one member serve `&`, `const&`, `&&`, and even
acts as a clean CRTP replacement (see the `cpp-templates` guide chapter 14).

```cpp
class Widget {
    std::string data_;
public:
    // one function replaces const/non-const/rvalue getter overloads:
    template <class Self>
    auto&& data(this Self&& self) {
        return std::forward<Self>(self).data_;   // returns matching ref-ness
    }
};
```

```
   w.data()             -> std::string&
   const_w.data()       -> const std::string&
   std::move(w).data()  -> std::string&&
   ONE definition. Also enables recursive lambdas and simplifies CRTP-style
   static polymorphism. (Needs GCC 14+/Clang 18+, -std=c++23.)
```

Runnable: [`examples/ch14_deducing_this.cpp`](examples/ch14_deducing_this.cpp).

---

## 8. Three-way comparison `<=>` (C++20) in class design

```cpp
#include <compare>
struct Point {
    int x, y;
    auto operator<=>(const Point&) const = default;  // all six comparisons
    bool operator==(const Point&) const = default;
};
// Point is now usable in std::set, std::map keys, sorting, ==, etc. for free.
```

```
   One defaulted <=> (+ ==) replaces six hand-written operators (chapter 09).
   Makes value types "regular" (comparable) with almost no boilerplate.
```

---

## 9. Putting it together: a modern shape system

```cpp
#include <memory>
#include <vector>
#include <iostream>

class Shape {
public:
    virtual double area() const = 0;
    virtual ~Shape() = default;                 // virtual dtor
};

class Circle final : public Shape {             // final: not a base
    double r_;
public:
    explicit Circle(double r) : r_(r) {}
    double area() const override { return 3.14159265 * r_ * r_; }
};

class Rectangle final : public Shape {
    double w_, h_;
public:
    Rectangle(double w, double h) : w_(w), h_(h) {}
    double area() const override { return w_ * h_; }
};

// factory returns owning unique_ptr:
std::unique_ptr<Shape> make_circle(double r) { return std::make_unique<Circle>(r); }

int main() {
    std::vector<std::unique_ptr<Shape>> shapes;
    shapes.push_back(make_circle(2.0));
    shapes.push_back(std::make_unique<Rectangle>(3.0, 4.0));

    double total = 0;
    for (const auto& s : shapes) total += s->area();
    std::cout << "total = " << total << '\n';    // 24.5664
    // no delete needed — unique_ptr + vector clean up automatically.
}
```

Runnable: [`examples/ch14_modern_shapes.cpp`](examples/ch14_modern_shapes.cpp).

---

## 10. Summary

<!--diagram
title: Modern C++ OOP
box[green] Key points
  text: **Smart pointers**: `unique_ptr` (sole owner, default), `shared_ptr` (shared, refcounted), `weak_ptr` (observer, breaks cycles). Use `make_unique`/`make_shared`; never raw `new`/`delete`. Encode ownership in signatures
  text: Store polymorphic objects as `vector<unique_ptr<Base>>` (no slicing/leaks; needs virtual dtor)
  text: **Move**: `noexcept` moves, return by value, sink-by-value+`std::move`
  text: **Closed** set → `std::variant` + `std::visit` (value semantics, no heap). **Open** set → virtual. Non-intrusive → type erasure
  text: `override`/`final`/`=default`/`=delete` state intent. C++23 deducing `this` & C++20 `<=>` cut boilerplate
-->
```
 +-------------------------------------------------------------------+
 | SMART POINTERS: unique_ptr (sole owner, default), shared_ptr      |
 |   (shared, refcounted), weak_ptr (observer, breaks cycles).       |
 |   Use make_unique/make_shared; never raw new/delete. Encode       |
 |   ownership in signatures.                                        |
 | Store polymorphic objects as vector<unique_ptr<Base>> (no slicing |
 |   /leaks; needs virtual dtor).                                    |
 | MOVE: noexcept moves, return by value, sink-by-value+std::move.   |
 | CLOSED set -> std::variant + std::visit (value semantics, no      |
 |   heap). OPEN set -> virtual. Non-intrusive -> type erasure.      |
 | override/final/=default/=delete state intent. C++23 deducing      |
 |   'this' & C++20 <=> cut boilerplate.                             |
 +-------------------------------------------------------------------+
```

Next: [15-pitfalls-and-best-practices.md](15-pitfalls-and-best-practices.md).
