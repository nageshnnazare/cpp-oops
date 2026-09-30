# 09 — Operator Overloading

An overloaded operator is a function with a punctuation name. It does not add
syntax to the language. Precedence, associativity, and the number of operands
stay exactly what they are for the built-in operator. The only thing you
control is what the function does when the operands have your types.

Used when the meaning is the one a reader already has, operators make value
types usable. Used as clever names, they make the type harder to read than
an ordinary function. This chapter is the mechanics and the small set of
shapes that are worth memorizing.

Prereq: [01-classes-and-objects.md](01-classes-and-objects.md),
[04-copy-move-rule-of-five.md](04-copy-move-rule-of-five.md).

---

## 1. What you can overload

```
   overloadable
     +  -  *  /  %     unary and binary
     == != < > <= >=   <=>
     += -= *= /= %=    &= |= ^=  <<= >>=
     << >>
     && || ! ~         (you can, and you should not overload &&, ||, or ,)
     ++ --             prefix and postfix
     [] () -> ->*
     & | ^
     new new[] delete delete[]
     ,                 the comma operator; do not
     user-defined literals: operator""_suffix   (not an OOP feature; skip)

   not overloadable
     ::   .   .*   ?:   sizeof   alignof   typeid   noexcept
     and the preprocessing operators
```

You cannot invent a token, and you cannot change `*` so that it binds looser
than `+`. If the precedence is wrong for your domain, use a named function.

A member operator's left operand is the object. A non-member's operands are
the function parameters.

---

## 2. Member or free function

```
   must be a member          =   []   ()   ->   ->*
                                 (the language requires it)
   usually a member          compound assignment (+= and the rest)
                                 so the left side is clearly the object
                                 being modified
   usually a non-member      binary + - * / %
                             == != < > <= >= <=>
                             << >>   for streams
                                 so both operands convert symmetrically
```

```cpp
struct Vec2 {
    double x = 0, y = 0;
    Vec2& operator+=(const Vec2& o) {
        x += o.x;
        y += o.y;
        return *this;
    }
};

Vec2 operator+(Vec2 a, const Vec2& b) {   // free function
    a += b;
    return a;
}
```

If `operator+` is a member, `v + 2` can work (the right-hand side converts)
and `2 + v` cannot (`int` has no member `operator+` that takes `Vec2`). A
free function `operator+(Vec2, Vec2)` with an implicit conversion from `int`
to `Vec2` accepts both, but only if that conversion is something you actually
want. For a vector, a converting constructor from `double` is usually a
mistake (chapter 02). Write two overloads, `operator*(Vec2, double)` and
`operator*(double, Vec2)`, and keep the constructor `explicit`.

---

## 3. The arithmetic pattern

Implement the compound assignment. Implement the binary operator as a copy
(or a move, by taking the left operand by value) plus the compound
assignment.

```cpp
struct Money {
    long cents = 0;
    Money& operator+=(Money o) { cents += o.cents; return *this; }
    Money& operator-=(Money o) { cents -= o.cents; return *this; }
    friend Money operator+(Money a, Money b) { return a += b; }
    friend Money operator-(Money a, Money b) { return a -= b; }
};
```

One place holds the real arithmetic. The binary operators cannot drift out
of sync with it. Taking `Money` by value in `operator+` lets an rvalue left
operand be moved into `a` and then modified, and it lets an lvalue be
copied. Return by value. Return `*this` from `+=`, as a reference, so
compound expressions chain and so you do not copy the left operand twice.

Qualify `operator=` with `&` if you want to reject assignment to a
temporary (chapter 01):

```cpp
Money& operator=(const Money&) & = default;
```

---

## 4. Comparisons and `<=>`

C++20 adds the three-way comparison operator. A **defaulted** `<=>` also
gives you `==`. A `<=>` you write yourself does not.

```cpp
#include <compare>

struct Version {
    int major = 0, minor = 0, patch = 0;
    auto operator<=>(const Version&) const = default;
};
// == != < <= > >= all exist.
// Version{1, 2, 0} < Version{1, 3, 0}   is true
```

`= default` compares members in declaration order, lexicographically, and
deletes the operator if a member cannot be compared that way. The return
type is the common comparison category of the members:

```
   std::strong_ordering     exactly one of <, ==, >; equality means interchangeable
                            (int, string, most value types)
   std::weak_ordering       <, equivalent, >; equivalent values may be
                            distinguishable (case-folded strings)
   std::partial_ordering    <, ==, >, or unordered
                            (double, because NaN compares unordered with everything)
```

`std::strong_ordering` converts to the weaker categories, so a function
written against `partial_ordering` accepts a strong result. The other
direction does not convert.

What the default actually declares:

```
   auto operator<=>(const Version&) const = default;
     => memberwise <=> 
     => a defaulted operator== is declared for you as well
     => < <= > >= are rewritten as comparisons of the <=> result against 0
     => != is rewritten from ==

   bool operator==(const Version&) const = default;
     => memberwise ==
     => != is rewritten as the negation
     => does not give you <
```

Writing both `<=>` and `==` as `= default` is explicit and harmless. Writing
only the defaulted `<=>` is enough for the compiler. Writing a *user-defined*
body for `<=>` does **not** declare `==`:

```cpp
struct CaseFold {
    std::string s;
    std::weak_ordering operator<=>(const CaseFold& o) const {
        return fold(s) <=> fold(o.s);
    }
    bool operator==(const CaseFold& o) const {   // required, or == will not compile
        return fold(s) == fold(o.s);
    }
};
```

Define `==` yourself in that case, and implement it as a real equality test.
Do not implement it as `(a <=> b) == 0`. Equality is allowed to be cheaper
and, for types like `vector`, it can stop at the first mismatch without
producing a full ordering.

Rewrite rules you will hit while debugging an overload set:

```
   a < b     may be rewritten as  (a <=> b) < 0    or as  0 < (b <=> a)
   a == b    may be rewritten as  b == a           when only one order is a candidate
```

A deleted `<=>` still participates and can block a built-in candidate. If a
defaulted comparison comes out deleted, a member is the reason: it has no
`<=>`, or it is a pointer and you asked for an ordering the pointer does not
have under the default. Pointers do not have a defaulted `<=>` that orders
them; `==` on pointers is fine.

`std::set` and `std::map` need an ordering. A defaulted `<=>` that yields
`strong_ordering` or `weak_ordering` supplies `<`. Floating members yield
`partial_ordering`, and a strict weak ordering must not treat values as
unordered, so do not use a raw `double` as the only key without a policy for
NaN.

Runnable: [`examples/ch09_spaceship.cpp`](examples/ch09_spaceship.cpp).

---

## 5. Hidden friends

A friend function defined inside the class is a **hidden friend**. Ordinary
lookup does not find it. Argument-dependent lookup finds it when an argument
has the class as an associated type.

```cpp
class Point {
    int x_, y_;
public:
    Point(int x, int y) : x_(x), y_(y) {}

    friend bool operator==(const Point& a, const Point& b) {
        return a.x_ == b.x_ && a.y_ == b.y_;
    }
    friend std::ostream& operator<<(std::ostream& os, const Point& p) {
        return os << '(' << p.x_ << ", " << p.y_ << ')';
    }
};
```

`operator<<` cannot be a member: the left operand is `std::ostream`. It
should not be a namespace-scope function declared in a header for every
translation unit to overload against, because those overloads get considered
for unrelated types and slow compilation, and they can be ambiguous. A hidden
friend is visible only when a `Point` is an argument.

Return the stream so callers can write `std::cout << p << '\n'`.

The same pattern is the right default for `operator==` when you want access
to private members and you do not want a member function (a member `==` is
also fine and is what `= default` produces; prefer `= default` when the
comparison really is memberwise).

---

## 6. Subscript, call, and arrow

```cpp
class Grid {
    std::vector<int> data_;
    std::size_t cols_;
public:
    Grid(std::size_t rows, std::size_t cols) : data_(rows * cols), cols_(cols) {}

    int&       operator[](std::size_t i)       { return data_[i]; }
    const int& operator[](std::size_t i) const { return data_[i]; }

    int& operator[](std::size_t r, std::size_t c) {          // C++23
        return data_[r * cols_ + c];
    }
};

struct Adder {
    int base = 0;
    int operator()(int x) const { return base + x; }
};
// Adder{10}(5) == 15
```

Provide both `const` and non-const `operator[]` (chapter 01). A const object
must be able to read. A non-const object is the only one that should hand
out a mutable reference. C++23 allows `operator[]` to take more than one
parameter, which replaces the old `operator()(r, c)` workaround. One of them
is enough; do not invent both for the same meaning.

`operator()` makes the object a functor. Lambdas are compiler-written
functors with an `operator()`. The standard algorithms take anything
callable, not a base class.

`operator->` is a member. The expression `sp->member` is rewritten as
`(sp.operator->())->member`, and if that result is a class type with its own
`operator->`, the rewrite repeats until a raw pointer comes out.

```cpp
template <class T>
class SmartPtr {
    T* p_;
public:
    explicit SmartPtr(T* p) : p_(p) {}
    T* operator->() const { return p_; }
    T& operator*() const { return *p_; }
};
```

If you overload `->`, overload `*` as well, and keep them consistent. A
smart pointer that can be null must define what `*` does on null. The
standard `unique_ptr` says the behavior is undefined, matching a raw pointer.

---

## 7. Increment

```cpp
struct Counter {
    int n = 0;
    Counter& operator++() {              // prefix ++c
        ++n;
        return *this;
    }
    Counter operator++(int) {            // postfix c++
        Counter old = *this;
        ++*this;
        return old;
    }
};
```

The extra unused `int` parameter is how the language tells the two overloads
apart. Prefix returns a reference to the incremented object. Postfix returns
the old value by value, which costs a copy. In a `for` loop, prefer prefix
on iterators and on types that own resources. Implement postfix by calling
prefix, so the actual increment lives in one function.

---

## 8. Conversion operators

```cpp
class Fraction {
    int num_, den_;
public:
    Fraction(int n, int d) : num_(n), den_(d == 0 ? 1 : d) {}
    explicit operator double() const { return static_cast<double>(num_) / den_; }
    explicit operator bool() const { return num_ != 0; }
};

Fraction f(1, 2);
double d = static_cast<double>(f);
if (f) { /* contextual conversion to bool is allowed for explicit operator bool */ }
```

`operator Target()` defines a conversion *from* your type. Mark it
`explicit` for the same reason you mark converting constructors explicit.
`explicit operator bool` is special-cased: it is allowed in the condition of
`if`, `while`, `for`, and in the operands of the built-in `&&`, `||`, and
`!`. It is not allowed as a silent conversion to `int`. That replaced the
old "safe bool" idiom, which was a conversion to a pointer-to-member. Do not
write the old idiom.

A conversion operator and a converting constructor in the other direction
together create ambiguity (`Fraction` to `double` and `double` to `Fraction`
both viable for some calls). Prefer constructors for "make my type from
yours" and a named function (`to_double()`) unless the conversion is
obvious.

---

## 9. Operators that lose something when overloaded

```
   && and ||     the built-in operators short-circuit and sequence their
                 operands. An overloaded && is an ordinary function call:
                 both operands are evaluated, in an unspecified order.
                 Leave them alone.
   ,             the built-in comma sequences its operands. An overload does
                 not give you that guarantee in the way readers think, and
                 nobody expects operator, to mean something. Leave it alone.
   & (unary)     overloading address-of breaks generic code that forms
                 pointers. Leave it alone.
   && on types   you already have no reason. Use a named function.
```

`new` and `delete` as class members are allocation hooks, not constructors.
A member `operator new` is a static function whether you write `static` or
not. It returns storage; the constructor runs afterward. Overload them only
when you are writing an allocator or tracking allocations. A class-specific
`operator new` does not run for `make_unique` unless `make_unique` ends up
calling `new Widget`, which it does — so a member `operator new` *is* used
by `make_unique`. It is not used by `std::allocator` unless the allocator is
written to call it. Measure before you replace the global allocator from
inside a value type.

---

## 10. Consistency

If you overload `==`, then `a == a` is true, `==` is symmetric, and it is
transitive on the values you claim are equal. If you also overload `<`, then
`<` is a strict weak ordering consistent with `==`: if `a == b`, then
neither `a < b` nor `b < a`. Defaulted `<=>` gives you this for memberwise
comparison. Hand-written operators drift. A set of six hand-written
comparisons will eventually disagree with itself; that is the bug `<=>`
removes.

`std::hash<YourType>` must agree with `==`. Two equal values must hash
equal, or `unordered_set` is wrong. The standard library does not generate
the hash from `<=>`. You specialize `std::hash` yourself when the type is a
hash-table key.

---

## 11. Worked example

```cpp
#include <cmath>
#include <compare>
#include <iostream>

struct Vector2D {
    double x = 0, y = 0;

    Vector2D& operator+=(const Vector2D& o) { x += o.x; y += o.y; return *this; }
    Vector2D& operator*=(double s) { x *= s; y *= s; return *this; }

    friend Vector2D operator+(Vector2D a, const Vector2D& b) { return a += b; }
    friend Vector2D operator*(Vector2D a, double s) { return a *= s; }
    friend Vector2D operator*(double s, Vector2D a) { return a *= s; }

    friend bool operator==(const Vector2D& a, const Vector2D& b) {
        return a.x == b.x && a.y == b.y;
    }
    friend std::ostream& operator<<(std::ostream& os, const Vector2D& v) {
        return os << '(' << v.x << ", " << v.y << ')';
    }
};

int main() {
    Vector2D a{1, 2};
    Vector2D b{3, 4};
    std::cout << (a + b) << '\n';
    std::cout << (2.0 * a) << '\n';
    std::cout << (a == a) << '\n';
}
```

`==` is written by hand because a defaulted comparison on `double` is
`partial_ordering` and treats NaN as unordered, which is correct IEEE and
surprising in application code. If the type should not contain NaN, say so
in the constructor and then a defaulted comparison is fine. The example
keeps exact `==` so the output is obvious.

Runnable: [`examples/ch09_vector2d.cpp`](examples/ch09_vector2d.cpp).

---

## 12. Exercises

1. Why is `2 + money` ill-formed when `operator+` is a member of `Money` and
   there is an implicit constructor `Money(int)`? What signature accepts both
   `2 + money` and `money + 2`?
2. You default `operator<=>` and you do not declare `operator==`. Does
   `a == b` compile?
3. You write a body for `operator<=>` and you do not declare `operator==`.
   Does `a == b` compile?
4. What does the unused `int` parameter on postfix `operator++` mean, and
   why does prefix return a reference?

### Answers

1. The member call is `money.operator+(2)` or `2.operator+(money)`. `int`
   has no such member, so the left-hand `2` cannot start the call. A free
   `operator+(Money, Money)` lets the converting constructor build a `Money`
   from `2` on either side. Only add that conversion if cents-from-int is
   really a conversion you want; otherwise write an explicit named factory
   and two non-converting overloads.
2. Yes. A defaulted `<=>` implicitly declares a defaulted `==`.
3. No. A user-provided `<=>` does not declare `==`. Write `operator==`.
4. It distinguishes postfix from prefix in the overload set. The caller
   writes `c++` and the compiler passes `0`. Prefix returns `*this` because
   the new value *is* the object. Postfix must return the old value, which
   is a distinct object.

---

## 13. Summary

<!--diagram
title: Operator overloading
box[green] Key points
  text: Operators are functions. Precedence and arity do not change. = [] () -> must be members
  text: Binary symmetric operators are non-members so both sides convert. Implement += once and build + from it
  text: A defaulted <=> also gives == and the relational operators. A hand-written <=> does not give ==
  text: Hidden friends are found by ADL only, which is the right shape for operator<< and for operators that need private access
  text: Do not overload &&, ||, or comma. explicit operator bool is the boolean conversion. Postfix ++ takes an unused int and returns the old value
-->
```
 +------------------------------------------------------------------+
 | Operator functions keep built-in precedence and arity.           |
 | Members: = [] () ->. Non-members: symmetric arithmetic,           |
 |   comparisons, stream insertion.                                  |
 | Write +=, then + as (by value) += .                               |
 | Defaulted <=> implies ==. A user-written <=> does not.           |
 | Hidden friends: ADL-only, the right default for << and ==.       |
 | Do not overload &&, ||, or comma. explicit operator bool.        |
 | Postfix ++ takes int and returns the previous value by value.    |
 +------------------------------------------------------------------+
```

Next: [10-composition-vs-inheritance.md](10-composition-vs-inheritance.md).
