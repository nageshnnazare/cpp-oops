# 08 — Multiple & Virtual Inheritance

A class may have more than one direct base. Each base is a distinct
subobject, at its own offset, with its own constructor and destructor. That
is useful for implementing several interfaces. It is also the way you
accidentally create two copies of a common base. Virtual inheritance is the
tool that merges those copies into one, and it has a real cost.

Prereq: [07-abstract-classes-interfaces.md](07-abstract-classes-interfaces.md).
The pointer adjustments, thunks, and construction vtables are chapter 20.

---

## 1. Multiple bases, multiple subobjects

```cpp
class Camera {
public:
    void snap() {}
};
class Phone {
public:
    void call() {}
};
class Smartphone : public Camera, public Phone {
public:
    void use() { snap(); call(); }
};
```

```
   Smartphone object
   +------------------+  <- Smartphone*, Camera*     (primary base, offset 0)
   | Camera subobject |
   +------------------+  <- Phone*                   (a different address)
   | Phone subobject  |
   +------------------+
   | Smartphone members
   +------------------+
```

`Smartphone` is a `Camera` and a `Smartphone` is a `Phone`. Both conversions
are implicit. The conversion to `Phone*` adds the offset of the `Phone`
subobject. A `reinterpret_cast<Phone*>(static_cast<Smartphone*>(p))` skips
that adjustment and points at the `Camera` subobject. Use the language
conversion.

The good use of this layout is interfaces with no data:

```cpp
class Serializable {
public:
    virtual std::string save() const = 0;
    virtual ~Serializable() = default;
};
class Drawable {
public:
    virtual void draw() const = 0;
    virtual ~Drawable() = default;
};

class Widget : public Serializable, public Drawable {
public:
    std::string save() const override { return "widget"; }
    void draw() const override {}
};
```

Two abstract bases, no shared state, no diamond. `Widget` has two vptrs if
both bases are polymorphic, because each polymorphic base subobject carries
its own. That is usually one extra pointer, and it is the right trade for a
class that genuinely implements two contracts. Callers who have a `Drawable*`
do not see `save`, which is interface segregation (chapter 12) realized as
layout.

---

## 2. Ambiguity

If the same name is found in two bases along unrelated paths, lookup is
ambiguous and the program is ill-formed until you disambiguate.

```cpp
class A { public: void f(); int x; };
class B { public: void f(); int x; };
class C : public A, public B {};

C c;
c.A::f();
c.B::x = 1;
// c.f();     // error: A::f or B::f?
```

The fix at the call is qualification. The fix in the class, when one of the
two should be the `C` interface, is a using-declaration or a forwarding
function:

```cpp
class C : public A, public B {
public:
    using A::f;            // C::f means A::f; B::f is still available as B::f
};
```

Ambiguous conversions are the same problem for pointers:

```cpp
struct L { int x; };
struct R { int x; };
struct D : L, R {};
D d;
// int* p = &d.x;         // error: which x?
int* p = &d.L::x;
```

---

## 3. The diamond

Non-virtual inheritance of a common base duplicates that base.

```cpp
class Animal { public: std::string name; };
class Mammal : public Animal {};
class Winged : public Animal {};
class Bat : public Mammal, public Winged {};
```

```
              Animal                 Animal
                 \                     /
                 Mammal            Winged
                      \            /
                          Bat

   Bat layout, non-virtual:
   +---------------------------+
   | Mammal's Animal::name     |
   +---------------------------+
   | Winged's Animal::name     |
   +---------------------------+
   | Bat members               |
   +---------------------------+
```

`b.name` is ambiguous. `b.Mammal::name` and `b.Winged::name` are different
strings. A `Bat` does not have one name. The conversion from `Bat*` to
`Animal*` is also ambiguous: there are two `Animal` subobjects, and the
compiler will not pick one.

Sometimes the duplication is what you want. An input iterator and an output
iterator embedded in a bidirectional iterator, in some designs, are
independent subobjects. State that you mean it. Most diamonds are accidents.

---

## 4. Virtual inheritance merges the shared base

```cpp
class Animal { public: std::string name; };
class Mammal : public virtual Animal {};
class Winged : public virtual Animal {};
class Bat : public Mammal, public Winged {};

Bat b;
b.name = "Bruce";     // one Animal::name
```

```
   Bat layout, virtual inheritance (shape, not a promise of offsets):
   +---------------------------+
   | Mammal part               |
   +---------------------------+
   | Winged part               |
   +---------------------------+
   | Bat members               |
   +---------------------------+
   | shared Animal             |   one subobject, location depends on the
   +---------------------------+   most-derived class
```

`Mammal` and `Winged` do not contain an `Animal` at a compile-time-fixed
offset anymore. They contain enough information (a vbase offset in the
vtable, on the Itanium ABI) to *find* the `Animal` inside whatever complete
object they are part of. A standalone `Mammal` and a `Mammal` subobject of a
`Bat` place `Animal` in different places. The same `Mammal` member function
works in both, because it loads the offset instead of adding a constant.

That load is the runtime cost. Virtual inheritance also makes construction
heavier (section 5) and makes the type non-standard-layout. Use it when two
classes must share one base subobject. Do not use it as a precaution on
every base.

Runnable: [`examples/ch08_diamond.cpp`](examples/ch08_diamond.cpp).

---

## 5. The most-derived class initializes the virtual base

A virtual base is constructed once, by the **most-derived** class, before any
non-virtual base. Mem-initializers for that virtual base in intermediate
classes are ignored when the intermediate class is not the complete object.

```cpp
class Animal {
public:
    explicit Animal(std::string n) : name(std::move(n)) {}
    std::string name;
};
class Mammal : public virtual Animal {
public:
    Mammal() : Animal("mammal-default") {}     // ignored when constructing a Bat
};
class Winged : public virtual Animal {
public:
    Winged() : Animal("winged-default") {}     // ignored when constructing a Bat
};
class Bat : public Mammal, public Winged {
public:
    explicit Bat(std::string n) : Animal(std::move(n)) {}
};
```

`Bat b("Bruce")` runs `Animal(std::string)`, then `Mammal()`, then
`Winged()`, then `Bat`'s body. The `Animal(...)` expressions in `Mammal` and
`Winged` do not run. If `Bat` does not initialize `Animal` and `Animal` has
no default constructor, `Bat`'s constructor is ill-formed.

```
   Order for a most-derived class D:
     1. Virtual bases, depth-first, left to right, as they appear in the
        base-specifier lists of D and of everything D inherits. Each virtual
        base once.
     2. Direct non-virtual bases, left to right in D's base-clause.
     3. Members of D, declaration order.
     4. D's constructor body.

   Destruction is the reverse. Virtual bases are destroyed last, by the
   most-derived destructor.
```

A virtual base with no explicit initializer uses its default constructor,
and that default must exist. This is why virtual bases are often
default-constructible abstract interfaces: there is nothing to pass.

---

## 6. Dominance

Virtual inheritance can make a name that looks ambiguous actually be fine.
If one declaration **dominates** the others, lookup selects it.

A declaration in a derived class dominates a declaration in a base when every
path to the base declaration goes through the class that has the derived
declaration. In practice: an override in one intermediate class beats the
original in the shared virtual base.

```cpp
struct V {
    virtual void f() { std::puts("V"); }
};
struct A : virtual V {
    void f() override { std::puts("A"); }
};
struct B : virtual V {};
struct D : A, B {};

D d;
d.f();            // "A"  — A::f dominates V::f; B does not declare f
```

Without `A::f`, `d.f()` would still be unambiguous: there is one `V`
subobject and one `V::f`. Dominance matters when one path *overrides* and
another path merely inherits. The override wins. You do not write `d.A::f()`
to resolve it.

Dominance does not merge two unrelated functions. `A::f` and `B::f`, both
declared in the intermediate classes, are still ambiguous if they are
different functions. Virtual inheritance does not pick a winner between
siblings.

---

## 7. Casting across the diamond

```cpp
struct A { virtual ~A() = default; int ax; };
struct B { virtual ~B() = default; int bx; };
struct D : A, B {};

D d;
A* a = &d;
B* b = dynamic_cast<B*>(a);    // cross-cast: sibling bases of the same D
```

`static_cast<B*>(a)` is ill-formed. `A` and `B` are not in a base-derived
relationship with each other. `dynamic_cast` walks from the most-derived
object and can move sideways. It needs a polymorphic source. Chapter 21 is
the walk.

`dynamic_cast<Animal*>(mammal_ptr)` inside a virtual hierarchy adjusts to the
single shared `Animal`, using the vbase offset. A `static_cast` can perform
that adjustment too when the static path is unambiguous. It cannot check that
the object really is a `Bat`.

---

## 8. When to use which

```
   several stateless interfaces              public multiple inheritance
   one shared polymorphic subobject          virtual inheritance of that base
     (a common "Object" or "EnableShared")
   reuse of implementation                   a member, or private inheritance
   a diamond you did not draw on purpose     stop and delete a base
```

`std::enable_shared_from_this<T>` is a virtual-base-shaped problem in user
code only if two bases both derive from it. Derive from it once, on the
most-derived class (chapter 14).

Mixin bases that are templates (`template <class D> struct Printable`) are
not a diamond: each instantiation is a different base type. CRTP mixins
(chapter 23) are the usual way to stack compile-time behavior without
virtual bases.

The standard library almost never asks you to inherit from its types.
`std::vector` as a base is the running example of the wrong relationship
(chapter 10), and its destructor is not virtual.

---

## 9. Worked example

```cpp
#include <iostream>
#include <string>
#include <utility>

struct Animal {
    std::string name;
    explicit Animal(std::string n) : name(std::move(n)) {}
};
struct Mammal : virtual Animal {
    bool warm_blooded = true;
    Mammal() : Animal("unset") {}
};
struct Winged : virtual Animal {
    double wingspan = 0;
    Winged() : Animal("unset") {}
};
struct Bat : Mammal, Winged {
    Bat(std::string n, double span) : Animal(std::move(n)) {
        wingspan = span;
    }
};

int main() {
    Bat b("Bruce", 0.3);
    std::cout << b.name << " span=" << b.wingspan
              << " warm=" << std::boolalpha << b.warm_blooded << '\n';
}
```

`Animal("unset")` in `Mammal` and `Winged` does not run for `b`. `Bat`'s
mem-initializer does. If you removed `: Animal(std::move(n))` from `Bat`,
the program would not compile: `Animal` has no default constructor, and the
most-derived class is responsible for calling one.

---

## 10. Exercises

1. `Widget` derives publicly from `Drawable` and `Serializable`, both
   abstract and stateless. How many vptrs does a `Widget` typically have,
   and why is that acceptable?
2. `Mammal` and `Winged` both non-virtually derive from `Animal`, and `Bat`
   derives from both. Why is `Animal* p = &bat;` ill-formed?
3. Both intermediate classes virtually inherit `Animal`, and only `Mammal`
   overrides `virtual void speak()`. Is `bat.speak()` ambiguous?
4. Who calls `Animal`'s constructor when you construct a `Bat`, and what
   happens to the `Animal(...)` initializer written in `Mammal`?

### Answers

1. Two, on the Itanium ABI: one per polymorphic base subobject. Callers can
   hold either interface without seeing the other, and neither base carries
   state. The extra pointer is the cost of two independent contracts.
2. There are two `Animal` subobjects. The conversion does not know which
   address to produce.
3. No. `Mammal::speak` dominates `Animal::speak`. `Winged` does not declare
   `speak`. Lookup selects the override.
4. `Bat`, the most-derived class, calls it, before `Mammal`'s constructor
   body. `Mammal`'s mem-initializer for `Animal` is not executed when the
   complete object is a `Bat`.

---

## 11. Summary

<!--diagram
title: Multiple & virtual inheritance
box[green] Key points
  text: Each direct base is a subobject. A conversion to a secondary base changes the address. Stateless interfaces are the usual reason to have several
  text: A non-virtual diamond duplicates the shared base and makes the upcast ambiguous. Virtual inheritance keeps one shared subobject, found through a runtime offset
  text: The most-derived constructor initializes each virtual base once. Intermediate initializers for that base do not run. Virtual bases are destroyed last
  text: Dominance lets an override on one path win over the virtual-base declaration. It does not choose between two sibling declarations
  text: dynamic_cast can cross-cast between sibling bases. static_cast cannot invent a relationship that is not there
-->
```
 +------------------------------------------------------------------+
 | Multiple bases = multiple subobjects. Secondary-base casts       |
 |   adjust the pointer. Interfaces: a good reason. Stateful        |
 |   implementation inheritance: usually a member instead.          |
 | Non-virtual diamond: two subobjects, ambiguous upcast.           |
 | Virtual base: one subobject, runtime offset, non-standard-layout.|
 | Most-derived ctor initializes virtual bases; intermediates'      |
 |   initializers for them are skipped. Destroy virtual bases last. |
 | Dominance picks an override over the shared base declaration.    |
 | Cross-casts need dynamic_cast.                                   |
 +------------------------------------------------------------------+
```

Next: [09-operator-overloading.md](09-operator-overloading.md).
