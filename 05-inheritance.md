# 05 — Inheritance

Inheritance embeds a **base-class subobject** inside a derived object. Public
inheritance additionally promises that the derived object can be used where
the base is expected. Those are different claims. The first is a layout fact.
The second is a design constraint (the Liskov substitution principle), and it
is the only good reason to inherit publicly.

Prereq: [03-encapsulation.md](03-encapsulation.md). Polymorphism, which is
why you usually bother, is [06-polymorphism-vtables.md](06-polymorphism-vtables.md).

---

## 1. The is-a test

```cpp
class Animal {
public:
    void breathe() {}
    std::string name;
};

class Dog : public Animal {
public:
    void bark() {}
};
```

```
   Dog d;
   d.breathe();      // Animal's function, called with this pointing at d
   d.bark();
   d.name = "Rex";

        Animal
          ^  public
          |
         Dog
```

"Every `Dog` is an `Animal`" has to stay true for every public function of
`Animal`, with the same preconditions and postconditions. If the sentence is
false, do not use public inheritance. A stack is not a vector. A square with
independent `set_width` / `set_height` is not a rectangle
([10-composition-vs-inheritance.md](10-composition-vs-inheritance.md),
[12-solid-principles.md](12-solid-principles.md)).

Reusing some code is not a reason to inherit. Composition reuses code without
publishing the base interface.

---

## 2. Layout: a derived object contains a base

```cpp
struct Base { int a; };
struct Derived : Base { int b; };
```

```
   Derived object
   +------------------+  <- Derived* and Base* (single inheritance, no virtuals)
   |  Base::a         |
   +------------------+
   |  Derived::b      |
   +------------------+
```

Under single inheritance the base subobject is at offset 0, so the conversion
from `Derived*` to `Base*` does not change the address. Multiple inheritance
breaks that assumption: a pointer to a secondary base points *into* the
object, and the cast adjusts the address
([20-internals-multiple-inheritance.md](20-internals-multiple-inheritance.md)).
Do not `reinterpret_cast` between base and derived. Use the implicit derived-to-base
conversion, or `static_cast` / `dynamic_cast`, which apply the adjustment.

An empty base may occupy no bytes (empty base optimization). A polymorphic
base places a vptr at offset 0 of that subobject (Itanium ABI) and the object
grows by a pointer. Chapter 17 is the full layout story.

---

## 3. Access in the base-clause

The base-clause access specifier controls two things: how the base's public
and protected members are re-exported, and who may implicitly convert a
derived pointer or reference to a base pointer or reference.

```
   base member   public inheritance   protected inheritance   private inheritance
   ------------  -------------------  ----------------------  -------------------
   public        public               protected               private
   protected     protected            protected               private
   private       inaccessible everywhere, inheritance does not open it

   implicit Derived* -> Base* is available:
     public inheritance      everywhere
     protected inheritance   in Derived, further derived, and their friends
     private inheritance     in Derived and its friends only
```

```cpp
class Pub  : public    Base {};
class Prot : protected Base {};
class Priv : private   Base {};
```

Public inheritance is the is-a relationship. Private inheritance means
"implemented in terms of": `Derived` can use `Base`'s protected members and
can override `Base`'s virtual functions, and callers cannot treat a `Derived`
as a `Base`. That is almost always better written as a member. Reach for
private inheritance when you need an override or access to a protected
function and a member cannot provide it (the empty-base optimization used to
be the other reason; `[[no_unique_address]]` covers that now).

Protected inheritance is rare. It says "my derived classes may treat me as a
`Base`, and the outside world may not."

Most derived classes should say `: public Base` explicitly even in a
`struct`, so a later edit that changes `struct` to `class` does not silently
flip the inheritance to private.

---

## 4. Construction and destruction

```
   Derived construction:
     1. virtual bases, by the most-derived constructor (chapter 08)
     2. direct bases, left to right
     3. members, declaration order
     4. derived constructor body
   Destruction is the reverse. The derived destructor body runs first, the
   base destructor last.
```

```cpp
struct Base {
    Base() { std::puts("Base"); }
    ~Base() { std::puts("~Base"); }
};
struct Derived : Base {
    Derived() { std::puts("Derived"); }
    ~Derived() { std::puts("~Derived"); }
};
// Derived d;   prints: Base  Derived  ~Derived  ~Base
```

Pass arguments to the base constructor in the initializer list. If you omit
it, the base's default constructor is used, and the program is ill-formed if
there is none.

```cpp
class Employee : public Person {
public:
    Employee(std::string name, double salary)
        : Person(std::move(name)), salary_(salary) {}
private:
    double salary_;
};
```

Base subobjects initialize before members, in the order the bases are listed,
not the order they appear in the initializer list. A member must not be used
to initialize a base.

Order across several bases and the virtual-base rule are in chapter 08.
Runnable: [`examples/ch05_ctor_order.cpp`](examples/ch05_ctor_order.cpp).

---

## 5. Upcasts, downcasts, cross-casts

```cpp
Dog d;
Animal* ap = &d;                     // implicit upcast, always safe
Animal& ar = d;

Dog* dp1 = static_cast<Dog*>(ap);    // you are asserting the dynamic type
Dog* dp2 = dynamic_cast<Dog*>(ap);   // checked; requires a polymorphic Animal
```

An implicit conversion from `Derived*` to `Base*` exists when the inheritance
path is accessible and unambiguous. It adjusts the pointer if the base is not
at offset 0.

`static_cast` from `Base*` to `Derived*` is valid only when the object really
is a `Derived` (or the pointer is null). If you are wrong, the behavior is
undefined. It does not check. It can downcast. It cannot cross-cast from one
sibling base to another when the path is not direct; that conversion is not
in the static type system.

`dynamic_cast` checks at run time. On failure, a pointer cast returns null
and a reference cast throws `std::bad_cast`. The source type must be
polymorphic (have at least one virtual function), because the check reads the
RTTI pointer out of the vtable. `dynamic_cast<void*>` yields a pointer to the
most-derived object. Chapter 21 is the algorithm.

```cpp
Dog* dp = dynamic_cast<Dog*>(ap);
if (!dp) { /* ap was not a Dog */ }
```

Prefer a virtual function over a downcast. A downcast means the caller is
doing work the object should have done itself. When you do need one (a
plugin boundary, a test), `dynamic_cast` is the one that fails closed.

---

## 6. Name hiding is not overloading and not overriding

A name declared in the derived class hides **every** base declaration of that
name, including overloads with different signatures. Lookup stops when it
finds the name. Overload resolution never sees the hidden functions.

```cpp
struct Base {
    void f(int);
    void f(double);
};
struct Derived : Base {
    void f(std::string);     // hides both Base::f overloads
};

Derived d;
d.f("hi");          // Derived::f(string)
d.Base::f(42);      // ok, qualified
// d.f(42);         // error: Base::f(int) is hidden
```

Bring the base overloads back into the overload set with a using-declaration:

```cpp
struct Derived : Base {
    using Base::f;
    void f(std::string);
};
```

Now `f(int)`, `f(double)`, and `f(std::string)` are overloaded. A
using-declaration can also change access: `using Base::helper;` in the public
section of `Derived` re-exports a public or protected base name at public
access. It cannot widen a private base member; private names are not
accessible to name in the using-declaration.

This is unrelated to `virtual`. Overriding requires the same signature (with
the covariant-return exception in chapter 06). A derived function with a
*different* signature hides; it does not override, even if you write
`virtual` on it. `override` turns that mistake into a compile error.

---

## 7. Inheriting constructors, from the derived side

```cpp
struct Base {
    explicit Base(int);
    Base(std::string);
};
struct Derived : Base {
    using Base::Base;
    int flag_ = 1;
};
```

Chapter 02 lists the rules. The inheritance-specific ones:

```
   * Default, copy, and move constructors are not inherited. Derived gets
     its own, generated under the usual rules.
   * Derived members are initialized from default member initializers, not
     by any logic in a base constructor.
   * An inherited constructor does not give you a place to write Derived's
     body. If you need a body, write a real constructor.
   * If Base's constructor is explicit, the inherited one is explicit.
```

---

## 8. Object slicing

Copying a derived object into a **base value** copies only the base
subobject. The derived members are not there. Virtual calls on the result
use the base, because the dynamic type *is* the base.

```cpp
Dog d;
Animal a = d;                 // a is an Animal, not a Dog

std::vector<Animal> zoo;
zoo.push_back(Dog{"Rex"});    // sliced on insert

void feed(Animal a);          // by value: slices the argument
feed(d);
```

```
   Dog:     [ Animal subobject | Dog members ]
                      |
                      |  copy-construct an Animal
                      v
   Animal:  [ Animal subobject ]
```

Any of these fixes, depending on what you meant:

```cpp
void feed(const Animal& a);                         // no copy, virtual calls work
std::vector<std::unique_ptr<Animal>> zoo;           // owns the complete object
zoo.push_back(std::make_unique<Dog>("Rex"));

std::vector<std::variant<Dog, Cat>> pets;           // closed set, no slicing
```

Slicing is not a downcast bug. It is a copy. The type system accepts it
because a `Dog` *is* an `Animal` and `Animal`'s copy constructor is public.
Delete or protect the base copy operations if the hierarchy should not be
sliced (chapter 04, the `clone` pattern).

Passing by `const Animal&` does not slice. Storing `Animal` by value does.

---

## 9. `final`

```cpp
class Sealed final {};
// class X : Sealed {};        // error

class Widget {
public:
    virtual void draw() final;
};
```

`final` on a class forbids further derivation. `final` on a virtual function
forbids further overriding. Both are design locks and optimization hints:
when the compiler knows a class is `final`, a call on that type can be
devirtualized (chapter 18).

Apply `final` to concrete leaves unless you have a reason to leave the
hierarchy open. An open hierarchy is a promise that you will preserve the
base contracts for strangers.

---

## 10. What derived classes do not get

```
   * Friendship is not inherited. A friend of Base is not a friend of Derived,
     and a friend of Derived is not a friend of Base.
   * Private members of Base are not accessible in Derived. Protected members
     are, subject to the object-expression rule in chapter 03.
   * Constructors and destructors are not inherited as ordinary functions.
     using Base::Base is the explicit opt-in, with the limits above.
   * A using-declaration of a base assignment operator does not become
     Derived's copy assignment. Special members stay special.
```

Overriding a virtual function does not inherit a default argument. Default
arguments are bound to the static type of the call. That trap is in chapter
06 because it only bites once the function is virtual.

---

## 11. Worked example

```cpp
#include <iostream>
#include <string>
#include <utility>

class Shape {
public:
    explicit Shape(std::string name) : name_(std::move(name)) {}
    const std::string& name() const { return name_; }
    virtual ~Shape() = default;
private:
    std::string name_;
};

class Circle : public Shape {
public:
    explicit Circle(double r) : Shape("circle"), r_(r) {}
    double area() const { return 3.141592653589793 * r_ * r_; }
private:
    double r_;
};

int main() {
    Circle c(2.0);
    std::cout << c.name() << " area = " << c.area() << '\n';
    const Shape& s = c;                 // no slice: s refers to the Circle
    std::cout << "as shape: " << s.name() << '\n';
    // s.area();                        // error: area() is not in Shape
}
```

`area()` is not visible through `Shape` because it was never declared there.
Chapter 06 puts it on the base as a virtual function. Chapter 07 makes it
pure. The hierarchy in this chapter is only the subobject relationship.

`name_` is private, not protected. `Circle` does not reach into it; it uses
`name()`. If a derived class needs a different name, the base constructor
argument is the extension point. Protected data would let `Circle` write
`name_` and skip whatever checks `Shape` grows later.

Runnable: [`examples/ch05_shape.cpp`](examples/ch05_shape.cpp).

---

## 12. Exercises

1. `class Stack : public std::vector<int> {}` compiles and `s.insert(...)`
   works. Why is the type wrong even though the code is legal?
2. A function takes `Base` by value. You pass a `Derived`. Inside the
   function, a virtual call resolves to `Base`. Explain it as a copy, not as
   a failed virtual dispatch.
3. `Derived` declares `void f(int)`. `Base` declares `void f(double)`.
   `d.f(1.0)` does not call `Base::f`. What declaration fixes the overload
   set, and what does a qualified call `d.Base::f(1.0)` do instead?
4. When does a `Derived*` to `Base*` conversion change the pointer's address?

### Answers

1. Public inheritance publishes the whole `vector` interface, so callers can
   break LIFO. `Stack` is not substitutable for `vector` either, if you
   expected stack behavior from every inherited function. Hold a `vector`
   as a member. Also, `std::vector` does not have a virtual destructor;
   deleting a `Stack` through a `vector*` is undefined.
2. The parameter is a distinct `Base` object, copy-constructed from the
   `Base` subobject of your `Derived`. The `Derived` part was never copied.
   The dynamic type of the parameter is `Base`, so the virtual call is
   correct for the object that actually exists inside the function.
3. `using Base::f;` inside `Derived` puts both functions in the overload set.
   `d.Base::f(1.0)` does not fix the set; it bypasses it and calls `Base::f`
   directly.
4. When `Base` is not at offset 0 of `Derived`: a secondary base in multiple
   inheritance, or a virtual base whose location depends on the most-derived
   type. Single non-virtual inheritance keeps the primary base at offset 0.

---

## 13. Summary

<!--diagram
title: Inheritance
box[green] Key points
  text: A derived object contains a base subobject. Public inheritance means substitutable-for. Private inheritance is an implementation detail
  text: Construct bases, then members, then the body. Pass base arguments in the initializer list. Destruction is the reverse
  text: Upcasts are implicit and may adjust the address. static_cast downcasts without a check. dynamic_cast checks and can cross-cast
  text: A derived name hides every base overload of that name. using Base::f restores them. Hiding is not overriding
  text: Copying a derived object into a base value slices. Use a reference, unique_ptr, or variant. final seals a class or a virtual function
-->
```
 +------------------------------------------------------------------+
 | Derived contains a Base subobject. Public inheritance = is-a     |
 |   and must stay substitutable. Private inheritance = implemented |
 |   in terms of.                                                    |
 | Ctor: bases, members, body. Dtor: reverse.                        |
 | Upcast may adjust the pointer. static_cast down is unchecked.    |
 |   dynamic_cast checks and can cross-cast.                        |
 | A derived name hides all base overloads. using Base::f; restores.|
 | Copy into a base value slices. Store pointers, references, or    |
 |   variant. final seals a leaf or an override.                    |
 +------------------------------------------------------------------+
```

Next: [06-polymorphism-vtables.md](06-polymorphism-vtables.md).
