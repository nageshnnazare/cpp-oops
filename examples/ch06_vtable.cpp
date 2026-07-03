// ch06 — the vptr costs one pointer; virtual dispatch by dynamic type
// build: clang++ -std=c++20 -Wall -Wextra ch06_vtable.cpp -o /tmp/d && /tmp/d
#include <iostream>

struct NonPoly { int x; };                 // no virtual -> no vptr
struct Poly    { int x; virtual ~Poly() = default; };  // has a vptr

struct Base { virtual const char* who() const { return "Base"; } virtual ~Base()=default; };
struct Derived : Base { const char* who() const override { return "Derived"; } };

int main() {
    std::cout << "sizeof(NonPoly) = " << sizeof(NonPoly) << '\n';  // 4
    std::cout << "sizeof(Poly)    = " << sizeof(Poly)    << '\n';  // 16 (int+pad+vptr)

    Derived d;
    Base* p = &d;                          // static type Base*, dynamic type Derived
    std::cout << "p->who() = " << p->who() << '\n';  // Derived (vtable dispatch)

    Base b;
    Base* q = &b;
    std::cout << "q->who() = " << q->who() << '\n';  // Base
}
