# 12 — SOLID Principles

SOLID is five names for failure modes that show up in real class designs.
They are heuristics. A design that satisfies all five and has twelve
interfaces to add two integers has failed a different test. Apply them where
a second implementation, a test double, or a breaking change has actually
appeared, or where you can see it coming.

Prereqs: chapters 05–11.

```
   S   Single Responsibility     one reason to change
   O   Open/Closed               extend without editing the stable part
   L   Liskov Substitution       a subtype honors the base contract
   I   Interface Segregation     clients do not depend on methods they do not use
   D   Dependency Inversion      depend on abstractions, inject concretions
```

---

## 1. Single Responsibility

A responsibility is a reason to change. A class that computes pay, writes SQL,
and renders HTML changes when the tax rules change, when the database
changes, and when the page layout changes. A fix in the SQL can break the
tax calculation because they share a file, a lock, and a reviewer who is
tired.

```cpp
class Employee {
public:
    Money salary() const { return salary_; }
    // domain facts only: name, grade, tenure
private:
    Money salary_;
};

class PayCalculator {
public:
    Money compute(const Employee& e, const TaxRules& rules) const;
};

class EmployeeRepository {
public:
    void save(const Employee& e);
};

class EmployeeView {
public:
    std::string render(const Employee& e) const;
};
```

Each type is now edited for one reason. They collaborate through `Employee`,
which is data and domain rules, not a god object.

The failure mode in the other direction is a class per function with no
cohesion. `PayCalculator` may legitimately know about tax brackets and
rounding. Splitting those into `BracketSelector` and `RoundingPolicy` before
either has varied is speculative. Keep together what changes together.

SRP also applies to functions and to headers. A header that includes the
database and the GUI couples every client of `Employee` to both.

---

## 2. Open/Closed

Open for extension, closed for modification: add a behavior by adding code,
without editing the code that already works and has tests.

```cpp
// Closed against extension: every new shape edits this function.
double area(const Shape& s) {
    switch (s.kind) {
    case Kind::Circle:    return 3.141592653589793 * s.r * s.r;
    case Kind::Rectangle: return s.w * s.h;
    }
}
```

```cpp
struct Shape {
    virtual double area() const = 0;
    virtual ~Shape() = default;
};
struct Circle : Shape {
    double r;
    double area() const override { return 3.141592653589793 * r * r; }
};
```

`total_area` from chapter 07 does not change when `Triangle` appears.
`Triangle` is the extension. The stable part stayed closed.

Virtual functions are one way. They are not the only way, and they are the
wrong way when the set is closed:

```
   open set, new types in new libraries        virtual function
   closed set, new types should break the build std::variant + std::visit
   behavior injected from outside              strategy object / std::function
   algorithm fixed, type known at the call     function template
```

A `switch` on an enum is closed against extension and open to a forgotten
case. A `variant` is closed against extension and the compiler lists the
forgotten cases. Pick the failure mode you want. An open hierarchy that
nobody extends is a vtable you did not need. A switch that every feature
team edits is a merge conflict you did need to avoid.

OCP is not "never edit old files." Fixing a bug is a modification, and it
should happen in the old file. OCP is about where *variation* goes.

---

## 3. Liskov Substitution

If `S` is a subtype of `T`, objects of type `T` in a program may be replaced
by objects of type `S` without breaking the program's correctness. Public
inheritance in C++ is exactly the claim that this is true.

The contract has several parts. An override must:

```
   honor the base invariant
   not strengthen preconditions     (it may accept more, not less)
   not weaken postconditions        (it may promise more, not less)
   not throw exceptions the base promised not to throw
   not surprise a caller who used the base's observable behavior,
     including the history of calls ("history constraint")
```

### Preconditions and postconditions

```cpp
struct Reader {
    // precondition: n <= remaining()
    virtual int read(char* dst, int n) = 0;
};

struct StrictReader : Reader {
    int read(char* dst, int n) override {
        if (n > 1) throw std::invalid_argument("n");   // rejects inputs the base accepted
        return Reader::read(dst, n);
    }
};
```

A caller holding a `Reader&` is allowed to `read(buf, remaining())`.
`StrictReader` throws on a call the contract permits. That is a stronger
precondition, and it is a Liskov violation. The fix is a different type, not
an override.

### The rectangle and the square

```cpp
struct Rectangle {
    virtual void set_width(int w) { w_ = w; }
    virtual void set_height(int h) { h_ = h; }
    int area() const { return w_ * h_; }
protected:
    int w_ = 0, h_ = 0;
};

struct Square : Rectangle {
    void set_width(int w) override { w_ = h_ = w; }
    void set_height(int h) override { w_ = h_ = h; }
};

void stretch(Rectangle& r) {
    r.set_width(5);
    r.set_height(4);
    assert(r.area() == 20);     // true for Rectangle, false for Square (16)
}
```

`Rectangle::set_width`'s postcondition is "the height is unchanged and the
width equals the argument." `Square` cannot satisfy that and keep equal
sides. The history constraint is the same fact from the caller's side: after
`set_width(5)` and `set_height(4)`, a rectangle's area is 20. A square
breaks a property the caller was entitled to remember.

Model both as shapes, or store a size that is not independently settable.
Do not inherit.

### Functions that throw "not supported"

An override that throws `logic_error("unsupported")` for a base operation
fails substitutability. Callers of the base cannot call the function. Split
the base (section 4) or stop claiming the subtype relationship.

### What LSP is not

It does not require the override to have the same *algorithm*. `Dog::speak`
may print a different string from `Animal::speak`. It requires the override
to meet the promises callers of `Animal::speak` rely on: it returns, it does
not throw if the base said it does not, it leaves the animal in a valid
state. Document those promises. An undocumented base is an invitation to
violate LSP by accident.

---

## 4. Interface Segregation

Clients should not be forced to depend on methods they do not use. A fat
interface forces implementers to stub functions, and it forces callers to
recompile when an unrelated function changes.

```cpp
struct Machine {
    virtual void print() = 0;
    virtual void scan() = 0;
    virtual void fax() = 0;
    virtual ~Machine() = default;
};

struct SimplePrinter : Machine {
    void print() override {}
    void scan() override { throw std::logic_error("no scanner"); }
    void fax() override { throw std::logic_error("no fax"); }
};
```

`SimplePrinter` is not a `Machine`. The stubs are the smell.

```cpp
struct Printer {
    virtual void print() = 0;
    virtual ~Printer() = default;
};
struct Scanner {
    virtual void scan() = 0;
    virtual ~Scanner() = default;
};
struct Fax {
    virtual void fax() = 0;
    virtual ~Fax() = default;
};

struct SimplePrinter : Printer {
    void print() override {}
};
struct AllInOne : Printer, Scanner, Fax {
    void print() override {}
    void scan() override {}
    void fax() override {}
};
```

A function that prints takes a `Printer&`. It does not mention fax, and a
printer with no fax implements nothing extra. Multiple inheritance of
stateless interfaces is the C++ expression of this split (chapter 08).

Segregation has a cost: more types, and a class that does all three names
all three bases. Pay it when implementers are stubbing, or when callers are
recompiling because an unrelated method changed. Do not split an interface
that every implementer implements in full and every caller uses in full.

---

## 5. Dependency Inversion

High-level policy should not depend on low-level detail. Both should depend
on an abstraction. In C++ the abstraction is usually an interface the
high-level side owns, and the low-level side implements.

```cpp
struct MessageSender {
    virtual void send(const std::string& msg) = 0;
    virtual ~MessageSender() = default;
};

class EmailSender : public MessageSender {
public:
    void send(const std::string& msg) override { /* SMTP */ }
};

class NotificationService {
public:
    explicit NotificationService(MessageSender& sender) : sender_(sender) {}
    void notify(const std::string& msg) { sender_.send(msg); }
private:
    MessageSender& sender_;
};
```

```
   NotificationService ──► MessageSender ◄── EmailSender
                                   ▲
                                   └── SmsSender, FakeSender
```

`NotificationService` does not include the SMTP headers. A test passes a
`FakeSender` that records the string. The concrete sender is constructed in
`main` (or in a composition root) and passed in. That is dependency
injection. The principle is the direction of the arrow. Injection is the
technique.

The reference member means `NotificationService` must not outlive the
sender. A `unique_ptr<MessageSender>` means the service owns the sender. A
`shared_ptr` means ownership is shared. Pick one. A concrete `EmailSender`
member means the service cannot be tested without SMTP, which is the
original problem.

DIP does not mean every type has an interface. A `NotificationService` that
takes `const std::string&` depends on `std::string`, and that is fine,
because `std::string` is stable and is not a detail you will swap. Invert
the dependency when the low-level side is volatile, slow, or something you
must replace in tests.

Templates invert dependencies in a different direction: the algorithm is
written against a concept, and the concrete type is substituted at compile
time. There is no runtime injection and no vptr. Use that when the concrete
type is known at the call and you want the body inlined. Use a virtual
interface when the concrete type is chosen at run time or you need a
heterogeneous collection.

Runnable: [`examples/ch12_dip.cpp`](examples/ch12_dip.cpp).

---

## 6. How the five interact

```
   SRP   keeps a class small enough that its contract is sayable
   ISP   keeps each contract small enough that implementers mean it
   LSP   is the rule those contracts impose on overrides
   DIP   points both sides at the contract instead of at each other
   OCP   is what you get when a new subtype or a new strategy slots in
         without editing the callers
```

A fat interface (failed ISP) makes LSP violations likely, because some
implementer will stub a method. A concrete dependency (failed DIP) makes OCP
fail, because adding a second sender edits `NotificationService`. A god class
(failed SRP) makes all of the edits happen in one file, so the other four
are academic.

---

## 7. Other heuristics worth keeping next to SOLID

```
   YAGNI          do not build the extension point until a second variant exists
   KISS           the boring design is the one you can still read in a year
   Law of Demeter a function talks to its parameters and its members,
                  not to the internals of those objects (chapter 03)
   Rule of Zero   resource ownership stays in members that already implement it
   composition    reuse by members, inherit to be substitutable (chapter 10)
```

YAGNI and OCP pull in opposite directions. The resolution is time. The first
shape is concrete. When the second shape appears, introduce the interface
and the injection point, and leave the call sites depending on the
interface. Introducing the interface on day one, with one implementation, is
how codebases fill up with `IThing` and `ThingImpl`.

---

## 8. Exercises

1. `Square` overrides `Rectangle::set_width` and also sets the height. Which
   part of the base contract breaks, the precondition or the postcondition?
2. A `switch` on an enum of three shapes lives in `area()`. You control
   every shape, and there will be a fourth next month, still in this
   repository. Give one reason to switch to a virtual function and one
   reason to switch to `std::variant` instead.
3. `SimplePrinter` implements `scan` by throwing. Which two principles does
   that violate at once?
4. Why is a `MessageSender&` constructor parameter a test seam, and what
   lifetime obligation does the reference create?

### Answers

1. The postcondition. `set_width` on a rectangle promises that the height
   stays what it was. The square's override changes the height. Callers who
   were allowed to rely on that promise cannot.
2. Virtual: the function `area()` never changes again, and a new shape is a
   new file. Variant: adding a shape is a compile error at every `visit`
   until you handle it, and the objects stay values with no heap and no
   vptr. If the set is closed and local, the variant's exhaustiveness is
   the better bug. If plugins will supply shapes, the variant cannot see
   them and the virtual function can.
3. Liskov substitution, because a `Machine&` caller may call `scan`.
   Interface segregation, because `SimplePrinter` was forced to implement
   operations it does not have. The fix is a `Printer` base that has only
   `print`.
4. The test constructs a fake sender and passes it in; production passes
   `EmailSender`. The service does not construct the dependency, so it does
   not name it. The reference does not extend the sender's lifetime. The
   sender must outlive the service.

---

## 9. Summary

<!--diagram
title: SOLID principles
box[green] Key points
  text: SRP — one reason to change. Split what changes for different reasons. Do not split what always changes together
  text: OCP — new behavior arrives as new types or new strategies. Virtual for an open set, variant for a closed set
  text: LSP — overrides weaken nothing callers of the base rely on. Rectangle/Square fails a postcondition
  text: ISP — small interfaces so implementers do not stub and callers do not depend on unused functions
  text: DIP — the policy depends on an abstraction the policy owns; main injects the concrete type. Invert volatile dependencies, not std::string
-->
```
 +------------------------------------------------------------------+
 | S  one reason to change.                                         |
 | O  extend by adding code. Virtual = open set. variant = closed.  |
 | L  subtypes honor preconditions, postconditions, invariants,     |
 |    and the history callers observed.                             |
 | I  split interfaces that force stubs.                            |
 | D  depend on an abstraction; inject the concretion at the edge.  |
 | YAGNI: add the seam when the second implementation shows up.     |
 +------------------------------------------------------------------+
```

Next: [13-design-patterns.md](13-design-patterns.md).
