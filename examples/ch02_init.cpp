// ch02 — list-initialization prefers initializer_list; explicit blocks a conversion.
// build: clang++ -std=c++20 -Wall -Wextra ch02_init.cpp -o /tmp/d && /tmp/d
#include <iostream>
#include <vector>

struct Meters {
    double v;
    explicit Meters(double x) : v(x) {}
};

void travel(Meters m) { std::cout << "travel " << m.v << " m\n"; }

int main() {
    std::vector<int> counted(3, 1);    // three elements, each 1
    std::vector<int> listed{3, 1};     // two elements: 3 and 1
    std::cout << "counted size " << counted.size()
              << " listed size " << listed.size() << '\n';
    std::cout << "listed[0]=" << listed[0] << " listed[1]=" << listed[1] << '\n';

    travel(Meters{5});                 // direct-list-init may call explicit
    // travel(5);                      // ill-formed: explicit constructor
    std::cout << "explicit ctor kept the conversion out of travel(5)\n";
}
