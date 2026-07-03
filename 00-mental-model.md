# 00 — The Mental Model: What OOP *Really* Is

Before syntax, install the right mental model. OOP is a way of **organizing code
around "objects"** — bundles of *data* together with the *operations* that act on
that data — with rules that keep the data valid. C++ gives you OOP tools but
(unlike Java/C#) does **not** force you to use them; you choose.

---

## 1. An object = state + behavior + identity

<!--diagram
title: An object
box[blue] Object
  text: **STATE** (data members): `balance = 100`, `owner = "Ada"`, `id = 42`
  text: **BEHAVIOR** (methods): `deposit()`, `withdraw()`, `statement()`
  text: **Identity**: this specific account, distinct from any other
-->
```
   +-------------------------------------------+
   |                OBJECT                     |
   |  STATE (data members)   BEHAVIOR (methods)|
   |   balance = 100          deposit()        |
   |   owner   = "Ada"        withdraw()       |
   |   id      = 42           statement()      |
   +-------------------------------------------+
        ^ identity: this specific account, distinct from any other
```

Contrast with procedural code, where data and functions are separate:

```
   PROCEDURAL                         OBJECT-ORIENTED
   ----------------------------       -------------------------------
   struct Account { double bal; };    class Account {
   void deposit(Account*, double);      double bal;              // data
   void withdraw(Account*, double);   public:
   // anyone can poke bal directly      void deposit(double);   // + behavior
                                        void withdraw(double);  // guards bal
                                      };                         // invariant safe
```

The OOP win: the object **owns and protects** its data. Callers use `deposit()`
instead of scribbling on `bal`, so the account can never enter an invalid state.

---

## 2. The four pillars

<!--diagram
title: The four pillars
box[green] Encapsulation (ch 03)
  text: Bundle data + methods; hide internals; protect invariants
box[teal] Abstraction (ch 07)
  text: Expose **WHAT**, hide **HOW**; model the essentials (interfaces)
box[purple] Inheritance (ch 05)
  text: Build new types from existing ones (is-a); reuse & extend
box[orange] Polymorphism (ch 06)
  text: One interface, many behaviors at runtime (`virtual`) or compile time
-->
```
   +----------------+   +----------------+   +----------------+   +----------------+
   | ENCAPSULATION  |   | ABSTRACTION    |   | INHERITANCE    |   | POLYMORPHISM   |
   |----------------|   |----------------|   |----------------|   |----------------|
   | bundle data +  |   | expose WHAT,   |   | build new types|   | one interface, |
   | methods; hide  |   | hide HOW; model|   | from existing  |   | many behaviors |
   | internals;     |   | the essentials |   | ones (is-a);   |   | at runtime     |
   | protect        |   | (interfaces)   |   | reuse & extend |   | (virtual) or   |
   | invariants     |   |                |   |                |   | compile time   |
   +----------------+   +----------------+   +----------------+   +----------------+
       ch 03               ch 07               ch 05               ch 06
```

- **Encapsulation** — keep data private; expose a controlled interface; maintain
  *invariants* (rules that must always hold, e.g. "balance ≥ 0").
- **Abstraction** — present a simple concept (a "Shape" that can `area()`) and
  hide the messy details of each implementation.
- **Inheritance** — a `SavingsAccount` *is-a* `Account`; reuse and specialize.
- **Polymorphism** — call `shape->area()` and get the right behavior for a
  `Circle` or `Square` without knowing which it is.

We spend a chapter on each. Keep this picture in mind — every later topic slots
into one of these pillars.

---

## 3. `class` vs `struct` in C++ (a small but important truth)

In C++ they are **the same feature**; the *only* difference is the default access
level:

```cpp
struct S { int x; };   // members default to PUBLIC
class  C { int x; };   // members default to PRIVATE
```

```
   struct  ->  default public   (convention: "plain data / no invariants")
   class   ->  default private  (convention: "has invariants / behavior")
   Everything else (methods, inheritance, ctors) works identically in both.
```

Convention: use `struct` for passive aggregates of data with no invariants; use
`class` when the type maintains invariants or has meaningful behavior.

---

## 4. C++ OOP is *value-oriented* (this surprises Java/Python people)

In C++, objects are **values** by default — they live on the stack, are copied
when assigned, and are destroyed deterministically when they go out of scope.

```cpp
Account a;        // a real object, right here on the stack
Account b = a;    // b is a COPY of a (not a reference/alias!)
b.deposit(50);    // does NOT affect a
                  // when the scope ends, a and b are destroyed automatically
```

```
   Java/Python:  variable ---> [object on heap]   (references; GC frees later)
   C++ default:  variable IS the object (a value); copy = new object;
                 destroyed deterministically at end of scope (RAII, chapter 02).
```

This value semantics is the foundation of **RAII** (chapter 02) and drives why
copy/move semantics (chapter 04) matter so much. Runtime polymorphism, by
contrast, requires **pointers or references** (chapter 06) — you can't get
virtual dispatch through a plain value.

---

## 5. When (and when not) to reach for OOP

C++ is multi-paradigm. OOP is one tool among several:

```
   Use OOP (classes + virtual) when:
     * you have entities with invariants and behavior (Account, Connection)
     * you need runtime polymorphism over a stable interface (plugins, shapes)
     * a natural is-a hierarchy exists AND you'll use it polymorphically

   Prefer alternatives when:
     * pure data -> a struct / aggregate
     * "one of a fixed set of types" -> std::variant + visit (closed set)
     * compile-time polymorphism / zero overhead -> templates (see cpp-templates)
     * a free function suffices -> just write a function
```

```
   Don't force everything into a class hierarchy. Deep inheritance trees are a
   classic over-engineering smell. "Composition over inheritance" (chapter 10)
   is the default modern advice.
```

---

## 6. The two kinds of polymorphism (preview)

```
   RUNTIME (dynamic) polymorphism        COMPILE-TIME (static) polymorphism
   -------------------------------       ----------------------------------
   virtual functions + vtable            templates / overloading / CRTP
   Shape* s = pick(); s->area();         template<class S> auto a(S s){s.area();}
   chosen at RUN time                    chosen at COMPILE time
   flexible, needs pointer/ref,          zero overhead, no pointer needed,
   small indirection cost                but no heterogeneous containers
   (chapter 06)                          (see the cpp-templates guide)
```

This guide focuses on **runtime OOP**, but chapter 14 shows modern alternatives
(`std::variant`, CRTP, type erasure) so you can pick the right tool.

---

## 7. Mental model summary card

<!--diagram
title: Mental model summary
box[green] Key points
  text: **OBJECT** = state (data) + behavior (methods) + identity
  text: Four pillars: **ENCAPSULATION**, **ABSTRACTION**, **INHERITANCE**, **POLYMORPHISM**
  text: `class` vs `struct`: only default access differs (private vs public)
  text: C++ objects are **VALUES**: copied on assignment, destroyed at scope end (RAII). Runtime polymorphism needs pointers/references
  text: OOP is one tool; prefer composition, use `variant`/templates when they fit better. Don't over-engineer hierarchies
-->
```
 +---------------------------------------------------------------------+
 | OBJECT = state (data) + behavior (methods) + identity.              |
 | Four pillars: ENCAPSULATION, ABSTRACTION, INHERITANCE, POLYMORPHISM.|
 |                                                                     |
 | class vs struct: only default access differs (private vs public).   |
 | C++ objects are VALUES: copied on assignment, destroyed at scope    |
 |   end (RAII). Runtime polymorphism needs pointers/references.       |
 |                                                                     |
 | OOP is one tool; prefer composition, use variant/templates when     |
 | they fit better. Don't over-engineer hierarchies.                   |
 +---------------------------------------------------------------------+
```

Next: [01-classes-and-objects.md](01-classes-and-objects.md).
