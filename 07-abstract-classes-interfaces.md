# 07 — Abstract Classes & Interfaces

Abstraction — the second pillar — is about defining **what** something does
without saying **how**. In C++ this is expressed with **pure virtual functions**,
**abstract classes**, and **interfaces**. This is how you program to a contract.

Prereq: [06-polymorphism-vtables.md](06-polymorphism-vtables.md).

---

## 1. Pure virtual functions

A pure virtual function declares an operation with **no implementation required**
in the base — derived classes *must* provide one.

```cpp
class Shape {
public:
    virtual double area() const = 0;        // pure virtual (the "= 0")
    virtual double perimeter() const = 0;
    virtual ~Shape() = default;             // virtual dtor (chapter 06 §7)
};
```

```
   virtual double area() const = 0;
                                ^^^
   "= 0" means PURE virtual: no body needed here; subclasses must override it.
   A class with >=1 pure virtual is ABSTRACT: it cannot be instantiated.
```

---

## 2. Abstract classes cannot be instantiated

```cpp
// Shape s;                 // ERROR: 'Shape' is abstract (has pure virtuals)
Shape* p = nullptr;         // OK: pointers/references to abstract types are fine

class Circle : public Shape {
    double r_;
public:
    Circle(double r) : r_(r) {}
    double area() const override { return 3.14159265 * r_ * r_; }      // must impl
    double perimeter() const override { return 2 * 3.14159265 * r_; }  // must impl
};

Circle c(2);                // OK: Circle implements ALL pure virtuals -> concrete
```

```
   Shape (abstract)                 Circle (concrete)
   +------------------+             +------------------+
   | area() = 0       |  <-------   | area() override  |
   | perimeter() = 0  |  implements | perimeter() over |
   +------------------+             | r_               |
   can't create a Shape             +------------------+
                                    can create a Circle

   If Circle FORGOT to implement perimeter(), Circle would ALSO be abstract.
```

---

## 3. Programming to an interface

The power move: write code against the **abstract base**, and it works for any
current *or future* derived type.

```cpp
double total_area(const std::vector<std::unique_ptr<Shape>>& shapes) {
    double sum = 0;
    for (const auto& s : shapes)
        sum += s->area();       // virtual dispatch: each shape's own area()
    return sum;
}
```

```
   total_area doesn't know about Circle/Square/Triangle. It only knows Shape.
   Add a new Shape subclass later -> total_area works UNCHANGED.
   -> This is the Open/Closed Principle (chapter 12): open to extension,
      closed to modification.
```

```
   caller ──uses──> [ Shape interface ] <──implements── Circle
                                        <──implements── Square
                                        <──implements── Triangle (added later)
   Dependencies point at the ABSTRACTION, not concrete types.
```

Runnable: [`examples/ch07_shapes.cpp`](examples/ch07_shapes.cpp).

---

## 4. "Interfaces" in C++ (pure abstract classes)

C++ has no `interface` keyword. The idiom is a class with **only** pure virtual
functions (plus a virtual destructor) and no data — a pure abstract class.

```cpp
class Drawable {                 // an "interface"
public:
    virtual void draw() const = 0;
    virtual ~Drawable() = default;
};

class Serializable {             // another "interface"
public:
    virtual std::string serialize() const = 0;
    virtual ~Serializable() = default;
};

// A class can implement MULTIPLE interfaces (multiple inheritance, chapter 08):
class Button : public Drawable, public Serializable {
public:
    void draw() const override { /* ... */ }
    std::string serialize() const override { return "Button{...}"; }
};
```

```
   Interface = pure abstract class (all methods pure virtual, no state).
   Implementing several interfaces via multiple inheritance is the ONE place
   multiple inheritance is broadly endorsed (chapter 08). No diamond issues
   because interfaces have no data.
```

---

## 5. Pure virtual functions CAN have a body (rare but useful)

`= 0` means "must be overridden," but you may still *provide* a default
implementation that derived classes call explicitly.

```cpp
class Logger {
public:
    virtual void log(const std::string& msg) = 0;   // still pure -> abstract
    virtual ~Logger() = default;
};
void Logger::log(const std::string& msg) {          // ...but has a definition
    std::cout << "[default] " << msg << '\n';
}

class FileLogger : public Logger {
public:
    void log(const std::string& msg) override {
        Logger::log(msg);          // reuse the base's default behavior
        // + write to file
    }
};
```

```
   A pure virtual with a body = "you MUST override, but here's shared code you
   can delegate to via Base::method()." Handy for common setup/teardown.
```

---

## 6. The Non-Virtual Interface (NVI) idiom

Make the **public** interface non-virtual and the **customization points**
private/protected virtual. This keeps a stable public API while letting
subclasses customize the guts, and lets the base add pre/post logic.

```cpp
class Report {
public:
    void generate() {              // public, NON-virtual: fixed algorithm skeleton
        write_header();
        write_body();              // the varying part
        write_footer();
    }
    virtual ~Report() = default;
private:
    void write_header() { /* common */ }
    void write_footer() { /* common */ }
    virtual void write_body() = 0; // the ONLY customization point (private virtual)
};

class SalesReport : public Report {
    void write_body() override { /* sales-specific */ }
};
```

```
   Public API (generate) is STABLE and non-virtual -> base controls the flow
   (add logging, locking, invariants around the virtual call).
   Subclasses override the PRIVATE virtual (write_body) — they can override a
   private virtual even though they can't CALL it. This is the Template Method
   pattern (chapter 13) done the NVI way.
```

---

## 7. Abstract class vs interface vs concrete base

<!--diagram
title: Abstract vs interface vs concrete
box[green] Concrete class
  text: All methods implemented; can instantiate
box[teal] Abstract class
  text: ≥1 pure virtual; can't instantiate; **may** have data + implemented methods (partial)
box[purple] Interface (idiom)
  text: **All** pure virtual, **no** data; a pure contract
-->
```
   +-------------------+---------------------------------------------+
   | Concrete class    | all methods implemented; can instantiate    |
   | Abstract class    | >=1 pure virtual; can't instantiate; MAY    |
   |                   | have data + implemented methods (partial)   |
   | Interface (idiom) | ALL pure virtual, NO data; a pure contract  |
   +-------------------+---------------------------------------------+

   Use an INTERFACE when you only need a contract (and maybe multiple ones).
   Use an ABSTRACT class when subclasses share state / common code too.
```

---

## 8. Worked example: a plugin-style shape system

```cpp
#include <iostream>
#include <memory>
#include <vector>

class Shape {
public:
    virtual double area() const = 0;
    virtual const char* name() const = 0;
    virtual ~Shape() = default;
};

class Circle : public Shape {
    double r_;
public:
    explicit Circle(double r) : r_(r) {}
    double area() const override { return 3.14159265 * r_ * r_; }
    const char* name() const override { return "Circle"; }
};

class Rectangle : public Shape {
    double w_, h_;
public:
    Rectangle(double w, double h) : w_(w), h_(h) {}
    double area() const override { return w_ * h_; }
    const char* name() const override { return "Rectangle"; }
};

int main() {
    std::vector<std::unique_ptr<Shape>> shapes;
    shapes.push_back(std::make_unique<Circle>(2.0));
    shapes.push_back(std::make_unique<Rectangle>(3.0, 4.0));

    double total = 0;
    for (const auto& s : shapes) {
        std::cout << s->name() << " area = " << s->area() << '\n';
        total += s->area();
    }
    std::cout << "total area = " << total << '\n';
}
```

```
   Output:
     Circle area = 12.5664
     Rectangle area = 12
     total area = 24.5664
   The loop is written against Shape ONLY. Adding a Triangle class later needs
   ZERO changes to main's loop.
```

Runnable: [`examples/ch07_shapes.cpp`](examples/ch07_shapes.cpp).

---

## 9. Summary

<!--diagram
title: Abstract classes & interfaces
box[green] Key points
  text: **Pure virtual**: `virtual R f() = 0;` → subclasses **must** override. A class with any pure virtual is **abstract** (can't instantiate), but pointers/refs are fine
  text: **Interface** (C++ idiom) = pure abstract class: all pure virtual, no data, virtual dtor. Implement several via multiple inheritance
  text: Program to the **interface**: code depends on the abstraction, so new subclasses work without changing callers (Open/Closed)
  text: Pure virtuals **may** have a body (shared default via `Base::f()`). **NVI** idiom: public non-virtual API + private virtual hooks
-->
```
 +-------------------------------------------------------------------+
 | Pure virtual: 'virtual R f() = 0;' -> subclasses MUST override.   |
 | A class with any pure virtual is ABSTRACT (can't instantiate),    |
 |   but you can have pointers/refs to it.                           |
 | Interface (C++ idiom) = pure abstract class: all pure virtual,    |
 |   no data, virtual dtor. Implement several via multiple inherit.  |
 | Program to the INTERFACE: code depends on the abstraction, so new |
 |   subclasses work without changing callers (Open/Closed).         |
 | Pure virtuals MAY have a body (shared default via Base::f()).     |
 | NVI idiom: public non-virtual API + private virtual hooks.        |
 +-------------------------------------------------------------------+
```

Next: [08-multiple-virtual-inheritance.md](08-multiple-virtual-inheritance.md).
