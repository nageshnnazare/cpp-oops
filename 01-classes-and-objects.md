# 01 — Classes & Objects

A **class** is a blueprint; an **object** is an instance built from it. This
chapter covers the anatomy of a class, member functions, `this`, and how objects
are laid out in memory.

Prereq: [00-mental-model.md](00-mental-model.md).

---

## 1. Anatomy of a class

```cpp
class Rectangle {
public:                          // access specifier (chapter 03)
    Rectangle(double w, double h) : width_(w), height_(h) {}  // constructor

    double area() const { return width_ * height_; }          // member function
    void scale(double f) { width_ *= f; height_ *= f; }       // mutator

private:
    double width_;               // data member (a.k.a. field / attribute)
    double height_;
};
```

```
   class Rectangle {  ...  };     <- the BLUEPRINT (a type)

   Rectangle r(3, 4);            <- an OBJECT (an instance) of that type
   Rectangle s(5, 6);            <- another, independent object
```

```
   Blueprint (class)          Objects (instances)
   +----------------+         r: [ width_=3  height_=4 ]
   | width_         |  --->   s: [ width_=5  height_=6 ]
   | height_        |         each object has its OWN copy of the data members
   | area()/scale() |         (methods are shared, not stored per object)
   +----------------+
```

Naming convention used throughout this guide: trailing underscore (`width_`) for
private data members. Pick a convention and be consistent.

---

## 2. Creating objects (several ways)

```cpp
Rectangle a(3, 4);            // direct initialization
Rectangle b{3, 4};            // brace (list) initialization — preferred, safer
Rectangle c = Rectangle(3,4); // create + (elided) move
auto d = Rectangle(3, 4);     // auto with the type on the right

Rectangle* p = new Rectangle(3, 4);   // on the heap (raw ptr — prefer smart ptr!)
delete p;                             // must free manually (error-prone)

auto up = std::make_unique<Rectangle>(3, 4);  // heap + automatic cleanup (ch 14)
```

```
   STACK object:   Rectangle a(3,4);   -> lives until end of scope, auto-destroyed
   HEAP object:    new Rectangle(...)  -> lives until you 'delete' it (or leak!)
   -> Prefer stack objects and smart pointers; avoid raw new/delete (chapter 14).
```

Brace init `{}` is preferred: it prevents narrowing conversions and avoids the
"most vexing parse" (`Rectangle r();` declares a *function*, not an object!).

---

## 3. Member functions and `const`-correctness

```cpp
double area()  const { return width_ * height_; }   // does NOT modify -> const
void   scale(double f) { width_ *= f; }             // modifies -> non-const
```

```
   A 'const' member function promises not to modify the object's state.
   You can call const methods on const objects; non-const methods you CANNOT.

   const Rectangle r(3,4);
   r.area();     // OK: area() is const
   r.scale(2);   // ERROR: scale() is non-const, r is const
```

`const`-correctness is central to good C++ OOP: mark every method that doesn't
mutate as `const`. It documents intent and lets objects be used as `const`.

<!--diagram
box[green] const-correctness
  text: **const method** → callable on `const` AND non-`const` objects
  text: **non-const method** → callable on non-`const` objects only
-->
```
   +------------------------------------------------------------+
   | const method  -> callable on const AND non-const objects   |
   | non-const     -> callable on non-const objects only        |
   +------------------------------------------------------------+
```

---

## 4. `this` — the hidden pointer

Every non-static member function receives an implicit pointer, `this`, to the
object it was called on.

```cpp
void Rectangle::scale(double f) {
    this->width_  *= f;      // 'this->' is usually optional
    this->height_ *= f;
}
```

```
   r.scale(2);
        |
        v   the compiler calls  Rectangle::scale(&r, 2);
   inside scale: this == &r    -> this->width_ is r.width_
```

Uses of `this`:

```cpp
class Builder {
    int x_ = 0;
public:
    Builder& set_x(int x) { x_ = x; return *this; }   // return self -> chaining
    // usage: b.set_x(1).set_x(2);   (fluent interface)
};
```

```
   'this'   -> pointer to the current object (type: Rectangle*)
   '*this'  -> the current object itself (a reference when returned)
   Common uses: disambiguate names, method chaining, self-comparison (chapter 04).
```

Since C++23 you can make `this` an **explicit parameter** ("deducing this") —
chapter 14.

---

## 5. Declaration vs definition; separating interface & implementation

Small classes go entirely in a header. Larger ones split declaration (`.hpp`)
from definition (`.cpp`):

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
#include "rectangle.hpp"
Rectangle::Rectangle(double w, double h) : width_(w), height_(h) {}
double Rectangle::area() const { return width_ * height_; }
```

```
   Rectangle::area   <- the '::' says "area is a member of Rectangle"
   Definitions of NON-inline, NON-template members go in ONE .cpp (ODR).
   Methods defined INSIDE the class body are implicitly 'inline'.
```

---

## 6. Object layout in memory

A (non-polymorphic) object is just its data members laid out in order, subject to
**alignment padding**.

```cpp
class Mixed {
    char  c;      // 1 byte
    int   i;      // 4 bytes
    char  d;      // 1 byte
};
```

```
   Naive size: 1 + 4 + 1 = 6 bytes. Actual: usually 12 (padding for alignment).

   Offset:  0    1  2  3    4  5  6  7    8    9 10 11
            +----+---+---+---+---+--+--+--+----+---+---+---+
            | c  |pad pad pad| i (4 bytes)| d  |pad pad pad|
            +----+-----------+------------+----+-----------+
   'i' must start at a 4-byte boundary -> 3 bytes padding after 'c'.
   Trailing padding rounds the size up to a multiple of the largest alignment.
```

Reorder members largest-to-smallest to minimize padding:

```cpp
class Packed { int i; char c; char d; };   // 4 + 1 + 1 + 2pad = 8 bytes
```

```
   sizeof(Mixed)  == 12    (poor ordering)
   sizeof(Packed) == 8     (better ordering — same data!)
   Member ORDER affects size. Check with sizeof / offsetof.
```

Empty classes have `sizeof >= 1` (so distinct objects have distinct addresses).
Static members and methods do **not** add to object size (chapter 11).

Runnable: [`examples/ch01_layout.cpp`](examples/ch01_layout.cpp).

---

## 7. Aggregates & designated initializers (C++20)

A class with only public data, no user-declared constructors, no private/virtual
stuff is an **aggregate** and supports brace/designated init:

```cpp
struct Point { int x; int y; int z; };

Point p{1, 2, 3};                 // aggregate init
Point q{.x = 1, .y = 2, .z = 3};  // C++20 designated initializers (in order!)
Point r{.x = 1};                  // y and z are value-initialized to 0
```

```
   Designated init makes intent explicit and is great for config-style structs.
   C++ requires the designators to be in DECLARATION ORDER (unlike C).
```

---

## 8. Worked example: a `BankAccount`

```cpp
#include <iostream>
#include <string>

class BankAccount {
public:
    BankAccount(std::string owner, double initial)
        : owner_(std::move(owner)), balance_(initial) {}

    void deposit(double amount) { if (amount > 0) balance_ += amount; }

    bool withdraw(double amount) {
        if (amount > 0 && amount <= balance_) { balance_ -= amount; return true; }
        return false;                       // invariant: balance never negative
    }

    double balance() const { return balance_; }              // const accessor
    const std::string& owner() const { return owner_; }

private:
    std::string owner_;
    double      balance_;
};

int main() {
    BankAccount acc("Ada", 100.0);
    acc.deposit(50);
    std::cout << acc.owner() << " has " << acc.balance() << '\n';  // Ada has 150
    std::cout << (acc.withdraw(500) ? "ok" : "declined") << '\n';  // declined
    std::cout << "balance still " << acc.balance() << '\n';        // 150
}
```

Runnable: [`examples/ch01_bank_account.cpp`](examples/ch01_bank_account.cpp).

---

## 9. Summary

<!--diagram
title: Classes & objects
box[green] Key points
  text: `class` = blueprint; `object` = instance. Each object has its own copy of **data** members; methods are shared
  text: Prefer brace init `{}` (avoids narrowing & most-vexing-parse)
  text: Mark non-mutating methods `const` (const-correctness)
  text: `this` = pointer to current object; return `*this` for chaining
  text: Object size = members + alignment padding; **order** matters
  text: Aggregates support (designated, C++20) brace initialization
  text: Prefer stack objects + smart pointers over raw `new`/`delete`
-->
```
 +------------------------------------------------------------------+
 | class = blueprint; object = instance. Each object has its own    |
 | copy of DATA members; methods are shared.                        |
 |                                                                  |
 | Prefer brace init {} (avoids narrowing & most-vexing-parse).     |
 | Mark non-mutating methods 'const' (const-correctness).           |
 | 'this' = pointer to current object; return *this for chaining.   |
 | Object size = members + alignment padding; ORDER matters.        |
 | Aggregates support (designated, C++20) brace initialization.     |
 | Prefer stack objects + smart pointers over raw new/delete.       |
 +------------------------------------------------------------------+
```

Next: [02-constructors-destructors.md](02-constructors-destructors.md).
