# 14 — Modern C++ OOP

"Modern" here means C++11 through C++23 as they changed class design:
ownership in the type system, move semantics, closed-set polymorphism, and a
few language features that delete boilerplate (`override`, `= default`,
deducing `this`, `<=>`). The mechanics of copy and move are chapter 04. The
mechanics of `<=>` are chapter 09. This chapter is how those pieces sit in
an object-oriented design.

Prereqs: chapters 04, 06, 09, 10.

---

## 1. Ownership as a type

A raw owning pointer does not say who deletes it. The type system cannot
help. Smart pointers put the ownership policy in the type and the deletion
in a destructor, which is RAII (chapter 02).

```cpp
#include <memory>

auto a = std::make_unique<Widget>(args);    // sole owner
auto b = std::make_shared<Widget>(args);    // shared owner
std::weak_ptr<Widget> w = b;                // observes b, does not own
```

```
   unique_ptr<T>     exclusive owner. Move-only. Size of a pointer when the
                     deleter is stateless (empty-base / no_unique_address).
                     Destruction is a direct delete. This is the default.

   shared_ptr<T>     shared owner. The last shared_ptr destroys the object.
                     A control block holds the refcount. Copies bump an
                     atomic counter. Larger and slower than unique_ptr.

   weak_ptr<T>       non-owning. lock() returns a shared_ptr if the object
                     is still alive, otherwise an empty shared_ptr.
                     Does not keep the object alive. Breaks cycles.
```

```
   unique_ptr:   up ──owns──► [Widget]

   shared_ptr:   sp1 ─┐
                 sp2 ─┼─► [control block: use_count, weak_count] ─► [Widget]
                 sp3 ─┘
                 wp  ·····► the same control block, does not increment use_count
```

### Signatures

```
   parameter                              meaning
   -------------------------------------  ---------------------------------
   unique_ptr<T> by value                 "I take ownership" (a sink)
   unique_ptr<T>&                         "I may reseat your pointer"; rare
   shared_ptr<T> by value                 "I will keep the object alive"
   shared_ptr<T> const&                   "I may copy the shared_ptr or not"
   T& or T*                               "I do not own this; you keep it alive"
   const T&                               observe, do not own, do not mutate
```

Return `unique_ptr<T>` from a factory. Return `shared_ptr<T>` only when the
factory is handing out shared ownership, not as a default. Accept `T&` when
the function merely uses the object. A function that accepts `shared_ptr`
by value forces every caller into shared ownership, including the ones that
had a `unique_ptr` and now have to convert and allocate a control block.

`unique_ptr<T>` converts to `shared_ptr<T>` (`shared_ptr<T> s = std::move(u)`).
The other direction does not exist: you cannot steal unique ownership out of
a shared group.

### `make_unique` and `make_shared`

```cpp
auto p = std::make_unique<Widget>(a, b);     // one allocation, exception-safe
auto q = std::make_shared<Widget>(a, b);     // object and control block together
```

`make_unique` is exception-safe in a way that matters when a function call
has two `new` expressions and one of them throws before both raw pointers are
owned. `make_shared` performs one allocation for the object and the control
block. The tradeoff: memory for the object is not released until the last
`weak_ptr` is gone, because the control block and the object share an
allocation. A large object with long-lived `weak_ptr`s can retain that
memory. `shared_ptr<T>(std::make_unique<T>(...))` uses two allocations and
releases the object memory when the last `shared_ptr` dies, leaving only the
control block for remaining weak pointers. Prefer `make_shared` unless that
retention matters.

### Custom deleters

`unique_ptr`'s deleter is part of the type:

```cpp
auto closer = [](std::FILE* f) { if (f) std::fclose(f); };
std::unique_ptr<std::FILE, decltype(closer)> file(std::fopen("a.txt", "r"), closer);
```

A stateless deleter adds no size. A stateful one is stored next to the
pointer. `shared_ptr`'s deleter is type-erased and lives in the control
block, so `shared_ptr<FILE>` can close a file without the deleter appearing
in the type. That convenience costs the control block even when you only
wanted a custom delete.

`unique_ptr<T[]>` calls `delete[]`. Do not store an array in `unique_ptr<T>`.
Better: store a `std::vector` or a `std::unique_ptr<T[]>`.

### `enable_shared_from_this`

A member function that needs to hand out a `shared_ptr` to `*this` cannot
do `shared_ptr<T>(this)`. That would create a second control block, and both
would delete the object.

```cpp
class Session : public std::enable_shared_from_this<Session> {
public:
    std::shared_ptr<Session> lock() { return shared_from_this(); }
};
```

`shared_from_this()` works only when an existing `shared_ptr` already owns
the object. The `shared_ptr` constructor stores a `weak_ptr` inside the
`enable_shared_from_this` base. Calling `shared_from_this()` from the
constructor is too early: the `shared_ptr` has not finished, and the call
throws `std::bad_weak_ptr`. Call it after the object is owned — in a member
function the caller invokes, or in a factory after `make_shared` returns:

```cpp
std::shared_ptr<Session> make_session() {
    auto s = std::make_shared<Session>();
    s->start();          // start() may call shared_from_this()
    return s;
}
```

Derive from `enable_shared_from_this<MostDerived>`, once. Two bases that
each derive from it is a classic diamond; do not do it.

### Cycles

```cpp
struct Node {
    std::shared_ptr<Node> next;
    std::shared_ptr<Node> prev;
};
// a->next = b; b->prev = a;     use_count never hits 0
```

One direction owns. The back-pointer observes:

```cpp
struct Node {
    std::shared_ptr<Node> next;
    std::weak_ptr<Node> prev;
};
```

`prev.lock()` gives a `shared_ptr` for the duration of the use. If the node
is gone, `lock()` is empty.

Runnable: [`examples/ch14_smart_pointers.cpp`](examples/ch14_smart_pointers.cpp).

---

## 2. Polymorphic collections

```cpp
std::vector<std::unique_ptr<Shape>> shapes;
shapes.push_back(std::make_unique<Circle>(2.0));
shapes.push_back(std::make_unique<Rectangle>(3.0, 4.0));
for (const auto& s : shapes)
    total += s->area();
```

The vector owns the shapes. Destruction runs each `unique_ptr`, which
deletes through `Shape*`, which requires `virtual ~Shape()` (chapter 06).
Nothing slices, because the vector never stores a `Shape` by value. The
concrete objects live on the heap, one allocation each, and walking them
chases pointers.

That is the right container when the set of shapes is open. It is a lot of
machinery when the set is three types you own.

---

## 3. `std::variant` for a closed set

```cpp
#include <variant>

struct Circle { double r = 0; double area() const { return 3.141592653589793 * r * r; } };
struct Square { double s = 0; double area() const { return s * s; } };
using Shape = std::variant<Circle, Square>;

double area(const Shape& sh) {
    return std::visit([](const auto& x) { return x.area(); }, sh);
}

std::vector<Shape> shapes{Circle{2}, Square{3}};
```

A `variant` holds exactly one alternative, in its own storage, plus a
discriminant. `sizeof` is about the size of the largest alternative, plus
the discriminant, plus padding. There is no vptr inside `Circle` and no heap
allocation unless an alternative itself allocates. `visit` is a type-checked
switch. Forgetting to handle an alternative in an exhaustive visitor is a
compile error when the visitor is an overload set that must be invocable for
every alternative; a generic lambda that only calls `x.area()` compiles as
long as every alternative has `area()`.

```
   virtual + unique_ptr                 variant + visit
   --------------------------------     --------------------------------
   open set                             every alternative named here
   heap, pointer chasing                contiguous values
   new subtype, old code still links    new alternative, visitors must compile
   vptr in the object                   discriminant in the variant
```

Valueless-by-exception: if an assignment throws after the old alternative
was destroyed and the new one failed to construct, the variant can be in a
special empty state. Rare if your alternatives' moves are `noexcept`. Check
`valueless_by_exception()` only if you have throwing moves.

`std::get<Circle>(sh)` throws `std::bad_variant_access` on the wrong
alternative. `std::get_if<Circle>(&sh)` returns a pointer or null. Prefer
`visit` so a new alternative cannot be silently ignored. `std::holds_alternative<Circle>(sh)`
is the boolean test.

`std::monostate` is an empty alternative for "no value yet" when you cannot
default-construct the others.

Runnable: [`examples/ch14_variant.cpp`](examples/ch14_variant.cpp).

---

## 4. Type erasure

Type erasure is a value type that can hold any object matching a duck-typed
interface. The stored types do not share a base. `std::function` is type
erasure for "callable with this signature." `std::any` is type erasure for
"anything, and you ask the type back with `any_cast`." A hand-rolled
`Drawable` is type erasure for "has `draw()`."

The implementation is the concept-model sandwich. The public type holds a
pointer to a private abstract interface. A private template derived class
wraps the concrete object.

```cpp
class Drawable {
    struct Concept {
        virtual void draw() const = 0;
        virtual std::unique_ptr<Concept> clone() const = 0;
        virtual ~Concept() = default;
    };
    template <class T>
    struct Model final : Concept {
        T obj;
        explicit Model(T o) : obj(std::move(o)) {}
        void draw() const override { draw_of(obj); }
        std::unique_ptr<Concept> clone() const override {
            return std::make_unique<Model>(obj);
        }
    };

    std::unique_ptr<Concept> self_;

public:
    template <class T>
    Drawable(T obj) : self_(std::make_unique<Model<std::decay_t<T>>>(std::move(obj))) {}

    Drawable(const Drawable& o) : self_(o.self_->clone()) {}
    Drawable(Drawable&&) noexcept = default;
    Drawable& operator=(Drawable&&) noexcept = default;
    Drawable& operator=(const Drawable& o) {
        if (this != &o) self_ = o.self_->clone();
        return *this;
    }

    void draw() const { self_->draw(); }
};
```

`Button` and `Icon` need a `draw()` that `draw_of` can find (a member or a
hidden friend). They do not inherit from `Drawable`. `vector<Drawable>`
stores values. Copying a `Drawable` clones the model. The cost is a heap
allocation and a virtual call, same order as `unique_ptr<Base>`, with the
difference that the concrete type never mentioned your base class.

Chapter 23 adds the small-buffer optimization that avoids the heap when
`Model<T>` is small, which is what `std::function` does for small lambdas.
The sketch above always allocates, which is easier to get right and fine
outside a hot path.

A templated constructor will swallow the copy constructor unless you
constrain it (chapter 04). The sketch has that bug if `T` can be `Drawable`.
Constrain it:

```cpp
template <class T>
    requires (!std::same_as<std::decay_t<T>, Drawable>)
Drawable(T obj);
```

---

## 5. Move in the design, briefly

Chapter 04 is the full story. The design rules that belong here:

```
   * Return big objects by value. C++17 initializes the caller directly
     from a prvalue, and named locals are moved if elision does not happen.
   * Take sink parameters by value and std::move them into the member.
   * Mark non-throwing moves noexcept so vector reallocation will move.
   * Rule of Zero: members that already move (string, vector, unique_ptr)
     make the class move correctly with no code.
```

```cpp
class Widget {
public:
    explicit Widget(std::string name) : name_(std::move(name)) {}
private:
    std::string name_;
};
```

---

## 6. `override`, `final`, `= default`, `= delete`

State the contract in the class body so a reader does not have to reconstruct
it from what the compiler would have generated.

```cpp
class Base {
public:
    virtual void f();
    virtual ~Base() = default;
};
class Derived final : public Base {
public:
    void f() override;
    Derived(const Derived&) = delete;
    Derived& operator=(const Derived&) = delete;
    Derived(Derived&&) noexcept = default;
    Derived& operator=(Derived&&) noexcept = default;
};
```

`final` on the class devirtualizes calls the compiler can see are on a
`Derived`. `= delete` on the copy operations makes `Derived` move-only.
Because the moves are user-declared, the copies would have been deleted
anyway; writing `= delete` documents it for the next reader and does not
depend on them remembering the rule.

---

## 7. Deducing `this` (C++23)

An explicit object parameter replaces the implicit `this`. The function is
still a member. It cannot be `virtual`, and it cannot also have a
cv-qualifier or a ref-qualifier on the declarator: that qualification is
written on the parameter.

```cpp
class Widget {
    std::string data_;
public:
    template <class Self>
    auto&& data(this Self&& self) {
        return std::forward<Self>(self).data_;
    }
};
```

```
   Widget w;
   w.data()                  Self is Widget&          returns string&
   std::as_const(w).data()   Self is const Widget&    returns const string&
   std::move(w).data()       Self deduces as Widget; the parameter is Widget&&
                             returns std::string&&
```

One template replaces the three ref-qualified overloads from chapter 01.
The same feature replaces a lot of CRTP: the explicit parameter can be a
derived type, and you call derived members on it without a
`static_cast<Derived*>(this)` in a base template. Chapter 23 shows the CRTP
form this retires.

You need C++23. GCC 14 and Clang 18 implement it. The example is
[`examples/ch14_deducing_this.cpp`](examples/ch14_deducing_this.cpp).

---

## 8. `<=>` in the class

```cpp
struct Point {
    int x = 0, y = 0;
    auto operator<=>(const Point&) const = default;
};
```

A defaulted `<=>` gives memberwise ordering and a defaulted `==` (chapter
09). The type can be a `std::set` key and can be compared in tests. Do this
for value types whose equality really is memberwise. Do not default it for a
type with a cache member, a pointer used as identity, or a `double` you are
not prepared to treat as partially ordered.

---

## 9. A modern shape hierarchy, end to end

```cpp
#include <memory>
#include <vector>

class Shape {
public:
    virtual double area() const = 0;
    virtual ~Shape() = default;
protected:
    Shape() = default;
    Shape(const Shape&) = default;
    Shape& operator=(const Shape&) = default;
};

class Circle final : public Shape {
    double r_;
public:
    explicit Circle(double r) : r_(r) {}
    double area() const override { return 3.141592653589793 * r_ * r_; }
};

class Rectangle final : public Shape {
    double w_, h_;
public:
    Rectangle(double w, double h) : w_(w), h_(h) {}
    double area() const override { return w_ * h_; }
};

std::unique_ptr<Shape> make_circle(double r) {
    return std::make_unique<Circle>(r);
}
```

`final` on the leaves, `override` on the functions, a virtual destructor, a
factory that returns `unique_ptr<Shape>`, protected copying on the base so
the value-copy path is `clone` if you add it later. `Circle` is still
copyable: its implicit copy calls the protected `Shape` copy from inside
`Circle`, which is allowed.

Runnable: [`examples/ch14_modern_shapes.cpp`](examples/ch14_modern_shapes.cpp).

---

## 10. Which polymorphism

```
   you have                                         use
   -----------------------------------------------  ---------------------------
   one concrete type at the call site               a template or a value
   a closed set of types you own                    std::variant, std::visit
   an open set, callers hold an interface           virtual, unique_ptr<Base>
   an open set, you cannot modify the types         type erasure
   shared immutable data, real shared ownership     shared_ptr
   a back-pointer into a shared structure           weak_ptr
   "I just need to use it"                          T& or T*
```

Default to values and `unique_ptr`. Introduce `shared_ptr` when ownership is
actually shared. Introduce `virtual` when the set of types is open.
Introduce `variant` when it is closed and you want the compiler to police
the visitors.

---

## 11. Exercises

1. Why does `shared_ptr<T>(this)` inside a member function create a bug that
   `shared_from_this()` does not, and why does `shared_from_this()` throw in
   the constructor?
2. `make_shared<Big>(...)` and a `weak_ptr` that outlives every `shared_ptr`.
   When is the memory for `Big` released?
3. `vector<unique_ptr<Shape>>` versus `vector<variant<Circle, Square>>`.
   Which one can accept a `Triangle` loaded from a plugin `.so` next year
   without editing the vector's type?
4. Why is a deducing-`this` member function unable to be `virtual`?

### Answers

1. `shared_ptr<T>(this)` allocates a new control block. The original owner
   has a different control block. Both delete the object. `shared_from_this`
   copies the existing control block, so there is still one owner count.
   During construction the owning `shared_ptr` has not yet stored the
   `weak_ptr` inside `enable_shared_from_this`, so there is nothing to lock
   and the call throws `bad_weak_ptr`.
2. When the last `weak_ptr` is destroyed. `make_shared` placed the object
   and the control block in one allocation, and the control block stays
   until the weak count drops to zero. The `Big` destructor has already run
   when the last `shared_ptr` died; the storage remains.
3. `vector<unique_ptr<Shape>>`, if `Triangle` derives from `Shape`. The
   variant names its alternatives. A plugin type that was not compiled into
   that list cannot be stored in it.
4. Virtual dispatch needs a fixed function in a vtable slot. A deducing-this
   function that is a template is a family of functions, one per `Self`, and
   an explicit object parameter is not allowed on a virtual function at all.
   The language rejects `virtual` together with an explicit object parameter.

---

## 12. Summary

<!--diagram
title: Modern C++ OOP
box[green] Key points
  text: unique_ptr is the default owner. shared_ptr shares, atomically. weak_ptr observes and breaks cycles. Signatures should say which one you mean
  text: enable_shared_from_this borrows the existing control block. It is unusable in the constructor. Do not write shared_ptr<T>(this)
  text: Open polymorphism is vector<unique_ptr<Base>> and a virtual destructor. Closed polymorphism is variant and visit
  text: Type erasure wraps unrelated types behind a value, using a private virtual interface. Constrain the templated constructor
  text: override, final, =default, and =delete state the contract. Deducing this (C++23) collapses ref-qualified overloads and cannot be virtual
-->
```
 +-------------------------------------------------------------------+
 | unique_ptr by default. shared_ptr only for real shared ownership. |
 | weak_ptr for non-owning observation and for cycles.               |
 | enable_shared_from_this after the object is owned, never in ctor. |
 | Open set: unique_ptr<Base>, virtual destructor.                   |
 | Closed set: variant + visit. Non-intrusive: type erasure.         |
 | Return by value. Sink by value. noexcept moves.                   |
 | override / final / =default / =delete. Deducing this is C++23     |
 |   and is not virtual.                                             |
 +-------------------------------------------------------------------+
```

Next: [15-pitfalls-and-best-practices.md](15-pitfalls-and-best-practices.md).
