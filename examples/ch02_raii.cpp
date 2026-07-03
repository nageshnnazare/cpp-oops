// ch02 — RAII: resource freed automatically, even on exceptions
// build: clang++ -std=c++20 -Wall -Wextra ch02_raii.cpp -o /tmp/d && /tmp/d
#include <iostream>
#include <stdexcept>

class Resource {
    const char* name_;
public:
    explicit Resource(const char* name) : name_(name) {
        std::cout << "acquire " << name_ << '\n';
    }
    ~Resource() { std::cout << "release " << name_ << '\n'; }
};

void risky(bool fail) {
    Resource a("A");
    Resource b("B");
    if (fail) throw std::runtime_error("boom");   // both A and B still released!
    std::cout << "did work\n";
}

int main() {
    std::cout << "-- normal path --\n";
    risky(false);
    std::cout << "-- exception path --\n";
    try { risky(true); } catch (const std::exception& e) {
        std::cout << "caught: " << e.what() << '\n';
    }
    // Note release order is reverse of acquisition (B before A).
}
