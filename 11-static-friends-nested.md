# 11 — Static Members, Friends & Nested Classes

Three supporting features that round out class design: **static** members (shared
across all objects), **friends** (controlled access grants), and **nested**
classes (types scoped inside a class).

Prereq: [03-encapsulation.md](03-encapsulation.md).

---

## 1. Static data members — one per class, not per object

A `static` data member is shared by **all** instances; there's exactly one copy,
belonging to the class itself.

```cpp
class Widget {
    static inline int count_ = 0;   // C++17: define + init inline in the header
public:
    Widget()  { ++count_; }
    ~Widget() { --count_; }
    static int count() { return count_; }   // static member function (see §3)
};
```

```
   Widget a, b, c;
   Widget::count();      // 3  -> ONE shared counter for the whole class

   Object layout is UNAFFECTED by static members:
     a: [ ...instance data... ]   b: [ ... ]   c: [ ... ]
   count_ lives ONCE, outside every object (in static storage):
     Widget::count_ = 3
```

```
   Access:  Widget::count_   (via the class, not an object).
   sizeof(Widget) does NOT include static members.
```

### Defining static members (pre-C++17)

```cpp
// header:
class Foo { static int n_; };
// exactly ONE .cpp:
int Foo::n_ = 0;         // out-of-line definition (needed before C++17)
```

```
   C++17 'static inline' lets you define+initialize in the class body (header),
   avoiding the separate .cpp definition. Prefer it.
```

### `static constexpr` constants

```cpp
class Circle {
public:
    static constexpr double pi = 3.14159265358979;   // compile-time constant
};
double area(double r) { return Circle::pi * r * r; }
```

---

## 2. Static members and the Singleton (Meyers' idiom)

A common use of a static local: the thread-safe lazy singleton.

```cpp
class Logger {
public:
    static Logger& instance() {
        static Logger inst;      // created ONCE, thread-safe since C++11
        return inst;
    }
    void log(const std::string& s) { /* ... */ }
private:
    Logger() = default;                      // private ctor -> no other instances
    Logger(const Logger&) = delete;          // non-copyable
    Logger& operator=(const Logger&) = delete;
};

// Logger::instance().log("hi");
```

```
   static local 'inst' is initialized on FIRST call, exactly once, and its
   construction is thread-safe (the compiler adds a guard). This is the simplest
   correct Singleton in C++. (But: singletons are often overused — chapter 13.)
```

---

## 3. Static member functions

A `static` member function belongs to the class, not an object: it has **no
`this`** and can only touch static members (or things passed to it).

```cpp
class Math {
public:
    static int square(int x) { return x * x; }   // no 'this'; call as Math::square(5)
};
Math::square(5);          // 25 — no object needed
```

```
   static method:  no 'this', can't access non-static members directly.
   Use for: factory functions, utility functions grouped under the class,
   operations on static state (like Widget::count()).
```

---

## 4. `friend` — granting access to your privates

A `friend` declaration lets a specific external function or class access your
`private`/`protected` members. Friendship is **granted** by the class (you can't
take it), is **not** inherited, and is **not** transitive.

```cpp
class Account {
    double balance_;
public:
    explicit Account(double b) : balance_(b) {}

    friend class Auditor;                    // Auditor may access Account's privates
    friend double total(const Account&, const Account&);  // this function too
};

class Auditor {
public:
    bool suspicious(const Account& a) { return a.balance_ > 1e9; }  // reads private
};

double total(const Account& a, const Account& b) { return a.balance_ + b.balance_; }
```

```
   friend class X;        -> X's methods can access this class's private members.
   friend R f(args);      -> the free function f can access them.

   Friendship:
     * is GRANTED by the grantor (you say who your friends are)
     * is NOT symmetric (A friend of B doesn't make B friend of A)
     * is NOT inherited (a friend of Base is not a friend of Derived)
     * is NOT transitive (friend of a friend is not a friend)
```

### The hidden-friend idiom (best practice for operators)

```cpp
class Point {
    int x_, y_;
public:
    Point(int x, int y) : x_(x), y_(y) {}
    // defined INSIDE the class as a friend -> only found via ADL, per-class:
    friend bool operator==(const Point& a, const Point& b) {
        return a.x_ == b.x_ && a.y_ == b.y_;    // accesses privates
    }
    friend std::ostream& operator<<(std::ostream& os, const Point& p) {
        return os << '(' << p.x_ << ',' << p.y_ << ')';
    }
};
```

```
   "Hidden friend" = a friend function DEFINED in the class body. Benefits:
     * can access privates,
     * found only by argument-dependent lookup (doesn't pollute the namespace),
     * one definition per class (no ODR headaches).
   The idiomatic way to write operator<< / operator== that need private access.
```

```
   Use friends SPARINGLY — they couple the friend to your internals. Prefer
   public methods where possible; reserve friend for operators & tightly-related
   helper classes (e.g. an iterator that needs container internals).
```

---

## 5. Nested classes

A class defined inside another. It's a member type — scoped to the outer class,
useful for implementation details (iterators, nodes, builders).

```cpp
class LinkedList {
    struct Node {                     // nested: an implementation detail
        int value;
        Node* next;
    };
    Node* head_ = nullptr;

public:
    class Iterator {                  // nested public type: part of the interface
        Node* cur_;
    public:
        explicit Iterator(Node* n) : cur_(n) {}
        int& operator*() { return cur_->value; }
        Iterator& operator++() { cur_ = cur_->next; return *this; }
        bool operator!=(const Iterator& o) const { return cur_ != o.cur_; }
    };

    Iterator begin() { return Iterator(head_); }
    Iterator end()   { return Iterator(nullptr); }
    // ...
};
```

```
   Referred to as  LinkedList::Node,  LinkedList::Iterator.
   A nested class is a normal class that happens to be scoped inside the outer.
   * It does NOT get a hidden pointer to the outer object (unlike Java inner classes).
   * Since C++11, a nested class can access the OUTER class's private static and
     type members by name; it is implicitly a friend of the enclosing class.
   Use nested classes for tightly-coupled helpers (Node, Iterator, Builder).
```

---

## 6. Putting it together: an ID generator

```cpp
#include <iostream>

class Entity {
    static inline int next_id_ = 1;    // shared across all entities
    int id_;
public:
    Entity() : id_(next_id_++) {}      // each object grabs & bumps the shared id
    int id() const { return id_; }
    static int created() { return next_id_ - 1; }   // static query
};

int main() {
    Entity a, b, c;
    std::cout << a.id() << b.id() << c.id() << '\n';   // 123
    std::cout << "created: " << Entity::created() << '\n';  // 3
}
```

Runnable: [`examples/ch11_static_friend.cpp`](examples/ch11_static_friend.cpp).

---

## 7. Summary

<!--diagram
title: Static members, friends & nested classes
box[green] Static members
  text: **Static data member**: one shared copy for the whole class (not per object). Prefer `static inline` (C++17) to define in header. `static constexpr` for compile-time constants
  text: **Static method**: no `this`; for factories/utilities/static state. **Meyers singleton**: `static Local inst;` inside `instance()` — thread-safe lazy init
box[teal] Friend
  text: Grants access to privates; granted by the class; **not** inherited/transitive/symmetric. Use for operators (hidden friend idiom) and tightly-coupled helper classes — sparingly
box[purple] Nested class
  text: A member type (`Node`/`Iterator`/`Builder`); scoped to the outer, implicitly a friend of it, no hidden outer pointer
-->
```
 +-------------------------------------------------------------------+
 | STATIC data member: one shared copy for the whole class (not per  |
 |   object). Prefer 'static inline' (C++17) to define in header.    |
 |   'static constexpr' for compile-time constants.                  |
 | STATIC method: no 'this'; for factories/utilities/static state.   |
 | Meyers singleton: 'static Local inst;' inside instance() —        |
 |   thread-safe lazy init.                                          |
 |                                                                   |
 | FRIEND: grants access to privates; granted by the class; NOT      |
 |   inherited/transitive/symmetric. Use for operators (hidden       |
 |   friend idiom) and tightly-coupled helper classes. Sparingly.    |
 | NESTED class: a member type (Node/Iterator/Builder); scoped to    |
 |   the outer, implicitly a friend of it, no hidden outer pointer.  |
 +-------------------------------------------------------------------+
```

Next: [12-solid-principles.md](12-solid-principles.md).
