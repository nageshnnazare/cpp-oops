# 04 — Copy, Move & the Rule of 0/3/5

C++ objects are values. Copying and moving them is not a library feature; it
is what initialization and assignment *mean*. The six special member functions
are how a class takes control of that meaning. The modern default is to take
control of none of them.

Prereq: [02-constructors-destructors.md](02-constructors-destructors.md).

---

## 1. The six special members

```cpp
class T {
public:
    T();                         // default constructor
    ~T();                        // destructor
    T(const T&);                 // copy constructor
    T& operator=(const T&);      // copy assignment
    T(T&&) noexcept;             // move constructor
    T& operator=(T&&) noexcept;  // move assignment
};
```

```
   Construction builds a new object. Assignment replaces an existing one.

   T b = a;                 copy constructor     (a is an lvalue)
   T b = std::move(a);      move constructor     (a is cast to an rvalue)
   b = a;                   copy assignment
   b = std::move(a);        move assignment
   T b = make();            neither, if make() returns a prvalue (see §7)
```

The default constructor is not part of the Rule of Five. It is suppressed by
any user-declared constructor, independently of copy and move.

---

## 2. Value categories, as far as copy and move need them

Every expression is one of:

```
   lvalue     a named object, *p, a function, a string literal
              "has identity, cannot be moved from implicitly"
   prvalue    a temporary that initializes an object: 42, make(), T{}
              "has no identity yet; C++17 initializes the destination directly"
   xvalue     an expiring object: std::move(a), a member of an expiring object
              "has identity, and we are allowed to steal from it"

   glvalue = lvalue or xvalue        (has identity)
   rvalue  = prvalue or xvalue       (can bind to T&&)
```

Overload resolution for the special members:

```
   T(const T&)    binds to lvalues, and also to rvalues if no T(T&&) exists
   T(T&&)         binds to rvalues only
```

```cpp
std::string a = "hello";
std::string b = a;               // lvalue: copy
std::string c = a + " world";    // prvalue: initializes c directly (C++17)
std::string d = std::move(a);    // xvalue: move; a is valid but unspecified
```

`std::move` does not move. It is a cast to an rvalue reference:

```cpp
template <class T>
constexpr std::remove_reference_t<T>&& move(T&& t) noexcept {
    return static_cast<std::remove_reference_t<T>&&>(t);
}
```

After `std::move(a)`, the name `a` still refers to the same object. The move
happens only if a move constructor or move assignment actually runs and steals
the resources. Using `a`'s value afterward is legal only to the extent the
type's moved-from contract allows. The standard library's contract is **valid
but unspecified**: you may assign a new value, you may destroy, you may call
functions with no precondition. You may not assume it is empty, except where
a specific type says so. `std::unique_ptr` does say so: a moved-from
`unique_ptr` is null. `std::vector` does not promise to be empty.

`std::forward<T>(t)` is the other cast. It preserves the value category of a
forwarding reference. You use it in templates that pass an argument on,
not in ordinary special members.

---

## 3. What the compiler generates

This table is the one to memorize. "User-declared" includes `= default` and
`= delete`.

```
   you declare                         implicit copy              implicit move
   ----------------------------------  -------------------------  -------------------------
   nothing                             declared                   declared
   destructor                          declared (deprecated)      not declared
   copy constructor                    (you declared it)          not declared
   copy assignment                     (you declared it)          not declared
   move constructor                    defined as deleted         (you declared it)
   move assignment                     defined as deleted         (you declared it)
```

Two consequences that cause real bugs:

1. A user-declared destructor **suppresses the move operations**. They are
   not declared, so a "move" silently selects the copy constructor. A class
   that frees a raw pointer in its destructor and forgets to declare a move
   will deep-copy on `std::move`, or fail to compile if the copy is deleted.
   It will not steal.

2. Declaring a move operation **deletes the copy operations**. A move-only
   type is what you get from `T(T&&) = default` when a member is move-only
   (`unique_ptr`), or from declaring a move and not restoring the copy.

If a move is implicitly declared but a member cannot be moved (a `const`
member, a reference member, or a member whose move is deleted), the move is
defined as deleted. A deleted move is worse than a missing one: overload
resolution selects it and then the program is ill-formed, instead of falling
back to the copy. A class with a `const` member or a reference member is not
a happy value type. Store values, or store pointers you can reseat.

Defaulted on the first declaration, the special member is trivial when the
members allow it. Defaulted out of line, it is user-provided and not trivial.
Trivial copy plus a trivial destructor is what makes a type trivially
copyable, and trivially copyable is the requirement for `memcpy` of the
object. A virtual function, a `std::string` member, or a user-provided copy
constructor all end that permission.

---

## 4. The Rule of Zero

If the class does not directly own a raw resource, declare none of the five
(destructor, copy constructor, copy assignment, move constructor, move
assignment). Members that already implement value semantics compose.

```cpp
class Person {
    std::string name_;
    std::vector<int> scores_;
public:
    explicit Person(std::string n) : name_(std::move(n)) {}
};
```

The compiler-generated copy deep-copies the string and the vector. The
generated move steals their buffers. The generated destructor destroys both.
`Person p3 = std::move(p1);` is correct with no code.

A `std::unique_ptr` member makes the generated copy deleted and the generated
move correct. The class becomes move-only, which is the right answer for
unique ownership. If you wanted deep copy, you would write a copy constructor
that allocates a new `T` — and then you are in the Rule of Five, because
declaring the copy suppresses the implicit move and you must restore it.

Aim here. `std::string`, `std::vector`, `std::unique_ptr`, `std::fstream`,
and `std::lock_guard` exist so your classes can follow the Rule of Zero.

---

## 5. The Rule of Three

If you manage a raw resource, the destructor, the copy constructor, and the
copy assignment all three exist, or none of them does. The compiler's copy
is a memberwise copy. For a pointer, that copies the address.

```cpp
class Buffer {
    int* data_;
    std::size_t size_;
public:
    explicit Buffer(std::size_t n) : data_(new int[n]{}), size_(n) {}

    ~Buffer() { delete[] data_; }

    Buffer(const Buffer& o) : data_(new int[o.size_]), size_(o.size_) {
        std::copy(o.data_, o.data_ + size_, data_);
    }

    Buffer& operator=(const Buffer& o) {
        if (this == &o) return *this;
        int* fresh = new int[o.size_];          // allocate first
        std::copy(o.data_, o.data_ + o.size_, fresh);
        delete[] data_;                         // then release the old
        data_ = fresh;
        size_ = o.size_;
        return *this;
    }
};
```

If only the destructor is user-declared, `Buffer b = a` memberwise-copies
`data_`. Both objects delete the same array. That is a double free.

Assignment is not construction. The left-hand object already owns a buffer.
The safe order is: allocate the new resource, copy into it, and only then
release the old one. If `new` throws, `*this` is unchanged (the strong
exception guarantee). If you `delete[]` first and then `new` throws, the
object owns a dangling pointer and its destructor will delete it again.

### Self-assignment

`b = b` must be a no-op that leaves `b` valid. The implementation above
returns early when `this == &o`. Without that test, `delete[]` first would
free the source before the copy reads it. Copy-and-swap (§8) makes the test
unnecessary.

---

## 6. The Rule of Five

Declaring the copy operations or the destructor suppresses implicit moves.
Put the moves back, and mark them `noexcept` when they cannot throw.
`std::vector` reallocation will move your element only if the move
constructor is `noexcept`. Otherwise it copies, to keep the strong guarantee
if a move would throw halfway through the buffer. A throwing move plus a
vector growth is a silent performance bug, not a compile error.

```cpp
Buffer(Buffer&& o) noexcept
    : data_(std::exchange(o.data_, nullptr)),
      size_(std::exchange(o.size_, 0)) {}

Buffer& operator=(Buffer&& o) noexcept {
    if (this != &o) {
        delete[] data_;
        data_ = std::exchange(o.data_, nullptr);
        size_ = std::exchange(o.size_, 0);
    }
    return *this;
}
```

`std::exchange(obj, new_value)` assigns `new_value` and returns the previous
value. In a move constructor there is no old buffer to free: the object is
being born, and the initializer list is the initialization.

Self-move-assignment (`a = std::move(a)`) must leave `a` in a valid state.
The `this != &o` test does that. Destroy-then-steal without the test is
unsafe when the two sides are the same object: the destructor of `*this`
destroys the source you are about to read.

Moved-from `Buffer` is empty: null pointer, size 0. That is a stronger
contract than the standard library's "valid but unspecified," and it is the
right contract for a type you designed. Document it. Do not read `data_[0]`
on a moved-from buffer.

A complete raw-owning class spells out all five, even when some are
`= default`:

```cpp
~Widget() = default;
Widget(const Widget&) = default;
Widget& operator=(const Widget&) = default;
Widget(Widget&&) noexcept = default;
Widget& operator=(Widget&&) noexcept = default;
```

You write that block when a member forces you to declare one of them (the
pimpl incomplete-type destructor in chapter 03) and you want the others to
stay compiler-generated. Declaring the destructor out of line without
declaring the moves would silently turn moves into copies.

Runnable: [`examples/ch04_rule_of_five.cpp`](examples/ch04_rule_of_five.cpp).

---

## 7. Copy elision

C++17 **guarantees** that a prvalue used to initialize an object of the same
type does not create a temporary and does not call the copy or move
constructor. The copy constructor need not even be accessible:

```cpp
Buffer make() {
    return Buffer(10);     // initializes the caller's object directly
}
Buffer b = make();         // still one object, not three
```

Named return value optimization is different. In `return local;`, where
`local` is an automatic object, the compiler *may* construct `local` in the
return slot. It is not required to. If it does not, the language treats
`local` as an rvalue for overload resolution, so the move constructor is
selected. You do not write `return std::move(local);`. That cast blocks
NRVO and, for a type that is only copyable, can also block the implicit move
and force a copy... actually for a local variable, `return std::move(local)`
selects the move and prevents elision. Prefer the bare `return local;`.

The one place `return std::move` is right: returning a parameter, or
returning something that is not a local automatic object. Parameters are not
eligible for NRVO in the way people hope, and a by-value parameter should be
moved out explicitly if you want the move.

---

## 8. Copy-and-swap

One assignment operator can serve as both copy and move assignment:

```cpp
friend void swap(Buffer& a, Buffer& b) noexcept {
    using std::swap;
    swap(a.data_, b.data_);
    swap(a.size_, b.size_);
}

Buffer& operator=(Buffer other) noexcept {   // by value: copy OR move
    swap(*this, other);
    return *this;
}                                             // other destroys the old state
```

```
   b = a;              other is copy-constructed from a, then swapped in
   b = std::move(a);   other is move-constructed from a, then swapped in
   b = b;              other is a copy of b; swap exchanges b with that copy
```

If the copy constructor throws, `operator=` has not started modifying
`*this`. That is the strong guarantee. `swap` of two pointers does not
throw, so the `noexcept` is honest. The cost is an extra move when the
right-hand side is an lvalue: copy into `other`, swap, destroy `other`. For
a buffer that is already a heap allocation, the extra pointer swaps are
noise next to the allocation. For a type where you can do better (reuse
capacity when `size()` fits), a hand-written assignment is the right tool.
Copy-and-swap is the default when you want correctness without thinking
about self-assignment.

Define `swap` as a non-member in the same namespace, `noexcept`, and
implement it with `using std::swap; swap(a.member, b.member);` so a member
type's own ADL `swap` is found. A member `swap` is optional sugar that the
free function can call.

---

## 9. Exception safety, three levels

```
   nothrow (no-throw)    the operation does not throw. Destructors, noexcept
                         moves, pointer swaps.
   strong                the operation completes, or the object is unchanged.
                         copy-and-swap assignment. vector reallocation when
                         the element move is noexcept, or when it copies.
   basic                 the invariant holds, but the value may have changed.
                         a sequence of pushes that throws on the third one.
   none                  the invariant may be broken. Do not ship this.
```

A function offers the strong guarantee by doing the work that might throw on
the side, then committing with only nothrow operations. That is the same
shape as "allocate, then delete the old buffer."

`noexcept` on a function is both a contract and an optimization. If a
`noexcept` function throws, `std::terminate` is called and the stack is not
unwound in the usual way. Put it on moves and swaps that truly cannot throw.
Do not put it on a copy that allocates.

---

## 10. Move-only types

Some resources have one owner. Delete the copy operations and define the
moves, or embed a `unique_ptr` and follow the Rule of Zero.

```cpp
class Socket {
    int fd_ = -1;
public:
    explicit Socket(int fd) : fd_(fd) {}
    ~Socket() { if (fd_ != -1) ::close(fd_); }

    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    Socket(Socket&& o) noexcept : fd_(std::exchange(o.fd_, -1)) {}
    Socket& operator=(Socket&& o) noexcept {
        if (this != &o) {
            if (fd_ != -1) ::close(fd_);
            fd_ = std::exchange(o.fd_, -1);
        }
        return *this;
    }
};
```

The moved-from socket holds `-1`, and the destructor treats `-1` as "no
file descriptor." Pick a sentinel and stick to it. `std::unique_ptr`,
`std::fstream`, and `std::jthread` are move-only for this reason.
`std::mutex` is neither copyable nor movable.

---

## 11. Copying a polymorphic object

A compiler-generated copy of a base slices (chapter 05). If a hierarchy
needs value copies, give it a virtual `clone`:

```cpp
class Shape {
public:
    virtual std::unique_ptr<Shape> clone() const = 0;
    virtual ~Shape() = default;
};

class Circle : public Shape {
    double r_;
public:
    explicit Circle(double r) : r_(r) {}
    std::unique_ptr<Shape> clone() const override {
        return std::make_unique<Circle>(*this);
    }
};
```

The override returns `unique_ptr<Shape>`. A covariant raw-pointer return
(`Circle* clone() const override` when the base returns `Shape*`) exists in
the language, and chapter 06 covers it. Prefer `unique_ptr` so ownership is
in the signature. The base copy and move should be protected or deleted so
callers do not slice by accident:

```cpp
class Shape {
protected:
    Shape() = default;
    Shape(const Shape&) = default;
    Shape& operator=(const Shape&) = default;
    Shape(Shape&&) noexcept = default;
    Shape& operator=(Shape&&) noexcept = default;
public:
    virtual std::unique_ptr<Shape> clone() const = 0;
    virtual ~Shape() = default;
};
```

Protected copy operations let `Circle`'s copy constructor invoke `Shape`'s,
and they stop `Shape a = some_circle;` outside the hierarchy.

---

## 12. A templated constructor is not a copy constructor

A copy constructor is a non-template constructor with a particular signature.
This template is a converting constructor, and it wins overload resolution
for a non-const lvalue:

```cpp
class Widget {
    std::string name_;
public:
    template <class S>
    explicit Widget(S&& s) : name_(std::forward<S>(s)) {}
};
Widget a("a");
Widget b(a);     // instantiates Widget(Widget&), not the copy constructor
                 // then std::forward tries to construct a string from a Widget
```

The template is a better match than `Widget(const Widget&)` because `Widget&`
binds to `a` with less qualification than `const Widget&`. Constrain it:

```cpp
template <class S>
    requires std::constructible_from<std::string, S>
          && (!std::same_as<std::remove_cvref_t<S>, Widget>)
explicit Widget(S&& s) : name_(std::forward<S>(s)) {}
```

Or take `std::string` by value, which is the sink-parameter pattern and does
not have this problem:

```cpp
explicit Widget(std::string name) : name_(std::move(name)) {}
```

An lvalue `std::string` is copied into the parameter, then moved into the
member. An rvalue is moved twice, and moves of `string` are cheap. One
function handles both.

---

## 13. Exercises

1. You wrote `~Buffer()` and a copy constructor, and you did not mention
   move. What does `Buffer b = std::move(a);` do?
2. Why does `std::vector<Buffer>` copy elements on growth when `Buffer`'s
   move constructor allocates, and move them when it only steals a pointer
   and is `noexcept`?
3. `struct S { const int n; std::string name; };` — is the generated move
   assignment usable? Why?
4. `return std::move(local);` at the end of a function that returns `local`
   by value. What optimization does that inhibit?

### Answers

1. The move constructor was not declared, because the user-declared
   destructor and copy constructor suppressed it. The copy constructor is
   selected. `a` is unchanged. You paid for a deep copy and you may have
   expected a steal.
2. If the move constructor is not `noexcept`, `vector` copies so that a
   throw leaves the original buffer intact (the strong guarantee). A
   `noexcept` move cannot throw, so `vector` moves. Mark non-throwing moves
   `noexcept`.
3. Move assignment is defined as deleted. `const int n` cannot be reseated
   or assigned. The class can be constructed but not assigned.
4. Named return value optimization. A bare `return local;` can construct
   `local` directly into the caller's return slot, and if the compiler
   declines, it still implicit-moves. `std::move` forces the move and blocks
   the elision.

---

## 14. Summary

<!--diagram
title: Copy, move & the rule of 0/3/5
box[green] Key points
  text: Rule of Zero — own resources through RAII members and declare no special members
  text: A user-declared destructor or copy suppresses implicit move, so a "move" becomes a copy. Declaring a move deletes the copy
  text: std::move is a cast. Moved-from standard types are valid but unspecified. Mark non-throwing moves noexcept so vector will use them
  text: C++17 elides prvalue copies. Do not std::move a local return value. Copy-and-swap gives self-assignment safety and the strong guarantee
  text: Polymorphic copies go through a virtual clone. A forwarding-reference constructor is not a copy constructor and will steal non-const lvalues
-->
```
 +-------------------------------------------------------------------+
 | Rule of Zero: RAII members, no special members declared.          |
 | User-declared dtor or copy => no implicit move (silent copies).   |
 | User-declared move => copy is deleted.                            |
 | std::move is a cast. Moved-from library objects: valid,           |
 |   unspecified. noexcept on non-throwing moves (vector cares).     |
 | C++17 elides prvalues. return local; not return std::move(local). |
 | Copy-and-swap: strong guarantee, self-assignment safe.            |
 | Hierarchies copy via virtual clone(). Constrain templated ctors.  |
 +-------------------------------------------------------------------+
```

Next: [05-inheritance.md](05-inheritance.md).
