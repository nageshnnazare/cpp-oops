// ch14 — closed-set polymorphism with std::variant + std::visit
// build: clang++ -std=c++20 -Wall -Wextra ch14_variant.cpp -o /tmp/d && /tmp/d
#include <iostream>
#include <variant>
#include <vector>

struct Circle { double r; double area() const { return 3.14159265 * r * r; } };
struct Square { double s; double area() const { return s * s; } };
struct Rect   { double w, h; double area() const { return w * h; } };

using Shape = std::variant<Circle, Square, Rect>;

double area(const Shape& sh) {
    return std::visit([](const auto& x) { return x.area(); }, sh);
}

int main() {
    std::vector<Shape> shapes{ Circle{2.0}, Square{3.0}, Rect{2.0, 5.0} };
    double total = 0;
    for (const auto& s : shapes) {
        std::cout << "area = " << area(s) << '\n';
        total += area(s);
    }
    std::cout << "total = " << total << '\n';   // 12.566 + 9 + 10 = 31.566
    // Value semantics: no heap, no vtable, no inheritance. Closed set of types.
}
