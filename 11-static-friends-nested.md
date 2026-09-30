# 11 — Static Members, Friends & Nested Classes

Three mechanisms sit beside ordinary members. A **static** member belongs to
the class, not to an object. A **friend** is a function or class you have
granted access to. A **nested** class is a member type, scoped inside the
outer class, with access to the outer class's privates and with no hidden
pointer to an outer object.

Prereq: [03-encapsulation.md](03-encapsulation.md).

---

## 1. Static data members

There is one copy, with static storage duration, shared by every object.

```cpp
class Widget {
public:
    Widget() { ++live_; }
    ~Widget() { --live_; }
    static int live() { return live_; }
private:
    static inline int live_ = 0;     // C++17: defined in the class
};
```

```
   a: [ Widget members ]     b: [ Widget members ]
   Widget::live_ lives once, in static storage, not inside a or b
   sizeof(Widget) does not include live_
```

`static inline` (C++17) is a definition, not just a declaration. The header
may be included in many translation units; the linker merges the inline
variable. Before C++17 the header held `static int live_;` and exactly one
`.cpp` held `int Widget::live_ = 0;`. Two definitions violated the ODR. Zero
definitions failed at link time with an undefined reference.

`static constexpr` members of literal type are compile-time constants. Since
C++17 a `static constexpr` data member is implicitly inline, so you rarely
need an out-of-line definition. Use them for names that belong to the type
(`Buffer::max_size`, `Color::red`) when a namespace-scope `constexpr` would
not communicate the owner.

A static data member may have an incomplete type in a few special cases; do
not rely on that. Initialize it with a constant expression when you can, so
its initialization is static (before any dynamic initialization) and cannot
participate in the order fiasco below.

---

## 2. The static initialization order fiasco

Within one translation unit, namespace-scope objects are dynamically
initialized in definition order. Across translation units, the order is
unspecified.

```cpp
// registry.cpp
Registry registry;             // constructor registers built-in items

// plugin.cpp
extern Registry registry;
Plugin p{registry};            // may run BEFORE registry's constructor
```

`p`'s constructor calls into `registry` while `registry`'s lifetime has not
started. That is undefined if it uses the object, and it is the bug behind
"works on my machine, fails in CI" when a link order changes.

Construct on first use. A function-local static is initialized the first
time control passes through its declaration, and since C++11 that
initialization is synchronized: if two threads race into the function, one
runs the constructor and the other waits.

```cpp
Registry& registry() {
    static Registry r;
    return r;
}
```

Recursive initialization of the same local static (the constructor calls
`registry()` again) is undefined. Do not re-enter it.

Destruction is the reverse of the completion of construction, after `main`
returns. A local static whose destructor calls `registry()` can run after
`registry`'s destructor. If shutdown order matters, do not depend on it.
Allocate with `new` and intentionally never destroy, or join the work before
`main` returns.

This is also why a global `std::string` or `std::mutex` used from another
global constructor is a defect. Pass dependencies into constructors
(chapter 12) and keep the process-lifetime objects behind functions.

---

## 3. Meyers' singleton

```cpp
class Logger {
public:
    static Logger& instance() {
        static Logger inst;
        return inst;
    }
    void log(const std::string& s);

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

private:
    Logger() = default;
};
```

The private constructor stops other code from creating a `Logger`. The
deleted copy operations stop a caller from copying the instance out. The
local static provides the one object, constructed on first call, in a
thread-safe way.

What this does not fix:

```
   * The dependency is invisible. A function that calls Logger::instance()
     cannot be tested against a different logger without a seam.
   * Destruction order at shutdown is still the fiasco, one step later.
   * A singleton that owns a scarce resource (a socket, a GPU context) hides
     who is using it.
```

Prefer passing a `Logger&` into the code that logs. Keep `instance()` for
the process-lifetime case you have measured a need for, and treat it as a
default argument at the boundary, not as a call sprinkled through the core.
Chapter 13 says the same thing in the language of patterns.

---

## 4. Static member functions

A static member function has no `this`. It cannot be `const` or `volatile`,
and it cannot be virtual. It can access private members of the class,
including the private members of an object someone passed in.

```cpp
class Math {
public:
    static int square(int x) { return x * x; }
};
int n = Math::square(5);
```

Use a static function when the operation belongs to the type and does not
use an instance: factories (`Widget::open`), counters (`Widget::live`),
named constructors (`Color::from_hex`). A free function in the class's
namespace is better when it does not need private access. ADL will find it,
and it will not clutter the member list.

A static member function has the same linkage rules as a free function. Define
it inline in the class or in one `.cpp`.

---

## 5. Friends

A friend declaration inside a class grants a function or another class access
to private and protected members. The class chooses its friends. There is no
way to claim friendship from outside.

```cpp
class Account {
public:
    explicit Account(double b) : balance_(b) {}
    friend class Auditor;
    friend double total(const Account& a, const Account& b);
private:
    double balance_;
};

class Auditor {
public:
    bool suspicious(const Account& a) const { return a.balance_ > 1e9; }
};

double total(const Account& a, const Account& b) {
    return a.balance_ + b.balance_;
}
```

Properties that surprise people coming from "friend means close":

```
   not symmetric      Account friends Auditor
                      does not make Auditor friend Account
   not transitive     a friend of Auditor is not a friend of Account
   not inherited      a friend of Account is not a friend of SavingsAccount
                      a friend of SavingsAccount is not a friend of Account
   not a member       a friend function has no this
   not mutual access  friendship does not grant the class access to the
                      friend's privates
```

A friend function defined inside the class is inline and is a hidden friend
when found only by ADL (chapter 09). That is the form to use for operators.

Friendship is the right tool for:

```
   * operators that need privates and should be free functions
   * a tightly coupled helper that is logically part of the class
     (an iterator, a builder, a test hook in a test-only build)
   * a factory in another class that must call a private constructor
```

It is the wrong tool for "I do not want to write a getter." A getter, or a
behavior method, keeps the access list inside the class. A friend published
in a header is part of the class's coupling: when the private layout
changes, the friend must change, and it must be recompiled.

You can friend a specific member function rather than a whole class, which
is a narrower grant:

```cpp
class Account {
    friend bool Auditor::suspicious(const Account&) const;
};
```

The friend's class must be defined first, far enough that the member
function's signature is known. The resulting header order is awkward. Most
code friends the class, or stops needing the friend.

---

## 6. Nested classes

```cpp
class LinkedList {
    struct Node {                  // private by default if LinkedList is a class
        int value;
        Node* next;
    };
    Node* head_ = nullptr;

public:
    class Iterator {
    public:
        explicit Iterator(Node* n) : cur_(n) {}
        int& operator*() const { return cur_->value; }
        Iterator& operator++() { cur_ = cur_->next; return *this; }
        bool operator!=(const Iterator& o) const { return cur_ != o.cur_; }
    private:
        Node* cur_;
    };

    Iterator begin() { return Iterator(head_); }
    Iterator end() { return Iterator(nullptr); }
};
```

`Node` and `Iterator` are members of `LinkedList`. Their names are
`LinkedList::Node` and `LinkedList::Iterator`. They do not receive a hidden
pointer to a `LinkedList`. If `Iterator` needs the list, pass it. This is
the opposite of a Java inner class.

Since C++11 a nested class is treated as a member for access purposes: it
may use the private members of the enclosing class. The enclosing class does
not automatically reach into the nested class's privates. In the example,
`Iterator` can see `Node` because `Node` is a private member *type* and
`Iterator` is nested in `LinkedList`. A function of `LinkedList` can use
`Node` because `Node` is private to `LinkedList` and the function is a
member.

Nested classes are the right scope for iterators, nodes, and pimpl `Impl`
types (chapter 03). They keep the helper's name out of the enclosing
namespace.

A **local class**, defined inside a function, may use type names,
enumerators, and static variables from the enclosing function. It may not
odr-use the function's automatic variables. Lambdas, which can capture,
replaced almost every reason to write a local class. Recognize the term so
the compiler diagnostic is familiar.

---

## 7. `thread_local`

`thread_local` storage exists once per thread. A `static thread_local` member
is a per-thread instance, constructed the first time that thread reaches the
declaration (for a function-local) or at thread start for some
namespace-scope cases. Use it for scratch buffers and for context that must
not be shared across threads. Do not use it as a back-door global to avoid
passing parameters. Destruction at thread exit has the same order hazards as
static destruction, on a smaller scale.

---

## 8. Worked example

```cpp
#include <iostream>

class Entity {
public:
    Entity() : id_(next_id_++) {}
    int id() const { return id_; }
    static int created() { return next_id_ - 1; }
private:
    static inline int next_id_ = 1;
    int id_;
};

int main() {
    Entity a, b, c;
    std::cout << a.id() << ' ' << b.id() << ' ' << c.id() << '\n';
    std::cout << "created: " << Entity::created() << '\n';
}
```

`next_id_` is not part of any `Entity` object's layout. Copying an `Entity`
copies `id_` and does not touch `next_id_`. If `Entity` should not be copied
with a duplicate id, delete the copy operations and decide what a move does
to the id. The example leaves the implicit copy alone so the counter stays
easy to see; a production identity type would not.

Runnable: [`examples/ch11_static_friend.cpp`](examples/ch11_static_friend.cpp).

---

## 9. Exercises

1. Why can `static inline int count_ = 0;` live in a header included by two
   `.cpp` files, when `int Widget::count_ = 0;` in that header cannot?
2. `Logger::instance()` uses a function-local static. What does C++11
   guarantee if two threads call `instance()` at the same time, before any
   logger exists?
3. `SavingsAccount` derives from `Account`. `Account` has `friend class
   Auditor`. Can `Auditor` read a private member declared in
   `SavingsAccount`? Can it read `Account`'s private balance through a
   `SavingsAccount` object?
4. Does a nested `Iterator` have a pointer to the `LinkedList` it came from,
   the way a Java inner class does?

### Answers

1. `static inline` has vague linkage. Each translation unit may emit the
   variable and the linker keeps one. A non-inline out-of-line definition in
   a header is a strong symbol in every translation unit, and the linker
   reports a multiple definition.
2. Exactly one thread runs `Logger`'s constructor. The other waits until
   that constructor finishes, then both receive the same reference.
   Re-entering `instance()` from inside `Logger`'s constructor is undefined.
3. `Auditor` can access `Account`'s privates, including the `Account`
   subobject inside a `SavingsAccount`. It cannot access private members
   declared in `SavingsAccount`. Friendship is not inherited.
4. No. A nested class is a member type with access rights, not an inner
   class with an enclosing instance. Pass a `LinkedList*` or a `Node*` into
   the iterator if it needs one. The example passes the `Node*`.

---

## 10. Summary

<!--diagram
title: Static members, friends & nested classes
box[green] Key points
  text: A static data member is one object for the class, outside every instance. static inline (C++17) defines it in the header
  text: Cross-TU static initialization order is unspecified. Construct on first use with a function-local static, which C++11 initializes in a thread-safe way
  text: A Meyers singleton is that local static plus a private constructor. It hides dependencies. Prefer passing the object in
  text: Friends are granted, not inherited, not transitive, not symmetric. Hidden friends are the right operators
  text: A nested class is a scoped member type with access to the outer privates and with no hidden outer pointer
-->
```
 +-------------------------------------------------------------------+
 | Static data: one per class, not in sizeof. static inline in the  |
 |   header since C++17.                                             |
 | Namespace-scope init order across TUs is unspecified. Use a       |
 |   function-local static. C++11 makes that init race-free.         |
 | Singleton = local static + private ctor. It is hidden global      |
 |   state. Pass dependencies instead when you can.                  |
 | friend: granted, not inherited, not transitive, not symmetric.    |
 | Nested class: scoped, can see outer privates, no outer this.      |
 +-------------------------------------------------------------------+
```

Next: [12-solid-principles.md](12-solid-principles.md).
