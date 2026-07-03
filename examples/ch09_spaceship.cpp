// ch09 — C++20 three-way comparison generates all six operators
// build: clang++ -std=c++20 -Wall -Wextra ch09_spaceship.cpp -o /tmp/d && /tmp/d
#include <compare>
#include <iostream>
#include <set>

struct Version {
    int major, minor, patch;
    auto operator<=>(const Version&) const = default;  // < <= > >=
    bool operator==(const Version&) const = default;   // == !=
};

int main() {
    Version a{1, 2, 0}, b{1, 3, 0};
    std::cout << std::boolalpha;
    std::cout << "a <  b : " << (a <  b) << '\n';   // true
    std::cout << "a == a : " << (a == a) << '\n';   // true
    std::cout << "a >= b : " << (a >= b) << '\n';   // false

    // works as a std::set key for free (needs ordering):
    std::set<Version> s{ {2,0,0}, {1,0,0}, {1,5,0} };
    for (const auto& v : s)
        std::cout << v.major << '.' << v.minor << '.' << v.patch << ' ';
    std::cout << '\n';                               // 1.0.0 1.5.0 2.0.0
}
