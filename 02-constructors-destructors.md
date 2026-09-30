# 02 — Constructors, Destructors & RAII

A constructor starts a lifetime and establishes the invariant. A destructor
ends the lifetime and releases what the object owns. **RAII** is the
discipline of tying those two together so every exit path, including
exceptions, runs the cleanup. Get this chapter right and leaks, double frees,
and "forgot to unlock" stop being a category of bug.

Prereq: [01-classes-and-objects.md](01-classes-and-objects.md).

---

## 1. What a constructor is

```cpp
class Widget {
public:
    Widget();                         // default constructor
    explicit Widget(int id);          // converting constructor, made explicit
    Widget(int id, std::string name);
    Widget(const Widget&);            // copy constructor (ch 04)
    Widget(Widget&&) noexcept;        // move constructor (ch 04)
};
```

A constructor has the class's name, no return type, and runs as part of
initialization. You do not call it like a normal function on an existing
object (placement new, below, is the exception that *creates* an object).
Constructors are overloadable. A constructor can be `constexpr`, `explicit`,
`noexcept`, and `= default` or `= delete`.

Declaring any constructor suppresses the implicit default constructor. If the
type should still be default-constructible, write `Widget() = default;`.

---

## 2. Initialization is not assignment

Members are initialized before the constructor body runs. The initializer
list *is* that initialization. Assignment in the body is a second step, and
for `const` members, references, and bases without a default constructor, the
second step is impossible.

```cpp
class Point {
    int x_;
    int y_;
    const int id_;
public:
    Point(int x, int y, int id) : x_(x), y_(y), id_(id) {}
};
```

```
   : x_(x)     constructs x_ with x          one initialization
   { x_ = x; } default-initializes x_, then assigns     two steps
```

For an `int` the cost is academic. For a `std::string` or a `std::vector`,
body-assignment default-constructs an empty object and then assigns over it.
For a member of a type with no default constructor, body-assignment does not
compile.

### Order is declaration order

```cpp
class Bad {
    int a_;
    int b_;
public:
    Bad(int x) : b_(x), a_(b_) {}   // a_ is initialized first, b_ is still
};                                  // uninitialized. The read of b_ is UB.
```

Members initialize in the order they are declared, not the order written in
the initializer list. Bases initialize before members. Compilers warn
(`-Wreorder`). Write the initializer list in declaration order so the source
matches the execution.

A default member initializer is used only when the constructor's initializer
list does not mention that member:

```cpp
class Config {
    int timeout_ = 30;
    std::string host_ = "localhost";
public:
    Config() = default;
    explicit Config(int t) : timeout_(t) {}   // host_ uses "localhost"
};
```

If both a default member initializer and a mem-initializer exist, the
mem-initializer wins and the default is not evaluated.

---

## 3. Kinds of initialization

The standard distinguishes several kinds. The ones you actually choose between:

```
   form                         kind                     for a class type
   ---------------------------  -----------------------  ---------------------------
   T t;                         default-initialization   default constructor
   T t{};                       value-initialization     see below
   T t{a, b};                   list-initialization      ctor, or aggregate init
   T t(a, b);                   direct-initialization    constructor
   T t = u;                     copy-initialization      converting ctor, or copy
   T t = {a};                   copy-list-initialization like list-init, but
                                                            explicit ctors are
                                                            not candidates
```

`T t;` on a class calls the default constructor and does nothing else. On an
`int` it leaves an indeterminate value. `T t{}` value-initializes: for a
class with a user-provided default constructor, that constructor runs; for a
class whose default constructor is implicit or `= default` and not
user-provided, the object is first zero-initialized and then default-initialized
if the default constructor is non-trivial. Practically: `int x{}` is 0,
`int x;` is garbage, and `std::string s{}` is empty.

Copy-list-initialization (`T t = {a}`) ignores `explicit` constructors.
Direct-list-initialization (`T t{a}`) considers them. This is why
`explicit` still protects `T t = {a}` and function arguments written as `{a}`
in a copy-initialization context, while `T t{a}` and `f({a})` in direct
contexts can call an explicit constructor.

### The `initializer_list` preference

If the type has a constructor taking `std::initializer_list<U>` and the brace
list can form one, list-initialization prefers that constructor over every
other constructor.

```cpp
std::vector<int> n(3, 1);     // three 1s
std::vector<int> m{3, 1};     // elements 3, 1
std::vector<int> e{};         // empty: an empty brace list does not call
                              // the initializer_list constructor with a
                              // one-element surprise; it value-initializes
```

When you design a constructor set, an `initializer_list` constructor will
steal brace initialization from your other overloads. Provide a parenthesized
path for the count/value meaning, and document it. The standard library's
`vector` is the canonical example of living with this rule.

---

## 4. `explicit` and `explicit(bool)`

A constructor that can be called with a single argument is a **converting
constructor**. It is a candidate for implicit conversions unless it is
`explicit`.

```cpp
class Meters {
public:
    Meters(double v) : v_(v) {}
    double v_;
};
void travel(Meters);
travel(5.0);                 // calls Meters(double) implicitly

class Grams {
public:
    explicit Grams(double v) : v_(v) {}
    double v_;
};
void weigh(Grams);
weigh(Grams{5.0});           // ok
// weigh(5.0);               // error
```

Make single-argument constructors `explicit` unless the conversion is part of
the type's value model (`std::string` from `const char*` is the classic
deliberate implicit conversion; a unit type is the classic non-conversion).

C++20 allows the explicitness to be conditional:

```cpp
template <class T>
class Wrapper {
public:
    template <class U>
    explicit(!std::is_convertible_v<U, T>)
    Wrapper(U&& u);
};
```

`explicit(false)` is an ordinary converting constructor. `explicit(true)` is
explicit. The condition is a constant expression.

---

## 5. `= default` and `= delete`

```cpp
class Foo {
public:
    Foo() = default;
    Foo(const Foo&) = delete;
    explicit Foo(int);
};
```

`= default` on the first declaration asks for the compiler's implementation
and can still be trivial (chapter 04). `= default` out of line, in a `.cpp`,
is user-provided: it is not trivial. That distinction matters for `memcpy`
and for unions.

`= delete` makes the function exist for overload resolution and then renders
the program ill-formed if it is selected. Deleting a converting constructor
is sometimes clearer than making it private and undefined, which was the
pre-C++11 trick.

---

## 6. Delegating constructors

A constructor may forward to another constructor of the same class. The
initializer list then contains only that one call.

```cpp
class Widget {
    int id_;
    std::string name_;
public:
    Widget(int id, std::string name) : id_(id), name_(std::move(name)) {}
    explicit Widget(int id) : Widget(id, "unnamed") {}
    Widget() : Widget(0) {}
};
```

Rules:

```
   * The target constructor is the one that initializes the members.
   * You cannot mix a delegating call with other mem-initializers.
   * The delegating constructor's body runs after the target returns.
   * If the target throws, the delegating body does not run.
   * A cycle (a constructor delegates, directly or indirectly, to itself) is
     ill-formed, no diagnostic required. Don't.
```

Delegating constructors are how you keep one "real" constructor that
establishes the invariant, and thin overloads that fill in defaults.

---

## 7. Inheriting constructors

```cpp
struct Base {
    explicit Base(int);
    Base(int, std::string);
};
struct Derived : Base {
    using Base::Base;          // inherits Base(int) and Base(int, string)
    int extra_ = 0;            // default member initializer covers this
};
Derived d(1);                  // calls Base(int); extra_ is 0
```

`using Base::Base` inherits constructors that are not the default, copy, or
move constructor. Each inherited constructor keeps the base's `explicit`,
`constexpr`, and access, with a twist: the inherited constructor is a
constructor of `Derived` that initializes the base with those arguments and
initializes `Derived`'s own members from their default member initializers.
A member with no default member initializer is default-initialized (its
default constructor runs; a raw `int` member is left indeterminate).

What it does not do:

```
   * It does not inherit the default, copy, or move constructor.
   * It does not run any Derived-specific constructor body. There is no body.
   * A member without a default member initializer is default-initialized,
     which fails if that member's type has no default constructor.
   * If Derived declares a constructor with the same signature, that one
     wins and hides the inherited one.
   * Default arguments are baked into the inherited constructor, and several
     signatures may be inherited from one base constructor with defaults.
```

If the derived class needs to compute something in a constructor body, write
a real constructor and call the base from its initializer list. Inheriting
constructors are for the "I add nothing but a member with a default" case.

---

## 8. Destructors

```cpp
class FileHandle {
    std::FILE* f_;
public:
    explicit FileHandle(const char* path) : f_(std::fopen(path, "r")) {}
    ~FileHandle() { if (f_) std::fclose(f_); }
    FileHandle(const FileHandle&) = delete;
    FileHandle& operator=(const FileHandle&) = delete;
};
```

The destructor is `~ClassName()`, takes no arguments, and has no return type.
It is called automatically when the lifetime ends: end of scope, `delete`,
the end of a temporary's full expression, stack unwinding, or an explicit
destructor call after placement new. You almost never call it yourself. The
one time you do is paired with placement new (section 14).

A destructor is implicitly `noexcept` unless a member or base destructor is
potentially throwing, or you wrote `noexcept(false)`. If a `noexcept`
destructor throws, `std::terminate` is called. If a destructor throws while
another exception is already unwinding the stack, `std::terminate` is called
regardless. Destructors do not throw. If cleanup can fail, report it some
other way (an error code stored earlier, a log) and still complete the
destructor.

A polymorphic base that is deleted through a base pointer needs a virtual
destructor. That rule is chapter 06; the machine-level reason (the deleting
destructor slot) is chapter 19. A base that should *never* be deleted through
a base pointer can have a protected non-virtual destructor, so the mistake is
a compile error.

If you define a destructor, the implicit move operations are not generated
(chapter 04). A class that only needs a destructor because of an incomplete
type (`unique_ptr` to a pimpl) should `= default` the destructor out of line
and `= default` the move operations next to it.

---

## 9. RAII

**Resource Acquisition Is Initialization**: acquire in the constructor,
release in the destructor. Because the destructor runs on every exit path,
the resource is released on return, on `break`, on `goto` out of the block,
and on a thrown exception.

```cpp
{
    FileHandle f("data.txt");
    use(f);
    if (error) throw std::runtime_error("fail");
}   // ~FileHandle runs here, including when use() or the throw unwinds
```

```
   enter scope   constructor acquires (file, lock, memory, socket)
   leave scope   destructor releases, on every path
```

The standard library is a catalogue of RAII types:

```
   std::string, std::vector, std::unique_ptr     memory
   std::fstream                                  files
   std::lock_guard, std::unique_lock, std::scoped_lock    mutexes
   std::jthread                                  joins on destruction
```

A `lock_guard` is the pattern in miniature. The constructor locks, the
destructor unlocks, and there is no `unlock()` the caller must remember:

```cpp
void bump(std::mutex& m, int& counter) {
    std::lock_guard<std::mutex> lock(m);
    ++counter;
}   // unlocked here, even if ++ were replaced by something that throws
```

### A scope guard

When the cleanup is a one-off action and does not deserve a named type:

```cpp
template <class F>
class ScopeExit {
    F fn_;
    bool active_ = true;
public:
    explicit ScopeExit(F fn) : fn_(std::move(fn)) {}
    ScopeExit(ScopeExit&& o) noexcept : fn_(std::move(o.fn_)), active_(o.active_) {
        o.active_ = false;
    }
    ScopeExit(const ScopeExit&) = delete;
    ScopeExit& operator=(const ScopeExit&) = delete;
    ~ScopeExit() { if (active_) fn_(); }
    void release() { active_ = false; }
};
```

`std::experimental::scope_exit` and the C++ working-draft `std::scope_exit`
are this idea standardized. Until you can rely on them, a ten-line guard is
enough. The guard itself must not throw from its destructor.

### Two-phase initialization

```cpp
Widget w;          // "empty" object, invariant not yet true
w.init(args);      // easy to forget; w is observable in a half-ready state
```

Prefer a constructor that either succeeds with the invariant established or
throws (or a factory that returns `std::optional` / `std::expected`) so a
live object is always valid. A private constructor plus a static factory is
the pattern when failure is an expected outcome rather than an exception:

```cpp
class Connection {
    explicit Connection(Socket s) : socket_(std::move(s)) {}
    Socket socket_;
public:
    static std::optional<Connection> open(const std::string& host);
};
```

Callers cannot build a `Connection` that is not open, because the constructor
is private and `open` returns a value only on success.

Runnable: [`examples/ch02_raii.cpp`](examples/ch02_raii.cpp),
[`examples/ch02_order.cpp`](examples/ch02_order.cpp),
[`examples/ch02_init.cpp`](examples/ch02_init.cpp).

---

## 10. Construction and destruction order

```
   Constructing a complete object:
     1. virtual base classes, depth-first, left to right, once
        (initialized by the most-derived constructor — chapter 08)
     2. direct non-virtual bases, left to right as written in the base-clause
     3. non-static data members, in declaration order
     4. the constructor body

   Destroying:
     4. the destructor body
     3. members, reverse declaration order
     2. non-virtual bases, reverse order
     1. virtual bases, reverse of construction
```

```cpp
struct A { A() { std::puts("A"); } ~A() { std::puts("~A"); } };
struct B { B() { std::puts("B"); } ~B() { std::puts("~B"); } };
struct C {
    A a;
    B b;
    C() { std::puts("C"); }
    ~C() { std::puts("~C"); }
};
// C c;  prints:  A  B  C  ~C  ~B  ~A
```

Automatic objects in a block are destroyed in reverse order of the completion
of their construction. Temporaries are destroyed at the end of the full
expression, with the special case that a temporary bound to a reference in a
function parameter lives until the end of the full call expression, and a
temporary bound to a local reference extends to the reference's lifetime.

Members are destroyed after the destructor body. The body may still use the
members. It must not use them after an explicit early destruction.

Runnable: [`examples/ch02_order.cpp`](examples/ch02_order.cpp).

---

## 11. If the constructor throws

Lifetime begins only when the constructor completes. If it throws:

```
   * Subobjects whose constructors finished are destroyed, in reverse order.
   * Subobjects whose constructors did not start are not destroyed.
   * The destructor of the object under construction does not run.
   * A new-expression deallocates the memory if the constructor throws
     (the matching operator delete is called).
```

```cpp
struct Boom {
    Boom() { throw std::runtime_error("nope"); }
};
struct Holder {
    std::string name = "ok";   // constructed first; its destructor runs
    Boom boom;                 // throws; ~Holder does NOT run
};
```

This is why a constructor that acquires two raw resources is fragile: if the
second acquisition throws, the constructor must release the first itself,
because the destructor will not run. Members that are themselves RAII types
already have destructors, so they clean up without extra code. That is the
practical argument for "own resources only through members."

### Function-try-block

A constructor can catch exceptions from its own initializer list:

```cpp
Widget::Widget(int x)
try : member_(x) {
} catch (const std::exception& e) {
    // translate, log — then the exception is rethrown automatically
}
```

A function-try-block on a constructor or destructor rethrows if the handler
does not throw something else. You cannot swallow the exception and produce a
live object: the subobjects are already being destroyed, and there is nothing
valid to return. Use this to translate an exception type, not to ignore
failure.

---

## 12. Trivial constructors and destructors

A default constructor is **trivial** when it is not user-provided, it is not
deleted, and every base and member has a trivial default constructor (with a
few further constraints: no virtual functions, no virtual bases). A trivial
default constructor does nothing. The object exists, and its bytes are
whatever was already there, unless the initialization was value-initialization,
which zero-fills first.

A destructor is trivial when it is not user-provided and every subobject
destructor is trivial. A trivial destructor generates no code. `std::vector`
can skip calling it in some operations; a union may hold the type without
tracking active-member destruction.

User-provided means you wrote a body, or you `= default`ed the function
*out of line*. `= default` on the first declaration inside the class can
still be trivial.

You may `memcpy` an object only when the type is trivially copyable (chapter
04 and chapter 17). A trivial destructor is necessary but not sufficient.

---

## 13. `constexpr` construction

```cpp
class Point {
    int x_, y_;
public:
    constexpr Point(int x, int y) : x_(x), y_(y) {}
    constexpr int x() const { return x_; }
};
constexpr Point p{3, 4};
static_assert(p.x() == 3);
```

A `constexpr` constructor can run at compile time. Since C++20, destructors
may be `constexpr`, and so can `virtual` functions. A compile-time object
still obeys the same order rules; the compiler evaluates them as part of the
constant expression.

---

## 14. Placement new and explicit destruction

Ordinary code does not do this. Allocators, `std::vector`, `std::optional`,
and unions do.

```cpp
alignas(Widget) unsigned char buf[sizeof(Widget)];
Widget* p = new (buf) Widget(args);   // lifetime of a Widget begins at p
p->use();
p->~Widget();                          // lifetime ends; buf is just bytes again
```

After the explicit destructor call, `p` does not point at a live `Widget`.
Starting a new lifetime in the same storage with placement new, when the type
has a const or reference member, requires `std::launder` before you use a
pointer that was formed before the replacement. Vocabulary types hide this.
If you are writing one, read `[basic.life]` before inventing a pointer dance.

Switching the active member of a union of non-trivial types is the same
pattern: explicitly destroy the old member, then placement-new the new one.

---

## 15. Static initialization, in one section

Namespace-scope objects are initialized before `main`, in an order that is
specified within one translation unit (definition order) and unspecified
across translation units. That unspecified cross-TU order is the **static
initialization order fiasco**: `A a` in `a.cpp` whose constructor uses `B b`
in `b.cpp` may run before `b` exists.

The fix is to construct on first use:

```cpp
Registry& registry() {
    static Registry r;     // initialized once, on the first call
    return r;              // C++11: concurrent first calls are synchronized
}
```

This is also Meyers' singleton (chapter 11). It solves initialization order.
It does not solve destruction order at shutdown: the local static is
destroyed after `main`, in reverse order of completion of construction, which
can still surprise a destructor that calls into another local static already
destroyed. Prefer not to depend on destruction order. If you must, leak the
singleton on purpose (`static Registry* r = new Registry;`) and document it.

---

## 16. Worked example: a tiny owning buffer

This is the shape of a resource owner before you remember that `std::vector`
already exists. Chapter 04 will give it correct copy and move. Here the copy
operations are deleted so the class is safe but incomplete.

```cpp
#include <cstdio>
#include <utility>

class Buffer {
    int* data_;
    std::size_t size_;
public:
    explicit Buffer(std::size_t n) : data_(new int[n]{}), size_(n) {}
    ~Buffer() { delete[] data_; }

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    int& operator[](std::size_t i) { return data_[i]; }
    std::size_t size() const { return size_; }
};
```

The constructor acquires, the destructor releases, and copying is impossible,
so the double-free of chapter 04 cannot happen. The next step is either to
write the five special members, or to replace `int*` with `std::unique_ptr<int[]>`
and delete nothing by hand (the Rule of Zero).

---

## 17. Exercises

1. `struct S { std::string a; int b; S(int v) : b(v), a(std::to_string(b)) {} };`
   What is wrong, even though it compiles?
2. Why does a class that owns two `FILE*` opened in the constructor body need
   a try/catch, while a class that owns two `std::fstream` members does not?
3. `struct D : B { using B::B; std::string name; };` and `B` has `B(int)`.
   What is `name` after `D d(1);`? Is there a `D` constructor body you can
   put a log statement in?
4. A destructor logs and then throws. The program is already unwinding from
   another exception. What happens?

### Answers

1. `b` is declared after `a`, so `a` is initialized first. The initializer
   reads `b` before `b`'s initialization. Reorder the members so `b` is
   declared first, or initialize `a` from `v` directly.
2. If the second `fopen` throws (or you throw between the two opens),
   `~Holder` does not run, so the first `FILE*` leaks unless the constructor
   catches and closes it. Member `fstream` objects that finished construction
   are destroyed automatically when the constructor throws.
3. `name` is empty, because `std::string`'s default constructor runs via
   default-initialization of the member (it has no default member
   initializer, and `std::string` is default-constructible). The inherited
   constructor has no derived constructor body. Write `D(int i, std::string n)`
   if you need a body or a non-default `name`.
4. `std::terminate`. Destructors must not throw, and a throw during unwind
   has nowhere to go.

---

## 18. Summary

<!--diagram
title: Constructors, destructors & RAII
box[green] Key points
  text: Initialize in the member-initializer list. Order is declaration order, then the body runs
  text: `{}` value-initializes and rejects narrowing. `initializer_list` constructors win brace initialization. Mark converting constructors `explicit`
  text: Delegating constructors have an empty initializer list besides the target. `using Base::Base` does not inherit default/copy/move and has no derived body
  text: If the constructor throws, finished subobjects are destroyed and the object's destructor does not run
  text: RAII acquires in the constructor and releases in the destructor, on every path including unwind. Destructors are noexcept and do not throw
-->
```
 +--------------------------------------------------------------------+
 | Initialize in the mem-initializer list, in declaration order.      |
 | {} value-initializes and rejects narrowing.                        |
 | initializer_list ctors win list-initialization.                    |
 | explicit on converting constructors. explicit(bool) since C++20.   |
 | Delegating ctor: one target, body runs after.                      |
 | using Base::Base skips default/copy/move and has no derived body.  |
 | Ctor throw: finished subobjects die; the object's dtor does not.   |
 | RAII: acquire in ctor, release in dtor, including during unwind.   |
 | Destructors are noexcept. A throw during unwind calls terminate.   |
 +--------------------------------------------------------------------+
```

Next: [03-encapsulation.md](03-encapsulation.md).
