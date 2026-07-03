// ch08 — the diamond problem and virtual inheritance
// build: clang++ -std=c++20 -Wall -Wextra ch08_diamond.cpp -o /tmp/d && /tmp/d
#include <iostream>
#include <string>

struct Animal {
    std::string name;
    explicit Animal(std::string n = "unknown") : name(std::move(n)) {}
};

// virtual inheritance -> ONE shared Animal subobject in Bat
struct Mammal       : virtual Animal { bool warm_blooded = true; };
struct WingedAnimal : virtual Animal { double wingspan = 0; };

struct Bat : Mammal, WingedAnimal {
    Bat(std::string n, double span) : Animal(std::move(n)) {   // most-derived inits vbase
        wingspan = span;
    }
};

int main() {
    Bat b("Bruce", 0.3);
    // Without virtual inheritance, 'b.name' would be ambiguous (two Animals).
    std::cout << b.name                       // Bruce  (single shared Animal)
              << " span=" << b.wingspan
              << " warm=" << b.warm_blooded << '\n';   // Bruce span=0.3 warm=1
}
