# 04 — Copy, Move & the Rule of 0/3/5

Because C++ objects are **values** (chapter 00), what happens when you copy or
move one is fundamental. Getting the "special member functions" right is where a
lot of correctness (and performance) lives. This chapter demystifies them.

Prereq: [02-constructors-destructors.md](02-constructors-destructors.md).

---

## 1. The six special member functions

The compiler can auto-generate these six:

```cpp
class T {
    T();                              // 1. default constructor
    ~T();                             // 2. destructor
    T(const T&);                      // 3. copy constructor
    T& operator=(const T&);           // 4. copy assignment
    T(T&&) noexcept;                  // 5. move constructor       (C++11)
    T& operator=(T&&) noexcept;       // 6. move assignment        (C++11)
};
```

```
   CONSTRUCTION (make a new object):        ASSIGNMENT (overwrite existing):
     copy ctor:  T b = a;                     copy assign:  b = a;
     move ctor:  T b = std::move(a);          move assign:  b = std::move(a);
```

---

## 2. Copy vs move — the intuition

```
   COPY: duplicate the source; source stays intact ("photocopy").
   MOVE: steal the source's guts; leave source empty-but-valid ("hand over").

   Copy of a vector {1,2,3}:            Move of a vector {1,2,3}:
     src: [1,2,3] ---------> dst        src: [1,2,3] ---\
     src still [1,2,3]                                    >  dst: [1,2,3]
     (allocated a new buffer, copied)   src: [] (empty)  /
                                        (just swapped the internal pointer!)
```

```
   Move is an OPTIMIZATION: for types owning heap data (string, vector, unique_ptr)
   moving is O(1) pointer-swaps instead of O(n) deep copy. It's used automatically
   for rvalues (temporaries, std::move'd values).
```

---

## 3. lvalues, rvalues, and `std::move`

```
   lvalue: has a name / an address you can take   (int x;  x is an lvalue)
   rvalue: a temporary, about to expire            (x + 1, foo(), std::move(x))

   Overload resolution:
     T(const T&)   binds to lvalues (copy)
     T(T&&)        binds to rvalues (move)
```

```cpp
std::string a = "hello";
std::string b = a;             // a is an lvalue -> COPY constructor
std::string c = a + " world";  // 'a + " world"' is an rvalue -> MOVE constructor
std::string d = std::move(a);  // std::move CASTS a to rvalue -> MOVE (a now empty)
```

```
   std::move does NOT move anything! It's just a cast to an rvalue reference,
   which lets the MOVE overload be selected. After std::move(a), 'a' is in a
   "valid but unspecified" state — don't use its value, but you may reassign it.
```

---

## 4. The Rule of Zero (the goal)

**If your class doesn't manage a raw resource, declare NONE of the six.** Let the
compiler generate them, and rely on members (`std::string`, `std::vector`,
`std::unique_ptr`) that already handle copy/move/destroy correctly.

```cpp
class Person {                     // Rule of Zero: no special members declared
    std::string name_;
    std::vector<int> scores_;
    std::unique_ptr<Address> addr_;
public:
    Person(std::string n) : name_(std::move(n)) {}
    // copy/move/destroy are all correctly AUTO-generated:
    //   name_ & scores_ copy/move deeply; addr_ moves (and is non-copyable)
};
```

```
   RULE OF ZERO: manage resources via RAII MEMBERS, declare no special members.
   -> The compiler-generated copy/move/dtor "just work" by calling the members'.
   This is the modern default. Aim for it.
```

```
   Person p1("Ada");
   Person p2 = p1;             // deep-copies name_ & scores_...
                              // ...but addr_ is unique_ptr (non-copyable) ->
                              // COMPILE ERROR unless you decide copy semantics.
   Person p3 = std::move(p1); // moves everything cheaply. Works out of the box.
```

---

## 5. The Rule of Three (pre-C++11, still relevant)

**If you need to write ONE of {destructor, copy ctor, copy assignment}, you
almost certainly need all THREE.** This happens when you manage a raw resource.

```cpp
class Buffer {                      // manages a raw heap array
    int* data_;
    std::size_t size_;
public:
    Buffer(std::size_t n) : data_(new int[n]{}), size_(n) {}

    ~Buffer() { delete[] data_; }                       // 1. destructor

    Buffer(const Buffer& o) : data_(new int[o.size_]), size_(o.size_) {  // 2. copy ctor
        std::copy(o.data_, o.data_ + size_, data_);     //   DEEP copy
    }

    Buffer& operator=(const Buffer& o) {                // 3. copy assignment
        if (this != &o) {                               //   self-assignment check!
            int* fresh = new int[o.size_];              //   allocate FIRST (safety)
            std::copy(o.data_, o.data_ + o.size_, fresh);
            delete[] data_;                             //   free old
            data_ = fresh; size_ = o.size_;
        }
        return *this;
    }
};
```

```
   Why all three? If you only wrote the destructor:
     Buffer a(10);
     Buffer b = a;      // compiler's default copy = shallow copy of the POINTER
     // now a.data_ == b.data_  (same buffer!)
     // at scope end: ~b deletes data_, then ~a deletes it AGAIN -> DOUBLE FREE.
   Managing a resource forces you to define copying correctly.
```

```
   DOUBLE-FREE / SHALLOW-COPY BUG:
     a: [ptr]---+
                 >---> [ heap buffer ]     both point to the SAME buffer
     b: [ptr]---+                          -> both destructors free it -> CRASH
```

### The self-assignment trap

```cpp
b = b;   // if operator= did "delete data_; data_ = new...; copy from o"
         // BUT o IS *this -> you deleted the source before copying it! -> UB
```

The `if (this != &o)` guard (or copy-and-swap, §7) prevents this.

---

## 6. The Rule of Five (C++11+)

Once you write copy operations and a destructor, the move operations are **not**
auto-generated. To keep moves efficient, write all **five** (the default ctor is
separate). Add move ctor & move assignment:

```cpp
    Buffer(Buffer&& o) noexcept                     // 4. move ctor
        : data_(o.data_), size_(o.size_) {
        o.data_ = nullptr; o.size_ = 0;             //   leave source empty-valid
    }

    Buffer& operator=(Buffer&& o) noexcept {        // 5. move assignment
        if (this != &o) {
            delete[] data_;                         //   free ours
            data_ = o.data_; size_ = o.size_;       //   steal theirs
            o.data_ = nullptr; o.size_ = 0;         //   null out source
        }
        return *this;
    }
```

```
   MOVE ctor:  steal the pointer, null the source. O(1). NO new allocation.
     a: [ptr->buf]        moved-from a: [nullptr]
                    ===>   b: [ptr->buf]   (same buffer, ownership transferred)

   Mark moves 'noexcept' — std::vector and others require it to move (not copy)
   your objects during reallocation (strong exception guarantee). Big perf win.
```

```
   THE COMPLETE PICTURE (Rule of Five):
     ~Buffer()                 free
     Buffer(const Buffer&)     deep copy
     operator=(const Buffer&)  deep copy assign (self-check)
     Buffer(Buffer&&) noexcept steal
     operator=(Buffer&&) noexcept steal + free old
```

Runnable: [`examples/ch04_rule_of_five.cpp`](examples/ch04_rule_of_five.cpp).

---

## 7. Copy-and-swap idiom (elegant, self-assign-safe)

A slick way to write assignment once, correctly, using the copy ctor + a `swap`:

```cpp
class Buffer {
    // ... data_, size_, dtor, copy ctor, move ctor as above ...
    friend void swap(Buffer& a, Buffer& b) noexcept {
        using std::swap;
        swap(a.data_, b.data_);
        swap(a.size_, b.size_);
    }

    // ONE assignment operator handles BOTH copy and move assignment:
    Buffer& operator=(Buffer other) noexcept {   // NOTE: take BY VALUE
        swap(*this, other);                       // steal from the copy/move
        return *this;
    }                                             // 'other' (old state) dies here
};
```

```
   b = a;              // 'other' is a COPY of a  -> swap -> b has a's data
   b = std::move(a);   // 'other' is MOVED from a -> swap -> b has a's data
   b = b;              // 'other' is a copy of b -> swap with itself's copy -> safe

   By taking the parameter BY VALUE, the copy/move is done by the compiler
   (using the copy/move ctor), and swap can't throw -> automatically:
     * self-assignment safe
     * exception safe (strong guarantee: if the copy throws, *this is untouched)
     * one function serves copy AND move assignment
```

---

## 8. What the compiler generates (the rules table)

```
   If you declare...            then the compiler...
   ---------------------------  --------------------------------------------
   nothing                      generates all 6 (Rule of Zero) — best case
   a destructor                 still gives copy (deprecated) but NOT moves*
   a copy ctor/assign           does NOT generate moves (they fall back to copy)
   a move ctor/assign           deletes the copy operations (move-only type)
   = default                    generates the standard version explicitly
   = delete                     forbids that operation

   * Declaring a destructor (or any copy op) SUPPRESSES implicit MOVE generation.
     So a class with a user destructor silently COPIES on "move" -> perf trap.
     -> If you write ANY of the five, explicitly handle ALL of them (= default
        the ones you don't customize).
```

```
   Common modern pattern — spell out intent even when using defaults:
     ~Widget() = default;
     Widget(const Widget&) = default;
     Widget& operator=(const Widget&) = default;
     Widget(Widget&&) noexcept = default;
     Widget& operator=(Widget&&) noexcept = default;
```

---

## 9. Move-only types

Some types should be movable but not copyable (unique ownership): `unique_ptr`,
`thread`, `fstream`, `mutex`(not even movable). Express it with `= delete`:

```cpp
class Socket {
    int fd_ = -1;
public:
    explicit Socket(int fd) : fd_(fd) {}
    ~Socket() { if (fd_ != -1) ::close(fd_); }

    Socket(const Socket&) = delete;             // no copying a socket
    Socket& operator=(const Socket&) = delete;

    Socket(Socket&& o) noexcept : fd_(o.fd_) { o.fd_ = -1; }      // movable
    Socket& operator=(Socket&& o) noexcept {
        if (this != &o) { if (fd_!=-1) ::close(fd_); fd_ = o.fd_; o.fd_ = -1; }
        return *this;
    }
};
```

```
   Move-only = delete copies, define moves. The resource has exactly ONE owner
   at a time; ownership transfers on move. (This IS how unique_ptr works.)
```

---

## 10. Summary

<!--diagram
title: Copy, move & the rule of 0/3/5
box[green] Key points
  text: Six special members: default ctor, dtor, copy ctor/assign, move ctor/assign
  text: **COPY** duplicates; **MOVE** steals (O(1)) and leaves source valid-empty. `std::move` is just a cast to rvalue (enables the move overload)
  text: **RULE OF ZERO**: own resources via RAII members; declare none (the modern default — aim for it)
  text: **RULE OF THREE**: dtor + copy ctor + copy assign go together
  text: **RULE OF FIVE**: add move ctor + move assign (mark `noexcept`!). Writing a dtor **suppresses** implicit moves → handle all five
  text: Copy-and-swap: one by-value `operator=` → safe copy AND move. Move-only: `= delete` copies, define moves (unique ownership)
-->
```
 +-------------------------------------------------------------------+
 | Six special members: default ctor, dtor, copy ctor/assign,        |
 |   move ctor/assign.                                               |
 | COPY duplicates; MOVE steals (O(1)) and leaves source valid-empty.|
 | std::move is just a cast to rvalue (enables the move overload).   |
 |                                                                   |
 | RULE OF ZERO: own resources via RAII members; declare none.       |
 |   (the modern default — aim for it)                               |
 | RULE OF THREE: dtor + copy ctor + copy assign go together.        |
 | RULE OF FIVE: add move ctor + move assign (mark noexcept!).       |
 |   Writing a dtor SUPPRESSES implicit moves -> handle all five.    |
 | Copy-and-swap: one by-value operator= -> safe copy AND move.      |
 | Move-only: = delete copies, define moves (unique ownership).      |
 +-------------------------------------------------------------------+
```

Next: [05-inheritance.md](05-inheritance.md).
