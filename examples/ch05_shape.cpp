// ch05 — inheritance mechanics + upcasting + slicing demo
// build: clang++ -std=c++20 -Wall -Wextra ch05_shape.cpp -o /tmp/d && /tmp/d
#include <iostream>
#include <string>

class Shape {
public:
    explicit Shape(std::string name) : name_(std::move(name)) {}
    const std::string& name() const { return name_; }
    virtual double area() const { return 0; }   // virtual so slicing is visible
    virtual ~Shape() = default;
protected:
    std::string name_;
};

class Circle : public Shape {
    double r_;
public:
    explicit Circle(double r) : Shape("circle"), r_(r) {}
    double area() const override { return 3.14159265 * r_ * r_; }
};

int main() {
    Circle c(2.0);
    Shape& ref = c;                        // reference: no slicing
    std::cout << ref.name() << " area(ref) = " << ref.area() << '\n'; // 12.566

    Shape sliced = c;                      // SLICING: copies only the Shape part
    std::cout << sliced.name() << " area(sliced) = " << sliced.area() << '\n';
    // area(sliced) == 0 (Circle::area lost) -> demonstrates slicing.
}
