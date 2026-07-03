# 05 — Inheritance

Inheritance builds a new class (**derived**) from an existing one (**base**),
modeling an **"is-a"** relationship and enabling reuse. This chapter covers the
mechanics; polymorphism (the *reason* to inherit, usually) is chapter 06.

Prereq: [03-encapsulation.md](03-encapsulation.md).

---

## 1. Basic syntax and the "is-a" relationship

```cpp
class Animal {                     // base class (a.k.a. superclass / parent)
public:
    void breathe() { /* ... */ }
    std::string name;
};

class Dog : public Animal {        // derived class (subclass / child)
public:
    void bark() { /* ... */ }
};
```

```
   Dog IS-A Animal.
        Animal (base)
          ^  public inheritance
          |
        Dog (derived) -- adds bark(), inherits breathe() and name

   Dog d;
   d.breathe();   // inherited from Animal
   d.bark();      // Dog's own
   d.name = "Rex";// inherited data member
```

```
   The "is-a" test: "Every Dog IS-A(n) Animal." If that sentence is false, do
   NOT use public inheritance (use composition, chapter 10). Classic mistake:
   "Stack is-a vector" — no; Stack HAS-A vector.
```

---

## 2. Object layout under inheritance

A derived object **contains** a base subobject, followed by its own members.

```cpp
class Base    { int a; };
class Derived : public Base { int b; };
```

<!--diagram
title: Derived object layout
box[blue] Derived object in memory
  box[teal] Base subobject
    text: `int a`
  box[purple] Derived members
    text: `int b`
  text: A `Derived*` can be treated as a `Base*` (Base part at the front) — upcasting (§5)
-->
```
   Derived object in memory:
   +------------------+  <- start of Derived (and of the Base subobject)
   |  Base:  int a    |
   +------------------+
   |  Derived: int b  |
   +------------------+

   A Derived* can be treated as a Base* (the Base part is at the front) ->
   this is why 'Base* p = &derived;' works (upcasting, §5).
```

---

## 3. The three inheritance access modes

`public`, `protected`, `private` inheritance change how the base's members are
seen *through the derived class*.

```cpp
class Pub  : public    Base {};   // is-a; base's public->public, protected->protected
class Prot : protected Base {};   // base's public & protected -> protected
class Priv : private   Base {};   // base's public & protected -> private
```

```
   Base member  | public inh. | protected inh. | private inh.
   -------------+-------------+----------------+-------------
   public       | public      | protected      | private
   protected    | protected   | protected      | private
   private      | (inaccessible in all cases)

   public inheritance   = "is-a" (the common, meaningful one)
   private inheritance  = "implemented-in-terms-of" (like composition; rare)
   protected inheritance= even rarer; usually a design smell
```

```
   99% of the time you want PUBLIC inheritance. Private inheritance is an
   alternative to composition when you need access to protected members or to
   override virtuals — prefer a member (composition) unless you need those.
```

---

## 4. Construction & destruction order with inheritance

```
   Constructing a Derived:
     1. Base constructor runs first (the base subobject is built)
     2. Derived's members initialized (declaration order)
     3. Derived constructor body runs
   Destroying: EXACT REVERSE
     3. Derived destructor body
     2. Derived members destroyed (reverse order)
     1. Base destructor runs last
```

```cpp
struct Base    { Base(){puts("Base ctor");}    ~Base(){puts("Base dtor");} };
struct Derived : Base { Derived(){puts("Derived ctor");} ~Derived(){puts("Derived dtor");} };

// Derived d;  prints:
//   Base ctor        <- base built first
//   Derived ctor
//   Derived dtor     <- derived torn down first
//   Base dtor        <- base last
```

You pass arguments to the base constructor via the initializer list:

```cpp
class Employee : public Person {
public:
    Employee(std::string name, double salary)
        : Person(std::move(name)),   // call the Base constructor explicitly
          salary_(salary) {}
private:
    double salary_;
};
```

```
   : Person(std::move(name))   <- initialize the BASE subobject.
   If you don't call a base ctor explicitly, the base's DEFAULT ctor is used
   (and it's an error if the base has no default ctor).
```

Runnable: [`examples/ch05_ctor_order.cpp`](examples/ch05_ctor_order.cpp).

---

## 5. Upcasting and downcasting

```cpp
Dog d;
Animal* ap = &d;      // UPCAST: Derived* -> Base*  (always safe, implicit)
Animal& ar = d;       // upcast via reference too

// Downcast (Base* -> Derived*) — needs a cast; may be wrong:
Dog* dp1 = static_cast<Dog*>(ap);    // you ASSERT it's really a Dog (unchecked)
Dog* dp2 = dynamic_cast<Dog*>(ap);   // CHECKED at runtime (needs polymorphism);
                                     // returns nullptr if ap isn't a Dog
```

```
   UPCAST (Derived -> Base): safe & implicit. A Dog IS an Animal.
        [ Animal | Dog ]
          ^ap points here (the Animal part) — always valid.

   DOWNCAST (Base -> Derived): risky. Is this Animal actually a Dog?
     static_cast : trust me (no check) -> UB if wrong.
     dynamic_cast: verify at runtime -> nullptr (ptr) / throws (ref) if wrong.
   dynamic_cast requires a POLYMORPHIC base (has a virtual function, chapter 06).
```

---

## 6. Name hiding & `using`

A name in the derived class **hides** *all* base overloads of that name — even
ones with different signatures.

```cpp
struct Base {
    void f(int)    { puts("Base::f(int)"); }
    void f(double) { puts("Base::f(double)"); }
};
struct Derived : Base {
    void f(std::string) { puts("Derived::f(string)"); }   // HIDES both Base::f!
};

Derived d;
d.f("hi");     // Derived::f(string)
// d.f(42);    // ERROR: Base::f(int) is HIDDEN by Derived::f
d.Base::f(42); // works with explicit qualification
```

```
   Fix: bring the base overloads into scope:
     struct Derived : Base {
         using Base::f;                 // un-hide Base::f overloads
         void f(std::string) { ... }
     };
   Now d.f(42), d.f(1.0), and d.f("hi") all work.
```

---

## 7. Object slicing (a critical pitfall)

Assigning/copying a derived object into a **base value** chops off the derived
part — the "slice."

```cpp
Dog d;
Animal a = d;        // SLICING: only the Animal part is copied; 'bark' is gone
                     // 'a' is a plain Animal, not a Dog.

std::vector<Animal> zoo;
zoo.push_back(Dog{});// SLICED into an Animal — the Dog-ness is lost!
```

```
   Dog:    [ Animal part | Dog part ]
                  |  copy to Animal value
                  v
   Animal: [ Animal part ]              <- Dog part SLICED off (discarded)

   Consequence: virtual calls on 'a' use Animal's behavior, not Dog's.
   FIX: use pointers/references for polymorphism:
     std::vector<std::unique_ptr<Animal>> zoo;   // stores Dogs as Animal* -> no slicing
```

Slicing is one of the most common OOP bugs — chapter 06 (polymorphism needs
references/pointers) and chapter 15 revisit it.

---

## 8. `final` — stop further inheritance/overriding

```cpp
class Sealed final { };            // no class may inherit from Sealed
// class X : Sealed {};            // ERROR

class Widget {
    virtual void draw() final;     // no override in derived (chapter 06)
};
```

```
   final on a CLASS  -> can't be a base class.
   final on a METHOD -> can't be overridden further.
   Used to lock a design and to enable devirtualization optimizations.
```

---

## 9. Worked example: `Shape` hierarchy (mechanics only)

```cpp
#include <iostream>
#include <string>

class Shape {
public:
    Shape(std::string name) : name_(std::move(name)) {}
    const std::string& name() const { return name_; }
protected:                         // accessible to derived classes
    std::string name_;
};

class Circle : public Shape {
    double r_;
public:
    Circle(double r) : Shape("circle"), r_(r) {}
    double area() const { return 3.14159265 * r_ * r_; }
};

int main() {
    Circle c(2.0);
    std::cout << c.name() << " area = " << c.area() << '\n';  // circle area = 12.566
    Shape* s = &c;                 // upcast
    std::cout << "as shape: " << s->name() << '\n';          // circle
    // s->area();  // ERROR: area() is not part of Shape's interface (see ch 06/07)
}
```

Runnable: [`examples/ch05_shape.cpp`](examples/ch05_shape.cpp).

---

## 10. Summary

<!--diagram
title: Inheritance
box[green] Key points
  text: Inheritance models **IS-A** (use **public** inheritance). If not is-a, use composition (ch 10)
  text: A `Derived` **contains** a `Base` subobject (Base part at the front)
  text: Construct: Base → members → Derived body; destroy: reverse. Pass base-ctor args via the initializer list
  text: Upcast (`Derived`→`Base`): safe/implicit. Downcast: `dynamic_cast` (checked) or `static_cast` (unchecked)
  text: A derived name **hides** all base overloads → `using Base::f;`
  text: **SLICING**: copying `Derived` into a `Base` **value** loses the derived part → use pointers/references for polymorphism
  text: `final` seals a class or method
-->
```
 +------------------------------------------------------------------+
 | Inheritance models IS-A (use PUBLIC inheritance). If not is-a,   |
 |   use composition (chapter 10).                                  |
 | A Derived CONTAINS a Base subobject (Base part at the front).    |
 | Construct: Base -> members -> Derived body; destroy: reverse.    |
 |   Pass base-ctor args via the initializer list.                  |
 | Upcast (Derived->Base): safe/implicit. Downcast: dynamic_cast    |
 |   (checked) or static_cast (unchecked).                          |
 | A derived name HIDES all base overloads -> 'using Base::f;'.     |
 | SLICING: copying Derived into a Base VALUE loses the derived part|
 |   -> use pointers/references for polymorphism.                   |
 | 'final' seals a class or method.                                 |
 +------------------------------------------------------------------+
```

Next: [06-polymorphism-vtables.md](06-polymorphism-vtables.md).
