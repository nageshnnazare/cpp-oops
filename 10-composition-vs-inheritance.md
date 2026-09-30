# 10 — Composition vs Inheritance

Public inheritance publishes a base interface and promises substitutability.
A data member publishes nothing. Most "I need to reuse this" designs want
the second one. This chapter is how to tell, and what to do when the answer
is a member, a reference, or a private base.

Prereq: [05-inheritance.md](05-inheritance.md).

---

## 1. Four relationships

```
   is-a            public inheritance     Dog is an Animal
   has-a, owns     member object          Car contains an Engine
                                          Engine's lifetime is nested in Car's
   has-a, refers   pointer or reference   Team refers to Players who outlive it
   uses-a          parameter              Report::print(Printer&) does not store it
```

```
   Car ──owns──► Engine          destroyed with the Car (RAII)
   Team ──ref──► Player          Player is not destroyed with the Team
   Report ──uses► Printer        a parameter, not a member
   Dog ──is──► Animal            public inheritance, and LSP holds
```

Ownership is a lifetime statement, not a UML decoration. If the member is a
value, the outer destructor destroys it. If the member is a
`unique_ptr`, the outer object is still the unique owner. If the member is
a raw pointer or a reference, someone else owns it, and the class's
invariant must say what happens if that someone destroys it first. A
`weak_ptr` is the vocabulary type for "I refer to a shared object and I can
detect that it is gone" (chapter 14).

Aggregation in older textbooks means "refers to, does not own." Composition
means "owns, nested lifetime." C++ expresses the difference in the type of
the member, which is better than a comment.

---

## 2. Composition

```cpp
class Engine {
public:
    void start() {}
    void stop() {}
};

class Car {
public:
    void drive() { engine_.start(); }
private:
    Engine engine_;
};
```

```
   Car
   +----------------------+
   | Engine engine_       |   created and destroyed with the Car
   +----------------------+
```

Callers cannot pass a `Car` where an `Engine` is required, because a `Car`
is not an `Engine`. The engine's public functions are not on `Car`. `drive`
is the interface; `start` is an implementation detail. That is encapsulation
applied to a relationship.

A member subobject is initialized in declaration order, before the
constructor body (chapter 02). If `Engine`'s constructor throws, `Car`'s
destructor does not run, and members that were already constructed are
destroyed. Owning real resources through members is what makes that sentence
safe.

---

## 3. Referring without owning

```cpp
class Player { /* ... */ };

class Team {
public:
    void add(Player& p) { members_.push_back(&p); }
private:
    std::vector<Player*> members_;     // non-owning
};
```

Destroying the `Team` does not destroy the `Player`s. Destroying a `Player`
while the `Team` still holds its address leaves a dangling pointer. The type
system does not catch that. Ways to make the contract visible:

```
   Player& or Player*          caller guarantees the lifetime, documented
   std::reference_wrapper<Player>    same contract, assignable, lives in containers
   std::weak_ptr<Player>       Player is owned by shared_ptr elsewhere
   observer stored and unregistered in Player's destructor
                               the Player knows about its observers
```

`shared_ptr` for every player "to be safe" shares ownership with everyone and
creates cycles (chapter 15). Non-owning pointers are the right tool when the
lifetime really is someone else's. Say so in the constructor's
precondition.

---

## 4. What inheritance couples

A derived class depends on:

```
   * the base's protected interface and, in practice, its private layout
   * the order of virtual calls the base makes during its public functions
   * which functions are virtual, const, and noexcept
   * the base's invariants, including the ones nobody wrote down
```

Changing a base function's behavior can break a derived class that overrode
something else and relied on the old order. That is the **fragile base
class** problem. The derived class was compiled against a base contract that
existed only as "whatever the base does today."

Composition depends on the member's public interface. You can replace the
member's type with another type that offers the functions you call. You
cannot accidentally expose `insert` to your callers.

Inheritance also fixes the variation to one axis. A `Window` base with
`BorderedWindow`, `ScrollingWindow`, and `BorderedScrollingWindow` derived
classes grows a class per combination. A `Window` that *contains* a border
policy and a scroll policy grows two members (section 6).

---

## 5. The stack that inherited a vector

```cpp
template <class T>
class Stack : public std::vector<T> {
public:
    void push(const T& x) { this->push_back(x); }
    void pop() { this->pop_back(); }
};

Stack<int> s;
s.push(1);
s.insert(s.begin(), 5);     // vector::insert, published by public inheritance
s[0] = 99;                  // not a stack anymore
```

`std::vector`'s destructor is not virtual. Even if you liked the interface,
deleting a `Stack` through a `vector*` would be undefined. The design is
wrong before you get to that, because a stack's invariant is LIFO and
`insert` breaks it.

```cpp
template <class T>
class Stack {
public:
    void push(T x) { data_.push_back(std::move(x)); }
    void pop() { data_.pop_back(); }
    const T& top() const { return data_.back(); }
    bool empty() const { return data_.empty(); }
private:
    std::vector<T> data_;
};
```

This is what `std::stack` is: a container adapter. The underlying container
is a template parameter with a default of `deque`, held as a member. The
protected member `c` in the real `std::stack` exists so further adapters can
reach it; your stack can keep `data_` private.

Runnable: [`examples/ch10_stack_composition.cpp`](examples/ch10_stack_composition.cpp).

---

## 6. Vary behavior by injecting it

Subclassing to change one step produces one class per variant and a
combinatorial explosion when two steps vary.

```cpp
class Sorter {
public:
    explicit Sorter(std::function<bool(int, int)> cmp) : cmp_(std::move(cmp)) {}
    void sort(std::vector<int>& v) const {
        std::sort(v.begin(), v.end(), cmp_);
    }
private:
    std::function<bool(int, int)> cmp_;
};

Sorter ascending([](int a, int b) { return a < b; });
Sorter descending([](int a, int b) { return a > b; });
```

The strategy is a member. It can be replaced at run time. A lambda does not
need a base class. If the strategy must be inlined and the set is known at
compile time, a template parameter is the same design with no `std::function`
and no indirect call:

```cpp
template <class Cmp>
void sort_with(std::vector<int>& v, Cmp cmp) {
    std::sort(v.begin(), v.end(), cmp);
}
```

Chapter 13 names the runtime version Strategy. The thing to copy is the
structure, not the name: the varying part is data, and the stable part is
the class that uses it.

---

## 7. Private inheritance, when a member will not do

Private inheritance is "implemented in terms of," with two extra powers a
member does not have:

```
   * the derived class can override the base's virtual functions
   * the derived class can access the base's protected members
   * an empty base can occupy zero bytes (EBO), which a member cannot
     without [[no_unique_address]]
```

```cpp
class Impl : private std::vector<int> {
public:
    void push(int x) { push_back(x); }     // using the base as a hidden vector
};
// Impl is not a vector. vector* p = &impl; is ill-formed outside Impl.
```

If you do not need an override, protected access, or a zero-size empty
policy, a member is simpler and does not participate in overload lookup of
the base's functions. Empty stateless policies in modern C++:

```cpp
template <class Deleter>
class Handle {
    [[no_unique_address]] Deleter del_;
    void* ptr_ = nullptr;
};
```

`sizeof(Handle<DefaultDelete>)` can equal `sizeof(void*)` when `Deleter` is
empty. That used to require inheriting the deleter. Chapter 17 shows both
layouts.

---

## 8. A decision procedure

Ask in order:

```
   1. Is every public operation of the base still correct for the derived
      type, with the same preconditions and postconditions?
        No  -> do not inherit publicly. Compose.
   2. Do you need a heterogeneous collection or a runtime-selected override?
        No  -> you probably do not need inheritance at all.
   3. Would the base interface let callers break the derived invariant?
        Yes -> compose, and expose only the operations that preserve it.
   4. Are you inheriting only to call some of the base's functions?
        That is reuse. Compose, or write a free function.
```

```
   Inherit to be substitutable.
   Compose to reuse, to own, and to vary a part.
```

The square/rectangle pair fails step 1. `Rectangle` promises that
`set_width` does not change the height. `Square` cannot keep that promise
and keep equal sides. They are both shapes. Neither is a subtype of the
other. Chapter 12 works the contracts.

---

## 9. Bridge: the interface and the implementation as two objects

When the abstraction and the implementation should vary independently, the
abstraction *owns* an implementation pointer.

```cpp
class Renderer {
public:
    virtual void draw_circle(double x, double y, double r) = 0;
    virtual ~Renderer() = default;
};

class Shape {
public:
    explicit Shape(std::unique_ptr<Renderer> r) : renderer_(std::move(r)) {}
    virtual void paint() const = 0;
    virtual ~Shape() = default;
protected:
    Renderer& renderer() const { return *renderer_; }
private:
    std::unique_ptr<Renderer> renderer_;
};
```

`Circle` can override `paint` without knowing whether the renderer is OpenGL
or a test double. `OpenGLRenderer` can change without a `Circle` subclass.
That is two hierarchies connected by composition. It is also pimpl, one
level up (chapter 03). Use it when both sides actually vary. A single
concrete renderer does not need a `Renderer` base; a member of that concrete
type is enough until a second one exists.

---

## 10. Exercises

1. Name the lifetime relationship in each snippet: `Engine engine_;` inside
   `Car`, `Player* captain_;` inside `Team`, `void print(std::ostream&)` on
   `Report`.
2. Why does `class Stack : public std::vector<T>` fail the is-a test even
   before you consider the non-virtual destructor?
3. You want a stateless deleter to add zero bytes to a handle. Give two
   techniques, one from before C++20 and one from C++20.
4. Two policies vary independently, and you need to swap one of them at run
   time. Why does a subclass per combination lose?

### Answers

1. `engine_` is owned; its lifetime is nested in `Car`. `captain_` does not
   own the `Player`; the `Team` must not outlive the player it points at,
   or it must detect that. `ostream&` is a use, not stored.
2. `vector::insert`, `operator[]`, and `clear` become public `Stack`
   operations and break LIFO. A function that takes a `vector&` and inserts
   in the middle is legal for a vector and fatal for a stack. The derived
   type is not substitutable for the base, and the base's interface is not
   safe on the derived type.
3. Private inheritance of an empty deleter (empty base optimization), or a
   member marked `[[no_unique_address]]`.
4. Each new option on either axis multiplies the number of classes. A member
   (a strategy object or a `std::function`) lets you choose each axis
   separately, including after construction.

---

## 11. Summary

<!--diagram
title: Composition vs inheritance
box[green] Key points
  text: Public inheritance is substitutability. A member is ownership or reuse without publishing the member's interface
  text: A value member's lifetime is nested. A pointer member does not own. Say which one you mean in the type
  text: Fragile bases and combinatorial subclasses are what inheritance costs. Inject the varying part
  text: Private inheritance is for overrides, protected access, or empty bases. [[no_unique_address]] covers the empty case for members
  text: A bridge splits a varying abstraction from a varying implementation by owning the implementation through an interface
-->
```
 +------------------------------------------------------------------+
 | Public inheritance = substitutable is-a. Otherwise compose.      |
 | Value member: nested lifetime. Pointer/reference: no ownership.  |
 | Do not inherit a container to reuse it. Adapt it.                |
 | Vary a behavior by injecting a member, not by a subclass matrix. |
 | Private inheritance: override, protected access, or EBO.         |
 | Bridge: the abstraction owns a pointer to the implementation.    |
 +------------------------------------------------------------------+
```

Next: [11-static-friends-nested.md](11-static-friends-nested.md).
