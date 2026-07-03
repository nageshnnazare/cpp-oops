# 03 — Encapsulation & Access Control

Encapsulation is the first pillar: **bundle data with the code that manages it,
hide the internals, and expose a controlled interface that preserves the object's
invariants.** This is what makes objects trustworthy.

Prereq: [01-classes-and-objects.md](01-classes-and-objects.md).

---

## 1. The three access levels

```cpp
class Widget {
public:      // accessible to everyone
    void api();
protected:   // accessible to this class and its DERIVED classes (chapter 05)
    void helper_for_subclasses();
private:     // accessible ONLY within this class (and friends, chapter 11)
    int secret_;
};
```

```
   +-----------+---------------------------------------------+
   | public    | anyone: the class's INTERFACE               |
   | protected | this class + derived classes                |
   | private   | this class only (+ friends)                 |
   +-----------+---------------------------------------------+

           outside world
                |  can touch only PUBLIC
                v
   +-------------------------------+
   | public:    api()              |  <- the contract
   |-------------------------------|
   | protected: helper()           |  <- for subclasses
   |-------------------------------|
   | private:   secret_            |  <- hidden internals
   +-------------------------------+
```

Access is checked at **compile time** and is **per-class, not per-object**: a
`Widget` method can access the private members of *another* `Widget` (relevant
for copy constructors and `operator==`).

---

## 2. Invariants — the *why* of encapsulation

An **invariant** is a rule about the object's state that must always hold between
public operations. Encapsulation exists to protect invariants.

```cpp
class Temperature {
    double celsius_;
public:
    explicit Temperature(double c) { set(c); }
    void set(double c) {
        if (c < -273.15) c = -273.15;   // INVARIANT: never below absolute zero
        celsius_ = c;
    }
    double celsius() const { return celsius_; }
};
```

```
   celsius_ is PRIVATE -> the only way to change it is set(), which ENFORCES
   the invariant (>= absolute zero). No caller can put the object in a bad state.

   If celsius_ were public:
       t.celsius_ = -1000;   // invariant violated; object corrupted
   Encapsulation makes this IMPOSSIBLE.
```

<!--diagram
box[green] Encapsulation
  text: Encapsulation = "make invalid states unrepresentable/unreachable"
-->
```
   +-------------------------------------------------------------------+
   | Encapsulation = "make invalid states unrepresentable/unreachable" |
   +-------------------------------------------------------------------+
```

---

## 3. Getters, setters, and NOT overusing them

Accessors expose state in a controlled way. But a class that's just public
getters+setters for every field is barely encapsulated — it's a struct in
disguise.

```cpp
class Circle {
    double radius_;
public:
    explicit Circle(double r) : radius_(r) {}

    // GOOD: expose BEHAVIOR / derived values, not raw fields
    double area() const { return 3.14159265 * radius_ * radius_; }
    double radius() const { return radius_; }
    void set_radius(double r) { if (r >= 0) radius_ = r; }  // validated setter
};
```

```
   ANTI-PATTERN ("anemic" object):
     class Circle { public: double radius; };   // no invariants, no behavior
     // caller computes area everywhere -> logic scattered, radius can go negative

   BETTER: the object OWNS its logic (area()) and GUARDS its data (set_radius).
```

```
   Ask: does this getter/setter EARN its place, or am I just exposing a field?
   Prefer methods that express INTENT (deposit(), rotate(), connect()) over
   raw set_x/get_x pairs. Expose the LEAST that clients truly need.
```

---

## 4. `const` correctness = encapsulation of *mutability*

Marking methods `const` encapsulates *who can change what*. A `const` reference
to your object can only call `const` methods — a compile-time guarantee it won't
be mutated.

```cpp
class Account {
    double balance_ = 0;
public:
    double balance() const { return balance_; }   // read-only view
    void   deposit(double a) { balance_ += a; }    // mutating

    // reads through 'const Account&' are guaranteed non-mutating:
    void print(std::ostream& os) const { os << balance_; }
};

void audit(const Account& a) {
    a.balance();          // OK
    a.print(std::cout);   // OK
    // a.deposit(10);     // COMPILE ERROR: can't mutate a const&
}
```

```
   Passing 'const T&' encapsulates read-only intent AT THE TYPE LEVEL.
   The compiler enforces it. This is cheaper and stronger than a comment.
```

### `mutable` — the deliberate exception

```cpp
class Cache {
    mutable std::optional<double> cached_;   // 'mutable' can change in const methods
    double compute() const;
public:
    double value() const {                   // logically const...
        if (!cached_) cached_ = compute();   // ...but updates the cache
        return *cached_;
    }
};
```

```
   'mutable' = "this member may change even in a const method." Use SPARINGLY,
   only for things that don't affect the object's OBSERVABLE state (caches,
   mutexes, hit-counters). Overuse breaks the const guarantee's meaning.
```

---

## 5. The Pimpl idiom — encapsulating the *implementation*

To hide implementation details from the header (and speed up compilation), put
the private data behind a pointer to an incomplete type.

```cpp
// widget.hpp  (the public interface; leaks NO implementation details)
class Widget {
public:
    Widget();
    ~Widget();                      // defined in .cpp (needs complete Impl)
    void do_work();
private:
    struct Impl;                    // forward-declared, INCOMPLETE here
    std::unique_ptr<Impl> pimpl_;   // pointer to the hidden implementation
};
```

```cpp
// widget.cpp  (all the real internals live here)
struct Widget::Impl {
    int cache; std::vector<int> data; /* ...whatever... */
};
Widget::Widget() : pimpl_(std::make_unique<Impl>()) {}
Widget::~Widget() = default;        // here Impl is complete -> OK
void Widget::do_work() { pimpl_->data.push_back(pimpl_->cache); }
```

```
   header sees:   Widget -> [ pimpl_ (just a pointer) ]
   .cpp  sees:    Impl   -> [ cache, data, ...real members... ]

   Benefits:
     * Clients recompile only when the PUBLIC interface changes (not internals).
     * True information hiding: the header reveals nothing about internals.
     * Stable ABI (binary layout of Widget = one pointer, regardless of Impl).
   Cost: one heap allocation + an indirection.
```

Runnable: [`examples/ch03_pimpl.cpp`](examples/ch03_pimpl.cpp).

---

## 6. Encapsulation across files: the header is the contract

```
   +-------------------------------------------------------------+
   | The PUBLIC section of a class is its API contract.          |
   | Everything else (private members, .cpp bodies, Impl) is     |
   | free to change without breaking callers.                    |
   |                                                             |
   | Minimize the public surface -> more freedom to refactor.    |
   +-------------------------------------------------------------+
```

---

## 7. Worked example: a validated `Stack`

```cpp
#include <vector>
#include <stdexcept>
#include <iostream>

template <class T>
class Stack {
    std::vector<T> data_;          // private: callers can't corrupt the order
public:
    bool empty() const { return data_.empty(); }
    std::size_t size() const { return data_.size(); }

    void push(T value) { data_.push_back(std::move(value)); }

    T pop() {
        if (data_.empty())                       // enforce invariant / precondition
            throw std::out_of_range("pop from empty stack");
        T top = std::move(data_.back());
        data_.pop_back();
        return top;
    }

    const T& top() const {
        if (data_.empty()) throw std::out_of_range("top of empty stack");
        return data_.back();
    }
};

int main() {
    Stack<int> s;
    s.push(1); s.push(2); s.push(3);
    std::cout << s.top() << '\n';   // 3
    std::cout << s.pop() << '\n';   // 3
    std::cout << s.size() << '\n';  // 2
}
```

```
   data_ is private -> clients can ONLY use push/pop/top/etc.
   They can never reorder, resize, or corrupt the underlying vector.
   The class GUARANTEES stack semantics (LIFO, no invalid pops).
```

Runnable: [`examples/ch03_stack.cpp`](examples/ch03_stack.cpp).

---

## 8. Summary

<!--diagram
title: Encapsulation & access control
box[green] Key points
  text: Access: **public** (interface) / **protected** (subclasses) / **private** (internals). Checked at compile time, per-**class** not per-object
  text: Encapsulation **protects invariants**: make invalid states unreachable by hiding data behind validating methods
  text: Avoid anemic get/set-everything classes; expose **behavior** & intent
  text: `const`-correctness encapsulates mutability (`const&` = read-only). `mutable` is the deliberate, sparing exception (caches/mutexes)
  text: **Pimpl** hides the implementation from the header (compile firewall). Minimize the public surface = maximum freedom to refactor
-->
```
 +-------------------------------------------------------------------+
 | Access: public (interface) / protected (subclasses) / private     |
 |   (internals). Checked at compile time, per-CLASS not per-object. |
 |                                                                   |
 | Encapsulation PROTECTS INVARIANTS: make invalid states            |
 |   unreachable by hiding data behind validating methods.           |
 | Avoid anemic get/set-everything classes; expose BEHAVIOR & intent.|
 |                                                                   |
 | const-correctness encapsulates mutability (const& = read-only).   |
 | 'mutable' is the deliberate, sparing exception (caches/mutexes).  |
 | Pimpl hides the implementation from the header (compile firewall).|
 | Minimize the public surface = maximum freedom to refactor.        |
 +-------------------------------------------------------------------+
```

Next: [04-copy-move-rule-of-five.md](04-copy-move-rule-of-five.md).
