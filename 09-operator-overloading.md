# 09 — Operator Overloading

Operator overloading lets your types use natural syntax: `a + b`, `v[i]`,
`s1 == s2`, `std::cout << obj`. Used well, it makes user types feel built-in.
Used poorly, it obscures. This chapter covers the mechanics, idioms, and the
C++20 spaceship operator.

Prereq: [01-classes-and-objects.md](01-classes-and-objects.md), [04-copy-move-rule-of-five.md](04-copy-move-rule-of-five.md).

---

## 1. What you can overload

```
   Overloadable: + - * / %  == != < > <= >=  += -= *= ...  << >>  [] () -> 
                 ++ -- unary + - !  ~  & | ^  new delete  , (comma)  <=> (C++20)
   NOT overloadable:  ::   .   .*   ?:   sizeof   typeid   (and a few others)

   You cannot invent new operators or change arity/precedence/associativity.
```

An overloaded operator is just a function with a special name: `operator+`,
`operator==`, etc.

---

## 2. Member vs non-member (free function)

```cpp
struct Vec2 {
    double x, y;
    Vec2& operator+=(const Vec2& o) { x += o.x; y += o.y; return *this; }  // member
};

// binary + as a FREE function (symmetric operands):
Vec2 operator+(Vec2 a, const Vec2& b) { a += b; return a; }   // reuse +=
```

```
   MEMBER operator:      a.operator@(b)   -> left operand MUST be your type.
   NON-MEMBER operator:  operator@(a, b)  -> both operands symmetric; allows
                         conversions on the LEFT operand too.

   Guidelines:
     * Must be MEMBER: = [] () -> and (by convention) compound assign += -= ...
     * Prefer NON-MEMBER (often friend) for symmetric binary ops: + - * == < <<
       so that '2 + obj' and 'obj + 2' both work.
```

```
   Why non-member for symmetric ops:
     If operator+ is a member, 'obj + 2' works (obj.operator+(2)) but
     '2 + obj' does NOT (int has no operator+ taking Vec2).
     A free operator+(Vec2,Vec2) with an implicit int->Vec2 conversion fixes both.
```

---

## 3. The canonical arithmetic pattern: implement compound, derive binary

```cpp
struct Money {
    long cents = 0;
    Money& operator+=(Money o) { cents += o.cents; return *this; }
    Money& operator-=(Money o) { cents -= o.cents; return *this; }
};
inline Money operator+(Money a, Money b) { return a += b; }   // reuse +=
inline Money operator-(Money a, Money b) { return a -= b; }
```

```
   Write the MUTATING compound op (+=) once with the real logic; define the
   binary op (+) in terms of it (take first operand BY VALUE, += the second,
   return it). Less duplication, consistent behavior.
```

---

## 4. Comparison: C++20 spaceship `<=>` (huge simplification)

Pre-C++20 you wrote six comparison operators. C++20's three-way comparison
generates them from one defaulted operator.

```cpp
#include <compare>

struct Version {
    int major, minor, patch;
    auto operator<=>(const Version&) const = default;   // generates < <= > >=
    bool operator==(const Version&) const = default;    // == and != (declare too)
};
```

```
   = default  ->  member-wise lexicographic comparison, in declaration order.
   Now all of  < <= > >= == !=  work:
     Version{1,2,0} < Version{1,3,0}   // true (compares major, then minor, ...)

   a <=> b returns an ordering category:
     strong_ordering  : ints, etc. (a<b, a==b, a>b; equal == identical)
     weak_ordering    : equivalent but not identical (case-insensitive strings)
     partial_ordering : may be unordered (floats: NaN)
```

Custom three-way when defaults don't fit:

```cpp
struct CaseInsensitive {
    std::string s;
    std::weak_ordering operator<=>(const CaseInsensitive& o) const {
        return lower(s) <=> lower(o.s);   // compare normalized forms
    }
    bool operator==(const CaseInsensitive& o) const { return lower(s)==lower(o.s); }
};
```

```
   Pre-C++20: 6 operators (== != < <= > >=), lots of boilerplate & bugs.
   C++20:     one '<=> = default' (+ '== = default') -> all six. 
```

Runnable: [`examples/ch09_spaceship.cpp`](examples/ch09_spaceship.cpp).

---

## 5. Stream insertion: `operator<<` (the hidden-friend idiom)

```cpp
class Point {
    int x_, y_;
public:
    Point(int x, int y) : x_(x), y_(y) {}
    // hidden friend: found by ADL, one per class, can access privates:
    friend std::ostream& operator<<(std::ostream& os, const Point& p) {
        return os << '(' << p.x_ << ", " << p.y_ << ')';
    }
};

// std::cout << Point{1,2};   ->  (1, 2)
```

```
   operator<< MUST be a non-member (the left operand is std::ostream, not your
   type). Declaring it a 'friend' inside the class lets it read privates and
   keeps it discoverable via ADL. Return the stream (os) to allow chaining:
     std::cout << p1 << " " << p2 << '\n';
```

---

## 6. Subscript, call, and arrow

```cpp
class Grid {
    std::vector<int> data_;
    std::size_t cols_;
public:
    Grid(std::size_t r, std::size_t c) : data_(r*c), cols_(c) {}

    int&       operator[](std::size_t i)       { return data_[i]; }   // mutable
    const int& operator[](std::size_t i) const { return data_[i]; }   // const

    // C++23: multidimensional subscript
    int& operator[](std::size_t r, std::size_t c) { return data_[r*cols_ + c]; }
};

struct Adder {                          // function-call operator -> "functor"
    int base;
    int operator()(int x) const { return base + x; }   // Adder{10}(5) == 15
};

template <class T>
class SmartPtr {                        // arrow operator -> proxy/smart pointers
    T* p_;
public:
    T* operator->() const { return p_; }    // sp->member  ==  sp.operator->()->member
    T& operator*()  const { return *p_; }
};
```

```
   operator[]  : indexing. Provide const & non-const overloads. (C++23: multi-arg)
   operator()  : makes objects CALLABLE (functors; used by STL, lambdas desugar to this)
   operator->  : for smart pointers/iterators; chains until it reaches a raw ptr.
   All three MUST be member functions.
```

---

## 7. Increment/decrement (pre vs post)

```cpp
struct Counter {
    int n = 0;
    Counter& operator++()    { ++n; return *this; }        // PRE-increment  (++c)
    Counter  operator++(int) { Counter old = *this; ++n; return old; } // POST (c++)
};
```

```
   ++c  -> operator++()      returns a REFERENCE to the incremented object (fast)
   c++  -> operator++(int)   the dummy 'int' param distinguishes it; returns the
                             OLD value BY VALUE (needs a copy -> prefer ++c).
```

---

## 8. Conversion operators (and why `explicit` matters)

```cpp
class Fraction {
    int num_, den_;
public:
    Fraction(int n, int d) : num_(n), den_(d) {}
    explicit operator double() const { return double(num_) / den_; }  // to double
    explicit operator bool() const { return num_ != 0; }              // "is nonzero"
};

Fraction f(1, 2);
double d = static_cast<double>(f);   // explicit -> must cast (0.5)
if (f) { /* uses explicit operator bool -> allowed in boolean contexts */ }
```

```
   operator TargetType() const;   -> defines a conversion FROM your type.
   Mark it 'explicit' to avoid surprising implicit conversions (same reasoning
   as explicit constructors, chapter 02). 'explicit operator bool' is special:
   it still works in if/while/&&/|| contexts (contextual conversion to bool).
```

---

## 9. Rules & good taste

<!--diagram
box[green] DO
  text: Keep semantics **intuitive**: `+` should add, `==` should compare equal
  text: Make `==` and `<` consistent with each other and with `<=>`
  text: Return by value for arithmetic (`a+b`), by reference for `+=`/`[]`/`<<` (chaining)
  text: Prefer non-member (friend) for symmetric binary ops
  text: Use `= default` for comparisons (C++20) whenever member-wise is correct
box[red] DON'T
  text: Overload operators with surprising meaning (`operator+` that subtracts)
  text: Overload `&&` `||` `,` — they lose short-circuit / sequencing semantics
  text: Add operators the type doesn't naturally have (don't force it)
-->
```
   DO:
     * Keep semantics INTUITIVE: '+' should add, '==' should compare equal.
     * Make == and < consistent with each other and with <=>.
     * Return by value for arithmetic (a+b), by reference for +=/[]/<< (chaining).
     * Prefer non-member (friend) for symmetric binary ops.
     * Use '= default' for comparisons (C++20) whenever member-wise is correct.

   DON'T:
     * Overload operators with surprising meaning (operator+ that subtracts).
     * Overload && || , (comma) — they lose short-circuit / sequencing semantics.
     * Add operators the type doesn't naturally have (don't force it).
```

<!--diagram
box[orange] Good taste
  text: Overload an operator **only** when the meaning is **obvious** to a reader. If you must document "what does `+` mean here?", use a named function instead
-->
```
   +---------------------------------------------------------------+
   | Overload an operator ONLY when the meaning is OBVIOUS to a    |
   | reader. If you must document "what does + mean here?", use a  |
   | named function instead.                                       |
   +---------------------------------------------------------------+
```

---

## 10. Worked example: a `Vector2D`

```cpp
#include <iostream>
#include <compare>

struct Vector2D {
    double x = 0, y = 0;

    Vector2D& operator+=(const Vector2D& o) { x += o.x; y += o.y; return *this; }
    Vector2D& operator*=(double s)          { x *= s;  y *= s;  return *this; }

    bool operator==(const Vector2D&) const = default;   // member-wise ==

    friend Vector2D operator+(Vector2D a, const Vector2D& b) { return a += b; }
    friend Vector2D operator*(Vector2D a, double s)          { return a *= s; }
    friend Vector2D operator*(double s, Vector2D a)          { return a *= s; } // symmetric
    friend std::ostream& operator<<(std::ostream& os, const Vector2D& v) {
        return os << '(' << v.x << ", " << v.y << ')';
    }
};

int main() {
    Vector2D a{1, 2}, b{3, 4};
    std::cout << a + b << '\n';     // (4, 6)
    std::cout << 2.0 * a << '\n';   // (2, 4)   (symmetric operator*)
    std::cout << (a == a) << '\n';  // 1
}
```

Runnable: [`examples/ch09_vector2d.cpp`](examples/ch09_vector2d.cpp).

---

## 11. Summary

<!--diagram
title: Operator overloading
box[green] Key points
  text: Operators are functions named `operator@`. Can't invent new ones or change precedence/arity
  text: **Member** for: `=` `[]` `()` `->` (and compound assigns). **Non-member**/friend for symmetric binary ops (`+` `-` `==` `<<`) so left operand converts
  text: Implement compound (`+=`) then derive binary (`+`) from it
  text: C++20 `<=>`: `auto operator<=>(...) const = default;` (+ `==`) gives all six comparisons. Category: strong/weak/partial ordering
  text: `operator<<` = non-member friend returning the stream (chaining). `operator()` → functors; `operator->` → smart pointers
  text: Mark conversion operators `explicit`. Keep meanings **intuitive**
-->
```
 +------------------------------------------------------------------+
 | Operators are functions named operator@. Can't invent new ones   |
 | or change precedence/arity.                                      |
 | MEMBER for: = [] () -> (and compound assigns). NON-MEMBER/friend |
 |   for symmetric binary ops (+ - == <<) so left operand converts. |
 | Implement compound (+=) then derive binary (+) from it.          |
 | C++20 <=>: 'auto operator<=>(...) const = default;' (+ ==) gives |
 |   all six comparisons. Category: strong/weak/partial ordering.   |
 | operator<< = non-member friend returning the stream (chaining).  |
 | operator() -> functors; operator-> -> smart pointers.            |
 | Mark conversion operators 'explicit'. Keep meanings INTUITIVE.   |
 +------------------------------------------------------------------+
```

Next: [10-composition-vs-inheritance.md](10-composition-vs-inheritance.md).
