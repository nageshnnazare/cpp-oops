// ch14 — modern OOP: virtual + unique_ptr, no raw new/delete, no leaks
// build: clang++ -std=c++20 -Wall -Wextra ch14_modern_shapes.cpp -o /tmp/d && /tmp/d
#include <iostream>
#include <memory>
#include <vector>

class Shape {
public:
    virtual double area() const = 0;
    virtual ~Shape() = default;
};

class Circle final : public Shape {
    double r_;
public:
    explicit Circle(double r) : r_(r) {}
    double area() const override { return 3.14159265 * r_ * r_; }
};

class Rectangle final : public Shape {
    double w_, h_;
public:
    Rectangle(double w, double h) : w_(w), h_(h) {}
    double area() const override { return w_ * h_; }
};

std::unique_ptr<Shape> make_circle(double r) { return std::make_unique<Circle>(r); }

int main() {
    std::vector<std::unique_ptr<Shape>> shapes;
    shapes.push_back(make_circle(2.0));
    shapes.push_back(std::make_unique<Rectangle>(3.0, 4.0));

    double total = 0;
    for (const auto& s : shapes) total += s->area();
    std::cout << "total = " << total << '\n';   // 24.5664
    // vector<unique_ptr> cleans everything up; no delete needed.
}
