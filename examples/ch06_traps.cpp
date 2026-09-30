// ch06 — two virtual-function traps: default arguments, and a near-miss override.
// build: clang++ -std=c++20 -Wall -Wextra ch06_traps.cpp -o /tmp/d && /tmp/d
#include <iostream>

struct Base {
    virtual void f(int x = 1) const { std::cout << "Base " << x << '\n'; }
    virtual ~Base() = default;
};

struct Derived : Base {
    void f(int x = 2) const override { std::cout << "Derived " << x << '\n'; }
};

int main() {
    Derived d;
    Base& b = d;
    std::cout << "through Base&  (default from Base): ";
    b.f();          // Derived's body, Base's default argument: "Derived 1"
    std::cout << "through Derived (default from Derived): ";
    d.f();          // "Derived 2"
}
