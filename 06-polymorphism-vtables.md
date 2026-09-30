# 06 — Polymorphism & Virtual Functions

Polymorphism here means a call written against a base is dispatched to the
function that belongs to the **dynamic type** of the object. In C++ that is
what `virtual` does. The mechanism is a per-class table of function pointers
and a per-object pointer to that table. This chapter is the language rules.
The ABI layout of the table is
[18-internals-vtables.md](18-internals-vtables.md).

Prereq: [05-inheritance.md](05-inheritance.md).

---

## 1. Static type decides, unless the function is virtual

```cpp
struct Animal {
    std::string sound() const { return "..."; }
};
struct Dog : Animal {
    std::string sound() const { return "Woof"; }   // hides Animal::sound
};

Dog d;
Animal* a = &d;
a->sound();     // "..."   the static type of a is Animal*
```

Lookup uses the static type of the expression. `Animal` has a function
`sound`, it is not virtual, and the compiler emits a direct call to
`Animal::sound`. The fact that the complete object is a `Dog` is irrelevant.
`Dog::sound` is a different function that hides the base name when the
expression has static type `Dog`.

---

## 2. `virtual` selects on the dynamic type

```cpp
struct Animal {
    virtual std::string sound() const { return "..."; }
    virtual ~Animal() = default;
};
struct Dog : Animal {
    std::string sound() const override { return "Woof"; }
};
struct Cat : Animal {
    std::string sound() const override { return "Meow"; }
};

Animal* a = new Dog();
a->sound();     // "Woof"
```

The call is resolved at run time from the object `a` points at. The same
source line calls `Dog::sound` or `Cat::sound` depending on that object.
References work the same way. Values do not: a `Animal` parameter would be a
sliced `Animal` object, and the dynamic type would be `Animal`.

```cpp
void speak_all(const std::vector<Animal*>& animals) {
    for (const Animal* a : animals)
        std::cout << a->sound() << '\n';
}
```

The caller of `speak_all` does not name `Dog` or `Cat`. That is the point of
the indirection, and it is also why the set of animals is open: a new derived
class works without editing `speak_all`.

Runnable: [`examples/ch06_polymorphism.cpp`](examples/ch06_polymorphism.cpp).

---

## 3. What the compiler emits

For each polymorphic class the compiler builds a **vtable**: a table of
function pointers, one slot per virtual function, in an order fixed by the
ABI (declaration order, base slots first, so an override occupies the same
index as the function it overrides). Each object carries a hidden **vptr**
that points at its class's table.

```
   Dog vtable (one per class)          Cat vtable
   +------------------+                 +------------------+
   | &Dog::sound      |                 | &Cat::sound      |
   | &Dog::~ (D1/D0)  |                 | &Cat::~          |
   +------------------+                 +------------------+

   dog object: [ vptr --> Dog vtable | dog members ]
   cat object: [ vptr --> Cat vtable | cat members ]

   a->sound(), with a pointing at dog:
     load a->vptr
     load slot 0
     call it, with this = a
```

A non-virtual call is a direct call to a known address and can be inlined. A
virtual call is two dependent loads and an indirect branch. The per-object
cost is one pointer, regardless of how many virtual functions the class has.
Further virtual functions add slots to the vtable, not fields to the object.

```cpp
struct Empty {};
struct Poly { virtual ~Poly(); };
// sizeof(Empty) == 1
// sizeof(Poly)  == sizeof(void*)   on Itanium, typically 8
```

Adding the first virtual function also makes the type non-standard-layout.
Do not add a virtual function to a type you `memcpy` or share with C.

Runnable: [`examples/ch06_vtable.cpp`](examples/ch06_vtable.cpp).

There is a second, quieter effect. During construction the vptr does not
point at the final class. Section 9 is that rule; chapter 18 shows the stores
the compiler inserts.

---

## 4. Signatures, `override`, and `final`

An override must match the base virtual function:

```
   name
   parameter types
   cv-qualifiers (const, volatile)
   ref-qualifiers (&, &&)
   exception specification is not part of the match in the way people fear,
     but a looser nothrow specification on an override is constrained
```

The return type matches, or it is covariant (section 5).

`override` means "this must override something." If it does not, the program
is ill-formed. The keyword is not optional in a codebase that wants the
compiler to catch typos.

```cpp
struct Base {
    virtual void f(int) const;
};

struct Good : Base {
    void f(int) const override;          // ok
};

struct Bad : Base {
    void f(int) override;                // error: drops const, would hide
    void f(long) const override;         // error: parameter type differs
    void g() const override;             // error: Base has no virtual g
};
```

Without `override`, `void f(int)` in `Bad` is a perfectly legal new function.
Calls through `Base*` still hit `Base::f`. The derived function is never
entered. That bug is silent, and it is the reason `override` exists.

You may repeat `virtual` on the override. It changes nothing. `override` is
the one that checks. A function that overrides stays virtual in further
derived classes whether or not they write `virtual`.

```cpp
struct Mid : Base {
    void f(int) const final;
};
struct Leaf : Mid {
    void f(int) const override;          // error: Mid::f is final
};
```

`final` on a function stops further overrides. `final` on a class stops
further derivation. Both let the compiler devirtualize calls it can prove
have a unique target.

A member function template cannot be virtual. Virtual dispatch needs a fixed
slot; a template is a family of functions. `static` member functions cannot
be virtual either: there is no object and no vptr.

---

## 5. Covariant return types

The override may return a pointer or reference to a class derived from the
base's return type, with the same or stronger cv-qualification.

```cpp
struct Shape {
    virtual Shape* clone() const = 0;
    virtual ~Shape() = default;
};
struct Circle : Shape {
    Circle* clone() const override { return new Circle(*this); }
};
```

`Circle::clone` overrides `Shape::clone` even though the return types differ.
A call through `Shape*` has static return type `Shape*`. A call on a
`Circle&` has static return type `Circle*`. The implementation typically
returns the `Circle*` and adjusts at the call site if the caller asked for a
`Shape*`.

Covariant returns apply to raw pointers and to lvalue references. They do not
apply to `unique_ptr<Circle>` versus `unique_ptr<Shape>`: those are
unrelated class types. Smart-pointer factories return the base pointer type
and are still the better ownership model; you just do not get the covariant
conversion for free:

```cpp
virtual std::unique_ptr<Shape> clone() const = 0;
// override also returns unique_ptr<Shape>, and make_unique<Circle>(*this)
```

There are no virtual constructors. `clone` (or a factory) is the substitute:
a virtual function that constructs the dynamic type.

---

## 6. Default arguments are static

Default arguments are substituted by the compiler using the **static** type
of the call. They are not stored in the vtable.

```cpp
struct Base {
    virtual void f(int x = 1) { std::cout << "Base " << x << '\n'; }
};
struct Derived : Base {
    void f(int x = 2) override { std::cout << "Derived " << x << '\n'; }
};

Derived d;
Base& b = d;
b.f();       // prints "Derived 1"   — Derived::f, Base's default argument
d.f();       // prints "Derived 2"
```

The function that runs is the override. The missing argument is filled in as
if the base declared it. Do not change default arguments in overrides. Better:
do not put default arguments on virtual functions. Use an overload that
forwards to the virtual function with an explicit argument, inside the
non-virtual public interface (chapter 07).

Runnable: [`examples/ch06_traps.cpp`](examples/ch06_traps.cpp).

---

## 7. Qualified calls do not dispatch

```cpp
struct Derived : Base {
    void f() override {
        Base::f();       // direct call to Base::f, no vtable
    }
};
```

`Base::f()` is a non-virtual call. Use it when the override wants to extend
the base behavior rather than replace it. A qualified call from outside,
`d.Base::f()`, also suppresses dispatch. That is occasionally what a test
wants and almost never what production code wants.

---

## 8. The virtual destructor

Deleting an object through a pointer to base has undefined behavior if the
base destructor is not virtual and the static type differs from the dynamic
type.

```cpp
struct Base { ~Base() { std::puts("~Base"); } };
struct Derived : Base {
    std::string big;
    ~Derived() { std::puts("~Derived"); }
};

Base* p = new Derived();
delete p;     // undefined: ~Derived does not run
```

The fix, when deletion through the base is part of the contract:

```cpp
struct Base {
    virtual ~Base() = default;
};
```

Then `delete p` runs `~Derived` and then `~Base`. Chapter 19 shows why: the
vtable holds a *deleting destructor* for the dynamic type, and `delete`
calls that slot.

The companion rule: if a class has any virtual function, give it a virtual
destructor, unless you have deliberately forbidden polymorphic deletion. The
way to forbid it is a protected non-virtual destructor:

```cpp
class Interface {
public:
    virtual void draw() = 0;
protected:
    ~Interface() = default;     // delete through Interface* will not compile
};
```

External code cannot call the destructor, so it cannot `delete` an
`Interface*`. The owner deletes the concrete object, or deletes through a
derived type whose destructor is public and which destroys the base in the
normal order. This is the right shape for an interface that is never an
owning pointer's static type.

Runnable: [`examples/ch06_virtual_dtor.cpp`](examples/ch06_virtual_dtor.cpp).

---

## 9. Virtual calls in constructors and destructors

While `Base::Base()` is running, the `Derived` constructor has not started.
The object is a `Base`. The vptr points at `Base`'s vtable. A virtual call
resolves to `Base`'s function, not to the override.

```cpp
struct Base {
    Base() { init(); }                       // calls Base::init
    virtual void init() { std::puts("Base"); }
};
struct Derived : Base {
    void init() override { std::puts("Derived"); }
};
// Derived d;   prints Base
```

In the destructor the order is reversed. `~Derived` runs first and then the
vptr is set back to `Base` before `~Base`'s body. A virtual call in `~Base`
does not enter `Derived`, whose members have already been destroyed. That is
the safe outcome. It is still the wrong place to structure your code.

A call from a constructor to a **pure** virtual function of the class under
construction is undefined behavior. In practice the slot holds
`__cxa_pure_virtual` and the program aborts (chapter 18).

Do the work that depends on the dynamic type after the complete object
exists: in the derived constructor body, or in a function the caller invokes
on the finished object. Do not use a virtual call to "initialize the derived
part" from the base constructor. The derived part does not exist yet, and its
members are not initialized.

---

## 10. `constexpr` virtual functions

Since C++20 a virtual function may be `constexpr`. If the dynamic type is
known while evaluating a constant expression, the override is called at
compile time. If it is not known, the call is an ordinary virtual call at run
time. This does not remove the vptr from the object. It lets a polymorphic
hierarchy participate in compile-time evaluation when the concrete type is
right there in the expression.

---

## 11. When a virtual call is the wrong tool

```
   open set of types, runtime extension, stable ABI     virtual
   closed set known at the variant's declaration        std::variant + visit
   one known type at the call site, zero overhead       templates or CRTP
   non-intrusive value type over unrelated types        type erasure
```

A virtual call inhibits inlining unless the compiler devirtualizes it
(local variable of concrete type, `final`, or link-time proof). In a hot
loop over a homogeneous collection, a template will be faster because there
is nothing to devirtualize. In a plugin host, a template cannot see the
plugin. Chapter 14 and chapter 23 compare the four rows properly.

Calling `dynamic_cast` in a loop to recover the derived type, then branching,
reimplements a worse vtable. Add a virtual function.

---

## 12. Exercises

1. `Base` declares `virtual void f() const;` and `Derived` declares
   `void f();` with no `override`. Which function does `Base& b = d; b.f();`
   call, and why does adding `override` change the program?
2. `b.f()` where `f` has a default argument `1` in `Base` and `2` in
   `Derived`, and `b` has static type `Base&` and dynamic type `Derived`.
   Which body runs, and which argument does it see?
3. Why is `delete base_ptr` undefined when `~Base` is non-virtual and the
   object is a `Derived`? What are the two deliberate alternatives?
4. Why does `Base::Base()` calling a virtual `log()` not reach
   `Derived::log`?

### Answers

1. `Base::f`. `Derived::f` is a different, non-const function; it hides
   nothing useful and does not override. `override` makes that declaration
   ill-formed, so you fix the signature (`const`) and then the call
   dispatches.
2. `Derived::f` runs with `x == 1`. The function is virtual. The default
   argument is taken from the static type `Base`.
3. The static type does not match the dynamic type, so the delete expression
   does not call the derived destructor. `virtual ~Base() = default` if
   owning `Base*` is the contract. A protected non-virtual destructor if
   polymorphic deletion should not compile.
4. During `Base`'s constructor the dynamic type is `Base`. The vptr points
   at `Base`'s vtable. `Derived`'s constructor has not run, and `Derived`'s
   members are not initialized.

---

## 13. Summary

<!--diagram
title: Polymorphism & virtual functions
box[green] Key points
  text: virtual dispatches on the dynamic type through a base pointer or reference. A value has already been sliced
  text: One vptr per object, one vtable per class. override checks the signature. final seals an override or a class
  text: Covariant returns are raw pointers and references only. Default arguments bind to the static type
  text: A base deleted polymorphically needs a virtual destructor. A protected non-virtual destructor forbids that delete
  text: In a constructor or destructor the dynamic type is the class currently running. Pure virtual calls there are undefined
-->
```
 +------------------------------------------------------------------+
 | virtual: dynamic type, via pointer or reference.                 |
 | vptr per object, vtable per class. First virtual also costs      |
 |   standard-layout.                                               |
 | Write override. final seals. Templates cannot be virtual.        |
 | Covariant return: pointer or reference to a more derived type.   |
 | Default arguments use the static type. Avoid them on virtuals.   |
 | Polymorphic delete requires a virtual destructor, or a protected |
 |   non-virtual one to forbid the delete.                          |
 | Ctors and dtors dispatch to the class currently running.         |
 +------------------------------------------------------------------+
```

Next: [07-abstract-classes-interfaces.md](07-abstract-classes-interfaces.md).
