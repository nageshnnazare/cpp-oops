// ch09 — operator overloading idioms
// build: clang++ -std=c++20 -Wall -Wextra ch09_vector2d.cpp -o /tmp/d && /tmp/d
#include <iostream>

struct Vector2D {
    double x = 0, y = 0;

    Vector2D& operator+=(const Vector2D& o) { x += o.x; y += o.y; return *this; }
    Vector2D& operator*=(double s)          { x *= s;  y *= s;  return *this; }

    bool operator==(const Vector2D&) const = default;

    friend Vector2D operator+(Vector2D a, const Vector2D& b) { return a += b; }
    friend Vector2D operator*(Vector2D a, double s)          { return a *= s; }
    friend Vector2D operator*(double s, Vector2D a)          { return a *= s; }
    friend std::ostream& operator<<(std::ostream& os, const Vector2D& v) {
        return os << '(' << v.x << ", " << v.y << ')';
    }
};

int main() {
    Vector2D a{1, 2}, b{3, 4};
    std::cout << (a + b) << '\n';     // (4, 6)
    std::cout << (2.0 * a) << '\n';   // (2, 4)
    std::cout << (a * 3.0) << '\n';   // (3, 6)
    std::cout << std::boolalpha << (a == a) << '\n';  // true
}
