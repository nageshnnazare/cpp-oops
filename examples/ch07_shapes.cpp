// ch07 — abstract base + programming to an interface
// build: clang++ -std=c++20 -Wall -Wextra ch07_shapes.cpp -o /tmp/d && /tmp/d
#include <iostream>
#include <memory>
#include <vector>

class Shape {
public:
    virtual double area() const = 0;
    virtual const char* name() const = 0;
    virtual ~Shape() = default;
};

class Circle : public Shape {
    double r_;
public:
    explicit Circle(double r) : r_(r) {}
    double area() const override { return 3.14159265 * r_ * r_; }
    const char* name() const override { return "Circle"; }
};

class Rectangle : public Shape {
    double w_, h_;
public:
    Rectangle(double w, double h) : w_(w), h_(h) {}
    double area() const override { return w_ * h_; }
    const char* name() const override { return "Rectangle"; }
};

// written against the ABSTRACTION only:
double total_area(const std::vector<std::unique_ptr<Shape>>& shapes) {
    double sum = 0;
    for (const auto& s : shapes) sum += s->area();
    return sum;
}

int main() {
    std::vector<std::unique_ptr<Shape>> shapes;
    shapes.push_back(std::make_unique<Circle>(2.0));
    shapes.push_back(std::make_unique<Rectangle>(3.0, 4.0));

    for (const auto& s : shapes)
        std::cout << s->name() << " area = " << s->area() << '\n';
    std::cout << "total area = " << total_area(shapes) << '\n';
}
