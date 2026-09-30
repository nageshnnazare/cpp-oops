# 01 — Classes & Objects

A **class** is a type. An **object** is a region of storage whose type is that
class and whose lifetime has started. This chapter is the anatomy: what can
appear in a class, how member functions actually receive `this`, how objects
are created, and how the bytes are laid out.

Prereq: [00-mental-model.md](00-mental-model.md).

---

## 1. Anatomy of a class

```cpp
class Rectangle {
public:
    Rectangle(double w, double h) : width_(w), height_(h) {}

    double area() const { return width_ * height_; }
    void scale(double f) { width_ *= f; height_ *= f; }

private:
    double width_;
    double height_;
};
```

A class definition can contain more than data and functions. The full menu:

```
   non-static data members          one per object
   static data members              one per class, not in the object (ch 11)
   member functions                 non-static (have this) and static (do not)
   nested types                     class, enum, using-alias (ch 11)
   member templates                 allowed; cannot be virtual
   friend declarations              grant access; not members (ch 11)
   access specifiers                public / protected / private, repeatable
   static_assert, member constants  compile-time checks and values
   bit-fields                       packed integer members
```

Naming in this guide: trailing underscore for private data (`width_`). Pick one
convention and keep it. The language has no opinion.

```
   Blueprint (the type)       Objects (each has its own non-static data)
   +----------------+         r: [ width_=3  height_=4 ]
   | width_         |  --->   s: [ width_=5  height_=6 ]
   | height_        |
   | area()/scale() |         the functions exist once, as code
   +----------------+
```

Runnable: [`examples/ch01_bank_account.cpp`](examples/ch01_bank_account.cpp).

---

## 2. Creating objects

There are several initialization forms, and they are not interchangeable.

```cpp
Rectangle a(3, 4);             // direct-initialization
Rectangle b{3, 4};             // direct-list-initialization
Rectangle c = {3, 4};          // copy-list-initialization
auto d = Rectangle(3, 4);      // prvalue; C++17 does not move (ch 04)

Rectangle r();                 // MOST VEXING PARSE: declares a function
Rectangle r{};                 // an object, value-initialized
```

`Rectangle r();` declares a function named `r` that returns `Rectangle` and
takes no arguments. The grammar prefers a declaration when a statement can be
either. Brace initialization cannot be a function declaration, which is why
`{}` is the form that removes the ambiguity.

Brace initialization also refuses **narrowing**:

```cpp
int n{3.14};          // error: double -> int is a narrowing conversion
int n(3.14);          // compiles, truncates to 3
Rectangle bad{1, 2.5}; // fine: int -> double is not narrowing
```

List-initialization prefers a constructor that takes `std::initializer_list`
if one exists. That single rule explains a famous surprise:

```cpp
std::vector<int> a(10, 2);    // 10 elements, each 2     (count, value)
std::vector<int> b{10, 2};    // 2 elements: 10 and 2    (initializer_list)
```

Parentheses call the overload set in the ordinary way. Braces will pick
`initializer_list` whenever it is viable.

Heap allocation:

```cpp
auto up = std::make_unique<Rectangle>(3, 4);   // owns the object (ch 14)
Rectangle* p = new Rectangle(3, 4);            // you must delete p
delete p;
```

Prefer automatic objects and smart pointers. A raw `new` without an owner is a
leak waiting for an early return.

---

## 3. Member functions and the type of `this`

A non-static member function has an implicit object parameter. You write
`this` to name it. Its type depends on the cv-qualifiers and ref-qualifiers on
the function:

```
   declaration                         type of this          *this
   ----------------------------------  --------------------  -----------------
   void f();                           Rectangle*            Rectangle&
   void f() const;                     const Rectangle*      const Rectangle&
   void f() volatile;                  volatile Rectangle*   (rare)
   void f() &;                         Rectangle*            lvalue only
   void f() &&;                        Rectangle*            rvalue only
   void f() const &;                   const Rectangle*      const lvalue
   static void f();                    (no this)
```

```cpp
class Rectangle {
public:
    double area() const { return width_ * height_; }   // callable on const objects
    void scale(double f) { width_ *= f; height_ *= f; }

    double width() const & { return width_; }          // lvalues: copy out
    double width() && { return width_; }               // rvalues: same, by value
};
```

```cpp
const Rectangle r(3, 4);
r.area();      // ok
r.scale(2);    // error: scale() requires a non-const object
```

Overload on `const` when a member should hand out a mutable reference only for
mutable objects. This is how `std::vector::operator[]` is written:

```cpp
class Buffer {
    char* data_;
    std::size_t n_;
public:
    char&       operator[](std::size_t i)       { return data_[i]; }
    const char& operator[](std::size_t i) const { return data_[i]; }
};
```

A `const Buffer` can only call the second overload, so the caller cannot write
through the returned reference. Forgetting the `const` overload means a
`const` object cannot be indexed at all.

### Ref-qualifiers

A ref-qualifier restricts which value category the object may have. The
important use is assignment, and getters that move out of temporaries:

```cpp
class Widget {
    std::string name_;
public:
    Widget& operator=(const Widget&) &;   // cannot assign to a temporary
    std::string& name() & { return name_; }
    const std::string& name() const & { return name_; }
    std::string name() && { return std::move(name_); }
};

Widget make();
make() = Widget{};     // error if operator= is &-qualified
std::string s = make().name();   // calls the && overload, moves
```

Without the `&` on `operator=`, `make() = Widget{}` compiles and assigns into
an object that dies at the semicolon. Qualifying assignment with `&` makes
that a compile error. C++23 deducing `this` can collapse the three `name()`
overloads into one template ([14-modern-cpp-oop.md](14-modern-cpp-oop.md)).
An explicit object parameter cannot appear on a `virtual` function.

`this` inside a `const` member function is a pointer to const. Assigning
through `const_cast<Rectangle*>(this)` to modify a const object is undefined
behavior. `mutable` is the sanctioned escape hatch
([03-encapsulation.md](03-encapsulation.md)).

### What `this` is for

```cpp
class Builder {
    int x_ = 0;
public:
    Builder& set_x(int x) & {
        x_ = x;          // parameter x hides nothing here; this-> is optional
        return *this;    // chaining: b.set_x(1).set_x(2);
    }
};
```

`this->width_` and `width_` are the same lookup, except when a parameter
shadows the member. Prefer not to shadow. When you must, `this->` is the
disambiguator.

A member function defined inside the class body is implicitly `inline`. An
out-of-line definition is not, unless you mark it `inline`:

```cpp
// rectangle.hpp
class Rectangle {
public:
    Rectangle(double w, double h);
    double area() const;
private:
    double width_, height_;
};

// rectangle.cpp
Rectangle::Rectangle(double w, double h) : width_(w), height_(h) {}
double Rectangle::area() const { return width_ * height_; }
```

The qualified name `Rectangle::area` says which class the function belongs to.
A non-inline, non-template member function must be defined in exactly one
translation unit (the One Definition Rule). Defining it in a header without
`inline` produces a multiple-definition error at link time.

---

## 4. `const` members, reference members, `mutable`

```cpp
class Stamp {
    const int id_;          // must be initialized, can never be assigned
    std::string& label_;    // must be bound in the initializer list, cannot be reseated
public:
    Stamp(int id, std::string& label) : id_(id), label_(label) {}
};
```

Both make the compiler-generated copy assignment deleted: you cannot reseat a
reference or assign to a const member. Prefer storing a value, or a pointer if
you truly need reseating. A `const` member also blocks move assignment.

`mutable` is discussed with logical const in chapter 03. The short version: it
is for caches and mutexes, not for quietly mutating observable state inside a
`const` function.

---

## 5. Default member initializers

A brace-or-equal initializer on a member is used by any constructor that does
not mention that member in its initializer list:

```cpp
class Config {
    int timeout_ = 30;
    bool verbose_ = false;
public:
    Config() = default;                 // timeout_ == 30, verbose_ == false
    explicit Config(int t) : timeout_(t) {}   // verbose_ still false
};
```

The initializer is not an assignment that runs before the constructor. It is
an alternative constructor argument, consumed in declaration order along with
the initializer list (chapter 02).

---

## 6. Object layout

A non-polymorphic object is its non-static data members, in declaration order,
plus padding.

Every type has `sizeof` (how many bytes it occupies, including padding) and
`alignof` (the address of an object of that type must be a multiple of this).
The compiler places each member at the next offset that is a multiple of that
member's alignment, then rounds the class size up to a multiple of the class
alignment so that arrays stay aligned.

```cpp
struct Mixed {
    char  c;     // align 1
    int   i;     // align 4
    char  d;     // align 1
};
```

```
   offset:  0    1  2  3    4  5  6  7    8    9 10 11
            +----+---+---+---+---+--+--+--+----+---+---+---+
            | c  |pad pad pad| i (4 bytes)| d  |pad pad pad|
            +----+-----------+------------+----+-----------+

   sizeof(Mixed) == 12, alignof(Mixed) == 4
   The tail padding exists so that Mixed arr[2] has arr[1].i aligned.
```

Reorder from stricter alignment to weaker alignment when you control the
order. The language does not reorder for you; the ABI forbids it, because
layout is part of the binary contract.

```cpp
struct Packed { int i; char c; char d; };   // sizeof == 8 on ordinary ABIs
```

Static data members and all member functions contribute 0 bytes. A polymorphic
class adds a vptr, typically at offset 0 on the Itanium ABI, and stops being
standard-layout (chapter 17).

`sizeof` of an empty class is at least 1, so two complete empty objects have
different addresses. As a base, that empty class may occupy 0 bytes (EBO).

### `alignas`

```cpp
struct alignas(64) CacheLine {
    int value;
};
// sizeof is a multiple of 64. Used so two atomics do not share a cache line.
```

`alignas` can raise alignment, not lower it below what the members require.

### `offsetof`

`offsetof(T, member)` is defined for standard-layout types. Using it on a
type with virtual functions, mixed access, or virtual bases is conditionally
supported at best and not portable. Chapter 17 gives the exact category rules.

### Bit-fields

```cpp
struct Flags {
    unsigned ready : 1;
    unsigned error : 1;
    unsigned code  : 6;
};
```

Adjacent bit-fields are packed into an addressable storage unit. You cannot
form a pointer to a bit-field, and you cannot bind a non-const reference to
one. The order of bits inside the unit is ABI-defined. A zero-width unnamed
bit-field (`unsigned : 0;`) forces the next bit-field into a new unit. Do not
serialize bit-fields by `memcpy` and expect another compiler to read them.

### Unions, briefly

```cpp
union U {
    int i;
    float f;
};
U u;
u.i = 1;          // i is the active member
// read u.f       // undefined behavior: f is not active
u.f = 1.0f;       // now f is active; for non-trivial members you must
                  // destroy the old one first (ch 02, placement new)
```

Anonymous unions inject their members into the enclosing scope. They are the
mechanism `std::variant` and `std::optional` are built on, but writing them by
hand means you are responsible for lifetime. Prefer `std::variant` unless you
are implementing a vocabulary type.

---

## 7. Aggregates and designated initializers

In C++20 a type is an **aggregate** when it has:

```
   no user-declared constructors          (S() = default disqualifies it)
   no private or protected non-static data members
   no virtual functions
   no virtual, private, or protected bases
   no user-declared or inherited constructors
```

Public bases are allowed. Brace-or-equal initializers on members are allowed.

```cpp
struct Point { int x; int y = 0; int z = 0; };

Point p{1, 2, 3};
Point q{.x = 1, .y = 2, .z = 3};   // C++20 designated initializers
Point r{.x = 1};                   // y and z are value-initialized from = 0
```

Designators must appear in declaration order. You cannot write `.z = 3, .x = 1`.
You cannot mix a designator with a nested reordering. C allows out-of-order
designators; C++ does not, so the compiler can initialize in one forward pass.

```cpp
struct Line { Point a; Point b; };
Line L{.a = {.x = 1, .y = 2}, .b{.x = 3}};
```

A user-declared constructor, including one that is `= default`, ends aggregate
status. The moment you add a constructor to enforce an invariant, callers must
use that constructor. That is usually what you want.

Structured bindings unpack aggregates (and tuple-like types):

```cpp
Point p{1, 2, 3};
auto [x, y, z] = p;    // copies; auto& [x, y, z] = p; binds references
```

The number of names must match the number of non-static data members.

---

## 8. Incomplete types

A class can be named before it is defined:

```cpp
class Widget;                 // incomplete: size unknown

Widget* p;                    // ok: pointers have a known size
Widget& f();                  // ok: references are not objects
// Widget w;                  // error: cannot allocate an incomplete object
// sizeof(Widget)             // error

class Widget { int x; };      // now complete
```

Incomplete types are how you break header cycles and how the pimpl idiom hides
a member's definition ([03-encapsulation.md](03-encapsulation.md)). You may
not declare a non-static data member of incomplete type, except that a class
may contain a pointer or reference to itself (linked nodes).

---

## 9. Special member functions, preview

The compiler can declare six special members for you. Chapter 04 is their
full treatment. What matters here:

```cpp
class Foo {
public:
    Foo() = default;            // "give me the usual default constructor"
    Foo(const Foo&) = delete;   // "copying this type is a compile error"
    explicit Foo(int);
};
```

Declaring any constructor suppresses the implicit default constructor. If you
still want one, `= default` it. `= delete` participates in overload
resolution and then makes the program ill-formed, which is stronger than
"there is no such function": a deleted function can be a better match than a
viable one and thereby block an unwanted conversion.

---

## 10. `constexpr` and `consteval` members

A sufficiently simple member function may be `constexpr`: the call is a
constant expression when its arguments are.

```cpp
class Point {
    int x_, y_;
public:
    constexpr Point(int x, int y) : x_(x), y_(y) {}
    constexpr int x() const { return x_; }
};

constexpr Point p{1, 2};
static_assert(p.x() == 1);
```

C++20 allows `constexpr` virtual functions: if the dynamic type is known at
compile time, the override is called at compile time. `consteval` means the
function *must* be evaluated at compile time. Destructors can be `constexpr`
since C++20, which is what lets `constexpr` containers work.

---

## 11. What does not belong in the object

```cpp
class Widget {
public:
    static int live;             // not in sizeof(Widget)
    static int count() { return live; }   // no this
    using value_type = int;      // a nested alias, not data
    enum class Kind { A, B };    // nested enum
};
```

A static member function is an ordinary function with access to private
members. It cannot use `this`, and it cannot be `const`, because there is no
object to const-qualify. Chapter 11 covers static initialization order.

---

## 12. Worked example: `BankAccount`

The account's invariant is `balance_ >= 0`. The constructor and `withdraw`
are the only writers, so the invariant is checkable in one place.

```cpp
#include <iostream>
#include <string>
#include <utility>

class BankAccount {
public:
    BankAccount(std::string owner, double initial)
        : owner_(std::move(owner)), balance_(initial < 0 ? 0 : initial) {}

    void deposit(double amount) {
        if (amount > 0) balance_ += amount;
    }

    bool withdraw(double amount) {
        if (amount <= 0 || amount > balance_) return false;
        balance_ -= amount;
        return true;
    }

    double balance() const { return balance_; }
    const std::string& owner() const { return owner_; }

private:
    std::string owner_;
    double balance_;
};

int main() {
    BankAccount acc("Ada", 100.0);
    acc.deposit(50);
    std::cout << acc.owner() << " has " << acc.balance() << '\n';
    std::cout << (acc.withdraw(500) ? "ok" : "declined") << '\n';
    std::cout << "balance still " << acc.balance() << '\n';
}
```

`owner()` returns a `const` reference to a member. That is safe for the
caller who uses it before `acc` dies. It is a dangling reference if the
caller does `const std::string& s = BankAccount{"tmp", 0}.owner();` — the
temporary account dies at the semicolon. Return by value when the caller
might outlive the object. Return a reference when the object is clearly the
owner and the signature documents that (a member function of a long-lived
container, for example).

Runnable: [`examples/ch01_bank_account.cpp`](examples/ch01_bank_account.cpp)
and [`examples/ch01_layout.cpp`](examples/ch01_layout.cpp).

---

## 13. Exercises

1. Explain the type of each declaration: `Widget w;`, `Widget w{};`,
   `Widget w();`, `Widget w = Widget{};`.
2. Why does `struct S { int a; char b; };` often have `sizeof` 8, not 5?
3. `char& at(std::size_t i) const { return data_[i]; }` compiles if `data_` is
   `char*`. What invariant of `const` did you just break, and what is the fix?
4. Is `struct P { P() = default; int x; int y; };` an aggregate in C++20?
   Can you write `P{.x = 1}`?

### Answers

1. `Widget w;` default-initializes (default constructor; indeterminate if it
   were a built-in). `Widget w{}` value-initializes / list-initializes.
   `Widget w();` is a function declaration. `Widget w = Widget{}` initializes
   `w` from a prvalue; since C++17 the prvalue initializes `w` directly.
2. `a` occupies bytes 0–3. `b` occupies byte 4. `alignof(S)` is 4, so the
   size rounds up to 8 with 3 bytes of tail padding. An array `S[2]` needs
   each `a` on a 4-byte boundary.
3. A `const` member function returned a mutable reference to the object's
   state, so a `const Widget` can be modified. Return `const char&`, or
   provide two overloads and give the non-const one the mutable reference.
4. No. `P() = default` is user-declared, so `P` is not an aggregate.
   Designated initializers do not apply. Give it no constructor, or call a
   real constructor.

---

## 14. Summary

<!--diagram
title: Classes & objects
box[green] Key points
  text: A class is a type; each object has its own non-static data. Functions are code, not per-object fields
  text: `{}` avoids the most vexing parse and forbids narrowing. `initializer_list` constructors win list-initialization
  text: `this` is const inside a const member function. Ref-qualifiers (`&` / `&&`) restrict lvalues and rvalues. Overload on const
  text: Layout is declaration order plus alignment padding. Empty complete objects have size at least 1
  text: C++20 aggregates have no user-declared constructors. Designated initializers are in-order
-->
```
 +------------------------------------------------------------------+
 | Class = type; object = storage of that type with a lifetime.     |
 | Non-static data is per object. Member functions are not.         |
 | Prefer {} : no most-vexing-parse, no narrowing.                  |
 | initializer_list constructors are preferred by list-init.        |
 | const member => const this. Ref-qualifiers restrict & vs &&.     |
 | Overload on const to hand out mutable references safely.         |
 | Layout = declaration order + padding. alignof rounds sizeof.     |
 | C++20 aggregate: no user-declared ctor. Designators in order.    |
 +------------------------------------------------------------------+
```

Next: [02-constructors-destructors.md](02-constructors-destructors.md).
