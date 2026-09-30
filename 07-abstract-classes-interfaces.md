# 07 — Abstract Classes & Interfaces

An abstract class defines a contract that concrete classes fulfill. Callers
write against the contract. New concrete classes extend the program without
editing the callers. That is abstraction as a C++ mechanism, and it is also
the Open/Closed Principle in chapter 12.

Prereq: [06-polymorphism-vtables.md](06-polymorphism-vtables.md).

---

## 1. Pure virtual functions

```cpp
class Shape {
public:
    virtual double area() const = 0;
    virtual double perimeter() const = 0;
    virtual ~Shape() = default;
};
```

`= 0` makes the function **pure virtual**. A class with at least one pure
virtual function is **abstract**. You cannot create an object of an abstract
type. You can have pointers and references to it, and other classes can
derive from it.

```cpp
// Shape s;                 // error: abstract
Shape* p = nullptr;         // ok
const Shape& worst(const Shape& a, const Shape& b) {
    return a.area() < b.area() ? a : b;
}
```

A derived class that does not override every pure virtual function is itself
abstract. The compiler will say so when someone tries to construct it, often
with a note listing the functions that are still pure. That note is the
diagnostic to read.

```cpp
class Circle : public Shape {
    double r_;
public:
    explicit Circle(double r) : r_(r) {}
    double area() const override { return 3.141592653589793 * r_ * r_; }
    double perimeter() const override { return 2 * 3.141592653589793 * r_; }
};
```

`Circle` is concrete because both pure functions are overridden. Forgetting
`perimeter` would make `Circle` abstract, and `Circle c(1);` would fail.

---

## 2. A pure virtual destructor

A destructor may be pure, which makes the class abstract even if every other
function has a body. The destructor still needs a definition, because
destructors of derived classes call the base destructor.

```cpp
class Interface {
public:
    virtual void draw() const = 0;
    virtual ~Interface() = 0;
};

Interface::~Interface() = default;     // required, and out of line or inline
```

`virtual ~Interface() = 0;` with no definition links as an undefined
reference to the base destructor the first time a derived object is
destroyed. Provide the body. If the only reason the destructor is pure is to
force the class to be abstract, a different pure function is clearer, and a
public virtual destructor that is `= default` is the normal owning-interface
shape.

---

## 3. Pure virtual with a body

`= 0` means "an override is required." It does not mean "there is no
function." You may still define the function, and derived classes call it
explicitly:

```cpp
class Logger {
public:
    virtual void log(const std::string& msg) = 0;
    virtual ~Logger() = default;
};

void Logger::log(const std::string& msg) {
    std::cerr << msg << '\n';
}

class FileLogger : public Logger {
public:
    void log(const std::string& msg) override {
        Logger::log(msg);          // qualified call, runs the base body
        // then write to the file
    }
};
```

The qualified call is required. An unqualified `log(msg)` inside
`FileLogger::log` is a recursive call to the override. The base body is
shared setup, not a default implementation that runs automatically. If you
want a default that runs automatically, the function should not be pure:
give it a virtual body in the base and let derived classes override only when
they differ.

A pure virtual function's vtable slot, in the abstract class itself, does not
point at that body in a way you should call through the vtable during
construction. Derived classes that have finished construction have their own
slot pointing at the override. The base body is reached by `Base::f()`.

---

## 4. Programming to the contract

```cpp
double total_area(const std::vector<std::unique_ptr<Shape>>& shapes) {
    double sum = 0;
    for (const auto& s : shapes)
        sum += s->area();
    return sum;
}
```

`total_area` names `Shape` and `area()`. It does not name `Circle`. A
`Triangle` added next year, in another library, works if it derives from
`Shape` and overrides `area()`. The caller recompiles only if `Shape` itself
changes.

That last sentence is the real constraint. Adding a pure virtual function to
`Shape` breaks every derived class. Adding a non-pure virtual function with a
body is source-compatible for derived classes and binary-incompatible for the
vtable layout (every slot after the insertion moves). Treat the set of
virtual functions on a stable ABI as append-only, and prefer to add behavior
in new types rather than new virtuals on old bases.

```
   callers ---> Shape::area() <--- Circle
                              <--- Rectangle
                              <--- Triangle     (added later)
```

Runnable: [`examples/ch07_shapes.cpp`](examples/ch07_shapes.cpp).

---

## 5. Interface, abstract class, concrete class

C++ has no `interface` keyword. The idiom is a class with no state, a virtual
destructor, and only pure virtual functions.

```cpp
class Drawable {
public:
    virtual void draw() const = 0;
    virtual ~Drawable() = default;
};

class Serializable {
public:
    virtual std::string serialize() const = 0;
    virtual ~Serializable() = default;
};

class Button : public Drawable, public Serializable {
public:
    void draw() const override { /* ... */ }
    std::string serialize() const override { return "Button"; }
};
```

```
   concrete class     every virtual function has a final overrider; can construct
   abstract class     at least one pure virtual remains; may have data and
                      ordinary member functions
   interface idiom    no data, all functions pure, virtual destructor
```

An abstract class with data and protected helpers is a partial implementation.
It is useful and it is dangerous. Derived classes depend on that data's
layout and on the helpers' behavior (the fragile base class). An interface
with no data has nothing for them to depend on except the function contracts.
Implementing several interfaces is the multiple inheritance that stays
boring, because there is no diamond of state. Chapter 08 covers the case
where there is.

Copy operations on an interface should be deleted or protected (chapter 04).
A public copy constructor on `Drawable` invites slicing the moment someone
writes a function that takes `Drawable` by value. If copies are part of the
concept, publish `clone()` returning `unique_ptr<Drawable>`.

---

## 6. The non-virtual interface

Make the public function non-virtual. Put the customization point in a
private or protected virtual function. The base keeps control of the
contract: preconditions, locking, logging, the order of steps. Derived
classes fill in the step they actually vary.

```cpp
class Report {
public:
    void generate(std::ostream& out) {
        write_header(out);
        write_body(out);
        write_footer(out);
    }
    virtual ~Report() = default;

private:
    void write_header(std::ostream& out) const { out << "REPORT\n"; }
    void write_footer(std::ostream& out) const { out << "END\n"; }
    virtual void write_body(std::ostream& out) const = 0;
};

class SalesReport : public Report {
    void write_body(std::ostream& out) const override {
        out << "sales\n";
    }
};
```

`SalesReport` overrides `write_body` even though it is private. Access is
checked at the call site, not at the override. `SalesReport` cannot call
`write_body` on another `Report` (it is private), and it can still override
it. That is what you want: the hook is not part of the public surface, and
it is not a way for one report to drive another report's internals.

This is the Template Method pattern (chapter 13) with the call direction made
explicit. The public function is the stable name. The virtual function is an
implementation detail you can rename only as carefully as any virtual, but
callers of `generate` do not see it.

NVI is also the right place for the default-argument problem from chapter 06.
The public non-virtual function has the default argument and forwards to a
private virtual function that takes the argument explicitly:

```cpp
class Parser {
public:
    void parse(std::string_view text, int flags = 0) { parse_impl(text, flags); }
private:
    virtual void parse_impl(std::string_view text, int flags) = 0;
};
```

There is one default, on the function that is not dispatched.

---

## 7. Preconditions and the contract of an override

An override is a refinement, not a new function. Callers who hold a `Shape&`
are entitled to everything `Shape::area` promised.

```
   An override may weaken preconditions (accept more inputs).
   An override may strengthen postconditions (promise more about the result).
   An override may not strengthen preconditions (reject inputs the base accepted).
   An override may not weaken postconditions (return less than the base promised).
   An override may not throw exceptions the base said it would not throw.
```

`Square::set_width` that also changes the height breaks a postcondition of
`Rectangle::set_width` ("the height is unchanged"). That is a Liskov
violation, and it is why the hierarchy is the wrong model (chapter 12).
`SimplePrinter::scan` that throws "not supported" breaks `Machine::scan`.
Split the interface (chapter 12, interface segregation) instead of stubbing
the function.

`noexcept` on a virtual function is part of this story. A base virtual that
is `noexcept` forces every override to be `noexcept`. A base that is not
`noexcept` allows an override to add `noexcept`, which is a stronger promise
and is allowed.

---

## 8. Factories return the interface

Concrete classes can stay out of headers that clients include:

```cpp
std::unique_ptr<Shape> make_circle(double r);
std::unique_ptr<Shape> make_shape(std::string_view spec);
```

The definitions, in a `.cpp`, name `Circle`. Clients depend on `Shape` and on
the factory signature. Chapter 13 separates simple factory, factory method,
and abstract factory; the ownership rule is the same. Return `unique_ptr` to
the interface. Do not return a raw owning pointer. Do not return `Shape` by
value.

---

## 9. Concepts are the compile-time version of an interface

A base class is a runtime contract. A C++20 concept is a compile-time
constraint on a type, with no vtable and no inheritance:

```cpp
template <class T>
concept HasArea = requires(const T& t) {
    { t.area() } -> std::convertible_to<double>;
};

template <HasArea T>
double total(const std::vector<T>& items) {
    double sum = 0;
    for (const auto& x : items) sum += x.area();
    return sum;
}
```

`total` accepts any type with `area()`, and it does not accept a
heterogeneous vector. `Circle` and `Rectangle` are different `T`s, so they
need different instantiations, or they need to share a base, or they need to
sit in a `variant`. Concepts do not replace virtual functions. They replace
the documentation that used to live in a comment above a template, and they
replace a certain amount of CRTP scaffolding (chapter 23).

Use a concept when the algorithm can be generic and the type is known at the
call. Use an abstract class when the type is chosen at run time.

---

## 10. Worked example

```cpp
#include <iostream>
#include <memory>
#include <vector>

class Shape {
public:
    virtual double area() const = 0;
    virtual const char* name() const = 0;
    virtual ~Shape() = default;

protected:
    Shape() = default;
    Shape(const Shape&) = default;
    Shape& operator=(const Shape&) = default;
};

class Circle : public Shape {
    double r_;
public:
    explicit Circle(double r) : r_(r) {}
    double area() const override { return 3.141592653589793 * r_ * r_; }
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

The protected copy operations let the compiler generate `Circle`'s copy (used
by `make_unique` only indirectly; `Circle` is concrete and copyable) while
`Shape s = *shapes[0]` outside the hierarchy fails because `Shape`'s copy
constructor is protected. `Shape` itself stays abstract because of the pure
functions, so that line would also fail for abstractness. The protected copy
matters for the day someone gives `Shape` a body for every virtual function
and it stops being abstract.

---

## 11. Exercises

1. `struct B { virtual void f() = 0; }; void B::f() {}` — can you create a
   `B`? Can a derived class call `B::f()`?
2. Why does a pure virtual destructor still need a function body?
3. `write_body` in the NVI example is private. How does `SalesReport`
   override it, and why can `SalesReport` not call `r.write_body(out)` on
   some other `Report& r`?
4. You need `total_area` over a `vector` that mixes circles and rectangles,
   and you will never add a third shape. What are the two honest designs,
   and which one avoids a vptr?

### Answers

1. You cannot create a `B`. The class is abstract. A derived override can
   call `B::f()` and run the provided body.
2. Derived destructors call the base destructor. `= 0` does not supply that
   definition. Without `Interface::~Interface()`, destroying a derived
   object fails at link time.
3. Overriding is not a call. Access is not checked on the override.
   `write_body` is private in `Report`, so a member of `SalesReport` has no
   right to call it on a `Report`. It may call it on itself only if access
   allows; here the public entry point is `generate`.
4. `vector<unique_ptr<Shape>>` with virtual `area()`, or
   `vector<variant<Circle, Rectangle>>` with `visit`. The variant stores
   values, has no vptr in `Circle`, and rejects a forgotten alternative at
   compile time. The virtual design stays open. For a forever-closed pair,
   the variant is the one that avoids a vptr.

---

## 12. Summary

<!--diagram
title: Abstract classes & interfaces
box[green] Key points
  text: "= 0" makes a function pure and the class abstract. Derived classes that leave it pure stay abstract. Pointers and references to the abstract type are fine
  text: A pure virtual destructor still needs a definition. A pure virtual may have a body, reached only by a qualified call
  text: An interface idiom is a stateless pure-abstract class. Several of them are the usual reason for multiple inheritance
  text: NVI keeps the public function non-virtual and the hook private. Overrides must honor the base contract
  text: Concepts constrain templates at compile time. They do not provide a heterogeneous container
-->
```
 +-------------------------------------------------------------------+
 | = 0 => pure virtual => abstract class. Objects: no. Pointers: yes.|
 | A pure virtual destructor must still be defined. A body on a pure |
 |   virtual is opt-in via Base::f(), not an automatic default.      |
 | Interface idiom: no data, all pure, virtual destructor.           |
 | NVI: public non-virtual wrapper, private virtual hook.            |
 | Overrides honor base pre- and postconditions.                     |
 | Concepts are the static analogue. variant is the closed analogue. |
 +-------------------------------------------------------------------+
```

Next: [08-multiple-virtual-inheritance.md](08-multiple-virtual-inheritance.md).
