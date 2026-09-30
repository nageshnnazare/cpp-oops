# 00 — The Mental Model: What OOP *Really* Is

Before syntax, install the model the rest of the guide uses. C++ OOP is not
"put everything in a class." It is a way of giving a **type** a state, a
lifetime, an interface, and invariants — and of choosing, deliberately, whether
behavior is selected at compile time or at run time.

C++ gives you the tools and does not force them. A class with no virtual
functions has the same layout cost as a C struct. A virtual function is a
request for a vtable. You pay for the dynamism you ask for
([23-zero-overhead-and-idioms.md](23-zero-overhead-and-idioms.md)).

Prereq: intermediate C++ (functions, references, `const`, basic STL).

---

## 1. The C++ object model, in one page

An **object** is a region of storage with a type and a lifetime. A **class
type** (`class`, `struct`, or `union`) describes how that storage is carved
into **subobjects**:

```
   complete object          the thing you created (a local, a heap Widget, ...)
     |
     +-- base-class subobjects     the Base parts inside a Derived
     +-- member subobjects         each non-static data member
           |
           +-- their own subobjects, recursively

   A subobject is itself an object. It has its own lifetime, which is nested
   inside the complete object's lifetime.
```

<!--diagram
title: Complete object and subobjects
box[blue] Complete object: Circle c
  box[teal] Base subobject: Shape
    text: `name_`
  box[purple] Member subobject
    text: `radius_`
  text: One complete object, two subobjects, one address for the complete object
-->
```
   Circle c{2.0};          // one complete object

   +---------------------------+  <- &c  (also the address of the Shape subobject
   | Shape subobject: name_    |     when Shape is the primary base)
   +---------------------------+
   | member: radius_           |
   +---------------------------+
```

Three words people mix up:

| Word | Means |
| --- | --- |
| **Storage** | The bytes. A local's storage exists for the whole block. |
| **Lifetime** | The interval during which those bytes *are* a `Widget`. Starts when the constructor finishes. Ends when the destructor starts. |
| **Scope** | Where the *name* is visible. A name can die while a heap object's lifetime continues, and the reverse (a temporary's lifetime ends at the end of the full expression even though no name ever existed). |

Identity, in C++, is the address of the object. Two complete objects of the
same type must have distinct addresses, which is why `sizeof` of an empty class
is at least 1. Base subobjects are allowed to overlap (empty-base
optimization); C++20 `[[no_unique_address]]` extends that permission to members
([17-internals-object-layout.md](17-internals-object-layout.md)).

Non-static member **functions are not stored in the object**. A call `r.area()`
is a function call whose hidden first argument is `&r`. Only a polymorphic
class adds a hidden pointer (the vptr) to the object
([06-polymorphism-vtables.md](06-polymorphism-vtables.md)).

---

## 2. State, behavior, invariants

The useful design picture, sitting on top of that model:

<!--diagram
title: An object
box[blue] Object
  text: **STATE** (non-static data members): `balance = 100`, `owner = "Ada"`
  text: **BEHAVIOR** (the interface): `deposit()`, `withdraw()`, `operator==`, free `transfer()`
  text: **INVARIANT**: between public calls, `balance >= 0` and `owner` is non-empty
-->
```
   +-------------------------------------------+
   |                OBJECT                     |
   |  STATE                  BEHAVIOR          |
   |   balance = 100          deposit()        |
   |   owner   = "Ada"        withdraw()       |
   |  INVARIANT: balance >= 0, owner non-empty |
   +-------------------------------------------+
```

An **invariant** is a predicate the constructor establishes and every public
operation preserves. Encapsulation exists to make states that violate it
unreachable ([03-encapsulation.md](03-encapsulation.md)). Behavior is the set
of operations that are allowed to see the representation. That set is wider
than the member functions: it includes free functions found by argument-dependent
lookup, especially hidden friends such as `operator<<`
([11-static-friends-nested.md](11-static-friends-nested.md)).

Procedural code leaves the invariant as a comment. A class makes it a
constructor postcondition:

```
   PROCEDURAL                         OBJECT-ORIENTED
   ----------------------------       -------------------------------
   struct Account { double bal; };    class Account {
   void deposit(Account*, double);      // bal is private
   // anyone can write bal = -1;       public:
                                        void deposit(double);   // guards bal
                                      };
```

---

## 3. The four pillars, stated as C++ mechanisms

<!--diagram
title: The four pillars
box[green] Encapsulation (ch 03)
  text: Access control + invariants. Private data, a small public surface
box[teal] Abstraction (ch 07)
  text: A contract (`area()`) with the implementation hidden behind it
box[purple] Inheritance (ch 05)
  text: A derived object *contains* a base subobject. Public inheritance means substitutable-for
box[orange] Polymorphism (ch 06)
  text: One call, a behavior chosen at run time (`virtual`) or compile time (templates, `variant`)
-->
```
   ENCAPSULATION   access + invariants            chapter 03
   ABSTRACTION     program to a contract          chapter 07
   INHERITANCE     base subobject + substitutability   chapter 05
   POLYMORPHISM    one interface, a chosen behavior    chapter 06
```

- **Encapsulation** — `private` / `protected` / `public`, plus the discipline of
  not handing out a reference that punches through the wall.
- **Abstraction** — callers depend on `Shape::area()`, not on whether the
  radius lives in a `double` or a GPU buffer.
- **Inheritance** — `class Dog : public Animal` means a `Dog` object *contains*
  an `Animal` subobject. Public inheritance additionally promises that a `Dog`
  can stand in wherever an `Animal` is required (the Liskov substitution
  principle, [12-solid-principles.md](12-solid-principles.md)).
- **Polymorphism** — the call is written once. *Which function runs* is decided
  either by the dynamic type (virtual functions) or by the static type
  (overloads, templates, `std::visit`).

Public inheritance is a subtype relationship. Private inheritance is an
implementation technique and does **not** create a public is-a relationship
([05-inheritance.md](05-inheritance.md),
[10-composition-vs-inheritance.md](10-composition-vs-inheritance.md)).

---

## 4. `class`, `struct`, and `union`

`class` and `struct` are the same language feature. The only difference is the
default:

```cpp
struct S { int x; };   // members and bases default to public
class  C { int x; };   // members and bases default to private
```

```cpp
struct D : Base { };   // public inheritance
class  E : Base { };   // private inheritance
```

Convention in this guide: `struct` for aggregates and passive data with no
invariant; `class` when the type maintains an invariant or is meant to be a
base. The compiler does not care.

A `union` is also a class type, with a different rule: at most one non-static
data member is **active**, and the others do not have a lifetime. Reading the
inactive member is undefined behavior (the common-initial-sequence rule is the
narrow exception). `std::variant` is the safe union
([14-modern-cpp-oop.md](14-modern-cpp-oop.md)).

---

## 5. C++ is value-oriented

In C++, a variable of class type **is** the object. Assignment copies or moves.
The destructor runs when the lifetime ends, including during stack unwinding.

```cpp
Account a{"Ada", 100};
Account b = a;     // b is a distinct object, a copy of a
b.deposit(50);     // a.balance() is still 100
                   // leaving the scope runs ~b then ~a
```

```
   Java/Python:  name ---> [object on the heap]     GC decides when
   C++ default:  name IS the object (a value)
                 copy constructs a new object
                 destructor runs at the end of the lifetime (RAII)
```

This is why [04-copy-move-rule-of-five.md](04-copy-move-rule-of-five.md) is not
an advanced topic. The moment a type owns a resource, copying it is a design
decision: deep copy, move-only, or not copyable at all.

Runtime polymorphism does not work through values, because a `Shape` value has
no room for a `Circle`'s extra members (object slicing,
[05-inheritance.md](05-inheritance.md)). It works through pointers and
references, which can refer to a derived complete object:

```cpp
void draw(const Shape& s);     // binds to Circle, Square, ...
Shape s = Circle{1};           // slices: s is a Shape, the radius is gone
```

There is a third option. A **value-semantic** polymorphic type
(`std::variant`, or a type-erased wrapper) stores the concrete object inside a
fixed-size shell or behind an owning pointer, and still behaves like a value
([14-modern-cpp-oop.md](14-modern-cpp-oop.md),
[23-zero-overhead-and-idioms.md](23-zero-overhead-and-idioms.md)).

---

## 6. Four ways to get "one interface, many types"

Memorize this table. Most design arguments in the later chapters are about
which row you are on.

```
   Technique            When the set of types is    Dispatch        Heterogeneous container
   -------------------  --------------------------- --------------- ------------------------
   virtual functions    open (plugins, new .so)     indirect call   vector<unique_ptr<Base>>
   std::variant+visit   closed (all types listed)   switch / visit  vector<variant<...>>
   templates / CRTP     known at the call           inlined         no single container
   type erasure         open, non-intrusive         indirect call   vector<Drawable> by value
```

- **Virtual** — the caller holds a base pointer or reference. New derived
  classes can appear without recompiling the caller. Cost: a vptr per object
  and an indirect call ([06](06-polymorphism-vtables.md),
  [18](18-internals-vtables.md)).
- **`std::variant`** — every alternative is named in one place. Adding a type
  is a compile error at every `visit` you forgot to update. No heap, no vptr
  in your objects ([14](14-modern-cpp-oop.md)).
- **Templates and CRTP** — the compiler generates a separate function per type.
  Zero runtime dispatch. You cannot put `Circle` and `Square` in one
  homogeneous container of a shared base, because `Shape<Circle>` and
  `Shape<Square>` are unrelated types ([23](23-zero-overhead-and-idioms.md)).
- **Type erasure** — a value type such as `std::function` that can hold any
  type matching a duck-typed interface. Internally it is a virtual call. The
  stored types do not inherit from anything.

Reach for the first row only when the set of types is genuinely open or you
need a stable ABI. A closed set of three shapes does not need a class
hierarchy.

---

## 7. What belongs in a class

A class earns its existence when at least one of these is true:

```
   * It owns a resource and must release it (RAII).
   * It has an invariant that callers must not be able to break.
   * It is a polymorphic base (or a concrete type in such a hierarchy).
   * Grouping the data makes the function signatures honest.
```

A class is the wrong tool when:

```
   * The data has no invariant. Use an aggregate struct.
   * The operation does not need access to private state. Write a free function
     in the type's namespace (it will be found by ADL).
   * You want reuse of some code and the "is-a" sentence is false. Compose
     (chapter 10).
   * The set of alternatives is fixed. Use std::variant.
   * You are about to add a Manager, a Handler, and a Helper that only forward
     calls. Delete a layer.
```

The interface of a well-designed value type is often mostly free functions and
operators, with a small set of private members and a few essential methods.
`std::string` is a class; `std::getline` is not a member, and that is correct,
because `getline` needs to be a friend of the stream as well as the string.

---

## 8. The lifetime timeline you will keep re-deriving

For a single object with members and one base, construction is:

```
   1. virtual bases, most-derived class initializes them (chapter 08)
   2. direct non-virtual bases, left to right
   3. members, in declaration order (not initializer-list order)
   4. constructor body
```

Destruction is the exact reverse. If a constructor throws, every subobject
whose constructor completed is destroyed, and the object's own destructor does
**not** run — there is no object yet
([02-constructors-destructors.md](02-constructors-destructors.md)).

During a base constructor, the dynamic type is the base. Virtual calls do not
reach the derived override
([06-polymorphism-vtables.md](06-polymorphism-vtables.md)).

---

## 9. The bugs this guide is built around

If you remember nothing else, remember that these are undefined behavior or
silent logic bugs, not style nits:

```
   delete through a base pointer whose destructor is not virtual
   copying a derived object into a base value (slicing)
   a compiler-generated copy of a type that owns a raw pointer (double free)
   calling a virtual function in a constructor and expecting the override
   a default argument on a virtual function (bound to the static type)
   a shared_ptr cycle
   a moved-from object used as if it still held its old value
   returning a reference to a local or to a temporary
   memcpy of a type that is not trivially copyable
   treating a pointer-to-member-function as an ordinary function pointer
```

Each one has a chapter. [15-pitfalls-and-best-practices.md](15-pitfalls-and-best-practices.md)
collects them with the fixes.

---

## 10. How the rest of the guide is organized

Chapters 01–16 are the language and the design. They include the rules, the
failure mode, and a runnable example. Chapters 17–23 are the ABI: what GCC and
Clang actually emit (Itanium C++ ABI, the one used on Linux and macOS). MSVC
differs in representation details; the concepts transfer.

Build any example with:

```bash
clang++ -std=c++20 -Wall -Wextra -Wnon-virtual-dtor examples/ch06_polymorphism.cpp -o /tmp/demo && /tmp/demo
```

`examples/ch14_deducing_this.cpp` needs `-std=c++23`.

---

## 11. Exercises

1. A `Config` struct has three public ints and no invariant. Should it be a
   `class` with getters? What do you gain?
2. `void log(const Account& a)` and `void log(Account a)` both compile when
   you pass a `SavingsAccount`. Which one can still see that it is a savings
   account, and why?
3. You need to store circles, rectangles, and (later, from a plugin) shapes
   you have not written yet. Which row of the table in §6 applies? Which row
   applies if the plugin requirement disappears and the three shapes are the
   whole set forever?

### Answers

1. Leave it an aggregate `struct`. Getters add a function-call syntax and no
   invariant. Designated initializers and structured bindings work on the
   aggregate (chapter 01).
2. The `const Account&` parameter refers to the complete `SavingsAccount`
   object. A virtual call on it can reach the override. The by-value
   parameter copy-constructs an `Account`, which slices off the derived part.
3. Plugins: virtual functions, owned by `unique_ptr<Shape>`. A forever-closed
   set: `std::variant<Circle, Rectangle, Triangle>`.

---

## 12. Summary

<!--diagram
title: Mental model summary
box[green] Key points
  text: An object is storage plus a type plus a lifetime. A class object contains base and member subobjects
  text: Invariants are what encapsulation protects. The interface includes members, friends, and ADL free functions
  text: `class` and `struct` differ only in default access. C++ objects are values; runtime polymorphism needs a pointer, a reference, or a type-erasing value
  text: Pick virtual, variant, templates, or type erasure to match whether the set of types is open or closed
  text: Construction is bases, then members in declaration order, then the body. Destruction is the reverse
-->
```
 +---------------------------------------------------------------------+
 | Object = storage + type + lifetime. Class objects contain base and  |
 |   member subobjects. Lifetime starts when the ctor finishes.        |
 | Invariants are why encapsulation exists. The interface is members   |
 |   plus friends plus ADL free functions.                             |
 | class vs struct: default access only. Objects are values. Runtime   |
 |   polymorphism needs a pointer, a reference, or a type-erasing value.|
 | virtual / variant / templates / type erasure: pick by open vs closed.|
 | Ctor: bases, members (declaration order), body. Dtor: reverse.      |
 +---------------------------------------------------------------------+
```

Next: [01-classes-and-objects.md](01-classes-and-objects.md).
