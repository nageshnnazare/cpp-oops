# 12 — SOLID Principles

SOLID is five design principles for building OOP systems that are easy to
maintain, extend, and test. They're not rules to follow blindly, but heuristics
that steer you away from rigid, fragile designs. Each shown with a C++
before/after.

Prereqs: chapters 05–11.

<!--diagram
title: SOLID
box[blue] S — Single Responsibility Principle
box[teal] O — Open/Closed Principle
box[purple] L — Liskov Substitution Principle
box[orange] I — Interface Segregation Principle
box[gray] D — Dependency Inversion Principle
-->
```
   S - Single Responsibility Principle
   O - Open/Closed Principle
   L - Liskov Substitution Principle
   I - Interface Segregation Principle
   D - Dependency Inversion Principle
```

---

## 1. S — Single Responsibility Principle (SRP)

> A class should have **one reason to change** — one responsibility.

<!--diagram
title: SRP violation
box[red dashed] class Employee — God class
  text: `computePay()` — payroll logic (changes when tax rules change)
  text: `saveToDatabase()` — persistence (changes when DB changes)
  text: `renderHtml()` — presentation (changes when UI changes)
  text: Three reasons to change → edits for one concern risk breaking the others
-->
```
   BAD: a "God class" doing everything
   +------------------------------------------+
   | class Employee {                         |
   |   computePay();       // payroll logic   |  changes when tax rules change
   |   saveToDatabase();   // persistence     |  changes when DB changes
   |   renderHtml();       // presentation    |  changes when UI changes
   | };                                       |
   +------------------------------------------+
   Three reasons to change -> edits for one concern risk breaking the others.
```

```cpp
// GOOD: split responsibilities
class Employee { /* data + core domain rules only */ };
class PayCalculator { public: Money computePay(const Employee&); };  // payroll
class EmployeeRepository { public: void save(const Employee&); };    // persistence
class EmployeeView { public: std::string renderHtml(const Employee&); }; // UI
```

```
   Each class has ONE responsibility -> one reason to change -> changes are
   localized and testable. (Don't over-split trivial classes, though.)
```

---

## 2. O — Open/Closed Principle (OCP)

> Software entities should be **open for extension, closed for modification.**
> Add new behavior by adding code, not by editing existing, working code.

```cpp
// BAD: adding a shape forces editing this function (and re-testing it)
double area(const Shape& s) {
    switch (s.type) {
        case CIRCLE:    return 3.14159 * s.r * s.r;
        case RECTANGLE: return s.w * s.h;
        // add TRIANGLE -> must EDIT this switch. Every new type reopens it.
    }
}
```

```cpp
// GOOD: polymorphism — extend by adding a subclass, not editing existing code
struct Shape { virtual double area() const = 0; virtual ~Shape() = default; };
struct Circle : Shape { double r; double area() const override { return 3.14159*r*r; } };
struct Rectangle : Shape { double w,h; double area() const override { return w*h; } };
// Adding Triangle: write a new class. area()-callers stay UNCHANGED & untested-again.
```

```
   OCP via abstraction:
     caller ──> [ Shape::area() ] <── Circle
                                  <── Rectangle
                                  <── Triangle   (added later, no edits upstream)
   Extension points = virtual functions / interfaces / injected strategies.
```

---

## 3. L — Liskov Substitution Principle (LSP)

> Subtypes must be **substitutable** for their base types without breaking
> correctness. A `Derived` must honor everything a `Base` promises.

```cpp
// VIOLATION: Square is-a Rectangle?
struct Rectangle {
    virtual void set_width(int w)  { w_ = w; }
    virtual void set_height(int h) { h_ = h; }
    int area() const { return w_ * h_; }
protected: int w_ = 0, h_ = 0;
};
struct Square : Rectangle {
    void set_width(int w)  override { w_ = h_ = w; }   // must keep w==h
    void set_height(int h) override { w_ = h_ = h; }   // ...breaks Rectangle's contract
};

void resize_and_check(Rectangle& r) {
    r.set_width(5); r.set_height(4);
    assert(r.area() == 20);        // holds for Rectangle... FAILS for Square (16)!
}
```

```
   Square breaks the Rectangle contract (independent width/height). Passing a
   Square where a Rectangle is expected corrupts logic -> LSP violated.
   FIX: don't force the is-a. Model Shape with area(); Square & Rectangle are
   siblings, not parent/child. (See chapter 10 §8.)
```

```
   LSP red flags in a subclass:
     * overriding a method to do NOTHING or throw "not supported"
     * strengthening PREconditions (accepting less than the base)
     * weakening POSTconditions (delivering less than the base promised)
     * changing observable behavior clients relied on
```

---

## 4. I — Interface Segregation Principle (ISP)

> Don't force clients to depend on methods they don't use. Prefer **many small,
> focused interfaces** over one fat one.

```cpp
// BAD: a fat interface forces unrelated obligations
struct IMachine {
    virtual void print()  = 0;
    virtual void scan()   = 0;
    virtual void fax()    = 0;
    virtual ~IMachine() = default;
};
struct SimplePrinter : IMachine {
    void print() override { /* ok */ }
    void scan() override  { throw std::logic_error("no scanner"); }  // forced!
    void fax()  override  { throw std::logic_error("no fax"); }      // forced!
};
```

```cpp
// GOOD: segregate into role interfaces; implement only what applies
struct IPrinter { virtual void print() = 0; virtual ~IPrinter() = default; };
struct IScanner { virtual void scan()  = 0; virtual ~IScanner() = default; };
struct IFax     { virtual void fax()   = 0; virtual ~IFax()     = default; };

struct SimplePrinter : IPrinter { void print() override { /* ... */ } };  // just this
struct AllInOne : IPrinter, IScanner, IFax {                              // opt-in
    void print() override {} void scan() override {} void fax() override {}
};
```

```
   Fat interface:  everyone implements print+scan+fax (even a plain printer).
   Segregated:     implement only the roles you actually fulfill.
   -> Clients depend only on what they use; no dummy/throwing overrides.
```

---

## 5. D — Dependency Inversion Principle (DIP)

> Depend on **abstractions**, not concretions. High-level modules shouldn't
> depend on low-level details; both depend on interfaces.

```cpp
// BAD: high-level NotificationService hard-wired to a concrete EmailSender
class EmailSender { public: void send(const std::string&) { /* SMTP */ } };
class NotificationService {
    EmailSender sender_;                 // depends on a CONCRETE class
public:
    void notify(const std::string& msg) { sender_.send(msg); }   // can't swap/test
};
```

```cpp
// GOOD: depend on an abstraction; inject the concrete impl
struct IMessageSender {
    virtual void send(const std::string&) = 0;
    virtual ~IMessageSender() = default;
};
class EmailSender : public IMessageSender { public: void send(const std::string&) override; };
class SmsSender   : public IMessageSender { public: void send(const std::string&) override; };

class NotificationService {
    IMessageSender& sender_;             // depends on the ABSTRACTION
public:
    explicit NotificationService(IMessageSender& s) : sender_(s) {}  // injected
    void notify(const std::string& msg) { sender_.send(msg); }
};
```

```
   BEFORE:  NotificationService ────► EmailSender   (high-level -> low-level)

   AFTER:   NotificationService ────► IMessageSender ◄──── EmailSender
            (high-level)              (abstraction)        (low-level)
                                          ▲
                                          └──── SmsSender / MockSender (for tests)

   Both sides depend on the interface. You can swap Email<->SMS, and inject a
   MockSender in unit tests. This is "dependency injection."
```

Runnable: [`examples/ch12_dip.cpp`](examples/ch12_dip.cpp).

---

## 6. SOLID at a glance

<!--diagram
title: SOLID at a glance
box[green] S — Single Responsibility
  text: One class, one responsibility (one reason to change)
box[green] O — Open/Closed
  text: Extend via new code (subclasses/strategies), don't edit working code
box[green] L — Liskov Substitution
  text: Subtypes must be drop-in replacements for their base (honor the contract)
box[green] I — Interface Segregation
  text: Small, focused interfaces; don't force unused methods on clients
box[green] D — Dependency Inversion
  text: Depend on abstractions (interfaces) and inject concretions
-->
```
   S  One class, one responsibility (one reason to change).
   O  Extend via new code (subclasses/strategies), don't edit working code.
   L  Subtypes must be drop-in replacements for their base (honor the contract).
   I  Small, focused interfaces; don't force unused methods on clients.
   D  Depend on abstractions (interfaces) and inject concretions.
```

```
   How they reinforce each other:
     * OCP is usually achieved via abstractions (D) + polymorphism (L).
     * ISP keeps those abstractions small so implementers aren't over-burdened.
     * SRP keeps classes small enough that the others are natural.
```

---

## 7. Balance & pragmatism

```
   SOLID are HEURISTICS, not laws. Over-applying them causes its own harm:
     * too many tiny classes/interfaces -> indirection soup, hard to follow.
     * abstracting things that never vary -> speculative generality (YAGNI).
   Apply them where change is LIKELY. Start simple; refactor toward SOLID when
   a real need (a second implementation, a testing seam) appears.
```

---

## 8. Summary

<!--diagram
title: SOLID principles
box[green] SOLID = maintainable OOP heuristics
  text: **S** Single Responsibility: one reason to change per class
  text: **O** Open/Closed: extend by adding code (polymorphism), not editing
  text: **L** Liskov Substitution: subtypes must honor the base's contract
  text: **I** Interface Segregation: many small interfaces > one fat one
  text: **D** Dependency Inversion: depend on abstractions; inject impls
  text: They interlock (OCP needs D+L; I keeps abstractions lean; S keeps classes small). Apply where change is likely — not dogmatically (avoid over-engineering)
-->
```
 +------------------------------------------------------------------+
 | SOLID = maintainable OOP heuristics:                             |
 |  S Single Responsibility: one reason to change per class.        |
 |  O Open/Closed: extend by adding code (polymorphism), not editing|
 |  L Liskov Substitution: subtypes must honor the base's contract. |
 |  I Interface Segregation: many small interfaces > one fat one.   |
 |  D Dependency Inversion: depend on abstractions; inject impls.   |
 | They interlock (OCP needs D+L; I keeps abstractions lean; S      |
 |   keeps classes small). Apply where change is likely — not       |
 |   dogmatically (avoid over-engineering).                         |
 +------------------------------------------------------------------+
```

Next: [13-design-patterns.md](13-design-patterns.md).
