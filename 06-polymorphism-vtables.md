# 06 — Polymorphism & Virtual Functions (with vtables)

Polymorphism — "one interface, many forms" — is *the* payoff of inheritance. Call
`shape->area()` through a base pointer and get the *derived* behavior. This
chapter explains `virtual`, how it works under the hood (**vtables**), and the
`override`/`final` keywords.

Prereq: [05-inheritance.md](05-inheritance.md).

---

## 1. The problem: non-virtual calls use the *static* type

```cpp
struct Animal { std::string sound() { return "..."; } };
struct Dog : Animal { std::string sound() { return "Woof"; } };  // NOT virtual

Dog d;
Animal* a = &d;
a->sound();     // "..."  <- calls Animal::sound! (based on the POINTER's type)
```

```
   Without 'virtual', the compiler picks the function by the STATIC type of the
   expression (Animal*), not the DYNAMIC type of the object (Dog).
   -> a->sound() == Animal::sound()  even though it points at a Dog. Not useful.
```

---

## 2. `virtual` enables dynamic dispatch

Mark the function `virtual` in the base; now the call resolves by the **dynamic
type** of the actual object.

```cpp
struct Animal {
    virtual std::string sound() const { return "..."; }   // virtual!
    virtual ~Animal() = default;                            // virtual dtor (§7)
};
struct Dog : Animal {
    std::string sound() const override { return "Woof"; }  // override
};
struct Cat : Animal {
    std::string sound() const override { return "Meow"; }
};

Animal* a = new Dog();
a->sound();     // "Woof"  <- resolves to Dog::sound at RUNTIME. 
```

```
   virtual -> the call is dispatched based on the OBJECT'S REAL type:
     Animal* -> Dog object  => Dog::sound()
     Animal* -> Cat object  => Cat::sound()
   ONE line of code, MANY behaviors. This is runtime polymorphism.
```

```
   for (Animal* a : {mk_dog(), mk_cat(), mk_dog()})
       std::cout << a->sound();          // Woof Meow Woof
   The caller doesn't know or care which concrete type it has.
```

Runnable: [`examples/ch06_polymorphism.cpp`](examples/ch06_polymorphism.cpp).

---

## 3. How it works: the vtable and vptr

For each polymorphic class the compiler builds a **vtable** (virtual table): an
array of function pointers to the class's virtual functions. Each object gets a
hidden **vptr** pointing to its class's vtable.

```
   Class vtables (one per class, shared):
     Dog::vtable        Cat::vtable
     +-------------+    +-------------+
     | &Dog::sound |    | &Cat::sound |
     | &Animal::~  |    | &Animal::~  |
     +-------------+    +-------------+

   Objects (each carries a hidden vptr):
     dog: [ vptr -> Dog::vtable | ...Dog data... ]
     cat: [ vptr -> Cat::vtable | ...Cat data... ]

   a->sound()  with a = &dog:
     1. read dog.vptr        -> Dog::vtable
     2. load slot[0]         -> &Dog::sound
     3. call it              -> "Woof"
   Two pointer dereferences + an indirect call. That's the cost of 'virtual'.
```

```
   Non-virtual call:  direct call to a known address (can inline).
   Virtual call:      load vptr -> load slot -> indirect call (cannot inline
                      unless the compiler proves the dynamic type).

   Size cost: a polymorphic object is larger by one pointer (the vptr).
     struct Empty {};                sizeof == 1
     struct Poly { virtual ~Poly(); }; sizeof == 8 (the vptr) on 64-bit
```

Runnable: [`examples/ch06_vtable.cpp`](examples/ch06_vtable.cpp) (prints sizes to
show the vptr overhead).

---

## 4. `override` — say what you mean (always use it)

`override` tells the compiler "this is meant to override a base virtual." If it
doesn't actually override one (typo, wrong signature, missing `const`), you get a
**compile error** instead of a silent bug.

```cpp
struct Base { virtual void f(int) const; };

struct Good : Base { void f(int) const override; };   // OK: really overrides

struct Bad : Base {
    void f(int) override;         // ERROR: not const -> doesn't match Base::f
    void f(long) override;        // ERROR: different param -> not an override
    void g() override;            // ERROR: Base has no virtual g()
};
```

```
   WITHOUT override, 'void f(int)' (non-const) would silently create a NEW
   function that HIDES nothing and is never called polymorphically -> the classic
   "why isn't my override being called?" bug.
   RULE: put 'override' on every intended override. It's free insurance.
```

---

## 5. `final` on virtual functions

```cpp
struct Base    { virtual void f(); };
struct Mid : Base { void f() final; };   // Mid::f is the last word
struct Leaf : Mid { void f(); };         // ERROR: can't override a final f()
```

```
   final = "no further overrides." Locks the behavior and lets the compiler
   devirtualize (turn virtual calls into direct calls) when it knows the type.
```

---

## 6. Calling the base version explicitly

```cpp
struct Derived : Base {
    void f() override {
        Base::f();          // call the base implementation first...
        // ...then add derived behavior
    }
};
```

```
   Base::f()  <- qualified call bypasses virtual dispatch and runs Base's version.
   Common for extending (not replacing) base behavior.
```

---

## 7. The virtual destructor rule (CRITICAL)

**If a class is meant to be used polymorphically (deleted through a base
pointer), its destructor MUST be virtual.** Otherwise `delete base_ptr` only runs
the base destructor — the derived part leaks or is destroyed incorrectly (UB).

```cpp
struct Base { /* no virtual dtor! */ ~Base() { puts("~Base"); } };
struct Derived : Base { std::string big; ~Derived() { puts("~Derived"); } };

Base* p = new Derived();
delete p;              // prints only "~Base" -> ~Derived NEVER runs -> UB/leak!
```

```
   delete (Base*) pointing to Derived, with a NON-virtual dtor:
     -> only ~Base runs. Derived's members (big) not destroyed -> LEAK / UB.

   FIX: make the base destructor virtual:
     struct Base { virtual ~Base() = default; };
   Now delete p runs ~Derived THEN ~Base (correct order).
```

```
   +---------------------------------------------------------------+
   | If a class has ANY virtual function, give it a virtual dtor.  |
   | If you'll delete derived objects via base pointers, virtual   |
   | dtor is MANDATORY. When in doubt for a base class: virtual ~. |
   +---------------------------------------------------------------+
```

Runnable: [`examples/ch06_virtual_dtor.cpp`](examples/ch06_virtual_dtor.cpp).

---

## 8. Pure virtual preview & abstract classes

A `virtual f() = 0;` is a **pure virtual** function — no (required) body,
making the class **abstract** (can't be instantiated). That's chapter 07.

```cpp
struct Shape {
    virtual double area() const = 0;   // pure virtual -> Shape is abstract
    virtual ~Shape() = default;
};
// Shape s;             // ERROR: abstract
```

---

## 9. Don't call virtuals from constructors/destructors

During Base's constructor, the object is *not yet* a Derived, so virtual calls
resolve to **Base's** version (the vptr points to Base's vtable at that moment).

```cpp
struct Base {
    Base() { init(); }                 // calls Base::init, NOT Derived::init!
    virtual void init() { puts("Base::init"); }
};
struct Derived : Base {
    void init() override { puts("Derived::init"); }
};
// Derived d;  prints "Base::init"  (surprising!)
```

```
   During Base::Base(), the Derived part doesn't exist yet -> vptr = Base's.
   So virtual dispatch gives Base::init. Same in destructors (reverse).
   RULE: avoid calling virtual functions in constructors/destructors.
```

---

## 10. Cost & when to use runtime polymorphism

```
   COST of virtual: +1 vptr per object, an indirect call (usually cheap, but
     not inlinable), and it disables some optimizations.
   USE it when: you need a heterogeneous collection behind one interface, or
     runtime-selected behavior (plugins, shapes, strategies).
   AVOID it when: the type set is fixed & known (std::variant, chapter 14) or
     you need zero overhead (templates/CRTP — see cpp-templates guide).
```

```
   Runtime polymorphism (virtual)   vs   std::variant + visit (closed set)
     open set of types                    fixed set of types
     heap + base pointers                 value semantics, no heap
     indirect call                        visit (often devirtualized)
   Chapter 14 compares these in depth.
```

---

## 11. Summary

<!--diagram
title: Polymorphism & virtual functions
box[green] Key points
  text: `virtual` → calls dispatch on the **object's dynamic type** (through base pointer/reference). Without it, the **static** type decides (no polymorphism)
  text: Mechanism: per-class **vtable** of function ptrs + per-object **vptr**. Cost: one extra pointer per object + an indirect call
  text: Always write `override` (catches signature mistakes). `final` locks a method/class and enables devirtualization
  text: **Virtual destructor** is mandatory for polymorphic base classes (else `delete`-through-base = UB/leak)
  text: Don't call virtuals from ctors/dtors (resolve to current class)
-->
```
 +------------------------------------------------------------------+
 | 'virtual' -> calls dispatch on the OBJECT'S dynamic type         |
 |   (through base pointer/reference). Without it, the STATIC type  |
 |   decides (no polymorphism).                                     |
 | Mechanism: per-class VTABLE of function ptrs + per-object VPTR.  |
 |   Cost: one extra pointer per object + an indirect call.         |
 | Always write 'override' (catches signature mistakes).            |
 | 'final' locks a method/class and enables devirtualization.       |
 | VIRTUAL DESTRUCTOR is mandatory for polymorphic base classes     |
 |   (else delete-through-base = UB/leak).                          |
 | Don't call virtuals from ctors/dtors (resolve to current class). |
 +------------------------------------------------------------------+
```

Next: [07-abstract-classes-interfaces.md](07-abstract-classes-interfaces.md).
