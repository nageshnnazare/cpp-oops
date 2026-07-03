// ch22 — pointers to members: data (offset) and function (2-word {ptr, adj}),
//        and how a member-function pointer to a VIRTUAL still dispatches.
// build: clang++ -std=c++20 -Wall -Wextra ch22_ptmf.cpp -o /tmp/d && /tmp/d
#include <iostream>

struct Point { int x; int y; };

struct Base {
    virtual int f() { return 1; }   // virtual
    int g() { return 2; }           // non-virtual
    virtual ~Base() = default;
};
struct Derived : Base {
    int f() override { return 100; }
};

int main() {
    // --- pointer to DATA member: really an offset ---
    int Point::* px = &Point::x;
    int Point::* py = &Point::y;
    Point p{10, 20};
    std::cout << "p.*px = " << p.*px << ", p.*py = " << p.*py << '\n';  // 10 20

    // --- pointer to MEMBER FUNCTION: two words; virtual dispatches correctly ---
    int (Base::* pf)() = &Base::f;   // points at a VIRTUAL
    int (Base::* pg)() = &Base::g;   // points at a non-virtual

    Base b;
    Derived d;
    Base& rb = d;                    // refers to a Derived

    std::cout << "(b.*pf)()  = " << (b.*pf)()  << '\n';   // 1   (Base::f)
    std::cout << "(rb.*pf)() = " << (rb.*pf)() << '\n';   // 100 (Derived::f! virtual)
    std::cout << "(b.*pg)()  = " << (b.*pg)()  << '\n';   // 2   (non-virtual)

    std::cout << "\nsizeof(int Point::*)      = " << sizeof(px) << " (data: 1 word)\n";
    std::cout << "sizeof(int (Base::*)())  = " << sizeof(pf)
              << " (function: often 2 words {ptr, this-adjust})\n";
}
