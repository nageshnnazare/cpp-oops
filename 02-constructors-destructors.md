# 02 — Constructors, Destructors & RAII

Constructors set up an object's invariants; destructors tear down its resources.
The pairing of the two — **RAII** — is arguably the single most important idea in
C++. Master this chapter and resource bugs (leaks, double-frees, dangling)
largely disappear.

Prereq: [01-classes-and-objects.md](01-classes-and-objects.md).

---

## 1. Constructors: the kinds

```cpp
class Widget {
public:
    Widget();                          // default constructor (no args)
    Widget(int id);                    // parameterized constructor
    Widget(int id, std::string name);  // another parameterized ctor (overload)
    Widget(const Widget& other);       // copy constructor (chapter 04)
    Widget(Widget&& other) noexcept;   // move constructor (chapter 04)
    // (destructor, assignment operators — later)
};
```

```
   A constructor:
     * has the SAME name as the class
     * has NO return type (not even void)
     * runs automatically when an object is created
     * can be overloaded (many ctors with different parameters)
```

---

## 2. The member initializer list (use it!)

Members should be initialized in the **initializer list**, not assigned in the
body. The difference is real, not stylistic.

```cpp
class Point {
    int x_, y_;
    const int id_;               // const & reference members MUST use init list
public:
    Point(int x, int y, int id)
        : x_(x), y_(y), id_(id)  // <- initializer list: constructs in place
    {
        // body runs AFTER members are initialized
    }
};
```

```
   INIT LIST:   : x_(x)      -> x_ is CONSTRUCTED with value x  (one step)
   BODY ASSIGN: { x_ = x; }  -> x_ is DEFAULT-constructed, THEN assigned (two steps)

   For class-type members this is a real cost; for const/reference members,
   body assignment is IMPOSSIBLE (they can't be reassigned) -> init list required.
```

### Initialization order = declaration order (gotcha!)

```cpp
class Bad {
    int a_;
    int b_;
public:
    Bad(int x) : b_(x), a_(b_) {}   // BUG: a_ is init FIRST (declared first),
                                    // using b_ which isn't initialized yet!
};
```

```
   Members are initialized in the order they are DECLARED in the class,
   NOT the order they appear in the initializer list.
     class Bad { int a_; int b_; };  -> a_ first, then b_
   Compilers warn (-Wreorder). Keep init-list order == declaration order.
```

---

## 3. Default member initializers (C++11)

Give members default values right where they're declared:

```cpp
class Config {
    int    timeout_ = 30;          // default member initializer
    bool   verbose_ = false;
    std::string host_ = "localhost";
public:
    Config() = default;            // uses the defaults above
    Config(int t) : timeout_(t) {} // overrides just timeout_; others use defaults
};
```

```
   host_ = "localhost" at the declaration -> every ctor that doesn't set host_
   gets it for free. Reduces duplication across multiple constructors.
```

---

## 4. `= default` and `= delete`

```cpp
class Foo {
public:
    Foo() = default;               // ask the compiler for the default ctor
    Foo(const Foo&) = delete;      // forbid copying (e.g. unique ownership)
    Foo(int);                      // a user ctor...
    // ...suppresses the implicit default ctor, so add '= default' if you want it
};
```

```
   = default : "generate the standard implementation" (and it stays trivial-ish).
   = delete  : "this operation is forbidden; using it is a COMPILE error."
   Declaring ANY constructor suppresses the implicit DEFAULT constructor.
```

---

## 5. `explicit` — stop unwanted implicit conversions

A single-argument constructor doubles as an implicit conversion unless marked
`explicit`.

```cpp
class Meters {
public:
    Meters(double v) : v_(v) {}    // implicit: 'Meters m = 5.0;' compiles
    double v_;
};

void travel(Meters);
travel(5.0);                       // OOPS: 5.0 silently becomes Meters(5.0)

class Grams {
public:
    explicit Grams(double v) : v_(v) {}   // explicit: no silent conversion
    double v_;
};
// travel-like g(5.0);   // ERROR if it wanted Grams — must write Grams(5.0)
```

```
   Rule of thumb: make single-argument constructors 'explicit' unless you
   specifically WANT implicit conversion. Prevents surprising bugs.
   (C++20: 'explicit(bool)' lets you make it conditional.)
```

---

## 6. Destructors

The destructor runs when an object is destroyed (scope end, `delete`, container
removal). Its job: release whatever the object owns.

```cpp
class FileHandle {
    std::FILE* f_;
public:
    explicit FileHandle(const char* path) : f_(std::fopen(path, "r")) {}
    ~FileHandle() { if (f_) std::fclose(f_); }   // destructor: clean up
};
```

```
   ~ClassName()   <- destructor: same name with leading ~, no args, no return.
   Called automatically. You almost never call it explicitly.
   If you manage a resource, the destructor is where you free it.
```

---

## 7. RAII — Resource Acquisition Is Initialization

The core idiom: **acquire a resource in the constructor, release it in the
destructor.** Because destructors run automatically (even on exceptions), the
resource is *always* freed. No manual cleanup, no leaks.

```cpp
{
    FileHandle f("data.txt");   // constructor opens the file
    use(f);
    if (error) throw ...;       // even if this throws...
}                               // ...~FileHandle() runs HERE -> file closed. Always.
```

```
   RAII lifecycle:
     enter scope  ->  constructor ACQUIRES (open file / lock / allocate)
        use...
     leave scope  ->  destructor  RELEASES (close / unlock / free)  AUTOMATICALLY

   Works on: normal return, early return, break, AND thrown exceptions
   (stack unwinding runs destructors). This is why C++ rarely needs try/finally.
```

```
   Manual (C-style, error-prone)          RAII (C++)
   ---------------------------            ----------------------
   f = fopen(...);                        FileHandle f(...);
   ... 10 return paths, each must         ... just use f ...
       remember fclose ...                (destructor closes on EVERY path)
   fclose(f);   // easy to forget!
```

The standard library is built on RAII: `std::string`, `std::vector`,
`std::unique_ptr`, `std::lock_guard`, `std::fstream` all free their resources in
their destructors. **Prefer these over manual management** (chapter 14).

Runnable: [`examples/ch02_raii.cpp`](examples/ch02_raii.cpp).

---

## 8. Construction & destruction order

```
   For a single object with members:
     1. base class subobjects (chapter 05), in inheritance order
     2. data members, in DECLARATION order
     3. the constructor BODY runs
   Destruction is the EXACT REVERSE:
     3. destructor body runs
     2. members destroyed in REVERSE declaration order
     1. base subobjects destroyed in reverse order
```

```cpp
struct A { A(){puts("A ctor");} ~A(){puts("A dtor");} };
struct B { B(){puts("B ctor");} ~B(){puts("B dtor");} };
struct C { A a; B b; C(){puts("C body");} ~C(){puts("C ~body");} };

// Creating a C prints:
//   A ctor          <- member a (declared first)
//   B ctor          <- member b
//   C body          <- ctor body
//   C ~body         <- dtor body
//   B dtor          <- members destroyed in REVERSE order
//   A dtor
```

```
   Construction:  members in declaration order, THEN body.
   Destruction:   body, THEN members in REVERSE order.
   (For locals in a scope, objects are destroyed in reverse order of creation.)
```

Runnable: [`examples/ch02_order.cpp`](examples/ch02_order.cpp).

---

## 9. Delegating & inheriting constructors

```cpp
class Widget {
    int id_; std::string name_;
public:
    Widget(int id, std::string name) : id_(id), name_(std::move(name)) {}
    Widget(int id) : Widget(id, "unnamed") {}   // DELEGATE to the other ctor
    Widget()       : Widget(0) {}               // delegate again
};
```

```
   Delegating ctor: 'Widget(id) : Widget(id, "unnamed")' reuses another ctor.
   Avoids duplicating initialization logic across overloads.
```

```cpp
struct Base { Base(int); Base(const std::string&); };
struct Derived : Base {
    using Base::Base;    // INHERIT Base's constructors (chapter 05)
};
```

---

## 10. Summary

<!--diagram
title: Constructors, destructors & RAII
box[green] Key points
  text: **Constructor**: same name, no return, runs at creation; overloadable
  text: Use the **initializer list** (not body assignment); order = **declaration** order (not list order). Default member initializers reduce duplication
  text: `= default` / `= delete` to request/forbid special members. Mark single-arg ctors `explicit` to avoid silent conversions
  text: **Destructor** `~T()`: frees owned resources; runs automatically
  text: **RAII**: acquire in ctor, release in dtor → exception-safe, no leaks. Prefer `std::` RAII types over manual `new`/`delete`/`fopen`
  text: Ctor order: bases → members (decl order) → body; dtor reverse
-->
```
 +--------------------------------------------------------------------+
 | Constructor: same name, no return, runs at creation; overloadable. |
 | Use the INITIALIZER LIST (not body assignment); order = DECLARATION|
 | order (not list order). Default member initializers reduce dup.    |
 |                                                                    |
 | = default / = delete to request/forbid special members.            |
 | Mark single-arg ctors 'explicit' to avoid silent conversions.      |
 |                                                                    |
 | Destructor ~T(): frees owned resources; runs automatically.        |
 | RAII: acquire in ctor, release in dtor -> exception-safe, no       |
 |   leaks. Prefer std:: RAII types over manual new/delete/fopen.     |
 | Ctor order: bases -> members(decl order) -> body; dtor reverse.    |
 +--------------------------------------------------------------------+
```

Next: [03-encapsulation.md](03-encapsulation.md).
