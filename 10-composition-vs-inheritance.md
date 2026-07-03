# 10 — Composition vs Inheritance (Object Relationships)

Choosing how objects relate is where OOP designs live or die. The modern maxim is
**"favor composition over inheritance."** This chapter explains the object
relationships (is-a, has-a, uses-a), when each fits, and why composition is
usually the safer default.

Prereq: [05-inheritance.md](05-inheritance.md).

---

## 1. The relationships

```
   IS-A     (inheritance)  : Dog IS-A Animal            -> public inheritance
   HAS-A    (composition)  : Car HAS-A Engine (owns it) -> member object
   HAS-A    (aggregation)  : Team HAS players (refers)  -> pointer/reference member
   USES-A   (dependency)   : Order USES a PriceService  -> parameter / temporary
```

```
   Car ─owns──► Engine        (composition: Engine lifetime = Car lifetime)
   Team ─refs─► Player...      (aggregation: players exist independently)
   Report ─uses► Printer       (dependency: passed in, not stored)
   Dog ──is───► Animal         (inheritance)
```

---

## 2. Composition: "has-a" via member objects

```cpp
class Engine {
public:
    void start() { /* ... */ }
    void stop()  { /* ... */ }
};

class Car {
    Engine engine_;            // Car HAS-A Engine (composition; owns it)
    Wheels wheels_;
public:
    void drive() {
        engine_.start();       // delegate to the part
        // ...
    }
};
```

<!--diagram
title: Composition — Car has-a Engine
box[blue] Car
  text: `Engine engine_` — the Engine is **part of** the Car (created & destroyed with the Car, RAII)
  text: `Wheels wheels_`
  text: Car exposes what it wants (`drive()`) and forwards to its parts. It is **not** an Engine; it **has** one
-->
```
   Car
   +----------------------+
   |  Engine engine_      |   <- the Engine is PART OF the Car
   |  Wheels wheels_      |      created & destroyed with the Car (RAII)
   +----------------------+

   Car exposes what it wants (drive()) and forwards to its parts. It is NOT an
   Engine; it HAS one. Callers can't accidentally treat a Car as an Engine.
```

```
   Composition = strong ownership. When the Car is destroyed, its Engine is too.
   The whole controls access to its parts; parts don't leak into the interface.
```

---

## 3. Aggregation: "has-a" but parts live independently

```cpp
class Player { /* ... */ };

class Team {
    std::vector<Player*> members_;      // Team refers to Players it doesn't own
public:                                 // (or std::shared_ptr for shared ownership)
    void add(Player* p) { members_.push_back(p); }
};
```

```
   Aggregation: the Team knows about Players but does NOT own their lifetime.
   Deleting the Team does not delete the Players. Use non-owning pointers,
   references, or shared_ptr depending on ownership semantics.
```

---

## 4. Why "favor composition over inheritance"

Inheritance is the **tightest** coupling in OOP: the derived class depends on the
base's implementation details and inherits its *entire* interface — including
things you may not want.

```
   PROBLEMS with (over)using inheritance:
     * Tight coupling: change the base -> ripples into all derived classes.
     * The "fragile base class" problem: base edits silently break subclasses.
     * You inherit the WHOLE interface, even parts that don't apply.
     * Deep hierarchies are hard to understand and rigid to change.
     * Only one thing can vary along the inheritance axis.

   COMPOSITION advantages:
     * Loose coupling: depend on a small interface, swap implementations.
     * Flexible: combine multiple behaviors; change parts at runtime.
     * Clear ownership; no accidental "is-a" that isn't true.
     * Easier to test (inject mock parts).
```

---

## 5. The classic cautionary tale: `Stack` from `Vector`

```cpp
// BAD: Stack IS-A vector?  No! This exposes push_back, operator[], insert...
template <class T>
class Stack : public std::vector<T> {    // WRONG relationship
public:
    void push(const T& x) { this->push_back(x); }
    void pop() { this->pop_back(); }
};

Stack<int> s;
s.push(1);
s[0] = 99;                 // OOPS: inherited operator[] breaks stack semantics!
s.insert(s.begin(), 5);    // OOPS: inherited insert bypasses push/pop entirely!
```

```
   Public inheritance gave Stack the ENTIRE vector interface, letting callers
   violate the LIFO invariant. Stack is NOT a vector; it HAS a vector.
```

```cpp
// GOOD: Stack HAS-A vector (composition) -> expose ONLY stack operations
template <class T>
class Stack {
    std::vector<T> data_;                // composition
public:
    void push(const T& x) { data_.push_back(x); }
    void pop()            { data_.pop_back(); }
    const T& top() const  { return data_.back(); }
    bool empty() const    { return data_.empty(); }
};
```

```
   Now the ONLY operations are push/pop/top/empty. The vector is hidden; the
   LIFO invariant cannot be broken. (This is exactly what std::stack does — it's
   a container ADAPTER built by composition, not inheritance.)
```

Runnable: [`examples/ch10_stack_composition.cpp`](examples/ch10_stack_composition.cpp).

---

## 6. Strategy pattern: composition for varying behavior

Instead of subclassing to change behavior, **inject** a behavior object. This
lets behavior change at runtime and avoids a class explosion.

```cpp
// INHERITANCE approach (rigid): a subclass per behavior combo -> explosion
//   class SortedListAscending : public List { ... };
//   class SortedListDescending : public List { ... };  // and every combo...

// COMPOSITION approach: inject a comparator (strategy)
class Sorter {
    std::function<bool(int,int)> compare_;      // the strategy (a part)
public:
    explicit Sorter(std::function<bool(int,int)> cmp) : compare_(std::move(cmp)) {}
    void sort(std::vector<int>& v) { std::sort(v.begin(), v.end(), compare_); }
};

Sorter asc([](int a, int b){ return a < b; });
Sorter desc([](int a, int b){ return a > b; });
// swap strategy at runtime; no new subclasses needed.
```

```
   Vary behavior by COMPOSING a strategy object rather than SUBCLASSING.
   -> N behaviors + M contexts = N+M pieces (composition)
      vs  N*M subclasses (inheritance explosion).
```

---

## 7. Decision guide: inherit or compose?

```
   Ask, in order:
   1. Is it TRULY "is-a" (Liskov: a Derived works ANYWHERE a Base is expected)?
        NO  -> compose.
        YES -> continue.
   2. Do you need RUNTIME POLYMORPHISM over a shared interface?
        YES -> (public) inheritance with virtuals is a good fit.
        NO  -> composition is usually simpler.
   3. Would inheriting expose base operations that break your invariants?
        YES -> compose (hide the part).
   4. Do you just want to REUSE code?
        -> that's NOT a reason to inherit. Compose (or free functions).
```

<!--diagram
box[green] Inheritance rule
  text: "Inherit to be **SUBSTITUTABLE** (polymorphism), not to **REUSE**."
  text: Reuse is what composition (and functions) are for
-->
```
   +---------------------------------------------------------------+
   | "Inherit to be SUBSTITUTABLE (polymorphism), not to REUSE."   |
   | Reuse is what composition (and functions) are for.            |
   +---------------------------------------------------------------+
```

---

## 8. Liskov Substitution Principle (the is-a litmus test)

Public inheritance must obey LSP (chapter 12): a `Derived` must be usable
**anywhere** a `Base` is, without breaking expectations.

```
   Classic violation: "Square IS-A Rectangle"
     Rectangle has setWidth/setHeight independently.
     Square must keep width == height -> setWidth(5) also changes height.
     Code that does 'r.setWidth(5); r.setHeight(3); assert(area()==15);' BREAKS
     when r is actually a Square (area becomes 9). Square is NOT substitutable.
   -> Don't inherit; model differently (compose, or separate types).
```

```
   If a subclass must WEAKEN a base guarantee or throw "not supported" for an
   inherited method, the is-a relationship is false. Prefer composition.
```

---

## 9. Summary

<!--diagram
title: Composition vs inheritance
box[green] Key points
  text: Relationships: **IS-A** (inherit), **HAS-A** (compose/aggregate), **USES-A** (dependency)
  text: **Favor composition** over inheritance: looser coupling, flexible, clear ownership, no accidental interface leakage
  text: Inherit **only** for genuine is-a + substitutability (LSP) and/or runtime polymorphism — **not** merely to reuse code
  text: Classic trap: `Stack : public vector` leaks the interface; `Stack { vector data_; }` (composition) hides it
  text: **Strategy** pattern: inject behavior objects instead of subclassing
-->
```
 +------------------------------------------------------------------+
 | Relationships: IS-A (inherit), HAS-A (compose/aggregate),        |
 |   USES-A (dependency).                                           |
 | FAVOR COMPOSITION over inheritance: looser coupling, flexible,   |
 |   clear ownership, no accidental interface leakage.              |
 | Inherit ONLY for genuine is-a + substitutability (LSP) and/or    |
 |   runtime polymorphism — NOT merely to reuse code.               |
 | Classic trap: 'Stack : public vector' leaks the interface;       |
 |   'Stack { vector data_; }' (composition) hides it.              |
 | Strategy pattern: inject behavior objects instead of subclassing.|
 +------------------------------------------------------------------+
```

Next: [11-static-friends-nested.md](11-static-friends-nested.md).
