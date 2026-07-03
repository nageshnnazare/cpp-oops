# 08 — Multiple & Virtual (Diamond) Inheritance

C++ is one of the few mainstream languages allowing a class to inherit from
**multiple** bases. This is powerful (implementing several interfaces) but has a
famous trap — the **diamond problem** — solved with **virtual inheritance**. This
chapter makes both crystal clear.

Prereq: [07-abstract-classes-interfaces.md](07-abstract-classes-interfaces.md).

---

## 1. Multiple inheritance basics

```cpp
class Camera   { public: void snap();   };
class Phone    { public: void call();   };

class Smartphone : public Camera, public Phone {  // inherit from BOTH
public:
    void use() { snap(); call(); }                // has both capabilities
};
```

<!--diagram
title: Smartphone object layout
box[blue] Smartphone
  box[teal] Camera subobject
    text: Camera part
  box[purple] Phone subobject
    text: Phone part
  box[gray] Smartphone members
    text: Smartphone part
-->
```
        Camera        Phone
           \           /
            \         /
           Smartphone       <- IS-A Camera AND IS-A Phone

   Smartphone object layout:
   +------------------+  <- Camera subobject
   |  Camera part     |
   +------------------+  <- Phone subobject
   |  Phone part      |
   +------------------+
   |  Smartphone part |
   +------------------+
```

The most common, safest use is implementing **multiple interfaces** (pure
abstract classes with no data — chapter 07):

```cpp
class Serializable { public: virtual std::string save() const = 0; virtual ~Serializable()=default; };
class Drawable     { public: virtual void draw() const = 0;         virtual ~Drawable()=default; };

class Widget : public Serializable, public Drawable {   // implements both
public:
    std::string save() const override { return "..."; }
    void draw() const override { /* ... */ }
};
```

```
   Multiple inheritance of INTERFACES (no state) = clean & recommended.
   Multiple inheritance of STATEFUL classes = where trouble can start (next).
```

---

## 2. Ambiguity from multiple bases

If two bases share a member name, the derived class must disambiguate:

```cpp
class A { public: void f(); int x; };
class B { public: void f(); int x; };
class C : public A, public B {};

C c;
// c.f();      // ERROR: ambiguous — A::f or B::f?
c.A::f();      // disambiguate explicitly
c.B::x = 1;
```

```
   Two bases with the same name -> the compiler can't choose -> you qualify:
     c.A::f()   or   c.B::f()
   Same for data members.
```

---

## 3. The Diamond Problem

When two bases themselves derive from a common base, the common base is
**duplicated** in the most-derived object.

```cpp
class Animal      { public: std::string name; };
class Mammal : public Animal {};
class WingedAnimal : public Animal {};
class Bat : public Mammal, public WingedAnimal {};   // DIAMOND
```

<!--diagram
title: Bat layout (non-virtual inheritance)
box[red] Diamond problem — Animal duplicated
  box[orange] Mammal subobject
    text: Animal #1 (via Mammal)
  box[orange] WingedAnimal subobject
    text: Animal #2 (via WingedAnimal)
  box[blue] Bat part
  text: `b.name` is **ambiguous** — two `Animal::name` subobjects
-->
```
              Animal                 Animal appears TWICE in Bat!
             /      \
        Mammal    WingedAnimal
             \      /
              Bat

   Bat object layout (NON-virtual inheritance):
   +--------------------------+
   | Mammal:  [Animal #1]     |   <- one copy of Animal (via Mammal)
   +--------------------------+
   | WingedAnimal: [Animal #2]|   <- ANOTHER copy of Animal (via WingedAnimal)
   +--------------------------+
   | Bat part                 |
   +--------------------------+

   Bat b;
   // b.name = "Bruce";   // ERROR: ambiguous — Mammal's Animal::name or
   //                     //        WingedAnimal's Animal::name?
   b.Mammal::name = "Bruce";        // must pick one copy
   b.WingedAnimal::name = "Wayne";  // ...and now there are TWO names. Bug-prone!
```

```
   The diamond gives you TWO Animal subobjects -> duplicated data, ambiguous
   access. Usually NOT what you want (a Bat has ONE name).
```

---

## 4. Virtual inheritance solves the diamond

Declare the shared base `virtual` in the intermediate classes. Now there's a
**single, shared** base subobject.

```cpp
class Animal      { public: std::string name; };
class Mammal        : public virtual Animal {};   // virtual inheritance
class WingedAnimal  : public virtual Animal {};   // virtual inheritance
class Bat : public Mammal, public WingedAnimal {};
```

<!--diagram
title: Bat layout (virtual inheritance)
box[green] Virtual inheritance — one shared Animal
  box[teal] Mammal part
  box[teal] WingedAnimal part
  box[blue] Bat part
  box[green] Animal (shared, once)
    text: Exactly one `Animal`, referenced by both paths — `b.name` is unambiguous
-->
```
              Animal   (ONE shared copy)
             /  ^  \
            /   |   \
        Mammal  |  WingedAnimal
             \  |  /
              Bat

   Bat object layout (VIRTUAL inheritance):
   +--------------------------+
   | Mammal part              |
   +--------------------------+
   | WingedAnimal part        |
   +--------------------------+
   | Bat part                 |
   +--------------------------+
   | Animal (SHARED, once)    |  <- exactly one Animal, referenced by both paths
   +--------------------------+

   Bat b;
   b.name = "Bruce";   // OK now — there's only ONE Animal::name. Unambiguous.
```

Runnable: [`examples/ch08_diamond.cpp`](examples/ch08_diamond.cpp).

---

## 5. Who constructs the virtual base? The most-derived class

With virtual inheritance, the **most-derived class** is responsible for
initializing the shared virtual base directly (bypassing the intermediates).

```cpp
class Animal {
public:
    Animal(std::string n) : name(std::move(n)) {}
    std::string name;
};
class Mammal : public virtual Animal {
public:
    Mammal() : Animal("default") {}   // this Animal(...) is IGNORED for a Bat!
};
class WingedAnimal : public virtual Animal {
public:
    WingedAnimal() : Animal("default") {}   // also ignored for a Bat
};
class Bat : public Mammal, public WingedAnimal {
public:
    Bat(std::string n) : Animal(std::move(n)) {}   // MOST-DERIVED inits the vbase
};
```

```
   For 'Bat b("Bruce");':
     * Bat's init list must (and does) construct the shared Animal directly.
     * Mammal's and WingedAnimal's ' : Animal(...) ' calls are SKIPPED.
   Rule: the most-derived object initializes each virtual base ONCE, directly.
   (If Bat didn't, Animal's DEFAULT ctor would be needed — error if none.)
```

```
   Construction order with virtual bases:
     1. ALL virtual bases first (in depth-first, left-to-right order), ONCE
     2. then non-virtual bases
     3. then members, then body
```

---

## 6. Costs of virtual inheritance

```
   * Access to the virtual base is INDIRECT: objects store an offset/pointer to
     locate the shared base subobject -> a small runtime cost per access.
   * Object layout is more complex; sizeof grows.
   * Construction is more involved (most-derived initializes vbase).

   -> Use virtual inheritance ONLY when you truly have a shared-state diamond.
      For interfaces (no state), plain multiple inheritance is fine and cheaper.
```

---

## 7. When to use multiple inheritance (guidance)

```
   GOOD (recommended):
     * Implement multiple INTERFACES (pure abstract, no data). No diamond issues.
     * Mix-in small stateless behavior classes (policy/mixin).

   USE WITH CARE:
     * Multiple STATEFUL bases -> possible ambiguity; think hard.
     * Diamonds with shared state -> need virtual inheritance -> complexity.

   OFTEN BETTER ALTERNATIVE:
     * COMPOSITION (chapter 10): hold members instead of inheriting. Usually
       simpler than multiple stateful inheritance.
```

```
   +--------------------------------------------------------------+
   | Rule of thumb: inherit multiple INTERFACES freely; be very   |
   | cautious inheriting multiple IMPLEMENTATIONS. Prefer         |
   | composition for "has-a" reuse.                               |
   +--------------------------------------------------------------+
```

---

## 8. Worked example (concept recap)

```cpp
#include <iostream>
#include <string>

struct Animal { std::string name; };
struct Mammal       : virtual Animal { bool warm_blooded = true; };
struct WingedAnimal : virtual Animal { double wingspan = 0; };
struct Bat : Mammal, WingedAnimal {
    Bat(std::string n, double span) { name = std::move(n); wingspan = span; }
};

int main() {
    Bat b("Bruce", 0.3);
    std::cout << b.name << " span=" << b.wingspan   // ONE name (shared Animal)
              << " warm=" << b.warm_blooded << '\n';// Bruce span=0.3 warm=1
}
```

Runnable: [`examples/ch08_diamond.cpp`](examples/ch08_diamond.cpp).

---

## 9. Summary

<!--diagram
title: Multiple & virtual inheritance
box[green] Key points
  text: C++ allows **multiple inheritance**. Best use: implement several **interfaces** (pure abstract, no state) — clean, no diamond
  text: Shared names across bases → disambiguate with `Base::member`
  text: **DIAMOND**: two bases share a common base → the base is **duplicated** (two subobjects, ambiguous, buggy)
  text: **Virtual inheritance** (`: public virtual Base`) → **one** shared base subobject. The **most-derived** class constructs the virtual base. Cost: indirect access + complexity — use only for real shared diamonds. Prefer composition for stateful reuse
-->
```
 +------------------------------------------------------------------+
 | C++ allows MULTIPLE inheritance. Best use: implement several     |
 |   INTERFACES (pure abstract, no state) — clean, no diamond.      |
 | Shared names across bases -> disambiguate with Base::member.     |
 |                                                                  |
 | DIAMOND: two bases share a common base -> the base is DUPLICATED |
 |   (two subobjects, ambiguous, buggy).                            |
 | VIRTUAL INHERITANCE (': public virtual Base') -> ONE shared base |
 |   subobject. The MOST-DERIVED class constructs the virtual base. |
 |   Cost: indirect access + complexity -> use only for real shared |
 |   diamonds. Prefer composition for stateful reuse.               |
 +------------------------------------------------------------------+
```

Next: [09-operator-overloading.md](09-operator-overloading.md).
