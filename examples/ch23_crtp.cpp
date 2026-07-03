// ch23 — CRTP: static polymorphism with zero runtime overhead (no vtable)
// build: clang++ -std=c++20 -Wall -Wextra ch23_crtp.cpp -o /tmp/d && /tmp/d
#include <iostream>

template <class Derived>
struct Shape {                       // CRTP base
    double area() const {
        // static (compile-time) dispatch to the derived implementation:
        return static_cast<const Derived*>(this)->area_impl();
    }
    void describe() const {
        std::cout << "area = " << area() << '\n';   // base code, derived behavior
    }
};

struct Circle : Shape<Circle> {
    double r;
    explicit Circle(double r) : r(r) {}
    double area_impl() const { return 3.14159265 * r * r; }
};

struct Square : Shape<Square> {
    double s;
    explicit Square(double s) : s(s) {}
    double area_impl() const { return s * s; }
};

// A generic function constrained to CRTP shapes — resolved at compile time,
// fully inlinable, NO virtual dispatch and NO vptr in the objects:
template <class T>
void print_area(const Shape<T>& shape) { shape.describe(); }

int main() {
    Circle c(2.0);
    Square s(3.0);
    print_area(c);   // area = 12.5664
    print_area(s);   // area = 9

    std::cout << "sizeof(Circle) = " << sizeof(Circle)
              << " (no vptr -> just the double)\n";   // 8
}
