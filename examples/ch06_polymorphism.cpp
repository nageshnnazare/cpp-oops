// ch06 — runtime polymorphism via virtual functions
// build: clang++ -std=c++20 -Wall -Wextra ch06_polymorphism.cpp -o /tmp/d && /tmp/d
#include <iostream>
#include <memory>
#include <vector>
#include <string>

struct Animal {
    virtual std::string sound() const = 0;
    virtual ~Animal() = default;
};
struct Dog : Animal { std::string sound() const override { return "Woof"; } };
struct Cat : Animal { std::string sound() const override { return "Meow"; } };
struct Cow : Animal { std::string sound() const override { return "Moo";  } };

int main() {
    std::vector<std::unique_ptr<Animal>> zoo;
    zoo.push_back(std::make_unique<Dog>());
    zoo.push_back(std::make_unique<Cat>());
    zoo.push_back(std::make_unique<Cow>());

    for (const auto& a : zoo)
        std::cout << a->sound() << ' ';    // Woof Meow Moo
    std::cout << '\n';
    // The loop knows only Animal; each object's real type decides the behavior.
}
