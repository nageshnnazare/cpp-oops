# 15 — Pitfalls & Best Practices

This is the catalogue. Each entry is a bug that compiles, what it actually
does, and the fix. The surrounding chapters are the long version.

---

## 1. Non-virtual destructor, polymorphic delete

```cpp
struct Base { ~Base(); };
struct Derived : Base { std::vector<int> data; };
Base* p = new Derived();
delete p;                              // undefined behavior
```

`delete` uses the static type when the destructor is not virtual. `~Derived`
does not run. `data` leaks, and the standard calls the whole delete undefined.

Fix: `virtual ~Base() = default;` if `Base*` is an owning pointer. Or a
protected non-virtual `~Base()` so `delete` on a `Base*` does not compile
(chapter 06, chapter 19).

---

## 2. Slicing

```cpp
std::vector<Animal> zoo;
zoo.push_back(Dog{"Rex"});             // copies the Animal subobject only
void feed(Animal a);                   // parameter is a different Animal
```

Fix: `const Animal&`, `unique_ptr<Animal>`, or `variant<Dog, Cat>`. Protect
the base copy constructor if slicing should be ill-formed (chapter 05,
chapter 04).

---

## 3. Virtual call in a constructor or destructor

```cpp
struct Base {
    Base() { setup(); }                // dynamic type is Base here
    virtual void setup() { /* Base */ }
};
```

The derived override does not run. Derived members are not initialized yet.
A call to a pure virtual in this window is undefined and typically aborts in
`__cxa_pure_virtual`. Fix: do derived work in the derived constructor, after
the base constructor has returned (chapter 06, chapter 18).

---

## 4. Default arguments on virtual functions

```cpp
struct Base    { virtual void f(int x = 1); };
struct Derived { void f(int x = 2) override; };
Base& b = derived;
b.f();                                 // Derived::f(1), not 2
```

The override is selected dynamically. The default is filled in from the
static type. Fix: one default, on a non-virtual public function that
forwards to a virtual function without a default (chapter 06, chapter 07).

---

## 5. Hiding is not overriding

```cpp
struct Base    { virtual void f() const; };
struct Derived : Base { void f(); };   // different function, not an override
```

`Base& b = d; b.f();` calls `Base::f`. Fix: `override` on every intended
override. The missing `const` becomes a compile error (chapter 06).

---

## 6. Rule of Three / Five, shallow copy

```cpp
class Buf {
    int* p_;
public:
    explicit Buf(int n) : p_(new int[n]) {}
    ~Buf() { delete[] p_; }
};
Buf a(10);
Buf b = a;                             // copies p_; both delete it
```

The user-declared destructor also suppressed the move operations, so
`std::move(a)` copies too. Fix: Rule of Zero with `vector` or `unique_ptr`,
or write all five special members, moves `noexcept` (chapter 04).

---

## 7. Self-assignment and self-move

```cpp
T& operator=(const T& o) {
    delete p_;                         // o and *this are the same object
    p_ = new U(*o.p_);                 // o.p_ is already freed
    return *this;
}
```

Fix: allocate the new resource before releasing the old one, or test
`this != &o`, or take the parameter by value and swap (chapter 04).
Self-move (`a = std::move(a)`) must leave `a` valid. Destroy-then-steal
without the identity test does not.

---

## 8. `return std::move(local)`

```cpp
Buffer make() {
    Buffer local(10);
    return std::move(local);           // blocks NRVO
}
```

Fix: `return local;`. The language moves if it does not elide. Reserve
`return std::move(x)` for a parameter or another expression that is not a
local automatic object (chapter 04).

---

## 9. Forwarding-reference constructor steals the copy

```cpp
template <class S>
Widget(S&& s) : name_(std::forward<S>(s)) {}

Widget a("a");
Widget b(a);                           // S = Widget&, not the copy constructor
```

A non-const lvalue prefers the template to `Widget(const Widget&)`. Fix:
constrain `S` so it is not `Widget`, or take `std::string` by value
(chapter 04).

---

## 10. Dangling references

```cpp
const std::string& name() const { return std::string("x"); }  // temporary dies
int& at(int i) { int local = data_[i]; return local; }        // local dies
const std::string& s = BankAccount{"a", 0}.owner();           // account dies
```

Fix: return by value, or return a reference to a member and do not bind it
to a temporary object. `string_view` and `span` into a temporary have the
same shape. If the function returns a view, the caller must own the buffer.

---

## 11. Returning a mutable reference to private state

```cpp
std::string& Account::owner() { return owner_; }   // bypasses rename()
```

The invariant "owner is non-empty" is now the caller's problem. Fix: return
`const std::string&` or a copy. Mutate through a function that checks
(chapter 03).

---

## 12. `const` that does not reach the pimpl

```cpp
int Widget::size() const { impl_->mutate(); }      // compiles
```

`unique_ptr::operator->() const` returns `Impl*`, not `const Impl*`. Fix:
propagate const, or only call const operations on `*impl_` from const
methods (chapter 03).

---

## 13. Pimpl destructor defined in the header

```cpp
class Widget {
    struct Impl;
    std::unique_ptr<Impl> impl_;
public:
    ~Widget() = default;               // inline, Impl is incomplete
};
```

Deleting an incomplete type is undefined. Fix: declare `~Widget()` and the
move operations in the header and default them in the `.cpp` after `Impl` is
defined (chapter 03).

---

## 14. Inheriting to reuse

```cpp
class Stack : public std::vector<int> {};
class Square : public Rectangle {};
```

`Stack` publishes `insert`. `Square` breaks `set_width`'s postcondition.
`vector`'s destructor is not virtual. Fix: a member `vector`, and separate
types for square and rectangle (chapters 10 and 12).

---

## 15. Protected data, and protected access through the wrong object

Protected data makes every derived class part of the representation. Prefer
private data and protected or public functions.

A derived class may use a protected member through its own type, not through
an arbitrary `Base&` or a sibling (chapter 03). `b.value_ = 1` inside
`Derived`, where `b` is a `Base&`, is ill-formed on purpose.

---

## 16. Name hiding

```cpp
struct Base { void f(int); void f(double); };
struct Derived : Base { void f(std::string); };
// d.f(1);                              // error, Base::f is hidden
```

Fix: `using Base::f;` in `Derived` (chapter 05). This is not virtual
dispatch. Add `override` when you did mean to override.

---

## 17. Object lifetime and `memcpy`

`memcpy` of a type that is not trivially copyable is undefined. A type with
a `string`, a `vector`, a virtual function, or a user-provided copy
constructor is not trivially copyable. Copy it with `=` or with the copy
constructor. `offsetof` is for standard-layout types (chapter 17).

Tail padding of a base can hold derived members. `memcpy` of the base
subobject using `sizeof(Base)` can overwrite them. Assign the base, or copy
the complete object.

---

## 18. `shared_ptr` cycles and `shared_ptr(this)`

```cpp
struct Node {
    std::shared_ptr<Node> next;
    std::shared_ptr<Node> prev;        // cycle if both directions are live
};
```

Fix: `weak_ptr` on the back edge. `shared_ptr<T>(this)` inside a member
creates a second control block and a double free. Fix:
`enable_shared_from_this`, used only after a `shared_ptr` owns the object,
never from the constructor (chapter 14).

---

## 19. `shared_ptr` as a default

`shared_ptr` costs an atomic refcount and a control block, and it blurs who
destroys the object. Fix: `unique_ptr` for ownership, `T&` or `T*` for use.
Reach for `shared_ptr` when two owners genuinely outlive each other's scopes.

---

## 20. Static initialization order

A namespace-scope object in `a.cpp` whose constructor uses a namespace-scope
object in `b.cpp` may run first. The order across translation units is
unspecified. Fix: a function-local static, which C++11 initializes once in a
thread-safe way (chapter 11). Do not call back into that function from its
own constructor.

---

## 21. Throwing destructors

Destructors are implicitly `noexcept`. A throw from a `noexcept` destructor
calls `std::terminate`. A throw during stack unwinding calls `std::terminate`
even if the destructor is `noexcept(false)`. Fix: destructors do not throw.
Log the failure, or store it, and finish releasing the resource (chapter 02).

---

## 22. Constructor failure leaks raw acquisitions

If the constructor throws after a raw `fopen` and before the object exists,
the destructor does not run. Fix: acquire into a RAII member so the member's
destructor runs when the constructor fails (chapter 02).

---

## 23. Missing `explicit`

```cpp
struct Timer { Timer(int seconds); };
void wait(Timer);
wait(5);                               // Timer(5), silently
```

Fix: `explicit` on converting constructors and on conversion operators,
except where the conversion is the type's actual model (chapter 02,
chapter 09).

---

## 24. Overloading `&&`, `||`, or comma

The built-in operators short-circuit and sequence their operands. The
overloads are ordinary function calls. Both sides run. Fix: do not overload
them (chapter 09).

---

## 25. `initializer_list` stealing a constructor

```cpp
std::vector<int> a(10, 2);             // ten 2s
std::vector<int> b{10, 2};             // two elements: 10 and 2
```

Brace initialization prefers an `initializer_list` constructor. Fix: know
which overload you are calling. Parentheses when you mean the count
constructor (chapter 02).

---

## 26. Most vexing parse

```cpp
Widget w();                            // declares a function
Widget w{};                            // defines an object
```

---

## 27. Diamond without `virtual`

Two non-virtual paths to `Animal` produce two `Animal` subobjects and an
ambiguous `Animal*` conversion. Fix: virtual inheritance on the shared base,
and initialize that base from the most-derived constructor. Or, more often,
do not build the diamond (chapter 08).

---

## 28. `reinterpret_cast` across bases

A secondary base does not live at offset 0. `reinterpret_cast` does not add
the offset. `static_cast` and the implicit conversion do. Cross-casts from
one sibling to another need `dynamic_cast` (chapters 08 and 20).

---

## 29. `dynamic_cast` and RTTI

`dynamic_cast` on a non-polymorphic type does not do a runtime check down
the hierarchy; the source must be polymorphic for a runtime cast.
`typeid(*p)` on a null polymorphic pointer throws `bad_typeid`. A reference
`dynamic_cast` throws `bad_cast` on failure; the pointer form returns null.
`-fno-rtti` removes both. Prefer a virtual function to a cast in a loop
(chapter 21).

---

## 30. Pointer-to-member is not a pointer

A pointer to a member function is often two words (function or vtable index,
plus a `this` adjustment). Casting it to `void (*)()` is undefined. Use
`std::invoke` or `std::mem_fn` (chapter 22).

---

## 31. Checklist

```
   CLASS
   [ ] Invariant stated. Data private. No mutable escape hatches.
   [ ] const on every function that does not change observable state.
   [ ] explicit on converting constructors.
   [ ] Rule of Zero, or all five special members, moves noexcept.
   [ ] No raw owning pointer.

   HIERARCHY
   [ ] Public inheritance only where LSP holds.
   [ ] virtual destructor, or a protected non-virtual one.
   [ ] override on overrides. final on leaves that should not grow.
   [ ] No virtual calls from constructors or destructors.
   [ ] No default arguments on virtual functions.
   [ ] No slicing: no by-value base parameters, no vector<Base>.

   OWNERSHIP
   [ ] unique_ptr by default, shared_ptr with a reason, weak_ptr for back edges.
   [ ] T& / T* for non-owning use.
   [ ] Factories return unique_ptr<Interface>.
   [ ] Pimpl special members defined where Impl is complete.

   MODERN CHOICE
   [ ] Closed set of types: variant, not a hierarchy.
   [ ] Open set: virtual.
   [ ] Comparisons: defaulted <=> when memberwise is the real order.
```

---

## 32. Tooling

```bash
clang++ -std=c++20 -Wall -Wextra -Wnon-virtual-dtor -Woverloaded-virtual \
        -fsanitize=address,undefined prog.cpp -o prog && ./prog
```

```
   -Wall -Wextra              reorder, unused, a lot of real bugs
   -Wnon-virtual-dtor         polymorphic base with a public non-virtual dtor
   -Woverloaded-virtual       a derived function hides a base virtual
   -fsanitize=address         use-after-free, double free, leaks
   -fsanitize=undefined       bad casts, null deref, signed overflow
   clang-tidy                 modernize-*, cppcoreguidelines-*
```

Sanitizers change codegen and slow the program down. Use them in tests, not
as the only production build. They do not catch every slicing bug: a sliced
copy is a well-defined `Animal`, it is just the wrong one. The type system
and the checklist catch that one.

---

## 33. Summary

<!--diagram
title: Pitfalls & best practices
box[red] Bugs that compile
  text: non-virtual delete, slicing, virtual call in a constructor, static default arguments, missing override, shallow copy, self-assignment, dangling return, shared_ptr cycles, cross-TU static init, incomplete-type pimpl destructor
box[green] Habits
  text: private data, const, explicit, Rule of Zero, virtual destructor plus override, composition, unique_ptr, variant for a closed set, warnings and sanitizers
-->
```
 +-------------------------------------------------------------------+
 | The bugs that compile are listed above, each with the chapter     |
 | that fixes it. The short habits: private data, const, explicit,   |
 | Rule of Zero, virtual destructor, override, no virtual calls in   |
 | ctors, composition for reuse, unique_ptr for ownership, variant   |
 | for a closed set, -Wall -Wextra and sanitizers in tests.          |
 +-------------------------------------------------------------------+
```

Next: [16-cheatsheet.md](16-cheatsheet.md).
