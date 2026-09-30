# 03 — Encapsulation & Access Control

Encapsulation is how a class makes its invariant enforceable. Access control
is the language mechanism. The design mechanism is smaller: a public surface
that preserves the invariant, and a representation nobody else can touch.

Prereq: [01-classes-and-objects.md](01-classes-and-objects.md).

---

## 1. The three access levels

```cpp
class Widget {
public:       // the interface: anyone
    void api();
protected:    // this class, derived classes, and their friends
    void helper();
private:      // this class and its friends
    int secret_;
};
```

```
   public     anyone
   protected  the class, classes derived from it, and friends of those
   private    the class and its friends

   Access is a compile-time check. It is not a security boundary: anyone who
   can see the object representation can reinterpret the bytes. It is a
   boundary against accidental coupling.
```

Access is **per class, not per object**. A member function may touch the
private members of *any* object of that class, which is what makes copy
constructors and `operator==` possible:

```cpp
class Account {
    double balance_;
public:
    Account(const Account& other) : balance_(other.balance_) {}  // other's private
    bool same_balance(const Account& o) const { return balance_ == o.balance_; }
};
```

`friend` (chapter 11) is an explicit extra grant. Friendship is not inherited,
not transitive, and not symmetric.

You may repeat access specifiers, and the order of sections is conventional
(`public` first) rather than required. A `struct` defaults to `public`; a
`class` defaults to `private`.

---

## 2. The protected-access rule people get wrong

A derived class may use a protected member of its base. There is a second
check when the access goes through an object: the object expression's type
must be the derived class itself, or a class further derived from it.

```cpp
class Base {
protected:
    int value_ = 0;
};

class Derived : public Base {
public:
    void set_own(Derived& d) { d.value_ = 1; }     // ok: d is a Derived
    void set_base(Base& b)   { b.value_ = 1; }     // error: b is not a Derived
};

class Sibling : public Base {
public:
    void poke(Derived& d) { d.value_ = 1; }        // error: Sibling is not Derived
};
```

The rule exists so a `Derived` cannot reach into some arbitrary `Base` (or a
sibling) and break that object's invariant using a protected knob it only
understands for itself. Inside `Derived::set_own`, the compiler knows the
object really is a `Derived`. Inside `set_base`, it might be a `Sibling`.

Protected **data** is still a design smell. Every derived class becomes part
of the representation, and the invariant is no longer local. Prefer protected
functions (or, better, the non-virtual interface in chapter 07) and keep data
private.

---

## 3. Invariants

An invariant is a predicate on the representation that holds whenever no
member function is in the middle of running. Callers may assume it before
every public call. The class must re-establish it before every public return.

```cpp
class Temperature {
public:
    explicit Temperature(double c) { set(c); }
    void set(double c) {
        if (c < -273.15) c = -273.15;    // absolute zero
        celsius_ = c;
    }
    double celsius() const { return celsius_; }
private:
    double celsius_;
};
```

If `celsius_` were public, `t.celsius_ = -1000` would compile and the
predicate would be a comment. With `celsius_` private, every modification
goes through `set`.

Think in three layers, even if you never write them as contracts:

```
   precondition    what the caller must guarantee (index in range)
   postcondition   what the function guarantees on return (size grew by 1)
   invariant       what is true between calls (size <= capacity, buffer non-null
                   or size == 0)
```

Check preconditions that indicate programmer error with `assert` (or a
contract facility, when you have one). Report precondition failures that are
part of the API — a parse error, a missing file — through the return channel
the API chose (`std::expected`, `std::optional`, an exception type that means
"this input was bad"). Do not `assert` on user input in a library that must
survive release builds: `assert` disappears under `NDEBUG`.

The constructor is the one place the invariant is established rather than
preserved. A constructor either finishes with the invariant true, or it
throws and no object exists (chapter 02).

"Make invalid states unrepresentable" is the stronger form of the same idea.
A day-of-month stored as `int` can be 32. A day stored as an enum, or as a
class whose constructor rejects 32, cannot. `std::optional<T>` represents
"maybe absent" without a magic sentinel. A `Meters` type and a `Grams` type
will not add if their constructors are explicit and there is no mixed
`operator+`.

---

## 4. Getters are not encapsulation

A class whose public surface is a getter and a setter per field has no
invariant. It is a `struct` with extra syntax. Callers still implement the
behavior, and they implement it differently in each place.

```cpp
class Circle {
public:
    explicit Circle(double r) : radius_(r < 0 ? 0 : r) {}
    double area() const { return 3.141592653589793 * radius_ * radius_; }
    double radius() const { return radius_; }
    void set_radius(double r) { if (r >= 0) radius_ = r; }
private:
    double radius_;
};
```

`area()` belongs on `Circle` because it is defined by the representation and
callers would otherwise re-derive πr², sometimes badly. `set_radius` earns
its place because it enforces `radius_ >= 0`. A setter that assigns blindly
does not.

Prefer names that say what the operation means: `deposit`, `connect`,
`rotate`. A `set_x` is appropriate when the abstraction really is "this
attribute may change, under these constraints."

### Returning the representation

```cpp
const std::string& name() const { return name_; }   // caller must not outlive *this
std::string name() const { return name_; }          // caller owns a copy
```

Returning a non-const reference to a private member deletes the invariant for
that member: the caller can write anything through the reference. Returning a
pointer to an internal buffer has the same effect, plus a lifetime trap.

```cpp
std::string& name() { return name_; }     // caller may assign name() = "" and
                                          // bypass every check set_name would do
```

If the object is logically mutable through a controlled operation, write the
operation. If the member is genuinely a public field of a passive aggregate,
make the type an aggregate and stop pretending.

---

## 5. `const` is part of the interface

A `const` member function promises not to modify the observable state. The
compiler enforces the mechanical half: you cannot assign to non-mutable
members, and you cannot call non-const members.

```cpp
void audit(const Account& a) {
    a.balance();            // ok if balance() is const
    // a.deposit(10);       // error
}
```

Pass `const T&` (or `T` by value, for cheap types) when the callee only
observes. The signature is the documentation, and it is checked.

### Logical const vs bitwise const

Bitwise const: no bit of the object changes. Logical const: no change an
observer can detect through the public interface. Caches and mutexes change
bits and change nothing an observer would call "the value."

```cpp
class Table {
    std::vector<int> data_;
    mutable std::mutex mu_;
    mutable bool cached_ = false;
    mutable long sum_ = 0;
public:
    long sum() const {
        std::lock_guard<std::mutex> lock(mu_);
        if (!cached_) {
            sum_ = 0;
            for (int v : data_) sum_ += v;
            cached_ = true;
        }
        return sum_;
    }
    void push(int v) {
        std::lock_guard<std::mutex> lock(mu_);
        data_.push_back(v);
        cached_ = false;
    }
};
```

`mutable` is the keyword that permits this. It is also the keyword that lets
a `const` function quietly break the object. Use it for synchronization and
for caches that are functions of the other members. Do not use it to avoid
removing `const` from a function that genuinely updates the value.

`const_cast` to write to an object that was *defined* as const is undefined
behavior. `const_cast` to write to an object that is not const, through a
`const` path you are sure about, is defined and almost always a design
problem. Fix the signature.

### Const overload pairs

```cpp
class Buffer {
    char* data_;
public:
    char&       operator[](std::size_t i)       { return data_[i]; }
    const char& operator[](std::size_t i) const { return data_[i]; }
};
```

The two overloads are how a const object observes and a mutable object
modifies, without casting. Chapter 01 covers the ref-qualified variants.

---

## 6. Law of Demeter, stated practically

A member function may talk to:

```
   * this
   * its parameters
   * objects it creates
   * its direct members
```

`order.customer().address().zip()` couples `order`'s caller to three
representations. When `Address` changes, every chain breaks. A method
`order.shipping_zip()` hides the chain. This is a heuristic about coupling,
not a ban on returning objects. Returning a `span` or a `string_view` into
data you own is normal. Returning an internal object so the caller can drive
it is how invariants leak.

---

## 7. The pimpl idiom

**Pimpl** (pointer to implementation) puts the representation in a `.cpp`
file behind an incomplete type. Clients include a header that does not
change when the representation changes, and the ABI of the class stays "one
pointer."

```cpp
// widget.hpp
#include <memory>

class Widget {
public:
    Widget();
    ~Widget();
    Widget(Widget&&) noexcept;
    Widget& operator=(Widget&&) noexcept;
    Widget(const Widget&);
    Widget& operator=(const Widget&);

    void do_work();
    int size() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
```

```cpp
// widget.cpp
#include "widget.hpp"
#include <vector>

struct Widget::Impl {
    int cache = 0;
    std::vector<int> data;
};

Widget::Widget() : impl_(std::make_unique<Impl>()) {}
Widget::~Widget() = default;
Widget::Widget(Widget&&) noexcept = default;
Widget& Widget::operator=(Widget&&) noexcept = default;

Widget::Widget(const Widget& o) : impl_(std::make_unique<Impl>(*o.impl_)) {}
Widget& Widget::operator=(const Widget& o) {
    if (this != &o) *impl_ = *o.impl_;
    return *this;
}

void Widget::do_work() { impl_->data.push_back(impl_->cache); }
int Widget::size() const { return static_cast<int>(impl_->data.size()); }
```

Why every special member is out of line: `std::unique_ptr<Impl>`'s destructor
and the destroying move-assignment instantiate `delete` on `Impl*`. That
requires `Impl` to be complete. If `~Widget()` is inline in the header,
every caller instantiates that destructor where `Impl` is still incomplete,
which is undefined or a hard error depending on the standard-library version.
Defining `~Widget() = default;` in the `.cpp`, after `Impl` is defined, is
the fix. Do the same for the move operations. This is the Rule of Five
showing up even though you never wrote a line of resource management
(chapter 04).

### Const does not propagate through `unique_ptr`

```cpp
int Widget::size() const { return impl_->data.size(); }
```

`size()` is const. `impl_` is a `unique_ptr` member, so it cannot be
reseated. `unique_ptr::operator->() const` returns `Impl*`, not
`const Impl*`. A const method can call non-const methods on `*impl_` and the
compiler will accept it. The const on `Widget` is then a comment about the
shell, not about the representation.

Fixes, from lightest to heaviest:

```
   * Only call const operations on *impl_ inside const methods, and review it.
   * Propagate const with a small wrapper (std::experimental::propagate_const,
     or your own) whose const operator-> returns const Impl*.
   * Split queries so the const path takes const Impl&.
```

`propagate_const` is not in the C++ standard. The technique is: a pointer-like
member whose `operator->() const` returns `const T*`.

### Costs

```
   * One heap allocation per object, plus a pointer chase on every access.
   * The header cannot inline anything that touches Impl.
   * Copying Widget copies the whole Impl (you wrote that copy constructor).
   * A move is a pointer move, and it is noexcept if you defaulted it that way.
```

Use pimpl when the representation is large, volatile, or part of a stable
ABI (a shared library's public class). Do not use it for a two-int value
type. A forward declaration of a type you only hold by reference is the
lighter compile-time firewall and does not need a heap allocation.

Runnable: [`examples/ch03_pimpl.cpp`](examples/ch03_pimpl.cpp).

---

## 8. The header is the contract

```
   Public section of the header   callers may depend on this
   Private section                callers should not, but they recompile when
                                  it changes, and it is part of the ABI unless
                                  you pimpl it
   .cpp file                      invisible to callers
```

Minimizing the public surface is what makes later refactoring possible.
Adding a public function is source-compatible. Removing one, changing a
signature, or adding a data member to a class that is standard-layout and
copied across a shared-library boundary, is not.

Forward-declare types that appear only as pointers or references in the
header. Include the header that defines a type when you contain it by value,
inherit from it, or use its members inline.

---

## 9. Worked example: a stack that can only be a stack

```cpp
#include <stdexcept>
#include <utility>
#include <vector>

template <class T>
class Stack {
public:
    bool empty() const { return data_.empty(); }
    std::size_t size() const { return data_.size(); }

    void push(T value) { data_.push_back(std::move(value)); }

    T pop() {
        if (data_.empty()) throw std::out_of_range("pop from empty stack");
        T top = std::move(data_.back());
        data_.pop_back();
        return top;
    }

    const T& top() const {
        if (data_.empty()) throw std::out_of_range("top of empty stack");
        return data_.back();
    }

private:
    std::vector<T> data_;
};
```

`data_` is private, so `insert`, `operator[]`, and `clear` are not part of
the interface. The LIFO invariant is structural: the only mutating operations
push onto the back and pop from the back. `top()` returns `const T&` because
a mutable reference would let the caller replace an element but not reorder
the stack — often acceptable, and worth a deliberate choice. Here the stack
does not offer it.

`std::stack` is this design. It is a container adapter, implemented by
composition, not by inheriting `std::vector` (chapter 10).

Runnable: [`examples/ch03_stack.cpp`](examples/ch03_stack.cpp).

---

## 10. Exercises

1. `class D : public B` and `B` has `protected: void reset();`. Why is
   `void D::f(B& b) { b.reset(); }` ill-formed, while `void D::f() { reset(); }`
   is fine?
2. A `const` method updates a cache through a `mutable` member and also
   appends to a log vector that callers can read back. Which of those two
   updates matches logical const?
3. You default `~Widget()` inside the class that holds `unique_ptr<Impl>`
   with `Impl` only forward-declared. What goes wrong, and where should the
   destructor be defined?
4. `string& Account::owner() { return owner_; }` lets a caller clear the
   owner and bypass `rename()`, which rejects empty strings. What should the
   signature be?

### Answers

1. Protected access through an object requires that object's type to be `D`
   or a class derived from `D`. A bare `B&` might refer to a sibling.
   `reset()` with no object expression uses `*this`, which is a `D`.
2. The cache. It is determined by the other members and is not part of the
   observable value. The log is observable state; updating it in a `const`
   method lies about the function. Drop `const` or stop logging there.
3. Deleting `Impl` through `unique_ptr` requires a complete type. Inline
   `~Widget()` instantiates that delete in every caller, where `Impl` is
   incomplete. Define `Widget::~Widget() = default;` in the `.cpp` after
   `struct Widget::Impl { ... };`. Default the move operations there too.
4. `const std::string& owner() const`, or `std::string owner() const` if you
   want a copy. Mutation goes through `rename`.

---

## 11. Summary

<!--diagram
title: Encapsulation & access control
box[green] Key points
  text: public / protected / private are compile-time and per class. A Widget method can read another Widget's privates
  text: Protected access through an object only works when that object is the derived class (or further derived), not an arbitrary base or a sibling
  text: Invariants hold between public calls. Constructors establish them. Getters and setters that expose every field do not create an invariant
  text: const is part of the interface. mutable is for caches and mutexes. const_cast on a truly const object is undefined
  text: Pimpl hides the representation and stabilizes sizeof, but the destructor and moves must be defined where Impl is complete, and const does not propagate through unique_ptr
-->
```
 +-------------------------------------------------------------------+
 | Access is compile-time and per class, not per object.            |
 | Protected + an object: the object must be the derived type.      |
 | Invariants hold between calls. Ctors establish them or throw.    |
 | Blind get/set pairs do not encapsulate. Do not return mutable    |
 |   references to private data.                                     |
 | const member functions are part of the type. mutable = cache or  |
 |   mutex. const_cast on a const object is UB.                      |
 | Pimpl: define ~T and the moves in the .cpp. Const does not reach |
 |   *unique_ptr unless you propagate it.                           |
 +-------------------------------------------------------------------+
```

Next: [04-copy-move-rule-of-five.md](04-copy-move-rule-of-five.md).
